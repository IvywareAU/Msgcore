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
//  Platform layer — threads, critical sections, events.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §5.3, §6.1/6.2).
//
//  _WIN32 : pass-through (CreateThread/CRITICAL_SECTION/CreateEvent from windows.h).
//  Linux  : CRITICAL_SECTION -> std::recursive_mutex (Win32 CS is RECURSIVE — a plain
//           std::mutex would deadlock, the Linux port plan §6.1 / Risk #4); CreateThread ->
//           std::jthread behind a P2PHandle{Thread}; CreateEvent/SetEvent -> the ring's
//           eventfd (§5.4). Implemented in Phase 1 (CS/events) and Phase 3 (thread spawn).
//
#pragma once
#include "p2ptypes.h"

#if defined(_WIN32)
  #include <windows.h>   // CRITICAL_SECTION, CreateThread, CreateEvent, ...
#else
  #include <mutex>
  #include <cstdint>
  #include <cerrno>
  #include <unistd.h>
  #include <poll.h>
  #include <sched.h>          // sched_yield (SwitchToThread)
  #include <sys/eventfd.h>
  #include <thread>           // CreateThread -> std::thread
  #include <atomic>
  #include <memory>

  //  Thread entry-point type (CreateThread / the Proc* pump procs, §6.2). WINAPI is empty
  //  on Linux; the jthread-backed CreateThread lands in Phase 3, but the *type* is named in
  //  signatures now. SwitchToThread -> sched_yield (cooperative yield, same contract).
  using LPTHREAD_START_ROUTINE = DWORD (*)(LPVOID);
  inline BOOL SwitchToThread() { return ::sched_yield() == 0 ? TRUE : FALSE; }

  //  CRITICAL_SECTION -> std::recursive_mutex (Win32 CS is RECURSIVE — a plain
  //  std::mutex would deadlock on re-entrant EnterCriticalSection, the Linux port plan
  //  §6.1 / Risk #4). Header-only (inline) so the libraries link without a platform
  //  .cpp. CreateThread -> std::jthread behind a P2PHandle{Thread} (Phase 3).
  struct CRITICAL_SECTION { void *impl = nullptr; };   // impl = std::recursive_mutex*

  inline void InitializeCriticalSection(CRITICAL_SECTION *cs) {
      cs->impl = new std::recursive_mutex(); }
  inline void InitializeCriticalSectionAndSpinCount(CRITICAL_SECTION *cs, DWORD) {
      cs->impl = new std::recursive_mutex(); }
  inline void DeleteCriticalSection(CRITICAL_SECTION *cs) {
      delete static_cast<std::recursive_mutex*>(cs->impl); cs->impl = nullptr; }
  inline void EnterCriticalSection(CRITICAL_SECTION *cs) {
      static_cast<std::recursive_mutex*>(cs->impl)->lock(); }
  inline void LeaveCriticalSection(CRITICAL_SECTION *cs) {
      static_cast<std::recursive_mutex*>(cs->impl)->unlock(); }
  inline BOOL TryEnterCriticalSection(CRITICAL_SECTION *cs) {
      return static_cast<std::recursive_mutex*>(cs->impl)->try_lock() ? TRUE : FALSE; }

  // -------------------------------------------------------------------------
  //  Events (the Linux port plan §5.4). An event HANDLE is a P2PHandle{Event} whose
  //  fd is an eventfd:
  //    - manual-reset : stays signalled (counter > 0) until ResetEvent drains it;
  //                     a successful wait does NOT consume it.
  //    - auto-reset   : a successful wait consumes the signal (reads the eventfd),
  //                     matching Win32's "release one waiter, then reset".
  //  A plain (non-semaphore) eventfd matches Win32 semantics best: SetEvent on an
  //  already-signalled event is idempotent (write may EAGAIN at the u64 ceiling —
  //  treated as success), and a single read clears the whole count.
  //
  //  NOTE (§5.4): the pump's queue-event and the ring's own eventfd are the SAME
  //  descriptor once P2PmsgPump is wired (Phase 3) — SetEvent(m_hQueEvent) then wakes
  //  a GetQueuedCompletionStatus idle-wait through the ring. This standalone event is
  //  the general primitive; the pump-specific folding happens at that call site.
  //  impl holds a P2PEventImpl: the manual-reset flag, plus the generation
  //  counter that carries the HAPPENS-BEFORE EDGE described next.
  //
  //  WHY A WAIT PUBLISHES A GENERATION, measured under TSan on 2026-09-17.
  //  poll() reporting a descriptor readable is a WAKE-UP, not a memory-ordering
  //  edge. The auto-reset path gets its edge for free - it read()s the eventfd,
  //  and a read paired with the signaller's write orders the two threads (it is
  //  what the kernel's eventfd context lock actually does, and it is what TSan
  //  models). The MANUAL-RESET path must not consume the signal, so it only
  //  ever polled, and published nothing: a waiter had NO ordering against the
  //  thread that signalled it, and everything that thread did before SetEvent
  //  was formally unordered against everything the waiter did after waking.
  //
  //  Not theoretical. P2PeerHub::CloseHub() waits on exactly such an event for
  //  its spawned pump thread to leave, and ~P2PeerHub then frees the hub - so
  //  the destructor's CloseHandle, its `delete m_pAuthPolicy` and its
  //  DeleteCriticalSection all ran unordered against ProcHub's epilogue. TSan
  //  reported eleven races that way across p2p_e2ewaive, p2p_authrelay and
  //  p2p_hubsnap, and named what was holding it together: "As if synchronized
  //  via sleep" - the Sleep(1) in YieldForP2PmsgPump. A sleep is not a
  //  synchronisation primitive. It is a bet on the other thread being quick,
  //  and this counter is what cancels the bet.
  // -------------------------------------------------------------------------
  //  Bumped with RELEASE after the eventfd write, so a waiter's ACQUIRE load of
  //  a non-zero value orders it after that write as well as after everything
  //  the signaller did before it. Zero means "never signalled"; an event
  //  created already signalled starts at one, because for that one there is no
  //  SetEvent to do the bumping.
  //
  //  MONOTONIC, and ResetEvent deliberately does NOT clear it. It is an
  //  ordering carrier, not the signal state - the eventfd counter is the signal
  //  state. Clearing it would let a waiter that has already polled readable
  //  spin for ever on a generation that went backwards.
  struct P2PEventImpl {
      bool                       manual;
      std::atomic<std::uint64_t> gen;
      P2PEventImpl(bool m, bool signalled) : manual(m), gen(signalled ? 1u : 0u) {}
  };

  //  The acquire half. Called once poll() has said the descriptor is readable,
  //  so the write that made it readable has already happened and the release
  //  that follows it is a few instructions away at most: this spins, it does
  //  not block, and on every signal after the first it is one load.
  inline void p2p_event_acquire(void* impl) {
      auto* ev = static_cast<P2PEventImpl*>(impl);
      if (!ev) return;
      while (ev->gen.load(std::memory_order_acquire) == 0u)
          std::this_thread::yield();
  }

  #ifndef INFINITE
    #define INFINITE 0xFFFFFFFFu
  #endif
  #ifndef WAIT_OBJECT_0
    #define WAIT_OBJECT_0 0x00000000u
  #endif
  #ifndef WAIT_FAILED
    #define WAIT_FAILED   0xFFFFFFFFu
  #endif
  //  WAIT_TIMEOUT (258) is defined in p2ptypes.h.

  inline HANDLE CreateEventW(void* /*sec*/, BOOL manualReset, BOOL initialState,
                             const wchar_t* /*name*/) {
      int efd = ::eventfd(initialState ? 1u : 0u, EFD_CLOEXEC | EFD_NONBLOCK);
      if (efd < 0) { SetLastError(win32_from_errno(errno)); return nullptr; }
      return p2p_handle_new(HKind::Event, efd,
                            new P2PEventImpl(manualReset != FALSE, initialState != FALSE));
  }
  inline HANDLE CreateEventA(void* sec, BOOL m, BOOL i, const char* /*name*/) {
      return CreateEventW(sec, m, i, nullptr); }
  #define CreateEvent CreateEventW

  inline BOOL SetEvent(HANDLE h) {
      if (!h || h->kind != HKind::Event) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      std::uint64_t one = 1;
      ssize_t r = ::write(h->fd, &one, sizeof one);
      //  EAGAIN => counter already at the u64 ceiling, i.e. already signalled: success.
      if (r < 0 && errno != EAGAIN) { SetLastError(win32_from_errno(errno)); return FALSE; }
      //  The release half, and it goes AFTER the write on purpose: a waiter that
      //  acquires this value is then ordered after the write too, which is the
      //  one that CloseHandle on the other thread would otherwise race.
      if (h->impl)
        static_cast<P2PEventImpl*>(h->impl)->gen.fetch_add(1u, std::memory_order_release);
      return TRUE;
  }

  inline BOOL ResetEvent(HANDLE h) {
      if (!h || h->kind != HKind::Event) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      std::uint64_t sink;
      ssize_t r = ::read(h->fd, &sink, sizeof sink);   // drain; EAGAIN if already clear
      (void)r;
      return TRUE;
  }

  // -------------------------------------------------------------------------
  //  Threads (the Linux port plan §6.2). CreateThread -> std::thread behind a
  //  P2PHandle{Thread}; the Win32 thread-proc contract (DWORD proc(LPVOID)) maps
  //  directly. A shared control block carries the exit code + a done flag so
  //  GetExitCodeThread still works after the thread ends, and CloseHandle can detach
  //  a still-running thread without a use-after-free (the running lambda keeps its own
  //  ref to the control block).
  // -------------------------------------------------------------------------
  #ifndef STILL_ACTIVE
    #define STILL_ACTIVE 259
  #endif

  struct P2PThreadCtl  { std::atomic<bool> done{false}; DWORD exit_code = 0; };
  struct P2PThreadImpl { std::thread th; std::shared_ptr<P2PThreadCtl> ctl; };

  inline HANDLE CreateThread(void* /*sec*/, SIZE_T /*stack*/, LPTHREAD_START_ROUTINE start,
                             LPVOID param, DWORD /*flags*/, LPDWORD threadId) {
      //  Win32 writes the id synchronously before the thread runs; some call sites reuse
      //  that same DWORD for a pump id the proc overwrites, so set it BEFORE spawning.
      //  The id MUST be the value GetCurrentThreadId() reports inside the child (the pump
      //  registries + P2PeerHub::RunHub's assert depend on it), so mint from the shared
      //  p2p_next_tid() counter and seed the child's thread_local id first thing.
      DWORD tid = p2p_next_tid();
      if (threadId) *threadId = tid;
      auto* ti = new P2PThreadImpl();
      ti->ctl = std::make_shared<P2PThreadCtl>();
      auto ctl = ti->ctl;                                  // copy for the lambda (outlives ti)
      ti->th = std::thread([start, param, ctl, tid]() {
          t_p2p_tid = tid;                                 // GetCurrentThreadId() == tid here
          DWORD rc = start ? start(param) : 0u;
          ctl->exit_code = rc;
          ctl->done.store(true, std::memory_order_release);
      });
      return p2p_handle_new(HKind::Thread, -1, ti);
  }

  inline BOOL GetExitCodeThread(HANDLE h, LPDWORD code) {
      if (!h || h->kind != HKind::Thread || !h->impl) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
      auto* ti = static_cast<P2PThreadImpl*>(h->impl);
      if (code) *code = ti->ctl->done.load(std::memory_order_acquire) ? ti->ctl->exit_code
                                                                      : (DWORD)STILL_ACTIVE;
      return TRUE;
  }

  //  Join (if finished) or detach (still running) + free the impl. Called from
  //  CloseHandle (p2pfile.h) for a Thread handle; CloseHandle deletes the P2PHandle.
  inline void p2p_thread_close(HANDLE h) {
      if (!h || h->kind != HKind::Thread || !h->impl) return;
      auto* ti = static_cast<P2PThreadImpl*>(h->impl);
      if (ti->th.joinable()) {
          if (ti->ctl->done.load(std::memory_order_acquire)) ti->th.join();
          else                                               ti->th.detach();
      }
      delete ti; h->impl = nullptr;
  }

  //  WaitForSingleObject dispatches on kind: Event -> eventfd poll (§5.4);
  //  Thread -> join (INFINITE) or bounded done-flag poll.
  inline DWORD WaitForSingleObject(HANDLE h, DWORD dwMilliseconds) {
      if (!h) { SetLastError(ERROR_INVALID_HANDLE); return WAIT_FAILED; }
      if (h->kind == HKind::Thread && h->impl) {
          auto* ti = static_cast<P2PThreadImpl*>(h->impl);
          if (dwMilliseconds == INFINITE) { if (ti->th.joinable()) ti->th.join(); return WAIT_OBJECT_0; }
          for (DWORD waited = 0; ; waited += 5) {
              if (ti->ctl->done.load(std::memory_order_acquire)) {
                  if (ti->th.joinable()) ti->th.join();
                  return WAIT_OBJECT_0;
              }
              if (waited >= dwMilliseconds) return WAIT_TIMEOUT;
              ::usleep(5000);
          }
      }
      if (h->kind == HKind::Event) {
          struct pollfd pfd{ h->fd, POLLIN, 0 };
          int timeout = (dwMilliseconds == INFINITE) ? -1 : (int)dwMilliseconds;
          for (;;) {
              int rc = ::poll(&pfd, 1, timeout);
              if (rc < 0) { if (errno == EINTR) continue;
                            SetLastError(win32_from_errno(errno)); return WAIT_FAILED; }
              if (rc == 0) return WAIT_TIMEOUT;
              bool manual = h->impl && static_cast<P2PEventImpl*>(h->impl)->manual;
              if (!manual) { std::uint64_t sink; ssize_t r = ::read(h->fd, &sink, sizeof sink); (void)r; }
              //  Take the signaller's writes with us. The auto-reset read above
              //  is already such an edge; this is the manual-reset path's, and
              //  running it for both costs one load and keeps them honest.
              p2p_event_acquire(h->impl);
              return WAIT_OBJECT_0;
          }
      }
      SetLastError(ERROR_INVALID_HANDLE);
      return WAIT_FAILED;
  }

  #ifndef MAXIMUM_WAIT_OBJECTS
    #define MAXIMUM_WAIT_OBJECTS 64
  #endif

  //  WaitForMultipleObjects: the array form of WaitForSingleObject over Event/Thread
  //  handles (§5.4). bWaitAll TRUE waits for every object; FALSE returns when the first
  //  is signalled (WAIT_OBJECT_0 + its index). An auto-reset event is consumed only once
  //  the wait is actually satisfied — for bWaitAll that means only after ALL are ready,
  //  matching Win32's all-or-nothing reset. Bounded by dwMilliseconds (INFINITE blocks).
  //  Implemented as a 5 ms poll loop, which is adequate for the shim's callers.
  inline DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles,
                                      BOOL bWaitAll, DWORD dwMilliseconds) {
      if (!pHandles || nCount == 0 || nCount > MAXIMUM_WAIT_OBJECTS) {
          SetLastError(ERROR_INVALID_PARAMETER); return WAIT_FAILED;
      }
      auto ready = [](HANDLE h) -> int {   // 1 = signalled, 0 = not, -1 = error
          if (!h) return -1;
          if (h->kind == HKind::Thread) {
              auto* ti = static_cast<P2PThreadImpl*>(h->impl);
              return (ti && ti->ctl->done.load(std::memory_order_acquire)) ? 1 : 0;
          }
          if (h->kind == HKind::Event) {
              struct pollfd pfd{ h->fd, POLLIN, 0 };
              int rc = ::poll(&pfd, 1, 0);
              if (rc < 0) return errno == EINTR ? 0 : -1;
              return (rc > 0 && (pfd.revents & POLLIN)) ? 1 : 0;
          }
          return -1;
      };
      auto consume = [](HANDLE h) {   // drain an auto-reset event / join a done thread
          if (h->kind == HKind::Event) {
              bool manual = h->impl && static_cast<P2PEventImpl*>(h->impl)->manual;
              if (!manual) { std::uint64_t s; ssize_t r = ::read(h->fd, &s, sizeof s); (void)r; }
              p2p_event_acquire(h->impl);   // the same edge WaitForSingleObject takes
          } else if (h->kind == HKind::Thread) {
              auto* ti = static_cast<P2PThreadImpl*>(h->impl);
              if (ti && ti->th.joinable()) ti->th.join();
          }
      };
      for (DWORD waited = 0; ; waited += 5) {
          if (bWaitAll) {
              bool all = true;
              for (DWORD i = 0; i < nCount; ++i) {
                  int r = ready(pHandles[i]);
                  if (r < 0) { SetLastError(ERROR_INVALID_HANDLE); return WAIT_FAILED; }
                  if (r == 0) { all = false; break; }
              }
              if (all) { for (DWORD i = 0; i < nCount; ++i) consume(pHandles[i]); return WAIT_OBJECT_0; }
          } else {
              for (DWORD i = 0; i < nCount; ++i) {
                  int r = ready(pHandles[i]);
                  if (r < 0) { SetLastError(ERROR_INVALID_HANDLE); return WAIT_FAILED; }
                  if (r == 1) { consume(pHandles[i]); return WAIT_OBJECT_0 + i; }
              }
          }
          if (dwMilliseconds != INFINITE && waited >= dwMilliseconds) return WAIT_TIMEOUT;
          ::usleep(5000);
      }
  }
#endif
