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
//  Platform layer — same-host, cross-process shared memory (p2p_performance §6.3a, S1).
//
//  The primitives under Targetcore's shared-memory transport: one MAPPING holding two
//  single-producer / single-consumer byte RINGS (one per direction), a WAKE object per side
//  that lands in that side's pump completion port, and the means to hand both to the peer
//  process. Header-only, no exported symbols.
//
//  THE TRUST RULE. The peer is another process inside the host boundary but outside ours: it
//  can rewrite every byte of the mapping at any moment. So the reader COPIES bytes out
//  before anything looks at them (RingRead), validates every position it is given, and
//  reports a bad ring rather than following it; the bytes then go through the same frame
//  validation as a socket's (Targetcore P2Peerio::RecvP2PeerMsg); the mapping is fixed-size (Windows: a section cannot shrink;
//  Linux: the memfd is SEALED against shrink/grow, which the opener verifies), so a peer
//  cannot turn our own reads into a fault.
//
//  WAKES, only when the other side sleeps. Each ring carries readerParked / writerParked
//  words. A side about to sleep sets its word, fences, and re-checks the ring (Dekker): the
//  other side publishes its position, fences, and signals only if it takes the word from 1
//  to 0. A lost race costs one spurious wake, never a lost one.
//
//  _WIN32 : unnamed CreateFileMapping; an auto-reset event per side, tied to the pump's
//           completion port with NtAssociateWaitCompletionPacket (ntdll, Win8+, resolved at
//           run time — WakeSupported() is false without it and the transport refuses to arm);
//           handles reach the peer with DuplicateHandle.
//  Linux  : memfd_create + seals + mmap(MAP_SHARED); an eventfd per side waited on as a
//           one-shot POLLIN completion (p2p_iocp_poll, provided weakly by p2piocp.cpp);
//           descriptors reach the peer with SCM_RIGHTS over the AF_UNIX pipe.
//
#pragma once
#include "p2ptypes.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstddef>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <cerrno>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <sys/eventfd.h>
  #include <sys/socket.h>
  #include <sys/uio.h>
  #include <poll.h>
  #include "p2pfile.h"        // CloseHandle on an Fd handle (cancels + closes)
  #include "p2piocp.h"        // CreateIoCompletionPort (fd -> port association)
  //  Async POLLIN wait - provided weakly by p2piocp.cpp (linked into Targetcore), as for
  //  p2pserial.h.
  extern "C" BOOL p2p_iocp_poll(int fd, void* ov) __attribute__((weak));
#endif

namespace p2pshm
{
    // ------------------------------------------------------------------------------------
    //  Layout. Each ring is a BYTE STREAM, a socket's semantics: positions are byte counts
    //  that only ever grow (mod 2^32), used = tail - head, and reads and writes may be
    //  short. Framing is the caller's: Targetcore's P2Peerio already frames every message
    //  and validates what it reads as it would from a socket (p2p_performance §6.3a,
    //  revised at S2).
    // ------------------------------------------------------------------------------------
    constexpr uint32_t kMagic     = 0x48533250u;      // "P2SH"
    constexpr uint32_t kVersion   = 2;                // 1 was the length-prefixed draft
    constexpr uint32_t kRingBytes = 64u * 1024u;      // power of two
    constexpr uint32_t kMaxFrame  = 32768u;           // recorded in the Block for the peer
    static_assert ( (kRingBytes & (kRingBytes - 1)) == 0, "ring size must be a power of two" );

    struct Ring
    {
        alignas(64) std::atomic<uint32_t> tail;          // producer's
        alignas(64) std::atomic<uint32_t> head;          // consumer's
        alignas(64) std::atomic<uint32_t> readerParked;  // consumer asleep: wake it on data
                    std::atomic<uint32_t> writerParked;  // producer asleep: wake it on space
        alignas(64) uint8_t               data[kRingBytes];
    };
    static_assert ( std::atomic<uint32_t>::is_always_lock_free,
                    "ring words must be lock-free to be shared across processes" );

    //  ring[0] carries accepter -> dialler, ring[1] dialler -> accepter.
    struct Block
    {
        uint32_t magic;
        uint32_t version;
        uint32_t ringBytes;
        uint32_t maxFrame;
        alignas(64) Ring ring[2];
    };
    constexpr size_t kBlockBytes = sizeof ( Block );

