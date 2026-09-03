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
//  Platform layer — narrow MFC shim.
//
//  Part of the Msgcore + TargetCore Linux port (see the Linux port plan §4.3).
//
//  The two libraries use MFC narrowly: CObject + DECLARE/IMPLEMENT_DYNCREATE (on
//  P2PeerConPlc), CList/CMap/CListP2PeerMsg, CString, ASSERT/VERIFY/AfxCheckMemory/
//  DEBUG_NEW. On _WIN32 real MFC is used and this shim is skipped, so both DLLs stay
//  MFC extension DLLs (no consumer disruption). On Linux the shim supplies façades
//  over std:: containers and assert().
//
//  Phase status: Windows side = skip (real MFC). Linux side is a Phase-1 deliverable;
//  the CObject/CRuntimeClass and CList/CMap façades are declared here as scaffold and
//  fleshed out once the used member set is inventoried from the legacy call sites.
//
#pragma once

#if defined(_WIN32)
  // Real MFC is in force (pulled by stdafx.h). Nothing to shim.
#else
  #include <cassert>
  #include <list>
  #include <vector>
  #include <unordered_map>
  #include <ctime>
  #include <mutex>          // CMutex -> std::recursive_mutex
  #include "p2pstr.h"

  //  ASSERT / VERIFY / memory macros (§4.3).
  #define ASSERT(expr)          assert(expr)
  #define VERIFY(expr)          ((void)(expr))
  #define ASSERT_VALID(p)       ((void)0)
  #define AfxCheckMemory()      (true)
  #define TRACE(...)            ((void)0)

  //  MFC calling-convention / linkage macros used on the message-map declarations
  //  (P2Pwin32.h Find*/Dispatch* protos, P2PmsgMaps.h map tables). AFXAPI is __stdcall
  //  (no-op off-Windows); AFX_COMDAT is __declspec(selectany) — a def that may appear in
  //  multiple TUs and must dedup, so map it to GCC's weak (the selectany equivalent).
  //  Undefined, AFXAPI derails every `RET AFXAPI Func(...)` proto into "expected
  //  initializer", which then suppresses the file-scope `class P2PeerTarget;` fwd-decls
  //  later in P2PmsgMaps.h — one macro clears that whole cascade.
  #ifndef AFXAPI
  #define AFXAPI
  #endif
  #ifndef AFX_CDECL
  #define AFX_CDECL
  #endif
  #ifndef AFX_COMDAT
  #define AFX_COMDAT            __attribute__((weak))
  #endif
  #ifndef AFX_MANAGE_STATE
  #define AFX_MANAGE_STATE(p)   ((void)0)
  #endif

  //  Misc MFC helpers named in the compiled set. UNUSED(x) marks a parameter used only
  //  in some builds; AfxIsValidAddress is a debug memory probe (always "valid" here, as
  //  IsBadWritePtr); CN_COMMAND is an MFC control-notification code (unused live on Linux).
  #ifndef UNUSED
  #define UNUSED(x)             ((void)(x))
  #endif
  #ifndef UNUSED_ALWAYS
  #define UNUSED_ALWAYS(x)      ((void)(x))
  #endif
  inline BOOL AfxIsValidAddress(const void* p, unsigned long, BOOL = TRUE) { return p != nullptr; }
  #ifndef CN_COMMAND
  #define CN_COMMAND 0
  #endif

  //  Neutralize the legacy `#define new DEBUG_NEW` sites (both the `#ifndef NO_DEBUG_NEW`
  //  guarded ones and any unguarded ones) without editing every TU: defining NO_DEBUG_NEW
  //  skips the guarded blocks, and defining DEBUG_NEW as `new` makes an unguarded
  //  `#define new DEBUG_NEW` expand new -> DEBUG_NEW -> new (self-limiting, harmless).
  #ifndef NO_DEBUG_NEW
  #define NO_DEBUG_NEW
  #endif
  #ifndef DEBUG_NEW
  #define DEBUG_NEW new
  #endif

  //  CObject + minimal CRuntimeClass for DYNCREATE factory creation (§4.3).
  //  Keep the macro spelling identical to the legacy code.
  struct CRuntimeClass;      // Phase 1: minimal runtime-class record
  class  CObject {           // Phase 1: virtual dtor + Serialize hook subset
  public:
      virtual ~CObject() = default;
  };
  #define DECLARE_DYNCREATE(cls)   /* Phase 1 */
  #define IMPLEMENT_DYNCREATE(cls, base) /* Phase 1 */

  //  MSVC `__super::Method()` extension (GCC has no equivalent). The only live use in
  //  the compiled set is `__super::AssertValid()`, a debug-only MFC validation hook.
  //  Rather than hard-code each class's base, route __super:: to a dummy carrying the
  //  called methods as no-ops — compiles regardless of the real hierarchy, and the
  //  (debug-only) validation is simply skipped on Linux. Add methods here if new
  //  __super:: targets surface.
  struct P2P__super {
      static void AssertValid() {}
      static void Reset() {}
      static void Nullify() {}
      template <class... A> static void Print(A&&...) {}   // e.g. Print(FILE*, indent, depth)
  };
  #define __super P2P__super

  //  MFC CException base. Caught as `CException*`, may yield a message, and Delete()s
  //  itself (MFC exceptions are heap-owned).
  class CException {
  public:
      virtual ~CException() = default;
      virtual BOOL GetErrorMessage(LPTSTR buf, UINT n, PUINT /*pHelp*/ = nullptr) const {
          if (buf && n) buf[0] = 0; return TRUE; }
      void Delete() { delete this; }
  };

  //  MFC CTime subset. Ctor from a 64-bit epoch-seconds value (__time64_t), plus the
  //  six field getters the TIME64 formatting path calls. Uses localtime_r (POSIX).
  class CTime {
      std::tm m_tm{};
  public:
      CTime() = default;
      CTime(__int64 secs) { std::time_t t = (std::time_t)secs; localtime_r(&t, &m_tm); }
      int GetYear()   const { return m_tm.tm_year + 1900; }
      int GetMonth()  const { return m_tm.tm_mon + 1; }
      int GetDay()    const { return m_tm.tm_mday; }
      int GetHour()   const { return m_tm.tm_hour; }
      int GetMinute() const { return m_tm.tm_min; }
      int GetSecond() const { return m_tm.tm_sec; }
  };

  //  MFC COleDateTime subset (TargetCoreLog timestamps). Only the local-time field
  //  getters + the static GetCurrentTime() the logging path uses; a std::tm backs it,
  //  same as CTime.
  class COleDateTime {
      std::tm m_tm{};
  public:
      COleDateTime() = default;
      static COleDateTime GetCurrentTime() {
          COleDateTime d; std::time_t t = std::time(nullptr); localtime_r(&t, &d.m_tm); return d; }
      int GetYear()   const { return m_tm.tm_year + 1900; }
      int GetMonth()  const { return m_tm.tm_mon + 1; }
      int GetDay()    const { return m_tm.tm_mday; }
      int GetHour()   const { return m_tm.tm_hour; }
      int GetMinute() const { return m_tm.tm_min; }
      int GetSecond() const { return m_tm.tm_sec; }
  };

  //  MFC CMutex / CSingleLock (the AfxMt sync pair). Win32 CMutex is recursive, so a
  //  std::recursive_mutex backs it (Risk #4). CSingleLock is the scoped guard the log
  //  TU uses; ctor(mutex, bInitiallyLock) + Lock()/Unlock()/IsLocked().
  class CMutex {
      std::recursive_mutex m_m;
  public:
      CMutex(BOOL = FALSE, const wchar_t* = nullptr, void* = nullptr) {}
      BOOL Lock()      { m_m.lock();   return TRUE; }
      BOOL Lock(DWORD) { m_m.lock();   return TRUE; }
      BOOL Unlock()    { m_m.unlock(); return TRUE; }
  };
  class CSingleLock {
      CMutex* m_p; bool m_locked = false;
  public:
      explicit CSingleLock(CMutex* p, BOOL bInitialLock = FALSE) : m_p(p) { if (bInitialLock) Lock(); }
      ~CSingleLock() { if (m_locked) Unlock(); }
      BOOL Lock()          { if (m_p) m_p->Lock();          m_locked = true;  return TRUE; }
      BOOL Unlock()        { if (m_p && m_locked) m_p->Unlock(); m_locked = false; return TRUE; }
      BOOL IsLocked() const { return m_locked ? TRUE : FALSE; }
  };

  //  Minimal CWinApp — the compiled set reads only m_nThreadID (P2PeerContextSwap via
  //  AfxGetApp()). A single process-wide instance suffices; m_nThreadID is the id of the
  //  first caller (the main/UI thread on Windows).
  class CWinApp {
  public:
      DWORD          m_nThreadID  = 0;
      const wchar_t* m_pszAppName = L"p2pmsg";
  };
  inline CWinApp* AfxGetApp() {
      static CWinApp app;
      if (app.m_nThreadID == 0) app.m_nThreadID = GetCurrentThreadId();
      return &app;
  }

  //  CList<T> façade over an intrusive doubly-linked list (the Linux port plan §4.3),
  //  implementing the MFC POSITION-cursor idiom the compiled set uses. POSITION is a
  //  node pointer (opaque); a null POSITION means "past the end" — matching MFC, where
  //  GetNext returns the current element and advances the cursor. Member set
  //  inventoried from the call sites (GetNext/GetHeadPosition/AddTail/RemoveAt/...).
  //
  //  NB: a linked list (not a vector) is required for correctness. MFC's contract is
  //  that removing one node leaves every OTHER POSITION valid, which the teardown
  //  loops rely on (save posCur, GetNext to advance, RemoveAt(posCur)). A vector
  //  backing shifted indices on erase, so an already-advanced cursor over-ran the
  //  shrunken storage (std::vector::operator[] OOB) once a list held >1 element and
  //  removed during iteration — e.g. CloseP2PmsgHub draining m_oCListP2PmsgCon.
  template <class T, class ARG = const T&>
  class CList {
      struct Node { T val; Node* prev; Node* next; };
      Node*       m_head = nullptr;
      Node*       m_tail = nullptr;
      std::size_t m_n    = 0;
      static POSITION mk(Node* n)    { return reinterpret_cast<POSITION>(n); }
      static Node*    nd(POSITION p) { return reinterpret_cast<Node*>(p); }
      void copyFrom(const CList& o)  { for (Node* n = o.m_head; n; n = n->next) AddTail(n->val); }
  public:
      CList() = default;
      CList(const CList& o)            { copyFrom(o); }             // deep copy (nodes, not pointees)
      CList& operator=(const CList& o) { if (this != &o) { RemoveAll(); copyFrom(o); } return *this; }
      ~CList()                         { RemoveAll(); }

      int  GetCount() const { return (int)m_n; }
      int  GetSize()  const { return (int)m_n; }
      bool IsEmpty()  const { return m_n == 0; }
      void RemoveAll()      { Node* n = m_head; while (n) { Node* x = n->next; delete n; n = x; }
                              m_head = m_tail = nullptr; m_n = 0; }

      POSITION GetHeadPosition() const { return mk(m_head); }
      POSITION GetTailPosition() const { return mk(m_tail); }

      //  FindIndex(n): the POSITION of the n-th element, or NULL when n is out of range.
      //  MFC's contract, and O(n) there too - it walks. Added when P2Pwin32.cpp's hub-teardown
      //  loop started re-deriving its cursor from an index each pass (the fix for holding a
      //  POSITION across Destroy()/Drop()), which is a call site the shim's original inventory
      //  predates. Found by building the current TargetCore on Linux for the PHP hub note's
      //  P6 MSCS leg; nothing else in the compiled set uses it yet.
      POSITION FindIndex(INT_PTR nIndex) const {
          if (nIndex < 0 || (std::size_t)nIndex >= m_n) return nullptr;
          Node* n = m_head;
          for (INT_PTR i = 0; i < nIndex && n; ++i) n = n->next;
          return mk(n);
      }

      T& GetNext(POSITION& p) { Node* n = nd(p); p = mk(n->next); return n->val; }
      T& GetPrev(POSITION& p) { Node* n = nd(p); p = mk(n->prev); return n->val; }
      T& GetHead() { return m_head->val; }
      T& GetTail() { return m_tail->val; }
      //  GetAt(pos): element at the cursor WITHOUT advancing (MFC).
      T& GetAt(POSITION p)             { return nd(p)->val; }
      const T& GetAt(POSITION p) const { return nd(p)->val; }

      POSITION AddTail(ARG x) { Node* n = new Node{ x, m_tail, nullptr };
          if (m_tail) m_tail->next = n; else m_head = n; m_tail = n; ++m_n; return mk(n); }
      POSITION AddHead(ARG x) { Node* n = new Node{ x, nullptr, m_head };
          if (m_head) m_head->prev = n; else m_tail = n; m_head = n; ++m_n; return mk(n); }
      //  MFC RemoveHead/RemoveTail RETURN the removed element (callers assign it).
      T RemoveHead() { Node* n = m_head; T v = n->val; m_head = n->next;
          if (m_head) m_head->prev = nullptr; else m_tail = nullptr; delete n; --m_n; return v; }
      T RemoveTail() { Node* n = m_tail; T v = n->val; m_tail = n->prev;
          if (m_tail) m_tail->next = nullptr; else m_head = nullptr; delete n; --m_n; return v; }
      void RemoveAt(POSITION p) { Node* n = nd(p);
          if (n->prev) n->prev->next = n->next; else m_head = n->next;
          if (n->next) n->next->prev = n->prev; else m_tail = n->prev;
          delete n; --m_n; }
      POSITION InsertAfter(POSITION p, ARG x) { if (!p) return AddHead(x);
          Node* a = nd(p);
          Node* n = new Node{ x, a, a->next };
          if (a->next) a->next->prev = n; else m_tail = n; a->next = n; ++m_n; return mk(n); }
  };

  //  CMap<K,AK,V,AV> façade over std::unordered_map (hub/thread maps, trigger/alloc
  //  registries). Lookup/SetAt/RemoveKey/GetCount plus the MFC POSITION iteration
  //  (GetStartPosition/GetNextAssoc). An insertion-order key vector backs iteration;
  //  POSITION encodes (index+1) as an opaque pointer, mirroring the CList idiom.
  template <class K, class AK, class V, class AV>
  class CMap {
      std::unordered_map<K, V> m_m;
      std::vector<K>           m_order;
      static POSITION mk(std::size_t i) { return reinterpret_cast<POSITION>(i); }
  public:
      int  GetCount() const { return (int)m_m.size(); }
      bool IsEmpty()  const { return m_m.empty(); }
      void RemoveAll()      { m_m.clear(); m_order.clear(); }
      void SetAt(AK key, AV val) {
          if (m_m.find(key) == m_m.end()) m_order.push_back(key); m_m[key] = val; }
      bool Lookup(AK key, V& out) const {
          auto it = m_m.find(key); if (it == m_m.end()) return false; out = it->second; return true; }
      bool RemoveKey(AK key) {
          auto n = m_m.erase(key);
          if (n) { auto it = std::find(m_order.begin(), m_order.end(), key);
                   if (it != m_order.end()) m_order.erase(it); }
          return n != 0; }
      POSITION GetStartPosition() const { return m_order.empty() ? nullptr : mk(1); }
      void GetNextAssoc(POSITION& p, K& key, V& val) const {
          std::size_t i = reinterpret_cast<std::size_t>(p) - 1;
          key = m_order[i]; val = m_m.at(m_order[i]);
          p = (i + 1 < m_order.size()) ? mk(i + 2) : nullptr; }
  };

#endif // _WIN32
