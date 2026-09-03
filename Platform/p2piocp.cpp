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
//  Platform layer — IOCP over io_uring implementation (the Linux port plan §5).  Linux only.
//
//  One IoRing per completion port (== per pump thread). All submissions and the wait
//  happen on a single "owner" thread (the first to call GetQueuedCompletionStatus),
//  satisfying IORING_SETUP_SINGLE_ISSUER; calls from other threads (PostQueued..., a
//  SendP2PeerMsg off a hub thread, cancellation) are marshalled onto an MPSC queue and
//  an eventfd wakes the owner to drain + submit them (§5.3, §5.4).
//
#if !defined(_WIN32)

#include "p2piocp.h"

#include <liburing.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <deque>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace {

//  Env-gated diagnostic tracing (P2P_IOCP_TRACE=1). Owner id + event, to stderr.
static bool p2p_trace_on() { static int v = getenv("P2P_IOCP_TRACE") ? 1 : 0; return v; }
#define IOTRACE(...) do { if (p2p_trace_on()) { std::fprintf(stderr, "[iocp] " __VA_ARGS__); std::fflush(stderr); } } while(0)

//  Fault injection for the submission-failure paths (P2P_IOCP_FAULT=sqe), read once at
//  first use like the trace flag. Those paths are otherwise unreachable from a test: a
//  healthy ring absorbs every op a machine can queue, and the kernel's CQ-overflow backlog
//  swallows the rest -- so without a way to force the failure, the recovery code would
//  ship having never executed. checks/iocp_test.cpp re-execs itself with this set.
static bool p2p_fault_sqe() {
    static int v = [] { const char* e = getenv("P2P_IOCP_FAULT");
                        return (e && std::strstr(e, "sqe")) ? 1 : 0; }();
    return v != 0;
}

constexpr std::uint64_t EVENTFD_TAG = ~0ull;   // reserved SQE user_data for the wake poll

//  A queued item is either a completion to hand back verbatim (PostQueuedCompletionStatus)
//  or a request to submit an SQE on the owner thread (cross-thread read/write/cancel).
enum class QKind : std::uint8_t { Post, Read, Write, Poll, Connect, Accept, Cancel };
struct QItem {
    QKind        kind;
    DWORD        bytes = 0;         // Post
    ULONG_PTR    key   = 0;         // Post
    OVERLAPPED*  ov    = nullptr;   // Post / Read / Write
    int          fd    = -1;        // Read / Write / Cancel
    void*        buf   = nullptr;   // Read / Write
    DWORD        n     = 0;         // Read / Write
    DWORD        err   = 0;         // Post: non-zero => hand this completion back as a
                                    //       FAILURE (GQCS returns FALSE + SetLastError),
                                    //       which is how a submission that could not be
                                    //       issued reaches the caller that was already
                                    //       told ERROR_IO_PENDING.
    bool         owns_fd = false;   // Cancel: `fd` is a private dup this ring must close
                                    //         once the cancel has reached the kernel
                                    //         (see p2p_iocp_on_close_fd).
};

struct IoRing {
    io_uring                            ring{};
    int                                 evfd = -1;
    std::atomic<std::uintptr_t>         owner{0};       // pthread_self() of the pump thread
    std::atomic<bool>                   evfd_armed{false};
    std::mutex                          qmx;
    std::deque<QItem>                   q;              // MPSC: many producers, owner drains
    std::unordered_map<int, ULONG_PTR>  assoc;          // fd -> completion key
    //  fds dup'd to keep a file description alive across a marshalled cancel. Prepared
    //  cancels reference them, so they are closed only once a submit has actually reached
    //  the kernel (ring_submit). Guarded by their OWN mutex rather than qmx, and never
    //  taken before it, because the drain loop touches them while holding qmx -- and
    //  because TargetCore's teardown drains (P2Pwin32.cpp CloseP2PmsgHub and the con
    //  sweep) call GetQueuedCompletionStatus from a thread that is NOT the pump, so
    //  "only the owner ever gets here" is not an assumption worth building on.
    std::mutex                          dupmx;
    std::vector<int>                    cancel_dups;
    //  Set when io_uring_submit refused SQEs that are already prepared (-EBUSY on a CQ
    //  in overflow, -EEXIST from a non-issuer thread). The prepared entries stay in the
    //  SQ ring and GQCS retries the flush once it has consumed CQEs -- so the ops are
    //  delayed, never lost, and never completed twice.
    std::atomic<bool>                   submit_pending{false};

    ULONG_PTR key_for(int fd) {
        std::lock_guard<std::mutex> lk(qmx);
        auto it = assoc.find(fd);
        return it == assoc.end() ? 0 : it->second;
    }
    void wake() { std::uint64_t one = 1; ssize_t r = ::write(evfd, &one, sizeof one); (void)r; }
    void hold_dup(int fd) { std::lock_guard<std::mutex> lk(dupmx); cancel_dups.push_back(fd); }
    void release_dups() {                       // close outside the lock: no syscall under it
        std::vector<int> v;
        { std::lock_guard<std::mutex> lk(dupmx); v.swap(cancel_dups); }
        for (int fd : v) ::close(fd);
    }
    bool is_owner() const {
        std::uintptr_t o = owner.load(std::memory_order_acquire);
        return o != 0 && o == (std::uintptr_t)pthread_self();
    }
};