    inline void
    BlockInit ( Block* b )
    {
        std::memset ( static_cast<void*> ( b ), 0, sizeof ( Block ) );
        b->magic = kMagic; b->version = kVersion; b->ringBytes = kRingBytes; b->maxFrame = kMaxFrame;
        std::atomic_thread_fence ( std::memory_order_release );
    }

    inline bool
    BlockValid ( const Block* b )
    {
        return b && b->magic == kMagic && b->version == kVersion &&
               b->ringBytes == kRingBytes && b->maxFrame == kMaxFrame;
    }

    // ------------------------------------------------------------------------------------
    //  Ring operations
    // ------------------------------------------------------------------------------------
    enum class Put { Ok, Full, Bad };
    enum class Get { Ok, Empty, Bad };

    inline void
    CopyIn ( Ring& r, uint32_t pos, const void* src, uint32_t n )
    {
        const uint32_t off   = pos & (kRingBytes - 1);
        const uint32_t first = (n < kRingBytes - off) ? n : kRingBytes - off;
        std::memcpy ( r.data + off, src, first );
        if ( n > first )
          std::memcpy ( r.data, static_cast<const uint8_t*> ( src ) + first, n - first );
    }

    inline void
    CopyOut ( const Ring& r, uint32_t pos, void* dst, uint32_t n )
    {
        const uint32_t off   = pos & (kRingBytes - 1);
        const uint32_t first = (n < kRingBytes - off) ? n : kRingBytes - off;
        std::memcpy ( dst, r.data + off, first );
        if ( n > first )
          std::memcpy ( static_cast<uint8_t*> ( dst ) + first, r.data, n - first );
    }

    //  Producer. Appends up to n bytes: as many as fit, so `written` may be short.
    //  wakeReader is set when the consumer was parked and this call took its word: the
    //  caller then signals the consumer's wake.
    //  Full: no room at all (park with WriterPark, retry). Bad: the consumer's head is not a
    //  position this ring could hold (a corrupt or hostile peer).
    inline Put
    RingWrite ( Ring& r, const void* p, uint32_t n, uint32_t& written, bool& wakeReader )
    {
        wakeReader = false;
        written = 0;
        const uint32_t tail = r.tail.load ( std::memory_order_relaxed );
        const uint32_t head = r.head.load ( std::memory_order_acquire );
        const uint32_t used = tail - head;
        if ( used > kRingBytes )
          return Put::Bad;
        const uint32_t room = kRingBytes - used;
        if ( room == 0 )
          return Put::Full;
        const uint32_t k = n < room ? n : room;
        CopyIn ( r, tail, p, k );
        r.tail.store ( tail + k, std::memory_order_release );
        std::atomic_thread_fence ( std::memory_order_seq_cst );
        if ( r.readerParked.load ( std::memory_order_relaxed ) &&
             r.readerParked.exchange ( 0, std::memory_order_acq_rel ) )
          wakeReader = true;
        written = k;
        return Put::Ok;
    }

    //  Consumer. Copies up to cap bytes out into buf and frees their space; `got` may be
    //  short. The bytes are the caller's private copy before it looks at them: the peer can
    //  rewrite the ring the instant after. wakeWriter is set when the producer was parked
    //  on space and this call took its word.
    //  Bad: the producer published a tail no honest producer could (more than a ring ahead).
    inline Get
    RingRead ( Ring& r, void* buf, uint32_t cap, uint32_t& got, bool& wakeWriter )
    {
        wakeWriter = false;
        got = 0;
        const uint32_t head  = r.head.load ( std::memory_order_relaxed );
        const uint32_t tail  = r.tail.load ( std::memory_order_acquire );
        const uint32_t avail = tail - head;
        if ( avail == 0 )
          return Get::Empty;
        if ( avail > kRingBytes )
          return Get::Bad;
        const uint32_t k = cap < avail ? cap : avail;
        CopyOut ( r, head, buf, k );
        r.head.store ( head + k, std::memory_order_release );
        std::atomic_thread_fence ( std::memory_order_seq_cst );
        if ( r.writerParked.load ( std::memory_order_relaxed ) &&
             r.writerParked.exchange ( 0, std::memory_order_acq_rel ) )
          wakeWriter = true;
        got = k;
        return Get::Ok;
    }

