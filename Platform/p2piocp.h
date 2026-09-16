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
//  Platform layer — IOCP API surface over io_uring.  THE CORE OF THE PORT.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §5 in full).
//
//  _WIN32 : pass-through — the genuine NT IOCP API (CreateIoCompletionPort,
//           GetQueuedCompletionStatus, PostQueuedCompletionStatus, OVERLAPPED) is used
//           unchanged, so the Windows binary keeps real IOCP.
//  Linux  : one IoRing per pump thread (io_uring_queue_init with SINGLE_ISSUER |
//           DEFER_TASKRUN — safe *because* of the one-port-per-thread model), an
//           fd->completion-key association table, an MPSC posted-completion queue +
//           eventfd for PostQueuedCompletionStatus/SetEvent, cross-thread submission
//           marshalling, and cancel_fd teardown. CQE->GQCS translation reproduces the
//           three outcomes the pump relies on plus EOF (§5.2).
//
//  Phase status: Windows = pass-through. Linux IoRing = Phase 2. The IoRing class and
//  the io_uring machinery live in p2piocp.cpp (which links liburing); this header
//  declares only the Win32-shaped surface + the shim OVERLAPPED so legacy TUs that never
//  touch a socket (e.g. Msgcore) do not pull liburing.
//
#pragma once
#include "p2ptypes.h"

#if defined(_WIN32)
  #include <windows.h>   // real OVERLAPPED + IOCP entry points
