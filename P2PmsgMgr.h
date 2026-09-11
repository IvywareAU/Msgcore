// Copyright © 2007-2010, 2018, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//
//  P2PmsgMgr message definitions and prototypes
//
//
//  High-level message manager and persistence controller for the P2P/P3P
//  messaging framework.
//  NOTES: P2PmsgMgr encapsulates a VBHeap-backed message store containing
//         a complete hierarchy of P3PmsgItem objects (fields, lists,
//         vectors, attributes, and descendants). It provides lifecycle
//         management, persistence, paging, trigger notification, and object
//         addressing services.
//       : The manager presents a unified interface for constructing, accessing,
//         storing, and monitoring message object graphs.
//
//  Architecture Overview
//       : P2PmsgMgr
//         Root manager and entry point
//       : VBHeap
//         Underlying memory allocator (via P2PmsgHeap)
//       : VBLock
//         Packed memory representation of all objects
//       : P3PmsgItem  
//         Logical object model layered over VBLock structures
//  The manager owns and coordinates:
//       : Heap allocation and attachment
//       : Object graph construction and navigation
//       : File-backed persistence and shared access
//       : Paging and caching of large datasets
//       : Trigger-based event notification
//
//  Key Responsibilities
//       : Creation, loading, saving, and renaming of message stores
//       : Attachment to and management of VBHeap memory regions
//       : Translation between positional references (P2Pos) and objects
//       : Path-based object resolution and navigation
//       : Dirty state tracking and optional heap defragmentation
//
//  Paging & Caching
//       : Supports on-demand paging of message subtrees (PageDatasetIn/Out)
//       : Callback-based population and eviction mechanisms
//       : Page summary flags track cache state (dirty, cached, flushed, etc.)
//       : Designed for large datasets exceeding in-memory capacity
//
//  Trigger System
//       : Event notification for INSERT, UPDATE, DELETE operations
//       : Windows message-based delivery (WM_APP derived)
//       : Registration and filtering by object position (P2Pos)
//       : Manual and automatic trigger activation support
//
//  Addressing Model
//       : P2Pos provides a stable positional reference into the object graph
//       : Conversion utilities map positions to fields, attributes, objects, and paths
//       : Root and relative path resolution supported
//
//  Concurrency & Safety
//       : Internal critical section for manager-level synchronisation
//       : Safe registration push/pop helpers for paging callbacks
//       : Validation and debugging utilities for heap and object integrity
//
//  Specialisations
//       : P2PmsgMgrnn templates provide fixed addressing modes (16/32/64-bit)
//       : Safe pointer wrappers and container helpers included
//
//  SUMMARY
//       : This is the top-level control layer of the P2P messaging architecture
//       : Strong coupling with VBHeap, VBLock, and P3Pmsg object model
//       : Designed for high-performance, low-copy, persistent message handling
//       : Suitable for IPC, storage, and large structured data manipulation
//
#pragma once
#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif

#include "wtypes.h"
#include "P2Pmsg.h"
#include "MsgAttr.h"
//  P3PmsgDesc must be COMPLETE here: P2Pos2Path takes the address of
//  r_Desc() for P3Pmsg_GetPath. It arrived transitively through MsgCurs.h
//  and now says so.
#include "MsgDesc.h"
#include "MsgList.h"
#include "MsgVect.h"
#include "MsgCurs.h"
#include "MsgCollectors.h"

#define  UINT08 UCHAR

///////////////////////////////////////////////////////////////////////
//  P2PmsgMgr trigger containers and definitions
//  NOTES: Used to manage P2Pmsg trigger registration and notification
//       : Trigger registration is based on P2Pos references to target items
//

