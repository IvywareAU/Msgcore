// Copyright © 2026 Khrustal & Mann
//              MELBOURNE, VICTORIA, AUSTRALIA, 3000
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
//  Standalone Phase-2 shim test for p2piocp (IOCP over io_uring). No legacy code.
//  Exercises the semantics the pump relies on (the Linux port plan §5.2, §8 "Shim test
//  suite green incl. cancellation & Fix-4 semantics"):
//    - normal async read completion + byte count + completion key
//    - the Fix-4 invariant: data already buffered at submit time STILL posts a CQE
//    - PostQueuedCompletionStatus from another thread (wake + drain)
//    - cancellation -> ERROR_OPERATION_ABORTED
//    - graceful peer close -> ERROR_HANDLE_EOF (0-byte read)
//    - timeout -> FALSE + null OVERLAPPED + WAIT_TIMEOUT
//
//  Also covers the Phase-2 wiring layered on p2piocp: the async ReadFile/WriteFile file
//  shim routing to the ring, CreateEvent/SetEvent/ResetEvent/WaitForSingleObject over
//  eventfd, and CloseHandle dispatch (Iocp teardown, Event close, Fd cancel-on-close §5.6).
//
//  Build (Linux) — from the Platform/ directory, NOT from checks/ (the line that stood
//  here mixed the two: `checks/iocp_test.cpp` needs cwd Platform/, while `-I..` and
//  `../p2piocp.cpp` need cwd Platform/checks/, so it worked from neither):
//    g++ -std=c++23 -I. checks/iocp_test.cpp p2piocp.cpp -luring -pthread -o iocp_test
//
#include "../p2piocp.h"
#include "../p2pfile.h"     // ReadFile/WriteFile/CloseHandle shims (weak hooks resolve here)
#include "../p2pthread.h"   // CreateEvent/SetEvent/WaitForSingleObject
#include "../p2psock.h"     // send()/SOCKET_ERROR + the WSAStartup that sets SIGPIPE policy

#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/resource.h>
#include <sys/wait.h>
#include <cstdlib>
#include <csignal>
#include <atomic>

//  ThreadSanitizer detection (GCC spells it one way, clang another) — see case (15).
#if defined(__SANITIZE_THREAD__)
#  define P2P_TEST_UNDER_TSAN 1
#elif defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define P2P_TEST_UNDER_TSAN 1
#  endif
#endif
#ifndef P2P_TEST_UNDER_TSAN
#  define P2P_TEST_UNDER_TSAN 0
#endif

static int g_fail = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { \
    std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fail; } } while (0)

static HANDLE wrap_fd(int fd) { return p2p_handle_new(HKind::Fd, fd, nullptr); }