inline IoRing* ring_of(HANDLE port) {
    return (port && port->kind == HKind::Iocp) ? static_cast<IoRing*>(port->impl) : nullptr;
}

//  Global fd -> owning-ring registry (§5.3). Populated when a socket/file is associated
//  with a completion port, so the portless overlapped ReadFile/WriteFile shim (p2pfile.h)
//  can find the ring from just the fd. Its own mutex; never nested with a ring's qmx
//  (lock, copy, unlock — no held-lock crossings) so the lock order stays trivial.
std::mutex                        g_reg_mx;
std::unordered_map<int, IoRing*>  g_fd_ring;

void    reg_fd(int fd, IoRing* r) { std::lock_guard<std::mutex> lk(g_reg_mx); g_fd_ring[fd] = r; }
IoRing* ring_for_fd(int fd) {
    std::lock_guard<std::mutex> lk(g_reg_mx);
    auto it = g_fd_ring.find(fd); return it == g_fd_ring.end() ? nullptr : it->second;
}
IoRing* unreg_fd(int fd) {                 // returns the ring the fd was on, or nullptr
    std::lock_guard<std::mutex> lk(g_reg_mx);
    auto it = g_fd_ring.find(fd); if (it == g_fd_ring.end()) return nullptr;
    IoRing* r = it->second; g_fd_ring.erase(it); return r;
}
void unreg_ring(IoRing* r) {               // drop every fd owned by a torn-down ring
    std::lock_guard<std::mutex> lk(g_reg_mx);
    for (auto it = g_fd_ring.begin(); it != g_fd_ring.end();)
        if (it->second == r) it = g_fd_ring.erase(it); else ++it;
}

//  Global live-completion-port registry (§5.6). PostQueuedCompletionStatus takes a raw
//  port HANDLE that a detached helper (notably P2PeerConWsa::ConnectExThread) can post to
//  AFTER the owning hub tore the port down. On Windows a closed IOCP handle just makes PQCS
//  return FALSE; on Linux the port P2PHandle is freed, so ring_of(Port) would dereference
//  freed memory (ASan heap-use-after-free during teardown). We track live ports and validate
//  Port under g_port_mx, held across the whole post so it is mutually exclusive with
//  p2p_iocp_destroy's erase+free: a post to an already-destroyed port fails cleanly instead
//  of touching the freed ring (matching the Windows semantics the ConnectExThread relies on).
std::mutex                   g_port_mx;
std::unordered_set<HANDLE>   g_live_ports;
void reg_port(HANDLE p)   { std::lock_guard<std::mutex> lk(g_port_mx); g_live_ports.insert(p); }

//  ---------------------------------------------------------------------------
//  Submission plumbing. Owner-thread only, and every kernel return value inspected.
//
//  These exist because the shim used to call io_uring_get_sqe() twice and then
//  dereference the result, and to call io_uring_submit() eleven times without once
//  looking at what it returned. Both are silent-loss bugs rather than cosmetic ones:
//  the async submit primitives have ALREADY told their caller ERROR_IO_PENDING by the
//  time they run, so an SQE that never reaches the kernel leaves an OVERLAPPED that
//  can never complete -- a connection that waits forever, with no error anywhere.
//  ---------------------------------------------------------------------------

//  Obtain an SQE, flushing a full submission queue once. nullptr means the ring could
//  not take the op AND nothing was prepared for it, so the caller owns the failure.
io_uring_sqe* get_sqe(IoRing* r) {
    if (p2p_fault_sqe()) return nullptr;         // forced failure, test-only (see above)
    io_uring_sqe* sqe = io_uring_get_sqe(&r->ring);
    if (sqe) return sqe;
    int rc = io_uring_submit(&r->ring);          // SQ full: flush and retry once
    if (rc < 0) { IOTRACE("get_sqe flush failed ring=%p rc=%d\n", (void*)r, rc);
                  r->submit_pending.store(true, std::memory_order_release); return nullptr; }
    sqe = io_uring_get_sqe(&r->ring);
    if (!sqe) IOTRACE("no SQE even after flush ring=%p\n", (void*)r);
    return sqe;
}

//  io_uring_submit with its result acted on. A negative return means the entries
//  prepared since the last submit did NOT reach the kernel -- they stay in the SQ ring,
//  submit_pending asks GQCS to retry the flush after it has drained CQEs, and the dup'd
//  cancel fds stay held because the cancels referencing them have not run yet.
int ring_submit(IoRing* r) {
    int rc = io_uring_submit(&r->ring);
    if (rc < 0) {
        IOTRACE("submit failed ring=%p rc=%d (%s)\n", (void*)r, rc, std::strerror(-rc));
        r->submit_pending.store(true, std::memory_order_release);
        return rc;
    }
    r->submit_pending.store(false, std::memory_order_release);
    r->release_dups();                           // the cancels are in the kernel now
    return rc;
}