#else
  #include <cstdint>

  // -------------------------------------------------------------------------
  //  OVERLAPPED shim (§5.1). Standard Win32 field names + layout so OVERLAPPEDcon
  //  can embed it as its first member and cast exactly as it does on Windows. The
  //  trailing shim-private fields are invisible to the legacy code (which only ever
  //  passes the OVERLAPPED* around and reads Internal/InternalHigh); io_uring holds
  //  the OVERLAPPED* in the SQE user_data exactly as the NT kernel held the pointer.
  // -------------------------------------------------------------------------
  struct OVERLAPPED {
      ULONG_PTR Internal;        // completion status: 0 on success, +errno on failure
      ULONG_PTR InternalHigh;    // bytes transferred
      union {
          struct { DWORD Offset; DWORD OffsetHigh; };
          PVOID Pointer;
      };
      HANDLE    hEvent;
      // ---- shim-private (absent on Windows) --------------------------------
      int       _p2p_fd;         // fd the op was submitted on
      int       _p2p_op;         // P2POp classifier
      void*     _p2p_ring;       // owning IoRing*
      ULONG_PTR _p2p_key;        // completion key, BOUND AT SUBMISSION -- see below
      //  Why the key is captured at submission rather than looked up from the
      //  fd at completion, which is what this shim used to do:
      //
      //  closesocket() cancels in-flight ops and then closes, and
      //  p2p_iocp_on_close_fd() erases the fd -> key association BEFORE those
      //  cancellations complete. So every ERROR_OPERATION_ABORTED the cancel
      //  produced arrived with completion key 0 instead of the key of the
      //  object that owned the I/O. Measured in Targetcore's production plan
      //  at Stage 2 step 6: dropping one timed-out connection delivered an
      //  unattributable completion, the service read it as a failure of the
      //  LISTENER, and a single silent peer took the whole listener down. That
      //  cascade is why DEF_P2PeerConLogin was 0 and the login deadline was
      //  unusable here.
      //
      //  Windows binds the key when the handle is associated with the port, and
      //  every completion carries it -- aborted ones included, and regardless of
      //  a later close. Capturing at submission reproduces that. It also closes
      //  an fd-recycling window the lookup had: a completion can no longer pick
      //  up the key of whatever socket reused its fd number.
      void*     _p2p_acceptsock; // ACCEPT: the pre-created accept-socket HANDLE the CQE's
                                 //         accepted fd is stashed into (§5.1)
      unsigned char _p2p_addr[28]; // CONNECT: copy of the target sockaddr (survives the
      int       _p2p_addrlen;      //          async gap; sockaddr_in/in6 fit in 28 bytes)
      void*     _p2p_wbuf;         // WRITE: base buffer, for short-write resubmission (§5.2)
      DWORD     _p2p_wlen;         // WRITE: total bytes requested
      DWORD     _p2p_wdone;        // WRITE: bytes written so far across partial CQEs
  };
  using LPOVERLAPPED = OVERLAPPED*;

  //  Op classifier stored in OVERLAPPED::_p2p_op (mirrors the four connection slots +
  //  the serial-poll case). The pump classifies a completion by OVERLAPPED* identity;
  //  this only helps the shim translate a CQE (a POLL yields 0 bytes; a 0-byte READ is
  //  a graceful peer close -> EOF).
  enum P2POp : int {
      P2POP_READ = 0, P2POP_WRITE = 1, P2POP_ACCEPT = 2, P2POP_CONNECT = 3, P2POP_POLL = 4
  };

  #ifndef INFINITE
    #define INFINITE 0xFFFFFFFFu
  #endif

  // -------------------------------------------------------------------------
  //  IOCP port API (implemented in p2piocp.cpp over liburing >= 2.5, kernel >= 6.1).
  // -------------------------------------------------------------------------
  //  ExistingPort == nullptr : create a bare port (one IoRing). If FileHandle is a real
  //  fd it is also associated with CompletionKey. NumberOfConcurrentThreads is ignored
  //  (one thread per ring). Returns a P2PHandle{Iocp} or nullptr on failure.
  //  ExistingPort != nullptr : associate FileHandle->fd with CompletionKey on that port.
  HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingPort,
                                ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads);

  //  Drain one completion. Reproduces the pump's three outcomes (§5.2):
  //   (a) FALSE + *lpOverlapped == nullptr  -> timeout / no work (WAIT_TIMEOUT)
  //   (b) FALSE + *lpOverlapped != nullptr  -> an I/O failed (GetLastError set;
  //                                            ERROR_HANDLE_EOF for a graceful 0-byte read,
  //                                            ERROR_OPERATION_ABORTED for a cancelled op)
  //   (c) TRUE                              -> success, *lpNumberOfBytes valid
  BOOL GetQueuedCompletionStatus(HANDLE Port, LPDWORD lpNumberOfBytes,
                                 PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped,
                                 DWORD dwMilliseconds);

  //  Post an arbitrary (bytes, key, OVERLAPPED*) triple from any thread (Wakeup + Dmx).
  BOOL PostQueuedCompletionStatus(HANDLE Port, DWORD dwNumberOfBytesTransferred,
                                  ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

  // -------------------------------------------------------------------------
  //  Async submit primitives — what the async ReadFile/WriteFile and the transports
  //  call. On a successful io_uring_submit they ALWAYS return FALSE + ERROR_IO_PENDING,
  //  preserving the Fix-4 convention (P2Peerio::Send/Recv `return ERROR_IO_PENDING;`).
  //  io_uring always posts a CQE even for a synchronously-ready op, so the "a completion
  //  packet is always posted" invariant holds by construction (§5.2).
  // -------------------------------------------------------------------------
  BOOL p2p_iocp_read (HANDLE Port, HANDLE File, void* buf, DWORD n, LPOVERLAPPED ov);
  BOOL p2p_iocp_write(HANDLE Port, HANDLE File, const void* buf, DWORD n, LPOVERLAPPED ov);

  //  Cancel all in-flight ops on File (the CloseHandle §5.6 path). Each cancelled op
  //  surfaces later through GetQueuedCompletionStatus as ERROR_OPERATION_ABORTED. The
  //  request is marshalled to the ring's pump thread when called cross-thread.
  BOOL p2p_iocp_cancel(HANDLE Port, HANDLE File);

  //  Tear a port down: cancel-all, drain outstanding CQEs, io_uring_queue_exit, free.
  //  (Phase-3 wires this into CloseHandle(Iocp); named explicitly for now.)
  BOOL p2p_iocp_destroy(HANDLE Port);
#endif