const DWORD P2PmsgTrig_INSERT = (1<<0);
const DWORD P2PmsgTrig_UPDATE = (1<<1);
const DWORD P2PmsgTrig_DELETE = (1<<2);
const DWORD P2PmsgTrig_ALL    = (P2PmsgTrig_INSERT|P2PmsgTrig_UPDATE|P2PmsgTrig_DELETE);
//
//  Page summary definitions
//  NOTES: Used to manage P3PmsgItem caching. The summary is a bit field that tracks
//         the state of the item in relation to the disk cache.  The summary is used
//         to determine the appropriate action when a page in or out request is made,
//         and to manage the transition between dirty, cached, and flushed states.
//       : The summary flags are defined as follows:
const DWORD P2Pmsg_PAGESUMM_DIRTY   = (1<<0);    // Dirty copy, to be cached
const DWORD P2Pmsg_PAGESUMM_CACHED  = (1<<1);    // Cached from disk
const DWORD P2Pmsg_PAGESUMM_FLUSH   = (1<<2);    // Flushed out to disk
const DWORD P2Pmsg_PAGESUMM_NOMERGE = (1<<3);    // Merge cached and flushed data
const DWORD P2Pmsg_PAGESUMM_DECACHE = (1<<4);    // De-cache into flushed state
//
//  P2PmsgMgr paging callback definitions (default set)
//  NOTES: Used to manage P2Pmsg tree pageing notifications
//       : Call backs will be performed upon request and may or may not
//         be to P2PmsgMgr derived objects
//       : Remember to cancel the callbacks prior to allowing
//         the implementation code to go out of scope
//       : The callback function signatures are defined as follows:
typedef BOOL (CALLBACK *P2PageinCBFnc)
         ( PINT_PTR   nP2PageCBKey,    // P2PmsgMgr defined callback key
           P2Pos    posItem );         // Item to be paged IN
typedef BOOL (CALLBACK *P2PageoutCBFnc)
         ( PINT_PTR   nP2PageCBKey,    // P2PmsgMgr defined callback key
           P2Pos    posItem,           // Item to be paged OUT
           BOOL       bFlush );        // Flushed of disk flag
typedef BOOL (CALLBACK *P2PopulateCBFnc)
         ( PINT_PTR   nP2PageCBKey,    // P2PmsgMgr defined callback key
           P2Pos    posItem,           // Item to be populated
           BOOL       bSpare );        // Spare flag
//
//  P2PmsgTrigger notification messages
//  NOTES: Definition sequence is designed to trap use of the
//         message number elsewhere
//       : Default set, re-define within P2PmsgMgr to resolve conflicts
//       : The windows message numbers are defined as follows:
const DWORD WM_APP_0x00a1        = WM_APP + 0x00a1;
const DWORD WM_P2Pmsg_TrigUPDATE = (WM_APP_0x00a1);
const DWORD WM_APP_0x00a2        = WM_APP + 0x00a2;
const DWORD WM_P2Pmsg_TrigINSERT = WM_APP_0x00a2;
const DWORD WM_APP_0x00a3        = WM_APP + 0x00a3;
const DWORD WM_P2Pmsg_TrigDELETE = WM_APP_0x00a3;
const DWORD WM_APP_0x00a4        = WM_APP + 0x00a4;
const DWORD WM_P2Pmsg_TrigACTIVE = WM_APP_0x00a4;
const DWORD WM_APP_0x00a5        = WM_APP + 0x00a5;
const DWORD WM_P2Pmsg_Properties = WM_APP_0x00a5;

///////////////////////////////////////////////////////////////////////
//  P2PmsgMgr class
//  NOTES: Manages chunk of memory holding a self contained contiguous
//         sequence of P3Pmsg<Field, Vect, List, ...> objects held
//         beneath a transparent P3PmsgItem container
class Msgcore_EXT P2PmsgMgr : public P3PmsgItem
{
      void
        RenderThisSafe ( );

    // Constructors and destructor
    public:
        P2PmsgMgr ( );

        P2PmsgMgr ( UCHAR uAddrNN, UINT nSizeInitial, UINT nSizeMax );

        P2PmsgMgr ( const P2PmsgMgr& rhs );

        P2PmsgMgr ( LPCTSTR lpszFilename );

      virtual
       ~P2PmsgMgr ( );

    // Initialisation and serialisation
    public:
      P2PmsgMgr&
        Attacheap ( LPCTNAM lpszName, const P3PmsgData& oData );
      BOOL
        Load ( LPCTSTR lpszFilename );
      BOOL
        Save ( LPCTSTR lpszFilename = 0, bool bDefragment = false );
      BOOL
        SharedMode ( DWORD dwSharedMode );
      void
        Nullify ( );
      BOOL
        Rename ( LPCTSTR lpszNewname );

    // Factories
    public:
      static P2PmsgMgr*
        Factory ( LPCTSTR lpszFilename
                , UCHAR uAddrNN, UINT nSizeInitial, UINT nSizeMax );
      static P2PmsgMgr*
        Factory ( LPCTSTR lpszFilename, DWORD dwSharedMode = 0 );