//  The submission-failure recovery paths cannot be reached on a healthy ring -- io_uring
//  absorbs everything a test can queue at it -- so p2piocp.cpp forces them when
//  P2P_IOCP_FAULT=sqe is set. That flag is read once at first use, so the scenario runs in
//  its OWN process: the suite re-execs itself with the variable set (case 13 below).
static int fault_main() {
    int fail = 0, checks = 0;
    #define FCHECK(cond) do { ++checks; if (!(cond)) { \
        std::printf("  FAIL[fault] %s:%d  %s\n", __FILE__, __LINE__, #cond); ++fail; } } while (0)

    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    FCHECK(port != nullptr);
    if (!port) return 1;
    int sv[2];
    FCHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    HANDLE h = wrap_fd(sv[0]);
    FCHECK(CreateIoCompletionPort(h, port, 0xFAu, 0) == port);

    DWORD bytes = 0; ULONG_PTR key = 0; LPOVERLAPPED pov = nullptr;
    GetQueuedCompletionStatus(port, &bytes, &key, &pov, 0);   // claim ownership here

    //  Owner thread. The op cannot be issued, so the failure has to be reported right
    //  here: answering ERROR_IO_PENDING would promise a completion packet that nothing
    //  will ever post, which is the silent hang this whole change exists to remove.
    char buf1[16] = {0};
    OVERLAPPED ov1{};
    BOOL ok = p2p_iocp_read(port, h, buf1, sizeof buf1, &ov1);
    FCHECK(ok == FALSE);
    FCHECK(GetLastError() == ERROR_NO_SYSTEM_RESOURCES);

    //  Cross-thread. Here the caller HAS already been told ERROR_IO_PENDING by the time
    //  the owner discovers it cannot issue the op, so the only honest outcome left is a
    //  failed completion carrying that same OVERLAPPED back.
    char buf2[16] = {0};
    OVERLAPPED ov2{};
    BOOL  qok  = TRUE;
    DWORD qerr = 0;
    std::thread t([&]{ qok = p2p_iocp_read(port, h, buf2, sizeof buf2, &ov2);
                       qerr = GetLastError(); });
    t.join();
    FCHECK(qok == FALSE && qerr == ERROR_IO_PENDING);        // queued, exactly as promised
    pov = nullptr; key = 0;
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 2000);
    FCHECK(ok == FALSE);
    FCHECK(pov == &ov2);                                     // the op came back...
    FCHECK(GetLastError() == ERROR_NO_SYSTEM_RESOURCES);     // ...as a failure, not silence
    FCHECK(key == 0xFAu);                                    // ...attributable to its socket

    ::close(sv[1]);
    CloseHandle(h);
    CloseHandle(port);
    std::printf("  [fault] %d checks, %d failures\n", checks, fail);
    return fail == 0 ? 0 : 1;
    #undef FCHECK
}
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--fault-sqe") == 0) return fault_main();
    std::printf("=== p2piocp shim test ===\n");

    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    CHECK(port != nullptr);
    if (!port) { std::printf("FATAL: no port\n"); return 1; }

    int sv[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    HANDLE hConn = wrap_fd(sv[0]);              // the "connection" (associated) end
    int    peer  = sv[1];                        // raw peer end we poke directly

    const ULONG_PTR KEY = 0xCAFEu;
    CHECK(CreateIoCompletionPort(hConn, port, KEY, 0) == port);   // associate fd -> KEY

    DWORD      bytes = 0;
    ULONG_PTR  key   = 0;
    LPOVERLAPPED pov = nullptr;
    BOOL ok;

    // ---- (6) timeout: nothing pending -> FALSE + null ov + WAIT_TIMEOUT ----
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 50);
    CHECK(ok == FALSE);
    CHECK(pov == nullptr);
    CHECK(GetLastError() == WAIT_TIMEOUT);

    // ---- (1) Fix-4: data ALREADY available before the read is armed ----
    CHECK(::write(peer, "hello", 5) == 5);       // buffer data first
    char buf1[64] = {0};
    OVERLAPPED ov1{};
    ok = p2p_iocp_read(port, hConn, buf1, sizeof buf1, &ov1);
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);   // Fix-4 convention
    pov = nullptr; bytes = 0; key = 0;
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 1000);
    CHECK(ok == TRUE);                            // io_uring posts a CQE even though ready
    CHECK(pov == &ov1);
    CHECK(bytes == 5);
    CHECK(key == KEY);
    CHECK(std::memcmp(buf1, "hello", 5) == 0);

    // ---- (2) normal async completion: arm read, THEN data arrives ----
    char buf2[64] = {0};
    OVERLAPPED ov2{};
    ok = p2p_iocp_read(port, hConn, buf2, sizeof buf2, &ov2);
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);
    CHECK(::write(peer, "WORLD!", 6) == 6);
    pov = nullptr; bytes = 0;
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 1000);
    CHECK(ok == TRUE);
    CHECK(pov == &ov2);
    CHECK(bytes == 6);
    CHECK(std::memcmp(buf2, "WORLD!", 6) == 0);

    // ---- (3) PostQueuedCompletionStatus from another thread ----
    OVERLAPPED ov3{};
    std::thread poster([&]{
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        PostQueuedCompletionStatus(port, 777, 0x1234u, &ov3);
    });
    pov = nullptr; bytes = 0; key = 0;
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 2000);
    poster.join();
    CHECK(ok == TRUE);
    CHECK(pov == &ov3);
    CHECK(bytes == 777);
    CHECK(key == 0x1234u);

    // ---- (4) cancellation -> ERROR_OPERATION_ABORTED ----
    char buf4[64] = {0};
    OVERLAPPED ov4{};
    ok = p2p_iocp_read(port, hConn, buf4, sizeof buf4, &ov4);   // no data -> stays pending
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);
    p2p_iocp_cancel(port, hConn);
    pov = nullptr;
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 1000);
    CHECK(ok == FALSE);
    CHECK(pov == &ov4);
    CHECK(GetLastError() == ERROR_OPERATION_ABORTED);

    // ---- (5) graceful peer close -> EOF (0-byte read) ----
    ::close(peer);                               // peer hangs up
    char buf5[64] = {0};
    OVERLAPPED ov5{};
    ok = p2p_iocp_read(port, hConn, buf5, sizeof buf5, &ov5);
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);
    pov = nullptr;
    ok = GetQueuedCompletionStatus(port, &bytes, &key, &pov, 1000);
    CHECK(ok == FALSE);
    CHECK(pov == &ov5);
    CHECK(GetLastError() == ERROR_HANDLE_EOF);

    // ---- (7) events: CreateEvent/SetEvent/ResetEvent/WaitForSingleObject ----
    HANDLE evM = CreateEvent(nullptr, TRUE, FALSE, nullptr);   // manual-reset, unsignalled
    CHECK(evM != nullptr);
    CHECK(WaitForSingleObject(evM, 0) == WAIT_TIMEOUT);        // not signalled
    CHECK(SetEvent(evM) == TRUE);
    CHECK(WaitForSingleObject(evM, 0) == WAIT_OBJECT_0);       // signalled
    CHECK(WaitForSingleObject(evM, 0) == WAIT_OBJECT_0);       // manual: STAYS signalled
    CHECK(ResetEvent(evM) == TRUE);
    CHECK(WaitForSingleObject(evM, 0) == WAIT_TIMEOUT);        // reset
    CHECK(CloseHandle(evM) == TRUE);

    HANDLE evA = CreateEvent(nullptr, FALSE, TRUE, nullptr);   // auto-reset, initially signalled
    CHECK(evA != nullptr);
    CHECK(WaitForSingleObject(evA, 0) == WAIT_OBJECT_0);       // consumes the signal
    CHECK(WaitForSingleObject(evA, 0) == WAIT_TIMEOUT);        // auto-reset cleared it
    CHECK(SetEvent(evA) == TRUE);
    CHECK(WaitForSingleObject(evA, 100) == WAIT_OBJECT_0);
    CHECK(CloseHandle(evA) == TRUE);

    // ---- (8) async ReadFile/WriteFile via the file shim -> the ring (Fix-4) ----
    HANDLE port2 = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    CHECK(port2 != nullptr);
    int sv2[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) == 0);
    HANDLE hW = wrap_fd(sv2[0]), hR = wrap_fd(sv2[1]);
    const ULONG_PTR KEY_W = 0xAAAAu, KEY_R = 0xBBBBu;
    CHECK(CreateIoCompletionPort(hW, port2, KEY_W, 0) == port2);
    CHECK(CreateIoCompletionPort(hR, port2, KEY_R, 0) == port2);

    OVERLAPPED ovW{};
    BOOL w = WriteFile(hW, "PING", 4, nullptr, &ovW);          // overlapped write via shim
    CHECK(w == FALSE && GetLastError() == ERROR_IO_PENDING);
    pov = nullptr; bytes = 0; key = 0;
    ok = GetQueuedCompletionStatus(port2, &bytes, &key, &pov, 1000);
    CHECK(ok == TRUE);
    CHECK(pov == &ovW);
    CHECK(bytes == 4);
    CHECK(key == KEY_W);

    char rb[16] = {0};
    OVERLAPPED ovR{};
    BOOL rr = ReadFile(hR, rb, sizeof rb, nullptr, &ovR);      // overlapped read via shim
    CHECK(rr == FALSE && GetLastError() == ERROR_IO_PENDING);
    pov = nullptr; bytes = 0; key = 0;
    ok = GetQueuedCompletionStatus(port2, &bytes, &key, &pov, 1000);
    CHECK(ok == TRUE);
    CHECK(pov == &ovR);
    CHECK(bytes == 4);
    CHECK(key == KEY_R);
    CHECK(std::memcmp(rb, "PING", 4) == 0);
    CHECK(CloseHandle(hW) == TRUE);                            // Fd close dispatch (deassoc)
    CHECK(CloseHandle(hR) == TRUE);
    CHECK(CloseHandle(port2) == TRUE);                        // Iocp teardown dispatch

    // ---- (9) CloseHandle(Fd) on an associated socket cancels its in-flight ops (§5.6) ----
    HANDLE port3 = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    CHECK(port3 != nullptr);
    int sv3[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv3) == 0);
    HANDLE hc = wrap_fd(sv3[0]);
    CHECK(CreateIoCompletionPort(hc, port3, 0xCCu, 0) == port3);
    //  Claim ownership first so the read is submitted while the fd is still open, then
    //  CloseHandle cancels it deterministically (avoids the close-before-submit race).
    GetQueuedCompletionStatus(port3, &bytes, &key, &pov, 0);   // prime owner (returns timeout)
    char cbuf[32] = {0};
    OVERLAPPED ovc{};
    ok = p2p_iocp_read(port3, hc, cbuf, sizeof cbuf, &ovc);    // no data -> stays pending
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);
    CHECK(CloseHandle(hc) == TRUE);                            // cancels + deassoc + close(fd)
    pov = nullptr;
    ok = GetQueuedCompletionStatus(port3, &bytes, &key, &pov, 1000);
    CHECK(ok == FALSE);
    CHECK(pov == &ovc);
    CHECK(GetLastError() == ERROR_OPERATION_ABORTED);
    ::close(sv3[1]);
    CHECK(CloseHandle(port3) == TRUE);

    // ---- (10) CloseHandle(Fd) from a NON-pump thread still cancels (§5.6) -------------
    //  Test (9) claims ownership first so the cancel is submitted while the fd is still
    //  open — it says so — and that is the one path this suite ever exercised. A service
    //  drops a connection from wherever the drop happens (a hub thread), NOT from the
    //  pump, and then the cancel is marshalled and reaches the kernel after close() has
    //  already returned. Same assertions as (9), only the closing thread differs.
    HANDLE port4 = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    CHECK(port4 != nullptr);
    int sv4[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv4) == 0);
    HANDLE h4 = wrap_fd(sv4[0]);
    CHECK(CreateIoCompletionPort(h4, port4, 0xD4u, 0) == port4);
    GetQueuedCompletionStatus(port4, &bytes, &key, &pov, 0);   // this thread owns the ring
    char b4[32] = {0};
    OVERLAPPED ov4c{};
    ok = p2p_iocp_read(port4, h4, b4, sizeof b4, &ov4c);       // pending: no data
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);
    std::thread closer4([&]{ CloseHandle(h4); });              // ...closed from elsewhere
    closer4.join();
    pov = nullptr; key = 0;
    ok = GetQueuedCompletionStatus(port4, &bytes, &key, &pov, 2000);
    CHECK(ok == FALSE);
    CHECK(pov == &ov4c);
    CHECK(GetLastError() == ERROR_OPERATION_ABORTED);
    CHECK(key == 0xD4u);                   // bound at submission, so the abort is attributable
    ::close(sv4[1]);
    CHECK(CloseHandle(port4) == TRUE);

    // ---- (11) a cross-thread close must not cancel the socket that INHERITS its fd ----
    //  The other half of the same defect. The closing thread drops a connection and, before
    //  the pump has drained anything, accepts a new one — which the kernel hands the fd
    //  number just freed — and submits a read on it. A cancel that only resolves the fd
    //  NUMBER when it finally executes gets both answers wrong: the dead socket's read is
    //  never cancelled (its OVERLAPPED never completes and the pump waits on a connection
    //  that is gone) and the live socket's read is aborted for nothing. Both completions
    //  are therefore checked, not just the abort.
    HANDLE port5 = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    CHECK(port5 != nullptr);
    int sv5[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv5) == 0);
    const int oldfd = sv5[0];
    HANDLE h5 = wrap_fd(sv5[0]);
    CHECK(CreateIoCompletionPort(h5, port5, 0xD5u, 0) == port5);
    GetQueuedCompletionStatus(port5, &bytes, &key, &pov, 0);   // this thread owns the ring
    char b5[32] = {0};
    OVERLAPPED ov5old{};
    ok = p2p_iocp_read(port5, h5, b5, sizeof b5, &ov5old);
    CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);

    int    sv6[2] = { -1, -1 };
    char   b6[32] = {0};
    HANDLE h6     = nullptr;
    OVERLAPPED ov5new{};
    std::thread churn([&]{
        CloseHandle(h5);                                       // frees oldfd's number
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv6) != 0) return;
        h6 = wrap_fd(sv6[0]);                                  // ...which this very likely reuses
        CreateIoCompletionPort(h6, port5, 0xD6u, 0);
        p2p_iocp_read(port5, h6, b6, sizeof b6, &ov5new);
        ssize_t w = ::write(sv6[1], "LIVE", 4); (void)w;
    });
    churn.join();
    if (sv6[0] != oldfd)
        std::printf("  note: fd %d not recycled (got %d) - the reuse half was not exercised\n",
                    oldfd, sv6[0]);

    bool sawAbort = false, sawLive = false;
    for (int i = 0; i < 4 && !(sawAbort && sawLive); ++i) {
        pov = nullptr; key = 0; bytes = 0;
        ok = GetQueuedCompletionStatus(port5, &bytes, &key, &pov, 2000);
        DWORD err = GetLastError();
        if (!pov) break;                                       // timed out: nothing more due
        if      (pov == &ov5old) sawAbort = (ok == FALSE && err == ERROR_OPERATION_ABORTED
                                                         && key == 0xD5u);
        else if (pov == &ov5new) sawLive  = (ok == TRUE && bytes == 4 && key == 0xD6u
                                             && std::memcmp(b6, "LIVE", 4) == 0);
    }
    CHECK(sawAbort);        // the dead socket's read WAS cancelled...
    CHECK(sawLive);         // ...and the live socket's read was not
    if (h6) CHECK(CloseHandle(h6) == TRUE);
    if (sv6[1] >= 0) ::close(sv6[1]);
    ::close(sv5[1]);
    CHECK(CloseHandle(port5) == TRUE);
    // ---- (12) more in-flight ops than the ring can hold: nothing may vanish -----------
    //  The ring is created with 256 SQEs, so 512 CQEs. This puts more completions in
    //  flight than the completion queue can hold, which is where io_uring starts refusing
    //  submissions (-EBUSY while the CQ is in overflow). Those returns used to be
    //  discarded — io_uring_submit was called eleven times without its result ever being
    //  looked at, and io_uring_get_sqe's second attempt was dereferenced without a null
    //  check — so under exactly this pressure an op could be dropped AFTER its caller had
    //  been told ERROR_IO_PENDING, leaving a connection waiting for a packet that was
    //  never coming. An explicit refusal is fine; a disappearance is not. Every
    //  OVERLAPPED must come back exactly once, and carrying its own completion key.
    struct rlimit rl{};
    if (::getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < 4096) {
        rlim_t want = (rl.rlim_max == RLIM_INFINITY) ? 4096 : (rl.rlim_max < 4096 ? rl.rlim_max : 4096);
        rl.rlim_cur = want; ::setrlimit(RLIMIT_NOFILE, &rl);
    }
    ::getrlimit(RLIMIT_NOFILE, &rl);
    int N = 600;                                            // > the 512-entry CQ
    if (rl.rlim_cur != RLIM_INFINITY && (rlim_t)(2 * N + 128) > rl.rlim_cur)
        N = ((int)rl.rlim_cur - 128) / 2;
    if (N < 8) N = 8;
    if (N < 600) std::printf("  note: fd limit %d caps this at %d pairs (CQ overflow may not be reached)\n",
                             (int)rl.rlim_cur, N);

    HANDLE port6 = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    CHECK(port6 != nullptr);
    GetQueuedCompletionStatus(port6, &bytes, &key, &pov, 0);   // this thread owns the ring
    std::vector<int>        afd(N, -1), bfd(N, -1);
    std::vector<HANDLE>     hs (N, nullptr);
    std::vector<OVERLAPPED> ovs(N);
    std::vector<char>       bufs(N * 8, 0);
    int nsub = 0;
    for (int i = 0; i < N; ++i) {
        int s[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s) != 0) break;
        afd[i] = s[0]; bfd[i] = s[1];
        hs[i] = wrap_fd(s[0]);
        if (CreateIoCompletionPort(hs[i], port6, (ULONG_PTR)(0x1000 + i), 0) != port6) break;
        ok = p2p_iocp_read(port6, hs[i], &bufs[i * 8], 4, &ovs[i]);
        if (!(ok == FALSE && GetLastError() == ERROR_IO_PENDING)) break;   // refused, loudly
        ++nsub;
    }
    CHECK(nsub == N);
    for (int i = 0; i < nsub; ++i) { ssize_t w = ::write(bfd[i], "DATA", 4); (void)w; }

    std::vector<int> seen(nsub, 0);
    int accounted = 0, dupes = 0, strays = 0, badkey = 0;
    for (int guard = 0; guard < nsub * 4 && accounted < nsub; ++guard) {
        pov = nullptr; key = 0;
        GetQueuedCompletionStatus(port6, &bytes, &key, &pov, 3000);
        if (!pov) break;                                       // timed out: nothing more due
        long idx = (OVERLAPPED*)pov - &ovs[0];
        if (idx < 0 || idx >= nsub) { ++strays; continue; }
        if (seen[idx]) { ++dupes; continue; }
        seen[idx] = 1; ++accounted;
        if (key != (ULONG_PTR)(0x1000 + idx)) ++badkey;
    }
    CHECK(accounted == nsub);      // every op was accounted for...
    CHECK(dupes    == 0);          // ...exactly once...
    CHECK(strays   == 0);          // ...as itself...
    CHECK(badkey   == 0);          // ...and attributable to the socket that owned it
    if (accounted != nsub)
        std::printf("  LOST %d of %d completions\n", nsub - accounted, nsub);
    for (int i = 0; i < N; ++i) {
        if (hs[i])      CloseHandle(hs[i]);
        if (bfd[i] >= 0) ::close(bfd[i]);
    }
    CHECK(CloseHandle(port6) == TRUE);
    // ---- (14) a write to a peer that is gone must not kill the process ---------------
    //  Win32 has no SIGPIPE: a send to a dead peer returns an error and the service keeps
    //  running. On Linux the default disposition TERMINATES the process, and the ring's
    //  write path uses plain write(2) semantics (io_uring_prep_write), so one peer
    //  resetting at the wrong moment could take down a daemon that is behaving perfectly.
    //  CreateIoCompletionPort installs the policy (p2p_ignore_sigpipe, p2ptypes.h); if it
    //  ever stops doing so, this case does not fail politely -- the process dies here and
    //  the suite reports no summary at all, which is its own kind of loud.
    {
        struct sigaction sa{};
        CHECK(::sigaction(SIGPIPE, nullptr, &sa) == 0);
        CHECK(sa.sa_handler == SIG_IGN);          // installed when the first port was made

        HANDLE port7 = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        CHECK(port7 != nullptr);
        int sv7[2];
        CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv7) == 0);
        HANDLE h7 = wrap_fd(sv7[0]);
        CHECK(CreateIoCompletionPort(h7, port7, 0xE7u, 0) == port7);
        GetQueuedCompletionStatus(port7, &bytes, &key, &pov, 0);   // own the ring
        ::close(sv7[1]);                          // the peer goes away entirely

        //  Synchronous path (MSG_NOSIGNAL) -- must report the error, not raise a signal.
        int sr = send(h7, "gone", 4, 0);
        CHECK(sr == SOCKET_ERROR);

        //  Ring path -- the failure must arrive as a completion on this OVERLAPPED.
        OVERLAPPED ov7{};
        ok = p2p_iocp_write(port7, h7, "gone", 4, &ov7);
        CHECK(ok == FALSE && GetLastError() == ERROR_IO_PENDING);
        pov = nullptr;
        ok = GetQueuedCompletionStatus(port7, &bytes, &key, &pov, 2000);
        CHECK(pov == &ov7);                       // it completed...
        CHECK(ok == FALSE);                       // ...as a failure...
        CHECK(key == 0xE7u);                      // ...on the socket that owned it
        CHECK(CloseHandle(h7) == TRUE);
        CHECK(CloseHandle(port7) == TRUE);
    }
    // ---- (13) forced submission failure, in a child process (see fault_main) ---------
    {
        std::fflush(stdout);            // the child writes to this same stdout
        char exe[4096] = {0};
        ssize_t elen = ::readlink("/proc/self/exe", exe, sizeof exe - 1);
        CHECK(elen > 0);
        if (elen > 0) {
            pid_t pid = ::fork();
            if (pid == 0) {
                ::setenv("P2P_IOCP_FAULT", "sqe", 1);
                char* av[] = { exe, (char*)"--fault-sqe", nullptr };
                ::execv(exe, av);
                _exit(127);
            }
            CHECK(pid > 0);
            int st = 0;
            CHECK(::waitpid(pid, &st, 0) == pid);
            CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
        }
    }
    // ---- (15) a closed SOCKET must stay safe to touch (§4.1 handle lifetime) ---------
    //  Closing a socket from one thread to make another thread's call fail is a blunt but
    //  legitimate Win32 idiom: there a SOCKET is a table value, so the losing thread gets
    //  WSAENOTSOCK and the process lives. Here a SOCKET is a pointer that closesocket used
    //  to `delete`, which turned the same source into a use-after-free — TSan caught it in
    //  `p2p_authrelay` as closesocket's delete racing a relay thread's read of `s->fd`,
    //  four bytes into the freed allocation, and ASan calls it outright on the case below:
    //
    //      ERROR: AddressSanitizer: heap-use-after-free
    //      READ of size 4 ... in recv(P2PHandle*, char*, int, int) p2psock.h:258
    //      freed by thread T0 here: ... in closesocket(P2PHandle*)  p2psock.h:183
    //
    //  Handles are now retired to a free-list, so the losing thread reads a CLOSED handle
    //  instead of freed memory.
    {
        int sv8[2];
        CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv8) == 0);
        SOCKET s8 = wrap_fd(sv8[0]);
        char rb[8];
        CHECK(recv(s8, rb, sizeof rb, MSG_DONTWAIT) == SOCKET_ERROR);   // open -> EAGAIN
        CHECK(GetLastError() != ERROR_INVALID_HANDLE);
