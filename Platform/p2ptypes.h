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
//  Platform layer — Win32 scalar types + HANDLE/SOCKET model.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §4.1).
//
//  _WIN32 : pure pass-through. Includes the real Windows headers; defines nothing
//           that alters the ABI. The Windows binary stays bit-for-bit identical.
//  Linux  : provides the *subset* of Win32 scalar types the codebase actually uses
//           (DWORD/HANDLE/BOOL/UINT_PTR/HRESULT/...) plus the tagged-HANDLE model
//           that unifies files, sockets, events, threads and io_uring ports the way
//           Win32 muddles them into HANDLE. Anything outside the used subset is left
//           undefined on purpose, so an out-of-subset use is a compile error (§4 rule).
//
//  Phase status: Windows side complete (pass-through). Linux side is the Phase 0/1
//  scaffold — types are declared; the runtime behaviour behind P2PHandle (rings,
//  eventfds, cancellation) is implemented in Phase 2 (p2piocp.*).
//
#pragma once

#if defined(_WIN32)
// ---------------------------------------------------------------------------
//  Windows: pass-through to the genuine platform headers.
// ---------------------------------------------------------------------------
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <WinSock2.h>     // must precede <windows.h>
  #include <ws2tcpip.h>
  #include <mswsock.h>
  #include <windows.h>
  // All Win32 scalar types (DWORD, HANDLE, SOCKET, BOOL, UINT_PTR, HRESULT, ...)
  // and INVALID_HANDLE_VALUE / INVALID_SOCKET come from the headers above.

