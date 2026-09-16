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
//  Platform layer — file I/O (sync + async CreateFile/ReadFile/WriteFile).
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §5, §6.1).
//
//  _WIN32 : pass-through. Linux: sync store/lock-file I/O in P2PmsgMgr.cpp maps to
//  open/pread/pwrite/unlink (O_CLOEXEC); lock-file semantics (GENERIC_WRITE|DELETE +
//  share modes) reproduced with O_EXCL + flock (§6.1). Async ReadFile/WriteFile route
//  to io_uring and ALWAYS return FALSE + ERROR_IO_PENDING to preserve the Fix-4
//  convention (§5.2); that lives in p2piocp.* (Phase 2).
//
//  Phase status: Windows = pass-through. Linux sync file I/O = Phase 1; async = Phase 2.
//
#pragma once
#include "p2ptypes.h"

#if defined(_WIN32)
  #include <windows.h>   // CreateFileW/ReadFile/WriteFile/DeleteFileW, GENERIC_*, etc.
#else
  #include <string>
  #include <cstdint>
  #include <cstdlib>      // getenv/malloc/free (SHGetKnownFolderPath)
  #include <cstdio>       // ::remove (_wremove)
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/file.h>   // flock
  #include <cerrno>

  // -------------------------------------------------------------------------
  //  IOCP async hooks, provided by p2piocp.cpp (linked into Targetcore + the shim
  //  test, NOT into Msgcore). Declared *weak* so a Msgcore-only shared library —
  //  which never issues overlapped I/O and never links liburing — resolves them to
  //  null and links cleanly; every call below is guarded by an address test.
  //   - p2p_iocp_submit_rw : overlapped ReadFile/WriteFile -> io_uring on the ring
  //     that owns `fd` (found via the fd->ring registry populated at association),
  //     always returning FALSE+ERROR_IO_PENDING (Fix-4, §5.2). isWrite: 0=read 1=write.
  //   - p2p_iocp_on_close_fd : §5.6 — if `fd` is IOCP-associated, cancel its in-flight
  //     ops (they surface as ERROR_OPERATION_ABORTED) and drop the association before
  //     the fd is closed.
  //   - p2p_iocp_close_port : tear a completion port (IoRing) down (extern "C" façade
  //     over p2piocp.cpp's p2p_iocp_destroy; kept distinct so p2piocp.h can keep the
  //     C++-linkage p2p_iocp_destroy without a same-name/different-linkage clash here).
  // -------------------------------------------------------------------------
  extern "C" BOOL p2p_iocp_submit_rw(int fd, void* buf, DWORD n, void* ov, int isWrite)
      __attribute__((weak));
  extern "C" void p2p_iocp_on_close_fd(int fd) __attribute__((weak));
  extern "C" BOOL p2p_iocp_close_port(HANDLE Port) __attribute__((weak));

  //  Thread-handle teardown (join/detach + free impl) is defined inline in p2pthread.h;
  //  forward-declared here so CloseHandle's Thread case compiles regardless of the order
  //  in which this header and p2pthread.h are included (platform.h pulls both).
  void p2p_thread_close(HANDLE h);

  //  Named-pipe CLIENT connect (§6.2). A named-pipe client opens the pipe with
  //  CreateFile("\\.\pipe\Name"); on Linux that is an AF_UNIX socket()+connect() to the
  //  same $XDG_RUNTIME_DIR/p2pmsg/Name.sock the server bound (see p2p_pipe_path). Defined
  //  in p2psock.h (which owns the socket APIs) and forward-declared here because p2pfile.h
  //  is pulled before p2psock.h; both are in the same TU via platform.h, so CreateFileW's
  //  call resolves at the legacy call site. Returns a P2PHandle{Fd} on the connected socket
  //  or INVALID_HANDLE_VALUE (GetLastError set).
  HANDLE p2p_pipe_client_connect(const wchar_t* name);

  // -------------------------------------------------------------------------
  //  Win32 file constants (the subset P2PmsgMgr.cpp names). Access rights are the
  //  real Win32 bit values; creation dispositions and share flags likewise.
  // -------------------------------------------------------------------------
  #ifndef GENERIC_READ
    #define GENERIC_READ      0x80000000u
    #define GENERIC_WRITE     0x40000000u
    #define GENERIC_EXECUTE   0x20000000u
    #define GENERIC_ALL       0x10000000u
    #define DELETE            0x00010000u
    //  Real Win32 value. Not folded into GENERIC_WRITE: a handle opened with it
    //  appends, and CreateFileW maps it to O_WRONLY|O_APPEND below.
    #define FILE_APPEND_DATA  0x00000004u
  #endif
  #ifndef FILE_SHARE_READ
    #define FILE_SHARE_READ   0x1
    #define FILE_SHARE_WRITE  0x2
    #define FILE_SHARE_DELETE 0x4
  #endif
  #ifndef CREATE_NEW
    #define CREATE_NEW        1
    #define CREATE_ALWAYS     2
    #define OPEN_EXISTING     3
    #define OPEN_ALWAYS       4
    #define TRUNCATE_EXISTING 5
  #endif
  #ifndef FILE_ATTRIBUTE_NORMAL
    #define FILE_ATTRIBUTE_NORMAL     0x00000080
    #define FILE_ATTRIBUTE_TEMPORARY  0x00000100
    #define FILE_FLAG_DELETE_ON_CLOSE 0x04000000
    #define FILE_FLAG_OVERLAPPED      0x40000000
    #define FILE_FLAG_WRITE_THROUGH   0x80000000
  #endif
  #ifndef MOVEFILE_REPLACE_EXISTING
    #define MOVEFILE_REPLACE_EXISTING 0x1
    #define MOVEFILE_COPY_ALLOWED     0x2
    #define MOVEFILE_WRITE_THROUGH    0x8
  #endif
  #ifndef FILE_BEGIN
    #define FILE_BEGIN   0
    #define FILE_CURRENT 1
    #define FILE_END     2
  #endif
  #ifndef INVALID_FILE_SIZE
    #define INVALID_FILE_SIZE        ((DWORD)0xFFFFFFFF)
    #define INVALID_SET_FILE_POINTER ((DWORD)-1)
  #endif

  //  MSVC low-level CRT I/O (TargetcoreLog uses _open/_write/_close on a raw int fd,
  //  not the tagged HANDLE). Flags map to POSIX; _O_BINARY is a no-op on Linux. The
  //  functions forward straight to the POSIX syscalls (same int-fd contract).
  #ifndef _O_RDONLY
    #define _O_RDONLY O_RDONLY
    #define _O_WRONLY O_WRONLY
    #define _O_RDWR   O_RDWR
    #define _O_APPEND O_APPEND
    #define _O_CREAT  O_CREAT
    #define _O_TRUNC  O_TRUNC
    #define _O_EXCL   O_EXCL
    #define _O_BINARY 0
    #define _O_TEXT   0
  #endif
  #ifndef _S_IREAD
    #define _S_IREAD  S_IRUSR
    #define _S_IWRITE S_IWUSR
  #endif
  inline int _open(const char* path, int flags, int mode = 0644) { return ::open(path, flags | O_CLOEXEC, mode); }
  inline int _close(int fd)                                       { return ::close(fd); }
  inline int _write(int fd, const void* buf, unsigned n)          { return (int)::write(fd, buf, n); }
  inline int _read (int fd, void* buf, unsigned n)                { return (int)::read(fd, buf, n); }
  inline long _lseek(int fd, long off, int whence)                { return (long)::lseek(fd, off, whence); }

  // -------------------------------------------------------------------------
  //  wchar_t path (UTF-32 on Linux) -> UTF-8 bytes for the POSIX call. Full
  //  Unicode; ASCII paths (the common case) pass through unchanged.
  // -------------------------------------------------------------------------
  inline std::string p2p_wtopath(const wchar_t* w) {
      std::string s;
      if (!w) return s;
      for (; *w; ++w) {
          char32_t c = (char32_t)*w;
          if      (c < 0x80)    s.push_back((char)c);
          else if (c < 0x800)  { s.push_back((char)(0xC0 | (c >> 6)));
                                 s.push_back((char)(0x80 | (c & 0x3F))); }
          else if (c < 0x10000){ s.push_back((char)(0xE0 | (c >> 12)));
                                 s.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                                 s.push_back((char)(0x80 | (c & 0x3F))); }
          else                 { s.push_back((char)(0xF0 | (c >> 18)));
                                 s.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
                                 s.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                                 s.push_back((char)(0x80 | (c & 0x3F))); }
      }
      return s;
  }

  // -------------------------------------------------------------------------
  //  Synchronous file API over POSIX (§6.1). The HANDLE is a P2PHandle{Fd}; a
  //  FILE_FLAG_DELETE_ON_CLOSE handle stashes its path in `impl` (unlinked on
  //  close). Exclusive share (dwShare == 0) maps to an advisory flock(LOCK_EX|
  //  LOCK_NB) — a held lock yields ERROR_SHARING_VIOLATION, reproducing the
  //  lock-file contention P2PmsgMgr relies on.
  // -------------------------------------------------------------------------
  inline HANDLE CreateFileW(const wchar_t* name, DWORD access, DWORD share,
                            void* /*sec*/, DWORD creation, DWORD flags, HANDLE /*tmpl*/) {
      std::string path = p2p_wtopath(name);
      //  Named-pipe CLIENT: \\.\pipe\Name (or //./pipe/Name) -> AF_UNIX socket()+connect()
      //  to the server's bound socket path (§6.2). Must precede the ordinary-file path so a
      //  pipe name is never open(2)'d as a file. FILE_FLAG_OVERLAPPED / share / creation are
      //  irrelevant here — the client Connect() associates the fd with the ring and simulates
      //  the connect completion via PostOVERLAPPED, exactly as on Windows.
      if (path.rfind("\\\\.\\pipe\\", 0) == 0 || path.rfind("//./pipe/", 0) == 0)
          return p2p_pipe_client_connect(name);
      //  Serial device: \\.\COMx -> /dev/ttyS(x-1) (§6.2, 232 transport). The map is
      //  CONFIGURABLE per §6.2: an env var P2P_COM<n> overrides the default device path
      //  for COM<n> (used by the com232 mesh test to point the two ends at a PTY null-modem
      //  pair, and by real deployments to map COM<n> to any /dev/tty* / /dev/pts/*). Only
      //  this exact device prefix is remapped; ordinary file paths pass through unchanged.
      bool isCom = false;
      if (path.rfind("\\\\.\\COM", 0) == 0) {
          isCom = true;
          int comn = 0;
          for (const char* p = path.c_str() + 7; *p >= '0' && *p <= '9'; ++p) comn = comn * 10 + (*p - '0');
          if (comn > 0) {
              std::string ev = "P2P_COM" + std::to_string(comn);
              const char* over = std::getenv(ev.c_str());
              path = (over && *over) ? std::string(over)
                                     : "/dev/ttyS" + std::to_string(comn - 1);
          }
      }
      int oflags = 0;
      bool ap = (access & FILE_APPEND_DATA) != 0;
      bool wr = (access & GENERIC_WRITE) != 0 || ap, rd = (access & GENERIC_READ) != 0;
      if (wr && rd)      oflags = O_RDWR;
      else if (wr)       oflags = O_WRONLY;
      else               oflags = O_RDONLY;
      if (ap)            oflags |= O_APPEND;
      switch (creation) {
          case CREATE_ALWAYS:     oflags |= O_CREAT | O_TRUNC; break;
          case CREATE_NEW:        oflags |= O_CREAT | O_EXCL;  break;
          case OPEN_ALWAYS:       oflags |= O_CREAT;           break;
          case TRUNCATE_EXISTING: oflags |= O_TRUNC;           break;
          case OPEN_EXISTING:     default:                     break;
      }
      oflags |= O_CLOEXEC;
      if (isCom) oflags |= O_NOCTTY;   // a serial/tty device must not become a controlling terminal
      int fd = ::open(path.c_str(), oflags, 0644);
      if (fd < 0) { SetLastError(win32_from_errno(errno)); return INVALID_HANDLE_VALUE; }
      if (share == 0) {                                   // exclusive share => advisory lock
          if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
              int e = errno; ::close(fd);
              SetLastError(e == EWOULDBLOCK ? ERROR_SHARING_VIOLATION : win32_from_errno(e));
              return INVALID_HANDLE_VALUE;
          }
      }
      auto* h = p2p_handle_new(HKind::Fd, fd, nullptr);
      if (flags & FILE_FLAG_DELETE_ON_CLOSE) h->impl = new std::string(path);
      return h;
  }
  #define CreateFile CreateFileW

  inline BOOL ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, void* ov) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); if (read) *read = 0; return FALSE; }
      if (ov) {   // overlapped/async: submit to the owning ring (Fix-4: FALSE + ERROR_IO_PENDING)
          if (&p2p_iocp_submit_rw) return p2p_iocp_submit_rw(h->fd, buf, n, ov, 0);
          SetLastError(ERROR_INVALID_PARAMETER); return FALSE;   // no ring linked in this image
      }
      char* p = (char*)buf; DWORD total = 0;
      while (total < n) {
          ssize_t r = ::read(h->fd, p + total, n - total);
          if (r < 0)  { SetLastError(win32_from_errno(errno)); if (read) *read = total; return FALSE; }
          if (r == 0) break;                              // EOF
          total += (DWORD)r;
      }
      if (read) *read = total;
      return TRUE;
  }

  inline BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD written, void* ov) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); if (written) *written = 0; return FALSE; }
      if (ov) {   // overlapped/async: submit to the owning ring (Fix-4: FALSE + ERROR_IO_PENDING)
          if (&p2p_iocp_submit_rw) return p2p_iocp_submit_rw(h->fd, const_cast<void*>(buf), n, ov, 1);
          SetLastError(ERROR_INVALID_PARAMETER); return FALSE;   // no ring linked in this image
      }
      const char* p = (const char*)buf; DWORD total = 0;
      while (total < n) {
          ssize_t w = ::write(h->fd, p + total, n - total);
          if (w < 0) { SetLastError(win32_from_errno(errno)); if (written) *written = total; return FALSE; }
          total += (DWORD)w;
      }
      if (written) *written = total;
      return TRUE;
  }

  inline DWORD GetFileSize(HANDLE h, LPDWORD hi) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_FILE_SIZE; }
      struct stat st{};
      if (::fstat(h->fd, &st) != 0) { SetLastError(win32_from_errno(errno)); return INVALID_FILE_SIZE; }
      std::uint64_t sz = (std::uint64_t)st.st_size;
      if (hi) *hi = (DWORD)(sz >> 32);
      return (DWORD)(sz & 0xFFFFFFFFu);
  }

  inline DWORD SetFilePointer(HANDLE h, LONG lo, LONG* hi, DWORD method) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_SET_FILE_POINTER; }
      std::int64_t off = hi ? ((std::int64_t)(*hi) << 32 | (std::uint32_t)lo) : (std::int64_t)lo;
      int whence = method == FILE_END ? SEEK_END : method == FILE_CURRENT ? SEEK_CUR : SEEK_SET;
      off_t r = ::lseek(h->fd, (off_t)off, whence);
      if (r == (off_t)-1) { SetLastError(win32_from_errno(errno)); return INVALID_SET_FILE_POINTER; }
      if (hi) *hi = (LONG)((std::uint64_t)r >> 32);
      return (DWORD)((std::uint64_t)r & 0xFFFFFFFFu);
  }

  inline BOOL FlushFileBuffers(HANDLE h) {
      if (!h || h->kind != HKind::Fd) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      return ::fsync(h->fd) == 0 ? TRUE : (SetLastError(win32_from_errno(errno)), FALSE);
  }

  inline BOOL DeleteFileW(const wchar_t* name) {
      if (::unlink(p2p_wtopath(name).c_str()) != 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      return TRUE;
  }
  #define DeleteFile DeleteFileW

  //  GetFileSizeEx: the 64-bit size in one call, which is what the callers that
  //  do not want the two-DWORD dance use.
  inline BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER* out) {
      if (!h || h->kind != HKind::Fd || !out) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      struct stat st{};
      if (::fstat(h->fd, &st) != 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      out->QuadPart = (std::int64_t)st.st_size;
      return TRUE;
  }

  // -------------------------------------------------------------------------
  //  ANSI (…A) entry points. The legacy set and the test harnesses name both
  //  widths, and on Windows both are real exports. These are deliberately NOT a
  //  second implementation: each widens its path and delegates to the W form,
  //  so the pipe / COM-remap / share-lock behaviour cannot drift between the
  //  two spellings — a class of bug that would appear on one platform only.
  // -------------------------------------------------------------------------
  inline std::wstring p2p_pathtow(const char* s) {   // UTF-8 -> wide, inverse of p2p_wtopath
      std::wstring w;
      if (!s) return w;
      for (const unsigned char* p = (const unsigned char*)s; *p; ) {
          char32_t c = *p; std::size_t extra = 0;
          if      ((c & 0x80) == 0)    { extra = 0; }
          else if ((c & 0xE0) == 0xC0) { c &= 0x1F; extra = 1; }
          else if ((c & 0xF0) == 0xE0) { c &= 0x0F; extra = 2; }
          else if ((c & 0xF8) == 0xF0) { c &= 0x07; extra = 3; }
          ++p;
          for (std::size_t i = 0; i < extra && (*p & 0xC0) == 0x80; ++i, ++p)
              c = (c << 6) | (*p & 0x3F);
          w.push_back((wchar_t)c);
      }
      return w;
  }

  inline HANDLE CreateFileA(const char* name, DWORD access, DWORD share,
                            void* sec, DWORD creation, DWORD flags, HANDLE tmpl) {
      return CreateFileW(p2p_pathtow(name).c_str(), access, share, sec, creation, flags, tmpl);
  }
  inline BOOL DeleteFileA(const char* name) {
      return DeleteFileW(p2p_pathtow(name).c_str());
  }

  //  GetTempPathA: same contract as the W form (trailing separator, returns
  //  chars written excluding NUL, or the required size including NUL).
  inline DWORD GetTempPathA(DWORD nBufferLength, char* lpBuffer) {
      const char* t = std::getenv("TMPDIR");
      std::string dir = (t && *t) ? std::string(t) : std::string("/tmp");
      if (dir.empty() || dir.back() != '/') dir.push_back('/');
      DWORD need = (DWORD)dir.size();
      if (!lpBuffer || nBufferLength <= need) return need + 1;
      std::memcpy(lpBuffer, dir.c_str(), need + 1);
      return need;
  }

  //  GetTempPathW: the process temp directory WITH a trailing separator, matching the
  //  Win32 contract — returns the char count written (excluding NUL) on success, or the
  //  required size (including NUL) when the buffer is too small, 0 on failure. $TMPDIR
  //  else /tmp; the trailing '/' is the POSIX analogue of Win32's trailing backslash.
  inline DWORD GetTempPathW(DWORD nBufferLength, wchar_t* lpBuffer) {
      const char* t = std::getenv("TMPDIR");
      std::string dir = (t && *t) ? std::string(t) : std::string("/tmp");
      if (dir.empty() || dir.back() != '/') dir.push_back('/');
      DWORD need = (DWORD)dir.size();
      if (!lpBuffer || nBufferLength <= need) return need + 1;   // size incl. NUL
      std::size_t i = 0; for (char c : dir) lpBuffer[i++] = (wchar_t)(unsigned char)c;
      lpBuffer[i] = 0;
      return need;   // chars written, excluding NUL
  }

  //  _wremove: CRT wide remove(). Convert to the UTF-8 path and delete; returns 0 on
  //  success, non-zero on failure (matching the CRT), leaving errno set.
  inline int _wremove(const wchar_t* path) {
      return ::remove(p2p_wtopath(path).c_str());
  }

  inline BOOL MoveFileExW(const wchar_t* src, const wchar_t* dst, DWORD flags) {
      //  POSIX rename() already atomically replaces an existing dst on the same
      //  volume, matching MOVEFILE_REPLACE_EXISTING (which every live caller sets).
      std::string s = p2p_wtopath(src), d = p2p_wtopath(dst);
      if (::rename(s.c_str(), d.c_str()) != 0) { SetLastError(win32_from_errno(errno)); return FALSE; }
      (void)flags;   // MOVEFILE_WRITE_THROUGH: rename metadata is journalled; dir-fsync TODO
      return TRUE;
  }
  #define MoveFileEx MoveFileExW

  //  SHGetKnownFolderPath(FOLDERID_LocalAppData) -> $XDG_DATA_HOME (else $HOME/.local/share),
  //  returned as a CoTaskMemFree-able wide string. The rfid is ignored — the compiled set
  //  only ever asks for LocalAppData (the log directory base).
  using KNOWNFOLDERID    = GUID;
  using REFKNOWNFOLDERID = const GUID&;
  inline const GUID FOLDERID_LocalAppData =
      { 0xF1B32785,0x6FBA,0x4FCF,{0x9D,0x55,0x7B,0x8E,0x7F,0x15,0x70,0x91} };
  inline HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID, DWORD, HANDLE, wchar_t** out) {
      if (!out) return (HRESULT)-1;
      const char* x = std::getenv("XDG_DATA_HOME");
      std::string base = (x && *x) ? std::string(x)
          : (std::getenv("HOME") ? std::string(std::getenv("HOME")) + "/.local/share"
                                 : std::string("/tmp"));
      wchar_t* buf = (wchar_t*)std::malloc((base.size() + 1) * sizeof(wchar_t));
      if (!buf) return (HRESULT)-1;
      std::size_t i = 0; for (char c : base) buf[i++] = (wchar_t)(unsigned char)c; buf[i] = 0;
      *out = buf; return 0;   // S_OK
  }
  inline void CoTaskMemFree(void* p) { std::free(p); }

  //  Unified CloseHandle, dispatching on HKind (the Linux port plan §4.1, §5.6):
  //   - Fd    : if IOCP-associated, cancel in-flight ops + drop the association first
  //             (§5.6), then honour DELETE_ON_CLOSE, close() (releasing any flock).
  //   - Iocp  : tear the completion port / IoRing down (delegates to p2p_iocp_destroy,
  //             which also frees the handle).
  //   - Event : close the eventfd, free the manual-reset flag.
  //   - Thread: joined/detached when jthread spawn lands (Phase 3); free for now.
  inline BOOL CloseHandle(HANDLE h) {
      if (!h) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      //  Already closed: fail rather than run the kind switch over a retired handle.
      if (h->retired.load(std::memory_order_acquire)) {
          SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      switch (h->kind) {
          case HKind::Iocp:
              if (&p2p_iocp_close_port) return p2p_iocp_close_port(h);   // retires h too
              break;                                                     // no ring linked: retire below
          case HKind::Event: {
              if (h->impl) delete static_cast<bool*>(h->impl);
              int fd = h->fd.exchange(-1, std::memory_order_acq_rel);
              if (fd >= 0) ::close(fd);
              break;
          }
          case HKind::Fd: {
              //  Take the descriptor away from the handle first — see closesocket in
              //  p2psock.h for why the exchange rather than a plain read.
              int fd = h->fd.exchange(-1, std::memory_order_acq_rel);
              if (fd >= 0 && &p2p_iocp_on_close_fd) p2p_iocp_on_close_fd(fd);  // §5.6 cancel + deassoc
              if (h->impl) { auto* p = static_cast<std::string*>(h->impl); ::unlink(p->c_str()); delete p; }
              if (fd >= 0) ::close(fd);   // also releases any flock
              break;
          }
          case HKind::Thread:
              p2p_thread_close(h);   // join/detach the thread + free its impl
              break;
          default:
              break;
      }
      p2p_handle_retire(h);           // retired, not freed (§4.1)
      return TRUE;
  }
#endif
