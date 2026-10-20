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
//  Platform layer — IOCP over EPOLL. The second Linux backend, chosen at build time by
//  P2P_IO_BACKEND=epoll in place of p2piocp.cpp (io_uring). Same public surface
//  (p2piocp.h and the extern "C" hooks p2pfile.h / p2psock.h / p2pserial.h call), same
//  completion semantics, no liburing.
//
//  WHY IT EXISTS. Android does not let an ordinary app use io_uring (the syscalls are
//  blocked by its seccomp policy), and older Linux kernels lack the io_uring features
//  p2piocp.cpp needs. epoll is everywhere.
//
//  HOW A COMPLETION MODEL RUNS OVER A READINESS ONE.
//   - Each port owns an epoll fd and an eventfd (the wake). Each fd associated with the
//     port has two ordered queues of pending OVERLAPPEDs: one for ops that wait for
//     input (read, accept, poll) and one for ops that wait for output (write, connect).
//   - Submitting an op TRIES it at once with a nonblocking syscall when its queue is
//     empty. A result -- data, an error, an accepted fd -- becomes a completion on the
//     port's ready list; EAGAIN puts the op on its queue and epoll interest follows the
//     queues (level-triggered: EPOLLIN while input ops wait, EPOLLOUT while output ops do).
//   - GetQueuedCompletionStatus hands back posted packets first (PQCS priority, as the
//     io_uring backend does), then ready completions, and otherwise waits in epoll_wait;
//     each readiness event retries the head of the matching queue until it would block.
//   - So a completion packet is ALWAYS delivered, never a synchronous success: the Fix-4
//     invariant holds by construction, exactly as io_uring's "a CQE even for a ready op".
//   - A write completes only when the WHOLE buffer is out (Win32's all-or-error contract,
//     p2piocp.cpp case (f)): a short send leaves the op at the head of its queue with
//     _p2p_wdone advanced.
//
//  THREADS. All per-port state is under one mutex, and any thread may submit: there is no
//  owner-thread marshalling (io_uring needed it for SINGLE_ISSUER; epoll_ctl and nonblocking
//  syscalls are thread-safe). The only use of "owner" left is the fault-injection contract
//  below. Syscalls made under the mutex are all nonblocking, except a read/write on a
//  regular file, which epoll cannot watch and which is done synchronously.
//
//  CLOSE. p2p_iocp_on_close_fd runs BEFORE the caller closes the fd (p2psock.h closesocket,
//  p2pfile.h CloseHandle), from whatever thread. It completes every queued op of that fd as
//  ERROR_OPERATION_ABORTED and removes the fd from epoll right there, under the mutex, so
//  nothing can be left to resolve after the fd number has been reused -- the hazard
//  p2piocp.cpp needs dup() for does not arise here. A readiness event already returned for
//  the old fd and processed after reuse only RETRIES the new fd's ops, which is harmless:
//  a nonblocking retry that would block leaves them queued.
//
//  SOCKETS stay in whatever blocking mode their owner set: reads and writes use
//  MSG_DONTWAIT. Only a connecting socket and a listening socket are made O_NONBLOCK
//  (connect(2) and accept(2) have no per-call flag), and nothing else uses those
//  synchronously. Writes use MSG_NOSIGNAL; the SIGPIPE policy is installed as well.
//
#if !defined(_WIN32)

#include "p2piocp.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace {

static bool p2p_trace_on() { static int v = getenv("P2P_IOCP_TRACE") ? 1 : 0; return v; }
#define IOTRACE(...) do { if (p2p_trace_on()) { std::fprintf(stderr, "[iocp-epoll] " __VA_ARGS__); std::fflush(stderr); } } while(0)

//  P2P_IOCP_FAULT=sqe: the same test hook as the io_uring backend. Here it makes every
//  submission behave as if the port could not take the op (epoll_ctl refused), so the
//  failure contract checks/iocp_test.cpp case (13) holds both backends to is exercised:
//  on the owner thread the submit fails synchronously with ERROR_NO_SYSTEM_RESOURCES; on
//  any other thread the caller is told ERROR_IO_PENDING and the op comes back as a FAILED
//  completion on its own OVERLAPPED.
static bool p2p_fault_sqe() {
    static int v = [] { const char* e = getenv("P2P_IOCP_FAULT");
                        return (e && std::strstr(e, "sqe")) ? 1 : 0; }();
    return v != 0;
}