#else
// ---------------------------------------------------------------------------
//  Linux: the used subset of Win32 scalar types.
// ---------------------------------------------------------------------------
  #include <cstdint>
  #include <cstddef>
  #include <cwchar>
  #include <cstring>
  #include <cstdlib>
  #include <cerrno>
  #include <atomic>
  #include <mutex>          // p2p_handle_mx (handle free-list)
  #include <unistd.h>
  #include <pthread.h>
  #include <ctime>          // time (_time64 shim)
  #include <sys/random.h>   // getrandom (CoCreateGuid)
  #include <csignal>        // sigaction (p2p_ignore_sigpipe)

  //  This is the Unicode build (Windows sets CharacterSet=Unicode => _UNICODE/UNICODE).
  //  Many legacy headers gate their wide (LPCWSTR) API overloads behind _UNICODE
  //  (e.g. Msgexception's Module/Message/Advice); without it, wide string literals
  //  fall onto the narrow LPCSTR overload and fail to convert. Define it up front so
  //  every TU that pulls the platform layer sees the wide surface. (The CMake target
  //  also passes -D_UNICODE -DUNICODE belt-and-suspenders.)
  #ifndef _UNICODE
    #define _UNICODE
  #endif
  #ifndef UNICODE
    #define UNICODE
  #endif

  // -------------------------------------------------------------------------
  //  MSVC compiler-keyword shims (gcc/ELF). __declspec(dllexport) -> visibility;
  //  __declspec(dllimport) and the calling-convention keywords -> nothing.
  //  Uses token-paste so __declspec(x) dispatches on x.
  // -------------------------------------------------------------------------
  #define __declspec(x)      P2P_DS_##x
  #define P2P_DS_dllexport   __attribute__((visibility("default")))
  #define P2P_DS_dllimport
  #define P2P_DS_novtable
  #define P2P_DS_selectany   __attribute__((weak))
  #define P2P_DS_noreturn    __attribute__((noreturn))
  #define P2P_DS_noinline    __attribute__((noinline))
  #define P2P_DS_deprecated  __attribute__((deprecated))
  #define P2P_DS_nothrow
  #define P2P_DS_thread      thread_local
  #ifndef __cdecl
    #define __cdecl
  #endif
  #define __stdcall
  #define __fastcall
  #define __thiscall
  #define WINAPI
  #define APIENTRY
  #define CALLBACK
  #define __forceinline      inline

  using BYTE      = std::uint8_t;
  using WORD      = std::uint16_t;
  using DWORD     = std::uint32_t;
  using DWORD32   = std::uint32_t;
  //  64-bit types are `long long`, NOT std::uint64_t. On LP64 Linux std::uint64_t is
  //  `unsigned long`, which would make UINT64 identical to ULONG/DWORD_PTR and collide
  //  with distinct overloads the legacy headers declare (e.g. P3PmsgData(unsigned long)
  //  vs P3PmsgData(UINT64)). Windows (LLP64) has UINT64 == unsigned long long, distinct
  //  from ULONG; matching that type identity keeps the overload sets valid on both.
  using DWORD64   = unsigned long long;
  using QWORD     = unsigned long long;
  using UINT      = unsigned int;
  using INT       = int;
  using UINT08    = std::uint8_t;
  using UINT16    = std::uint16_t;
  using UINT32    = std::uint32_t;
  using UINT64    = unsigned long long;   // see DWORD64 note: match Windows type identity
  using INT08     = std::int8_t;
  using INT16     = std::int16_t;
  using INT32     = std::int32_t;
  using INT64     = long long;            // see DWORD64 note: match Windows type identity
  using UCHAR     = unsigned char;
  using USHORT    = unsigned short;
  using ULONG     = unsigned long;
  using LONG      = long;
  using LONGLONG  = long long;
  using ULONGLONG = unsigned long long;
  using BOOL      = int;
  using BOOLEAN   = unsigned char;
  using UINT_PTR  = std::uintptr_t;
  using INT_PTR   = std::intptr_t;
  using DWORD_PTR = std::uintptr_t;
  using ULONG_PTR = std::uintptr_t;
  using LONG_PTR  = std::intptr_t;
  using SIZE_T    = std::size_t;
  using SSIZE_T   = std::ptrdiff_t;
  using HRESULT   = long;

  //  MSVC built-in fixed-width type specifiers used in interface/serialized decls.
  //  Only the bare `__int64` form appears in the compiled set (no `unsigned __int64`),
  //  so a plain typedef is sufficient (a typedef could not carry an `unsigned` prefix).
  using __int64   = long long;
  using __int32   = int;
  using __int16   = short;
  using __int8    = signed char;

  //  MSVC 64-bit time types + the _time64 CRT function (P2Peer.h: `typedef __time64_t
  //  P2Pmsecs_t`, the pump's millisecond clock — the single most cascaded type in
  //  Targetcore). __time64_t is a 64-bit signed seconds-since-epoch, same shape as the
  //  Linux 64-bit time_t; _time64(&t) mirrors CRT semantics (returns + optionally stores).
  using __time64_t = long long;
  using __time32_t = int;
  inline __time64_t _time64(__time64_t* t) {
      __time64_t r = (__time64_t)::time(nullptr);
      if (t) *t = r;
      return r;
  }

  #ifndef TRUE
    #define TRUE  1
  #endif
  #ifndef FALSE
    #define FALSE 0
  #endif

  //  S_OK / FAILED / SUCCEEDED — the small HRESULT surface the code tests.
  #ifndef S_OK
    #define S_OK    ((HRESULT)0L)
    #define S_FALSE ((HRESULT)1L)
    #define E_FAIL  ((HRESULT)0x80004005L)
  #endif
  #define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
  #define FAILED(hr)    (((HRESULT)(hr)) <  0)

  //  Small SDK convenience macros the codebase names.
  #ifndef ARRAYSIZE
    #define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
  #endif
  #ifndef _countof
    #define _countof(a)  ARRAYSIZE(a)
  #endif
  #ifndef _MAX_PATH
    #define _MAX_PATH 260          // MSVC CRT spelling; same value as MAX_PATH
  #endif
  #ifndef MAX_PATH
    #define MAX_PATH 260
  #endif

  // -------------------------------------------------------------------------
  //  The tagged-HANDLE model (the Linux port plan §4.1).
  //
  //  Win32 casts SOCKET -> HANDLE freely (e.g. P2Peerio::Send(HANDLE) on a
  //  socket). We make HANDLE and SOCKET the SAME type — a pointer to a small
  //  tagged object — so every existing (HANDLE)m_oSocket cast compiles unchanged.
  //  The behaviour behind `impl` (ring, eventfd, jthread) lands in Phase 2.
  // -------------------------------------------------------------------------
  enum class HKind : std::uint8_t { Fd, Iocp, Event, Thread };

  struct P2PHandle {
      HKind            kind = HKind::Fd;
      //  Atomic because a Win32 SOCKET stays usable from other threads right up to the
      //  moment it is closed, and those threads read this field. closesocket() on one
      //  thread while another sits in recv() is a legitimate (if blunt) Win32 idiom, not
      //  a caller error, so that read must not be a data race: a reader sees either the
      //  live descriptor or -1, never a torn value and never freed memory.
      std::atomic<int> fd{-1};         // Fd: file / socket / pipe / tty
      void            *impl = nullptr; // Iocp: IoRing*; Event: EventImpl*; Thread: jthread*
      //  Set while the handle sits on the free-list. Win32 lets a caller close a handle
      //  twice -- the second call fails with ERROR_INVALID_HANDLE rather than corrupting
      //  the table -- and the free-list needs the same guarantee: retiring one handle
      //  twice would link it to itself and hand the same memory to two owners.
      std::atomic<bool> retired{false};
      P2PHandle       *next_free = nullptr;   // retired free-list link, see p2p_handle_retire
  };

  // -------------------------------------------------------------------------
  //  Handle lifetime (§4.1). Win32 hands out handle VALUES from a per-process table;
  //  closing one leaves the value valid to PASS, and a later call on it fails with
  //  ERROR_INVALID_HANDLE / WSAENOTSOCK. Nothing is freed under the caller's feet.
  //
  //  This shim hands out POINTERS, so `delete` on close turned that contract into a
  //  use-after-free -- and not a theoretical one. Under TSan, p2p_authrelay showed
  //  closesocket()'s `delete s` on the main thread racing a relay thread's read of
  //  `s->fd` four bytes into the same allocation: the exact Win32 idiom of closing a
  //  socket to make another thread's blocked call fail, which is memory-safe there and
  //  corrupting here.
  //
  //  So handles are retired to a free-list and reused, never returned to the allocator.
  //  A stale HANDLE therefore always points at readable memory and reads as closed
  //  (fd == -1). Reuse can hand a retired handle to a NEW object, at which point a stale
  //  pointer addresses that new object -- which is precisely what Windows does when it
  //  recycles a handle value, and is the same caller bug on both platforms rather than
  //  a new one here.
  //
  //  The visibility attributes are the same load-bearing kind as p2p_tid_counter()'s
  //  below: without them every module gets a private free-list, and a handle allocated
  //  in libmsgcore but retired in libtargetcore would never be reused.
  // -------------------------------------------------------------------------
  __attribute__((visibility("default")))
  inline std::mutex& p2p_handle_mx() { static std::mutex m; return m; }
  __attribute__((visibility("default")))
  inline P2PHandle*& p2p_handle_freelist() { static P2PHandle* head = nullptr; return head; }

  __attribute__((visibility("default")))
  inline P2PHandle* p2p_handle_new(HKind kind, int fd, void* impl) {
      P2PHandle* h = nullptr;
      { std::lock_guard<std::mutex> lk(p2p_handle_mx());
        if ((h = p2p_handle_freelist()) != nullptr) {
            p2p_handle_freelist() = h->next_free;
            h->next_free = nullptr;
        } }
      if (!h) h = new P2PHandle();
      h->kind = kind;
      h->impl = impl;
      h->retired.store(false, std::memory_order_relaxed);
      h->fd.store(fd, std::memory_order_release);   // last: publishes the handle as live
      return h;
  }

  //  Retire a handle: mark it closed, then return it to the free-list. The store happens
  //  BEFORE the handle becomes reusable, so a concurrent reader either still sees the old
  //  descriptor (it raced the close and would have anyway) or sees -1 and fails cleanly.
  __attribute__((visibility("default")))
  inline void p2p_handle_retire(P2PHandle* h) {
      if (!h) return;
      bool expect = false;
      if (!h->retired.compare_exchange_strong(expect, true, std::memory_order_acq_rel))
          return;                 // already retired -- a double close, not a second entry
      h->fd.store(-1, std::memory_order_release);
      h->impl = nullptr;
      std::lock_guard<std::mutex> lk(p2p_handle_mx());
      h->next_free = p2p_handle_freelist();
      p2p_handle_freelist() = h;
  }

  using HANDLE = P2PHandle*;
  using SOCKET = P2PHandle*;   // same type as HANDLE, by design
  using PHANDLE = HANDLE*;
  using LPVOID  = void*;
  using LPCVOID = const void*;
  typedef void  VOID;          // Win32 spells the return keyword this way (e.g. WINAPI VOID f())
  //  Fundamental string-pointer typedefs (also aliased in p2pstr.h — identical, legal).
  //  Defined here too so early users in this header (e.g. the MessageBox stubs) see them.
  using LPSTR   = char*;
  using LPCSTR  = const char*;
  using LPWSTR  = wchar_t*;
  using LPCWSTR = const wchar_t*;

  inline constexpr HANDLE INVALID_HANDLE_VALUE = nullptr;
  inline constexpr SOCKET INVALID_SOCKET       = nullptr;
  #ifndef NULL
    #define NULL 0
  #endif

  // -------------------------------------------------------------------------
  //  GUI/window handle types. The WM_APP_* window-message integration is
  //  compiled out on Linux (§6.2), but the *types* are still named in signatures
  //  and struct fields, so provide opaque typedefs. POSITION is MFC's collection
  //  cursor (an opaque pointer).
  // -------------------------------------------------------------------------
  using HWND      = struct HWND__*;
  using HINSTANCE = struct HINSTANCE__*;
  using HMODULE   = HINSTANCE;
  using HKEY      = struct HKEY__*;
  using HGLOBAL   = void*;
  using HLOCAL    = void*;
  using HMENU     = struct HMENU__*;
  using HICON     = struct HICON__*;
  using HDC       = struct HDC__*;
  using WPARAM    = UINT_PTR;
  using LPARAM    = LONG_PTR;
  using LRESULT   = LONG_PTR;
  using ATOM      = WORD;
  using POSITION  = void*;
  using LPWORD    = WORD*;
  using LPDWORD   = DWORD*;
  using LPBYTE    = BYTE*;
  using LPLONG    = LONG*;
  using PVOID     = void*;

  //  Pointer-to-scalar family (P* names). PINT_PTR undefined was the single root that
  //  broke P2PmsgMgr.h's callback typedefs and cascaded across every TU including it.
  using SHORT     = short;
  using CHAR      = char;
  using PINT_PTR  = INT_PTR*;
  using PUINT_PTR = UINT_PTR*;
  using PLONG_PTR = LONG_PTR*;
  using PULONG_PTR= ULONG_PTR*;
  using PDWORD_PTR= DWORD_PTR*;
  using PINT      = int*;
  using PUINT     = unsigned int*;
  using PLONG     = LONG*;
  using PULONG    = ULONG*;
  using PSHORT    = short*;
  using PUSHORT   = unsigned short*;
  using PCHAR     = char*;
  using PUCHAR    = UCHAR*;
  using PWCHAR    = wchar_t*;
  using PBYTE     = BYTE*;
  using PBOOL     = BOOL*;
  using PFLOAT    = float*;
  using PDWORD    = DWORD*;
  using PWORD     = WORD*;

  //  Window-message constants. The WM_APP_* integration is compiled out on Linux
  //  (§6.2), but the base constants are still named in switch/case labels.
  #ifndef WM_USER
    #define WM_USER 0x0400
    #define WM_APP  0x8000
    #define WM_NULL 0x0000
  #endif
  //  Window-message posting is compiled out on Linux (§6.2) — the CWnd hook is
  //  optional. Stub PostMessage/SendMessage so the few residual call sites link.
  inline BOOL PostMessage(HWND, UINT, WPARAM, LPARAM) { return TRUE; }
  inline LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }

  //  Message-box UI is compiled out on Linux; stub so the error-dialog fallback links.
  #ifndef MB_OK_UIDEFS
    #define MB_OK_UIDEFS
    #define MB_TASKMODAL   0x00002000
    #define MB_SYSTEMMODAL 0x00001000
    #define MB_YESNO       0x00000004
    #define IDOK    1
    #define IDYES   6
    #define IDNO    7
  #endif
  inline int  MessageBoxW (HWND, LPCWSTR, LPCWSTR, UINT) { return IDOK; }
  inline int  MessageBoxExW(HWND, LPCWSTR, LPCWSTR, UINT, WORD) { return IDOK; }
  #define MessageBox   MessageBoxW
  #define MessageBoxEx MessageBoxExW
  #ifndef MB_USERICON
    #define MB_USERICON 0x00000080
  #endif
  inline BOOL IsWindow(HWND) { return FALSE; }
  //  Callers use a non-null GetConsoleWindow() to mean "there is a console, so
  //  write the diagnostic to stderr instead of popping a modal dialog" — see
  //  Msgexception.cpp's Display(). On Linux stderr is ALWAYS the right answer:
  //  MessageBoxEx above is a stub that returns IDOK without showing anything,
  //  so reporting "no console" would silently discard every error message.
  //  Returns a non-null sentinel rather than a real handle; the value is only
  //  ever tested for truth.
  inline HWND GetConsoleWindow() { return (HWND)(~(uintptr_t)0); }
  inline HWND GetFocus() { return nullptr; }
  inline HWND SetFocus(HWND) { return nullptr; }
  inline BOOL PostThreadMessage(DWORD, UINT, WPARAM, LPARAM) { return TRUE; }
  inline HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
  #define GetModuleHandle GetModuleHandleW

  //  MSVC debugger-break intrinsic. On Linux, trap (matches Win32's no-debugger
  //  behaviour of terminating at the break site).
  #define __debugbreak() __builtin_trap()

  //  FormatMessage: the compiled set uses it to fetch a system/module error string.
  //  Stub writes a compact "system error <id>" into the caller's fixed buffer (the
  //  ALLOCATE_BUFFER path is commented out at every call site).
  #ifndef FORMAT_MESSAGE_FROM_SYSTEM
    #define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
    #define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
    #define FORMAT_MESSAGE_FROM_STRING     0x00000400
    #define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
    #define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
    #define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
    #define LANG_NEUTRAL     0x00
    #define SUBLANG_DEFAULT  0x01
    #define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
  #endif
  inline DWORD FormatMessageW(DWORD /*flags*/, LPCVOID /*src*/, DWORD msgId, DWORD /*lang*/,
                              LPWSTR buf, DWORD size, void* /*args*/) {
      if (!buf || !size) return 0;
      int n = std::swprintf(buf, size, L"system error %lu", (unsigned long)msgId);
      return n < 0 ? 0 : (DWORD)n;
  }
  #define FormatMessage FormatMessageW

  //  Legacy `byte` (rpcndr.h) — distinct from C++17 std::byte; the compiled set uses
  //  the global name. `_doserrno` is the MSVC CRT errno mirror.
  using byte = unsigned char;
  #ifndef _doserrno
    #define _doserrno errno
  #endif

  // -------------------------------------------------------------------------
  //  Win32 last-error mirror + the error codes the compiled set tests. Backed by
  //  a thread-local DWORD; the shims set it from errno via win32_from_errno().
  // -------------------------------------------------------------------------
  #ifndef ERROR_SUCCESS
    #define ERROR_SUCCESS            0
    #define NO_ERROR                 0
    #define ERROR_CALL_NOT_IMPLEMENTED 120
    #define ERROR_FILE_NOT_FOUND     2
    #define ERROR_PATH_NOT_FOUND     3
    #define ERROR_ACCESS_DENIED      5
    #define ERROR_INVALID_HANDLE     6
    #define ERROR_NOT_ENOUGH_MEMORY  8
    #define ERROR_INVALID_PARAMETER  87
    #define ERROR_SHARING_VIOLATION  32
    #define ERROR_HANDLE_EOF         38
    #define ERROR_NETNAME_DELETED    64
    #define ERROR_FILE_EXISTS        80
    #define ERROR_BROKEN_PIPE        109
    #define ERROR_ALREADY_EXISTS     183
    #define ERROR_NO_DATA            232
    #define ERROR_MORE_DATA          234
    #define ERROR_PIPE_CONNECTED     535
    #define ERROR_PIPE_LISTENING     536
    #define ERROR_OPERATION_ABORTED  995
    #define ERROR_IO_PENDING         997
    #define ERROR_NO_SYSTEM_RESOURCES 1450
    #define WAIT_TIMEOUT             258
  #endif

  inline DWORD& p2p_last_error() { static thread_local DWORD e = 0; return e; }
  inline DWORD  GetLastError()        { return p2p_last_error(); }
  inline void   SetLastError(DWORD e) { p2p_last_error() = e; }
  inline DWORD  win32_from_errno(int e) {
      switch (e) {
          case 0:              return ERROR_SUCCESS;
          case ENOENT:         return ERROR_FILE_NOT_FOUND;
          case EACCES:
          case EPERM:          return ERROR_ACCESS_DENIED;
          case EEXIST:         return ERROR_ALREADY_EXISTS;
          case EBADF:          return ERROR_INVALID_HANDLE;
          case ENOMEM:         return ERROR_NOT_ENOUGH_MEMORY;
          case EINVAL:         return ERROR_INVALID_PARAMETER;
          case EAGAIN:         return ERROR_SHARING_VIOLATION;   // flock LOCK_NB contention
          default:             return (DWORD)e;
      }
  }


  // -------------------------------------------------------------------------
  //  SIGPIPE (§6.1). Win32 has no such signal: writing to a socket or pipe whose
  //  peer has gone returns an error (WSAECONNRESET / ERROR_BROKEN_PIPE) and the
  //  process lives. On Linux the default disposition TERMINATES the process, so
  //  a single peer that resets at the wrong moment would kill a daemon that is
  //  behaving correctly -- and the io_uring write path goes through plain
  //  write(2) semantics (io_uring_prep_write), which is exactly the case that
  //  raises it. Ignoring the signal turns it back into the EPIPE the shims
  //  already translate.
  //
  //  Only a DEFAULT disposition is replaced: a host application that installed
  //  its own SIGPIPE handler keeps it. Idempotent, callable from any thread; the
  //  shim entry points that begin socket/ring work (WSAStartup,
  //  CreateIoCompletionPort) call it rather than a library constructor, so a TU
  //  that merely includes a header never changes process-wide signal state.
  // -------------------------------------------------------------------------
  inline void p2p_ignore_sigpipe() {
      static std::atomic<bool> done{false};
      bool expect = false;
      if (!done.compare_exchange_strong(expect, true)) return;
      struct sigaction sa{};
      if (::sigaction(SIGPIPE, nullptr, &sa) == 0 && sa.sa_handler == SIG_DFL) {
          struct sigaction ign{};
          ign.sa_handler = SIG_IGN;
          ::sigemptyset(&ign.sa_mask);
          ::sigaction(SIGPIPE, &ign, nullptr);
      }
  }
  //  Thread-id / sleep. GetCurrentThreadId must return the SAME value CreateThread
  //  (p2pthread.h) writes to *lpThreadId for that thread: the Targetcore pump keys its
  //  per-thread pump/hub registries on it (s_ThreadID_P2PmsgPump.Lookup(GetCurrentThreadId()))
  //  AND P2PeerHub::RunHub asserts m_nHubID==GetCurrentThreadId(). pthread_self() truncated
  //  to DWORD is per-thread-unique but does NOT equal the synthetic id CreateThread hands
  //  out, so we mint monotonically-increasing ids from one shared counter and cache each
  //  thread's id in a thread_local. CreateThread pre-assigns from p2p_next_tid() and seeds
  //  the child's t_p2p_tid before running the proc; the main thread (and any thread not
  //  created via CreateThread) lazily self-assigns on first call.
  //  The visibility attributes are load-bearing, not decoration. These are inline
  //  definitions in a header, and the libraries are built -fvisibility=hidden, so
  //  without them each module gets its OWN private copy: nm showed `b t_p2p_tid`
  //  (local) in libtargetcore.so, another in libmsgcore.so, and a third in every
  //  test executable, with `p2p_tid_counter()::c` duplicated alongside. That is
  //  fatal in two compounding ways. The caches diverge, so GetCurrentThreadId()
  //  answers differently on ONE thread depending on which module inlined the
  //  call - breaking P2PeerHub::RunHub's `m_nHubID==GetCurrentThreadId()` assert,
  //  where the id is written by CreateThread in one module and compared in
  //  another. And each counter restarts at 1, so the id SPACES collide: two
  //  threads can hold the same id, and the pump's per-thread registries
  //  (s_ThreadID_P2PmsgPump keyed on this value) then resolve to the wrong hub or
  //  to none at all.
  //
  //  Marking them default-visible makes the dynamic linker collapse the COMDAT
  //  copies onto one object, which is the invariant the whole scheme assumes.
  //  This class of bug cannot occur on Windows, where GetCurrentThreadId() is a
  //  single OS call with one true answer - which is exactly why it survived.
  __attribute__((visibility("default")))
  inline std::atomic<DWORD>& p2p_tid_counter() { static std::atomic<DWORD> c{1}; return c; }
  __attribute__((visibility("default")))
  inline DWORD p2p_next_tid() { return p2p_tid_counter().fetch_add(1, std::memory_order_relaxed); }
  __attribute__((visibility("default")))
  inline thread_local DWORD t_p2p_tid = 0;
  __attribute__((visibility("default")))
  inline DWORD GetCurrentThreadId() {
      if (t_p2p_tid == 0) t_p2p_tid = p2p_next_tid();
      return t_p2p_tid;
  }
  inline DWORD GetCurrentProcessId() { return (DWORD)getpid(); }
  inline void  Sleep(DWORD ms) { ::usleep((useconds_t)ms * 1000); }

  //  GetModuleFileName: the compiled set uses it only to fetch the running executable's
  //  path (P2Pwin32.cpp exe-path probe). /proc/self/exe is the Linux equivalent. (Placed
  //  after SetLastError/win32_from_errno, which it uses.)
  inline DWORD GetModuleFileNameW(HMODULE, LPWSTR buf, DWORD n) {
      if (!buf || n == 0) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
      char tmp[4096]; ssize_t r = ::readlink("/proc/self/exe", tmp, sizeof tmp - 1);
      if (r < 0) { SetLastError(win32_from_errno(errno)); buf[0] = 0; return 0; }
      tmp[r] = 0;
      DWORD i = 0; for (; tmp[i] && i < n - 1; ++i) buf[i] = (wchar_t)(unsigned char)tmp[i];
      buf[i] = 0;
      return i;
  }
  #define GetModuleFileName GetModuleFileNameW

  //  HGLOBAL memory (P2PsafeHGLOBAL is barely used, §6.1). HGLOBAL == void*, so
  //  lock/unlock are identity.
  #ifndef GMEM_MOVEABLE
    #define GMEM_MOVEABLE 0x0002
    #define GMEM_ZEROINIT 0x0040
    #define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
    #define GPTR 0x0040
  #endif
  inline HGLOBAL GlobalAlloc(UINT flags, SIZE_T n) {
      void* p = std::malloc(n); if (p && (flags & GMEM_ZEROINIT)) std::memset(p, 0, n); return p; }
  inline HGLOBAL GlobalFree(HGLOBAL p) { std::free(p); return nullptr; }
  inline LPVOID  GlobalLock(HGLOBAL p) { return p; }
  inline BOOL    GlobalUnlock(HGLOBAL) { return TRUE; }

  // -------------------------------------------------------------------------
  //  Small Win32 helper macros/functions the compiled set names.
  // -------------------------------------------------------------------------
  #ifndef ZeroMemory
    #define ZeroMemory(p, n)   std::memset((p), 0, (n))
    #define CopyMemory(d, s, n) std::memcpy((d), (s), (n))
    #define MoveMemory(d, s, n) std::memmove((d), (s), (n))
    #define FillMemory(p, n, v) std::memset((p), (v), (n))
  #endif

  //  SecureZeroMemory is NOT ZeroMemory with a longer name, and must never be
  //  shimmed to memset here. Its whole purpose is a wipe the optimiser is not
  //  allowed to elide - and eliding a memset of storage that is never read
  //  again is precisely what a compiler is entitled to do. Mapping it to memset
  //  would build clean and silently drop key-material wipes on Linux only,
  //  which is the worst shape a shim bug can take: invisible, and only on the
  //  platform nobody is looking at.
  #ifndef SecureZeroMemory
    inline void *SecureZeroMemory(void *p, SIZE_T n) {
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
        ::explicit_bzero(p, n);        // documented not to be optimised away
    #else
        volatile unsigned char *v = (volatile unsigned char *)p;
        while (n--) *v++ = 0;          // the portable form P2PeerSeal.cpp uses
    #endif
        return p;
    }
  #endif
  #ifndef UNREFERENCED_PARAMETER
    #define UNREFERENCED_PARAMETER(x) ((void)(x))
  #endif

  //  LARGE_INTEGER: the union, not just the 64-bit member. Callers use both
  //  QuadPart and the Low/High pair, so dropping the halves would compile for
  //  some callers and not others.
  #ifndef P2P_HAVE_LARGE_INTEGER
  #define P2P_HAVE_LARGE_INTEGER
    union LARGE_INTEGER {
        struct { DWORD LowPart; LONG HighPart; };
        std::int64_t QuadPart;
    };
    union ULARGE_INTEGER {
        struct { DWORD LowPart; DWORD HighPart; };
        std::uint64_t QuadPart;
    };
  #endif

  //  Interlocked*: the Win32 names over the C++ atomic builtins. Sequentially
  //  consistent to match the Win32 contract, which is a full barrier - a
  //  relaxed ordering here would be faster and wrong.
  #ifndef InterlockedIncrement
    inline LONG InterlockedIncrement(LONG volatile *p)
    { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
    inline LONG InterlockedDecrement(LONG volatile *p)
    { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
    inline LONG InterlockedExchange(LONG volatile *p, LONG v)
    { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
    inline LONG InterlockedExchangeAdd(LONG volatile *p, LONG v)
    { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
  #endif

  //  _setmode: the CRT call that switches a stdio stream between text and the
  //  wide/binary modes. Those modes exist because Windows translates line
  //  endings and UTF-16 on the way out; POSIX does neither, so the honest shim
  //  is a no-op that reports success rather than a failure the caller would
  //  have to special-case.
  #ifndef _O_U16TEXT
    #define _O_TEXT    0x4000
    #define _O_U16TEXT 0x20000
    #define _O_U8TEXT  0x40000
    #define _O_WTEXT   0x10000
  #endif
  #ifndef _setmode
    inline int _setmode(int /*fd*/, int mode) { return mode; }
  #endif
  #ifndef MB_OK
    #define MB_OK              0x00000000
    #define MB_ICONERROR       0x00000010
    #define MB_ICONWARNING     0x00000030
    #define MB_ICONINFORMATION 0x00000040
  #endif
  //  IsBadWritePtr/IsBadReadPtr are unreliable even on Windows; on Linux there is
  //  no equivalent — report "not bad" so the (diagnostic-only) callers proceed.
  inline BOOL IsBadWritePtr(const void*, UINT_PTR) { return FALSE; }
  inline BOOL IsBadReadPtr (const void*, UINT_PTR) { return FALSE; }
  //  MSVC CRT _msize has no portable equivalent; the lone caller uses it for a
  //  diagnostic size check — return 0 (unknown) rather than lie about capacity.
  inline SIZE_T _msize(void*) { return 0; }

  // -------------------------------------------------------------------------
  //  Minimal COM/OLE types that appear in interface and serialized layouts
  //  (wtypes.h / guiddef.h surface). The full COM object model is out of scope;
  //  only the value types the code names are provided.
  // -------------------------------------------------------------------------
  struct GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; };
  using IID       = GUID;
  using CLSID     = GUID;
  using REFGUID   = const GUID&;
  using REFIID    = const GUID&;
  using REFCLSID  = const GUID&;
  using OLECHAR   = wchar_t;
  using BSTR      = OLECHAR*;
  using LPOLESTR  = OLECHAR*;
  using LPCOLESTR = const OLECHAR*;
  using DATE      = double;              // OLE Automation date (VT_DATE payload)
  using FLOAT     = float;
  using DOUBLE    = double;
  using VARIANT_BOOL = short;

  //  COM GUID generation (used for temp/store identifiers). Produces a random
  //  RFC-4122 v4 UUID via getrandom() — sufficient for the non-cryptographic
  //  uniqueness the callers need.
  inline HRESULT CoCreateGuid(GUID* p) {
      if (!p) return E_FAIL;
      if (::getrandom(p, sizeof(GUID), 0) != (ssize_t)sizeof(GUID)) return E_FAIL;
      p->Data3    = (WORD)((p->Data3 & 0x0FFF) | 0x4000);   // version 4
      p->Data4[0] = (BYTE)((p->Data4[0] & 0x3F) | 0x80);    // variant 1
      return S_OK;
  }

  #ifndef VARIANT_TRUE
    #define VARIANT_TRUE  ((VARIANT_BOOL)-1)
    #define VARIANT_FALSE ((VARIANT_BOOL)0)
  #endif

  // -------------------------------------------------------------------------
  //  VARIANT / _variant_t / _bstr_t subset (oaidl.h + comutil.h surface).
  //
  //  Live use is confined to P2Pmsg.cpp's data-cell <-> variant conversion
  //  (P2PmsgData_var). The subset provides exactly: construction from each
  //  scalar the VBLockData union holds, `.vt`, conversion operators back to
  //  those exact stdint widths, VT_* tags, and _bstr_t{GetBSTR,length}.
  //  Nothing is layout-accurate to real OLE Automation — no live path
  //  serializes a VARIANT; this is an in-memory bridge only.
  // -------------------------------------------------------------------------
  using VARTYPE = unsigned short;
  enum VARENUM {
      VT_EMPTY = 0, VT_NULL = 1, VT_I2 = 2, VT_I4 = 3, VT_R4 = 4, VT_R8 = 5,
      VT_BSTR = 8, VT_BOOL = 11, VT_I1 = 16, VT_UI1 = 17, VT_UI2 = 18,
      VT_UI4 = 19, VT_I8 = 20, VT_UI8 = 21, VT_INT = 22, VT_UINT = 23,
      VT_CLSID = 72
  };

  class _variant_t {
      static BSTR dupBstr(const wchar_t* p) {
          if (!p) return nullptr;
          size_t n = std::wcslen(p);
          auto* b = new wchar_t[n + 1];
          std::wmemcpy(b, p, n + 1);
          return b;
      }
      static BSTR dupBstr(const char* p) {
          if (!p) return nullptr;
          size_t n = std::strlen(p);
          auto* b = new wchar_t[n + 1];
          for (size_t i = 0; i <= n; ++i) b[i] = (wchar_t)(unsigned char)p[i];
          return b;
      }
      void clear() { if (vt == VT_BSTR && bstrVal) { delete[] bstrVal; bstrVal = nullptr; } }

  public:
      VARTYPE vt = VT_EMPTY;
      union {
          std::int8_t   cVal;    std::uint8_t  bVal;
          std::int16_t  iVal;    std::uint16_t uiVal;
          std::int32_t  intVal;  std::uint32_t uintVal;
          long long     llVal;   unsigned long long ullVal;   // match INT64/UINT64 widths
          float         fltVal;  double        dblVal;
          short         boolVal;
          BSTR          bstrVal;
      };
      GUID uuidVal{};   // VT_CLSID payload (kept out of the union: non-trivial-free path)

      _variant_t()                 : vt(VT_EMPTY), llVal(0) {}
      _variant_t(std::int8_t   v)  : vt(VT_I1),  cVal(v)   {}
      _variant_t(std::uint8_t  v)  : vt(VT_UI1), bVal(v)   {}
      _variant_t(std::int16_t  v)  : vt(VT_I2),  iVal(v)   {}
      _variant_t(std::uint16_t v)  : vt(VT_UI2), uiVal(v)  {}
      _variant_t(std::int32_t  v)  : vt(VT_I4),  intVal(v) {}
      _variant_t(std::uint32_t v)  : vt(VT_UI4), uintVal(v){}
      _variant_t(long long     v)  : vt(VT_I8),  llVal(v)  {}
      _variant_t(unsigned long long v) : vt(VT_UI8), ullVal(v) {}
      _variant_t(float  v)         : vt(VT_R4),  fltVal(v) {}
      _variant_t(double v)         : vt(VT_R8),  dblVal(v) {}
      _variant_t(bool   v)         : vt(VT_BOOL),boolVal(v ? VARIANT_TRUE : VARIANT_FALSE) {}
      _variant_t(wchar_t v)        : vt(VT_UI2), uiVal((std::uint16_t)v) {}
      _variant_t(const wchar_t* p) : vt(VT_BSTR),bstrVal(dupBstr(p)) {}
      _variant_t(wchar_t* p)       : vt(VT_BSTR),bstrVal(dupBstr((const wchar_t*)p)) {}
      _variant_t(const char* p)    : vt(VT_BSTR),bstrVal(dupBstr(p)) {}
      _variant_t(char* p)          : vt(VT_BSTR),bstrVal(dupBstr((const char*)p)) {}

      _variant_t(const _variant_t& o) { copyFrom(o); }
      _variant_t& operator=(const _variant_t& o) { if (this != &o) { clear(); copyFrom(o); } return *this; }
      ~_variant_t() { clear(); }

      //  Conversion back to the exact union widths (P2PmsgData_var reverse path).
      //  Plain `char` is a distinct 3rd type from signed/unsigned char; provide it
      //  explicitly so `char x = var;` is not ambiguous between the two.
      operator char()          const { return (char)cVal; }
      operator std::int8_t()   const { return cVal;    }
      operator std::uint8_t()  const { return bVal;    }
      operator std::int16_t()  const { return iVal;    }
      operator std::uint16_t() const { return uiVal;   }
      operator std::int32_t()  const { return intVal;  }
      operator std::uint32_t() const { return uintVal; }
      operator long long()          const { return llVal;   }
      operator unsigned long long() const { return ullVal;  }
      operator float()         const { return fltVal;  }
      operator double()        const { return dblVal;  }
      operator bool()          const { return boolVal != VARIANT_FALSE; }

  private:
      void copyFrom(const _variant_t& o) {
          vt = o.vt; uuidVal = o.uuidVal;
          if (vt == VT_BSTR) bstrVal = dupBstr(o.bstrVal);
          else               llVal   = o.llVal;
      }
  };
  using VARIANT   = _variant_t;   // the lone bare `VARIANT` decl in the compiled set
  using VARIANTARG = _variant_t;

  class _bstr_t {
      BSTR  m_p = nullptr;
      size_t m_n = 0;
  public:
      _bstr_t() = default;
      _bstr_t(const _variant_t& v) {
          if (v.vt == VT_BSTR && v.bstrVal) {
              m_n = std::wcslen(v.bstrVal);
              m_p = new wchar_t[m_n + 1];
              std::wmemcpy(m_p, v.bstrVal, m_n + 1);
          }
      }
      _bstr_t(const _bstr_t& o) : m_n(o.m_n) {
          if (o.m_p) { m_p = new wchar_t[m_n + 1]; std::wmemcpy(m_p, o.m_p, m_n + 1); }
      }
      _bstr_t& operator=(const _bstr_t& o) {
          if (this != &o) { delete[] m_p; m_p = nullptr; m_n = o.m_n;
              if (o.m_p) { m_p = new wchar_t[m_n + 1]; std::wmemcpy(m_p, o.m_p, m_n + 1); } }
          return *this;
      }
      ~_bstr_t() { delete[] m_p; }
      BSTR   GetBSTR()   const { return m_p; }
      size_t length()    const { return m_n; }
      operator const wchar_t*() const { return m_p; }
  };

  //  VT_CLSID round-trip helpers named by P2Pmsg.cpp (SDK propvarutil surface).
  //  The pair is internally consistent: the forward call tags the variant VT_CLSID
  //  with the GUID in uuidVal, and the reverse extractor reads it back (matching the
  //  `var.vt == VT_CLSID` branch in P2PmsgData_var).
  inline HRESULT VariantToGUID(const _variant_t& var, GUID* out) {
      if (!out) return E_FAIL;
      *out = var.uuidVal;
      return S_OK;
  }
  inline HRESULT InitVariantFromGUIDAsBuffer(REFGUID g, _variant_t* out) {
      if (!out) return E_FAIL;
      out->vt = VT_CLSID; out->uuidVal = g;
      return S_OK;
  }

  // -------------------------------------------------------------------------
  //  Windows min/max. Legacy code uses unqualified max()/min() (the windef.h
  //  macros). We provide GLOBAL function templates rather than macros: macros
  //  named max/min would textually corrupt std::max/std::min inside the libstdc++
  //  headers that stdafx.h includes AFTER the platform layer. There is no
  //  `using namespace std` in the compiled set, so unqualified max()/min() bind
  //  to these ::max/::min without ambiguity. Mixed-type args are supported.
  // -------------------------------------------------------------------------
  //  Plain `auto` return (NOT a trailing decltype): the conditional on two same-type
  //  lvalue parameters is an lvalue, so `-> decltype(a<b?b:a)` would deduce a REFERENCE
  //  and return a dangling ref to the by-value parameter (stack-use-after-return). Bare
  //  `auto` decays to a value, returning a safe copy.
  #ifndef P2P_MINMAX_DEFINED
  #define P2P_MINMAX_DEFINED
  template <class A, class B>
  constexpr auto max(A a, B b) { return a < b ? b : a; }
  template <class A, class B>
  constexpr auto min(A a, B b) { return a < b ? a : b; }
  #endif

#endif // _WIN32
