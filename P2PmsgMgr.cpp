// Copyright © 2005-2010, 2022, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2PmsgMgr definitions and prototypes
//
#include "stdafx.h"
#include "P2PmsgMgr.h"
#include "Msgexception.h"
#include "MsgVBHeap.h"
#include "Kernel32_Ext.h"
#include <ASSERT.h>
#include "Tchar.h"

#define objVBList m_oObject.m_hVBList
#define objVBLock m_oObject.m_uVBLock

///////////////////////////////////////////////////////////////////////
//  Generic P2Pmsg manager

//  Constructors and destructor
P2PmsgMgr::P2PmsgMgr ( )
         : P3PmsgItem ( 0, 0, 0 )
{
    RenderThisSafe ( );
    m_hMgr = P2PmsgHeap_CreateBSTRio ( VBLock_Addr32, 2024, 0 );
    Attacheap ( L"P2PmsgMgr name", P3PmsgData() );
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
}

P2PmsgMgr::P2PmsgMgr ( UCHAR uAddrNN, UINT nSizeInitial, UINT nSizeMax )
         : P3PmsgItem ( 0, 0, 0 )
{
    RenderThisSafe ( );
    m_hMgr = P2PmsgHeap_CreateBSTRio ( uAddrNN, nSizeInitial, nSizeMax );
    Attacheap ( L"", P3PmsgData() );
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
}

P2PmsgMgr::P2PmsgMgr ( const P2PmsgMgr& rhs )
         : P3PmsgItem ( 0, 0, 0 )
{
    RenderThisSafe ( );
    m_hMgr = P2PmsgHeap_CreateBSTRio ( P2PmsgHeap_Addrnn(rhs.m_hMgr), 2024, 0 ); //TODO:LJM +2024 is a fudge
    Attacheap ( rhs.c_name(), ((P2PmsgMgr&)rhs).r_data() );
  (*this) = rhs;
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
}

P2PmsgMgr::P2PmsgMgr ( LPCTSTR lpszFilename )
         : P3PmsgItem ( 0, 0, 0 )
{
    RenderThisSafe ( );
    m_hMgr = P2PmsgHeap_CreateBSTRio ( VBLock_Addr32, 2024, 0 );
    Attacheap ( L"", P3PmsgData() );
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
    Load ( lpszFilename );
}

P2PmsgMgr::~P2PmsgMgr ( )
{
    if ( m_hMgr )
      P2PmsgHeap_Close ( m_hMgr );
    m_hMgr = 0;
    if ( m_hFile != INVALID_HANDLE_VALUE )
      CloseHandle ( m_hFile );
    m_hFile = INVALID_HANDLE_VALUE;
    DeleteCriticalSection ( &m_oCSectionMgr );
}

void
P2PmsgMgr::RenderThisSafe ( )
{
    //m_hMgr  = 0;
    //m_hFile = INVALID_HANDLE_VALUE;
    m_uiWM_APP_TrigINSERT = WM_P2Pmsg_TrigINSERT;
    m_uiWM_APP_TrigUPDATE = WM_P2Pmsg_TrigUPDATE;
    m_uiWM_APP_TrigDELETE = WM_P2Pmsg_TrigDELETE;

    // Resources
    CoCreateGuid ( &m_oGUID );
    InitializeCriticalSection ( &m_oCSectionMgr );
}

void
P2PmsgMgr::Nullify ( )
{
    if ( m_hMgr )
      P2PmsgHeap_Close ( m_hMgr );
    m_hMgr = 0;
    if ( m_hFile != INVALID_HANDLE_VALUE )
      CloseHandle ( m_hFile );
    m_hFile = INVALID_HANDLE_VALUE;
  __super::Nullify ( );
}

//
//  Attaches heap to this P2PmsgMgr
//
//  Parameters:  LPCTNAME lpszMsgName
//               Name allocated to heap
//
//               const P3PmsgData& oData
//               Top level heap data
//
//  Returns:     P2PmsgMgr&
//               Reference to this P2PmsgMgr
P2PmsgMgr&
P2PmsgMgr::Attacheap ( LPCTNAM lpszMsgName, const P3PmsgData& oData )
{
    // Environmental
    // NOTES: Create a dummy root item for sizing purposes, then allocate
    //        a chunk directly from the P2PmsgHeap since its yet to be attached
    //        to the P2PmsgMgr
    ASSERT(oData.VerifyContainment());
    UCHAR      uVBLaddrnn  = P2PmsgHeap_Addrnn ( m_hMgr );
    P3PmsgItem oRootItem ( lpszMsgName, oData );     // Dummy root for P2PmsgMgr
    VBLsize    nSizeofRoot = P2PmsgField_SizeofItem ( uVBLaddrnn, oRootItem, TRUE );

    VBLaddr     aVBLock = P2PmsgHeap_Alloc ( m_hMgr, VBLock_Item, nSizeofRoot );   //Displaces above line
    VBLock     *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_hMgr, aVBLock );
                pVBLock -> oHdr.uVBLockDefs |= VBLock_Linked;                  //Added later by LJM

    VBLockItem_Init ( uVBLaddrnn, VBLock_pItem(pVBLock), VBLock_Field );    // Added later by LJM
    VBLockField *pField = VBLock_pField ( pVBLock );
    VBLockField_Init ( pField, 0xFF ); //TODO: LJM node to field cutover 0xFF );
    VBLockName  *pName  = VBLockField_pName ( pField );
    VBLsize      nName_Sizeof = VBLockName_Sizeof ( uVBLaddrnn, P2PmsgObject_pName( oRootItem.r_Object() ) );
                 nName_Sizeof = max ( nName_Sizeof, VBLockName_Sizeof_Min(uVBLaddrnn) );
    VBLockName_Init  ( uVBLaddrnn, pName
                     , VBLockAttr_DEFAULT | VBLockAttr_NULL
                     , lpszMsgName, nName_Sizeof );
    VBLockData  *pData  = VBLockField_pData ( uVBLaddrnn, pField );
    VBLsize      nData_Sizeof = VBLockData_Sizeof ( uVBLaddrnn, P2PmsgObject_pData( oRootItem.r_Object() ) );
                 nData_Sizeof = max ( nData_Sizeof, VBLockData_Sizeof_Min(uVBLaddrnn) );
    VBLockData_Init  ( VBLockField_pData(m_oObject.m_aVBLock,pField)
                     , VBLockAttr_DEFAULT, oData.DataType(), nData_Sizeof );
    Connect ( m_hMgr, aVBLock, VBLock_Hdr_u_SizeNN(pVBLock) );
    r_data() = oData;
ASSERT(VBLock_IsLinked(pVBLock));
ASSERT(VBLock_IsAlloc(pVBLock));
ASSERT(VBLock_IsItem(pVBLock));
    return *this;
}