    //  The consumer is about to sleep. TRUE: still empty, sleep (the producer will wake it).
    //  FALSE: data arrived meanwhile - do not sleep; the word is withdrawn, and if a producer
    //  already took it a spurious wake follows, which the caller must tolerate.
    inline bool
    ReaderPark ( Ring& r )
    {
        r.readerParked.store ( 1, std::memory_order_relaxed );
        std::atomic_thread_fence ( std::memory_order_seq_cst );
        if ( r.tail.load ( std::memory_order_acquire ) == r.head.load ( std::memory_order_relaxed ) )
          return true;
        r.readerParked.exchange ( 0, std::memory_order_acq_rel );
        return false;
    }

    //  The producer found the ring full. Same contract: TRUE sleep, FALSE room appeared.
    inline bool
    WriterPark ( Ring& r )
    {
        r.writerParked.store ( 1, std::memory_order_relaxed );
        std::atomic_thread_fence ( std::memory_order_seq_cst );
        const uint32_t used = r.tail.load ( std::memory_order_relaxed ) -
                              r.head.load ( std::memory_order_acquire );
        if ( used >= kRingBytes )
          return true;
        r.writerParked.exchange ( 0, std::memory_order_acq_rel );
        return false;
    }

    // ------------------------------------------------------------------------------------
    //  Mapping
    // ------------------------------------------------------------------------------------
#if defined(_WIN32)
    struct Mapping { HANDLE h = nullptr; Block* view = nullptr; };

    //  Creator: an unnamed section (no name another process could guess or squat).
    inline bool
    MappingCreate ( Mapping& m )
    {
        m.h = CreateFileMappingW ( INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                   (DWORD)kBlockBytes, nullptr );
        if ( !m.h ) return false;
        m.view = static_cast<Block*> ( MapViewOfFile ( m.h, FILE_MAP_ALL_ACCESS, 0, 0, kBlockBytes ) );
        if ( !m.view ) { CloseHandle ( m.h ); m.h = nullptr; return false; }
        BlockInit ( m.view );
        return true;
    }

    //  Opener: a section handle the creator duplicated into this process.
    inline bool
    MappingOpen ( Mapping& m, HANDLE h )
    {
        m.h = h;
        m.view = static_cast<Block*> ( MapViewOfFile ( h, FILE_MAP_ALL_ACCESS, 0, 0, kBlockBytes ) );
        if ( !m.view || !BlockValid ( m.view ) )
        {
          if ( m.view ) UnmapViewOfFile ( m.view );
          m.view = nullptr; return false;
        }
        return true;
    }

    inline void
    MappingClose ( Mapping& m )
    {
        if ( m.view ) UnmapViewOfFile ( m.view );
        if ( m.h )    CloseHandle ( m.h );
        m.view = nullptr; m.h = nullptr;
    }

    //  A handle of ours, made valid in the process hTarget (which needs PROCESS_DUP_HANDLE).
    //  Returns the value to send to the peer, or nullptr.
    inline HANDLE
    DuplicateInto ( HANDLE hTarget, HANDLE h )
    {
        HANDLE hOut = nullptr;
        if ( !DuplicateHandle ( GetCurrentProcess ( ), h, hTarget, &hOut, 0, FALSE, DUPLICATE_SAME_ACCESS ) )
          return nullptr;
        return hOut;
    }
#else
    struct Mapping { int fd = -1; Block* view = nullptr; };

    constexpr int kSeals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;

    //  Creator: an anonymous memfd, sized, then sealed so neither side can resize it.
    inline bool
    MappingCreate ( Mapping& m )
    {
        m.fd = memfd_create ( "p2pshm", MFD_CLOEXEC | MFD_ALLOW_SEALING );
        if ( m.fd < 0 ) return false;
        if ( ftruncate ( m.fd, (off_t)kBlockBytes ) != 0 ||
             fcntl ( m.fd, F_ADD_SEALS, kSeals ) != 0 )
        { close ( m.fd ); m.fd = -1; return false; }
        void* pv = mmap ( nullptr, kBlockBytes, PROT_READ | PROT_WRITE, MAP_SHARED, m.fd, 0 );
        if ( pv == MAP_FAILED ) { close ( m.fd ); m.fd = -1; return false; }
        m.view = static_cast<Block*> ( pv );
        BlockInit ( m.view );
        return true;
    }