    // Paging
    public:
      virtual BOOL
        PageRegistration ( PINT_PTR m_nfnP2PageItemCB
                         , P2PageinCBFnc pP2PageinCBFnc, P2PageoutCBFnc pP2PageoutCBFnc ) noexcept;
      virtual BOOL
        PageRegistration ( PINT_PTR m_nfnP2PopulateItemCB
                         , P2PopulateCBFnc pP2PopulateCBFnc ) noexcept;
      virtual BOOL
        PageDatasetIn ( P2Pos posItem );
      virtual BOOL
        PageDatasetOut ( P2Pos posItem, BOOL bFlush = FALSE );
      virtual DWORD
        PageSumm ( P3PmsgItem& oItem, DWORD dwAdditions = 0, DWORD dwRemovals = 0u );
      virtual BOOL
        PageRegistrationPush ( );
      virtual BOOL
        PageRegistrationPop ( );

    // Operators
    public:
      P2PmsgMgr&
        operator  = ( const P2PmsgMgr& rhs );

    // Addressing
    public:
      P3PmsgField
        P2Pos2Field ( P2Pos pos );
      P3PmsgAttr
        P2Pos2Attr  ( P2Pos pos );
      CString
        P2Pos2Path  ( P2Pos pos );
      P3PmsgObject
        P2Pos2Object( P2Pos pos, BOOL bPageIn = FALSE );
      P3PmsgObject
        Path2Object ( LPCTSTR lpszObjectPath );
      P3PmsgObject
        RootPath2Object ( LPCWSTR lpszObjectPath );

    // Triggers
    // NOTES: External manual activation
    public:
      UINT // Upgraded implementation
        CreateTrigger ( UINT nTrigger_TypeMask
                      , HWND hWnd, P2Pos posP2Pobject );
      UINT // Upgraded implementation
        DropTriggers ( UINT nTrigger_TypeMask, HWND hWnd, P2Pos posP2Pobject = 0 );
      DWORD
        SelectTrigger ( P2Pos posP2Pobject, HWND hWnd );
      UINT
        TriggerINSERT ( P3PmsgObject& oObjectParent );
      UINT
        TriggerINSERT ( const P3PmsgItem& oItemParent );
      UINT
        TriggerUPDATE ( P3PmsgItem& oItem );
      UINT
        TriggerDELETE ( P3PmsgItem& oItem );
      // Headless notification: install a function-pointer sink (pfn == nullptr
      // clears it) fired alongside the HWND path for every armed node, and fire
      // a trigger by P2Pos without a P3PmsgItem in hand (the FUSE/daemon change
      // path). See P2PmsgTriggerSink in Msgcore.h.
      void
        SetTriggerSink ( P2PmsgTriggerSink pfn, void* pUser );
      UINT
        FireTrigger ( P2Pos posP2Pobject, UINT nTrigger_TypeMask );

    // Troubleshooting
    public:
      virtual BOOL
        IsValid ( ) const;
      virtual void
        AssertValid ( ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1  );

    // Properties
    public:
      LPCTSTR
        GetFilename ( );
      LPCTSTR
        GetRootname ( );
      virtual bool
        IsDirty ( );
      virtual BOOL
        SetDirty ( BOOL bDirty );
      bool
        IsField ( P2Pos posP2Pobject );
      BOOL
        IsAttributed ( UCHAR ucAttribute, P2Pos posP2Pobject );
      VBLsize
        Sizeof ( );

    // Attributes
    protected:
      P2PmsgHANDLE     m_hMgr{NULL};
      HANDLE           m_hFile{INVALID_HANDLE_VALUE};
      CString          m_strFilename;
      GUID             m_oGUID;
      // Default share mode allows concurrent readers of a saved store. Save
      // holds the file open for the manager's lifetime; FILE_SHARE_READ lets
      // other processes/handles open it for reading (e.g. to Load or inspect it)
      // while it is held. It still blocks concurrent writers. Override via
      // SharedMode() (e.g. 0 for exclusive access).
      DWORD            m_dwSharedMode{FILE_SHARE_READ};
    public:
      CRITICAL_SECTION m_oCSectionMgr;
      UINT             m_uiWM_APP_TrigINSERT;
      UINT             m_uiWM_APP_TrigUPDATE;
      UINT             m_uiWM_APP_TrigDELETE;
      P2PageinCBFnc    m_pfncP2PageinCB{nullptr}, m_pfncP2PageinCBp{nullptr};
      P2PageoutCBFnc   m_pfncP2PageoutCB{nullptr}, m_pfncP2PageoutCBp{nullptr};
      P2PopulateCBFnc  m_pfncP2PopulateCB{nullptr}, m_pfncP2PopulateCBp{nullptr};
      PINT_PTR         m_nfncP2PageCBKey{0}, m_nfncP2PageCBKeyp{0};
      PINT_PTR         m_nfncP2PopulateCBKey{0},m_nfncP2PopulateCBKeyp{0};
};
typedef P2PSafePtr<P2PmsgMgr> P2PmsgMgrSP;
typedef CList<P2PmsgMgr*> CListP2PmsgMgr;