#if !P2P_TEST_UNDER_TSAN
        //  The concurrent half is skipped under ThreadSanitizer, and not because it fails:
        //  it deliberately performs close-while-another-thread-is-in-recv, so TSan reports
        //  the CALLER-side fd race -- close(2) against a descriptor another thread has
        //  already handed to a syscall. That race is real, it belongs to the caller, and no
        //  shim can remove it: the descriptor is in flight by then. The handle lifetime
        //  this case exists to prove is what ASan checks, and there it is clean.
        std::atomic<bool> stop{false}, saw_closed{false}, saw_open{false};
        std::thread reader([&]{
            char b[8];
            while (!stop.load(std::memory_order_acquire)) {
                if (recv(s8, b, sizeof b, MSG_DONTWAIT) == SOCKET_ERROR) {
                    if (GetLastError() == ERROR_INVALID_HANDLE)        // fd taken away: closed
                        saw_closed.store(true, std::memory_order_release);
                    else                                               // EAGAIN: still open
                        saw_open.store(true, std::memory_order_release);
                }
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(saw_open.load());                       // the reader really was using it...
        CHECK(closesocket(s8) == 0);                  // ...when it was closed underneath
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stop.store(true, std::memory_order_release);
        reader.join();
        CHECK(saw_closed.load());                     // and it read a closed handle, not garbage
#else
        CHECK(closesocket(s8) == 0);
#endif
        //  Either way the retired handle stays readable and reports itself closed.
        CHECK(recv(s8, rb, sizeof rb, MSG_DONTWAIT) == SOCKET_ERROR);
        CHECK(GetLastError() == ERROR_INVALID_HANDLE);

        //  Closing it again must FAIL, not corrupt the free-list by linking the handle to
        //  itself -- Win32 answers ERROR_INVALID_HANDLE to a second close, not a crash.
        CHECK(closesocket(s8) == SOCKET_ERROR);
        CHECK(GetLastError() == ERROR_INVALID_HANDLE);
        //  Retired, not freed: the next allocation comes off the free-list, so the memory
        //  is bounded and a stale HANDLE always addresses a handle rather than a hole.
        SOCKET reused = p2p_handle_new(HKind::Fd, -1, nullptr);
        CHECK(reused == s8);
        p2p_handle_retire(reused);
        ::close(sv8[1]);
    }
    // ---- teardown of the original port/handle via the CloseHandle dispatch ----
    CHECK(CloseHandle(hConn) == TRUE);   // Fd: deassoc + close(sv[0])
    CHECK(CloseHandle(port) == TRUE);    // Iocp: ring teardown

    std::printf("=== %d checks, %d failures : %s ===\n",
                g_checks, g_fail, g_fail == 0 ? "PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