//  Owner-thread only: (re)arm the persistent multishot poll on the eventfd so a wake()
//  surfaces as a CQE tagged EVENTFD_TAG. Returns false if it could not be armed, leaving
//  evfd_armed clear so the GQCS loop retries -- an unarmed eventfd means cross-thread
//  PostQueuedCompletionStatus stops waking the pump, so this must not fail silently.
bool arm_eventfd(IoRing* r) {
    io_uring_sqe* sqe = get_sqe(r);
    if (!sqe) { r->evfd_armed.store(false, std::memory_order_release);
                IOTRACE("arm_eventfd: no SQE ring=%p\n", (void*)r); return false; }
    io_uring_prep_poll_multishot(sqe, r->evfd, POLLIN);
    io_uring_sqe_set_data64(sqe, EVENTFD_TAG);
    r->evfd_armed.store(true, std::memory_order_release);
    //  Submit NOW so the poll is armed in-kernel before the owner blocks in
    //  io_uring_wait_cqe_timeout. Without this, on a fresh ring whose queue is empty the
    //  poll SQE stays unsubmitted through the first wait, so a cross-thread wake()
    //  (PostQueuedCompletionStatus eventfd write) produces no CQE and the pump sleeps the
    //  full timeout — the Dmx handshake stalled ~8s per step until this was fixed. (In the
    //  standalone iocp_test the poll happened to be flushed early by an unrelated submit.)
    return ring_submit(r) >= 0;
}

//  Owner-thread only: turn one queued request into an SQE (Post items are handled by the
//  drain loop directly and never reach here). Returns false when no SQE could be had --
//  nothing was prepared and the request has NOT been issued.
bool submit_qitem(IoRing* r, const QItem& it) {
    io_uring_sqe* sqe = get_sqe(r);
    if (!sqe) return false;
    switch (it.kind) {
        case QKind::Read:   io_uring_prep_read (sqe, it.fd, it.buf, it.n, (std::uint64_t)-1); break;
        case QKind::Write:  io_uring_prep_write(sqe, it.fd, it.buf, it.n, (std::uint64_t)-1); break;
        case QKind::Poll:   io_uring_prep_poll_add(sqe, it.fd, POLLIN);                       break;
        case QKind::Connect:
            io_uring_prep_connect(sqe, it.fd,
                reinterpret_cast<const struct sockaddr*>(it.ov->_p2p_addr),
                (socklen_t)it.ov->_p2p_addrlen);
            break;
        case QKind::Accept:
            io_uring_prep_accept(sqe, it.fd, nullptr, nullptr, SOCK_CLOEXEC);
            break;
        case QKind::Cancel: io_uring_prep_cancel_fd(sqe, it.fd, IORING_ASYNC_CANCEL_ALL);     break;
        default: break;
    }
    io_uring_sqe_set_data64(sqe, it.kind == QKind::Cancel ? 0ull : (std::uint64_t)(std::uintptr_t)it.ov);
    //  A cancel carrying a dup owns that fd until the SQE referencing it is in the kernel.
    if (it.kind == QKind::Cancel && it.owns_fd) r->hold_dup(it.fd);
    return true;
}

//  Owner-thread only: prepare + flush one op. false == the op never reached the ring, so
//  the caller must report a failure instead of promising a completion. A prepared-but-
//  unflushed SQE is NOT a failure: it stays queued and GQCS retries the flush.
bool submit_now(IoRing* r, const QItem& it) {
    if (!submit_qitem(r, it)) return false;
    ring_submit(r);
    return true;
}

//  Owner-thread only. Drain the MPSC queue: submit every Read/Write/Cancel request, and
//  return the FIRST Post item (if any) so GetQueuedCompletionStatus can hand it back with
//  PQCS priority (§5.4). Returns true and fills *out when a Post was dequeued.
//
//  A request that cannot be given an SQE is turned into a FAILED completion for its own
//  OVERLAPPED and re-queued as a Post: the thread that submitted it cross-thread was told
//  ERROR_IO_PENDING and is waiting for a packet, so dropping it would hang that I/O
//  forever. Ops with no OVERLAPPED (cancels) can only be logged.
bool drain_queue(IoRing* r, QItem* out) {
    bool have_post = false, submitted = false;
    std::deque<QItem> failed;
    std::lock_guard<std::mutex> lk(r->qmx);
    for (auto it = r->q.begin(); it != r->q.end();) {
        if (it->kind == QKind::Post) {
            if (!have_post) { *out = *it; have_post = true; it = r->q.erase(it); continue; }
            ++it;   // leave later Posts for the next call
        } else {
            if (submit_qitem(r, *it)) { submitted = true; }
            else if (it->ov) {
                IOTRACE("drain: no SQE, failing ov=%p fd=%d\n", (void*)it->ov, it->fd);
                QItem f{QKind::Post}; f.ov = it->ov; f.key = it->ov->_p2p_key;
                f.err = ERROR_NO_SYSTEM_RESOURCES;
                failed.push_back(f);
            } else {
                IOTRACE("drain: no SQE, cancel on fd=%d LOST\n", it->fd);
                if (it->owns_fd) ::close(it->fd);
            }
            it = r->q.erase(it);
        }
    }
    if (submitted) ring_submit(r);
    //  Hand back a synthesised failure straight away when there is no real Post competing
    //  for this call: the thread that queued the op is blocked waiting for its packet, and
    //  making it wait for the NEXT GetQueuedCompletionStatus would be a needless stall --
    //  the wake it would rely on may itself be what failed.
    if (!failed.empty() && !have_post) { *out = failed.front(); failed.pop_front(); have_post = true; }
    for (auto& f : failed) r->q.push_back(f);   // any remainder: next drain, one loop away
    return have_post;
}