///////////////////////////////////////////////////////////////////////
//  P2PmsgMgrnn targeted addressing 
//  NOTES: Usage P2PmsgMgr32 oMgr32; etc
template<UCHAR uBSTRnn>
class P2PmsgMgrnn : public P2PmsgMgr
{
    public:
        // nSizeofMax 0 == no explicit ceiling, the same convention the
        // filename and copy constructors use when they call
        // P2PmsgHeap_CreateBSTRio. This used to name a two-argument
        // P2PmsgMgr that does not exist, so "P2PmsgMgr32 oMgr32;" -- the
        // usage the comment above advertises -- failed to compile. Being a
        // template, it went unnoticed: nothing ever instantiated it.
        P2PmsgMgrnn ( ) : P2PmsgMgr ( uBSTRnn, 2024, 0 ) {};
        // P2PmsgMgr sizes its heap in UINT; VBLsize is pointer-wide, so the
        // narrowing is explicit here rather than a C4244 in every consumer.
        P2PmsgMgrnn ( VBLsize nSizeof, VBLsize nSizeofMax )
                    : P2PmsgMgr ( uBSTRnn, (UINT)nSizeof, (UINT)nSizeofMax ) {};
      virtual
       ~P2PmsgMgrnn ( ) {};
};
typedef P2PmsgMgrnn<VBLock_Addr16> P2PmsgMgr16;
typedef P2PmsgMgrnn<VBLock_Addr32> P2PmsgMgr32;
typedef P2PmsgMgrnn<VBLock_Addr64> P2PmsgMgr64;

///////////////////////////////////////
//  Safe DSet Paging
//  NOTES: Manages the lifecycle of paged Data Sets within a P2PmsgMgr.
//         The paging is managed by the P2PmsgMgr and the SafeDSetPaging
//         class provides a convenient RAII-style wrapper to ensure that
//         the paging is properly cached and flushed.
//       : Only relevant for large datasets that are paged in and out of memory.
//         For small datasets, the paging is not necessary and the SafeDSetPaging
//         class is not applicable.
class Msgcore_EXT SafeDSetPaging
{
    public:
      SafeDSetPaging ( P2PmsgMgr& oP2PmsgMgr, P3PmsgItem& oDSetItem );
     ~SafeDSetPaging ();
    P3PmsgItem*
      operator -> ( ) noexcept;
    P3PmsgItem*
      operator = ( P3PmsgItem *pDSetItem );
    P3PmsgItem*
      Dereference ( ) noexcept;
    // Attributes
    private:
      P2PmsgMgr   *m_pP2PmsgMgr{nullptr};
      P3PmsgItem  *m_pDSetItem{nullptr};
      DWORD        m_eDSetPageSumm{0};
};

///////////////////////////////////////////////////////////////////////
//  Safe P2PmsgMgr paging registration push and subsequent pop
class Msgcore_EXT SafeRegistrationPush
{
    public:
      SafeRegistrationPush ( P2PmsgMgr *pP2PmsgMgr );
     ~SafeRegistrationPush ( );
    private:
      P2PmsgMgr *m_pP2PmsgMgr{nullptr};
};

///////////////////////////////////////////////////////////////////////
//  P2PmsgMgr helpers
//  NOTES: Standard extensions and activities

Msgcore_EXT BOOL
P2PmsgMgr_IsValid ( LPCTSTR lpszFilename ) noexcept;
Msgcore_EXT BOOL
P2PmsgMgr_IsValid ( P2PmsgMgr *pP2PmsgMgr ) noexcept;
Msgcore_EXT BOOL
P2PmsgMgr_Swap ( P2PmsgMgr& oP2PmsgMgr
               , P3PmsgItem& oSwapItem1, P3PmsgItem& oSwapItem2 );
Msgcore_EXT BOOL
P2PmsgMgr_Sort ( P2PmsgMgr& oP2PmsgMgr, P3PmsgItem& oSortItem );