enum class FdKind : std::uint8_t { Unknown, Socket, Regular, Other };

struct Op {
    OVERLAPPED* ov;
    bool        started = false;     // CONNECT: connect(2) issued, waiting for writability
};

struct FdState {
    FdKind          kind = FdKind::Unknown;
    bool            in_epoll = false;     // added to the epoll set
    bool            pollable = true;      // false: epoll refused it (regular file)
    std::uint32_t   events = 0;           // current interest
    std::deque<Op>  in;                   // READ / ACCEPT / POLL, in submission order
    std::deque<Op>  out;                  // WRITE / CONNECT, in submission order
};

//  A finished op waiting to be handed back. `res` follows the io_uring CQE convention so
//  the translation in GetQueuedCompletionStatus is the same as p2piocp.cpp's: >= 0 is a
//  result (bytes, or the accepted fd), < 0 is -errno.
struct Done { OVERLAPPED* ov; int res; };

struct Post { DWORD bytes; ULONG_PTR key; OVERLAPPED* ov; DWORD err; };

struct EpPort {
    int                                   epfd = -1;
    int                                   evfd = -1;
    std::atomic<std::uintptr_t>           owner{0};      // first GQCS caller (fault contract only)
    std::mutex                            mx;
    std::deque<Post>                      posts;         // guarded
    std::deque<Done>                      done;          // guarded
    std::unordered_map<int, ULONG_PTR>    assoc;         // fd -> completion key, guarded
    std::unordered_map<int, std::unique_ptr<FdState>> fds;   // guarded
    int                                   sleepers = 0;  // threads in epoll_wait, guarded

    void wake() { std::uint64_t one = 1; ssize_t r = ::write(evfd, &one, sizeof one); (void)r; }
    bool is_owner() const {
        std::uintptr_t o = owner.load(std::memory_order_acquire);
        return o != 0 && o == (std::uintptr_t)pthread_self();
    }
};

inline EpPort* port_of(HANDLE port) {
    return (port && port->kind == HKind::Iocp) ? static_cast<EpPort*>(port->impl) : nullptr;
}

//  fd -> port registry for the portless hooks, and the live-port set that makes a late
//  PostQueuedCompletionStatus safe. Same roles and lock discipline as p2piocp.cpp.
std::mutex                        g_reg_mx;
std::unordered_map<int, EpPort*>  g_fd_port;
void    reg_fd(int fd, EpPort* p) { std::lock_guard<std::mutex> lk(g_reg_mx); g_fd_port[fd] = p; }
EpPort* port_for_fd(int fd) {
    std::lock_guard<std::mutex> lk(g_reg_mx);
    auto it = g_fd_port.find(fd); return it == g_fd_port.end() ? nullptr : it->second;
}
EpPort* unreg_fd(int fd) {
    std::lock_guard<std::mutex> lk(g_reg_mx);
    auto it = g_fd_port.find(fd); if (it == g_fd_port.end()) return nullptr;
    EpPort* p = it->second; g_fd_port.erase(it); return p;
}
void unreg_port_fds(EpPort* p) {
    std::lock_guard<std::mutex> lk(g_reg_mx);
    for (auto it = g_fd_port.begin(); it != g_fd_port.end();)
        if (it->second == p) it = g_fd_port.erase(it); else ++it;
}

std::mutex                   g_port_mx;
std::unordered_set<HANDLE>   g_live_ports;
void reg_port(HANDLE p) { std::lock_guard<std::mutex> lk(g_port_mx); g_live_ports.insert(p); }

// ---------------------------------------------------------------------------
//  Per-fd machinery. Every function here runs with p->mx held.
// ---------------------------------------------------------------------------

FdState& state_of(EpPort* p, int fd) {
    auto& s = p->fds[fd];
    if (!s) {
        s = std::make_unique<FdState>();
        struct stat st{};
        if (::fstat(fd, &st) == 0)
            s->kind = S_ISSOCK(st.st_mode) ? FdKind::Socket
                    : S_ISREG(st.st_mode)  ? FdKind::Regular : FdKind::Other;
        if (s->kind == FdKind::Regular) s->pollable = false;
    }
    return *s;
}