//  Direct-submit on the owner thread, else marshal onto the queue and wake the owner.
//  Core form taking the resolved ring + fd (shared by the port-based submit_rw and the
//  portless p2p_iocp_submit_rw the ReadFile/WriteFile shim calls).
BOOL submit_rw_ring(IoRing* r, int fd, void* buf, DWORD n, LPOVERLAPPED ov, QKind kind) {
    ov->Internal = 0; ov->InternalHigh = 0;
    ov->_p2p_fd = fd; ov->_p2p_ring = r; ov->_p2p_key = r->key_for(fd);
    ov->_p2p_op = (kind == QKind::Write) ? P2POP_WRITE : P2POP_READ;
    ov->_p2p_wbuf = buf; ov->_p2p_wlen = n; ov->_p2p_wdone = 0;  // short-write tracking
    if (r->is_owner()) {
        QItem it{kind}; it.fd = fd; it.buf = buf; it.n = n; it.ov = ov;
        if (!submit_now(r, it)) {
            //  The op never reached the ring, so no completion packet will ever be posted
            //  for it. Say so synchronously instead of returning the ERROR_IO_PENDING that
            //  would leave the caller waiting for a packet that cannot arrive. Win32 fails
            //  the initial ReadFile/WSASend the same way, and the transports already handle
            //  it: `if (WSAGetLastError() != ERROR_IO_PENDING) ... Throw()`.
            SetLastError(ERROR_NO_SYSTEM_RESOURCES);
            return FALSE;
        }
    } else {
        { std::lock_guard<std::mutex> lk(r->qmx);
          r->q.push_back(QItem{kind, 0, 0, ov, fd, buf, n}); }
        r->wake();
    }
    SetLastError(ERROR_IO_PENDING);   // Fix-4 convention: always "pending" after submit
    return FALSE;
}