    //  Opener: a descriptor received from the creator. Refused unless it is sealed against
    //  resizing and exactly the expected size: otherwise the peer could shrink it under our
    //  mapping and turn our reads into SIGBUS.
    inline bool
    MappingOpen ( Mapping& m, int fd )
    {
        m.fd = fd;
        const int seals = fcntl ( fd, F_GET_SEALS );
        if ( seals < 0 || (seals & (F_SEAL_SHRINK | F_SEAL_GROW)) != (F_SEAL_SHRINK | F_SEAL_GROW) )
          return false;
        const off_t size = lseek ( fd, 0, SEEK_END );
        if ( size != (off_t)kBlockBytes )
          return false;
        void* pv = mmap ( nullptr, kBlockBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        if ( pv == MAP_FAILED ) return false;
        m.view = static_cast<Block*> ( pv );
        if ( !BlockValid ( m.view ) ) { munmap ( pv, kBlockBytes ); m.view = nullptr; return false; }
        return true;
    }

    inline void
    MappingClose ( Mapping& m )
    {
        if ( m.view ) munmap ( m.view, kBlockBytes );
        if ( m.fd >= 0 ) close ( m.fd );
        m.view = nullptr; m.fd = -1;
    }

    //  Descriptors over an AF_UNIX stream socket (SCM_RIGHTS), riding on a payload of
    //  n bytes (1..256) sent in the same message.
    inline bool
    SendFds ( int sock, const int* fds, int count, const void* payload, size_t n )
    {
        if ( count <= 0 || count > 8 || !payload || n == 0 || n > 256 ) return false;
        iovec iov { const_cast<void*> ( payload ), n };
        alignas(cmsghdr) char ctl[CMSG_SPACE ( sizeof ( int ) * 8 )];
        std::memset ( ctl, 0, sizeof ( ctl ) );
        msghdr msg {};
        msg.msg_iov = &iov; msg.msg_iovlen = 1;
        msg.msg_control = ctl; msg.msg_controllen = CMSG_SPACE ( sizeof ( int ) * count );
        cmsghdr* c = CMSG_FIRSTHDR ( &msg );
        c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN ( sizeof ( int ) * count );
        std::memcpy ( CMSG_DATA ( c ), fds, sizeof ( int ) * count );
        ssize_t r;
        do r = sendmsg ( sock, &msg, MSG_NOSIGNAL ); while ( r < 0 && errno == EINTR );
        return r == (ssize_t)n;
    }

    //  Exactly count descriptors and exactly n payload bytes within timeoutMs, or nothing
    //  (any descriptors received are closed on a mismatch). The payload is short and sent in
    //  one message, so a stream socket delivers it whole with its descriptors.
    inline bool
    RecvFds ( int sock, int* fds, int count, void* payload, size_t n, int timeoutMs )
    {
        if ( count <= 0 || count > 8 || !payload || n == 0 || n > 256 ) return false;
        pollfd pfd { sock, POLLIN, 0 };
        int pr;
        do pr = poll ( &pfd, 1, timeoutMs ); while ( pr < 0 && errno == EINTR );
        if ( pr != 1 || !(pfd.revents & POLLIN) ) return false;
        iovec iov { payload, n };
        alignas(cmsghdr) char ctl[CMSG_SPACE ( sizeof ( int ) * 8 )];
        msghdr msg {};
        msg.msg_iov = &iov; msg.msg_iovlen = 1;
        msg.msg_control = ctl; msg.msg_controllen = sizeof ( ctl );
        ssize_t r;
        do r = recvmsg ( sock, &msg, MSG_CMSG_CLOEXEC | MSG_DONTWAIT ); while ( r < 0 && errno == EINTR );
        if ( r != (ssize_t)n )
        {
          for ( cmsghdr* c = CMSG_FIRSTHDR ( &msg ); r >= 0 && c; c = CMSG_NXTHDR ( &msg, c ) )
            if ( c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS )
            {
              const int k = (int)((c->cmsg_len - CMSG_LEN ( 0 )) / sizeof ( int ));
              int tmp[8];
              std::memcpy ( tmp, CMSG_DATA ( c ), sizeof ( int ) * (k < 8 ? k : 8) );
              for ( int i = 0; i < k && i < 8; ++i ) close ( tmp[i] );
            }
          return false;
        }
        int got = 0;
        for ( cmsghdr* c = CMSG_FIRSTHDR ( &msg ); c; c = CMSG_NXTHDR ( &msg, c ) )
        {
          if ( c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS ) continue;
          const int k = (int)((c->cmsg_len - CMSG_LEN ( 0 )) / sizeof ( int ));
          int tmp[8];
          std::memcpy ( tmp, CMSG_DATA ( c ), sizeof ( int ) * (k < 8 ? k : 8) );
          for ( int i = 0; i < k && i < 8; ++i )
          {
            if ( got < count ) fds[got++] = tmp[i];
            else               close ( tmp[i] );
          }
        }
        if ( got != count || (msg.msg_flags & MSG_CTRUNC) )
        { for ( int i = 0; i < got; ++i ) close ( fds[i] ); return false; }
        return true;
    }
#endif

    // ------------------------------------------------------------------------------------
    //  Wake: a per-side object the OTHER process signals, delivered as a completion on this
    //  side's pump port with (key, ov). Arm() is one-shot: re-arm after each delivery.
    // ------------------------------------------------------------------------------------
#if defined(_WIN32)
    using NTSTATUS_T = LONG;
    using PFN_NtCreateWaitCompletionPacket = NTSTATUS_T ( NTAPI* ) ( PHANDLE, ACCESS_MASK, PVOID );
    using PFN_NtAssociateWaitCompletionPacket =
        NTSTATUS_T ( NTAPI* ) ( HANDLE, HANDLE, HANDLE, PVOID, PVOID, NTSTATUS_T, ULONG_PTR, PBOOLEAN );
    using PFN_NtCancelWaitCompletionPacket = NTSTATUS_T ( NTAPI* ) ( HANDLE, BOOLEAN );

    struct NtWaitApi
    {
        PFN_NtCreateWaitCompletionPacket    create    = nullptr;
        PFN_NtAssociateWaitCompletionPacket associate = nullptr;
        PFN_NtCancelWaitCompletionPacket    cancel    = nullptr;
    };

    //  Resolved once. All three or none.
    inline const NtWaitApi&
    NtWait ( )
    {
        static const NtWaitApi api = [] {
            NtWaitApi a;
            HMODULE h = GetModuleHandleW ( L"ntdll.dll" );
            if ( h )
            {
              a.create    = reinterpret_cast<PFN_NtCreateWaitCompletionPacket>(
                              reinterpret_cast<void*> ( GetProcAddress ( h, "NtCreateWaitCompletionPacket" ) ) );
              a.associate = reinterpret_cast<PFN_NtAssociateWaitCompletionPacket>(
                              reinterpret_cast<void*> ( GetProcAddress ( h, "NtAssociateWaitCompletionPacket" ) ) );
              a.cancel    = reinterpret_cast<PFN_NtCancelWaitCompletionPacket>(
                              reinterpret_cast<void*> ( GetProcAddress ( h, "NtCancelWaitCompletionPacket" ) ) );
            }
            if ( !a.create || !a.associate || !a.cancel ) a = NtWaitApi ( );
            return a;
        } ( );
        return api;
    }

    inline bool WakeSupported ( ) { return NtWait ( ).create != nullptr; }

    struct Wake { HANDLE hEvent = nullptr; HANDLE hPacket = nullptr; };

    //  The waiting side creates its own event (auto-reset) and duplicates it to the signaller.
    inline bool
    WakeCreate ( Wake& w )
    {
        if ( !WakeSupported ( ) ) return false;
        w.hEvent = CreateEventW ( nullptr, FALSE, FALSE, nullptr );
        if ( !w.hEvent ) return false;
        if ( NtWait ( ).create ( &w.hPacket, GENERIC_ALL, nullptr ) < 0 )
        { CloseHandle ( w.hEvent ); w.hEvent = nullptr; w.hPacket = nullptr; return false; }
        return true;
    }

    //  Waiting side, when the PEER created this side's event and duplicated it in (the
    //  accepter makes both events, so handles only ever travel accepter -> dialler).
    //  Takes ownership of hEvent.
    inline bool
    WakeAttach ( Wake& w, HANDLE hEvent )
    {
        if ( !WakeSupported ( ) || !hEvent ) return false;
        w.hEvent = hEvent;
        if ( NtWait ( ).create ( &w.hPacket, GENERIC_ALL, nullptr ) < 0 )
        { w.hPacket = nullptr; return false; }
        return true;
    }

    //  Signaller side: hEvent is the peer's event as duplicated into this process.
    inline void WakeSignal ( HANDLE hPeerEvent ) { SetEvent ( hPeerEvent ); }

    //  Queue (key, ov) on port when the event is signalled. The packet is ALWAYS queued,
    //  so the caller always takes it from the port: firedNow is false on Windows.
    //  MEASURED 2026-11-24 (perf_shm, Windows 10.0.26300): when the event was already
    //  signalled, AlreadySignaled came back TRUE *and* the packet was queued at once, 6 of 6.
    //  Treating TRUE as "nothing queued" left that packet in the port, so the next
    //  association of the same wait packet failed: random ping-pong breaks in 7 of 7 rounds.
    inline bool
    WakeArm ( Wake& w, HANDLE port, ULONG_PTR key, LPOVERLAPPED ov, bool& firedNow )
    {
        BOOLEAN already = FALSE;
        firedNow = false;
        return NtWait ( ).associate ( w.hPacket, port, w.hEvent, reinterpret_cast<PVOID> ( key ),
                                      ov, 0, 0, &already ) >= 0;
    }

    //  The delivery needs nothing more on Windows (the wait reset the auto-reset event).
    inline void WakeConsume ( Wake& ) { }

    //  Withdraw an armed wait. TRUE: withdrawn, NO completion will arrive for it, so the
    //  caller releases its OVERLAPPED itself. FALSE: a completion may still arrive.
    //  With RemoveSignaledPacket = TRUE a packet already queued is taken back too, so on
    //  an armed wait whose completion has not been processed this is TRUE on success.
    inline bool
    WakeCancel ( Wake& w )
    {
        if ( !w.hPacket ) return false;
        return NtWait ( ).cancel ( w.hPacket, TRUE ) == 0;   // STATUS_SUCCESS
    }

    inline void
    WakeClose ( Wake& w )
    {
        if ( w.hPacket ) { NtWait ( ).cancel ( w.hPacket, TRUE ); CloseHandle ( w.hPacket ); }
        if ( w.hEvent )  CloseHandle ( w.hEvent );
        w.hPacket = nullptr; w.hEvent = nullptr;
    }
#else
    inline bool WakeSupported ( ) { return &p2p_iocp_poll != nullptr; }

    struct Wake { int fd = -1; HANDLE h = nullptr; bool bound = false; };

    inline bool
    WakeCreate ( Wake& w )
    {
        w.fd = eventfd ( 0, EFD_NONBLOCK | EFD_CLOEXEC );
        if ( w.fd < 0 ) return false;
        w.h = p2p_handle_new ( HKind::Fd, w.fd, nullptr );
        return w.h != nullptr;
    }

    //  Waiting side, when the peer created this side's eventfd and passed it in. Takes
    //  ownership of fd.
    inline bool
    WakeAttach ( Wake& w, int fd )
    {
        if ( fd < 0 ) return false;
        w.fd = fd;
        w.h = p2p_handle_new ( HKind::Fd, w.fd, nullptr );
        return w.h != nullptr;
    }

    //  The raw eventfd, for the creating side to signal or pass on.
    inline int WakeFd ( const Wake& w ) { return w.fd; }

    //  Signaller side: fd is the peer's eventfd as received over SCM_RIGHTS.
    inline void
    WakeSignal ( int peerFd )
    {
        const uint64_t one = 1;
        ssize_t r;
        do r = write ( peerFd, &one, sizeof ( one ) ); while ( r < 0 && errno == EINTR );
        //  EAGAIN = the counter is saturated: a wake is already pending, which is the point.
    }

    //  One-shot POLLIN completion on port. The first arm associates the fd with (port, key).
    inline bool
    WakeArm ( Wake& w, HANDLE port, ULONG_PTR key, LPOVERLAPPED ov, bool& firedNow )
    {
        firedNow = false;
        if ( !WakeSupported ( ) ) return false;
        if ( !w.bound )
        {
          if ( !CreateIoCompletionPort ( w.h, port, key, 0 ) ) return false;
          w.bound = true;
        }
        if ( p2p_iocp_poll ( w.fd, ov ) ) return true;
        return GetLastError ( ) == ERROR_IO_PENDING;
    }

    //  Linux: an armed POLLIN is withdrawn by closing the descriptor (WakeClose), which
    //  the shim reports as an aborted completion - so one ALWAYS arrives: FALSE.
    inline bool WakeCancel ( Wake& ) { return false; }

    //  After a delivery: drain the counter so the next POLLIN waits for a new signal.
    inline void
    WakeConsume ( Wake& w )
    {
        uint64_t v;
        ssize_t r;
        do r = read ( w.fd, &v, sizeof ( v ) ); while ( r < 0 && errno == EINTR );
    }

    inline void
    WakeClose ( Wake& w )
    {
        if ( w.h ) CloseHandle ( w.h );      // closes the fd it wraps
        else if ( w.fd >= 0 ) close ( w.fd );
        w.h = nullptr; w.fd = -1; w.bound = false;
    }
#endif
} // namespace p2pshm