//  Bring epoll interest in line with the queues. Returns false only when the fd could not
//  be watched at all for a reason other than "regular file" -- the caller then fails the op.
bool sync_interest(EpPort* p, int fd, FdState& s) {
    if (!s.pollable) return true;
    std::uint32_t want = (s.in.empty() ? 0u : (std::uint32_t)EPOLLIN) |
                         (s.out.empty() ? 0u : (std::uint32_t)EPOLLOUT);
    if (s.in_epoll && want == s.events) return true;
    epoll_event ev{};
    ev.events = want;
    ev.data.fd = fd;
    //  The cached in_epoll can be wrong both ways: an fd closed WITHOUT the shim's close
    //  hook leaves the set silently (the kernel drops it with its file), and its number may
    //  then be reused -- so MOD falls back to ADD on ENOENT, and ADD to MOD on EEXIST.
    int op = s.in_epoll ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    if (op == EPOLL_CTL_ADD && want == 0) return true;
    for (int tries = 0; tries < 2; ++tries) {
        if (::epoll_ctl(p->epfd, op, fd, &ev) == 0) { s.in_epoll = true; s.events = want; return true; }
        if (op == EPOLL_CTL_ADD && errno == EPERM) { s.pollable = false; return true; }   // regular file
        if (op == EPOLL_CTL_MOD && errno == ENOENT) { op = EPOLL_CTL_ADD; continue; }
        if (op == EPOLL_CTL_ADD && errno == EEXIST) { op = EPOLL_CTL_MOD; continue; }
        break;
    }
    IOTRACE("epoll_ctl fd=%d op=%d failed errno=%d\n", fd, op, errno);
    s.in_epoll = false;
    return false;
}

//  One nonblocking attempt at an op. Returns true when it finished (with *res set),
//  false when it would block.
bool try_op(int fd, FdState& s, Op& op, int* res) {
    OVERLAPPED* ov = op.ov;
    switch (ov->_p2p_op) {
    case P2POP_READ: {
        ssize_t n;
        if (s.kind == FdKind::Socket)
            n = ::recv(fd, ov->_p2p_wbuf, ov->_p2p_wlen, MSG_DONTWAIT);
        else {
            if (s.pollable) {          // a tty or pipe: read only what is there
                pollfd pf{fd, POLLIN, 0};
                int pr = ::poll(&pf, 1, 0);
                if (pr == 0) return false;
                if (pr < 0) { if (errno == EINTR) return false; *res = -errno; return true; }
            }
            n = ::read(fd, ov->_p2p_wbuf, ov->_p2p_wlen);
        }
        if (n >= 0) { *res = (int)n; return true; }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return false;
        *res = -errno; return true;
    }
    case P2POP_WRITE: {
        while (ov->_p2p_wdone < ov->_p2p_wlen) {
            const char* b = (const char*)ov->_p2p_wbuf + ov->_p2p_wdone;
            size_t left = ov->_p2p_wlen - ov->_p2p_wdone;
            ssize_t n = (s.kind == FdKind::Socket) ? ::send(fd, b, left, MSG_DONTWAIT | MSG_NOSIGNAL)
                                                   : ::write(fd, b, left);
            if (n > 0) { ov->_p2p_wdone += (DWORD)n; continue; }
            if (n < 0 && errno == EINTR) continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false;
            *res = n < 0 ? -errno : -EIO;
            return true;
        }
        *res = (int)ov->_p2p_wlen;
        return true;
    }
    case P2POP_POLL: {
        pollfd pf{fd, POLLIN, 0};
        int pr = ::poll(&pf, 1, 0);
        if (pr == 0) return false;
        if (pr < 0) { if (errno == EINTR) return false; *res = -errno; return true; }
        *res = 0; return true;
    }
    case P2POP_ACCEPT: {
        int nfd = ::accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
        if (nfd >= 0) { *res = nfd; return true; }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED) return false;
        *res = -errno; return true;
    }
    case P2POP_CONNECT: {
        if (!op.started) {
            op.started = true;
            int r = ::connect(fd, reinterpret_cast<const sockaddr*>(ov->_p2p_addr), (socklen_t)ov->_p2p_addrlen);
            if (r == 0) { *res = 0; return true; }
            if (errno == EINPROGRESS || errno == EINTR || errno == EALREADY) return false;
            *res = -errno; return true;
        }
        pollfd pf{fd, POLLOUT, 0};
        if (::poll(&pf, 1, 0) <= 0) return false;
        int soerr = 0; socklen_t sl = sizeof soerr;
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) != 0) soerr = errno;
        *res = soerr ? -soerr : 0;
        return true;
    }
    default:
        *res = -EINVAL; return true;
    }
}