BOOL submit_rw(HANDLE Port, HANDLE File, void* buf, DWORD n, LPOVERLAPPED ov, QKind kind) {
    IoRing* r = ring_of(Port);
    if (!r || !File || File->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    return submit_rw_ring(r, File->fd, buf, n, ov, kind);
}

} // namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingPort,
                              ULONG_PTR CompletionKey, DWORD /*NumberOfConcurrentThreads*/) {
    if (ExistingPort) {                                   // associate fd -> key
        IoRing* r = ring_of(ExistingPort);
        if (!r || !FileHandle || FileHandle->kind != HKind::Fd) {
            SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
        { std::lock_guard<std::mutex> lk(r->qmx); r->assoc[FileHandle->fd] = CompletionKey; }
        reg_fd(FileHandle->fd, r);        // fd -> ring, for the portless ReadFile/WriteFile shim
        return ExistingPort;
    }
    //  Create a bare port. Prefer SINGLE_ISSUER|DEFER_TASKRUN (safe under the one-thread-
    //  per-ring model, §5.1); fall back to a plain ring on older kernels. Creating a port
    //  is also the point where a process starts doing overlapped socket I/O, so it is where
    //  the SIGPIPE policy is installed (p2ptypes.h): the io_uring write path uses plain
    //  write(2) semantics, and on Linux a peer that resets mid-write would otherwise
    //  deliver a signal whose default action is to kill the daemon outright.
    p2p_ignore_sigpipe();
    auto* r = new IoRing();
    io_uring_params p{}; p.flags = IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN;
    int rc = io_uring_queue_init_params(256, &r->ring, &p);
    if (rc < 0) { io_uring_params p2{}; rc = io_uring_queue_init_params(256, &r->ring, &p2); }
    if (rc < 0) { delete r; SetLastError(win32_from_errno(-rc)); return nullptr; }
    r->evfd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (r->evfd < 0) { io_uring_queue_exit(&r->ring); delete r; SetLastError(win32_from_errno(errno)); return nullptr; }
    if (FileHandle && FileHandle->kind == HKind::Fd) {
        r->assoc[FileHandle->fd] = CompletionKey;
        reg_fd(FileHandle->fd, r);
    }
    HANDLE port = p2p_handle_new(HKind::Iocp, -1, r);
    reg_port(port);                     // track as live so a late cross-thread PQCS is safe
    return port;
}

BOOL GetQueuedCompletionStatus(HANDLE Port, LPDWORD lpNumberOfBytes,
                               PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped,
                               DWORD dwMilliseconds) {
    IoRing* r = ring_of(Port);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }

    //  First caller becomes the owner (pump) thread; arm the wake poll here so the very
    //  first submit happens on the issuer thread (SINGLE_ISSUER).
    std::uintptr_t self = (std::uintptr_t)pthread_self();
    std::uintptr_t expect = 0;
    if (r->owner.compare_exchange_strong(expect, self)) { arm_eventfd(r);
        IOTRACE("GQCS owner set ring=%p tid=%lx\n", (void*)r, (unsigned long)self); }

    __kernel_timespec ts;
    bool infinite = (dwMilliseconds == INFINITE);
    if (!infinite) { ts.tv_sec = dwMilliseconds / 1000; ts.tv_nsec = (long)(dwMilliseconds % 1000) * 1000000L; }

    for (;;) {
        //  Recover from a refused flush before anything else: entries prepared earlier are
        //  still in the SQ ring, and every CQE consumed since has made room for them.
        if (r->submit_pending.load(std::memory_order_acquire)) ring_submit(r);
        //  An eventfd poll that failed to arm leaves the pump deaf to cross-thread wakes,
        //  so retry it here rather than waiting for a multishot drop that cannot come.
        if (!r->evfd_armed.load(std::memory_order_acquire)) arm_eventfd(r);

        QItem posted;
        if (drain_queue(r, &posted)) {                    // PQCS path (§5.4)
            if (lpNumberOfBytes) *lpNumberOfBytes = posted.err ? 0u : posted.bytes;
            if (lpCompletionKey) *lpCompletionKey = posted.key;
            if (lpOverlapped)    *lpOverlapped    = posted.ov;
            if (posted.err) {   // a submission that could not be issued at all (§5.4)
                if (posted.ov) { posted.ov->Internal = posted.err; posted.ov->InternalHigh = 0; }
                IOTRACE("GQCS drained FAILED POST ring=%p ov=%p err=%u\n",
                        (void*)r, (void*)posted.ov, (unsigned)posted.err);
                SetLastError(posted.err);
                return FALSE;
            }
            IOTRACE("GQCS drained POST ring=%p key=%p ov=%p\n", (void*)r, (void*)posted.key, (void*)posted.ov);
            return TRUE;
        }
        io_uring_cqe* cqe = nullptr;
        IOTRACE("GQCS wait ring=%p tid=%lx timeout=%ums\n", (void*)r, (unsigned long)self, dwMilliseconds);
        int rc = infinite ? io_uring_wait_cqe(&r->ring, &cqe)
                          : io_uring_wait_cqe_timeout(&r->ring, &cqe, &ts);
        IOTRACE("GQCS woke ring=%p rc=%d\n", (void*)r, rc);
        if (rc == -EINTR) continue;
        if (rc == -ETIME || rc == -ETIMEDOUT) {           // (a) timeout / no work
            if (lpOverlapped) *lpOverlapped = nullptr;
            SetLastError(WAIT_TIMEOUT); return FALSE;
        }
        if (rc < 0 || !cqe) { if (lpOverlapped) *lpOverlapped = nullptr;
            SetLastError(win32_from_errno(rc < 0 ? -rc : EINVAL)); return FALSE; }

        std::uint64_t data = io_uring_cqe_get_data64(cqe);
        int  res   = cqe->res;
        bool more  = (cqe->flags & IORING_CQE_F_MORE) != 0;

        if (data == EVENTFD_TAG) {                         // wake: drain queue, re-arm, retry
            std::uint64_t sink; ssize_t rd = ::read(r->evfd, &sink, sizeof sink); (void)rd;
            io_uring_cqe_seen(&r->ring, cqe);
            if (!more) arm_eventfd(r);                      // multishot dropped -> re-arm
            continue;
        }
        if (data == 0) { io_uring_cqe_seen(&r->ring, cqe); continue; }  // cancel op's own CQE

        auto* ov = reinterpret_cast<OVERLAPPED*>((std::uintptr_t)data);
        io_uring_cqe_seen(&r->ring, cqe);
        //  The key was bound when the op was submitted. Do NOT look it up from
        //  ov->_p2p_fd here: closesocket() erases the association before the
        //  cancellations it triggers complete, so an aborted op would arrive
        //  with key 0. See the comment on OVERLAPPED::_p2p_key in p2piocp.h.
        if (lpCompletionKey) *lpCompletionKey = ov->_p2p_key;
        if (lpOverlapped)    *lpOverlapped    = ov;

        if (res < 0) {                                     // (b) failed / cancelled I/O
            ov->Internal = (ULONG_PTR)(-res); ov->InternalHigh = 0;
            if (lpNumberOfBytes) *lpNumberOfBytes = 0;
            SetLastError(res == -ECANCELED ? ERROR_OPERATION_ABORTED : win32_from_errno(-res));
            return FALSE;
        }
        if (res == 0 && ov->_p2p_op == P2POP_READ) {       // (d) graceful close -> EOF
            ov->Internal = 0; ov->InternalHigh = 0;
            if (lpNumberOfBytes) *lpNumberOfBytes = 0;
            SetLastError(ERROR_HANDLE_EOF); return FALSE;
        }
        if (ov->_p2p_op == P2POP_ACCEPT) {                 // (e) res == accepted fd (§5.1)
            auto* asock = static_cast<P2PHandle*>(ov->_p2p_acceptsock);
            if (asock) {
                if (asock->fd >= 0) ::close(asock->fd);     // drop the pre-created placeholder
                asock->fd = res;
                ULONG_PTR k = 0;                            // inherit the listen socket's key
                { std::lock_guard<std::mutex> lk(r->qmx);
                  auto it = r->assoc.find(ov->_p2p_fd);
                  if (it != r->assoc.end()) k = it->second;
                  r->assoc[res] = k; }
                reg_fd(res, r);                             // so async recv on it finds the ring
            }
            ov->Internal = 0; ov->InternalHigh = 0;
            if (lpNumberOfBytes) *lpNumberOfBytes = 0;
            return TRUE;
        }
        //  (f) short write: an overlapped socket write on Windows completes only when the
        //  WHOLE buffer is sent, and the legacy send-completion handler (P2PeerCon.cpp:432)
        //  relies on that — it never checks the byte count, frees the message and sends the
        //  next. io_uring_prep_write on a TCP socket, however, can return res < len when the
        //  socket send buffer is full (exactly under sustained throughput). Reproduce the
        //  Win32 all-or-error contract: resubmit the remainder on this (owner) thread with the
        //  SAME OVERLAPPED* identity and wait for the next CQE, only reporting completion once
        //  the full length is out. (Reads need no such handling — the recv framing already
        //  accumulates partial reads.) Without this the wire frame is truncated -> the receiver
        //  desyncs -> re-sync path -> invalid image -> VBHeap assert.
        if (ov->_p2p_op == P2POP_WRITE && res >= 0) {
            ov->_p2p_wdone += (DWORD)res;
            if (ov->_p2p_wdone < ov->_p2p_wlen) {
                io_uring_sqe* sqe = get_sqe(r);
                if (!sqe) {
                    //  The remainder cannot be resubmitted. Failing the write is the only
                    //  honest answer: reporting success would hand the send-completion
                    //  handler a TRUNCATED frame, which is precisely the receiver desync
                    //  the all-or-error contract exists to prevent.
                    ov->Internal = ERROR_NO_SYSTEM_RESOURCES; ov->InternalHigh = ov->_p2p_wdone;
                    if (lpNumberOfBytes) *lpNumberOfBytes = 0;
                    IOTRACE("short-write resubmit: no SQE ring=%p ov=%p %u/%u\n", (void*)r,
                            (void*)ov, (unsigned)ov->_p2p_wdone, (unsigned)ov->_p2p_wlen);
                    SetLastError(ERROR_NO_SYSTEM_RESOURCES);
                    return FALSE;
                }
                io_uring_prep_write(sqe, ov->_p2p_fd,
                                    (char*)ov->_p2p_wbuf + ov->_p2p_wdone,
                                    ov->_p2p_wlen - ov->_p2p_wdone, (std::uint64_t)-1);
                io_uring_sqe_set_data64(sqe, (std::uint64_t)(std::uintptr_t)ov);
                ring_submit(r);                    // a refused flush is retried in this loop
                continue;                          // await the remainder's CQE
            }
            ov->Internal = 0; ov->InternalHigh = ov->_p2p_wlen;
            if (lpNumberOfBytes) *lpNumberOfBytes = ov->_p2p_wlen;
            return TRUE;
        }
        DWORD bytes = (ov->_p2p_op == P2POP_POLL) ? 0u : (DWORD)res;   // (c) success
        ov->Internal = 0; ov->InternalHigh = bytes;
        if (lpNumberOfBytes) *lpNumberOfBytes = bytes;
        return TRUE;
    }
}