//
//  Load and attach an existing P2P message heap from disk
//  NOTES: Existing manager/file state is released before loading.
//       : Entire file is read into memory, then type-dispatched
//         based on heap header identification.
//       : Supports BSTRio and IOMAGE heap formats.
//       : Files > 4GB are intentionally rejected.
//
BOOL
P2PmsgMgr::Load ( LPCTSTR lpszFilename )
{
    BOOL  bResult      = FALSE;
    char *pP2PmsgHeap  = nullptr;
    try
    {
        //
        // Release existing file handle/state
        if ( m_hFile != INVALID_HANDLE_VALUE )
          CloseHandle ( m_hFile );
        m_hFile = INVALID_HANDLE_VALUE;
        m_strFilename.Empty();
        //
        // Open the existing file READ-ONLY. Load reads the whole file into
        // memory (below) and the heap is an in-memory copy, so no write access
        // is needed. Sharing READ|WRITE means the open never blocks other
        // readers/writers and can even read a store that a live saver still
        // holds open.
        P2PsafeHANDLE shFile
          =  CreateFile ( lpszFilename
                        , GENERIC_READ
                        , FILE_SHARE_READ | FILE_SHARE_WRITE
                        , nullptr
                        , OPEN_EXISTING
                        , FILE_ATTRIBUTE_NORMAL, nullptr );
        if ( shFile == INVALID_HANDLE_VALUE )
          EVERR->MODULE
               ->Message(L"CreateFile(%s) failed", lpszFilename)
               ->HResult(0)
               ->Throw();
        //
        // Validate file size
        // NOTES: Reject files > 4GB and obviously invalid files
        DWORD dwFileSizeHi = 0;
        DWORD dwFileSizeLo = GetFileSize(shFile, &dwFileSizeHi);
        if ( dwFileSizeHi ||
             dwFileSizeLo < sizeof(VBListBSTRio) )
          EVERR->MODULE
               ->AFP(lpszFilename)
               ->Message(L"Invalid file size")
               ->HResult(0)
               ->Throw();
        //
        // Load entire file into memory
        pP2PmsgHeap = new char[dwFileSizeLo];
        DWORD dwBytesRead = 0;
        bResult = ReadFile ( shFile
                           , pP2PmsgHeap
                           , dwFileSizeLo
                           ,&dwBytesRead, nullptr );
        if ( !bResult || dwBytesRead != dwFileSizeLo )
          EVERR->MODULE
               ->Message(L"ReadFile(%s) failed", lpszFilename)
               ->HResult(0)
               ->Throw();
        //
        // Determine heap format and connect manager
        Nullify ( );
        if ( P2PmsgHeap_IsBSTRio(pP2PmsgHeap) )
        {
            // dwFileSizeLo is the real extent of the buffer above, and the
            // BSTRio branch went without it until finding F1. The IOMAGE branch
            // below has passed it since the C4 fix; this arm of the same
            // dispatch, reached by a file whose first byte is the BSTRio tag
            // rather than by anything the C4 test could produce, did not -- so
            // an offset read out of the file became a pointer with nothing in
            // between. Both arms now check the same thing against the same
            // number.
            m_hMgr = P2PmsgHeap_CreateBSTRio(
                        reinterpret_cast<VBListBSTRio*>(pP2PmsgHeap), dwFileSizeLo);
            pP2PmsgHeap = nullptr; // ownership transferred
            Connect ( m_hMgr, P2PmsgHeap_ConnectBSTRio(m_hMgr), 0 );
        }
        else if ( P2PmsgHeap_IsIOMAGE(pP2PmsgHeap) )
        {
            // THE STORE ACCEPTS A PRE-SENTINEL IMAGE AND THE WIRE DOES NOT,
            // and the asymmetry is the decision rather than an oversight
            // (TargetCore's versioning note, §6, byte_order.md §4.3).
            // A frame is a peer, and a peer can be upgraded; a file is
            // data somebody already has, and there is no conversation to
            // have with it.
            // It is still worth saying out loud. A generation-0 image makes no
            // statement about its own layout, so everything below parses it
            // under THIS build's rules on the strength of nothing - which is
            // exactly the silent misread the generation code exists to end.
            // F-S6-4 put WARNING in the default mask, which is what makes
            // saying so reach anybody.
            if ( P2PmsgHeap_IOMAGEform(pP2PmsgHeap) == VBLockSync_Legacy )
              EVWRN->MODULE
                   ->AFP(lpszFilename)
                   ->Message(L"Pre-sentinel message image (layout generation 0)")
                   ->Advice (L"Parsed under this build's layout, unverified")
                   ->Cancel  (true);   // display once, notify, and DELETE

            m_hMgr = P2PmsgHeap_CreateIOMAGE(
                        reinterpret_cast<VBListIOmage*>(pP2PmsgHeap), dwFileSizeLo);
            pP2PmsgHeap = nullptr; // ownership transferred
            VBLaddr aVBLock = sizeof(VBListIOmage::oSync);
            Connect( m_hMgr, P2PmsgHeap_Connect(m_hMgr), aVBLock );
        }
        else
            EVERR->MODULE
                 ->AFP(lpszFilename)
                 ->Message(L"Unknown or corrupted P2PmsgHeap")
                 ->Throw();
        //
        // Commit successful load. Do NOT retain the file handle: the heap is
        // an in-memory copy, so shFile closes here holding no lock. The
        // filename is remembered so a later Save() with no argument reopens the
        // file for writing (see Save). m_hFile stays INVALID_HANDLE_VALUE.
        m_strFilename = lpszFilename;
        return TRUE;
    }

    //
    // Exception handlers
    //
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast
    delete[] pP2PmsgHeap;
    return FALSE;
}
/*BOOL
P2PmsgMgr::Load ( LPCTSTR lpszFilename )
{
    // Locals
    BOOL   bResult;
    char  *pP2PmsgHeap = nullptr;

    // Because this is problematic
    try
    {
      // Resource recovery
      if ( m_hFile != INVALID_HANDLE_VALUE )
        CloseHandle ( m_hFile );
      m_hFile = INVALID_HANDLE_VALUE;
      m_strFilename.Empty();

      // Open existing disk file
      P2PsafeHANDLE shFile = CreateFile ( lpszFilename
                                        , (GENERIC_READ | GENERIC_WRITE)
                                        , m_dwSharedMode // No shared access
                                        , 0 // No security attributes
                                        , OPEN_EXISTING
                                        , FILE_ATTRIBUTE_NORMAL 
                                        , 0 ); // No template
      if ( shFile == INVALID_HANDLE_VALUE )
        EVERR->MODULE
             ->Message( L"CreateFile(%s) failed", lpszFilename )
             ->HResult( 0 )->Throw();

      // Load file contents
      DWORD dwFileSizeHi = 0;
      DWORD dwFileSizeLo = GetFileSize ( shFile, &dwFileSizeHi );
      if ( dwFileSizeHi                        ||
           dwFileSizeLo < sizeof(VBListBSTRio)    )
        EVERR->MODULE->AFP(lpszFilename)
             ->Message( L"GetFileSize(%s) failed", lpszFilename )
             ->HResult( 0 )->Throw();
      pP2PmsgHeap = new char [dwFileSizeLo];
      DWORD dwBytesRead = 0;
      bResult = ReadFile( shFile, pP2PmsgHeap, dwFileSizeLo, &dwBytesRead, 0 );
      if ( !bResult || dwBytesRead != dwFileSizeLo )
        EVERR->MODULE
             ->Message( L"ReadFile(%s) data failed", lpszFilename )
             ->HResult( 0 )->Throw();

      // Application
      // TODO:LJM this can be tidied up and made more generic
      if ( P2PmsgHeap_IsBSTRio(pP2PmsgHeap) )
      {
        Nullify ( );
        m_hMgr = P2PmsgHeap_CreateBSTRio ( (VBListBSTRio *)pP2PmsgHeap );
        pP2PmsgHeap = nullptr;
        Connect ( m_hMgr, P2PmsgHeap_ConnectBSTRio(m_hMgr), 0 );
      }
      else if ( P2PmsgHeap_IsIOMAGE(pP2PmsgHeap) )
      {
        //VBListIOmage oIOmage;
        Nullify ( );
        m_hMgr = P2PmsgHeap_CreateIOMAGE ( (VBListIOmage *)pP2PmsgHeap );
        pP2PmsgHeap = 0;               // Locked in elsewhere now
        VBLaddr aVBLock = sizeof(VBListIOmage::oSync); //was sizeof(oIOmage.oSync);
        Connect ( m_hMgr, P2PmsgHeap_Connect(m_hMgr), aVBLock );
        //Connect ( m_hMgr, sizeof(oIOmage.oSync), aVBLock ); 
      }
      else
        EVERR->MODULE->AFP(lpszFilename)
             ->Message( L"Unknown or corrupted P2PmsgHeap" )
             ->Throw();
      m_strFilename = lpszFilename;
      m_hFile       = shFile.Dereference();
      return TRUE;
    }

    // Exceptions
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast

    // Tidy up, and
    delete [] pP2PmsgHeap;
    return FALSE;
}*/