//  Retry the head of a queue until it would block; every finished op becomes a completion.
//  Returns the number completed.
int pump_queue(EpPort* p, int fd, FdState& s, std::deque<Op>& q) {
    int n = 0;
    while (!q.empty()) {
        int res = 0;
        if (!try_op(fd, s, q.front(), &res)) break;
        p->done.push_back(Done{q.front().ov, res});
        q.pop_front();
        ++n;
    }
    return n;
}

//  Queue an op on its fd and try it. Returns false if the fd could not be watched (the op
//  has then been taken back off the queue: nothing was promised for it).
bool enqueue(EpPort* p, int fd, OVERLAPPED* ov) {
    FdState& s = state_of(p, fd);
    const bool input = ov->_p2p_op == P2POP_READ || ov->_p2p_op == P2POP_ACCEPT || ov->_p2p_op == P2POP_POLL;
    std::deque<Op>& q = input ? s.in : s.out;
    q.push_back(Op{ov});
    if (q.size() == 1) pump_queue(p, fd, s, q);        // only the head may run: order is kept
    if (!sync_interest(p, fd, s)) {
        for (auto it = q.begin(); it != q.end(); ++it)
            if (it->ov == ov) { q.erase(it); return false; }
        //  It already completed: the completion stands, interest just could not be updated.
        return true;
    }
    return true;
}

//  Complete every queued op of `fd` as cancelled and forget the fd. Under p->mx.
void cancel_fd_locked(EpPort* p, int fd) {
    auto it = p->fds.find(fd);
    if (it == p->fds.end()) return;
    FdState& s = *it->second;
    for (Op& o : s.in)  p->done.push_back(Done{o.ov, -ECANCELED});
    for (Op& o : s.out) p->done.push_back(Done{o.ov, -ECANCELED});
    s.in.clear(); s.out.clear();
    if (s.in_epoll) ::epoll_ctl(p->epfd, EPOLL_CTL_DEL, fd, nullptr);
    p->fds.erase(it);
}

//  The common submit path for every op kind. `ov` has its shim fields filled.
BOOL submit(EpPort* p, int fd, OVERLAPPED* ov) {
    bool wake = false;
    if (p2p_fault_sqe()) {
        if (p->is_owner()) { SetLastError(ERROR_NO_SYSTEM_RESOURCES); return FALSE; }
        { std::lock_guard<std::mutex> lk(p->mx);
          p->posts.push_back(Post{0, ov->_p2p_key, ov, ERROR_NO_SYSTEM_RESOURCES}); }
        p->wake();
        SetLastError(ERROR_IO_PENDING);
        return FALSE;
    }
    {
        std::lock_guard<std::mutex> lk(p->mx);
        const size_t before = p->done.size();
        if (!enqueue(p, fd, ov)) {
            //  Nothing was promised yet: refuse synchronously, as Win32's initial
            //  WSARecv/ReadFile would, and the transports already handle.
            SetLastError(ERROR_NO_SYSTEM_RESOURCES);
            return FALSE;
        }
        wake = p->done.size() != before && p->sleepers > 0;
    }
    if (wake) p->wake();
    SetLastError(ERROR_IO_PENDING);    // Fix-4: a completion is always posted
    return FALSE;
}

void fill_ov(OVERLAPPED* ov, EpPort* p, int fd, int op, ULONG_PTR key) {
    ov->Internal = 0; ov->InternalHigh = 0;
    ov->_p2p_fd = fd; ov->_p2p_ring = p; ov->_p2p_op = op; ov->_p2p_key = key;
}