BOOL PostQueuedCompletionStatus(HANDLE Port, DWORD dwNumberOfBytesTransferred,
                                ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped) {
    //  Hold g_port_mx across validate + post so a concurrent p2p_iocp_destroy cannot free
    //  the ring mid-post: either the port is still live and this post lands on a valid ring,
    //  or it was already retired and we fail FALSE without dereferencing freed memory. This
    //  is the shim analogue of Windows PQCS safely failing on a closed IOCP handle (§5.6).
    std::lock_guard<std::mutex> lkp(g_port_mx);
    if (g_live_ports.find(Port) == g_live_ports.end()) {
        SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    IoRing* r = ring_of(Port);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    { std::lock_guard<std::mutex> lk(r->qmx);
      r->q.push_back(QItem{QKind::Post, dwNumberOfBytesTransferred, dwCompletionKey, lpOverlapped}); }
    r->wake();
    IOTRACE("PQCS ring=%p by tid=%lx (owner=%lx) key=%p ov=%p\n", (void*)r,
            (unsigned long)pthread_self(), (unsigned long)r->owner.load(),
            (void*)dwCompletionKey, (void*)lpOverlapped);
    return TRUE;
}

BOOL p2p_iocp_read(HANDLE Port, HANDLE File, void* buf, DWORD n, LPOVERLAPPED ov) {
    return submit_rw(Port, File, buf, n, ov, QKind::Read);
}
BOOL p2p_iocp_write(HANDLE Port, HANDLE File, const void* buf, DWORD n, LPOVERLAPPED ov) {
    return submit_rw(Port, File, const_cast<void*>(buf), n, ov, QKind::Write);
}

BOOL p2p_iocp_cancel(HANDLE Port, HANDLE File) {
    IoRing* r = ring_of(Port);
    if (!r || !File || File->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    if (r->is_owner()) {
        QItem it{QKind::Cancel}; it.fd = File->fd;
        if (!submit_now(r, it)) { SetLastError(ERROR_NO_SYSTEM_RESOURCES); return FALSE; }
    } else {
        { std::lock_guard<std::mutex> lk(r->qmx);
          QItem it{QKind::Cancel}; it.fd = File->fd; r->q.push_back(it); }
        r->wake();
    }
    return TRUE;
}

BOOL p2p_iocp_destroy(HANDLE Port) {
    IoRing* r = ring_of(Port);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    //  Retire the port from the live set FIRST, under g_port_mx: this blocks until any
    //  in-flight PostQueuedCompletionStatus (which holds g_port_mx across its post) finishes,
    //  and thereafter every late PQCS sees the port gone and returns FALSE without touching
    //  the ring we are about to free (§5.6 teardown race with ConnectExThread).
    { std::lock_guard<std::mutex> lk(g_port_mx); g_live_ports.erase(Port); }
    unreg_ring(r);                       // drop any fd -> ring registry entries first
    //  Release every fd this ring privately holds: dups whose cancels are prepared or
    //  already issued, and dups still sitting in the queue for a cancel that will now
    //  never be submitted. The ring is going away, so the ops they guarded die with it.
    { std::lock_guard<std::mutex> lk(r->qmx);
      for (auto& it : r->q) if (it.kind == QKind::Cancel && it.owns_fd) ::close(it.fd);
      r->q.clear(); }
    r->release_dups();                     // any cancel still holding an fd
    //  Best-effort drain of any already-posted CQEs (queue_exit cancels the rest).
    io_uring_cqe* cqe = nullptr;
    while (io_uring_peek_cqe(&r->ring, &cqe) == 0 && cqe) io_uring_cqe_seen(&r->ring, cqe);
    io_uring_queue_exit(&r->ring);
    if (r->evfd >= 0) ::close(r->evfd);
    delete r;
    p2p_handle_retire(Port);
    return TRUE;
}

// ---------------------------------------------------------------------------
//  extern "C" hooks the p2pfile.h shims call weakly (§5.3, §5.6). Defined here so
//  they exist only in an image that links this TU (TargetCore + the shim test); in a
//  Msgcore-only image they stay unresolved-weak -> null, and p2pfile.h guards the call.
// ---------------------------------------------------------------------------
extern "C" BOOL p2p_iocp_submit_rw(int fd, void* buf, DWORD n, void* ov, int isWrite) {
    IoRing* r = ring_for_fd(fd);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    return submit_rw_ring(r, fd, buf, n, reinterpret_cast<LPOVERLAPPED>(ov),
                          isWrite ? QKind::Write : QKind::Read);
}

//  Async POLLIN wait (WaitCommEvent(EV_RXCHAR) on the serial fd, §5.1). Submits a
//  one-shot poll op tagged P2POP_POLL; its CQE reports 0 bytes (the pump then reads).
//  Always FALSE + ERROR_IO_PENDING after submit (Fix-4).
extern "C" BOOL p2p_iocp_poll(int fd, void* ov) {
    IoRing* r = ring_for_fd(fd);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    auto* o = reinterpret_cast<LPOVERLAPPED>(ov);
    o->Internal = 0; o->InternalHigh = 0;
    o->_p2p_fd = fd; o->_p2p_ring = r; o->_p2p_op = P2POP_POLL;
    o->_p2p_key = r->key_for(fd);
    if (r->is_owner()) {
        QItem it{QKind::Poll}; it.fd = fd; it.ov = o;
        if (!submit_now(r, it)) { SetLastError(ERROR_NO_SYSTEM_RESOURCES); return FALSE; }
    } else {
        { std::lock_guard<std::mutex> lk(r->qmx);
          QItem it{QKind::Poll}; it.fd = fd; it.ov = o; r->q.push_back(it); }
        r->wake();
    }
    SetLastError(ERROR_IO_PENDING);
    return FALSE;
}

extern "C" void p2p_iocp_on_close_fd(int fd) {
    IoRing* r = unreg_fd(fd);
    if (!r) return;                                     // not IOCP-associated
    { std::lock_guard<std::mutex> lk(r->qmx); r->assoc.erase(fd); }
    //  Cancel in-flight ops so each surfaces as ERROR_OPERATION_ABORTED (§5.6).
    //
    //  The cancel MUST reference a file description that is still open, and the caller
    //  closes `fd` the instant this returns (p2psock.h closesocket, p2pfile.h CloseHandle).
    //  On the owner thread that holds, because io_uring_submit issues the cancel inline.
    //  Cross-thread it did NOT: the request was marshalled and the owner submitted it
    //  later, by which time `fd` was closed and -- under connection churn -- already
    //  REUSED by a freshly accepted socket. IORING_ASYNC_CANCEL_FD is resolved to a
    //  struct file when the cancel EXECUTES, and pending requests are matched against
    //  that file, so a late cancel had two ways to be wrong and both were silent:
    //    - fd number now free   -> the cancel fails, the original ops are never
    //                              cancelled, their OVERLAPPEDs never complete, and the
    //                              pump waits forever on a connection that is gone;
    //    - fd number now reused -> the cancel matches the WRONG socket and aborts the
    //                              reads of a healthy new connection.
    //  The suite never caught it because it only ever exercised the owner path, and
    //  checks/iocp_test.cpp said so in the one place it came close.
    //
    //  dup() closes both holes at once: a private fd number this ring alone owns,
    //  referring to the SAME file description, so the cancel matches the original
    //  requests whenever it runs, and the caller's close() cannot hand our number to
    //  anybody else. The dup is released once the cancel has actually reached the
    //  kernel -- not when it is prepared -- see IoRing::cancel_dups.
    //  F_DUPFD_CLOEXEC, not dup(): the duplicate lives for as long as it takes the owner
    //  to submit the cancel, and a fork+exec in that window must not carry a socket into
    //  the child (every other fd this layer creates is O_CLOEXEC for the same reason).
    int  cfd  = ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
    bool owns = (cfd >= 0);
    if (!owns) { cfd = fd; IOTRACE("on_close_fd: dup(%d) failed errno=%d\n", fd, errno); }
    QItem it{QKind::Cancel}; it.fd = cfd; it.owns_fd = owns;
    if (r->is_owner()) {
        if (!submit_now(r, it) && owns) ::close(cfd);   // nothing prepared: release it now
        return;
    }
    { std::lock_guard<std::mutex> lk(r->qmx); r->q.push_back(it); }
    r->wake();
}

//  Async connect (ConnectEx). The target sockaddr is copied into the OVERLAPPED so it
//  survives a cross-thread marshal to the owner. Success CQE res == 0 flows through the
//  normal success path (0 bytes); a refused/failed connect surfaces via the failure path.
extern "C" BOOL p2p_iocp_connect(int fd, const void* addr, int addrlen, void* ov) {
    IoRing* r = ring_for_fd(fd);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    auto* o = reinterpret_cast<LPOVERLAPPED>(ov);
    o->Internal = 0; o->InternalHigh = 0;
    o->_p2p_fd = fd; o->_p2p_ring = r; o->_p2p_op = P2POP_CONNECT;
    o->_p2p_key = r->key_for(fd);
    if (addrlen < 0) addrlen = 0;
    if (addrlen > (int)sizeof o->_p2p_addr) addrlen = (int)sizeof o->_p2p_addr;
    std::memcpy(o->_p2p_addr, addr, (size_t)addrlen);
    o->_p2p_addrlen = addrlen;
    if (r->is_owner()) {
        QItem it{QKind::Connect}; it.fd = fd; it.ov = o;
        if (!submit_now(r, it)) { SetLastError(ERROR_NO_SYSTEM_RESOURCES); return FALSE; }
    } else {
        { std::lock_guard<std::mutex> lk(r->qmx);
          QItem it{QKind::Connect}; it.fd = fd; it.ov = o; r->q.push_back(it); }
        r->wake();
    }
    SetLastError(ERROR_IO_PENDING);
    return FALSE;
}

//  Async accept (AcceptEx). The accepted fd lands in `acceptsock` (the pre-created accept
//  socket HANDLE) at completion (§5.1); until then only the listen fd is submitted.
extern "C" BOOL p2p_iocp_accept(int listen_fd, void* acceptsock, void* ov) {
    IoRing* r = ring_for_fd(listen_fd);
    if (!r) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    auto* o = reinterpret_cast<LPOVERLAPPED>(ov);
    o->Internal = 0; o->InternalHigh = 0;
    o->_p2p_fd = listen_fd; o->_p2p_ring = r; o->_p2p_op = P2POP_ACCEPT;
    o->_p2p_key = r->key_for(listen_fd);
    o->_p2p_acceptsock = acceptsock;
    if (r->is_owner()) {
        QItem it{QKind::Accept}; it.fd = listen_fd; it.ov = o;
        if (!submit_now(r, it)) { SetLastError(ERROR_NO_SYSTEM_RESOURCES); return FALSE; }
    } else {
        { std::lock_guard<std::mutex> lk(r->qmx);
          QItem it{QKind::Accept}; it.fd = listen_fd; it.ov = o; r->q.push_back(it); }
        r->wake();
    }
    SetLastError(ERROR_IO_PENDING);
    return FALSE;
}

extern "C" BOOL p2p_iocp_close_port(HANDLE Port) { return p2p_iocp_destroy(Port); }

#endif // !_WIN32