BOOL
P2PmsgMgr::Save ( LPCTSTR lpszFilename, bool bDefragment )
{
    // Locals
    BOOL       bResult = FALSE;
    HANDLE     hFile   = INVALID_HANDLE_VALUE;

    // Problematic
    try
    {
      // Resolve the target file. An explicit filename always wins; otherwise
      // fall back to the remembered file (e.g. save-back after a read-only
      // Load). An atomic save needs a concrete target name to rename onto.
      CString strTarget = ( lpszFilename && _tcslen(lpszFilename) > 0 )
                        ? CString(lpszFilename) : m_strFilename;
      if ( strTarget.IsEmpty() )
        EVERR -> MODULE
              -> Message( L"Save() has no target filename" )
              -> Throw();

      // Release any handle we hold on the target so the rename can replace it.
      if ( m_hFile != INVALID_HANDLE_VALUE )
        CloseHandle ( m_hFile );
      m_hFile = INVALID_HANDLE_VALUE;

      // Defragmentation
      // NOTES: Can take considerable time with large data stores
      if ( bDefragment )
      {
        ASSERT(m_hMgr==r_Object().m_hVBList);
        P2PmsgMgr oMgr = *this;
      __super::Nullify ( );            // Only P3PmsgItem base class
        P2PmsgHeap_Close ( m_hMgr );   // Drops our current list
        m_hMgr = oMgr.m_hMgr;          // Copy defragmented handle
        P2PmsgHeap_AddRef( m_hMgr );   // Increment reference count
        Connect ( m_hMgr, P2PmsgHeap_Connect(m_hMgr), 0 );
        ASSERT(m_hMgr==r_Object().m_hVBList);
      }

      // Capture the heap image to persist.
      void    *vpIOmage     = P2PmsgHeap_pImage ( m_hMgr );
      VBLsize  dwIOmageSize = P2PmsgHeap_Sizeof ( m_hMgr );

      // Serialise concurrent writers to the same store (cross-process). Acquire
      // an exclusive lock file beside the target; if another writer holds it,
      // back off briefly and retry, then fail cleanly rather than racing. The
      // lock is released and its file removed (FILE_FLAG_DELETE_ON_CLOSE) when
      // shLock goes out of scope - including on any throw below.
      CString strLock = strTarget + L".lock";
      HANDLE  hLockRaw = INVALID_HANDLE_VALUE;
      for ( int i = 0; ; i++ )
      {
        hLockRaw = CreateFileW ( strLock, GENERIC_WRITE | DELETE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr );
        if ( hLockRaw != INVALID_HANDLE_VALUE )
          break;
        DWORD dwErr = GetLastError();
        if ( dwErr != ERROR_SHARING_VIOLATION || i >= 50 )   // ~1s of retries
          EVERR -> MODULE -> AFP((LPCTSTR)strTarget)
                -> Message( L"Save: could not acquire write lock (concurrent writer?)" )
                -> HResult( dwErr )
                -> Throw();
        Sleep ( 20 );
      }
      P2PsafeHANDLE shLock = hLockRaw;   // RAII: releases lock + deletes lock file

      // Atomic save: write the full image to a sibling temp file (same
      // directory => same volume), flush it, then replace the target with a
      // single rename. A crash before the rename leaves the existing store
      // intact; a crash after it leaves the fully-written new store. There is
      // no window in which the target is half-written.
      CString strTemp;
      strTemp.Format ( L"%s.%lu.tmp", (LPCTSTR)strTarget, GetCurrentThreadId() );

      hFile = CreateFileW ( strTemp, GENERIC_WRITE, 0, nullptr
                          , CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
      if ( hFile == INVALID_HANDLE_VALUE )
      {
        DWORD dwErr = GetLastError();
        DeleteFileW ( strTemp );
        EVERR -> MODULE -> AFP((LPCTSTR)strTemp)
              -> Message( L"CreateFile(temp) failed" )
              -> HResult( dwErr )
              -> Throw();
      }

      DWORD dwBytesWritten = 0;
      BOOL  bWrote = WriteFile ( hFile, vpIOmage, (DWORD)dwIOmageSize, &dwBytesWritten, 0 );
      if ( !bWrote || dwBytesWritten != (DWORD)dwIOmageSize )
      {
        DWORD dwErr = GetLastError();
        CloseHandle ( hFile ); hFile = INVALID_HANDLE_VALUE;
        DeleteFileW ( strTemp );
        EVERR -> MODULE -> AFP((LPCTSTR)strTarget)
              -> Message( L"WriteFile(temp) failed" )
              -> HResult( dwErr )
              -> Throw();
      }
      FlushFileBuffers ( hFile );        // durable before the rename
      CloseHandle ( hFile ); hFile = INVALID_HANDLE_VALUE;

      // Atomic replace on the same volume; WRITE_THROUGH flushes the rename.
      if ( !MoveFileExW ( strTemp, strTarget
                        , MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) )
      {
        DWORD dwErr = GetLastError();
        DeleteFileW ( strTemp );
        EVERR -> MODULE -> AFP((LPCTSTR)strTarget)
              -> Message( L"MoveFileEx() atomic replace failed" )
              -> HResult( dwErr )
              -> Throw();
      }

      m_strFilename = strTarget;         // remembered for save-back; no handle retained
      P2PmsgHeap_SetDirty ( m_hMgr, FALSE );
      bResult = TRUE;
    }

    // Exceptions
    catch_pP2Pevent_Cancel
    catch_pCException_Cancel
    catch_ALL_Cancel

    // Tidy up, and
    return bResult;
}

BOOL
P2PmsgMgr::SharedMode ( DWORD dwSharedMode )
{
    
    if ( dwSharedMode == m_dwSharedMode )
      return dwSharedMode;

    return dwSharedMode;
}

BOOL
P2PmsgMgr::Rename ( LPCTSTR lpszNewname )
{
    if ( m_strFilename.IsEmpty() || !lpszNewname )
      return FALSE;
    if ( !MoveFileEx ( m_strFilename, lpszNewname
                     , MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) )
      return FALSE;
    m_strFilename = lpszNewname;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
//  Factories
P2PmsgMgr*
P2PmsgMgr::Factory ( LPCTSTR lpszFilename
                   , UCHAR uAddrNN, UINT nSizeInitial, UINT nSizeMax )
{
    // Locals
    P2PmsgMgr     *pMgr = 0;
    P2PsafeHANDLE shFile;

    try
    {
      //
      shFile = CreateFile ( lpszFilename
                          , (GENERIC_READ | GENERIC_WRITE)
                          , 0 //pMgr->m_dwSharedMode // No shared access
                          , 0 // No security attributes
                          , CREATE_NEW
                          , FILE_ATTRIBUTE_NORMAL 
                          , 0 ); // No template
      HRESULT hr = GetLastError();     // Destroyed by subsequent action
      if ( shFile == INVALID_HANDLE_VALUE )
        EVERR->MODULE
             ->AFP(lpszFilename)->AFP(uAddrNN)->AFP(nSizeInitial)->AFP(nSizeMax)
             ->Message( L"CreateFile(%s) failed", lpszFilename )
             ->HResult( hr )
             ->Throw();

      // Instanciation
      pMgr = new P2PmsgMgr ( uAddrNN, nSizeInitial, nSizeMax );
      pMgr -> m_strFilename = lpszFilename;
      pMgr -> m_hFile       = shFile.Dereference();

      // Instantiated
      return pMgr;
    }

    // Exceptions
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast

    // Tidy up, and
    delete pMgr;
    return nullptr;
}

P2PmsgMgr*
P2PmsgMgr::Factory ( LPCTSTR lpszFilename, DWORD dwSharedMode )
{
    // Locals
    P2PmsgMgr *pMgr = 0;

    // Because this is problematic
    try
    {
      // Target
      pMgr = new P2PmsgMgr ( );
      pMgr -> m_dwSharedMode = dwSharedMode;
      if ( !pMgr->Load(lpszFilename) )
        ThrowP2Pevent();
      return pMgr;
    }

    // Exceptions
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast

    // Tidy up, and
    delete pMgr;
    return 0;
}

/////////////////////////////////////////////////
//  Paging control etc

//
//  Paging callback registrations
//
//  Parameters:  PINT_PTR nfnP2PageItemCB
//               User defined callback data.  Usually pointer to
//               originating class
//
//               P2PageinCBFnc *pP2PageinCBFnc
//               Pointer to call back function for page in operations
//
//               P2PageoutCBFnc *pP2PageoutCBFnc
//               Pointer to call back function for page out operations
//
//  Returns:     BOOL
//               Operation summary
BOOL
P2PmsgMgr::PageRegistration ( PINT_PTR nfnP2PageItemCB
                            , P2PageinCBFnc pP2PageinCBFnc
                            , P2PageoutCBFnc pP2PageoutCBFnc ) noexcept
{
    // Activate registration
    m_nfncP2PageCBKey = nfnP2PageItemCB;
    m_pfncP2PageinCB  = pP2PageinCBFnc;
    m_pfncP2PageoutCB = pP2PageoutCBFnc; 
    return FALSE;
}

//
//  Population callback registrations
//
//  Parameters:  PINT_PTR nfnP2PopulateItemCB
//               User defined callback data.  Usually pointer to
//               originating class
//
//               P2PopulateCBFnc *pP2PopulateCBFnc
//               Pointer to call back function for population operations
//
//               P2PageoutCBFnc *pP2PageoutCBFnc
//               Pointer to call back function for page out operations
//
//  Returns:     BOOL
//               Operation summary
BOOL
P2PmsgMgr::PageRegistration ( PINT_PTR nfnP2PopulateItemCB
                            , P2PopulateCBFnc pP2PopulateCBFnc ) noexcept
{
    // Activate registration
    m_nfncP2PopulateCBKey = nfnP2PopulateItemCB;
    m_pfncP2PopulateCB  = pP2PopulateCBFnc;
    return FALSE;
}

//
//  Manages OHLCvs data set paging in and out
//  NOTES: Implementation via delegated callbacks. Null callbacks
//         efectively negates operations
//
//  Parameters:  P2Pos posItem
//               OHLCvs data set index
//
//               BOOL bFlush
//               Flushes loaded page from memory
//
//  Returns:     BOOL
//               Activity summary
BOOL
P2PmsgMgr::PageDatasetIn ( P2Pos posItem )
{
    // Logically only if callback exists
    if ( m_nfncP2PageCBKey )
      return (*m_pfncP2PageinCB)( m_nfncP2PageCBKey, posItem );
    return FALSE;
}
BOOL
P2PmsgMgr::PageDatasetOut ( P2Pos posItem, BOOL bFlush )
{
    // Logically only if callback exists
    if ( m_nfncP2PageCBKey )
      return (*m_pfncP2PageoutCB)( m_nfncP2PageCBKey, posItem, bFlush );
    return FALSE;
}

//
//  Manages paging summaries
//
//  Parameters:  P3PmsgField& oItem
//               Item for which paging is to be managed
//
//               DWORD dwAdditions
//
//               DWORD dwRemovals
//
//  Returns:     DWORD
//               Current paging summary
DWORD
P2PmsgMgr::PageSumm ( P3PmsgItem& oItem, DWORD dwAdditions, DWORD dwRemovals )
{
    // Logically only if callback exists
    if ( m_nfncP2PageCBKey == nullptr ) {
      ASSERT(0); return 0; }           // Absence is very problematic
    P3PmsgItem  oPagesumm  =   oItem.ATTR.DeclareItem(L"$Pagesumm",(DWORD)0).r_Object();
    DWORD      dwPagesumm  =   oPagesumm.c_uint();
               dwPagesumm |=  dwAdditions;
               dwPagesumm &= ~dwRemovals;
                oPagesumm.c_uint(dwPagesumm);
    ASSERT((dwPagesumm&P2Pmsg_PAGESUMM_NOMERGE)==0);
    return dwPagesumm;
}

//
//  Page registrations push and pop
//  NOTE: Single level maximum, must be complimented
//
BOOL
P2PmsgMgr::PageRegistrationPush ( )
{
    ASSERT(m_pfncP2PageinCBp==0&&m_pfncP2PageoutCBp==0&&m_nfncP2PageCBKeyp==0);
    m_pfncP2PageinCBp  = m_pfncP2PageinCB;
    m_pfncP2PageoutCBp = m_pfncP2PageoutCB;
    m_nfncP2PageCBKeyp = m_nfncP2PageCBKey;
    return TRUE;
}
BOOL
P2PmsgMgr::PageRegistrationPop ( )
{
    ASSERT(m_pfncP2PageinCBp&&m_pfncP2PageoutCBp&&m_nfncP2PageCBKeyp);
    m_pfncP2PageinCB  = m_pfncP2PageinCBp;  m_pfncP2PageinCBp = nullptr;
    m_pfncP2PageoutCB = m_pfncP2PageoutCBp; m_pfncP2PageoutCBp = nullptr;
    m_nfncP2PageCBKey = m_nfncP2PageCBKeyp; m_nfncP2PageCBKeyp = 0;
    // TRUE, not FALSE. The restore above always succeeds, and every documented
    // contract over this call -- Push's own return, and
    // msgcore_mgr_page_registration_pop's "returns 1 on success" -- says so.
    // Reporting failure after doing the work went unnoticed because the only
    // caller in the tree was SafeRegistrationPush's destructor, which cannot
    // use a return value. Found 2026-08-15 by MsgFacade's IMsgStore::PopPaging,
    // the first caller that reads it.
    return TRUE;
}

// Operators
P2PmsgMgr&
P2PmsgMgr::operator  = ( const P2PmsgMgr& rhs )
{
   (P3PmsgItem&)*this = (P3PmsgItem&)rhs;
   return *this;
}

///////////////////////////////////////////////////////////////////////
//  Addressing

P3PmsgField
P2PmsgMgr::P2Pos2Field ( P2Pos pos )
{
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
    return P3PmsgField ( objVBList, pos, P2PmsgHeap_Sizeof(objVBList,pos) );
}
P3PmsgAttr
P2PmsgMgr::P2Pos2Attr ( P2Pos pos )
{
    P3PmsgAttr oAttr;
    if ( IsField(pos) )
      oAttr = P3PmsgItem(objVBList,pos,P2PmsgHeap_Sizeof(objVBList,pos)).r_Attr().r_Object();
    //else if ( IsNode(pos) )
    //  oAttr = P3PmsgNode(objVBList,pos,P2PmsgHeap_Sizeof(objVBList,pos)).r_Attr().r_Object();
    return oAttr;
}
P3PmsgObject
P2PmsgMgr::P2Pos2Object ( P2Pos nP2Pos, BOOL bPageIn )
{
    P3PmsgObject oObject;
    oObject.Connectx(objVBList,nP2Pos,0);
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
    // Observe Data paging
    if ( bPageIn ) {
      ASSERT ( oObject.IsField() ); PageDatasetIn ( nP2Pos ); }
    return oObject;
}
CString
P2PmsgMgr::P2Pos2Path ( P2Pos pos )
{
    if ( IsField(pos) )
    {
      P3PmsgField oField = P2Pos2Field(pos).r_Object();
      return P3Pmsg_GetPath ( &oField );
    }
    //else if ( IsNode(pos) )
    //{
    //  ASSERT(0);
    //  P3PmsgNode oNode = P2Pos2Node(pos).r_Object();
    //  return P3Pmsg_GetPath ( &oNode );
    //}
    else ASSERT(0);
    return CString();
}
P3PmsgObject
P2PmsgMgr::Path2Object ( LPCWSTR lpszObjectPath )
{
    return P3Pmsg_SelectObject ( &this->r_Object(), lpszObjectPath );
}

//  Get P3PmsgObject for full P3PmsgObject path
//
//  Parameters:  LPCWSTR lpszObjectPath
//               Full path for P3PmsgObject to be retrieved.
//
//  Returns:     P3PmsgObject
//               Retrieved object, IsEmpty flags failed search
P3PmsgObject
P2PmsgMgr::RootPath2Object ( LPCWSTR lpszObjectPath )
{
    // Introduce locals
    CString        strRoot;
    CList<CString> oCListItems;
    P3PmsgItem     oItemParent = r_Object();     // Root becomes parent

    // Problematic
    try
    {
      P3Pmsg_SplitRootPath ( lpszObjectPath, strRoot, oCListItems );

      // Process the full Object path
      POSITION posItems = oCListItems.GetHeadPosition();
      while ( posItems )
      {
        CString strItem = oCListItems.GetNext(posItems);
        LPCWSTR lpszItemName = strItem;
        if ( P3Pmsg_IsPathDelimiter(lpszItemName) )
          lpszItemName++;

        // Requested path item may or may not exist at this stage
        // NOTES: If the requested Item is non-descendant type cannot proceed
        //      : P2PmsgTreeCtrl's only handle descendant items
        if ( !oItemParent.Exists(lpszItemName) )
        {
          wchar_t wTypeDelimiter = strItem[0];
          if ( wTypeDelimiter == T_DescDelim ||
               wTypeDelimiter == T_BackSlash ||
               wTypeDelimiter == T_ForeSlash    )
          {
            //RefreshFolder(oItemParent);
            if ( m_pfncP2PopulateCB )
              m_pfncP2PopulateCB ( m_nfncP2PopulateCBKey, oItemParent.GetP2Pos(), FALSE );
            if ( !oItemParent.Exists(lpszItemName) )
              EVERR->MODULE
                   //  L"%ls", never the path as the format itself: Message() is
                   //  Message(LPCWSTR lpszFormat, ...) and runs the string
                   //  through _vstprintf_s, so an object path containing a '%'
                   //  was consuming a variadic argument that was never passed
                   //  (finding M3 of the internal, unpublished security
                   //  review). Paths reach here from callers,
                   //  including the C ABI. %ls not %s - wide in both the MSVC
                   //  and glibc dialects (see commit 5aa9b2a).
                   ->Message(L"%ls", lpszObjectPath)
                   ->Message("Path to object does not exist")
                   ->Throw();
          }
        }

        // Select the current item
        // NOTES: Last item in list is the requested item
        oItemParent = oItemParent.SelectObject(lpszItemName);
      }
      return oItemParent.r_Object();
    }

    // Tidy up, and
    // Exceptions
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast
    ThrowP2Pevent();
    return P3PmsgObject();
}

///////////////////////////////////////////////////////////////////////
//  Triggers

#pragma pack(push,1)
typedef struct tagSTrigger
{
    HWND           hWnd;               // Notification window
    DWORD         dwTrigMask;          // Trigger mask
    WPARAM         wParam;             // WPARAM for notification
    LPARAM         lParam;             // LPARAM for notification
} STrigger;
#pragma pack(pop)

//
//  P2PmsgTrigger registrations
//  NOTES: Handles duplicate registrations
//
//  Parameters:  UINT dwTrigMask
//               Triggers mask, refer P2PmsgTrig_UPDATE, INSERT and
//               DELETE series of definitions for further details
//
//               HWND hWnd
//               HWND of window to which trigger notification is to
//               be posted
//
//               P2Pos posP2Pobject
//               P2Pobject to which trigger is attached
//
//               WPARAM wParam
//               Assigned to posted triggers
//
//  Returns:     UINT
//               Number of current registrations
UINT // Upgraded implementation
P2PmsgMgr::CreateTrigger ( UINT nTrigger_TypeMask, HWND hWnd, P2Pos posP2Pobject )
{
    if ( (nTrigger_TypeMask&TRIGGER_INSERT) == TRIGGER_INSERT )
      P2PmsgHeap_CreateTrigger ( m_hMgr, posP2Pobject, hWnd
                               , WM_P2Pmsg_TrigINSERT, TRIGGER_INSERT, (LPARAM)this );
    if ( (nTrigger_TypeMask&TRIGGER_UPDATE) == TRIGGER_UPDATE )
      P2PmsgHeap_CreateTrigger ( m_hMgr, posP2Pobject, hWnd
                               , WM_P2Pmsg_TrigUPDATE, TRIGGER_UPDATE, (LPARAM)this );
    if ( (nTrigger_TypeMask&TRIGGER_DELETE) == TRIGGER_DELETE )
      P2PmsgHeap_CreateTrigger ( m_hMgr, posP2Pobject, hWnd
                               , WM_P2Pmsg_TrigDELETE, TRIGGER_DELETE, (LPARAM)this );
    if ( (nTrigger_TypeMask&TRIGGER_ACTIVE) == TRIGGER_ACTIVE )
      P2PmsgHeap_CreateTrigger ( m_hMgr, posP2Pobject, hWnd
                               , WM_P2Pmsg_TrigACTIVE, TRIGGER_ACTIVE, (LPARAM)this );
    return 1;
}
/*UINT
P2PmsgMgr::CreateTrigger ( UINT dwTrigMask, P2Pos posP2Pobject
                         , HWND hWnd, WPARAM wParam )
{
    // Environmental
    P3PmsgList oListTrig = MakeTriggerList ( posP2Pobject ) ;

    // Duplication check
    STrigger *pSTrigger = 0;
    UINT aPos = oListTrig.GetHeadPos();
    while ( aPos && pSTrigger == nullptr )
    {
      P3PmsgData& oData = oListTrig.GetNext ( aPos );
                  pSTrigger = (STrigger *)oData.c_vBlob();
      if ( hWnd != pSTrigger->hWnd )
        pSTrigger = 0;
    }

    // Creation
    if ( pSTrigger == nullptr )
    {
      P3PmsgData oData ( (void *)0, sizeof(STrigger) );
      oListTrig.AddListTail ( oData );
      pSTrigger = (STrigger *)oListTrig.GetTail().c_vBlob();
    }

    // Tidy up, and
    pSTrigger -> dwTrigMask = dwTrigMask;
    pSTrigger -> hWnd       = hWnd;
    pSTrigger -> wParam     = wParam;
    return oListTrig.GetCount();
}*/

/*UINT
P2PmsgMgr::CreateTrigger ( UINT dwTrigMask, P3PmsgNode& oNode
                         , HWND hWnd, WPARAM wParam )
{
    // Environmental
    if ( !TriggerListExists(oNode) )
      MakeTriggerList ( oNode );
    P3PmsgList oListTrig = GetTriggerList ( oNode );

    // Duplication check
    STrigger *pSTrigger = 0;
    UINT aPos = oListTrig.GetHeadPos();
    while ( aPos && pSTrigger == nullptr )
    {
      P3PmsgData& oData = oListTrig.GetNext ( aPos );
                  pSTrigger = (STrigger *)oData.c_vBlob();
      if ( hWnd != pSTrigger->hWnd )
        pSTrigger = 0;
    }

    // Creation
    if ( pSTrigger == NULL )
    {
      P3PmsgData oData ( (void *)0, sizeof(STrigger) );
      oListTrig.AddListTail ( oData );
      pSTrigger = (STrigger *)oListTrig.GetTail().c_vBlob();
    }

    // Tidy up, and
    pSTrigger -> dwTrigMask = dwTrigMask;
    pSTrigger -> hWnd       = hWnd;
    pSTrigger -> wParam     = wParam;
    return oListTrig.GetCount();
}*/

/*UINT
P2PmsgMgr::CreateTrigger ( UINT dwTrigMask, P3PmsgField& oField
                         , HWND hWnd, WPARAM wParam )
{
    // Environmental
    if ( !TriggerListExists(oField) )
      MakeTriggerList ( oField );
    P3PmsgList oListTrig = GetTriggerList ( oField );

    // Duplication check
    STrigger *pSTrigger = 0;
    UINT aPos = oListTrig.GetHeadPos();
    while ( aPos && pSTrigger == nullptr )
    {
      P3PmsgData& oData = oListTrig.GetNext ( aPos );
                  pSTrigger = (STrigger *)oData.c_vBlob();
      if ( hWnd != pSTrigger->hWnd )
        pSTrigger = 0;
    }

    // Creation
    if ( pSTrigger == NULL )
    {
      P3PmsgData oData ( (void *)0, sizeof(STrigger) );
      oListTrig.AddListTail ( oData );
      pSTrigger = (STrigger *)oListTrig.GetTail().c_vBlob();
    }

    // Tidy up, and
    pSTrigger -> dwTrigMask = dwTrigMask;
    pSTrigger -> hWnd       = hWnd;
    pSTrigger -> wParam     = wParam;
    return oListTrig.GetCount();
}*/

//UINT
//P2PmsgMgr::CreateTrigger ( UINT dwTrigMask, P3PmsgField& oField
//                       , HWND hWnd, WPARAM wParam, LPARAM lParam )
//{
//    ASSERT(0);
//    return 0;
//}

DWORD
P2PmsgMgr::SelectTrigger ( P2Pos posP2Pobject, HWND hWnd )
{
    UNREFERENCED_PARAMETER(posP2Pobject);
    UNREFERENCED_PARAMETER(hWnd);
    // Isolate trigger list
    DWORD dwTrigMask = 0;
    ASSERT(0);
//    P3PmsgList oListTrig = GetP2PmsgListHdlTrig ( posP2Pobject );
//    if ( !oListTrig )
//      return dwTrigMask;

    // For all instances
//    UINT aPos = oListTrig.GetHeadPos();
//    while ( aPos )
//    {
      //UINT aPosDrop = aPos;
//      P3PmsgData& oData = oListTrig.GetNext ( aPos );
//      STrigger   *pSTrigger = (STrigger *)oData.c_vBlob();
//      if ( pSTrigger->hWnd != hWnd )
//        continue;
//      ASSERT(dwTrigMask==0);
//      dwTrigMask = pSTrigger -> dwTrigMask;
//    }
    return dwTrigMask;
}

//
//  P2PmsgTrigger de-registrations
//  NOTES: Deregisters triggers for nominated HWND
/*UINT
P2PmsgMgr::DropTrigger ( P2Pos posP2Pobject, HWND hWnd
                       , DWORD dwTrigMask, bool bRecurs )
{
    // Isolate trigger list
    P3PmsgList oListTrig = GetP2PmsgListHdlTrig ( posP2Pobject );
    if ( !oListTrig                 ||
          oListTrig.GetCount() <= 0    )
      return 0;
    UINT       nTriggers = 0;

    // For all instances
    UINT aPos = oListTrig.GetHeadPos();
    while ( aPos )
    {
      UINT aPosDrop = aPos;
      P3PmsgData& oData = oListTrig.GetNext ( aPos );
      STrigger   *pSTrigger = (STrigger *)oData.c_vBlob();
      if ( pSTrigger->hWnd != hWnd )
        continue;
      pSTrigger -> dwTrigMask &= ~dwTrigMask;
      if ( pSTrigger->dwTrigMask == 0 )
        oListTrig.Delete ( aPosDrop );
      nTriggers++;
    }
    return nTriggers;
}*/

UINT // Upgraded implementation
P2PmsgMgr::DropTriggers ( UINT nTrigger_TypeMask
                        , HWND hWnd, P2Pos posP2Pobject )
{
    if ( posP2Pobject )
      // Args are (hVBList, aVBLock, hWnd, nTriggerTypeMask): pass the P2Pos as the
      // address and the mask as the mask. (These were previously swapped, so a
      // drop-by-P2Pos looked up a bogus address and silently did nothing -- latent
      // because only the headless drop-by-pos path exercises this branch.)
      return P2PmsgHeap_DropTrigger ( m_hMgr, (VBLaddr)posP2Pobject, hWnd, nTrigger_TypeMask );
    else
      return P2PmsgHeap_DropTriggers( m_hMgr, hWnd, nTrigger_TypeMask );
}

//
//  P2PmsgTrigger activations
//  NOTES: Activates triggers for nominated P2PmsgObj's
UINT
P2PmsgMgr::TriggerINSERT ( P3PmsgObject& oObjectParent )
{
    // Direct delegation
    ASSERT(oObjectParent.m_hVBList == m_hMgr);
    return P2PmsgHeap_ProcTriggers ( m_hMgr, oObjectParent.GetP2Pos(), TRIGGER_INSERT );
}
UINT
P2PmsgMgr::TriggerINSERT ( const P3PmsgItem& oItemParent )
{
    // Direct delegation
    ASSERT(oItemParent.r_Object().m_hVBList == m_hMgr);
    return P2PmsgHeap_ProcTriggers ( m_hMgr, oItemParent.GetP2Pos(), TRIGGER_INSERT );
}

UINT
P2PmsgMgr::TriggerUPDATE ( P3PmsgItem& oItem )
{
    // Direct delegation
    ASSERT(oItem.r_Object().m_hVBList == m_hMgr);
    return P2PmsgHeap_ProcTriggers ( m_hMgr, oItem.GetP2Pos(), TRIGGER_UPDATE );
    /*UINT nTriggers = 0;

    // Isolate
    if ( !TriggerListExists(oChild) )
      return 0;
    P3PmsgList oListTrig = GetTriggerList ( oChild );

    // For all registrations
    UINT aPos = oListTrig.GetHeadPos();
    while ( aPos )
    {
      P3PmsgData& oData = oListTrig.GetNext ( aPos );
      STrigger   *pSTrigger = (STrigger *)oData.c_vBlob();
      PostMessage ( pSTrigger->hWnd, m_uiWM_APP_TrigUPDATE
                  , pSTrigger->wParam, oChild.GetP2Pos() );
      nTriggers++;
    }
    return nTriggers;*/
}
UINT
P2PmsgMgr::TriggerDELETE ( P3PmsgItem& oItem )
{
    // Direct delegation
    ASSERT(oItem.r_Object().m_hVBList == m_hMgr);
    return P2PmsgHeap_ProcTriggers ( m_hMgr, oItem.r_Object().GetP2Pos(), TRIGGER_DELETE );
}

void
P2PmsgMgr::SetTriggerSink ( P2PmsgTriggerSink pfn, void* pUser )
{
    P2PmsgHeap_SetTriggerSink ( m_hMgr, pfn, pUser );
}

UINT
P2PmsgMgr::FireTrigger ( P2Pos posP2Pobject, UINT nTrigger_TypeMask )
{
    // Fire by P2Pos directly (no P3PmsgItem needed): the FUSE/daemon write path
    // knows the changed object's P2Pos but not necessarily a live wrapper.
    return P2PmsgHeap_ProcTriggers ( m_hMgr, (VBLaddr)posP2Pobject, nTrigger_TypeMask );
}


//P2PmsgListHdl
//P2PmsgMgr::GetTriggerList ( P3PmsgField& oField )
//{
//    if ( !oField.r_Attr().Exists(_T("#Trig")) )
//    {
//      P2PmsgListHdl oHdl = { 0, 0, 0};
//      return oHdl;
//    }
//    P3PmsgList oListTrig = oField.r_Attr().SelectList (_T("#Trig")).GetP2PmsgListHdl();
//
//    // TODO:LJM Hack for progress vvvvvvv
//    if ( oListTrig.r_data().DataType() != VBLockData_BLOB08 )
//      oListTrig.r_data() = P3PmsgData ( &m_oGUID, sizeof(m_oGUID), VBLockData_BLOB08 );
//    // TODO:LJM Hack for progress ^^^^^^^
//    if ( !IsEqualGUID(m_oGUID,*(GUID *)oListTrig.r_data().c_vBlob()) )
//    {
//      oListTrig.r_data().c_vBlob(&m_oGUID,sizeof(m_oGUID));
//      oListTrig.Truncate();
//    }
//    return oListTrig.GetP2PmsgListHdl();
//}
//bool
//P2PmsgMgr::TriggerListExists ( P3PmsgNode& oNode )
//{
//    return oNode.r_Attr().Exists(_T("#Trig"));
//}
//bool
//P2PmsgMgr::TriggerListExists ( P3PmsgField& oField )
//{
//    return oField.r_Attr().Exists(_T("#Trig"));
//}

/*P2PmsgListHdl
P2PmsgMgr::GetP2PmsgListHdlTrig ( P2Pos posP2Pobject )
{
    VBLock       *pVBLock = (VBLock *)r_Object().Msg2Phys ( posP2Pobject );
    VBLockItem   *pItem   =  VBLock_pItem ( pVBLock );
    P2PmsgListHdl oHdl = { 0, 0, 0 };
    if ( VBLockItem_IsField(pItem) )
    {
      P3PmsgField oField (objVBList, posP2Pobject, VBLock_Hdr_u_SizeNN(pVBLock) );
      if ( !oField.r_Attr().Exists(_T("#Trig")) )
        return oHdl;
      return oField.r_Attr().SelectList(_T("#Trig")).GetP2PmsgListHdl();
    }
    if ( VBLockItem_IsNode(pItem) )
    {
      P3PmsgNode oNode (objVBList, posP2Pobject, VBLock_Hdr_u_SizeNN(pVBLock) );
      if ( !oNode.r_Attr().Exists(_T("#Trig")) )
        return oHdl;
      return oNode.r_Attr().SelectList(_T("#Trig")).GetP2PmsgListHdl();
    }
    if ( VBLockItem_IsList(pItem) )
    {
      P3PmsgList oList (objVBList, posP2Pobject, VBLock_Hdr_u_SizeNN(pVBLock) );
      if ( !oList.r_Attr().Exists(_T("#Trig")) )
        return oHdl;
      return oList.r_Attr().SelectList(_T("#Trig")).GetP2PmsgListHdl();
    }
    ASSERT(0);
    return oHdl;
}*/

///////////////////////////////////////////////////////////////////////
//  Troubleshooting

//
//  Checks the validity of the underlying P2Pmsg structure
//
//  Returns:     BOOL
//                 TRUE... All good
//                 FALSE.. Broken
BOOL
P2PmsgMgr::IsValid ( ) const
{
    // Delegate
    try
    {
      ASSERT(m_hMgr==this->r_Object().m_hVBList);
    __super::AssertValid ( );
      // P2PmsgHeap internals
      P2PmsgHeap_AssertValid ( m_hMgr );
      P2PmsgHeap_AssertVBlocks ( m_hMgr );
      // All good
      return TRUE;
    }
    // Exceptions
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast
    return FALSE;
}
void
P2PmsgMgr::AssertValid ( ) const
{
    // Delegate
    ASSERT(m_hMgr==this->r_Object().m_hVBList);
  __super::AssertValid ( );

    // P2PmsgHeap internals
    P2PmsgHeap_AssertValid ( m_hMgr );
    P2PmsgHeap_AssertVBlocks ( m_hMgr );
}
void
P2PmsgMgr::Print ( FILE *fd, int nDepthOS, int nDepthOSinc )
{
    // Delegate
  __super::Print ( fd, nDepthOS, nDepthOSinc );
}

///////////////////////////////////////////////////////////////////////
//  Properties

LPCTSTR
P2PmsgMgr::GetFilename ( )
{
    return m_strFilename;
}

LPCTSTR
P2PmsgMgr::GetRootname ( )
{
    return r_name().c_name();
}

bool
P2PmsgMgr::IsDirty ( )
{
    // Delegate
    return P2PmsgHeap_IsDirty(objVBList) ? true : false;
}

BOOL
P2PmsgMgr::SetDirty ( BOOL bDirty )
{
   return P2PmsgHeap_SetDirty ( objVBList, bDirty );
}

bool
P2PmsgMgr::IsField ( P2Pos posP2Pobject )
{
    if ( posP2Pobject == 0 )
      return false;
    VBLock *pVBLock = (VBLock *)r_Object().Msg2Phys ( posP2Pobject );
    if ( !VBLock_IsItem(pVBLock) )
      return VBLock_IsField(pVBLock);
    return VBLockItem_IsField ( VBLock_pItem(pVBLock) );
}

BOOL
P2PmsgMgr::IsAttributed (UCHAR ucAttribute, P2Pos posP2Pobject)
{
    if ( posP2Pobject == 0 )
      return FALSE;
    P3PmsgField oItem( objVBList, posP2Pobject, P2PmsgHeap_Sizeof(objVBList,posP2Pobject) );
    return oItem.r_Desc().GetPermissions(AttrField_EXPAND) ? TRUE : FALSE;
}

VBLsize
P2PmsgMgr::Sizeof ( )
{
    //void  *vpIOmage       = P2PmsgHeap_pImage ( m_hMgr ); NLR?
    return P2PmsgHeap_Sizeof ( m_hMgr );
}

///////////////////////////////////////
//  Safe DSet Paging
//  NOTES: Manages the lifecycle of paged Data Sets within a P2PmsgMgr.
//         The paging is managed by the P2PmsgMgr and the SafeDSetPaging
//         class provides a convenient RAII-style wrapper to ensure that
//         the paging is properly cached and flushed.
//       : Only relevant for large datasets that are paged in and out of memory.
//         For small datasets, the paging is not necessary and the SafeDSetPaging
//         class is not applicable.
SafeDSetPaging::SafeDSetPaging ( P2PmsgMgr& oP2PmsgMgr, P3PmsgItem& oDSetItem )
{
    m_pP2PmsgMgr    = &oP2PmsgMgr;
    m_eDSetPageSumm = m_pP2PmsgMgr -> PageSumm ( oDSetItem );
    m_pP2PmsgMgr -> PageDatasetIn ( oDSetItem.GetP2Pos() );
    m_pDSetItem     = &oDSetItem;
}
   
SafeDSetPaging::~SafeDSetPaging ()
{
    try
    {
      if ( m_pP2PmsgMgr &&
           m_pDSetItem  &&
          (m_eDSetPageSumm& P2Pmsg_PAGESUMM_CACHED) != P2Pmsg_PAGESUMM_CACHED )
        m_pP2PmsgMgr -> PageDatasetOut ( m_pDSetItem->GetP2Pos(), TRUE );
    }
    // Exceptions
    catch_pP2Pevent_Cancel
    catch_pCException_Cancel
    catch_ALL_Cancel
}

P3PmsgItem*
SafeDSetPaging::operator -> () noexcept
{
    return m_pDSetItem;
}

P3PmsgItem*
SafeDSetPaging::operator = ( P3PmsgItem *pDSetItem )
{
    if ( m_pP2PmsgMgr &&
         m_pDSetItem  &&
        (m_eDSetPageSumm& P2Pmsg_PAGESUMM_CACHED) != P2Pmsg_PAGESUMM_CACHED )
      m_pP2PmsgMgr -> PageDatasetOut ( m_pDSetItem->GetP2Pos(), TRUE );
    m_eDSetPageSumm = pDSetItem ? m_pP2PmsgMgr -> PageSumm ( *pDSetItem ) : 0;
    return m_pDSetItem = pDSetItem;
}
P3PmsgItem*
SafeDSetPaging::Dereference () noexcept
{
    auto pDSetItem = m_pDSetItem;
    m_pP2PmsgMgr = nullptr;
    m_pDSetItem  = nullptr;
    return pDSetItem;
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgMgr helpers
//  NOTES: Standard extensions and activities
BOOL
P2PmsgMgr_IsValid ( P2PmsgMgr *pP2PmsgMgr ) noexcept
{
    // Expect issues
    try
    {
      if ( pP2PmsgMgr )
        pP2PmsgMgr->AssertValid();
      return TRUE;
    }
    // Exceptions
    catch_pP2Pevent_SetLast
    catch_pCException_SetLast
    catch_ALL_SetLast
    return FALSE;
}
BOOL
P2PmsgMgr_IsValid ( LPCTSTR lpszFilename ) noexcept
{
    // Expect issues
    try
    {
      std::unique_ptr<P2PmsgMgr> upP2PmsgMgr ( P2PmsgMgr::Factory(lpszFilename,FILE_SHARE_READ) );
      if ( upP2PmsgMgr == nullptr )
        return FALSE;
      upP2PmsgMgr->AssertValid();
      return TRUE;
    }
    // Exceptions
    catch_pP2Pevent_Cancel
    catch_pCException_Cancel
    catch_ALL_Cancel
    return FALSE;
}

//
//  Swaps P3PmsgItem's within P2PmsgMgr
//  NOTES: Performs notifications
//
//  Parameters:  P2PmsgMgr oP2PmsgMgr
//
//               P3PmsgItem oSwapItem1
//
//               P3PmsgItem oSwapItem2
//
//  Returns:     ?
//
Msgcore_EXT BOOL
P2PmsgMgr_Swap ( P2PmsgMgr& oP2PmsgMgr
               , P3PmsgItem& oSwapItem1, P3PmsgItem& oSwapItem2 )
{
    if ( oSwapItem1.GetP2Pos() == oSwapItem2.GetP2Pos() )
      return FALSE;
    P2PmsgDesc_Swap ( oSwapItem1, oSwapItem2 );
    P3PmsgObject oParent1 = oSwapItem1.r_Object().GetParent();
    if ( oParent1.IsDesc() )
      oParent1 = oParent1.GetParentItem();
    if ( oParent1.IsField() )
      oP2PmsgMgr.TriggerINSERT(oParent1);
    else { ASSERT(0); }
    P3PmsgObject oParent2 = oSwapItem1.r_Object().GetParent();
    if ( oParent2.IsDesc() )
      oParent2 = oParent2.GetParentItem();
    if ( oParent2.IsField() )
      oP2PmsgMgr.TriggerINSERT(oParent2);
    else { ASSERT(0); }
    return TRUE;
}

//
//  Sorts P3PmsgItem's within P2PmsgMgr
//  NOTES: Performs notifications
//
//  Parameters:  P2PmsgMgr oP2PmsgMgr
//
//               P3PmsgItem oSortItem
//
//  Returns:     Summary
//                 TRUE... Items sorted
//                 FALSE.. No sorting required
Msgcore_EXT BOOL
P2PmsgMgr_Sort ( P2PmsgMgr& oP2PmsgMgr, P3PmsgItem& oSortItem )
{
    BOOL bResult = FALSE;
    P3PmsgCurs oSortCurs(oSortItem);
    if ( oSortCurs.GetCount() <= 1 )
      return FALSE;                    // Nothing to sort
    for ( int i = 1; oSortCurs.Goto(i); i++ )
    {
      ASSERT(i);
      P3PmsgItem oItemCurr = oSortCurs.r_Object();
      CString  strNameCurr = oItemCurr.c_name();
      oSortCurs.Goto(i-1);
      P3PmsgItem oItemPrev = oSortCurs.r_Object();
      CString  strNamePrev = oItemPrev.c_name();
      if ( strNameCurr.CompareNoCase(strNamePrev) >= 0 )
        continue;
      P2PmsgDesc_Swap ( oItemPrev, oItemCurr );
      oSortCurs.Goto(0); i = 0;
      bResult= TRUE;
    }
    ASSERT(!bResult);
    return bResult;
}

//
//  Safe bolded item class
//  NOTES: Bolds item upon instanciation and restores previous state when popped from stack
SafeRegistrationPush::SafeRegistrationPush ( P2PmsgMgr *pP2PmsgMgr )
            : m_pP2PmsgMgr(pP2PmsgMgr)
{
    if ( m_pP2PmsgMgr )
      m_pP2PmsgMgr->PageRegistrationPush();
}
SafeRegistrationPush::~SafeRegistrationPush ( )
{
    if ( m_pP2PmsgMgr )
      m_pP2PmsgMgr->PageRegistrationPop();
}