ULONG_PTR key_for(EpPort* p, int fd) {
    std::lock_guard<std::mutex> lk(p->mx);
    auto it = p->assoc.find(fd);
    return it == p->assoc.end() ? 0 : it->second;
}

BOOL submit_rw_port(EpPort* p, int fd, void* buf, DWORD n, LPOVERLAPPED ov, bool write) {
    fill_ov(ov, p, fd, write ? P2POP_WRITE : P2POP_READ, key_for(p, fd));
    ov->_p2p_wbuf = buf; ov->_p2p_wlen = n; ov->_p2p_wdone = 0;
    return submit(p, fd, ov);
}

void set_nonblock(int fd) {
    int fl = ::fcntl(fd, F_GETFL, 0);
    if (fl >= 0 && !(fl & O_NONBLOCK)) ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

} // namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingPort,
                              ULONG_PTR CompletionKey, DWORD /*NumberOfConcurrentThreads*/) {
    if (ExistingPort) {
        EpPort* p = port_of(ExistingPort);
        if (!p || !FileHandle || FileHandle->kind != HKind::Fd) {
            SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
        { std::lock_guard<std::mutex> lk(p->mx); p->assoc[FileHandle->fd] = CompletionKey; }
        reg_fd(FileHandle->fd, p);
        return ExistingPort;
    }
    p2p_ignore_sigpipe();                 // the same policy as the io_uring backend (p2ptypes.h)
    auto* p = new EpPort();
    p->epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (p->epfd < 0) { DWORD e = win32_from_errno(errno); delete p; SetLastError(e); return nullptr; }
    p->evfd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (p->evfd < 0) { DWORD e = win32_from_errno(errno); ::close(p->epfd); delete p; SetLastError(e); return nullptr; }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = p->evfd;
    if (::epoll_ctl(p->epfd, EPOLL_CTL_ADD, p->evfd, &ev) != 0) {
        DWORD e = win32_from_errno(errno); ::close(p->evfd); ::close(p->epfd); delete p; SetLastError(e); return nullptr; }
    if (FileHandle && FileHandle->kind == HKind::Fd) {
        p->assoc[FileHandle->fd] = CompletionKey;
        reg_fd(FileHandle->fd, p);
    }
    HANDLE port = p2p_handle_new(HKind::Iocp, -1, p);
    reg_port(port);
    return port;
}

BOOL GetQueuedCompletionStatus(HANDLE Port, LPDWORD lpNumberOfBytes,
                               PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped,
                               DWORD dwMilliseconds) {
    EpPort* p = port_of(Port);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    std::uintptr_t expect = 0;
    p->owner.compare_exchange_strong(expect, (std::uintptr_t)pthread_self());

    using clock = std::chrono::steady_clock;
    const bool infinite = (dwMilliseconds == INFINITE);
    const auto deadline = clock::now() + std::chrono::milliseconds(infinite ? 0 : dwMilliseconds);

    epoll_event evs[64];
    for (;;) {
        Post post{};
        Done d{};
        bool have_post = false, have_done = false;
        {
            std::lock_guard<std::mutex> lk(p->mx);
            if (!p->posts.empty())     { post = p->posts.front(); p->posts.pop_front(); have_post = true; }
            else if (!p->done.empty()) { d = p->done.front(); p->done.pop_front(); have_done = true; }
            else ++p->sleepers;        // committed to waiting: producers now wake us
        }
        if (have_post) {                                   // PQCS path
            if (lpNumberOfBytes) *lpNumberOfBytes = post.err ? 0u : post.bytes;
            if (lpCompletionKey) *lpCompletionKey = post.key;
            if (lpOverlapped)    *lpOverlapped    = post.ov;
            if (post.err) {
                if (post.ov) { post.ov->Internal = post.err; post.ov->InternalHigh = 0; }
                SetLastError(post.err);
                return FALSE;
            }
            return TRUE;
        }
        if (have_done) {
            OVERLAPPED* ov = d.ov;
            const int res = d.res;
            if (lpCompletionKey) *lpCompletionKey = ov->_p2p_key;   // bound at submission
            if (lpOverlapped)    *lpOverlapped    = ov;
            if (res < 0) {                                          // failed / cancelled
                ov->Internal = (ULONG_PTR)(-res); ov->InternalHigh = 0;
                if (lpNumberOfBytes) *lpNumberOfBytes = 0;
                SetLastError(res == -ECANCELED ? ERROR_OPERATION_ABORTED : win32_from_errno(-res));
                return FALSE;
            }
            if (res == 0 && ov->_p2p_op == P2POP_READ) {            // graceful close -> EOF
                ov->Internal = 0; ov->InternalHigh = 0;
                if (lpNumberOfBytes) *lpNumberOfBytes = 0;
                SetLastError(ERROR_HANDLE_EOF); return FALSE;
            }
            if (ov->_p2p_op == P2POP_ACCEPT) {                      // res == the accepted fd
                auto* asock = static_cast<P2PHandle*>(ov->_p2p_acceptsock);
                if (asock) {
                    if (asock->fd >= 0) ::close(asock->fd);          // the pre-created placeholder
                    asock->fd = res;
                    { std::lock_guard<std::mutex> lk(p->mx);
                      auto it = p->assoc.find(ov->_p2p_fd);
                      p->assoc[res] = it != p->assoc.end() ? it->second : 0; }
                    reg_fd(res, p);
                } else
                    ::close(res);                                   // nobody to hand it to
                ov->Internal = 0; ov->InternalHigh = 0;
                if (lpNumberOfBytes) *lpNumberOfBytes = 0;
                return TRUE;
            }
            const DWORD bytes = (ov->_p2p_op == P2POP_POLL || ov->_p2p_op == P2POP_CONNECT) ? 0u : (DWORD)res;
            ov->Internal = 0; ov->InternalHigh = bytes;
            if (lpNumberOfBytes) *lpNumberOfBytes = bytes;
            return TRUE;
        }

        int wait_ms = -1;
        if (!infinite) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
            wait_ms = left > 0 ? (int)left : 0;
        }
        const int n = ::epoll_wait(p->epfd, evs, 64, wait_ms);
        const int werr = errno;
        bool any = false;
        {
            std::lock_guard<std::mutex> lk(p->mx);
            --p->sleepers;
            for (int i = 0; i < n; ++i) {
                const int fd = evs[i].data.fd;
                if (fd == p->evfd) {
                    std::uint64_t sink; ssize_t r = ::read(p->evfd, &sink, sizeof sink); (void)r;
                    any = true;
                    continue;
                }
                auto it = p->fds.find(fd);
                if (it == p->fds.end()) continue;          // forgotten (cancelled) since the wait
                FdState& s = *it->second;
                const std::uint32_t e = evs[i].events;
                //  An error or hang-up is reported through the ops themselves: their
                //  syscalls return it, so both queues are retried.
                if (e & (EPOLLIN | EPOLLERR | EPOLLHUP))  pump_queue(p, fd, s, s.in);
                if (e & (EPOLLOUT | EPOLLERR | EPOLLHUP)) pump_queue(p, fd, s, s.out);
                sync_interest(p, fd, s);
                any = true;
            }
            if (!p->posts.empty() || !p->done.empty()) any = true;
        }
        if (n < 0 && werr != EINTR) {
            if (lpOverlapped) *lpOverlapped = nullptr;
            SetLastError(win32_from_errno(werr));
            return FALSE;
        }
        if (!any && !infinite && clock::now() >= deadline) {        // timeout / no work
            if (lpOverlapped) *lpOverlapped = nullptr;
            SetLastError(WAIT_TIMEOUT);
            return FALSE;
        }
    }
}

