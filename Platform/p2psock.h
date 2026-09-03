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
//  Platform layer — sockets: BSD/Winsock shims over the tagged SOCKET.
//
//  Part of the Msgcore + TargetCore Linux port (see the Linux port plan §4.1, §5.1, §6.2).
//
//  _WIN32 : pass-through (WinSock2 + mswsock). Linux: SOCKET == HANDLE == P2PHandle*
//  (§4.1), so socket-returning calls allocate a P2PHandle{Fd} and BSD-call shims unwrap
//  ->fd. Most POSIX socket fns are added as C++ *overloads* that take SOCKET (distinct
//  from the C-linkage int-taking originals, which the overloads call via ->fd). socket()
//  is the exception (same params, different return) — a helper + macro (the `(socket)`
//  trick prevents self-recursion). Winsock-only names (closesocket/ioctlsocket/WSA*) are
//  plain inline shims. AcceptEx/ConnectEx async wiring (io_uring) is the Wsa-transport
//  bring-up; here they get the types + declarations so the transport TUs parse.
//
#pragma once
#include "p2ptypes.h"

#if defined(_WIN32)
  #include <WinSock2.h>
  #include <ws2tcpip.h>
  #include <mswsock.h>
#else
  #include "p2piocp.h"      // OVERLAPPED / LPOVERLAPPED for the *Ex signatures
  #include <sys/socket.h>
  #include <sys/un.h>       // AF_UNIX (named-pipe transport)
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <sys/ioctl.h>
  #include <sys/stat.h>     // mkdir (pipe socket dir)
  #include <fcntl.h>
  #include <unistd.h>
  #include <cstring>
  #include <cstdlib>        // getenv
  #include <cerrno>

  // -------------------------------------------------------------------------
  //  Win/Winsock macros the transport sources name. FAR/OPTIONAL/PASCAL/WSAAPI are
  //  empty (Win16/SAL residue); without FAR, `const struct sockaddr FAR *name` fails to
  //  parse — the transitive blocker in P2PeerConWsa.h that broke P2PeerHub/Events.
  // -------------------------------------------------------------------------
  #ifndef FAR
    #define FAR
  #endif
  #ifndef OPTIONAL
    #define OPTIONAL
  #endif
  #ifndef PASCAL
    #define PASCAL
  #endif
  #ifndef WSAAPI
    #define WSAAPI
  #endif
  #ifndef SOCKET_ERROR
    #define SOCKET_ERROR (-1)
  #endif
  //  WSA error codes the code tests (Win numeric values, so a peer/log comparing them
  //  agrees with Windows). GetLastError()/errno-mirror carries them via win32_from_errno
  //  where relevant; these are the ones compared by name.
  #ifndef WSAEWOULDBLOCK
    #define WSAEWOULDBLOCK  10035
    #define WSAEINPROGRESS  10036
    #define WSAENOTSOCK     10038
    #define WSAEADDRINUSE   10048
    #define WSAENOBUFS      10055
    #define WSAETIMEDOUT    10060
    #define WSAECONNREFUSED 10061
    #define WSAECONNRESET   10054
    #define WSAECONNABORTED 10053
    #define WSAENOTCONN     10057
    #define WSAESHUTDOWN    10058
  #endif
  #ifndef SD_RECEIVE
    #define SD_RECEIVE 0
    #define SD_SEND    1
    #define SD_BOTH    2
  #endif
  #ifndef SO_UPDATE_ACCEPT_CONTEXT
    #define SO_UPDATE_ACCEPT_CONTEXT 0x700B   // no-op on Linux (accepted fd is ready)
  #endif
  #ifndef SO_UPDATE_CONNECT_CONTEXT
    #define SO_UPDATE_CONNECT_CONTEXT 0x7010  // no-op on Linux (connected fd is ready)
  #endif

  //  BSD scalar aliases some sources use.
  #ifndef _P2P_ULONG_T
    #define _P2P_ULONG_T
    using u_long  = unsigned long;
    using u_short = unsigned short;
    using u_char  = unsigned char;
  #endif

  //  Win socket type spellings over the POSIX structs.
  using SOCKADDR     = sockaddr;
  using PSOCKADDR    = sockaddr*;
  using LPSOCKADDR   = sockaddr*;
  using SOCKADDR_IN  = sockaddr_in;
  using PSOCKADDR_IN = sockaddr_in*;
  using IN_ADDR      = in_addr;
  using LPHOSTENT    = hostent*;

  struct WSAData {
      WORD  wVersion;        WORD  wHighVersion;
      char  szDescription[257]; char szSystemStatus[129];
      unsigned short iMaxSockets; unsigned short iMaxUdpDg;
      char* lpVendorInfo;
  };
  using WSADATA   = WSAData;
  using LPWSADATA = WSAData*;
  struct WSABUF { unsigned long len; char* buf; };
  using LPWSABUF = WSABUF*;

  //  Runtime-resolved extension fn pointer types (WSAIoctl on Windows; io_uring here).
  using LPFN_CONNECTEX = BOOL (*)(SOCKET, const sockaddr*, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
  using LPFN_ACCEPTEX  = BOOL (*)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);

  // -------------------------------------------------------------------------
  //  Small helper: translate a POSIX -1 return into SOCKET_ERROR + last-error.
  // -------------------------------------------------------------------------
  inline int p2p_sock_ret(int r) {
      if (r < 0) { SetLastError(win32_from_errno(errno)); return SOCKET_ERROR; }
      return r;
  }

  // -------------------------------------------------------------------------
  //  WSA lifecycle / error (§6.2: startup no-ops; last-error mirrors the errno shim).
  // -------------------------------------------------------------------------
  #ifndef MAKEWORD
  #define MAKEWORD(a,b) ((WORD)(((BYTE)((a) & 0xff)) | ((WORD)((BYTE)((b) & 0xff))) << 8))
  #endif
  //  WSAStartup also installs the SIGPIPE policy (p2ptypes.h): Winsock semantics are
  //  "a write to a dead peer returns an error", and on Linux that needs the default
  //  terminate-the-process disposition replaced. This is the first shim entry point
  //  any socket user calls, so it is where the process-wide policy is set.
  inline int  WSAStartup(WORD, LPWSADATA d) {
      p2p_ignore_sigpipe();
      if (d) std::memset(d, 0, sizeof *d);
      return 0;
  }
  inline int  WSACleanup() { return 0; }
  inline int  WSAGetLastError() { return (int)GetLastError(); }
  inline void WSASetLastError(int e) { SetLastError((DWORD)e); }

  // -------------------------------------------------------------------------
  //  socket() / WSASocket() — allocate a P2PHandle{Fd}. The macro routes the call; the
  //  helper reaches the real syscall via `(socket)` (parenthesized -> no macro expand).
  // -------------------------------------------------------------------------
  inline SOCKET p2p_socket(int af, int type, int proto) {
      int fd = (socket)(af, type, proto);
      if (fd < 0) { SetLastError(win32_from_errno(errno)); return INVALID_SOCKET; }
      return p2p_handle_new(HKind::Fd, fd, nullptr);
  }
  #define socket(af, type, proto) p2p_socket((af), (type), (proto))

  inline SOCKET WSASocketW(int af, int type, int proto,
                           void* = nullptr, unsigned = 0, DWORD = 0) {
      return p2p_socket(af, type, proto);
  }
  #define WSASocket WSASocketW

  //  closesocket: cancel any IOCP-associated in-flight ops (§5.6), close the fd, retire
  //  the handle (§4.1 -- retired, never freed, so a thread that still holds this SOCKET
  //  reads a closed handle rather than freed memory).
  inline int closesocket(SOCKET s) {
      if (!s) { SetLastError(ERROR_INVALID_HANDLE); return SOCKET_ERROR; }
      //  Already closed: Win32 fails the second closesocket rather than acting on it.
      if (s->retired.load(std::memory_order_acquire)) {
          SetLastError(ERROR_INVALID_HANDLE); return SOCKET_ERROR; }
      //  Claim the descriptor before touching it: exactly one closer wins, so a double
      //  close cannot land on a descriptor number that has since been recycled, and a
      //  thread reading the handle after this point sees -1 and fails cleanly instead of
      //  issuing a syscall on a descriptor being closed underneath it.
      int fd = s->fd.exchange(-1, std::memory_order_acq_rel);
      if (fd >= 0) {
          if (&p2p_iocp_on_close_fd) p2p_iocp_on_close_fd(fd);
          ::close(fd);
      }
      p2p_handle_retire(s);
      return 0;
  }

  //  ioctlsocket: only FIONBIO (non-blocking toggle) is on the live path -> fcntl. The
  //  arg pointer is templated because callers pass u_long* or DWORD* interchangeably
  //  (identical on Win LLP64, distinct widths on LP64 Linux); we only read it as a flag.
  template <class T>
  inline int ioctlsocket(SOCKET s, long cmd, T* argp) {
      if (!s) { SetLastError(ERROR_INVALID_HANDLE); return SOCKET_ERROR; }
      if (cmd == (long)FIONBIO) {
          int fl = ::fcntl(s->fd, F_GETFL, 0);
          if (fl < 0) return p2p_sock_ret(fl);
          if (argp && *argp) fl |= O_NONBLOCK; else fl &= ~O_NONBLOCK;
          return p2p_sock_ret(::fcntl(s->fd, F_SETFL, fl));
      }
      return p2p_sock_ret(::ioctl(s->fd, (unsigned long)cmd, argp));
  }

  //  InetPton (Unicode form on Windows) -> inet_pton over a UTF-8 copy of the address.
  inline int InetPtonW(int family, const wchar_t* str, void* addr) {
      char buf[128]; int i = 0;
      if (str) for (; str[i] && i < (int)sizeof buf - 1; ++i) buf[i] = (char)(unsigned char)str[i];
      buf[i] = 0;
      return ::inet_pton(family, buf, addr);
  }
  inline int InetPtonA(int family, const char* str, void* addr) { return ::inet_pton(family, str, addr); }
  #define InetPton InetPtonW

  // -------------------------------------------------------------------------
  //  BSD-call overloads taking SOCKET (distinct from the C originals they call).
  // -------------------------------------------------------------------------
  inline int bind(SOCKET s, const sockaddr* a, socklen_t l) {
      if (!s) return SOCKET_ERROR;
      // Windows lets a listener rebind a port that still has a connection in
      // TIME_WAIT; Linux returns EADDRINUSE unless SO_REUSEADDR is set. Every
      // bind() on the live path is a service/listen endpoint (clients connect
      // without an explicit bind), so enabling it here mirrors the Windows
      // rebind semantics and matches standard server practice.
      int one = 1;
      ::setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
      return p2p_sock_ret(::bind(s->fd, a, l)); }
  inline int listen(SOCKET s, int backlog) {
      return s ? p2p_sock_ret(::listen(s->fd, backlog)) : SOCKET_ERROR; }
  inline int connect(SOCKET s, const sockaddr* a, socklen_t l) {
      return s ? p2p_sock_ret(::connect(s->fd, a, l)) : SOCKET_ERROR; }
  inline SOCKET accept(SOCKET s, sockaddr* a, socklen_t* l) {
      if (!s) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_SOCKET; }
      int fd = ::accept(s->fd, a, l);
      if (fd < 0) { SetLastError(win32_from_errno(errno)); return INVALID_SOCKET; }
      return p2p_handle_new(HKind::Fd, fd, nullptr);
  }
  inline int setsockopt(SOCKET s, int level, int opt, const void* val, socklen_t len) {
      if (!s) return SOCKET_ERROR;
      // Windows-only IOCP context-update options: no-op on Linux. io_uring accept/
      // connect already hand back a fully-ready fd, so there is no AcceptEx/ConnectEx
      // context to inherit; the real ::setsockopt would fail these with ENOPROTOOPT.
      if (level == SOL_SOCKET &&
          (opt == SO_UPDATE_ACCEPT_CONTEXT || opt == SO_UPDATE_CONNECT_CONTEXT))
          return 0;
      return p2p_sock_ret(::setsockopt(s->fd, level, opt, val, len)); }
  inline int getsockopt(SOCKET s, int level, int opt, void* val, socklen_t* len) {
      return s ? p2p_sock_ret(::getsockopt(s->fd, level, opt, val, len)) : SOCKET_ERROR; }
  inline int getsockname(SOCKET s, sockaddr* a, socklen_t* l) {
      return s ? p2p_sock_ret(::getsockname(s->fd, a, l)) : SOCKET_ERROR; }
  inline int getpeername(SOCKET s, sockaddr* a, socklen_t* l) {
      return s ? p2p_sock_ret(::getpeername(s->fd, a, l)) : SOCKET_ERROR; }
  //  MSG_NOSIGNAL on the synchronous send: belt-and-braces with p2p_ignore_sigpipe()
  //  above, and correct even if a host application re-arms a SIGPIPE handler of its
  //  own. EPIPE still comes back through p2p_sock_ret as a last-error, which is the
  //  Win32 behaviour this call is standing in for.
  inline int send(SOCKET s, const char* buf, int len, int flags) {
      return s ? p2p_sock_ret((int)::send(s->fd, buf, (size_t)len, flags | MSG_NOSIGNAL))
               : SOCKET_ERROR; }
  inline int recv(SOCKET s, char* buf, int len, int flags) {
      return s ? p2p_sock_ret((int)::recv(s->fd, buf, (size_t)len, flags)) : SOCKET_ERROR; }
  inline int shutdown(SOCKET s, int how) {
      return s ? p2p_sock_ret(::shutdown(s->fd, how)) : SOCKET_ERROR; }

  // -------------------------------------------------------------------------
  //  Overlapped extension functions (AcceptEx / ConnectEx) — LinuxPortPlan §5.1.
  //  ConnectEx is resolved by the legacy code through WSAIoctl(SIO_GET_EXTENSION_
  //  FUNCTION_POINTER, WSAID_CONNECTEX); we return a pointer to an io_uring-backed shim,
  //  so the resolution + call site work unchanged. AcceptEx is called directly. Both
  //  route to p2piocp (weak hooks, resolved only in an image that links it).
  // -------------------------------------------------------------------------
  #ifndef WSA_FLAG_OVERLAPPED
    #define WSA_FLAG_OVERLAPPED 0x01
  #endif
  #ifndef SO_UPDATE_CONNECT_CONTEXT
    #define SO_UPDATE_CONNECT_CONTEXT 0x7010
  #endif
  #ifndef SIO_GET_EXTENSION_FUNCTION_POINTER
    #define SIO_GET_EXTENSION_FUNCTION_POINTER 0xC8000006u
  #endif
  using SOCKADDR_STORAGE = sockaddr_storage;

  extern "C" BOOL p2p_iocp_connect(int fd, const void* addr, int addrlen, void* ov) __attribute__((weak));
  extern "C" BOOL p2p_iocp_accept (int listen_fd, void* acceptsock, void* ov)       __attribute__((weak));

  //  The two extension GUIDs (Windows values), compared bytewise in WSAIoctl.
  inline const GUID& p2p_wsaid_connectex() {
      static const GUID g = {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}; return g; }
  inline const GUID& p2p_wsaid_acceptex() {
      static const GUID g = {0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}; return g; }
  #define WSAID_CONNECTEX (p2p_wsaid_connectex())
  #define WSAID_ACCEPTEX  (p2p_wsaid_acceptex())

  inline BOOL p2p_wsa_connectex(SOCKET s, const sockaddr* name, int namelen,
                                PVOID /*sendBuf*/, DWORD /*sendLen*/, LPDWORD sent, LPOVERLAPPED ov) {
      if (sent) *sent = 0;
      if (!s) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      if (&p2p_iocp_connect) return p2p_iocp_connect(s->fd, name, namelen, ov);
      SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
  }

  inline BOOL AcceptEx(SOCKET listenSock, SOCKET acceptSock, PVOID /*outBuf*/, DWORD /*recvLen*/,
                       DWORD /*localLen*/, DWORD /*remoteLen*/, LPDWORD received, LPOVERLAPPED ov) {
      if (received) *received = 0;
      if (!listenSock || !acceptSock) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      if (&p2p_iocp_accept) return p2p_iocp_accept(listenSock->fd, acceptSock, ov);
      SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
  }

  //  The legacy code extracts but does not use these addresses (the consumer is commented
  //  out), so return valid pointers to zeroed storage.
  inline void GetAcceptExSockaddrs(PVOID /*buf*/, DWORD /*recvLen*/, DWORD /*localLen*/,
                                   DWORD /*remoteLen*/, sockaddr** local, int* localLen2,
                                   sockaddr** remote, int* remoteLen2) {
      static thread_local sockaddr_storage ssLocal{}, ssRemote{};
      if (local)     *local     = reinterpret_cast<sockaddr*>(&ssLocal);
      if (localLen2) *localLen2 = (int)sizeof(sockaddr_in);
      if (remote)     *remote    = reinterpret_cast<sockaddr*>(&ssRemote);
      if (remoteLen2) *remoteLen2 = (int)sizeof(sockaddr_in);
  }

  //  WSAIoctl: only the extension-fn-pointer query is supported; it returns the io_uring
  //  ConnectEx/AcceptEx shim for the requested GUID.
  inline int WSAIoctl(SOCKET /*s*/, DWORD code, void* inbuf, DWORD /*cbin*/,
                      void* outbuf, DWORD /*cbout*/, LPDWORD outret,
                      void* /*ov*/ = nullptr, void* /*cr*/ = nullptr) {
      if (code == SIO_GET_EXTENSION_FUNCTION_POINTER && inbuf && outbuf) {
          if (std::memcmp(inbuf, &p2p_wsaid_connectex(), sizeof(GUID)) == 0) {
              *reinterpret_cast<void**>(outbuf) = reinterpret_cast<void*>(&p2p_wsa_connectex);
              if (outret) *outret = (DWORD)sizeof(void*); return 0;
          }
          if (std::memcmp(inbuf, &p2p_wsaid_acceptex(), sizeof(GUID)) == 0) {
              *reinterpret_cast<void**>(outbuf) = reinterpret_cast<void*>(&AcceptEx);
              if (outret) *outret = (DWORD)sizeof(void*); return 0;
          }
      }
      SetLastError(ERROR_INVALID_PARAMETER);
      return SOCKET_ERROR;
  }

  // -------------------------------------------------------------------------
  //  Named pipes -> AF_UNIX (LinuxPortPlan §6.2). \\.\pipe\Name maps to
  //  $XDG_RUNTIME_DIR/p2pmsg/Name.sock (fallback /tmp). The framing layer is already
  //  length-prefixed, so SOCK_STREAM suffices (message vs byte mode is a no-op). Server:
  //  CreateNamedPipe = socket/bind/listen; ConnectNamedPipe = async accept that MORPHS the
  //  handle into the accepted connection (the listen fd is replaced by the accepted fd —
  //  reusing p2p_iocp_accept with acceptsock == the pipe handle). Client CreateFile ->
  //  socket+connect (handled in p2pfile.h path translation is not enough; see note).
  // -------------------------------------------------------------------------
  #ifndef PIPE_ACCESS_DUPLEX
    #define PIPE_ACCESS_INBOUND       0x00000001
    #define PIPE_ACCESS_OUTBOUND      0x00000002
    #define PIPE_ACCESS_DUPLEX        0x00000003
    #define PIPE_TYPE_BYTE            0x00000000
    #define PIPE_TYPE_MESSAGE         0x00000004
    #define PIPE_READMODE_BYTE        0x00000000
    #define PIPE_READMODE_MESSAGE     0x00000002
    #define PIPE_WAIT                 0x00000000
    #define PIPE_NOWAIT               0x00000001
    #define PIPE_UNLIMITED_INSTANCES  255
  #endif

  //  \\.\pipe\Name -> socket path. Uses $XDG_RUNTIME_DIR else /tmp; dir is created best-effort.
  inline std::string p2p_pipe_path(const wchar_t* name) {
      std::string w;
      if (name) for (const wchar_t* p = name; *p; ++p) w.push_back((char)(unsigned char)*p);
      //  strip a leading \\.\pipe\ (or //./pipe/) prefix, keep the final component
      std::size_t pos = w.find_last_of("\\/");
      std::string leaf = (pos == std::string::npos) ? w : w.substr(pos + 1);
      const char* root = std::getenv("XDG_RUNTIME_DIR");
      std::string dir = (root && *root) ? std::string(root) + "/p2pmsg" : std::string("/tmp/p2pmsg");
      ::mkdir(dir.c_str(), 0700);
      return dir + "/" + leaf + ".sock";
  }

  //  Named-pipe CLIENT connect (forward-declared in p2pfile.h; defined here so it can reuse
  //  p2p_pipe_path + the socket APIs). Blocking AF_UNIX connect to the server's bound path —
  //  the pipe client's CreateFile is synchronous on Windows too (the connect completion is
  //  then simulated by the transport via PostOVERLAPPED). Returns a P2PHandle{Fd}.
  inline HANDLE p2p_pipe_client_connect(const wchar_t* name) {
      std::string path = p2p_pipe_path(name);
      int fd = (socket)(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
      if (fd < 0) { SetLastError(win32_from_errno(errno)); return INVALID_HANDLE_VALUE; }
      sockaddr_un sa{}; sa.sun_family = AF_UNIX;
      std::strncpy(sa.sun_path, path.c_str(), sizeof sa.sun_path - 1);
      if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), (socklen_t)sizeof sa) != 0) {
          //  Server not listening yet -> ENOENT/ECONNREFUSED. Map to the Win32 code the
          //  pipe client expects (ERROR_FILE_NOT_FOUND) so Connect()'s failure path matches.
          int e = errno; ::close(fd);
          SetLastError(e == ENOENT || e == ECONNREFUSED ? ERROR_FILE_NOT_FOUND
                                                        : win32_from_errno(e));
          return INVALID_HANDLE_VALUE;
      }
      return p2p_handle_new(HKind::Fd, fd, nullptr);
  }

  inline HANDLE CreateNamedPipeW(const wchar_t* name, DWORD /*openMode*/, DWORD /*pipeMode*/,
                                 DWORD /*maxInst*/, DWORD /*outBuf*/, DWORD /*inBuf*/,
                                 DWORD /*timeout*/, void* /*sec*/) {
      std::string path = p2p_pipe_path(name);
      int fd = (socket)(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
      if (fd < 0) { SetLastError(win32_from_errno(errno)); return INVALID_HANDLE_VALUE; }
      sockaddr_un sa{}; sa.sun_family = AF_UNIX;
      std::strncpy(sa.sun_path, path.c_str(), sizeof sa.sun_path - 1);
      ::unlink(path.c_str());                          // clear a stale socket file
      if (::bind(fd, reinterpret_cast<sockaddr*>(&sa), (socklen_t)sizeof sa) != 0 ||
          ::listen(fd, 8) != 0) {
          int e = errno; ::close(fd); SetLastError(win32_from_errno(e)); return INVALID_HANDLE_VALUE;
      }
      return p2p_handle_new(HKind::Fd, fd, nullptr);
  }
  #define CreateNamedPipe CreateNamedPipeW

  //  ConnectNamedPipe: overlapped -> async accept that morphs `h` into the connection;
  //  synchronous -> blocking accept. (The accepted fd replaces h->fd, inheriting the
  //  listen socket's completion key via p2p_iocp_accept.)
  inline BOOL ConnectNamedPipe(HANDLE h, LPOVERLAPPED ov) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      if (ov) {
          if (&p2p_iocp_accept) return p2p_iocp_accept(h->fd, h, ov);
          SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
      }
      int fd = ::accept(h->fd, nullptr, nullptr);
      if (fd < 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      ::close(h->fd); h->fd = fd; return TRUE;
  }
  inline BOOL DisconnectNamedPipe(HANDLE) { return TRUE; }
#endif