BOOL PostQueuedCompletionStatus(HANDLE Port, DWORD dwNumberOfBytesTransferred,
                                ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped) {
    std::lock_guard<std::mutex> lkp(g_port_mx);       // see p2piocp.cpp: safe against destroy
    if (g_live_ports.find(Port) == g_live_ports.end()) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    EpPort* p = port_of(Port);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    { std::lock_guard<std::mutex> lk(p->mx);
      p->posts.push_back(Post{dwNumberOfBytesTransferred, dwCompletionKey, lpOverlapped, 0}); }
    p->wake();
    return TRUE;
}

BOOL p2p_iocp_read(HANDLE Port, HANDLE File, void* buf, DWORD n, LPOVERLAPPED ov) {
    EpPort* p = port_of(Port);
    if (!p || !File || File->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    return submit_rw_port(p, File->fd, buf, n, ov, false);
}
BOOL p2p_iocp_write(HANDLE Port, HANDLE File, const void* buf, DWORD n, LPOVERLAPPED ov) {
    EpPort* p = port_of(Port);
    if (!p || !File || File->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    return submit_rw_port(p, File->fd, const_cast<void*>(buf), n, ov, true);
}

BOOL p2p_iocp_cancel(HANDLE Port, HANDLE File) {
    EpPort* p = port_of(Port);
    if (!p || !File || File->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    bool wake;
    { std::lock_guard<std::mutex> lk(p->mx);
      const size_t before = p->done.size();
      cancel_fd_locked(p, File->fd);
      wake = p->done.size() != before; }
    if (wake) p->wake();
    return TRUE;
}

BOOL p2p_iocp_destroy(HANDLE Port) {
    EpPort* p = port_of(Port);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    { std::lock_guard<std::mutex> lk(g_port_mx); g_live_ports.erase(Port); }
    unreg_port_fds(p);
    //  Pending ops die with the port, as io_uring_queue_exit cancels the ring's; nothing
    //  is left to deliver them to.
    ::close(p->epfd);
    ::close(p->evfd);
    delete p;
    p2p_handle_retire(Port);
    return TRUE;
}

// ---------------------------------------------------------------------------
//  extern "C" hooks (weak in p2pfile.h / p2psock.h / p2pserial.h).
// ---------------------------------------------------------------------------
extern "C" BOOL p2p_iocp_submit_rw(int fd, void* buf, DWORD n, void* ov, int isWrite) {
    EpPort* p = port_for_fd(fd);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    return submit_rw_port(p, fd, buf, n, reinterpret_cast<LPOVERLAPPED>(ov), isWrite != 0);
}

extern "C" BOOL p2p_iocp_poll(int fd, void* ov) {
    EpPort* p = port_for_fd(fd);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    auto* o = reinterpret_cast<LPOVERLAPPED>(ov);
    fill_ov(o, p, fd, P2POP_POLL, key_for(p, fd));
    return submit(p, fd, o);
}

extern "C" void p2p_iocp_on_close_fd(int fd) {
    EpPort* p = unreg_fd(fd);
    if (!p) return;
    bool wake;
    { std::lock_guard<std::mutex> lk(p->mx);
      p->assoc.erase(fd);
      const size_t before = p->done.size();
      cancel_fd_locked(p, fd);                 // synchronous: the fd is still open here
      wake = p->done.size() != before; }
    if (wake) p->wake();
}

extern "C" BOOL p2p_iocp_connect(int fd, const void* addr, int addrlen, void* ov) {
    EpPort* p = port_for_fd(fd);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    auto* o = reinterpret_cast<LPOVERLAPPED>(ov);
    fill_ov(o, p, fd, P2POP_CONNECT, key_for(p, fd));
    if (addrlen < 0) addrlen = 0;
    if (addrlen > (int)sizeof o->_p2p_addr) addrlen = (int)sizeof o->_p2p_addr;
    std::memcpy(o->_p2p_addr, addr, (size_t)addrlen);
    o->_p2p_addrlen = addrlen;
    set_nonblock(fd);
    return submit(p, fd, o);
}

extern "C" BOOL p2p_iocp_accept(int listen_fd, void* acceptsock, void* ov) {
    EpPort* p = port_for_fd(listen_fd);
    if (!p) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    auto* o = reinterpret_cast<LPOVERLAPPED>(ov);
    fill_ov(o, p, listen_fd, P2POP_ACCEPT, key_for(p, listen_fd));
    o->_p2p_acceptsock = acceptsock;
    set_nonblock(listen_fd);
    return submit(p, listen_fd, o);
}

extern "C" BOOL p2p_iocp_close_port(HANDLE Port) { return p2p_iocp_destroy(Port); }

#endif // !_WIN32
