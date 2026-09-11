// Copyright © 2005-2013, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2Peer message definitions and prototypes
//
#include "stdafx.h"
#include "Propvarutil.h"
#include "P2Pmsg.h"
#include "P2Pmsg_Ext.h"
#include "P2PmsgVBLock.h"
#include "MsgVBHeap.h"
#include "MsgAttr.h"
#include "MsgDesc.h"
#include "MsgStck.h"
#include "MsgList.h"
#include "MsgVect.h"
#include "MsgCurs.h"
#include "Msgexception.h"

bool Msgcore_EXT
g_bP2Pmsg_AssertValid = true;
VBLsize
g_nVBListCreateHeap_SizeMax = 10000000;

#define  OBJ__         m_oObject
#define pOBJ__         m_pObject
#define  OBJ__hVBList  m_oObject.m_hVBList
#define pOBJ__hVBList  m_pObject->m_hVBList
#define  OBJ__uVBLock  m_oObject.m_uVBLock
#define pOBJ__uVBLock  m_pObject->m_uVBLock
#define  OBJ__aVBLock  m_oObject.m_aVBLock
#define pOBJ__aVBLock  m_pObject->m_aVBLock
#define  OBJ__Alloc    m_oObject.Alloc
#define pOBJ__Alloc    m_pObject->Alloc
#define  OBJ__Free     m_oObject.Free
#define pOBJ__Free     m_pObject->Free
#define  OBJ__Msg2Phys m_oObject.Msg2Phys
#define pOBJ__Msg2Phys m_pObject->Msg2Phys
#define  OBJ__Drop     m_oObject.Drop
#define  OBJ__VBLocknn m_oObject.GetVBLocknn()
#define pOBJ__VBLocknn m_pObject->GetVBLocknn()
#define  OBJ__VBLock   ((VBLock *)m_oObject.GetVBLock())
#define  OBJ__VBLockc  m_oObject.GetVBLock()
#define pOBJ__VBLock   ((VBLock *)m_pObject->GetVBLock())
#define pOBJ__VBLockc   m_pObject->GetVBLock()
#define  OBJ__IsField  m_oObject.IsField
#define  OBJ__IsList   m_oObject.IsList
#define  OBJ__IsVect   m_oObject.IsVect
//#define  OBJ__IsNode   m_oObject.IsNode

#define  ptrVBLOCK(OBJ)  ((VBLock *)(OBJ).GetVBLock())


///////////////////////////////////////////////////////////////////////
//  P2PmsgData class
//  NOTES: VBlockData wrapper
//VBLockData*
//P2PmsgObject_pData  ( const P3PmsgObject& oObject, bool bIndirect = true );
//VBLockData*
//P2PmsgData_pData    ( const P3PmsgData& oData, bool bIndirect = true );
//VBLsize
//P2PmsgObject_VBLockDataSize( const P3PmsgObject& oObject );
//VBLockData*
//P2PmsgObject_NewVBLockData ( P3PmsgObject& oObject, VBLsize nSizeof );
//VBLockName*
//P2PmsgObject_pName  ( const P3PmsgObject& oObject, bool bIndirect = true );
//VBLsize
//P2PmsgObject_VBLockNameSize( const P3PmsgObject& oObject );

#define SET_DIRTY \
  if ( !m_bDataDirty ) \
  { m_bDataDirty = true; \
    if ( pOBJ__ ) \
      P2PmsgHeap_SetDirty ( pOBJ__hVBList, TRUE ); } 
    
//
//  Constructors and destructors
P3PmsgData::P3PmsgData ( const P3PmsgField *pField )
{
    pField;                            // Not used
}
P3PmsgData::P3PmsgData ( )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    pData -> uDataAttr |= VBLockAttr_NULL;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( const P3PmsgData& rhs )
{
    // Give this object its own cell first, then let operator= size it and
    // copy the payload in - the same two steps every other copy constructor
    // in the family takes. The implicit copy this replaces shared one cell
    // between two owners and double-deleted it.
    RenderThisSafe ( );
    P3PmsgData::operator = ( rhs );
}
P3PmsgData::P3PmsgData ( INT08 vInt08 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vInt08     =  vInt08;
    pData -> uDataType    =  VBLockData_INT08;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( UINT08 vuInt08 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vuInt08    =  vuInt08;
    pData -> uDataType    =  VBLockData_UINT08;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( INT16 vInt16 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vInt16     =  vInt16;
    pData -> uDataType    =  VBLockData_INT16;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( UINT16 vuInt16 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vuInt16    =  vuInt16;
    pData -> uDataType    =  VBLockData_UINT16;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( INT32 vInt32 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vInt32     =  vInt32;
    pData -> uDataType    =  VBLockData_INT32;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( UINT32 vuInt32 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vuInt32    =  vuInt32;
    pData -> uDataType    =  VBLockData_UINT32;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( INT64 vInt64 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vInt64     =  vInt64;
    pData -> uDataType    =  VBLockData_INT64;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( UINT64 vuInt64 )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vuInt64    =  vuInt64;
    pData -> uDataType    =  VBLockData_UINT64;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( long vLong )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vInt32     =  vLong;
    pData -> uDataType    =  VBLockData_INT32;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( unsigned long vuLong )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vuInt32    =  vuLong;
    pData -> uDataType    =  VBLockData_UINT32;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( float vFloat )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vFloat     = vFloat;
    pData -> uDataType    = VBLockData_FLOAT;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( double vDouble )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vDouble    = vDouble;
    pData -> uDataType    = VBLockData_DOUBLE;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( bool vBool )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vBool      = vBool;
    pData -> uDataType    = VBLockData_BOOL;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( wchar_t vwChar )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vwChar     = vwChar;
    pData -> uDataType    = VBLockData_WCHAR;
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( LPCSTR vString, size_t nLength, UCHAR uVBLockData )
{
    if ( nLength <= 0 && vString )
      nLength = strlen ( vString );
    RenderThisSafe ( );
    VBLockData_InitBlob ( P2PmsgObject_pData(*pOBJ__)
                        , VBLockAttr_DEFAULT, uVBLockData
                        , P2PmsgObject_Sizeof_VBLockData(*pOBJ__) );
    c_memcpy ( vString, nLength*sizeof(vString[0]) );
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( LPCWSTR vString, size_t nSizeofWSTR, UCHAR uVBLockData)
{
    if ( nSizeofWSTR <= 0 &&
         vString         )
      nSizeofWSTR = wcslen(vString);
    RenderThisSafe();
    VBLockData_InitBlob ( P2PmsgObject_pData(*pOBJ__)
                        , VBLockAttr_DEFAULT, uVBLockData
                        , P2PmsgObject_Sizeof_VBLockData(*pOBJ__));
#if defined(_WIN32)
    c_memcpy ( vString, nSizeofWSTR*sizeof(vString[0]) );
#else
    //  Convert wchar_t (4 bytes on Linux) -> 16-bit P2PWCHAR before the RAW blob copy, so
    //  the stored width matches the pinned on-disk/wire format (§4.2). Without this the raw
    //  copy dumps 4-byte chars into a 2-byte-char store, and c_wstr readback stops at the
    //  first embedded NUL -> every WSTR data cell (message source/destin addresses, event
    //  Dsc text) arrived as a single character. On Windows P2PWCHAR==wchar_t so the original
    //  raw copy is already correct -> hence the guard. (The audit's sizeof(wchar_t) sweep
    //  missed this site because it spells the width as sizeof(vString[0]).)
    //  Worst case one wchar_t -> two P2PWCHAR (astral surrogate pair); the stored byte
    //  length is the actual UTF-16 unit count, which is what Windows already stores.
    std::vector<P2PWCHAR> vStore ( nSizeofWSTR ? nSizeofWSTR * 2 : 1 );
    size_t nUnits = p2p_store_wide ( vStore.data(), vString, nSizeofWSTR );
    c_memcpy ( vStore.data(), nUnits*sizeof(P2PWCHAR) );
#endif
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( const void *pvBlob, VBLsize nSizeofBlob, UCHAR uVBLockData )
{
    RenderThisSafe ( );
    VBLockData_InitBlob ( P2PmsgObject_pData(*pOBJ__)
                        , VBLockAttr_DEFAULT, uVBLockData
                        , P2PmsgObject_Sizeof_VBLockData(*pOBJ__) );
    c_memcpy ( pvBlob, nSizeofBlob );
    //ASSERT(VerifyContainment());
}
P3PmsgData::P3PmsgData ( const GUID& oGUID )
{
    RenderThisSafe ( );
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    SET_DIRTY;
    pData -> u.vGUID      = oGUID;
    pData -> uDataType    = VBLockData_GUID;
    //ASSERT(VerifyContainment());
}
P3PmsgData::~P3PmsgData ( )
{
    if ( m_bDataDirty && pOBJ__ )
      P2PmsgHeap_SetDirty ( pOBJ__hVBList, TRUE );
    if ( pOBJ__ == nullptr)
      return;
    delete pOBJ__;
}
void
P3PmsgData::Connect ( P2PmsgHANDLE hVBList, VBLaddr aData, VBLsize nDataSize )
{
    if ( !pOBJ__hVBList &&
          pOBJ__aVBLock    )
      pOBJ__aVBLock = pOBJ__Free ( pOBJ__aVBLock );
    m_pObject -> Connectx ( hVBList, aData, nDataSize );
}
void
P3PmsgData::RenderThisSafe ( )
{
    ASSERT(m_pObject==nullptr);
    m_pObject    = new P3PmsgObject ( );
    m_pObject -> ConnectVBLock ( );
    /*VBLock& oVBLock = *(VBLock*)m_pObject->m_oVBLock;      // Delegate field object
    //m_bDataDirty = false;
    //m_hVBListData= 0;
    //m_uVBLock    = VBLock_Addr32;
    VBLock_Init    (&oVBLock
                   , pOBJ__uVBLock|VBLock_Data|VBLock_Linked|VBLock_Alloc
                   , sizeof(oVBLock) ); TODO:LJM 64 bit migration*/
    //TODO:LJM is this required VBLockItem_Init( m_uVBLock, VBLock_pItem(&oVBLock), VBLock_Data );
    VBLock *pVBLock = (VBLock *)m_pObject->GetVBLock();
            pVBLock -> oHdr.uVBLockDefs |= VBLock_Data;
    VBLockData_Init( VBLock_pData(pVBLock), VBLockAttr_DEFAULT
                   , VBLockData_NULL, VBLock_Sizeof_Hdr_ud(pVBLock) );
                   //, VBLockData_NULL, m_pObject->GetVBLockSize() ); //delete-bug-fix 11/04/2022
    //m_pObject -> Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock.ud.oData) );
}

void
P3PmsgData::Recreate ( P2Pvar_t nP2Pvar, const void *pvData, VBLsize iDataSize )
{
ASSERT(nP2Pvar);
    VBLockData *pData  = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != nP2Pvar )
      Nullify ( );
    pData -> uDataType = nP2Pvar;
    if ( pvData == nullptr )
      return;

    // Simple C types
    if ( nP2Pvar < VBLockData_BSTR08 )
    {
      ASSERT(iDataSize==0);
      if ( nP2Pvar == VBLockData_INT08 )
        pData->u.vInt08 = *(INT08*)pvData;
      else if ( nP2Pvar == VBLockData_UINT08 )
        pData->u.vuInt08 = *(UINT08*)pvData;
      else if ( nP2Pvar == VBLockData_INT16 )
        pData->u.vInt16 = *(INT16*)pvData;
      else if ( nP2Pvar == VBLockData_UINT16 )
        pData->u.vuInt16 = *(UINT16*)pvData;
      else if ( nP2Pvar == VBLockData_INT32 )
        pData->u.vInt32 = *(INT32*)pvData;
      else if ( nP2Pvar == VBLockData_UINT32 )
        pData->u.vuInt32 = *(UINT32*)pvData;
      //else if ( uDType == VBLockData_LONG )
      //  sprintf ( m_szToString, _T("%i"), c_long() );
      else if ( nP2Pvar == VBLockData_INT64 )
        pData->u.vInt64 = *(INT64*)pvData;
      else if ( nP2Pvar == VBLockData_UINT64 )
        pData->u.vuInt64 = *(UINT64*)pvData;
      else if ( nP2Pvar == VBLockData_FLOAT )
        pData->u.vFloat = *(FLOAT*)pvData;
      else if ( nP2Pvar == VBLockData_DOUBLE )
        pData->u.vDouble = *(DOUBLE*)pvData;
      else if ( nP2Pvar == VBLockData_TIME64 )
        pData->u.vTime64 = *(TIME64*)pvData;
      else if ( nP2Pvar == VBLockData_BOOL )
        pData->u.vBool   = *(bool*)pvData;
      else if ( nP2Pvar == VBLockData_WCHAR )
        pData->u.vwChar  = *(wchar_t*)pvData;
      else
        ASSERT(0);
      return;
    }

    // BSTR types
    else if ( nP2Pvar <= VBLockData_BSTR32 )
      c_memcpy( pvData, iDataSize );

    // WSTR types
    //  NOTE (§4.2): this raw c_memcpy assumes the source width == the stored P2PWCHAR
    //  width. That holds on Windows (wchar_t IS 16-bit) but NOT on Linux, where a
    //  wchar_t source is UTF-32 and would need p2p_store_wide conversion + a unit (not
    //  sizeof(TCHAR)) size. Currently UNEXERCISED — the only caller, P2Pc_str<WSTR..>,
    //  is never instantiated and no live code calls Recreate() with a WSTR type; wide
    //  values are set via c_wstr()/operator=(const wchar_t*), which pin correctly. If a
    //  WSTR Recreate() caller is ever added, convert here rather than raw-copy.
    else if ( nP2Pvar <= VBLockData_WSTR16 )
      c_memcpy ( pvData, iDataSize );
    else if ( nP2Pvar <= VBLockData_WSTR32 )
      c_memcpy ( pvData, iDataSize );
   
    // BLOB
    else if ( nP2Pvar >= VBLockData_BLOB08 &&
              nP2Pvar <= VBLockData_BLOB32    )
      c_vBlob ( pvData, iDataSize );

    // GUID
    else if ( nP2Pvar == VBLockData_GUID )
      pData->u.vGUID = *(GUID*)pvData;
    else
      ASSERT(0);
    return;
}

//  Operators
P3PmsgData&
P3PmsgData::operator = ( const P3PmsgData& rhs )
{
    if ( this == &rhs )
      return *this;
    //ASSERT(rhs.P3PmsgData::VerifyContainment()); // TODO:BBHere
    //ASSERT(P3PmsgData::VerifyContainment());
    //VBLsize nSizeof_rhs = rhs.P3PmsgData::Sizeof();
    VBLsize nSizeof_rhs = VBLockData_Sizeof ( pOBJ__uVBLock, P2PmsgObject_pData(*rhs.pOBJ__) );
    VBLsize nSizeof_lhs = P2PmsgObject_Sizeof_VBLockData ( *pOBJ__ );
    if ( nSizeof_lhs < nSizeof_rhs )
    { // Source and destination may have different heap addressing modes
      // NOTES: Exception if heap not available.  Sizes MUST be compared using same base
      //ASSERT(p_Object()->m_uVBLock==rhs.p_Object()->m_uVBLock);
      P2PmsgObject_NewVBLockData ( *pOBJ__, nSizeof_rhs );
    }
    VBLockData *pData_lhs  = P2PmsgObject_pData(*pOBJ__);     //TODO:LJM deprecated rhs.GetVBLockData();
    VBLockData *pData_rhs  = P2PmsgObject_pData(*rhs.pOBJ__); //TODO:LJM deprecated rhs.GetVBLockData();
    ASSERT(VBLock_IsContainedVBLump(GetContainerVBLock(),pData_lhs,0));
    //ASSERT(m_pObject->IsContainedVBLump(pData_rhs,nSizeof_rhs));
    VBLockData_Copy ( pData_lhs, P2PmsgObject_Sizeof_VBLockData(*pOBJ__)
                    , pData_rhs, nSizeof_rhs );
    if( P3PmsgData::Sizeof()>nSizeof_rhs) { //TODO:Delete debugging
    ASSERT(P3PmsgData::Sizeof()<=nSizeof_rhs); }
    SET_DIRTY;
    return *this;
}
BOOL
P3PmsgData::operator == ( const P3PmsgData& rhs )
{
    if ( this == &rhs )
      return TRUE;
    VBLockData *pData1 = P2PmsgObject_pData(*pOBJ__);
    VBLockData *pData2 = P2PmsgObject_pData(*rhs.m_pObject);
    if ( pData1->uDataType != pData2->uDataType )
      return FALSE;                    // Mis-matched data types
    P2Pvar_t nP2Pvar   = pData1->uDataType;

    // Simple C types
    if ( nP2Pvar < VBLockData_BSTR08 )
    {
      switch ( nP2Pvar )
      {
        case VBLockData_INT08:
          return pData1->u.vInt08 == pData2->u.vInt08 ? TRUE : FALSE;
          break;
        case VBLockData_UINT08:
          return pData1->u.vuInt08 == pData2->u.vuInt08 ? TRUE : FALSE;
          break;
        case VBLockData_INT16:
          return pData1->u.vInt16 == pData2->u.vInt16 ? TRUE : FALSE;
          break;
        case VBLockData_UINT16:
          return pData1->u.vuInt16 == pData2->u.vuInt16 ? TRUE : FALSE;
          break;
        case VBLockData_INT32:
          return pData1->u.vInt32 == pData2->u.vInt32 ? TRUE : FALSE;
          break;
        case VBLockData_UINT32:
          return pData1->u.vuInt32 == pData2->u.vuInt32 ? TRUE : FALSE;
          break;
      //else if ( uDType == VBLockData_LONG )
      //  sprintf ( m_szToString, _T("%i"), c_long() );
        case VBLockData_INT64:
          return pData1->u.vInt64 == pData2->u.vInt64 ? TRUE : FALSE;
          break;
        case VBLockData_UINT64:
          return pData1->u.vuInt64 == pData2->u.vuInt64 ? TRUE : FALSE;
          break;
        case VBLockData_FLOAT:
          return pData1->u.vFloat == pData2->u.vFloat ? TRUE : FALSE;
          break;
        case VBLockData_DOUBLE:
          return pData1->u.vDouble == pData2->u.vDouble ? TRUE : FALSE;
          break;
        case VBLockData_TIME64:
          return pData1->u.vTime64 == pData2->u.vTime64 ? TRUE : FALSE;
          break;
        case VBLockData_BOOL:
          return pData1->u.vBool == pData2->u.vBool ? TRUE : FALSE;
          break;
        case VBLockData_WCHAR:
          return pData1->u.vwChar == pData2->u.vwChar ? TRUE : FALSE;
          break;
        default:
        ASSERT(0);
        return FALSE;
      }
    }

    // BSTR types
    else if ( nP2Pvar <= VBLockData_BSTR32 )
    {
      if ( pData1->u.vBlob32.nBlobUsed != pData2->u.vBlob32.nBlobUsed )
        return FALSE;
      return memcmp( &pData1->u.vBlob32.cBlob, &pData2->u.vBlob32.cBlob, pData1->u.vBlob32.nBlobUsed) ? FALSE : TRUE;
    }

    // WSTR types
    else if ( nP2Pvar <= VBLockData_WSTR16 )
    {
      if ( pData1->u.vBlob16.nBlobUsed != pData2->u.vBlob16.nBlobUsed )
        return FALSE;
      return memcmp( &pData1->u.vBlob16.cBlob, &pData2->u.vBlob16.cBlob, pData1->u.vBlob16.nBlobUsed) ? FALSE : TRUE;
    }
    else if ( nP2Pvar <= VBLockData_WSTR32 )
    {
      if ( pData1->u.vBlob32.nBlobUsed != pData2->u.vBlob32.nBlobUsed )
        return FALSE;
      return memcmp( &pData1->u.vBlob32.cBlob, &pData2->u.vBlob32.cBlob, pData1->u.vBlob32.nBlobUsed) ? FALSE : TRUE;
    }
   
    // BLOB
    else if ( nP2Pvar >= VBLockData_BLOB08 &&
              nP2Pvar <= VBLockData_BLOB32    )
    {
      if ( c_size() != rhs.c_size() )
        return FALSE;
      return memcmp( c_vBlob(), rhs.c_vBlob(), c_size()) ? FALSE : TRUE;
    }

    // GUID
    else if ( nP2Pvar == VBLockData_GUID )
      ASSERT(0);
    else
      ASSERT(0);
    return FALSE;
}

BOOL
P3PmsgData::operator != ( const P3PmsgData& rhs )
{
    return (*this == rhs) ? FALSE : TRUE;
}


//
//  Data exposure
char
P3PmsgData::c_char ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT08 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_char() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vInt08;
}
wchar_t
P3PmsgData::c_wchar ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_WCHAR )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_char() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vwChar;
}
short
P3PmsgData::c_short ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT16 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_short() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vInt16;
}
int
P3PmsgData::c_int ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( (pData->uDataAttr&VBLockAttr_NULL) == VBLockAttr_NULL )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( _T("c_int32() is null") )
            -> Throw  ( );
    if ( pData->uDataType != VBLockData_INT32 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_int32() type=%i (%s)", (int)pData->uDataType, ToStringType() )
            -> Throw  ( );
    return pData->u.vInt32;
}
bool
P3PmsgData::ReadAnyInt ( INT64& iOut, bool& bUnsigned ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    bUnsigned = false;
    switch ( pData->uDataType )
    {
    case VBLockData_INT08:  iOut = (INT64)pData->u.vInt08;                     return true;
    case VBLockData_UINT08: iOut = (INT64)pData->u.vuInt08; bUnsigned = true;  return true;
    case VBLockData_INT16:  iOut = (INT64)pData->u.vInt16;                     return true;
    case VBLockData_UINT16: iOut = (INT64)pData->u.vuInt16; bUnsigned = true;  return true;
    case VBLockData_INT32:  iOut = (INT64)pData->u.vInt32;                     return true;
    case VBLockData_UINT32: iOut = (INT64)pData->u.vuInt32; bUnsigned = true;  return true;
    case VBLockData_INT64:  iOut = (INT64)pData->u.vInt64;                     return true;
    case VBLockData_UINT64: iOut = (INT64)pData->u.vuInt64; bUnsigned = true;  return true;
    case VBLockData_BOOL:   iOut = pData->u.vBool ? 1 : 0;                     return true;
    default:                return false;
    }
}
INT64
P3PmsgData::c_int64 ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT64 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_int64() type (%s)", ToStringType() )
            -> Throw  ( );
    if ( (pData->uDataAttr&VBLockAttr_NULL) == VBLockAttr_NULL )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( _T("c_int64() is null") )
            -> Throw  ( );
    return pData->u.vInt64;
}
unsigned int
P3PmsgData::c_uint ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_UINT32 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_uint() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vuInt32;
}
UINT64
P3PmsgData::c_uint64 ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_UINT64 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_uint64() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vuInt64;
}
// c_time64 is the reader for BOTH 64-bit tags: there is no c_int64, so an
// INT64 cell has always been read through here, and a TIME64 cell -- what
// P3PmsgTime constructs -- must be too, or the specialisation cannot read
// back what it just wrote. u.vInt64 and u.vTime64 alias the same eight bytes.
__int64
P3PmsgData::c_time64 ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT64 &&
         pData->uDataType != VBLockData_TIME64   )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_time64() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vInt64;
}

// The 32-bit counterpart, declared beside c_time64 in P2Pmsg.h but never
// defined -- any caller was an unresolved external at link time. Same shape:
// TIME32 is the tag a 32-bit time carries, UINT32 the untagged equivalent.
unsigned int
P3PmsgData::c_time ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_TIME32 &&
         pData->uDataType != VBLockData_UINT32   )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_time() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vuInt32;
}

float
P3PmsgData::c_float ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_FLOAT )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_float() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vFloat;
}
double
P3PmsgData::c_double ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_DOUBLE )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_double() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vDouble;
}
bool
P3PmsgData::c_bool ( ) const
{
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_BOOL )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_bool() type (%s)", ToStringType() )
            -> Throw();
    return pData->u.vBool;
}

//
//  THE ALIGNMENT-SAFE WRITERS  (F-S4-3)
//
//  One per scalar, each the exact counterpart of the reference-returning form
//  above: same type check, same NULL-attribute clear, same SET_DIRTY, same
//  bytes landing in the same place at the same moment.  The only difference is
//  that the value is COPIED IN rather than written through a bound reference,
//  so no aligned type is ever made to point at an unaligned address.
//
//  Why memcpy through u.aAlloc rather than &u.vInt16: every member of a union
//  begins at the union's own address, and aAlloc is the union's char array, so
//  it names those same bytes through a type whose alignment requirement is 1.
//  Taking &u.vInt16 would reintroduce the very pointer this is avoiding, and
//  GCC's -Waddress-of-packed-member would say so.
//
//  These are WRITERS and return what they stored, so `x = d.c_int(5);` reads
//  naturally.  The reader is still the const overload, which was always safe.
//
char
P3PmsgData::c_char ( char vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT08 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_char() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
wchar_t
P3PmsgData::c_wchar ( wchar_t vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_WCHAR )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_wchar() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
short
P3PmsgData::c_short ( short vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT16 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_short() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
int
P3PmsgData::c_int ( int vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT32 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_int() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
INT64
P3PmsgData::c_int64 ( INT64 vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT64 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_int64() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
unsigned int
P3PmsgData::c_uint ( unsigned int vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_UINT32 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_uint() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
UINT64
P3PmsgData::c_uint64 ( UINT64 vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_UINT64 )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_uint64() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
unsigned int
P3PmsgData::c_time ( unsigned int vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_TIME32 &&
         pData->uDataType != VBLockData_UINT32   )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_time() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
__int64
P3PmsgData::c_time64 ( __int64 vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_INT64 &&
         pData->uDataType != VBLockData_TIME64   )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_time64() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
float
P3PmsgData::c_float ( float vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_FLOAT )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_float() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
double
P3PmsgData::c_double ( double vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_DOUBLE )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_double() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
bool
P3PmsgData::c_bool ( bool vNew )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_BOOL )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_bool() type (%s)", ToStringType() )
            -> Throw();
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    ::memcpy ( pData->u.aAlloc, &vNew, sizeof vNew );
    return vNew;
}
LPCSTR
P3PmsgData::c_str ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( !VBLockData_IsBSTR(pData) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_wstr() type (%s)", ToStringType() )
            -> Throw();
    return (LPCSTR)VBLockData_pcBlob(pData);
}
LPCWSTR
P3PmsgData::c_wstr() const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( !VBLockData_IsWSTR(pData) )
      EVERR ->Module(__FUNCTION__)
            ->Message( L"Incompatible c_wstr() type (%s)", ToStringType() )
            ->Throw();
    //  Stored as 16-bit P2PWCHAR; widen to wchar_t on read (§4.2). nBlobUsed is
    //  bytes for data cells, so char count = nBlobUsed / sizeof(P2PWCHAR).
    return p2p_wstr_from_store ( (const P2PWCHAR*)VBLockData_pcBlob(pData),
                                 VBLockData_BlobUsed(pData) / sizeof(P2PWCHAR) );
}
void*
P3PmsgData::c_vBlob ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( !VBLockData_IsBlob(pData) )
    {
      //  No ASSERT(0). uDataType is a byte off the wire, and c_wstr() a few
      //  lines up makes the same check against the same byte and simply throws
      //  -- so the refusal below IS the design, and asserting in front of it
      //  only decided that Debug aborts where Release refuses. Wire data
      //  reaching a check meant for this library's own heaps is the whole of
      //  D64; 38 of these in a 30-minute frame soak, and c_vBlobCopy and
      //  c_vGUID below carried the same construct.
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_vBlob() type (%s)", ToStringType() )
            -> Throw();
    }
    return VBLockData_pcBlob(pData);
}

//
//  Alignment-safe read of a blob payload
//  NOTES: c_vBlob() hands back a pointer INTO the packed image, and the image
//         guarantees it no alignment at all - the header is pack(1), a
//         VBLockHdr is 9 bytes under 64-bit addressing, and names are
//         variable-length, so a payload starts wherever the running sum puts
//         it.  Reading those bytes through a type that needs alignment is
//         undefined: it works on x86, UBSan reports it on Linux, and it
//         faults on a strict-alignment target.  This is TargetCore's finding
//         F-S5-3.
//       : memcpy is the whole mechanism, and it is the right one - it is
//         defined for any alignment and compiles to the same loads once the
//         compiler knows the size.  Nothing is widened or converted: the
//         caller gets the stored bytes, in order, in storage it aligned
//         itself.
//       : Returns what it COPIED, which is min(nCount, stored size), so a
//         caller that asked for more than is there can tell.  A short blob is
//         not an error - it is the answer.
//
//  Parameters:  void *pvOut     destination, aligned for whatever it is
//               size_t nCount   its capacity in bytes
//
//  Returns:     size_t          bytes copied
//
size_t
P3PmsgData::c_vBlobCopy ( void *pvOut, size_t nCount ) const
{
    if ( !pvOut || !nCount )
      return 0;

    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( !VBLockData_IsBlob(pData) )
    {
      // See c_vBlob above: the throw is the refusal, the assert only aborted Debug.
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_vBlobCopy() type (%s)", ToStringType() )
            -> Throw();
    }

    const void  *pvBlob = VBLockData_pcBlob ( pData );
    const size_t nStored = (size_t)VBLockData_BlobUsed ( pData );
    const size_t nCopy   = nStored < nCount ? nStored : nCount;
    if ( pvBlob && nCopy )
      ::memcpy ( pvOut, pvBlob, nCopy );
    return nCopy;
}
void*
P3PmsgData::c_vGUID ( ) const
{
    // A GUID is stored inline in the union's dedicated 16-byte vGUID member
    // (VBLockData.u.vGUID), not in the blob area, so return its address.
    // Declared in the header but historically never defined; added for the FS
    // "value" path.
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( pData->uDataType != VBLockData_GUID )
    {
      // See c_vBlob above: the throw is the refusal, the assert only aborted Debug.
      EVERR -> Module ( __FUNCTION__ )
            -> Message( L"Incompatible c_vGUID() type (%s)", ToStringType() )
            -> Throw();
    }
    return &pData->u.vGUID;
}
//
//  Writes a blob into this cell, growing the cell if it has to
//  NOTES: RESIZING IS VALID, and this is the answer to the ASSERT(0) that used
//         to stand where the widening below does.  That assert made the whole
//         grow branch unreachable in Debug, so it ran only in Release - where
//         the containment checks inside P2PmsgObject_NewVBLockData are also
//         compiled out and had nothing to catch either of the two defects it
//         carried.  What made it visible was sealing becoming a default:
//         sealing a relayed body rewrites the payload 234 bytes LARGER through
//         P2PeerMsg::SetData, which is the first thing in the tree that grows a
//         payload in place, so turning RequireSeal on by default pointed the
//         send path straight at this branch.
//       : THE SIZE PASSED TO NewVBLockData IS A VBLockData SIZE, not a payload
//         length.  This used to hand it nBlobSize, so the allocation was short
//         by the descriptor header - and worse, VBLockData_Init SUBTRACTS that
//         header from the figure it is given to derive the capacity, so the
//         recorded capacity came out short by the same amount and BlobCopy then
//         wrote the full payload into it.  P3PmsgData::operator= and c_memcpy
//         both compute VBLockData_Sizeof first; this now does too.
//       : WIDEN BEFORE MEASURING.  BlobMax is a property of the TYPE TAG - a
//         BLOB08 records its length in a UINT08 and so cannot describe 256
//         bytes however much heap is free.  Refusing was the old behaviour and
//         it made "replace this body with a bigger one" succeed or fail on how
//         big the body happened to be when the cell was first typed.  The tag
//         travels on the wire and every reader dispatches on it, so widening
//         needs no agreement between the ends.  The throw is kept for the case
//         that survives it: past a BLOB32 there is nothing left to widen to.
//       : InitBlob AFTER the allocation, from what was ACTUALLY allocated
//         rather than what was asked for, so the capacity matches the heap
//         rather than the request.  c_memcpy again.
void*
P3PmsgData::c_vBlob ( const void *pvBlob, size_t nBlobSize )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( VBLockData_BlobSize(pData) < nBlobSize )
    {
      const VBLockDataAttr uDataAttr = pData -> uDataAttr;
      const VBLockDataType uDataType =
              VBLockData_WidenBlob ( pData->uDataType, (VBLsize)nBlobSize );

      if ( VBLockData_BlobMaxOf ( uDataType ) < nBlobSize )
        EVERR -> Module ( "%s(%i)", __FUNCTION__, nBlobSize )
              -> Message("Buffer overrun (%i vs %i) blocked"
                        , nBlobSize, VBLockData_BlobMaxOf ( uDataType ) )
              -> Throw();

      const VBLsize nVBLockData_Size =
              VBLockData_Sizeof ( pOBJ__uVBLock, uDataType, (VBLsize)nBlobSize );
      pData = P2PmsgObject_NewVBLockData ( *pOBJ__, nVBLockData_Size );
      VBLockData_InitBlob ( pData, uDataAttr, uDataType
                          , P2PmsgObject_Sizeof_VBLockData(*pOBJ__) );
    }
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
    return VBLockData_BlobCopy ( pData, pvBlob, nBlobSize );
}
void*
P3PmsgData::c_vGUID ( const void *pvGUID, size_t nGUIDsize )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( VBLockData_BlobSize(pData) < nGUIDsize )
    {
      ASSERT(0);//Is resizing valid
      if ( VBLockData_BlobMax(pData) < nGUIDsize )
        EVERR -> Module ( "%s(%i)", __FUNCTION__, nGUIDsize )
              -> Message("Buffer overrun (%i vs %i) blocked"
                        , nGUIDsize, VBLockData_BlobMax(pData) )
              -> Throw();
      //TODO:LJM deprecated by below pData = NewVBLockData ( nCount );
      pData = P2PmsgObject_NewVBLockData ( *pOBJ__, nGUIDsize );
    }
    SET_DIRTY;
    return VBLockData_BlobCopy ( pData, pvGUID, nGUIDsize );
}
void*
P3PmsgData::c_memcpy ( const void *pvBlob, size_t nBlobSize )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( VBLockData_BlobSize(pData) < nBlobSize )
    {
//SSERT(VerifyContainment());
      VBLockDataType uDataType = pData -> uDataType;
      VBLockDataAttr uDataAttr = pData -> uDataAttr;
      if ( VBLockData_BlobMax(pData) < nBlobSize )
        EVERR -> Module ( "%s(%i)", __FUNCTION__, nBlobSize )
              -> Message("Buffer overrun (%i vs %i) blocked"
                        , nBlobSize, VBLockData_BlobMax(pData) )
              -> Throw();
      //TODO:LJM deprecated by below pData = NewVBLockData ( VBLockData_Sizeof(uVBLockType,nCount) );
      VBLsize nVBLockData_Size = VBLockData_Sizeof ( pOBJ__uVBLock, uDataType, nBlobSize);
      pData = P2PmsgObject_NewVBLockData ( *pOBJ__, nVBLockData_Size );
//ASSERT(VerifyContainment());
      VBLockData_InitBlob ( pData
                          , uDataAttr, uDataType
                          , P2PmsgObject_Sizeof_VBLockData(*pOBJ__) ); //VBLockData_Sizenn(pData) ); 2022/02/25
//ASSERT(VerifyContainment());
//ASSERT(VBLockData_BlobSize(pData)==nBlobSize);
    }
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
//ASSERT(VerifyContainment());
    void *pvDestin = VBLockData_BlobCopy ( pData, pvBlob, nBlobSize );
ASSERT(VerifyContainment());
    return pvDestin;
}
LPCWSTR
P3PmsgData::c_wcscpy ( LPCWSTR lpszSrc, size_t nCount )
{
    if ( nCount <= 0 &&
         lpszSrc         )
      nCount = wcslen(lpszSrc);
    ASSERT(nCount>=0&&nCount<32000);
//VBLockData *pData1 = P2PmsgObject_pData(*pOBJ__);
//VBLockData *pData2 = P2PmsgObject_pData(*pOBJ__,false);
    size_t      nUnits     = p2p_wide_units ( lpszSrc, nCount ); // UTF-16 units (astral -> 2)
    UINT_PTR    nCountBytes = nUnits * sizeof(P2PWCHAR);   // stored 16-bit units (§4.2)
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( VBLockData_BlobSize(pData) < nCountBytes )
    {
      VBLockDataType uDataType = pData -> uDataType;
      VBLockDataAttr uDataAttr = pData -> uDataAttr;
      if ( VBLockData_BlobMax(pData) < nCountBytes )
        EVERR -> Module ( "%s(%i)", __FUNCTION__, nCount )
              -> Message("Buffer overrun (%i vs %i) blocked"
                        , nCount, VBLockData_BlobMax(pData) )
              -> Throw();
      //TODO:LJM deprecated by below pData = NewVBLockData ( VBLockData_Sizeof(uVBLockType,nCount) );
      pData = P2PmsgObject_NewVBLockData ( *pOBJ__, VBLockData_Sizeof(pOBJ__uVBLock,uDataType,nCountBytes) );
      VBLockData_InitBlob ( pData
                          , uDataAttr, uDataType
                          , P2PmsgObject_Sizeof_VBLockData(*pOBJ__) ); //VBLockData_Sizenn(pData) ); 2022/02/25
    }
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    SET_DIRTY;
//AssertValid();
//VBLockData *pData11 = P2PmsgObject_pData(*pOBJ__);
//VBLockData *pData22 = P2PmsgObject_pData(*pOBJ__,false);
    //  Convert the in-memory wchar_t source to 16-bit P2PWCHAR before the raw
    //  BlobCopy, so the stored width is platform-independent (§4.2). On Windows
    //  P2PWCHAR==wchar_t, so vStore is a byte-identical copy and this is a no-op.
    std::vector<P2PWCHAR> vStore ( nCount ? nCount * 2 : 1 );   // astral surrogate worst case
    p2p_store_wide ( vStore.data(), lpszSrc, nCount );          // writes nUnits units
    void *pStored = VBLockData_BlobCopy ( pData, vStore.data(), nCountBytes );
    return p2p_wstr_from_store ( (const P2PWCHAR*)pStored, nUnits );
}
void*
P3PmsgData::c_memset ( int c, size_t nCount )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( VBLockData_BlobSize(pData) < nCount )
    {
      ASSERT(0);//Is resizing valid
      if ( VBLockData_BlobMax(pData) < nCount )
        EVERR -> Module ( __FUNCTION__ )
              -> AFP(c) -> AFP(nCount)
              -> Message("Buffer overrun (%i vs %i) blocked"
                        , nCount, VBLockData_BlobMax(pData) )
              -> Throw();
      //TODO:LJM deprecated by pData = NewVBLockData ( nCount );
      pData = P2PmsgObject_NewVBLockData ( *pOBJ__, nCount );
    }
    memset ( VBLockData_pcBlob(pData), c, VBLockData_BlobSize(pData) );
    return VBLockData_pcBlob(pData);
}
int
P3PmsgData::c_strcmp ( LPCSTR lpszCompare ) const
{
    return strcmp ( c_str(), lpszCompare );
}
int
P3PmsgData::c_wcscmp ( LPCWSTR lpszCompare ) const
{
    return wcscmp ( c_wstr(), lpszCompare );
}
int
P3PmsgData::c_stricmp ( LPCSTR lpszCompareNocase ) const
{
    return _stricmp ( c_str(), lpszCompareNocase );
}
int
P3PmsgData::c_wcsicmp ( LPCWSTR lpszCompareNocase ) const
{
    return _wcsicmp ( c_wstr(), lpszCompareNocase );
}

size_t
P3PmsgData::c_size ( ) const
{
ASSERT(1);
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    UCHAR       uDType = pData->uDataType;

    if (  uDType == VBLockData_NULL                           /*   ||
         (pData->uVBLockAttr&VBLockAttr_NULL) == VBLockAttr_NULL*/    ) //TODO:LJM 22/5/2014 debugging
      return 0;
    if ( uDType == VBLockData_INT32 )
      return sizeof(INT32);
    if ( uDType == VBLockData_UINT32 )
      return sizeof(UINT32);
    if ( uDType == VBLockData_INT16 )
      return sizeof(INT16);
    if ( uDType == VBLockData_UINT16 )
      return sizeof(UINT16);
    if ( uDType == VBLockData_INT64 )
      return sizeof(INT64);
    if ( uDType == VBLockData_UINT64 )
      return sizeof(UINT64);
    if ( uDType == VBLockData_FLOAT )
      return sizeof(float);
    if ( uDType == VBLockData_DOUBLE )
      return sizeof(double);
    if ( uDType == VBLockData_INT08 )
      return sizeof(INT08);
    if ( uDType == VBLockData_UINT08 )
      return sizeof(UINT08);
    if ( uDType == VBLockData_BOOL )
      return sizeof(bool);
    if ( uDType == VBLockData_WCHAR )
      return sizeof(wchar_t);

    return VBLockData_BlobUsed ( pData );
}

const P3PmsgObject*
P3PmsgData::p_Object ( ) const
{
ASSERT(m_pObject);
    return m_pObject;
}

///////////////////////////////////////////////////////////////////////
//  Operations

void
P3PmsgData::Nullify ( )
{
    VBLockData *pData  = P2PmsgObject_pData(*pOBJ__);
    if ( !(pData->uDataAttr&VBLockAttr_NULLABLE) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Non-nullable field" )
            -> Throw();
    pData -> uDataAttr |= VBLockAttr_NULL;
}
LPCTSTR
P3PmsgData::ToString ( LPCTSTR /*lpszFormat*/ )
{
    VBLockData *pData  = P2PmsgObject_pData(*pOBJ__);
    UCHAR       uDType = pData->uDataType;
    m_szToString[0]    = 0;

    if ( uDType == VBLockData_NULL )
      return _T("Null");
    if ( (pData->uDataAttr&VBLockAttr_NULL) == VBLockAttr_NULL )
      return _T("Null");
    if ( uDType == VBLockData_INT08 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%c"), c_char() );
    else if ( uDType == VBLockData_UINT08 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("0x%02x"), pData->u.vuInt08 );
    else if ( uDType == VBLockData_INT16 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%i"), c_short() );
    else if ( uDType == VBLockData_UINT16 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%iu"), pData->u.vuInt16 );
    else if ( uDType == VBLockData_INT32 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%i"), c_int() );
    else if ( uDType == VBLockData_UINT32 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%iu"), pData->u.vuInt32 );
    //else if ( uDType == VBLockData_LONG )
    //  sprintf ( m_szToString, _T("%i"), c_long() );
    else if ( uDType == VBLockData_INT64 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%I64i"), pData->u.vInt64 );
    else if ( uDType == VBLockData_UINT64 )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%I64iu"), pData->u.vuInt64 );
    else if ( uDType == VBLockData_FLOAT )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%g"), c_float() );
    else if ( uDType == VBLockData_DOUBLE )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%g"), c_double() );
    else if ( uDType == VBLockData_BOOL )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%i"), c_bool() );
    else if ( uDType == VBLockData_WCHAR )
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%c"), c_wchar() );
    else if ( uDType == VBLockData_TIME64 )
    {
      CTime oTime(pData->u.vTime64);
     _stprintf_s ( m_szToString, ARRAYSIZE(m_szToString), _T("%04i-%02i-%02i %02i:%02i:%02i")
                 , oTime.GetYear(), oTime.GetMonth(), oTime.GetDay()
                 , oTime.GetHour(), oTime.GetMinute(), oTime.GetSecond() );
    }
    // WSTR's
    else if ( uDType == VBLockData_WSTR08    ||
              uDType == VBLockData_WSTR08var ||
              uDType == VBLockData_WSTR16    ||
              uDType == VBLockData_WSTR16var ||
              uDType == VBLockData_WSTR32    ||
              uDType == VBLockData_WSTR32var    )
    {
      //CString strBSTRnn = c_wstr(); //TODO:LJM debugging
      return c_wstr();
      //swprintf_s ( m_szToString, ARRAYSIZE(m_szToString), L"%s", c_wstr() );
    }
    // BSTR's
    else if ( uDType == VBLockData_BSTR08    ||
              uDType == VBLockData_BSTR08var ||
              uDType == VBLockData_BSTR16    ||
              uDType == VBLockData_BSTR16var ||
              uDType == VBLockData_BSTR32    ||
              uDType == VBLockData_BSTR32var    )
    {
      CString strBSTRnn = c_str(); //TODO:LJM debugging
      swprintf_s ( m_szToString, ARRAYSIZE(m_szToString), L"%s", (LPCTSTR)strBSTRnn );
    }
                                       // Entry type Blob
    else if ( uDType == VBLockData_BLOB08 )
      return _T("...");
    else if ( uDType == VBLockData_BLOB08var )
      return _T("...");
    else if ( uDType == VBLockData_BLOB16 )
      return _T("...");
    else if ( uDType == VBLockData_BLOB16var )
      return _T("...");
    else if ( uDType == VBLockData_BLOB32 )
      return _T("...");
    else if ( uDType == VBLockData_BLOB32var )
      return _T("...");

    else if ( uDType == VBLockData_GUID )
      GuidToString ( pData->u.vGUID, m_szToString );
    else
      ASSERT(0);
    return m_szToString;
}

LPCTSTR
P3PmsgData::ToStringDefs ( LPCTSTR /*lpszFormat*/ )
{
    VBLockData *pData  = P2PmsgObject_pData(*pOBJ__);
    UCHAR       uDType = pData->uDataType;
    m_szToStrDef[0]    = 0;

    if ( uDType == VBLockData_NULL )
      return L"";
    if ( uDType == VBLockData_INT08 )
      return L"";
    if ( uDType == VBLockData_UINT08 )
      return L"";
    if ( uDType == VBLockData_INT16 )
      return L"";
    if ( uDType == VBLockData_UINT16 )
      return L"";
    if ( uDType == VBLockData_INT32 )
      return L"";
    if ( uDType == VBLockData_UINT32 )
      return L"";
    if ( uDType == VBLockData_INT64 )
      return L"";
    if ( uDType == VBLockData_UINT64 )
      return L"";
    //else if ( uDType == VBLockData_LONG )
    //  sprintf ( m_szToString, _T("%i"), c_long() );
    if ( uDType == VBLockData_FLOAT )
      return L"";
    if ( uDType == VBLockData_DOUBLE )
      return L"";
    if ( uDType == VBLockData_BOOL )
      return L"";
    if ( uDType == VBLockData_WCHAR )
      return L"";

    if ( uDType == VBLockData_WSTR08 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	                , pData->u.vBlob08.nBlobSize , pData->u.vBlob08.nBlobUsed );
    else if ( uDType == VBLockData_WSTR08var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob08.nBlobSize, pData->u.vBlob08.nBlobUsed );
    else if ( uDType == VBLockData_WSTR16 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob16.nBlobSize, pData->u.vBlob16.nBlobUsed );
    else if ( uDType == VBLockData_WSTR16var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob16.nBlobSize, pData->u.vBlob16.nBlobUsed );
    else if ( uDType == VBLockData_WSTR32 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob32.nBlobSize, pData->u.vBlob32.nBlobUsed );
    else if ( uDType == VBLockData_WSTR32var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob32.nBlobSize, pData->u.vBlob32.nBlobUsed );

    else if ( uDType == VBLockData_BSTR08 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	                , pData->u.vBlob08.nBlobSize , pData->u.vBlob08.nBlobUsed );
    else if ( uDType == VBLockData_BSTR08var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob08.nBlobSize, pData->u.vBlob08.nBlobUsed );
    else if ( uDType == VBLockData_BSTR16 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob16.nBlobSize, pData->u.vBlob16.nBlobUsed );
    else if ( uDType == VBLockData_BSTR16var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob16.nBlobSize, pData->u.vBlob16.nBlobUsed );
    else if ( uDType == VBLockData_BSTR32 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob32.nBlobSize, pData->u.vBlob32.nBlobUsed );
    else if ( uDType == VBLockData_BSTR32var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob32.nBlobSize, pData->u.vBlob32.nBlobUsed );
                                       // Entry type Blob
    else if ( uDType == VBLockData_BLOB08 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob08.nBlobSize, pData->u.vBlob08.nBlobUsed );
    else if ( uDType == VBLockData_BLOB08var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob08.nBlobSize, pData->u.vBlob08.nBlobUsed );
    else if ( uDType == VBLockData_BLOB16 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob16.nBlobSize, pData->u.vBlob16.nBlobUsed );
    else if ( uDType == VBLockData_BLOB16var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob16.nBlobSize, pData->u.vBlob16.nBlobUsed );
    else if ( uDType == VBLockData_BLOB32 )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob32.nBlobSize, pData->u.vBlob32.nBlobUsed );
    else if ( uDType == VBLockData_BLOB32var )
     _stprintf_s ( m_szToStrDef, ARRAYSIZE(m_szToStrDef), _T("%02i:%02i")
	               , pData->u.vBlob32.nBlobSize, pData->u.vBlob32.nBlobUsed );

    else if ( uDType == VBLockData_GUID )
      ASSERT(0);
    return m_szToStrDef;
}

LPCTSTR
P3PmsgData::ToStringType ( LPCTSTR lpszFormat ) const
{
    UNREFERENCED_PARAMETER(lpszFormat);
    VBLockData *pData   = P2PmsgObject_pData(*pOBJ__);
    UCHAR       uDType  = pData->uDataType;

    if ( uDType == VBLockData_NULL )
      return L"null";
    if ( uDType == VBLockData_INT08 )
      return L"char";
    if ( uDType == VBLockData_UINT08 )
      return L"uchar";
    if ( uDType == VBLockData_INT16 )
      return L"short";
    if ( uDType == VBLockData_UINT16 )
      return L"ushort";
    if ( uDType == VBLockData_INT32 )
      return L"int32";
    if ( uDType == VBLockData_UINT32 )
      return L"uint32";
    if ( uDType == VBLockData_INT64 )
      return L"int64";
    if ( uDType == VBLockData_UINT64 )
      return L"uint64";
    if ( uDType == VBLockData_FLOAT )
      return L"float";
    if ( uDType == VBLockData_DOUBLE )
      return L"double";
    if ( uDType == VBLockData_BOOL )
      return L"bool";
    if ( uDType == VBLockData_WCHAR )
      return L"wchar";
    if ( uDType == VBLockData_TIME64 )
      return L"Time64";

    if ( uDType == VBLockData_WSTR08 )
      return L"WSTR08";
    if ( uDType == VBLockData_WSTR08var )
      return L"WSTR08var";
    if ( uDType == VBLockData_WSTR16 )
      return L"WSTR16";
    if ( uDType == VBLockData_WSTR16var )
      return L"WSTR16var";
    if ( uDType == VBLockData_WSTR32 )
      return L"WSTR32";
    if ( uDType == VBLockData_WSTR32var )
      return L"WSTR32var";

    if ( uDType == VBLockData_BSTR08 )
      return L"BSTR08";
    if ( uDType == VBLockData_BSTR08var )
      return L"BSTR08var";
    if ( uDType == VBLockData_BSTR16 )
      return L"BSTR16";
    if ( uDType == VBLockData_BSTR16var )
      return L"BSTR16var";
    if ( uDType == VBLockData_BSTR32 )
      return L"BSTR32";
    if ( uDType == VBLockData_BSTR32var )
      return L"BSTR32var";
                                       // Entry type Blob
    if ( uDType == VBLockData_BLOB08 )
      return L"BLOB08";
    if ( uDType == VBLockData_BLOB08var )
      return L"BLOB08var";
    if ( uDType == VBLockData_BLOB16 )
      return L"BLOB16";
    if ( uDType == VBLockData_BLOB16var )
      return L"BLOB16var";
    if ( uDType == VBLockData_BLOB32 )
      return L"BLOB32";
    if ( uDType == VBLockData_BLOB32var )
      return L"BLOB32var";

    if ( uDType == VBLockData_GUID )
      return L"GUID";
    //  No ASSERT(0). This function's whole contract is "name this byte for a
    //  human", and "Undef" is the name it already has for a byte it cannot
    //  place. Every caller is a Message() building the text of a REFUSAL --
    //  including the c_vBlob/c_wstr family above, which reaches here precisely
    //  BECAUSE the type is unknown. So the assert fired on the error path of a
    //  correct refusal and aborted Debug in the act of reporting cleanly.
    return L"Undef";
}

///////////////////////////////////////////////////////////////////////
//  Troubleshooting
void
P3PmsgData::AssertValid ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);

    // Indirections
    while ( VBLockData_IsChained(pData) )
    {
      VBLaddr aVBLock = VBLockData_GetChain2Next ( m_pObject->m_uVBLock, pData );
      VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_pObject->m_hVBList, aVBLock );
      //VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_pObject->m_hVBList //TODO:LJM deprecated GetP2PmsgHandle()
      //                                        , pData->u.aVBLockData );
      if ( !VBLock_IsData(pVBLock) )
        EVERR -> Module ( __FUNCTION__ )
              -> Message("Expected indirect data reference" )
              -> Throw();
      pData = VBLock_pData ( pVBLock );
    }

    // Internal sizing
    if ( VBLockData_IsBlob(pData) )
    {
      VBLsize nBlobUsed = VBLockData_BlobUsed ( pData );
      VBLsize nBlobSize = VBLockData_BlobSize ( pData );
      if ( nBlobUsed > nBlobSize )
      EVERR 
            -> Message("Sizing corruption Used(%i) > Size(%i)"
                      , nBlobUsed, nBlobSize )
                      -> Module ( __FUNCTION__ ) //TODO:LJM
            -> Throw();
    }

    // Allocations
    if ( VBLockData_Sizeof_uv(pOBJ__uVBLock,pData) > P2PmsgObject_Sizeof_VBLockData(*pOBJ__) )
      EVERR
            -> Message("Sizing corruption Sizeof(%i) > Allocated(%i)"
                      , VBLockData_Sizeof_uv(pOBJ__uVBLock,pData)
                      , P2PmsgObject_Sizeof_VBLockData(*pOBJ__) )
                       -> Module ( __FUNCTION__ ) //TODO:LJM
            -> Throw();

    // Containment
    if ( !VerifyContainment() )
      EVERR -> MODULE
            -> Message ("Containment coruption ")
            -> Throw ( );
    // Indirections
    //while ( pData->uDataType == 0xFF )
    //{
    //  VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_pObject->m_hVBList //TODO:LJM deprecated GetP2PmsgHandle()
    //                                          , pData->u.aVBLockData );
    //  if ( !VBLock_IsData(pVBLock) )
    //    EVERR -> Module ( __FUNCTION__ )
    //          -> Message("Expected indirect data reference" )
    //          -> Throw();
    //  pData = VBLock_pData ( pVBLock );
    //}
}

//
//  Verifies P3PmsgData object containment and optionally containment of
//  VBLump within that space.
//
//  Parameters:  void *pvBlob
//               Blob pointer to be checked for containment
//
//               VBLsize nSizeofBlob
//               Optional size of pvBlob
//
//  Returns:     BOOL
//               Containment result
//                 TRUE... Contained
//                 FALSE.. Outside of containment area
BOOL
P3PmsgData::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // VBLockData containment within VBLock
    VBLock     *pVBLock = GetContainerVBLock ( false );
    VBLockData *pData   = P2PmsgObject_pData ( *pOBJ__, false );
    VBLsize     nVBLockDataMax = VBLockData_Sizeof_Alloc ( pVBLock );
    if ( !VBLock_IsContainedVBLump(pVBLock,pData,nVBLockDataMax) )
      return FALSE;                    // VBLockData not contained within VBLock
    // VBLockData containment within chained VBLock
    while ( VBLockData_IsChained(pData) )
    {
      const VBLaddr aChain2Next = VBLockData_GetChain2Next (pOBJ__uVBLock, pData );
      pVBLock = (VBLock *)pOBJ__Msg2Phys ( aChain2Next );
      pData   = VBLock_pData ( pVBLock );
      nVBLockDataMax = VBLockData_Sizeof_Alloc ( pVBLock );
      if ( !VBLock_IsContainedVBLump(pVBLock,pData,nVBLockDataMax) )
        return FALSE;                  // VBLockData not contained within VBLock             
      const VBLsize nDataSizeof = VBLockData_Sizeof ( pOBJ__uVBLock, pData );
      if ( !VBLock_IsContainedVBLump(pVBLock,pData,nDataSizeof) ) {
        return FALSE;
      }
    }
    // pvBlob containment
    if ( pvBlob && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;
    // Tidy up and
    return TRUE;
}

/*virtual void
  Print ( FILE *fd, int iDepthOS = 0  );*/

//
//  Properties
//
UCHAR
P3PmsgData::SetAttr ( UCHAR ucAttrAdd, UCHAR ucAttrRemove )
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    if ( ucAttrRemove )
      pData -> uDataAttr &= ~ucAttrRemove;
    if ( ucAttrAdd )
      pData -> uDataAttr |=  ucAttrAdd;
    SET_DIRTY;
    return pData -> uDataAttr;
}
UCHAR
P3PmsgData::GetAttr ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    return pData -> uDataAttr;
}
//
//  Actual size used by VBLockData component of VBLock
//  NOTES: Reference P2PmsgObject_Sizeof_VBLockData(pObj__) for total space available,
//         which may be greater than returned value
VBLsize
P3PmsgData::Sizeof ( ) const
{   
    const VBLockData *pData = P2PmsgObject_pData(*pOBJ__); //TODO:LJM deprectaed GetVBLockData();
    return VBLockData_Sizeof(pOBJ__uVBLock, pData);
/*  VBLockData *pData = GetVBLockData();
    VBLockType  nType = GetVBLockData()->uVBLockType;
    VBLockData  oData;
    UINT        nSize = sizeof(oData) - sizeof(oData.u);
    if ( nType == VBLockData_NULL )
      return nSize + sizeof(oData.u.aVBLockData);
    if ( nType <  VBLockData_BSTR08 )
    {
      if ( nType == VBLockData_CHAR )
        return nSize + sizeof(char);
      if ( nType == VBLockData_SHORT )
        return nSize + sizeof(short);
      if ( nType == VBLockData_INT )
        return nSize + sizeof(int);
      if ( nType == VBLockData_LONG )
        return nSize + sizeof(long);
      if ( nType == VBLockData_DOUBLE )
        return nSize + sizeof(double);
      ASSERT(0);
    }

    // BSTR's
    if ( nType < VBLockData_BLOB08 )
    {
      if ( nType == VBLockData_BSTR08 )
        return nSize + pData->u.vBlob08.nBlobUsed;
      if ( nType == VBLockData_BSTR08var )
        return nSize + pData->u.vBlob08.nBlobSize;
      if ( nType == VBLockData_BSTR16 )
        return nSize + pData->u.vBlob16.nBlobUsed;
      if ( nType == VBLockData_BSTR16var )
        return nSize + pData->u.vBlob16.nBlobSize;
      if ( nType == VBLockData_BSTR32 )
        return nSize + pData->u.vBlob32.nBlobUsed;
      if ( nType == VBLockData_BSTR32var )
        return nSize + pData->u.vBlob32.nBlobSize;
      ASSERT(0);
    }

    // BLOB's
    if ( nType < VBLockData_EODefs )
    {
      if ( nType == VBLockData_BLOB08 )
        return nSize + pData->u.vBlob08.nBlobUsed;
      if ( nType == VBLockData_BLOB08var )
        return nSize + pData->u.vBlob08.nBlobSize;
      if ( nType == VBLockData_BLOB16 )
        return nSize + pData->u.vBlob16.nBlobUsed;
      if ( nType == VBLockData_BLOB16var )
        return nSize + pData->u.vBlob16.nBlobSize;
      if ( nType == VBLockData_BLOB32 )
        return nSize + pData->u.vBlob32.nBlobUsed;
      if ( nType == VBLockData_BLOB32var )
        return nSize + pData->u.vBlob32.nBlobSize;
      ASSERT(0);
    }
    return 0;*/
}
bool
P3PmsgData::IsDirty ( ) const
{
    return m_bDataDirty;
}
bool
P3PmsgData::IsNull ( ) const
{
    VBLockData *pData = P2PmsgObject_pData(*pOBJ__);
    return (pData->uDataAttr&VBLockAttr_NULL) ? true : false;
}
VBLockType
P3PmsgData::DataType ( ) const
{
    return P2PmsgObject_pData(*pOBJ__)->uDataType;
}
//
//  Fetch VBLock containing the VBLockData structure
//  NOTES: May be a chained VBLock remote from original parent
VBLock*
P3PmsgData::GetContainerVBLock ( bool bIndirect ) const
{ 
    VBLock     *pVBLock = (VBLock *)m_pObject -> GetVBLock();
    VBLockData *pData   = VBLock_pData ( pVBLock );
    while ( bIndirect && VBLockData_IsChained(pData) )
    {
      const VBLaddr aChain2Next = VBLockData_GetChain2Next (m_pObject->m_uVBLock, pData );
      pVBLock = (VBLock *)m_pObject -> Msg2Phys ( aChain2Next );
      pData   = VBLock_pData ( pVBLock );
    }
    return pVBLock;
}

_variant_t
P2PmsgData_var ( const P3PmsgData& oData )
{
    VBLockData *pData   = P2PmsgData_pData(oData); //TODO:LJM deprecated oData.GetVBLockData();
    UCHAR       uDType  = pData->uDataType;

    if ( uDType == VBLockData_NULL )
      return _variant_t();
    if ( uDType == VBLockData_INT08 )
      return _variant_t(pData->u.vInt08);
    if ( uDType == VBLockData_UINT08 )
      return _variant_t(pData->u.vuInt08);
    if ( uDType == VBLockData_INT16 )
      return _variant_t(pData->u.vInt16);
    if ( uDType == VBLockData_UINT16 )
      return _variant_t(pData->u.vuInt16);
    if ( uDType == VBLockData_INT32 )
      return _variant_t(pData->u.vInt32);
    if ( uDType == VBLockData_UINT32 )
      return _variant_t(pData->u.vuInt32);
    if ( uDType == VBLockData_INT64 )
      return _variant_t(pData->u.vInt64);
    if ( uDType == VBLockData_UINT64 )
      return _variant_t(pData->u.vuInt64);
    if ( uDType == VBLockData_FLOAT )
      return _variant_t(pData->u.vFloat);
    if ( uDType == VBLockData_DOUBLE )
      return _variant_t(pData->u.vDouble);
    if ( uDType == VBLockData_BOOL )
      return _variant_t(pData->u.vBool);
    if ( uDType == VBLockData_WCHAR )
      return _variant_t(pData->u.vwChar);

    if ( uDType == VBLockData_WSTR08 )
      return _variant_t((wchar_t*)p2p_wstr_from_store(&pData->u.vBlob08.cBlob, pData->u.vBlob08.nBlobUsed));  // widen 16-bit store (§4.2)
    if ( uDType == VBLockData_WSTR08var )
      return _variant_t((wchar_t*)p2p_wstr_from_store(&pData->u.vBlob08.cBlob, pData->u.vBlob08.nBlobUsed));  // widen 16-bit store (§4.2)
    if ( uDType == VBLockData_WSTR16 )
      return _variant_t((wchar_t*)p2p_wstr_from_store(&pData->u.vBlob16.cBlob, pData->u.vBlob16.nBlobUsed));  // widen 16-bit store (§4.2)
    if ( uDType == VBLockData_WSTR16var )
      return _variant_t((wchar_t*)p2p_wstr_from_store(&pData->u.vBlob16.cBlob, pData->u.vBlob16.nBlobUsed));  // widen 16-bit store (§4.2)
    if ( uDType == VBLockData_WSTR32 )
      return _variant_t((wchar_t*)p2p_wstr_from_store(&pData->u.vBlob32.cBlob, pData->u.vBlob32.nBlobUsed));  // widen 16-bit store (§4.2)
    if ( uDType == VBLockData_WSTR32var )
      return _variant_t((wchar_t*)p2p_wstr_from_store(&pData->u.vBlob32.cBlob, pData->u.vBlob32.nBlobUsed));  // widen 16-bit store (§4.2)

    if ( uDType == VBLockData_BSTR08 )
      return _variant_t((char*)&pData->u.vBlob08.cBlob);
    if ( uDType == VBLockData_BSTR08var )
      return _variant_t((char*)&pData->u.vBlob08.cBlob);
    if ( uDType == VBLockData_BSTR16 )
      return _variant_t((char*)&pData->u.vBlob16.cBlob);
    if ( uDType == VBLockData_BSTR16var )
      return _variant_t((char*)&pData->u.vBlob16.cBlob);
    if ( uDType == VBLockData_BSTR32 )
      return _variant_t((char*)&pData->u.vBlob32.cBlob);
    if ( uDType == VBLockData_BSTR32var )
      return _variant_t((char*)&pData->u.vBlob32.cBlob);
                                       // Entry type Blob
    if ( uDType == VBLockData_BLOB08 )
      return _variant_t(L"BLOB08");
    if ( uDType == VBLockData_BLOB08var )
      return _variant_t(L"BLOB08var");
    if ( uDType == VBLockData_BLOB16 )
      return _variant_t(L"BLOB16");
    if ( uDType == VBLockData_BLOB16var )
      return _variant_t(L"BLOB16var");
    if ( uDType == VBLockData_BLOB32 )
      return _variant_t(L"BLOB32");
    if ( uDType == VBLockData_BLOB32var )
      return _variant_t(L"BLOB32var");

    if ( uDType == VBLockData_GUID )
    {
      VARIANT var;
      InitVariantFromGUIDAsBuffer ( pData->u.vGUID, &var );
      return _variant_t(var);
    }
    ASSERT(0);
    return _variant_t();
}

BOOL
P2PmsgData_var ( P3PmsgData& oData, const _variant_t& var )
{
    VBLockData *pData   = P2PmsgData_pData(oData); //TODO:LJM deprecated oData.GetVBLockData();
    UCHAR       uDType  = pData->uDataType;

    if ( uDType == VBLockData_NULL )
      return _variant_t();
    else if ( var.vt == VT_I1 )
      pData->u.vInt08 = var;
    else if ( var.vt == VT_UI1 )
      pData->u.vuInt08 = var;
    else if ( var.vt == VT_I2 )
      pData->u.vInt16 = var;
    else if ( var.vt == VT_UI2 )
      pData->u.vuInt16 = var;
    else if ( var.vt == VT_I4 )
      pData->u.vInt32 = var;
    else if ( var.vt == VT_UI4 )
      pData->u.vuInt32 = var;
    else if ( var.vt == VT_UINT )
      pData->u.vuInt32 = var;
    else if ( var.vt == VT_I8 )
      pData->u.vInt64 = var;
    else if ( var.vt == VT_UI8 )
      pData->u.vuInt64 = var;
    else if ( var.vt == VT_R4 )
      pData->u.vFloat = var;
    else if ( var.vt == VT_R8 )
      pData->u.vDouble = var;
    else if ( var.vt == VT_BOOL )
      pData->u.vBool = var;

    else if ( var.vt == VT_BSTR )
      oData.c_memcpy ( (const void *)((_bstr_t)var).GetBSTR(), ((_bstr_t)var).length()*sizeof(TCHAR) );
    //else if ( uDType == VBLockData_BSTR08var )
    //  oData.c_memcpy ( (const char *)((_bstr_t)var), 0 );
    //else if ( uDType == VBLockData_BSTR16 )
    //  oData.c_memcpy ( (const char *)((_bstr_t)var), 0 );
    //else if ( uDType == VBLockData_BSTR16var )
    //  oData.c_memcpy ( (const char *)((_bstr_t)var), 0 );
    //else if ( uDType == VBLockData_BSTR32 )
    //  oData.c_memcpy ( (const char *)((_bstr_t)var), 0 );
    //else if ( uDType == VBLockData_BSTR32var )
    //  oData.c_memcpy ( (const char *)((_bstr_t)var), 0 );
                                       // Entry type Blob
    //if ( var.vt == VBLockData_BLOB08 )
    //  return _variant_t("BLOB08");
    //if ( uDType == VBLockData_BLOB08var )
    //  return _variant_t("BLOB08var");
    //if ( uDType == VBLockData_BLOB16 )
    //  return _variant_t("BLOB16");
    //if ( uDType == VBLockData_BLOB16var )
    //  return _variant_t("BLOB16var");
    //if ( uDType == VBLockData_BLOB32 )
    //  return _variant_t("BLOB32");
    //if ( uDType == VBLockData_BLOB32var )
    //  return _variant_t("BLOB32var");
    else if ( var.vt == VT_CLSID )
      VariantToGUID ( var, &pData->u.vGUID );
    else
    ASSERT(0);
    return true;
}

BOOL
P3PmsgData_var ( P2PmsgHANDLE hVBListData, P2Pos nP2Pos, _variant_t& var )
{
    P3PmsgObject oObject;
    oObject.Connectx ( hVBListData, nP2Pos, 0 );

    if ( oObject.IsField() )
    {
      P3PmsgField oField ( oObject );
      P2PmsgData_var ( oField.r_data(), var );
    }
    //else if ( oObject.IsNode() )
    //{
    //  P3PmsgNode oNode ( oObject );
    //  P2PmsgData_var ( oNode.r_data(), var );
    //}
    else
      ASSERT(0);
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
//  P3PmsgTime specialisation
P3PmsgTime::P3PmsgTime ( )
{
    VBLockData *pData = P2PmsgObject_pData(*p_Object());
    pData -> uDataAttr |= VBLockAttr_NULL;
    pData -> uDataType  = VBLockData_TIME64;
}
P3PmsgTime::P3PmsgTime ( __int64 vTime64 )
{
    // A time constructed FROM a value is not null - clear the flag the
    // default construction path sets, the way every P3PmsgData scalar
    // constructor leaves it clear. Otherwise IsNull() reports true for a
    // timestamp that plainly has one.
    VBLockData *pData = P2PmsgObject_pData(*p_Object());
    pData -> uDataAttr &= ~VBLockAttr_NULL;
    pData -> uDataType  = VBLockData_TIME64;
    pData -> u.vTime64    = vTime64;
}
P3PmsgTime::P3PmsgTime ( const P3PmsgTime& rhs )
          : P3PmsgData ( rhs )
{
}
P3PmsgTime::~P3PmsgTime ( )
{
}
P3PmsgTime&
P3PmsgTime::operator = ( const P3PmsgTime& rhs )
{
    if ( this != &rhs )
      (P3PmsgData&)*this = (P3PmsgData&)rhs;
    return *this;    
}

///////////////////////////////////////////////////////////////////////
//  P3PmsgName object manager
//  NOTES: Acts as VBlockName wrapper

VBLockName*
P3PmsgName_GetVBLockName ( P3PmsgObject *pObject, bool bIndirect = true )
{
    VBLock      *pVBLock = ptrVBLOCK(*pObject); //pObject -> GetVBLock();
    ASSERT(VBLock_IsAlloc(pVBLock));
    //VBLockField *pField  = VBLock_pField ( pVBLock ); //TODO:LJM deprecated GetVBLockField ( );
    VBLockName  *pName   = VBLock_pName ( pVBLock );
    //VBLockName *pName = &GetVBLockField(bIndirect)->oVBLockName;

    while ( bIndirect && VBLockName_IsChained(pName) )
    {
      VBLaddr aVBLock1 = VBLockName_GetChain2Next ( pObject->m_uVBLock, pName );
      VBLock *pVBLock1 = (VBLock *)pObject-> Msg2Phys ( aVBLock1 );
   //ASSERT(pObject->m_uVBLock!=VBLock_Addr64);
   ASSERT(VBLock_IsLinked(pVBLock1));
   ASSERT(VBLock_IsName(pVBLock1));
      pName = VBLock_pName ( pVBLock1 );
    }
    return pName;
}

VBLockName*
P3PmsgName_ResizeName ( P3PmsgObject *pObject, size_t nSizeof )
{
    // Garbage collection for existing
    //VBLock     *pVBLock = pObject -> GetVBLock();
    VBLockName *pName0  = P3PmsgName_GetVBLockName ( pObject, false );
    UCHAR       uAttr   = pName0 -> uVBLockAttr; 
    UCHAR       uVBLock = pObject-> m_uVBLock;
    if ( VBLockName_IsChained(pName0) )
    {
      VBLaddr aVBLock1 = VBLockName_GetChain2Next ( uVBLock, pName0 );
      VBLock *pVBLock1 = (VBLock *)pObject-> Msg2Phys ( aVBLock1 );
              uAttr    = VBLock_pName ( pVBLock1 ) -> uVBLockAttr;
      //ASSERT(uVBLock!=VBLock_Addr64); becuase following code fails on cutovers
      //uAttr = VBLock_pName ( (VBLock *)pObject->Msg2Phys(pName0->u.vBlin08.aVBLockAddr) )
      //                 -> uVBLockAttr;
      //pObject -> Free ( VBLockName_GetChain2Next(uVBLock,pName0) );
      pObject -> Free ( aVBLock1 );
      VBLockName_SetChain2Next ( uVBLock, pName0, 0 );
      //pName0 -> u.vBlin08.aVBLockAddr = 0;
      pName0 -> uVBLockAttr           = uAttr;
    }

    // Allocate and lock in new
    // NOTES: Alloc can invalidate pointers
    VBLaddr     aName1 = pObject -> AllocVBLock ( VBLock_Name, nSizeof );
    pName0  = P3PmsgName_GetVBLockName ( pObject, false );
    //pName0 -> uVBLockAttr           = 0xFF;
    //pName0 -> u.vBlin08.aVBLockAddr = aName1;
    VBLockName_SetChain2Next ( uVBLock, pName0, aName1 );
    ASSERT(VBLockName_GetChain2Next(uVBLock,pName0) == aName1);
    VBLock *pVBLockName1 = (VBLock *)pObject -> Msg2Phys(aName1);
    pVBLockName1->oHdr.uVBLockDefs |= VBLock_Linked;  //TODO:LJM Added by 2/10/2013

    // Initialise new
    VBLockName *pName1 = P3PmsgName_GetVBLockName ( pObject, true );
//   //pName -> u.vBlin08.nBlobSize   = (UINT08)sizeof(pName->u.vBlin08.aVBLockAddr);
//   pName -> u.vBlin08.aVBLockAddr = OBJ__Alloc ( VBLock_Name, nSizeof );
//   pName = GetVBLockName ( );
//if(pName->uVBLockAttr == (UCHAR)~0 ) //TODO:Delete release testing only
//ASSERT(pName -> u.vBlin08.aVBLockAddr==0);
//   //pName = VBLock_pName ( (VBLock *)Msg2Phys(pName->u.vBlin08.aVBLockAddr) );
    VBLockName_Init ( uVBLock, pName1, uAttr, 0, nSizeof );
    ASSERT(nSizeof==VBLockName_Sizeof_Alloc(uVBLock,pName1));
    ASSERT(VBLock_IsLinked(pVBLockName1));
    ASSERT(VBLock_IsAlloc(pVBLockName1));
    return pName1;
}

//  Constructors and destructor
P3PmsgName::P3PmsgName ( const P3PmsgField *pField )
{
    pField;                            // Not used
    //m_pObject = 0;
    ASSERT(pField==0);
}
P3PmsgName::P3PmsgName ( ) noexcept
{
    RenderThisSafe ( );
}
P3PmsgName::P3PmsgName ( LPCTNAM lpszName, size_t nSize )
{
    RenderThisSafe ( );
    // Delegate to the c_name() setter, which enforces the 63-char cap and
    // resizes the destination blob when nSize exceeds its capacity before
    // copying. The previous inline copy truncated the source to 255 chars but
    // never resized the default vBlob08, so a name 64..255 chars long overran
    // it: the capacity check was a debug-only ASSERT, and the runtime guard
    // fired only AFTER the overflowing memcpy had corrupted the heap. c_name()
    // is also null-safe for lpszName, which the old _tcslen path was not.
    c_name ( lpszName, nSize );
}
P3PmsgName::~P3PmsgName ( )
{
    if ( pOBJ__ == nullptr )
      return;
    //if ( !pOBJ__hVBList &&
    //      pOBJ__aVBLock    )
    //  pOBJ__aVBLock = pOBJ__Free ( pOBJ__aVBLock );
    //if ( !m_hVBListData &&
    //      m_aData          )
    //  Free ( m_aData );
    //if ( m_hVBListData )
    //  VBListClose ( m_hVBListData );
    delete pOBJ__;
    //if ( m_pName )
    //  delete [] m_pName;
}
void
P3PmsgName::RenderThisSafe ( )
{
    ////m_pObject    = new P3PmsgObject ( );
    /////VBLock& oVBLock = *(VBLock*)m_pObject->m_oVBLock;      // Delegate field object
    m_bNameDirty = false;
    //m_hVBListData= 0;
    //m_uVBLock    = VBLock_Addr32;
    ////VBLock_Init    ( &oVBLock
    ////               , VBLock_Addr32|VBLock_Name|VBLock_Linked|VBLock_Alloc
    ////               , sizeof(oVBLock) );
    //TODO:LJM is this required VBLockItem_Init( m_uVBLock, VBLock_pItem(&oVBLock), VBLock_Data );
    /////VBLockName_Init( VBLock_pName(&oVBLock), VBLockAttr_DEFAULT
    /////               , 0, sizeof(oVBLock.ud.oName) );
    //m_xName      = 0;
    //m_aName      = (UINT)&oVBLock;
    //m_nNameSize  = sizeof(oVBLock.ud.oName);
    ///////m_pObject -> Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock.ud.oName) );
    //-------------------------------------------------------
    //m_pObject    = new P3PmsgObject ( );
    //VBLock& oVBLock = m_pObject->m_oVBLock;    // Delegate field object
    //m_bNameDirty   = false;
    //m_uVBLock    = VBLock_Addr32;
    //VBLock_Init    ( &oVBLock
    //               , VBLock_Addr32|VBLock_Name|VBLock_Linked|VBLock_Alloc
    //               , sizeof(oVBLock) );
    //VBLockName_Init( VBLock_pName(&oVBLock), VBLockAttr_DEFAULT, 0, sizeof(oVBLock.ud.oName) );
////UINT nSizeof = VBLockName_Sizeof(&m_oName);
////UINT nSizenn = VBLockName_Sizenn(&m_oName);
////UINT nSize   = sizeof(m_oName);

//ASSERT(VBLockName_Sizenn(&oVBLock)==sizeof(oVBLock.ud.oName));
    ////m_pName        = 0;
    //m_nNameSize    = sizeof(oVBLock);
    //m_pVBLockName  = VBLock_pName(&oVBLock);
    //m_pObject->Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock.ud.oName) );
    ASSERT(m_pObject==nullptr);
    m_pObject    = new P3PmsgObject ( );
    m_pObject -> ConnectVBLock ( );
    /*VBLock& oVBLock = *(VBLock*)m_pObject->m_oVBLock;      // Delegate field object
    //m_bDataDirty = false;
    //m_hVBListData= 0;
    //m_uVBLock    = VBLock_Addr32;
    VBLock_Init    (&oVBLock
                   , pOBJ__uVBLock|VBLock_Data|VBLock_Linked|VBLock_Alloc
                   , sizeof(oVBLock) ); TODO:LJM 64 bit migration*/
    //TODO:LJM is this required VBLockItem_Init( m_uVBLock, VBLock_pItem(&oVBLock), VBLock_Data );
    VBLock *pVBLock = (VBLock *)m_pObject->GetVBLock();
            pVBLock -> oHdr.uVBLockDefs |= VBLock_Name;
    VBLockName_Init( pOBJ__uVBLock,VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                   , 0, sizeof(pVBLock->ud.oName) );
    //m_pObject -> Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock.ud.oData) );
}

//  Operators
P3PmsgName&
P3PmsgName::operator = ( const P3PmsgName& rhs )
{
    if ( this == &rhs )
      return *this;
    //  Pass 0, not rhs.c_size(): c_name() hands back a NUL-terminated wide string whose
    //  ELEMENT count is what the c_name(name,nSize) setter iterates, but c_size() returns
    //  the stored UTF-16 UNIT count (nBlobUsed). On Windows those are equal (wchar_t IS a
    //  UTF-16 unit), so this is byte-identical there. On Linux an astral name widens back to
    //  fewer wchar_t elements than its stored units (a surrogate pair -> one code point), so
    //  passing the unit count over-iterated into the terminator and stored one spurious unit
    //  (§4.2). 0 makes the setter recompute the element count via wcslen — correct on both.
    c_name ( rhs.c_name(), 0 );
    return *this;
}

bool
P3PmsgName::operator == ( const P3PmsgName& rhs ) const
{
    size_t nSizeThis = c_size();
    size_t nSizeThat = rhs.c_size();
    if ( nSizeThis != nSizeThat )
      return false;
    return _tcsnicmp ( c_name(), rhs.c_name(), nSizeThis) == 0 ? true : false;
}
bool
P3PmsgName::operator == ( LPCTSTR rhs ) const
{
    size_t nSizeThis = c_size();
    if ( rhs == nullptr )
      return nSizeThis ? false : true;
    size_t nSize = _tcslen ( rhs );
    if ( nSizeThis != nSize )
      return false;
    return _tcsnicmp ( c_name(), rhs, nSize) == 0 ? true : false;
}

bool
P3PmsgName::operator < ( const P3PmsgName& rhs ) const
{
    size_t nSize = c_size();
    if ( nSize > rhs.c_size() )
      nSize = rhs.c_size();
    return _tcsnicmp ( c_name(),rhs.c_name(),nSize) < 0 ? true : false;
}

bool
P3PmsgName::operator > ( const P3PmsgName& rhs ) const
{
    size_t nSize = c_size();
    if ( nSize > rhs.c_size() )
      nSize = rhs.c_size();
    return _tcsnicmp ( c_name(),rhs.c_name(),nSize) < 0 ? false : true;
}

//  Name exposure
LPCTNAM
P3PmsgName::c_name ( LPCTNAM lpszName, size_t nSize )
{
   //ASSERT(VerifyContainment()); //delete-bug-hunting
    // To be sure, to be sure
    if ( nSize <= 0 && lpszName )
      nSize = wcslen(lpszName);
    //  Stored width is UTF-16 units, not code points: an astral char takes two units
    //  (§4.2). nUnits == nSize on Windows / for BMP names, so this is byte-identical there.
    size_t      nUnits  = p2p_wide_units ( lpszName, nSize );
    if ( nUnits > 63 )
      EVERR -> Module ( __FUNCTION__ )->AFP(lpszName)->AFP(nSize)
            -> Message("Attempted buffer overrun nSize(%i) > 64"
                      , nSize )
            -> Throw();
    UCHAR       uVBLock = m_pObject->m_uVBLock;
    VBLockName *pName   = P3PmsgName_GetVBLockName(m_pObject,true);
    const P2PWCHAR *lpszName1 = &pName->u.vBlob08.cBlob;   // ptr into 16-bit store (§4.2)
    if ( nUnits > pName->u.vBlob08.nBlobSize ) {
      VBLsize nSizeof = VBLockName_Sizeof ( uVBLock, nUnits );
      P3PmsgName_ResizeName ( m_pObject, nSizeof ); //TODO:LJM dprecated NewVBLockName ( nSizeof );
    }

    //VBLsize nSizeof = VBLockName_Sizeof(uVBLock,nSize); /*TODO:LJM was VBLockName_Sizeof()+nSize;*/
    //if ( VBLockName_Sizenn(pOBJ__uVBLock,P3PmsgName_GetVBLockName(m_pObject)) < nSizeof )
    //  P3PmsgName_ResizeName ( m_pObject, nSizeof ); //TODO:LJM dprecated NewVBLockName ( nSizeof );
    pName = P3PmsgName_GetVBLockName(m_pObject,true);
    ASSERT(nUnits <= pName->u.vBlob08.nBlobSize);
    if ( lpszName && nSize > 0 )
      p2p_store_wide ( &pName->u.vBlob08.cBlob, lpszName, nSize );   // wchar->16-bit, nUnits written (§4.2)
    { wchar_t z = 0; p2p_store_wide ( &pName->u.vBlob08.cBlob + nUnits, &z, 1 ); } // NUL after nUnits units (§4.2)
      pName->u.vBlob08.nBlobUsed     = (UINT08)nUnits;
    m_bNameDirty = true;
   ASSERT(VerifyContainment()); //delete-bug-hunting
    return (LPCTNAM)&pName->u.vBlob08.cBlob;
}

LPCTNAM
P3PmsgName::c_name ( ) const
{
    //  Name stored as 16-bit P2PWCHAR; widen to wchar_t on read (§4.2). For names
    //  nBlobUsed is the character count (set by the name-write path above).
    VBLockName *pName = P3PmsgName_GetVBLockName(m_pObject);
    return (LPCTNAM)p2p_wstr_from_store ( &pName->u.vBlob08.cBlob, pName->u.vBlob08.nBlobUsed );
}
UCHAR
P3PmsgName::c_size ( ) const
{
    return P3PmsgName_GetVBLockName(m_pObject)->u.vBlob08.nBlobUsed;
}
int
P3PmsgName::c_strcmp ( LPCSTR lpszCompare ) const
{
    USES_CONVERSION;
    ASSERT(0);
    return strcmp ( W2A(c_name()), lpszCompare );
}
int
P3PmsgName::c_wcscmp ( LPCWSTR lpszCompare ) const
{
    return wcscmp ( c_name(), lpszCompare );
}
int
P3PmsgName::c_wcsicmp ( LPCWSTR lpszCompareNocase ) const
{
    LPCTSTR lpszName = c_name();
    return _wcsicmp ( lpszName, lpszCompareNocase );
}
int
P3PmsgName::c_wcsicmpWC ( LPCWSTR lpszCompareNocase ) const
{
    LPCTSTR lpszName = c_name();
    return MsgcoreWildcard ( lpszCompareNocase, lpszName );
}
int
P3PmsgName::c_stricmp ( LPCSTR lpszCompareNocase ) const
{
    USES_CONVERSION;
    ASSERT(0);
    return _stricmp ( W2A(c_name()), lpszCompareNocase );
}

//  Addressing
//VBLockName*
//P3PmsgName::NewVBLockName ( int nSizeof )
//{
//    ASSERT(0);
//    UCHAR uVBLockAttr = VBLockAttr_DEFAULT;
//    if ( m_pName )
//    {
//      uVBLockAttr = m_pVBLockName -> uVBLockAttr;
//      delete [] m_pName;
//                m_pName = 0;
//    }
//    m_pName       = new char [nSizeof];
//    m_nNameSize   = nSizeof;
//    m_pVBLockName = (VBLockName *)m_pName;
//    m_bNameDirty  = false;
//    VBLockName_Init( m_pVBLockName, uVBLockAttr, 0, sizeof(m_oName) );
//    return m_pVBLockName;
//}
VBLockName*
P3PmsgField_NewVBLockName_ ( P3PmsgObject& oObject, int nSizeof )
{
   ASSERT(0); // Deprecated, plus legacy and NOT fully upgraded, tagged for deletion
   // Garbage collection for existing
   VBLockName *pName0  = P2PmsgObject_pName(oObject,false); //TODO:LJM deprecated GetVBLockName ( false );
   UCHAR       uAttr   = pName0 -> uVBLockAttr; 
   UCHAR       uVBLock = oObject.m_uVBLock;
   if ( VBLockName_IsChained(pName0) )
   {
     uAttr = VBLock_pName ( (VBLock *)oObject.Msg2Phys(pName0->u.vBlin08.aVBLockAddr) )
                      -> uVBLockAttr;
     oObject.Free ( pName0->u.vBlin08.aVBLockAddr );
     //pName0 -> u.vBlin08.aVBLockAddr = 0;
     VBLockName_SetChain2Next ( uVBLock, pName0, 0 );
     pName0 -> uVBLockAttr           = uAttr;
   }

   // Allocate and lock in new
   VBLaddr aName1 = oObject.AllocVBLock ( VBLock_Name, nSizeof );
   pName0 = P2PmsgObject_pName ( oObject, false ); //TODO:LJM deprecated GetVBLockName ( false );
   pName0 -> uVBLockAttr           = 0xFF;
   //pName0 -> u.vBlin08.aVBLockAddr = aName1;
   VBLockName_SetChain2Next ( uVBLock, pName0, aName1 );
   ASSERT((pName0->u.vBlin08.aVBLockAddr&0xFFFFFFFF) == aName1);

   // Initialise new
   VBLock     *pVBLockName1 = (VBLock *)oObject.Msg2Phys ( aName1 );
   //if ( !VBLock_IsLinked(pVBLockName1) )
   //  pVBLockName1->oHdr.uVBLockDefs |= VBLock_Linked;
   VBLockName *pName1 = VBLock_pName ( pVBLockName1 );

   //VBLockName *pName1 = P2PmsgObject_pName(oObject); //TODO:LJM deprecated GetVBLockName ( );
//   //pName -> u.vBlin08.nBlobSize   = (UINT08)sizeof(pName->u.vBlin08.aVBLockAddr);
//   pName -> u.vBlin08.aVBLockAddr = OBJ__Alloc ( VBLock_Name, nSizeof );
//   pName = GetVBLockName ( );
//if(pName->uVBLockAttr == (UCHAR)~0 ) //TODO:Delete release testing only
//ASSERT(pName -> u.vBlin08.aVBLockAddr==0);
//   //pName = VBLock_pName ( (VBLock *)Msg2Phys(pName->u.vBlin08.aVBLockAddr) );
   VBLockName_Init ( uVBLock, pName1, uAttr, 0, nSizeof );
   ASSERT(VBLock_IsLinked(pVBLockName1));
   return pName1;
}

///////////////////////////////////////////////////////////////////////
//  Troubleshooting
void
P3PmsgName::AssertValid ( ) const
{
    VBLockName *pName = P3PmsgName_GetVBLockName(m_pObject,false);
//LPCTSTR lpszName=c_name();

    // Indirections
    while ( VBLockName_IsChained(pName) )
    {
      VBLaddr aVBLock = VBLockName_GetChain2Next ( m_pObject->m_uVBLock, pName );
      VBLock *pVBLock = (VBLock *)m_pObject-> Msg2Phys ( aVBLock );
      //VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_pObject->m_hVBList //TODO:LJM deprecated GetP2PmsgHandle()
      //                               , pName->u.vBlin08.aVBLockAddr );
      if ( !VBLock_IsName(pVBLock) )
        EVERR -> Module ( __FUNCTION__ )
              -> Message("Expected indirect name reference" )
              -> Throw();
      pName = VBLock_pName ( pVBLock );
    }

    // Internal sizing
    if ( pName->u.vBlob08.nBlobUsed > pName->u.vBlob08.nBlobSize )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Sizing corruption Used(%i) > Size(%i)"
                      , pName->u.vBlob08.nBlobSize
                      , pName->u.vBlob08.nBlobUsed )
            -> Throw();

    // Allocations
    if ( VBLockName_Sizeof(pOBJ__uVBLock,pName) > P2PmsgObject_VBLockNameSize(*m_pObject) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Sizing corruption Sizeof(%i) > Allocated(%i)"
                      , VBLockName_Sizeof(pOBJ__uVBLock,pName)
                      , P2PmsgObject_VBLockNameSize(*m_pObject) )
            -> Throw();
}
/*virtual void
  Print ( FILE *fd, int iDepthOS = 0  );*/

//
//  Verifies P3PmsgName object containment and optionally containment of
//  VBLump within that space.
//
//  Parameters:  void *pvBlob
//               Blob pointer to be checked for containment
//
//               VBLsize nSizeofBlob
//               Optional size of pvBlob
//
//  Returns:     BOOL
//               Containment result
//                 TRUE... Contained
//                 FALSE.. Outside of containment area
BOOL
P3PmsgName::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // VBLockName containment within VBLock
    VBLock     *pVBLock = GetContainerVBLock ( false );
    VBLockName *pName   = P2PmsgObject_pName ( *pOBJ__, false );
    VBLsize     nVBLockNameMax = VBLock_Sizeof_Name ( pVBLock );
    if ( !VBLock_IsContainedVBLump(pVBLock,pName,nVBLockNameMax) )   //#######THIS IS A PROBLEM
      return FALSE;                    // VBLockName not contained within VBLock
    // VBLockName containment within chained VBLock
    while ( VBLockName_IsChained(pName) )
    {
       const VBLaddr aChain2Next = VBLockName_GetChain2Next (pOBJ__uVBLock, pName );
       pVBLock = (VBLock *)pOBJ__Msg2Phys ( aChain2Next );
       pName   = VBLock_pName ( pVBLock );
       nVBLockNameMax = VBLock_Sizeof_Name ( pVBLock );
       if ( !VBLock_IsContainedVBLump(pVBLock,pName,nVBLockNameMax) )
         return FALSE;                  // VBLockName not contained within VBLock             
       const VBLsize nSizeofAlloc = VBLockName_Sizeof_Alloc ( pOBJ__uVBLock, pName );
       if ( !VBLock_IsContainedVBLump(pVBLock,pName,nSizeofAlloc) ) {
         return FALSE;
      }
    }
    // pvBlob containment
    if ( pvBlob && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;
    // Tidy up and
    return TRUE;
}

//
//  Properties

//  Sizeof the encapsulated VBLockName structure
VBLsize
P3PmsgName::Sizeof ( ) const
{   
    VBLockName *pName = P3PmsgName_GetVBLockName(m_pObject);
    return VBLockName_Sizeof(pOBJ__uVBLock,pName);
}
bool
P3PmsgName::IsDirty ( ) const noexcept
{
    return m_bNameDirty;
}
//
//  Fetch VBLock containing the VBLockName structure
//  NOTES: May be a chained VBLock remote from original parent
VBLock*
P3PmsgName::GetContainerVBLock ( bool bIndirect ) const
{ 
    VBLock     *pVBLock = (VBLock *)m_pObject -> GetVBLock();
    VBLockName *pName   = VBLock_pName ( pVBLock );
    while ( bIndirect && VBLockName_IsChained(pName) )
    {
      const VBLaddr aChain2Next = VBLockName_GetChain2Next (m_pObject->m_uVBLock, pName );
      pVBLock = (VBLock *)m_pObject -> Msg2Phys ( aChain2Next );
      pName   = VBLock_pName ( pVBLock );
    }
    return pVBLock;
}

///////////////////////////////////////////////////////////////////////////////
//  P2PmsgObject implementation
//  NOTES: Base class for P2Pmsg units, aka VBLock objects

static VBLockField*
P2PmsgObject_pField ( const P3PmsgObject& oObject );

//
//  Contructors and destructor
P3PmsgObject::P3PmsgObject ( ) noexcept
{
}

P3PmsgObject::P3PmsgObject ( UCHAR uVBLock )
{
UNREFERENCED_PARAMETER(uVBLock);
ASSERT(0);
}

//  Copy construction
//  NOTES: A VOID source must copy as VOID. Without the middle arm below the
//         else branch ran for a void rhs as well, pointing m_aVBLock at THIS
//         object's inline block - so the copy came out with m_hVBList==0 but
//         m_aVBLock!=0, which IsVoid() (m_hVBList==0 && m_aVBLock==0) reports
//         as NOT void.
//       : What that broke is every "not found" answer in the API, because they
//         are returned BY VALUE: P3Pmsg_FindChildWithAttr, _FindParentWithAttr,
//         _FindParentAttr and _SelectObject all report failure as a void
//         P3PmsgObject, and the caller's copy was not void. Measured with the
//         function instrumented: it printed "return C (oObject void=1)" while
//         the caller printed "void=0 empty=1" for the same object.
//       : Connect() - which operator=() delegates to - has taken a void source
//         to Nullify() since [LJM][2025-02-18] (P2Pmsg.cpp:2365-2372). So
//         `oA = oB` was already right and `P3PmsgObject oA = oB` was not; the
//         two paths now agree, and the discriminator here is deliberately the
//         same one Connect uses.
//       : Standalone objects are unaffected. ConnectVBLock() always sets a
//         non-zero m_nVBLockSize (it asserts the size equals sizeof(m_oVBLock)),
//         so P3PmsgData / P3PmsgName - which use that mode extensively - still
//         take the inline-copy branch.
P3PmsgObject::P3PmsgObject ( const P3PmsgObject& rhs )
{
    //  AN ITEM IS REHOMED BEFORE IT IS SHARED, and this is the only place a
    //  floating one is ever asked for by value -- P3Pmsg_SelectObject and every
    //  other "here is the object you asked for" returns one. Without it the
    //  answer is a DUPLICATE of the floating item rather than the item: its
    //  P2Pos differs and a write through it does not reach the original.
    //  RehomeInlineItem does nothing to a value block, so P3PmsgData and
    //  P3PmsgName still take the inline-copy branch below (refer its NOTES).
    //  Casting away const to do it is what Connect already does, one arm down,
    //  and for the same reason: the two handles can only name one block if the
    //  block moves out of the object that built it.
    //  ON THE STATE OF THE BLOCK, NOT THE STATE OF THE HEAP. The guard was
    //  `rhs.m_hVBList == 0`, and an object can hold a heap and still address
    //  its own inline array -- refer RehomeInlineItem. On that state nothing
    //  was rehomed, the AddRef arm below ran, and m_aVBLock was copied
    //  verbatim: a pointer into the SOURCE OBJECT. Where the source was a
    //  local, the copy outlived it and read a dead stack frame. §22.
    if ( rhs.m_nVBLockSize != 0 &&
         rhs.m_aVBLock == (VBLaddr)&rhs.m_oVBLock[0] )
      ((P3PmsgObject&)rhs).RehomeInlineItem ( );

    m_uVBLock     = rhs.m_uVBLock;
    m_aVBLock     = rhs.m_aVBLock;
    m_xVBLock     = rhs.m_xVBLock;
    m_nVBLockSize = rhs.m_nVBLockSize;
    m_hVBList     = rhs.m_hVBList;
    if ( m_hVBList )
      P2PmsgHeap_AddRef ( m_hVBList );

    if ( rhs.m_nVBLockSize == 0 && rhs.m_hVBList == 0 )
    {
      // Void: no heap and no inline block to copy. Stay void.
      m_aVBLock = 0;
      m_xVBLock = 0;
    }
    else if ( rhs.m_aVBLock == (VBLaddr)&rhs.m_oVBLock[0] )
    {
      // Still inline, so RehomeInlineItem declined it: a VALUE block and not
      // an item. Copy it, and address OUR copy of it -- never the source's,
      // which is the whole of the defect above. The heap, if there is one,
      // holds the payload the block points at and has been AddRef'd already.
      m_aVBLock = (VBLaddr)&m_oVBLock[0];
      m_xVBLock = 0;
      memcpy ( m_oVBLock, (void*)&rhs.m_oVBLock[0], sizeof(m_oVBLock) );

      //  AND OUR OWN COPY OF WHAT IT POINTS AT. The memcpy above copies the
      //  block; a value that has outgrown it does not keep its payload IN the
      //  block, it keeps a chain pointer to a second block on the heap -- so
      //  the memcpy copies the pointer and both objects name one payload. A
      //  write through either was then seen by the other, and the first of
      //  them to retype FREED it under the other. §23.
      PrivatiseInlineChain ( );
    }
}
P3PmsgObject::~P3PmsgObject ( )
{
    ReleaseInlineChain ( );            // ... before the heap it is on is closed
    if ( m_hVBList )
      P2PmsgHeap_Close ( m_hVBList );
}
void
P3PmsgObject::Nullify ( )
{
    ReleaseInlineChain ( );            // ... before the heap it is on is closed
    if ( m_hVBList )
      P2PmsgHeap_Close ( m_hVBList );
    m_hVBList     = 0;
    m_uVBLock     = VBLock_Addrxx;
    m_aVBLock     = 0;
    m_xVBLock     = 0;
    m_nVBLockSize = 0;
}
//
//  Connects to the internal VBLock (aka m_oVBLock)
//  NOTES: Effectively standalone instanciation.  P3PmsgData and
//         P3PmsgName use extensively
void
P3PmsgObject::ConnectVBLock ( )
{
    ASSERT(m_hVBList==0&&m_aVBLock==0);
    m_uVBLock     = VBLock_Addrxx;
    m_aVBLock     = (VBLaddr)&m_oVBLock[0];
    VBLock_Init ( (VBLock *)&m_oVBLock[0]
                , VBLock_Addrxx|VBLock_Linked|VBLock_Alloc
                , sizeof(m_oVBLock) );
    m_nVBLockSize = VBLock_Hdr_u_SizeNN((VBLock*)&m_oVBLock);
ASSERT(m_nVBLockSize==sizeof(m_oVBLock));
    m_xVBLock     = 0;
}
void
P3PmsgObject::Connect ( const P3PmsgObject& oObject )
{
    if ( this == &oObject )
      return;
    // Void P3PmsgObject, no data [LJM][2025-02-18]
    // NOTES: Both P3PmsgObject's end up empty with no connection
    if ( oObject.m_hVBList     == 0  &&
         oObject.m_nVBLockSize == 0    )
    {
      Nullify();
      return;
    }
    // Cannot share heap that does not exist
    // NOTES: Create heap and place data on heap
    //      : Which is what this arm said and did not do. It created the heap
    //        and left m_aVBLock pointing into the SOURCE's inline storage,
    //        then copied that pointer below as though it were an address on
    //        the new heap. A SYS heap addresses by raw pointer, so the result
    //        reads correctly and dangles the moment the source goes out of
    //        scope. The ASSERT beneath it said so -- "It's a bug should this
    //        occur", on a condition the guard above has already excluded, so
    //        every path that reached here asserted.
    //      : RehomeInlineItem is the "place data on heap" half. It answers 0
    //        for a block that is not an item, and a value block is duplicated
    //        instead -- the same thing the copy constructor does with one, so
    //        that `oA = oB` and `P3PmsgObject oA = oB` agree. They are
    //        deliberately kept in step; refer the copy constructor's NOTES.
    //      : ASKED OF THE BLOCK, NOT OF THE HEAP -- the copy constructor's
    //        correction, and for the identical reason. `oObject.m_hVBList == 0`
    //        skipped an object that has a heap and an inline block both, and
    //        the share below then took the source's own address. §22.
    if ( oObject.m_nVBLockSize != 0 &&
         oObject.m_aVBLock == (VBLaddr)&oObject.m_oVBLock[0] )
    {
      P3PmsgObject& oObj = (P3PmsgObject&)oObject;
      if ( oObj.RehomeInlineItem ( ) == 0 )
      {
        // A value block. AddRef before Nullify: the heap it names may be the
        // one this object is about to let go of.
        P2PmsgHANDLE hVBList = oObject.m_hVBList;
        if ( hVBList )
          P2PmsgHeap_AddRef ( hVBList );
        Nullify ( );
        m_hVBList     = hVBList;
        m_uVBLock     = oObject.m_uVBLock;
        m_aVBLock     = (VBLaddr)&m_oVBLock[0];
        m_xVBLock     = 0;
        m_nVBLockSize = oObject.m_nVBLockSize;
        memcpy ( m_oVBLock, (void*)&oObject.m_oVBLock[0], sizeof(m_oVBLock) );
        PrivatiseInlineChain ( );      // ... and of what it points at. §23.
        return;
      }
    }

    //  This object is about to stop naming whatever it names, so give back an
    //  inline chain first -- while m_hVBList is still the heap the chain is on.
    //  The VALUE arm above reaches this through Nullify(); the share arm below
    //  overwrites m_aVBLock outright and would otherwise leave the block on a
    //  heap that outlives the change.
    ReleaseInlineChain ( );

    P2PmsgHANDLE hVBListClose = m_hVBList;
    m_hVBList = oObject.m_hVBList;
    if ( m_hVBList )
      m_hVBList = P2PmsgHeap_AddRef ( m_hVBList );
    m_uVBLock = oObject.m_uVBLock;

    if ( hVBListClose )
      P2PmsgHeap_Close ( hVBListClose );
    m_xVBLock     = oObject.m_xVBLock;
    m_aVBLock     = oObject.m_aVBLock;
    m_nVBLockSize = oObject.m_nVBLockSize;
 
if(m_aVBLock&&m_aVBLock!=(VBLaddr)&m_oVBLock)ASSERT(m_hVBList);//TODO:LJM debugging
}
void
P3PmsgObject::Connecta ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nVBLockSize )
{
    ReleaseInlineChain ( );            // Before m_aVBLock stops naming it
    m_xVBLock     = 0;                 // Flags life cycle managed internally
    m_aVBLock     = aVBLock;
    m_nVBLockSize = nVBLockSize;
    if ( hVBList == m_hVBList )
      return;
    
    P2PmsgHANDLE hVBListClose = m_hVBList;
    m_hVBList = hVBList;
    if ( m_hVBList )
      P2PmsgHeap_AddRef ( m_hVBList );
    m_uVBLock = VBLock_Addrxx;
    ASSERT(m_hVBList);
    if ( m_hVBList )
      m_uVBLock = P2PmsgHeap_Addrnn ( hVBList );
    if (   hVBListClose )
      P2PmsgHeap_Close ( hVBListClose );
if(m_aVBLock!=(VBLaddr)&m_oVBLock)ASSERT(m_hVBList);//TODO:LJM debugging
}
void
P3PmsgObject::Connectx ( P2PmsgHANDLE hVBList, VBLaddr xVBLock, VBLsize nVBLockSize )
{
    Connecta ( hVBList, xVBLock, nVBLockSize );
    m_xVBLock     = xVBLock;           // Flags life cycle managed externally
    m_nVBLockSize = nVBLockSize;
    if ( hVBList && m_xVBLock )
    {
      // Span-checked, because m_aVBLock reached us from the image rather than
      // from an allocation this process made -- on the load path it is a root
      // offset read straight out of the file. VBLock_Hdr_u_SizeNN reads the
      // whole VBLockHdr, so the header has to FIT, not merely start in bounds:
      // the plain Addr2Phys accepted an offset one byte under the limit and let
      // the size field be read off the end. That is the tail of finding F1, the
      // part the declared-size check at the front door does not cover.
      //
      // F9. This block used to be guarded by `m_nVBLockSize <= 0` as well, so
      // the bound ran only when the caller asked this function to WORK OUT the
      // size and was skipped entirely when the caller supplied one. Those are
      // the same address either way -- an offset out of an image -- and whether
      // the caller happens to know the block's size says nothing about whether
      // its address is inside the arena. So a caller passing a size connected an
      // object to an unbounded offset, and the first accessor to follow it
      // (P2PmsgObject_pData -> VBLock_pData) read the header byte off the end.
      // Found by the receive-path fuzzer, on the same input as F8 and one
      // function further in: fixing F8 did not close it, it exposed it. That is
      // F2's sequencing all over again, and F3's lesson about forks -- the
      // guard was on one branch of this `if` and the other branch had never been
      // walked with a hostile address.
      //
      // Only the SIZE DERIVATION stays conditional. The bound does not.
      const VBLock *pVBLock
        = (const VBLock*)P2PmsgHeap_Addr2PhysChk ( m_hVBList, m_aVBLock
                                                 , sizeof(VBLockHdr) );
      // Unreachable while m_xVBLock is non-zero, which the guard above tests --
      // but this is a hostile-input path, and "the caller already checked" is
      // how the branch below got its bug.
      if ( pVBLock == nullptr )
        EVERR->MODULE
             ->Message(L"VBLock address 0x%x does not resolve", m_aVBLock )
             ->Throw();
      if ( m_nVBLockSize <= 0 )
        m_nVBLockSize = VBLock_Hdr_u_SizeNN ( pVBLock );
    }
    //VBLsize nSize1 = sizeof(VBLock);
    //VBLsize nSize2 = sizeof(m_oVBLock);
    ASSERT(sizeof(VBLock)<=sizeof(m_oVBLock));
}

// Operators
P3PmsgObject&
P3PmsgObject::operator = ( const P3PmsgObject& rhs )
{
    if ( this == &rhs )
      return *this;
    Nullify ( );                       // Added LJM [2025-02-17]
    Connect ( rhs );
    return *this;
}
bool
P3PmsgObject::operator == ( const P3PmsgObject& rhs ) const
{
    if ( m_hVBList == rhs.m_hVBList &&
         m_aVBLock == rhs.m_aVBLock    )
      return true;
    ASSERT(this!=&rhs);
    return false;
}

//
//  Do these two objects denote DIFFERENT items?
//  NOTES: The negation of the line above, written out because it could be
//         written before this existed and meant something else. operator bool
//         was an implicit conversion, so `oA != oB` had a viable built-in
//         candidate -- (int)(bool)oA != (int)(bool)oB -- and compiled to "is
//         exactly one of us void", which is an answer to a question nobody
//         asks. Refer P3PmsgField::operator == .
bool
P3PmsgObject::operator != ( const P3PmsgObject& rhs ) const
{
    return !( *this == rhs );
}

//
//  Does this object denote an item?
//  NOTES: EXACTLY !IsVoid(), and nothing else. It used to answer "is there a
//         heap", which is a different question and gave the opposite answer to
//         IsVoid() on the one state where they differ: an object carrying an
//         inline block -- a floating item, or a standalone value. IsVoid() has
//         asked "m_hVBList==0 && m_aVBLock==0" since 2025-02-18; this did not
//         follow it.
//       : §19 made that gap move. An inline item is rehomed onto a heap the
//         first time it is shared, so a floating object answered false here,
//         then true, with nothing about the item changed -- merely because
//         somebody took a copy of the handle. Asking a question must not be
//         what decides its answer.
//       : Every caller in this tree and in Chartboard reads it as "did I get
//         anything?" -- P3PmsgAttr and P3PmsgDesc delegate to it, GetParent
//         walks are guarded by it, and RootPath2Object failures are detected
//         with it. All of them are handed either a tree object or a void one,
//         so none of them changes behaviour; only the floating case is
//         corrected.
P3PmsgObject::operator bool ( ) const noexcept
{
    return !IsVoid ( );
}

//  Allocate a new VBLock of memory from the P2PmsgHeap
//  NOTES: Refer Free() for release of allocated chunks of
//         P2PmsgHeap
//       : Sizeof of allocated chunk will always be greater than
//         requested size. Refer GetVBLockSize() for retrieval of
//         allocated size.
//
//  Parameters:  UCHAR uVBlockType
//               Type of VBLock to be allocated
//
//               VBLsize nVBLockSize
//               Effective allocated VBLock size.
//               NOTES: Incremented for sizeof VBLockHdr according to
//                      addressing mode
//                    : Incremented to satisfy miniumum P2PmsgHeap
//                      allocation size according to addressing mode
//
//  Returns:     VBLaddr
//               Allocated VBLock address, includes VBLockHdr prefix
VBLaddr
P3PmsgObject::AllocVBLock ( UCHAR uVBLockType, VBLsize nVBLockSize, bool /*bZero*/ )
{
    ASSERT(uVBLockType==VBLock_Data||uVBLockType==VBLock_Name||uVBLockType==VBLock_Item
                                   ||uVBLockType==VBLock_Attr||uVBLockType==VBLock_Desc);
    if ( m_hVBList == 0 )
    { // Language heap used as default
      m_hVBList = P2PmsgHeap_CreateSYS ( m_uVBLock
                                  , g_nVBListCreateHeap_SizeMax );
      m_uVBLock = P2PmsgHeap_Addrnn ( m_hVBList );

      //  AND THE ITEM GOES WITH IT.  This is the last instant at which nothing
      //  can point at an inline block -- the allocation below is the first
      //  attribute, descendant, push or payload the object has ever had --
      //  which is the invariant RehomeInlineItem's NOTES rely on, stated
      //  there and not acted on here.  Leaving the item behind strands it:
      //  from the next line on the object has a heap AND addresses itself, a
      //  state every "is it shared" test in this file used to read as SHARED,
      //  and a copy then took the address of the SOURCE OBJECT.  §22.
      //
      //  This runs BEFORE the allocation below, so the block moves while it is
      //  still true that nothing points at it, and the item's identity changes
      //  when it GROWS rather than when somebody asks after it -- which is the
      //  rule §19 set and this keeps.
      RehomeInlineItem ( );
    }
    return P2PmsgHeap_Alloc ( m_hVBList, uVBLockType, nVBLockSize );
}
//
//  Moves an inline ITEM block onto a heap of its own
//  NOTES: A floating item's VBLock is built inside the P3PmsgObject that
//         carries it -- RenderThisSafe inits it in m_oVBLock and Connecta's
//         it with no heap at all. So the object IS the storage, and a copy of
//         the object is a second block rather than a second handle on one:
//         GetP2Pos differs, and a write through one is not seen by the other.
//         An item in a tree does neither, because its block is on the heap and
//         the copy shares it.
//       : That is only tenable while the block is never shared and never
//         outlives its object, and both of those fail. Connect says so in its
//         own comment -- "Cannot share heap that does not exist. NOTES: Create
//         heap and place data on heap" -- and then creates the heap without
//         placing anything on it, leaving m_aVBLock pointing into the SOURCE's
//         storage. It carries an ASSERT calling that a bug and a TODO to code
//         around it. This is the code around it.
//       : ONLY AN ITEM. A standalone VBLock that is NOT an item is a value --
//         P3PmsgData and P3PmsgName use ConnectVBLock for exactly that, and
//         the copy constructor duplicating one is what a value copy means.
//         Items are objects with identity; values are not. The discriminator
//         is the block header, which every block has (refer §16 of
//         stack_paths.md for why the header and not VBLock_pItem).
//       : NOTHING CAN POINT AT THE BLOCK YET. A collection or a push is
//         allocated through AllocVBLock, which creates the heap when there is
//         none -- so an item with no heap has no attributes, no descendants
//         and no stack, and there are no back-pointers to fix up.
//       : THAT INVARIANT IS A WINDOW, AND IT CLOSES. It was read here as a
//         standing property of any object without a heap, guarded by an
//         m_hVBList test at the top of this function; what it actually is is a
//         property of the moment BEFORE the heap exists. AllocVBLock is where
//         the heap is created, so AllocVBLock is where the block has to move,
//         and it now does. An object that reaches this function with a heap
//         already has therefore been through it once, and the item it carries
//         is on that heap -- unless the block is not an item at all, which the
//         test below is for. §22.
//       : The block is copied whole. P2PmsgHeap_Alloc adds the header size to
//         the request and VBLock_Init stamps only uVBLockDefs and the size, so
//         asking for nVBLockSize less the header yields a block of exactly
//         nVBLockSize whose header the copy then reproduces -- including
//         Linked and Alloc, which the inline block already carries and which
//         are true of the heap block as well.
//
//  Returns:     VBLaddr
//               Address of the block on its new heap, or 0 if this object does
//               not carry an inline item -- in which case the caller keeps
//               whatever it was doing before.
VBLaddr
P3PmsgObject::RehomeInlineItem ( )
{
    //  WHERE IS THE BLOCK -- not, is there a heap. It used to decline on
    //  `m_hVBList != 0` and call that "already on a heap", which is a
    //  different question and gives the wrong answer on the one state where
    //  the two disagree: an object that has a heap AND still addresses its own
    //  inline array. A floating item gets there by growing -- AllocVBLock
    //  creates a SYS heap for a payload the inline block cannot hold, and
    //  leaves the item block where it is -- and 128 wide characters of data is
    //  enough to do it. Refer stack_paths.md §22 for the measurement.
    //       : The test that matters was already on the next line, so the guard
    //         below is not merely wrong, it is redundant when it is right.
    if ( m_nVBLockSize == 0                       ||
         m_aVBLock != (VBLaddr)&m_oVBLock[0]         )
      return 0;                          // The block is not the inline one
    if ( !VBLock_IsItem ( (VBLock *)&m_oVBLock[0] ) )
      return 0;                          // A value, not an object

    const VBLsize nVBLockSize = m_nVBLockSize;
    if ( m_hVBList == 0 )
      m_hVBList = P2PmsgHeap_CreateSYS ( m_uVBLock, g_nVBListCreateHeap_SizeMax );
    m_uVBLock = P2PmsgHeap_Addrnn ( m_hVBList );

    const VBLsize nSizeofHdr = P2PmsgHeap_Sizeof_Hdr ( m_hVBList );
    ASSERT(nSizeofHdr>0&&nVBLockSize>nSizeofHdr);
    const VBLaddr aVBLock = P2PmsgHeap_Alloc ( m_hVBList, VBLock_Item
                                             , nVBLockSize - nSizeofHdr );
    memcpy ( P2PmsgHeap_Addr2Phys ( m_hVBList, aVBLock )
           , &m_oVBLock[0], nVBLockSize );

    m_aVBLock     = aVBLock;
    m_xVBLock     = 0;                   // Life cycle managed by the heap now
    m_nVBLockSize = nVBLockSize;
    ASSERT(P2PmsgHeap_AssertValidAlloc(m_hVBList,m_aVBLock));
    return aVBLock;
}
//
//  Duplicates one block on the heap it is already on
//  NOTES: Sized from the block's own header, the way RehomeInlineItem sizes
//         the block it moves and for the same reason: P2PmsgHeap_Alloc adds
//         the header back and may round the request up, so asking for the
//         declared size LESS the header yields a block the copy reproduces
//         exactly -- header, flags and all.
//       : The allocation invalidates pointers, which is why the source is
//         resolved a second time after it and only its size is read before.
static VBLaddr
P2PmsgObject_CopyHeapVBLock ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    VBLock       *pVBLock    = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLock );
    const VBLsize nVBLockSize= VBLock_Hdr_u_SizeNN ( pVBLock );
    const UCHAR   uVBLockType= pVBLock->oHdr.uVBLockDefs & VBLock_TypeMask;
    const VBLsize nSizeofHdr = P2PmsgHeap_Sizeof_Hdr ( hVBList );
    ASSERT(nSizeofHdr>0&&nVBLockSize>nSizeofHdr);

    const VBLaddr aCopy      = P2PmsgHeap_Alloc ( hVBList, uVBLockType
                                                , nVBLockSize - nSizeofHdr );
    memcpy ( P2PmsgHeap_Addr2Phys ( hVBList, aCopy )
           , P2PmsgHeap_Addr2Phys ( hVBList, aVBLock ), nVBLockSize );
    ASSERT(P2PmsgHeap_AssertValidAlloc(hVBList,aCopy));
    return aCopy;
}
//
//  Gives this object its own copy of what its inline VALUE block points at
//  NOTES: The copy constructor and Connect duplicate an inline block with a
//         memcpy and call that a value copy. It is one only while the whole
//         value fits in the block. It stops being one the moment the value
//         outgrows it: P2PmsgObject_NewVBLockData and P3PmsgName_ResizeName
//         both put the payload in a SECOND block on the heap and leave a
//         CHAIN POINTER behind in the first -- so what the memcpy copies from
//         that point on is an address, and the two objects name one payload.
//       : WHICH IS THE SAME MISTAKE §22 FIXED ONE LEVEL UP, and not the same
//         defect. §22's copy pointed into the SOURCE OBJECT and dangled when
//         the source died; this points into a HEAP both of them hold open, so
//         it stays readable. What it does instead is alias: a write through
//         either is seen by the other, and the first of them to retype hands
//         the block back to the heap while the other still chains to it --
//         P2PmsgObject_NewVBLockData walks the chain and Free()s what it
//         finds. Measured: two standalone values, one payload at one address,
//         a byte written through one read back through the other.
//       : ON THE HEAP THEY SHARE, not a new one. The heap handle was AddRef'd
//         by the caller before this runs, so it outlives either object on its
//         own; what the two of them must not share is the BLOCK. Allocating
//         here keeps the payload where every accessor already expects to
//         resolve it.
//       : THE CHAIN IS WALKED, not just its first link. Chaining is usually a
//         single step -- NewVBLockData collapses what it finds before adding
//         one -- but the readers loop, so this loops.
//       : Blocks that are not values do not come here. An ITEM is rehomed out
//         of the object before either caller reaches its value arm (refer
//         RehomeInlineItem), and an object whose block is already on a heap
//         shares that block deliberately: that is what a handle IS.
void
P3PmsgObject::PrivatiseInlineChain ( )
{
    if ( m_hVBList == 0                           ||
         m_aVBLock != (VBLaddr)&m_oVBLock[0]         )
      return;                          // Nothing inline, or nowhere to put it

    if ( VBLock_IsData ( (VBLock *)&m_oVBLock[0] ) )
    {
      VBLaddr aOwner = 0;              // 0 addresses the inline block itself
      for ( ;; )
      {
        VBLockData *pData = VBLock_pData ( aOwner ? (VBLock *)Msg2Phys ( aOwner )
                                                  : (VBLock *)&m_oVBLock[0] );
        if ( !VBLockData_IsChained ( pData ) )
          return;
        const VBLaddr aChain2Next = VBLockData_GetChain2Next ( m_uVBLock, pData );
        const VBLaddr aMine       = P2PmsgObject_CopyHeapVBLock ( m_hVBList
                                                                , aChain2Next );
                    pData = VBLock_pData ( aOwner ? (VBLock *)Msg2Phys ( aOwner )
                                                  : (VBLock *)&m_oVBLock[0] );
        VBLockData_SetChain2Next ( m_uVBLock, pData, aMine );
        aOwner = aMine;
      }
    }

    if ( VBLock_IsName ( (VBLock *)&m_oVBLock[0] ) )
    {
      VBLaddr aOwner = 0;
      for ( ;; )
      {
        VBLockName *pName = VBLock_pName ( aOwner ? (VBLock *)Msg2Phys ( aOwner )
                                                  : (VBLock *)&m_oVBLock[0] );
        if ( !VBLockName_IsChained ( pName ) )
          return;
        const VBLaddr aChain2Next = VBLockName_GetChain2Next ( m_uVBLock, pName );
        const VBLaddr aMine       = P2PmsgObject_CopyHeapVBLock ( m_hVBList
                                                                , aChain2Next );
                    pName = VBLock_pName ( aOwner ? (VBLock *)Msg2Phys ( aOwner )
                                                  : (VBLock *)&m_oVBLock[0] );
        VBLockName_SetChain2Next ( m_uVBLock, pName, aMine );
        aOwner = aMine;
      }
    }
}
//
//  Gives back what this object's inline VALUE block points at
//  NOTES: THE OTHER HALF OF PrivatiseInlineChain, and it was missing. That one
//         gives a copy its own payload block ON THE HEAP THE TWO OBJECTS SHARE,
//         which is right -- the AddRef has settled the heap's lifetime. But a
//         shared heap does not go away when the copy does, and ~P3PmsgObject
//         closes the heap and frees nothing, so the block stayed allocated on a
//         heap that was still open. Copying a grown value in a loop therefore
//         allocated once per iteration and gave nothing back: measured at 4841
//         copies before the heap refused at its ceiling.
//       : ONLY AN INLINE BLOCK, and only its chain. The block itself lives in
//         this object and is not the heap's to take. An item on a heap belongs
//         to the message and is nobody's to free here, which is what the
//         address test excludes; an externally managed block is excluded by
//         m_xVBLock, the same guard Free() keeps.
//       : SAFE TO CALL TWICE. Every walk starts from a chain pointer and stops
//         on a zero one, and the owner's pointer is cleared before returning,
//         so a Nullify() followed by the destructor frees each block once.
//       : The chain is one link long in practice and this does not assume it.
//         P2PmsgObject_NewVBLockData and P3PmsgName_ResizeName both REPLACE the
//         chained block rather than appending to it, so growth cannot lengthen
//         a chain; a longer one can only arrive already built, in an image. The
//         loops here tolerate that for the same reason the ones in
//         NewVBLockData do.
void
P3PmsgObject::ReleaseInlineChain ( )
{
    if ( m_hVBList == 0                           ||
         m_xVBLock != 0                           ||
         m_aVBLock != (VBLaddr)&m_oVBLock[0]         )
      return;                          // Not ours to give back

    if ( VBLock_IsData ( (VBLock *)&m_oVBLock[0] ) )
    {
      VBLockData *pData = VBLock_pData ( (VBLock *)&m_oVBLock[0] );
      VBLaddr     aNext = VBLockData_IsChained ( pData )
                            ? VBLockData_GetChain2Next ( m_uVBLock, pData ) : 0;
      if ( aNext )
        VBLockData_SetChain2Next ( m_uVBLock, pData, 0 );
      while ( aNext )
      {
        VBLockData   *pNext  = VBLock_pData ( (VBLock *)Msg2Phys ( aNext ) );
        const VBLaddr aAfter = VBLockData_IsChained ( pNext )
                                 ? VBLockData_GetChain2Next ( m_uVBLock, pNext )
                                 : 0;
        Free ( aNext );
        aNext = aAfter;
      }
      return;
    }

    if ( VBLock_IsName ( (VBLock *)&m_oVBLock[0] ) )
    {
      VBLockName *pName = VBLock_pName ( (VBLock *)&m_oVBLock[0] );
      VBLaddr     aNext = VBLockName_IsChained ( pName )
                            ? VBLockName_GetChain2Next ( m_uVBLock, pName ) : 0;
      if ( aNext )
        VBLockName_SetChain2Next ( m_uVBLock, pName, 0 );
      while ( aNext )
      {
        VBLockName   *pNext  = VBLock_pName ( (VBLock *)Msg2Phys ( aNext ) );
        const VBLaddr aAfter = VBLockName_IsChained ( pNext )
                                 ? VBLockName_GetChain2Next ( m_uVBLock, pNext )
                                 : 0;
        Free ( aNext );
        aNext = aAfter;
      }
    }
}
VBLaddr
P3PmsgObject::Free ( VBLaddr aVBLockAddr )
{
    if ( aVBLockAddr == m_xVBLock              &&
         aVBLockAddr == (VBLaddr)&m_oVBLock[0]    )
      m_xVBLock = 0;
    else if ( aVBLockAddr != (VBLaddr)&m_oVBLock[0] )
      P2PmsgHeap_Free ( m_hVBList, aVBLockAddr );
    if ( aVBLockAddr == m_aVBLock )
      m_aVBLock = 0;
    return 0;
}
void*
P3PmsgObject::Msg2Phys ( VBLaddr aVBLockAddr ) const
{
    if ( m_hVBList )
      return (void *)P2PmsgHeap_Addr2Phys ( m_hVBList, aVBLockAddr );
    void *vpVBLockAddr = (void *)aVBLockAddr;
    P2PASSERT(vpVBLockAddr==0||IsBadWritePtr(vpVBLockAddr,4)==0);
    return vpVBLockAddr;
}
VBLaddr
P3PmsgObject::Msg2Size ( VBLaddr aVBLockAddr ) const
{
    if ( m_hVBList )
      return P2PmsgHeap_Sizeof ( m_hVBList, aVBLockAddr );
    if ( aVBLockAddr == (VBLaddr)&m_oVBLock[0] )
      return sizeof(m_oVBLock);
    if ( aVBLockAddr == m_aVBLock )
      return m_nVBLockSize;
    return _msize ( (void *)aVBLockAddr );
}

//  Addressing and allocations
P2Pos
P3PmsgObject::GetP2Pos ( ) const noexcept
{
    ASSERT(m_aVBLock==0||m_hVBList==0||P2PmsgHeap_AssertValidAlloc(m_hVBList,m_aVBLock)); //TODO:LJM 64bit
    return m_aVBLock;
}
char*
P3PmsgObject::GetVBLock ( ) const
{
    ASSERT(m_aVBLock==0||m_hVBList==0||P2PmsgHeap_AssertValidAlloc(m_hVBList,m_aVBLock));
    if ( m_aVBLock == (VBLaddr)&m_oVBLock[0] )
      return (char*)&m_oVBLock[0];
    if ( m_hVBList )
      return (char *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    return (char *)m_aVBLock;
}
VBLaddr
P3PmsgObject::GetVBLocknn ( ) const
{
    ASSERT(m_aVBLock==0||m_hVBList==0||P2PmsgHeap_AssertValidAlloc(m_hVBList,m_aVBLock));
    return m_aVBLock;
}
VBLsize
P3PmsgObject::GetVBLockSize ( ) const
{
    return m_nVBLockSize;
}
P3PmsgObject
P3PmsgObject::GetParent ( ) const
{
    P3PmsgObject oObject;
    VBLock      *pVBLock = (VBLock *)GetVBLock();
    if ( pVBLock == nullptr )
      return oObject;
    if ( IsField() )
    {
      VBLaddr aVBLockParent = VBLockItem_GetParent ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      if ( aVBLockParent )
        oObject.Connectx ( m_hVBList, aVBLockParent, 0 );
      return oObject;
    }
    if ( IsDesc() )
    {
      VBLaddr aVBLockParent = VBLockDesc_GetParent ( pVBLock->oHdr.uVBLockDefs, VBLock_pDesc(pVBLock) );
      if ( aVBLockParent )
        oObject.Connectx ( m_hVBList, aVBLockParent, 0 );
      return oObject;
    }
    if ( IsAttr() )
    {
      VBLaddr aVBLockParent = VBLockAttr_GetParent ( pVBLock->oHdr.uVBLockDefs, VBLock_pAttr(pVBLock) );
      if ( aVBLockParent )
        oObject.Connectx ( m_hVBList, aVBLockParent, 0 );
      return oObject;
    }
    ASSERT(VBLock_IsLinked(pVBLock));  // To be sure, to be sure
    ASSERT(0);
    return oObject;
}
P3PmsgObject
P3PmsgObject::GetParentItem ( ) const
{
    VBLock      *pVBLock = (VBLock *)GetVBLock();
    VBLaddr      aVBLockParent = 0; //VBLockItem_GetParent ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    P3PmsgObject oObject;
    if ( IsField() )
    {
      aVBLockParent = VBLockItem_GetParent ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      if ( aVBLockParent == 0 )
        return oObject;
      oObject.Connectx ( m_hVBList, aVBLockParent, 0 );
      ASSERT ( oObject.IsDesc() || oObject.IsAttr() );
      return oObject.GetParentItem();
    }
    if ( IsDesc() )
    {
      aVBLockParent = VBLockDesc_GetParent ( pVBLock->oHdr.uVBLockDefs, VBLock_pDesc(pVBLock) );
      if ( aVBLockParent == 0 )
        return oObject;
      oObject.Connectx ( m_hVBList, aVBLockParent, 0 );
      ASSERT ( oObject.IsField() );
      return oObject;
    }
    if ( IsAttr() )
    {
      aVBLockParent = VBLockAttr_GetParent ( pVBLock->oHdr.uVBLockDefs, VBLock_pAttr(pVBLock) );
      if ( aVBLockParent == 0 )
        return oObject;
      oObject.Connectx ( m_hVBList, aVBLockParent, 0 );
      ASSERT ( oObject.IsField() );
      return oObject;
    }
    ASSERT(0);
    return oObject.GetParent();
}
P3PmsgObject
P3PmsgObject::GetObject ( P2Pos nP2Pos ) const
{
    P3PmsgObject oObject;
    oObject.Connectx ( m_hVBList, nP2Pos, 0 );
    return oObject;
}

//  Troubleshooting
void
P3PmsgObject::AssertValid ( ) const
{
    // Addressing
    if ( m_hVBList                                  &&
         m_uVBLock != P2PmsgHeap_Addrnn(m_hVBList )    )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Addressing corruption" )
            -> Throw();
    VBLock *pVBLock = (VBLock *)GetVBLock ( );
    if ( m_nVBLockSize && !VBLock_IsLinked(pVBLock) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Encountered unlinked VBLock object" )
            -> Throw();
    if ( m_hVBList )
      P2PmsgHeap_AssertValidAlloc(m_hVBList,m_aVBLock);
    if ( m_aVBLock                           &&
         m_aVBLock != (VBLaddr)&m_oVBLock[0] &&
         m_hVBList ==           NULL            )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted address" )
            -> Throw();
}
BOOL
P3PmsgObject::AssertCommon ( const P3PmsgObject& rhs ) const
{
    BOOL   bCommon = (m_hVBList==rhs.m_hVBList) ? TRUE : FALSE;
    ASSERT(bCommon);
    return bCommon;
}

//  Asserts passed VBLockAddr is valid within the scope of the
//          P2PmsgHANDLE contained within this object
//          NOTES: Covers all P2PmsgHANDLE types
//
//  Parameters:  VBLaddr aVBLockAddr
//               Address to be checked
//
//  Returns:     BOOL
//                 TRUE... Valid address
//                 FALSE.. Invalid address
BOOL
P3PmsgObject::AssertValidAddr ( VBLaddr aVBLockAddr )
{
    if ( m_hVBList )
      return P2PmsgHeap_AssertValidAlloc ( m_hVBList, aVBLockAddr );
    return aVBLockAddr == (VBLaddr)&m_oVBLock[0] ? TRUE : FALSE;
}

BOOL
P3PmsgObject::AssertCondition ( int nConditionID ) const
{
    BOOL bResult = FALSE;
    // Assert sizeof VBLockData > 9 bytes for 64-bit addressing mode
    // NOTES: AssertCondition(ObjectCheckID_DataSizeGT9);
    if ( nConditionID == ObjectCheckID_DataSizeGT9 )
    {
      VBLsize nVBLockData_SizeMin = VBLockData_Sizeof_Min ( m_uVBLock );
      VBLsize nVBLockData_Alloc   = VBLockData_Sizeof_Alloc ( (VBLock *)GetVBLock() );
      if ( m_uVBLock == 2 ) nVBLockData_SizeMin = 4; // TODO bug-hunt-delete 64-bit curover
      bResult = nVBLockData_SizeMin <= nVBLockData_Alloc ? TRUE : FALSE;
      ASSERT(bResult);
      return bResult;
    }
    return bResult;
}

// Properties
bool
P3PmsgObject::IsVoid ( ) const noexcept
{
    // Added "m_oVBLock==0" [20250218] LJM
    return (m_hVBList==0 && m_aVBLock==0)? true : false;
}

//
//  Is the item I denote stored INSIDE me?
//  NOTES: §22 recorded that a field cannot say whether it is a copy or a
//         handle. This is the half of that question which has an answer, and
//         it is the half that bites. TRUE means this object IS the storage:
//         m_oVBLock holds the block, nothing else in the process can be
//         looking at it, and a write through this object reaches nobody --
//         which is exactly the state a caller who meant to write THROUGH has
//         got wrong. FALSE means the block is on a heap and this object is one
//         NAME for it, so a copy of this object is a second name for the same
//         item and sees that write.
//       : IT IS NOT "is there a heap", which is what every test in this file
//         used to ask and what GetP2PmsgHandle() still answers. The two
//         disagreed on one state -- an inline item whose object had been given
//         a heap -- and on that state the heap question said SHARED about a
//         block that was nobody's but its own. AllocVBLock now rehomes the
//         item when it creates that heap, so the state no longer occurs; this
//         asks the question that was right either way.
//       : IT DOES NOT SAY WHOSE ITEM IT IS. A value copy that has since grown
//         lives on a heap too, and answers false here while reaching nothing
//         the caller holds. For whose, there is a comparand and it is §21's
//         `==`. What this adds is an answer that needs none.
//       : AND ITS FALSE IS NOT A GUARANTEE, which is what that row means: true
//         says a write reaches nobody, false says only that the block is not
//         in here. IsSole below is the same guarantee asked of the STORAGE
//         rather than of the block, and it covers that row. Ask this one when
//         the question is where the item is; ask that one when the question is
//         whether anyone else can see a write. §24.
//       : A void object answers false -- it denotes no item, so the item is
//         not inside it either. Ask IsVoid() first, as with every other
//         question in this family.
bool
P3PmsgObject::IsInline ( ) const noexcept
{
    return ( m_aVBLock != 0 && m_aVBLock == (VBLaddr)&m_oVBLock[0] )
             ? true : false;
}
//
//  Can anything at all, other than me, see a write through me?
//  NOTES: TRUE IS A GUARANTEE: no. FALSE is not the opposite guarantee, and
//         says only that the question is open -- which is the shape IsInline
//         has as well, and the reason for this is that IsInline's guarantee
//         covers too little. IsInline asks where the BLOCK is and answers
//         false for a duplicate that has since grown, which lives on a heap of
//         its very own and reaches nothing at all. §24 recorded that row. This
//         asks after the STORAGE and answers it true.
//       : Two ways to be sole, and they are the two ways to own storage. The
//         block is INSIDE this object, which is IsInline and which nothing
//         else in the process can address -- §22 rehomes an item before it is
//         ever shared and §23 duplicates a value's payload, so an inline block
//         is reachable only through the object carrying it. Or the block is on
//         a heap THIS OBJECT IS THE ONLY HOLDER OF: every object that names a
//         heap holds a reference to it (Connecta AddRefs, and the copy
//         constructor and Connect do too), so a count of one means there is no
//         second object to be looking.
//       : ASK THE FIELD, NOT THIS, WHEN THERE IS A FIELD TO ASK.
//         P3PmsgField::IsSole overrides rather than forwards: it knows which of
//         the heap's holders are its own sub-objects and subtracts them, which
//         is the row below. This one cannot -- an object has no parts -- so
//         what follows is about THIS answer, and the field's is narrower.
//       : WHAT FALSE DOES NOT SAY. The count is of holders of the HEAP, not of
//         names for the BLOCK, so a second holder may be naming something else
//         entirely -- including one of this object's own sub-objects. A field
//         that has been asked for its descendants keeps a P3PmsgDesc that holds
//         the heap, and answers false from then on while still being the only
//         name for its item. Measured, and left: narrowing it further needs a
//         count per block, which is a different library.
//       : It follows that false is not "shared" and must not be read as it.
//         For WHOSE the comparand is §21's `==`, which is exact in both
//         directions and needs the other object to compare against. This is
//         the answer available when there is nothing to compare to.
//       : A void object answers false. It denotes no storage, so it is not the
//         sole holder of any -- ask IsVoid() first, as with the rest of this
//         family.
//       : A heap handle handed out raw by GetP2PmsgHandle() and held without
//         an AddRef is outside the count and outside this guarantee. That is
//         the ownership contract the rest of the file keeps; this reads the
//         count it maintains.
bool
P3PmsgObject::IsSole ( ) const noexcept
{
    if ( m_aVBLock == 0 )
      return false;                    // Void: no storage to be sole holder of
    if ( m_aVBLock == (VBLaddr)&m_oVBLock[0] )
      return true;                     // The block is in here, so nowhere else
    return P2PmsgHeap_RefCount ( m_hVBList ) == 1;
}
bool
P3PmsgObject::IsData ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    return VBLock_IsData ( pVBLock );
}
bool
P3PmsgObject::IsName ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    return VBLock_IsName ( pVBLock );
}
bool
P3PmsgObject::IsField ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    if ( pVBLock == nullptr )
      return false;
    return VBLockItem_IsField ( VBLock_pItem(pVBLock) );
}
//bool
//P3PmsgObject::IsNode ( ) const
//{
//ASSERT(0);
//    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
//    if ( pVBLock == nullptr )
//      return false;
//    return VBLockItem_IsNode ( VBLock_pItem(pVBLock) );
//}
bool
P3PmsgObject::IsList ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    if ( pVBLock == nullptr )
      return false;
    return VBLockItem_IsList ( VBLock_pItem(pVBLock) );
}
bool
P3PmsgObject::IsVect ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    if ( pVBLock == nullptr )
      return false;
    return VBLockItem_IsVect ( VBLock_pItem(pVBLock) );
}
bool
P3PmsgObject::IsAttr ( ) const
{
    const VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    if ( pVBLock == nullptr )
      return false;
    return VBLock_IsAttr ( pVBLock );
}
bool
P3PmsgObject::IsDesc ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    if ( pVBLock == nullptr )
      return false;
    return VBLock_IsDesc ( pVBLock );
}
bool
P3PmsgObject::IsStck ( ) const
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( ); //TODO:LJM deprecated (VBLock *)P2PmsgHeap_Addr2Phys ( m_hVBList, m_aVBLock );
    if ( pVBLock == nullptr )
      return false;
    return VBLock_IsStck ( pVBLock );
}
bool
P3PmsgObject::IsRoot ( ) const
{
    if ( m_hVBList == 0 )
      return false;
    return P2PmsgHeap_IsRoot(m_hVBList,m_aVBLock) ? true : false;
}
BOOL
P3PmsgObject::IsVBLaddrnn ( UCHAR uVBLock ) const noexcept
{
    if ( ( uVBLock == VBLock_Addr64 ||
           uVBLock == 64                ) &&
          m_uVBLock == VBLock_Addr64         )
      return TRUE;
    if ( ( uVBLock == VBLock_Addr32 ||
           uVBLock == 32                ) &&
          m_uVBLock == VBLock_Addr32         )
      return TRUE;
    if ( ( uVBLock == VBLock_Addr16 ||
           uVBLock == 16                ) &&
          m_uVBLock == VBLock_Addr16         )
      return TRUE;
    if ( ( uVBLock == VBLock_Addr08 ||
           uVBLock == 8                ) &&
          m_uVBLock == VBLock_Addr08        )
      return TRUE;
    return FALSE;
}
BOOL
P3PmsgObject::IsContainedVBLump ( void *pVBLaddr, VBLsize nSizeofVBLump ) const noexcept
{
    VBLock *pVBLock = (VBLock *)GetVBLock ( );
    return VBLock_IsContainedVBLump ( pVBLock, pVBLaddr, nSizeofVBLump );
}
BOOL
P3PmsgObject::HasParent() const
{
    if ( m_hVBList == 0 )
      return FALSE;
    VBLock *pVBLock   = (VBLock *)GetVBLock();
    UCHAR   uVBLock   = pVBLock->oHdr.uVBLockDefs;
    ASSERT(VBLock_IsLinked(pVBLock));
    VBLockItem *pItem = VBLock_pItem ( pVBLock );
    if ( pItem == nullptr )
      return FALSE;
    VBLaddr aParent  = VBLockItem_GetParent ( uVBLock, pItem );
    if ( aParent == 0 )
      return FALSE;
    return TRUE;
}

//
//  Compares P3PmsgObject's for common VBList handles
//
//  Parameters:  const P3PmsgObject *pObject1
//               First comparision object
//
//               const P3PmsgObject *pObject2
//               Second comparision object
//
//  Returns:     BOOL
//               Comparision result
//                 TRUE... Common
//                 FALSE.. Independant
//
BOOL
P3PmsgObject_IsCommon ( const P3PmsgObject *pObject1, const P3PmsgObject *pObject2 ) noexcept
{
    if ( pObject1 == pObject2 )
      return TRUE;
    if ( pObject1 == nullptr || pObject2 == nullptr )
      return FALSE;
    return pObject1->m_hVBList == pObject2->m_hVBList ? TRUE : FALSE;
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgField implementation
//  NOTES: Acts as VBlockField item wrapper

//
//  Contructors and destructor
P3PmsgField::P3PmsgField ( )
           : P3PmsgName ( (P3PmsgField *)0 ), P3PmsgData ( (P3PmsgField *)0 )
{
    RenderThisSafe ( );
}
//
//  A P3PmsgField copies as a VALUE
//  NOTES: RenderThisSafe builds this field its own item and then operator=
//         copies rhs into it, member by member -- name, data, attributes,
//         descendants, stack. The result is a second item that reads the same,
//         not a second handle on the first, and that is what it has always
//         been.
//       : What stood here was an arm that AddRef'd rhs's heap and shared its
//         block -- a HANDLE copy -- behind `if ( !OBJ__hVBList )`, testing a
//         member of THIS field that RenderThisSafe had just set to zero on the
//         line above. Connecta(0,...) returns early when the handle it is
//         given already matches, so the guard was true on every call and the
//         arm below it never ran. It has been removed rather than repaired:
//         reviving it would flip every field copy in this tree and in
//         Chartboard from a value to an alias, silently, which is a decision
//         and not a bug fix.
//       : THE HANDLE IS SPELLED r_Object(). `P3PmsgField oB = oA` is a copy of
//         the item; `P3PmsgField oB = oA.r_Object()` is the item. The library
//         writes the second wherever it means to write through -- refer
//         P3PmsgRefactor_DataType and P2Pmsg_UpgradeMove -- because a collection
//         hands back its cursor, and the next Select moves it.
P3PmsgField::P3PmsgField ( const P3PmsgField& rhs )
           : P3PmsgName ( (P3PmsgField *)0 ), P3PmsgData ( (P3PmsgField *)0 )
{
    RenderThisSafe ( );
    ASSERT(OBJ__hVBList==nullptr);
   *this = rhs;
}
P3PmsgField::P3PmsgField ( LPCTSTR lpszName, size_t nSize )
           : P3PmsgName ( (P3PmsgField *)0 ), P3PmsgData ( (P3PmsgField *)0 )
{
    RenderThisSafe ( );
    c_name ( lpszName, nSize );
    //ASSERT(VerifyContainment());
}
P3PmsgField::P3PmsgField ( LPCTSTR lpszName, const P3PmsgData& oData )
           : P3PmsgName ( (P3PmsgField *)0 ), P3PmsgData ( (P3PmsgField *)0 )
{
    RenderThisSafe ( );
    c_name ( lpszName, 0 );
  (*this).r_data() = oData;
    //ASSERT(VerifyContainment());
}
P3PmsgField::P3PmsgField ( const P2PmsgFieldHdl& rhs )
           : P3PmsgName ( (P3PmsgField *)0 ), P3PmsgData ( (P3PmsgField *)0 )
{
    RenderThisSafe ( );
    Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
            , rhs.uiParam3 );
}
P3PmsgField::P3PmsgField ( P2PmsgHANDLE hVBList, VBLaddr aField, VBLsize nFieldSize )
           : P3PmsgName ( (P3PmsgField *)nullptr ), P3PmsgData ( (P3PmsgField *)nullptr )
{
    if ( hVBList && aField && nFieldSize <= 0 )
      nFieldSize = P2PmsgHeap_Sizeof ( hVBList, aField );
    P3PmsgData::m_pObject = &m_oObject;
    P3PmsgName::m_pObject = &m_oObject;
    m_oObject.Connectx ( hVBList, aField, nFieldSize );
    //m_pP3PmsgAttr  = 0;
    //m_pP3PmsgDesc  = 0;
    //m_pMsgStck     = 0;
}
P3PmsgField::P3PmsgField ( const P3PmsgObject& rhs )
           : P3PmsgName ( (P3PmsgField *)nullptr ), P3PmsgData ( (P3PmsgField *)nullptr )
           , m_oObject ( rhs)
{
    P3PmsgData::m_pObject = &m_oObject;
    P3PmsgName::m_pObject = &m_oObject;
                //m_oObject =  rhs;
    //m_pP3PmsgAttr  = 0;
    //m_pP3PmsgDesc  = 0;
    //m_pMsgStck     = 0;
}
P3PmsgField::~P3PmsgField ( )
{
    P3PmsgData::m_pObject = nullptr;
    P3PmsgName::m_pObject = nullptr;
    if ( m_pP3PmsgAttr ) {
      r_Object().AssertCommon(m_pP3PmsgAttr->r_Object() );
      delete m_pP3PmsgAttr;
    }
    if ( m_pP3PmsgDesc ) {
      r_Object().AssertCommon(m_pP3PmsgDesc->r_Object() );
      delete m_pP3PmsgDesc;
    }
    if ( m_pMsgStck ) {
      //r_Object().AssertCommon(m_pMsgStck-> );
      delete m_pMsgStck;
    }
}
void
P3PmsgField::RenderThisSafe ( )
{
    P3PmsgData::m_pObject = &m_oObject;
    P3PmsgName::m_pObject = &m_oObject;
    //OBJ__uVBLock  = VBLock_Addr32;    // TODO: LJM commented out 2022/02/04
    VBLock& oVBLock = *(VBLock *)m_oObject.m_oVBLock;       // Delegate field object
    ZeroMemory ( &oVBLock, sizeof(oVBLock) );
    VBLock_Init( &oVBLock
               , OBJ__uVBLock|VBLock_Item|VBLock_Linked|VBLock_Alloc, sizeof(oVBLock) );
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(&oVBLock), VBLock_Field );
    VBLockField_Init( VBLock_pField(&oVBLock), AttrField_DEFAULT );
    VBLockName_Init ( OBJ__uVBLock, VBLock_pName(&oVBLock)
                    , VBLockAttr_DEFAULT | VBLockAttr_NULL
                    , 0, sizeof(VBLockField::oVBLockName) );
    VBLockData_Init ( VBLock_pData(&oVBLock)
                    , VBLockAttr_DEFAULT, VBLockData_NULL 
                    , sizeof(VBLockField::oVBLockData) );
    //m_pP3PmsgAttr        = 0;
    //m_pP3PmsgDesc        = 0;
    //m_pMsgStck        = 0;
    m_oObject.Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock) );
}
void
P3PmsgField::Nullify ( )
{
    if ( !OBJ__hVBList &&
          OBJ__aVBLock    )
    {
      //ASSERT(0);//Suspect no longer valid
      //m_aField = OBJ__Free ( m_aField );
    }
    if ( m_pP3PmsgAttr )
      delete m_pP3PmsgAttr;
    m_pP3PmsgAttr = nullptr;
    if ( m_pP3PmsgDesc )
      delete m_pP3PmsgDesc;
    m_pP3PmsgDesc = nullptr;
    if ( m_pMsgStck )
      delete m_pMsgStck;
    m_pMsgStck = nullptr;
    m_oObject.Nullify ( );
}
void
P3PmsgField::Connect ( P2PmsgHANDLE hVBList, VBLaddr aField, VBLsize nFieldSize )
{
    m_oObject.Connectx ( hVBList, aField, nFieldSize );
    
    P3PmsgData::m_pObject = &m_oObject;
    P3PmsgName::m_pObject = &m_oObject;
    if (  m_pP3PmsgAttr )
      m_pP3PmsgAttr -> Connect ( this );
    if (  m_pP3PmsgDesc )
      m_pP3PmsgDesc -> Connect ( this );
//AssertValid();
}

//
//  Rewind internals
//  NOTES: Several instances of this P3PmsgItem may exist and be
//         asynchronoulsy operated on.  In such cases the internal
//         cursors etc may have to be reset.
//       : The P2Pmsg library functions are not support thread safety
//         or concurrency.  Expectation is each item is operated on
//         in isolation
//
void
P3PmsgField::RewindCurs ( )
{
    if ( m_pP3PmsgAttr )
      delete m_pP3PmsgAttr;
    m_pP3PmsgAttr = nullptr;
    if ( m_pP3PmsgDesc )
      delete m_pP3PmsgDesc;
    m_pP3PmsgDesc = nullptr;
    if ( m_pMsgStck )
      delete m_pMsgStck;
    m_pMsgStck = nullptr;
}

//  Operators
P3PmsgField&
P3PmsgField::operator = ( const P3PmsgField& rhs )
{
//ASSERT(VerifyContainment()); //delete-bug-hunting
//ASSERT(rhs.VerifyContainment()); //delete-bug-hunting
//ASSERT(rhs.OBJ__IsField());
    if ( this != &rhs )
    {
//AssertValid();rhs.AssertValid();
      //UINT nSizeof = rhs.Sizeof();
      //if ( VBLockField_Sizenn(GetVBLock()) < nSizeof )
      //  NewVBLockField ( nSizeof );
//ASSERT(VBLockField_Sizenn(GetVBLock())>=nSizeof);//TODO:LJM debugging
      P2PmsgObject_pField(OBJ__)->uAccessAttr = P2PmsgObject_pField(rhs.OBJ__)->uAccessAttr;
      //GetVBLockField()->uVBLockType = rhs.GetVBLockField()->uVBLockType;
//ASSERT(r_name().VerifyContainment());
     r_name() = rhs.r_name();
//ASSERT(r_name().VerifyContainment());
//ASSERT(r_data().VerifyContainment());
     r_data() = rhs.r_data();
//ASSERT(r_data().VerifyContainment());
      if ( IsAttributed() || rhs.IsAttributed() )
        r_Attr()        = ((P3PmsgField&)rhs).r_Attr();
      if ( IsDescendant() || rhs.IsDescendant() )
        r_Desc()        = ((P3PmsgField&)rhs).r_Desc();
      m_bFieldDirty = true;

      // Pushed components
      if ( IsStacked() || rhs.IsStacked() )
        r_Stck() = ((P3PmsgField&)rhs).r_Stck();
    }
//AssertValid();rhs.AssertValid();//TODO:Delete debugging
    return *this;
}
P3PmsgField&
P3PmsgField::operator = ( const P3PmsgData& rhs )
{
    this->r_data() = rhs;
    return *this;
}
P3PmsgField&
P3PmsgField::operator = ( const P2PmsgFieldHdl& rhs )
{
    // To be sure, to be sure
    if ( OBJ__hVBList == (P2PmsgHANDLE)rhs.uiParam1 &&
         OBJ__aVBLock ==     rhs.uiParam2    )
      return *this;
    if ( !OBJ__IsField() )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Invalid in overloaded context" )
            -> Throw();
    Nullify ( );
    Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
            , rhs.uiParam3 );
///////////////////////////////////////////////////
// TODO:LJM delete-me-debuggin
    if ( m_pP3PmsgAttr ) {
      r_Object().AssertCommon(m_pP3PmsgAttr->r_Object() );
    }
////////////////////////////////////////////////////
    return *this;
}
P3PmsgField&
P3PmsgField::operator = ( const P3PmsgObject& rhs )
{
    // To be sure, to be sure
    // NOTES: Self assignment
    if ( &m_oObject == &rhs )
      return *this;
    // TODO:LJM debugging is this necessary? [2025-07-11]
    //if ( OBJ__hVBList == rhs.m_hVBList  &&
    //     OBJ__aVBLock == rhs.m_aVBLock     )
    //  return *this;
    // TODO:LJM end-of-above debugging
    if ( !rhs.IsField() )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Invalid overloaded context" )
            -> Throw();
    Nullify ( );
    m_oObject = rhs;
///////////////////////////////////////////////////
// TODO:LJM delete-me-debuggin
    if ( m_pP3PmsgAttr ) {
      r_Object().AssertCommon(m_pP3PmsgAttr->r_Object() );
    }
    if ( m_pP3PmsgDesc ) {
      r_Object().AssertCommon(m_pP3PmsgDesc->r_Object() );
    }
////////////////////////////////////////////////////
    return *this;
}
P3PmsgField&
P3PmsgField::operator += ( const P3PmsgList& rhs )
{
    r_Desc() += rhs;
    return *this;
}
P3PmsgField&
P3PmsgField::operator += ( const P3PmsgVect& rhs )
{
    r_Desc() += rhs;
    return *this;
}
P3PmsgField&
P3PmsgField::operator += ( const P3PmsgItem& rhs )
{
    r_Desc() += rhs;
    return *this;
}
P3PmsgField&
P3PmsgField::operator = ( const P3PmsgName& rhs )
{
   (P3PmsgName&)*this = rhs;
    return *this;
}
//
//  Is this the field called <lpszName>?
//  NOTES: const since [2026-09-11]. P3PmsgName declares operator == against
//         both a name and another P3PmsgName, and both are const; this one
//         hides them and was not, so `oConstField == L"AAA"` -- the ONLY
//         comparison this class ever meant to offer -- was the one that did
//         not compile. C2678, and no fall-back, because a string literal does
//         not convert to int and so the built-in candidates below were not
//         viable either.
bool
P3PmsgField::operator == ( LPCTNAM lpszName ) const
{
    ASSERT(P3PmsgName::m_pObject==&m_oObject);
    return _tcsicmp(c_name(), lpszName ) ? false : true;
}

//
//  Do these two fields denote the SAME item?
//  NOTES: THIS IS AN IDENTITY TEST, NOT A VALUE TEST. It asks P3PmsgObject,
//         which compares the heap handle and the block address -- so a handle
//         and the item it was taken from are equal, and a value copy of that
//         item is NOT, however identically it reads. That distinction is the
//         whole of it: `P3PmsgField oB = oA` is a second item and
//         `P3PmsgField oB = oA.r_Object()` is the first one, and until this
//         existed the pair could not be asked which they were.
//       : What `oA == oB` used to do was compile anyway. operator bool was an
//         implicit conversion, so the built-in operator ==(int,int) was a
//         viable candidate and the expression meant
//         `(int)(bool)oA == (int)(bool)oB` -- "are we both non-void". Against
//         a store holding AAA=1 and BBB=2 that answered true for a handle, for
//         a value copy, for an unrelated item and for an empty floating one:
//         four questions, one answer, and only the first of them right.
//       : It is the CONVERSION that made that legal, so the conversion is now
//         explicit on this class, on P3PmsgObject, and on the four collections
//         -- refer the declarations in P2Pmsg.h. `if ( oField )`, `!oField`,
//         `oA && oB` and static_cast<bool> are contextual and unaffected;
//         `oA == oB`, `oA < oB`, `int n = oField` and `oField + 1` are not,
//         and no longer compile. Building every solution in this tree and in
//         Chartboard with the conversion explicit produced no error at all,
//         which is the measurement: nothing anywhere was using it.
//       : For "do these two read the same", which is what P3PmsgData::operator
//         == answers for a value, compare r_data() and r_name() -- this class
//         hides both, deliberately, because a field is an item first.
bool
P3PmsgField::operator == ( const P3PmsgField& rhs ) const
{
    return r_Object ( ) == rhs.r_Object ( );
}

//
//  Do these two fields denote DIFFERENT items?
//  NOTES: Refer P3PmsgField::operator == .
bool
P3PmsgField::operator != ( const P3PmsgField& rhs ) const
{
    return !( *this == rhs );
}
P3PmsgField&
P3PmsgField::operator [] ( LPCTNAM lpszName )
{
    return r_Desc()[lpszName];
}

//
//  `if ( oField )` -- does this field denote an item?
//  NOTES: EXACTLY !IsVoid(), as it is on P3PmsgObject and now on P3PmsgList
//         and P3PmsgVect, so that `if ( oField )` asks one question with one
//         answer wherever it is written.
//       : It used to answer "is it populated" -- a name of non-zero length or
//         data that is not NULL -- which is a reasonable question wearing the
//         spelling of a different one. A floating item therefore answered
//         `if ( oField )` true while `oField.IsVoid()` also answered true, in
//         the same breath. Where the old meaning is wanted it is still two
//         calls away: !r_data().IsNull() and r_name().c_size().
//       : Its one caller in this solution asked the question about the wrong
//         variable -- refer CListCtrl_Ext -- and never fired.
P3PmsgField::operator bool ( ) const
{
    return !IsVoid ( );
}

// Chained reference exposures
//

P3PmsgData&
P3PmsgField::r_data ( ) const
{
    return (P3PmsgData&)*this;
}
P3PmsgName&
P3PmsgField::r_name ( ) const
{
    return (P3PmsgName&)*this;
}
P3PmsgAttr&
P3PmsgField::r_Attr ( AttrCMD_e eAttrCmd ) const
{
    // Demand creation
    if ( m_pP3PmsgAttr == nullptr )
      m_pP3PmsgAttr = new P3PmsgAttr ( (P3PmsgField*)this );

    // According to command
    switch ( eAttrCmd )
    {
      case  AttrCMD_Get:
        break;
      case  AttrCMD_Create:
        m_pP3PmsgAttr -> Create ( );
        break;
//      case  AttrCMD_Drop:
//        m_pP3PmsgAttr -> Drop ( );
//        break;
//      case  AttrCMD_DropOnEmpty:
//      {
//        if ( m_pP3PmsgAttr->IsEmpty() )
//          m_pP3PmsgAttr -> Drop ( );
//        break;
//      }
      default:
        break;
    };

    // Tidy up, and
    return *m_pP3PmsgAttr;
}
P3PmsgDesc&
P3PmsgField::r_Desc ( AttrCMD_e eAttrCmd ) const
{
    // Demand creation
IsDescendant();//TODO:LJM bug tracking.
IsAttributed();//TODO:LJM bug tracking.
    if ( m_pP3PmsgDesc == nullptr )
      m_pP3PmsgDesc = new P3PmsgDesc ( (P3PmsgField*)this );

    // According to command
    switch ( eAttrCmd )
    {
      case  AttrCMD_Get:
        break;
      case  AttrCMD_Create:
        m_pP3PmsgDesc -> Create ( );
        break;
//      case  AttrCMD_Drop:
//        m_pP3PmsgDesc -> Drop ( );
//        break;
//      case  AttrCMD_DropOnEmpty:
//      {
//        if ( m_pP3PmsgDesc->IsEmpty() )
//          m_pP3PmsgDesc -> Drop ( );
//        break;
//      }
      default:
        break;
    };

    // Tidy up, and
    return *m_pP3PmsgDesc;
}

MsgStck&
P3PmsgField::r_Stck ( ) const
{
    // Demand creation
    if ( m_pMsgStck == 0 )
      m_pMsgStck = new MsgStck ( this );

    // Tidy up, and
    return *m_pMsgStck;
}

const P3PmsgObject&
P3PmsgField::r_Object ( ) const
{
    return m_oObject;
}

//  Memory management
void
P3PmsgField::Drop ( )
{
   ASSERT(OBJ__IsField());
   if ( IsAttributed() )
     r_Attr().Drop();// ( AttrCMD_Drop );
   if ( IsDescendant() )
     r_Desc().Drop();// ( AttrCMD_Drop );
   if ( IsStacked() )
     r_Stck().Drop( );
AssertValid(); //TODO:LJM debugging
   //UINT aStack = VBLockItem_GetStack(m_uVBLock, 0 );
   P2PmsgField_DropName ( this );
   P2PmsgField_DropData ( this );

   // Isolate parent
   VBLock *pVBLockParent = P2PmsgField_GetVBLockParent(this);
   if ( pVBLockParent )
   {
     //VBLockItem *pItemParent = VBLock_pItem ( pVBLockParent );
     if ( VBLock_IsAttr(pVBLockParent) )
     { // Child of P3PmsgAttr parent
       VBLockAttr *pAttrParent = VBLock_pAttr(pVBLockParent);
       P2PmsgAttr_UnLinkItem ( &m_oObject, pAttrParent, OBJ__VBLocknn );
     }
     else if ( VBLock_IsDesc(pVBLockParent) )
     { // Child of P3PmsgDesc parent
       VBLockDesc *pDescParent = VBLock_pDesc(pVBLockParent);
       P2PmsgDesc_UnLinkItem ( &m_oObject, pDescParent, OBJ__VBLocknn );
     }
     else ASSERT(0);
   }

   // Tidy up, and
   OBJ__Free ( OBJ__aVBLock );
}

///////////////////////////////////////
//  Navigation and 

P3PmsgItem&
P3PmsgField::SelectItem ( LPCTNAM lpszItemName )
{
    return r_Desc().SelectItem ( lpszItemName );
}
P3PmsgObject
P3PmsgField::SelectObject ( LPCTNAM lpszObjectName )
{
    return P3Pmsg_SelectObject ( &r_Object(), lpszObjectName );
}
P3PmsgItem&
P3PmsgField::DeclareItem ( LPCTNAM lpszItemName, const P3PmsgData& oData, BOOL bUpdate )
{
    return r_Desc().DeclareItem( lpszItemName, oData, bUpdate );
}
bool
P3PmsgField::Exists ( LPCTNAM lpszItemName ) const
{
    if ( IsVoid() )
      return false;
    return !P3Pmsg_SelectObject(&r_Object(),lpszItemName).IsVoid();
}
bool
P3PmsgField::Delete ( LPCTNAM lpszItemName )
{
    return r_Desc().Delete(lpszItemName);
}
void
P3PmsgField::Truncate ( )
{
   if ( IsAttributed() )
     r_Attr().Drop();// ( AttrCMD_Drop );
   if ( IsDescendant() )
     r_Desc().Drop();// ( AttrCMD_Drop );
   if ( IsStacked() )
     r_Stck().Drop( );
AssertValid(); //TODO:LJM debugging
}

//  Addressing
P2Pos
P3PmsgField::GetP2Pos ( ) const
{
    return m_oObject.GetP2Pos ( );
}

///////////////////////////////////////////////////////////////////////
//  Troubleshooting

void
P3PmsgField::AssertValid ( ) const
{
    // Containment
    if ( !VerifyContainment() )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Failed Containment" )
            -> Throw();
     if ( m_pP3PmsgAttr )
       r_Object().AssertCommon(m_pP3PmsgAttr->r_Object() );
     if ( m_pP3PmsgDesc )
       r_Object().AssertCommon(m_pP3PmsgDesc->r_Object() );

    // Delegation
    m_oObject.AssertValid ( );
    VBLock *pVBLock = OBJ__VBLock;
    P3PmsgName::AssertValid ( );
    P3PmsgData::AssertValid ( );
    if ( !VBLock_IsItem(pVBLock) )
      return;

    // Backwards navigation
    VBLaddr aPrev   = VBLockItem_GetPrev ( OBJ__uVBLock, VBLock_pItem(pVBLock) );
    VBLock *pPrev   = 0;
    if ( aPrev )
      pPrev = (VBLock *)OBJ__Msg2Phys(aPrev);
    if ( pPrev               &&
        !VBLock_IsItem(pPrev)   )
    {
      ASSERT(VBLock_IsLinked(pPrev));
      ASSERT(VBLock_IsAlloc(pPrev));
      VBLock_IsItem(pPrev);
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted backwards linked VBLock" )
            -> Throw();
    }

    VBLaddr aPrev_Next = 0;
    if ( pPrev )
      aPrev_Next = VBLockItem_GetNext ( OBJ__uVBLock, VBLock_pItem(pPrev) );
    if ( aPrev_Next && OBJ__VBLocknn != aPrev_Next )
      EVERR -> Module ( __FUNCTION__ )
            -> Message(L"%s", (LPCTSTR)P3Pmsg_GetPath(this) )
            -> Message("Corrupted backwards link aPrev=%i aThis=%i aPrev_Next=%i",
                        aPrev, OBJ__VBLocknn, aPrev_Next )
            -> Throw();

    // Forwards navigation
    VBLaddr aNext = VBLockItem_GetNext ( OBJ__uVBLock, VBLock_pItem(pVBLock) );
    VBLock *pNext = 0;
    if ( aNext )
      pNext = (VBLock *)OBJ__Msg2Phys(aNext);
    if ( pNext               &&
        !VBLock_IsItem(pNext)   )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted forewards linked VBLock" )
            -> Throw();
    VBLaddr aNext_Prev = 0;
    if ( pNext )
      aNext_Prev = VBLockItem_GetPrev ( OBJ__uVBLock, VBLock_pItem(pNext) );
    if ( aNext_Prev && OBJ__VBLocknn != aNext_Prev )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted forewards link aPrev=%i aThis=%i aPrev_Next=%i",
                        aNext, OBJ__VBLocknn, aNext_Prev )
            -> Throw();
    // Attributes
    if ( IsAttributed() )
      const_cast<P3PmsgField*>(this)->r_Attr().AssertValid();
    // Descendants
    if ( IsDescendant() )
      const_cast<P3PmsgField*>(this)->r_Desc().AssertValid();
    // Stack
    if ( IsStacked() )
      const_cast<P3PmsgField*>(this)->r_Stck().AssertValid();
}

//
//  Verifies P3PmsgField object containment and optionally containment of
//  VBLump within that space.
//
//  Parameters:  void *pvBlob
//               Blob pointer to be checked for containment
//
//               VBLsize nSizeofBlob
//               Optional size of pvBlob
//
//  Returns:     BOOL
//               Containment result
//                 TRUE... Contained
//                 FALSE.. Outside of containment area
BOOL
P3PmsgField::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // pVBlob containment within VBLock
    VBLock     *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pvBlob != nullptr && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;                    // pVBlob not contained within VBLock

    // Name component containment
    if ( !r_name().P3PmsgName::VerifyContainment() )
      return FALSE;
    VBLockName *pName = P2PmsgObject_pName  ( m_oObject, false );
    VBLaddr     nSizeofName = VBLockName_Sizeof_Alloc ( OBJ__uVBLock, pName ); //       P2PmsgObject_VBLockNameSize( m_oObject );
    if ( !VBLock_IsContainedVBLump(pVBLock,pName,nSizeofName) )
      return FALSE;                    // pData not contained within VBLock
    // Data component containment
    if ( !r_data().P3PmsgData::VerifyContainment() )
      return FALSE;
    VBLockData *pData = P2PmsgObject_pData  ( m_oObject, false );
    const VBLaddr nSizeofData = P2PmsgObject_Sizeof_VBLockData( m_oObject, FALSE );
    if ( !VBLock_IsContainedVBLump(pVBLock,pData,nSizeofData) )
      return FALSE;                    // pData not contained within VBLock

    // Tidy up and
    return TRUE;
}

void
P3PmsgField::Print ( FILE *fd, int nDepthOS, int nDepthOSinc  )
{
    // Encode P2PmsgField details
    int cSize = 0;
    for ( int i = 0; i < nDepthOS; i++ )
      cSize = P3Pmsg_fwprintf ( fd, L"  " );
    VBLaddr nVBLockSize = Sizeof();
    cSize = P3Pmsg_fwprintf ( fd, L"%s(%Ii) = (%s %s) %s\n"
                            , c_name()
                            , nVBLockSize
                            , r_data().ToStringType(), r_data().ToStringDefs()
                            , r_data().ToString() );

    // Attributes
    if ( !r_Attr().IsEmpty() )
      r_Attr().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
    if ( !r_Desc().IsEmpty() )
      r_Desc().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
    if (  IsStacked() )
      r_Stck().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgField Properties

P2PmsgFieldHdl
P3PmsgField::GetP2PmsgFieldHdl ( )
{
    ASSERT(OBJ__IsField());
    VBLaddr aField = OBJ__aVBLock;
    P2PmsgFieldHdl oHdl = { (UINT_PTR)OBJ__hVBList, aField, P2PmsgHeap_Sizeof(OBJ__hVBList,aField) };
    return oHdl;
}

UCHAR
P3PmsgField::SetAccess ( UCHAR uAccessAdd, UCHAR uAccessRemove )
{
    VBLockField *pField = P2PmsgObject_pField ( OBJ__ );
                 pField -> uAccessAttr &= ~uAccessRemove;
                 pField -> uAccessAttr |=  uAccessAdd;
    return pField -> uAccessAttr;
}
UCHAR
P3PmsgField::GetAccess ( UCHAR uAccessMask )
{
    VBLockField *pField = P2PmsgObject_pField ( OBJ__ );
    return pField -> uAccessAttr & uAccessMask;
}
VBLsize
P3PmsgField::Sizeof ( ) const
{
    VBLsize nSizeof = sizeof(VBLockField)
                    - sizeof(VBLockName) + P3PmsgName::Sizeof()
                    - sizeof(VBLockData) + P3PmsgData::Sizeof();
    return nSizeof;
}
bool
P3PmsgField::IsDirty ( ) const
{
    if ( m_bFieldDirty         ||
         P3PmsgData::IsDirty() ||
         P3PmsgName::IsDirty()    )
      return true;
    return false;
}

//
//  Does this field denote NO item?
//  NOTES: The object's question, asked of the object. It used to answer "is
//         there a heap", which reported a floating item -- one with a name, a
//         value and, after §19, an identity -- as void, and which §19 then
//         made mutable: sharing the item rehomes its block onto a heap, so the
//         answer changed from void to not-void with nothing about the item
//         changed.
//       : P3PmsgObject::IsVoid asks whether there is a block at all, which is
//         the question every caller here means. A field built over a void
//         object -- a lookup that found nothing -- has no block and is still
//         void; a field that Nullify() has emptied is too, because
//         P3PmsgObject::Nullify clears m_aVBLock as well as the handle.
//       : P3PmsgList and P3PmsgVect inherit this, and their operator bool is
//         built on it.
bool
P3PmsgField::IsVoid ( ) const
{
    return OBJ__.IsVoid ( );
}

//
//  Is this field's item stored inside the field?
//  NOTES: The object's question, asked of the object -- as IsVoid() is.
//       : `P3PmsgField oB = oA` copies the ITEM, and oB answers TRUE: a write
//         to oB is oB's alone. `P3PmsgField oB = oA.r_Object()` names the item
//         and answers false. Both spellings are deliberate and both are used;
//         until this existed a callee handed a P3PmsgField& could not tell
//         which of them it had been given. Refer P3PmsgObject::IsInline for
//         what that does and does not settle.
//       : P3PmsgList and P3PmsgVect inherit it, and it is virtual for the same
//         reason IsVoid() is.
bool
P3PmsgField::IsInline ( ) const
{
    return OBJ__.IsInline ( );
}
//
//  Can a write through this field be seen anywhere but here?
//  NOTES: The object's question, asked of the object -- as IsVoid() and
//         IsInline() are. A callee handed a P3PmsgField& and nothing to
//         compare it to can ask this one and get a guarantee out of TRUE:
//         whatever it writes, nobody else is looking.
//       : FALSE IS NOT THE OPPOSITE. Refer P3PmsgObject::IsSole for what it
//         does and does not settle, and §21's `==` for whose item it is.
//       : P3PmsgList and P3PmsgVect inherit it, and it is virtual for the same
//         reason IsVoid() is.
//       : IT DOES NOT SIMPLY FORWARD, and that is the point of overriding it.
//         The object counts holders of the HEAP and cannot tell a stranger from
//         one of this field's own parts, so a field that had been asked for its
//         descendants answered FALSE from then on while its item was still
//         nobody else's -- §24 measured that and left it. A field CAN tell:
//         m_pP3PmsgAttr and m_pP3PmsgDesc are its own, it made them, and each
//         holds one reference on the heap while it names it. Subtracting them
//         is exact rather than approximate.
//       : UNDERCOUNTING IS SAFE AND OVERCOUNTING IS NOT, which is why nothing
//         is subtracted on trust. A holder missed leaves the answer FALSE, and
//         false promises nothing; a holder subtracted that was never mine would
//         report TRUE with a stranger looking, and true is a guarantee. So only
//         the two sub-objects this class owns outright are counted, and each
//         only when its heap is this heap. What is NOT subtracted, because it
//         cannot be reached from here: a cursor, which lives inside the Desc or
//         Attr that made it, and whatever an MsgStck is holding, whose stack
//         fields are protected with no accessor. The cursor costs a FALSE
//         today and is pinned by a case. The MsgStck does not -- measured, a
//         push adds no holder of this heap at all -- but it is unreachable
//         either way, so it could never be subtracted even if it did.
bool
P3PmsgField::IsSole ( ) const
{
    if ( OBJ__.m_aVBLock == 0 )
      return false;                    // Void: no storage to be sole holder of
    if ( OBJ__.IsInline ( ) )
      return true;                     // The block is in here, so nowhere else

    const P2PmsgHANDLE hVBList = OBJ__.m_hVBList;
    int                nMine   = 1;    // This field's own object holds one
    if ( m_pP3PmsgAttr != nullptr &&
         m_pP3PmsgAttr -> r_Object ( ).m_hVBList == hVBList )
      nMine++;
    if ( m_pP3PmsgDesc != nullptr &&
         m_pP3PmsgDesc -> r_Object ( ).m_hVBList == hVBList )
      nMine++;

    return P2PmsgHeap_RefCount ( hVBList ) == nMine;
}
bool
P3PmsgField::IsStacked ( ) const
{
    VBLock *pVBLock = OBJ__VBLock;
    VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs
                                          , VBLock_pItem(pVBLock) );
    return  aStack ? true : false;
}
bool
P3PmsgField::IsAttributed ( ) const
{
    VBLock *pVBLock = OBJ__VBLock;
    VBLaddr aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLockDefs
                                          , VBLock_pItem(pVBLock) );
    return  aExtra ? true : false;
}
bool
P3PmsgField::IsDescendant ( ) const
{
    VBLock *pVBLock = OBJ__VBLock;
    VBLaddr aDescn  = VBLockItem_GetDescn ( pVBLock->oHdr.uVBLockDefs
                                          , VBLock_pItem(pVBLock) );
    return  aDescn ? true : false;
}

P2PmsgHANDLE
P3PmsgField::GetP2PmsgHandle ( ) const
{
    return OBJ__hVBList;
}

CString
P3PmsgField::GetPath ( ) const
{
    return P3Pmsg_GetPath ( this );
}

VBLaddr
P2PmsgField_GetVBLockParentnn ( const P3PmsgField *pField )
{
    VBLock *pVBLock   = (VBLock *)pField->r_Object().GetVBLock();
    UCHAR   uVBLock   = pVBLock->oHdr.uVBLockDefs;
    ASSERT(VBLock_IsLinked(pVBLock));
    VBLockItem *pItem = VBLock_pItem ( pVBLock );
    return VBLockItem_GetParent ( uVBLock, pItem );
}
VBLock*
P2PmsgField_GetVBLockParent ( const P3PmsgField *pField )
{
    VBLaddr aParent = P2PmsgField_GetVBLockParentnn ( pField );
    return (VBLock *)pField->r_Object().Msg2Phys(aParent);
}

void
P2PmsgNode_Swap ( P3PmsgField& oField1, P3PmsgField& oField2 )
{
    VBLock *pVBLock1 = (VBLock *)oField1.r_Object().GetVBLock   ( );
    VBLaddr aVBLock1 = oField1.r_Object().GetVBLocknn ( );
    VBLock *pVBLock2 = (VBLock *)oField2.r_Object().GetVBLock   ( );
    VBLaddr aVBLock2 = oField2.r_Object().GetVBLocknn ( );
    VBLock *pParent1 = P2PmsgField_GetVBLockParent ( &oField1 );
    VBLock *pParent2 = P2PmsgField_GetVBLockParent ( &oField2 );
    UCHAR   uVBLock  = P2PmsgHeap_Addrnn ( oField1.GetP2PmsgHandle() );

    // To be sure, to be sure
    if ( pParent1                  != pParent2                  ||
         oField1.GetP2PmsgHandle() != oField2.GetP2PmsgHandle()    )
      EVERR->Module ( __FUNCTION__ )
           ->Message("Invalid attempt to swap parents" )
           ->Throw();
    if ( aVBLock1 == aVBLock2 )
      return;

    // Parent Node(First) housekeeping
ASSERT(0);
/* TODO: LJM Node cutout
    VBLockNode *pNodeParent = VBLock_pNode ( pParent1 );
    if ( VBLockNode_GetFirst(uVBLock,pNodeParent,0) == aVBLock1 )
      VBLockNode_SetFirst ( uVBLock, pNodeParent, aVBLock2 );
    else if ( VBLockNode_GetFirst(uVBLock,pNodeParent,0) == aVBLock2 )
      VBLockNode_SetFirst ( uVBLock, pNodeParent, aVBLock1 );

    // Parent Node(Last) housekeeping
    if ( VBLockNode_GetLast(uVBLock,pNodeParent,0) == aVBLock1 )
      VBLockNode_SetLast ( uVBLock, pNodeParent, aVBLock2 );
    else if ( VBLockNode_GetLast(uVBLock,pNodeParent,0) == aVBLock2 )
      VBLockNode_SetLast ( uVBLock, pNodeParent, aVBLock1 );
TODO: LJM Node cutout*/
    // Linkages
    VBLockItem *pVBLockItem1 = VBLock_pItem ( pVBLock1 );
    VBLaddr aVBLock1p = VBLockItem_GetPrev ( uVBLock, pVBLockItem1 );
    VBLaddr aVBLock1n = VBLockItem_GetNext ( uVBLock, pVBLockItem1 );
    VBLockItem *pVBLockItem2 = VBLock_pItem ( pVBLock2 );
    VBLaddr aVBLock2p = VBLockItem_GetPrev ( uVBLock, pVBLockItem2 );
    VBLaddr aVBLock2n = VBLockItem_GetNext ( uVBLock, pVBLockItem2 );

    if ( aVBLock1p != aVBLock2 )
    {
      VBLockItem_SetPrev ( uVBLock, pVBLockItem2, aVBLock1p );
      if ( aVBLock1p )
      {
        VBLock *pVBLockPrev = (VBLock *)oField1.r_Object().Msg2Phys(aVBLock1p);
        VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aVBLock2 );
      }
    }
    else
      VBLockItem_SetPrev ( uVBLock, pVBLockItem2, aVBLock1 );

    if ( aVBLock1n != aVBLock2 )
    {
      VBLockItem_SetNext ( uVBLock, pVBLockItem2, aVBLock1n );
      if ( aVBLock1n )
      {
        VBLock *pVBLockNext = (VBLock *)oField2.r_Object().Msg2Phys(aVBLock1n);
        VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aVBLock2 ); //aVBLock1 );
      }
    }
    else
      VBLockItem_SetNext ( uVBLock, pVBLockItem2, aVBLock1 );

    if ( aVBLock2p != aVBLock1 )
    {
      VBLockItem_SetPrev ( uVBLock, pVBLockItem1, aVBLock2p );
      if ( aVBLock2p )
      {
        VBLock *pVBLockPrev = (VBLock *)oField2.r_Object().Msg2Phys(aVBLock2p);
        VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aVBLock1 );
      }
    }
    else
      VBLockItem_SetPrev ( uVBLock, pVBLockItem1, aVBLock2 );

    if ( aVBLock2n != aVBLock1 )
    {
      VBLockItem_SetNext ( uVBLock, pVBLockItem1, aVBLock2n );
      if ( aVBLock2n )
      {
        VBLock *pVBLockNext = (VBLock *)oField2.r_Object().Msg2Phys(aVBLock2n);
        VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aVBLock1 );
      }
    }
    else
      VBLockItem_SetNext ( uVBLock, pVBLockItem1, aVBLock2 );
}


P3PmsgField16::P3PmsgField16 ( ) noexcept
{
    m_oObject.m_uVBLock = VBLock_Addr16;
};
P3PmsgField16::~P3PmsgField16 ( ) {};


///////////////////////////////////////////////////////////////////////
//  P3PmsgNode object manager
//  NOTES: Acts as VBlockNode item wrapper

//VBLock*
//P2PmsgNode_GetVBLockParent  ( P3PmsgNode *pNode );
//UINT
//P2PmsgNode_GetVBLockParentnn( P3PmsgNode *pNode );

//
//  Contructors and destructor
/*P3PmsgNode::P3PmsgNode ( )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
}

P3PmsgNode::P3PmsgNode ( LPCTNAM lpszName, int nSize )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = P3PmsgField ( lpszName, nSize );
}

P3PmsgNode::P3PmsgNode ( LPCTNAM lpszName, const P3PmsgData& oData )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = P3PmsgField ( lpszName, oData );
}

P3PmsgNode::P3PmsgNode ( const P3PmsgNode& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = rhs;
}
P3PmsgNode::P3PmsgNode ( const P3PmsgField& oField )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = oField;
}
P3PmsgNode::P3PmsgNode ( const P2PmsgNodeHdl& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
    Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
            , rhs.uiParam3 );
}
P3PmsgNode::P3PmsgNode ( P2PmsgHANDLE hVBList, VBLaddr aNode, VBLsize nNodeSize )
          : P3PmsgField ( hVBList, 0, 0 )
{
    //RenderThisSafe ( );
    //m_xNode       = aNode;
    //m_aNode       = aNode;
    if ( hVBList && aNode && nNodeSize <= 0 )
      nNodeSize = P2PmsgHeap_Sizeof ( hVBList,aNode );
    m_oObject.Connectx ( hVBList, aNode, nNodeSize );
    //m_nNodeSize   = nNodeSize;
    m_pCurs       = 0;
    //OBJ__aVBLock     = aNode;
    //m_oObject.m_nVBLockSize = nNodeSize;
}
P3PmsgNode::P3PmsgNode ( const P3PmsgObject& rhs )
          : P3PmsgField( rhs )
{
    if ( !rhs.IsNode() )
    {
      RenderThisSafe ( );
      m_oObject.Nullify ( );
      ASSERT(0);
      return;
    }
    //m_xNode   = rhs.m_aVBLock;
    //m_aNode   = rhs.m_aVBLock;
    m_pCurs   = 0;
}
P3PmsgNode::~P3PmsgNode ( )
{
    //if ( !OBJ__hVBList &&
    //      OBJ__aVBLock    )
    //  OBJ__aVBLock = OBJ__Free ( OBJ__aVBLock );
    //if ( !OBJ__hVBList &&
    //      m_aNode      )
    //  OBJ__Free ( m_aNode );
    if (  m_pCurs )
      delete m_pCurs;
}
void
P3PmsgNode::RenderThisSafe ( )
{
    VBLock& oVBLock = *(VBLock *)m_oObject.m_oVBLock;       // Delegate field object
    ZeroMemory ( &oVBLock, sizeof(oVBLock) );
    VBLock_Init( &oVBLock
               , OBJ__uVBLock|VBLock_Item|VBLock_Linked|VBLock_Alloc, sizeof(oVBLock) );
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(&oVBLock), VBLock_Node );
    VBLockNode_Init ( OBJ__uVBLock
                    , VBLock_pNode(&oVBLock), AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(&oVBLock), AttrField_DEFAULT );
    VBLockName_Init ( VBLock_pName(&oVBLock)
                    , VBLockAttr_DEFAULT | VBLockAttr_NULL
                    , 0, sizeof(VBLockField::oVBLockName) );
    VBLockData_Init ( VBLock_pData(&oVBLock)
                    , VBLockAttr_DEFAULT, VBLockData_NULL
                    , sizeof(VBLockField::oVBLockData) );
ASSERT(VBLockName_Sizenn(VBLock_pName(&oVBLock))==sizeof(VBLockField::oVBLockName));
    //m_xNode       = 0;
    //m_aNode       = (UINT)&oVBLock;
    //m_nNodeSize   = sizeof(oVBLock);
    m_pCurs       = 0;
    m_oObject.Connecta ( 0/*m_hVBList*//*, (VBLaddr)&oVBLock, sizeof(oVBLock) );
}*/
/*void
P3PmsgNode::Nullify ( )
{
    if ( !OBJ__hVBList &&
          OBJ__aVBLock    )
    {
      //ASSERT(0);//Suspect this is no longer required
      //m_aNode = OBJ__Free ( m_aNode );
    }
    //m_xNode     = 0;
    //m_aNode     = 0;
    //m_nNodeSize = 0;
    if (  m_pCurs )
      delete m_pCurs;
    m_pCurs = nullptr;
    P3PmsgField::Nullify ( );
}
void
P3PmsgNode::Connect ( P2PmsgHANDLE hVBList, VBLaddr aNode, VBLsize nNodeSize )
{
    //Reset ( );
    if ( m_pCurs )
    {
      delete m_pCurs;
             m_pCurs = nullptr;
    }
    //m_xNode     = aNode;
    //m_aNode     = aNode;
    //m_nNodeSize = nNodeSize;

    // Propagate
    P3PmsgField::Connect ( hVBList, aNode, nNodeSize ); //0, 0 );
    // TODO: This needs auditing
    //OBJ__aVBLock     = aNode;
    //m_oObject.m_nVBLockSize = nNodeSize;
    // TODO: Above needs auditing
}

P3PmsgNode&
P3PmsgNode::AddNode ( LPCTNAM lpszName, const P3PmsgData& oData )
{
  //  P3PmsgNode oNode ( P3PmsgField(lpszName,oData) );
  (*this) += P3PmsgNode( P3PmsgField(lpszName,oData) );
    return *this;
}

P3PmsgField&
P3PmsgNode::AddField( LPCTNAM lpszName, const P3PmsgData& oData )
{
  (*this) += P3PmsgField(lpszName, oData );
    return *this;
}

//  Operators
P3PmsgNode&
P3PmsgNode::operator = ( const P3PmsgNode& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    Truncate ( );

    // Implementation
    VBLsize nSizeof = rhs.Sizeof ( OBJ__uVBLock ); //TODO:LJM deprecated
    //if ( GetVBLockNodeSize() < nSizeof ) //TODO:LJM deprectaed
    if ( m_oObject.m_nVBLockSize < nSizeof )
    {
      ASSERT(0);
      //TODO:LJM may be re-activating NewVBLockNode ( nSizeof );
    }
    //memcpy ( GetVBLockNode(), rhs.GetVBLockNode()
    //       , nSizeof-((P3PmsgField&)rhs).Sizeof() );
    //(P3PmsgField&)*this = (P3PmsgField&)rhs;
    m_bFieldDirty = true;

    // Recursively drop existing items and copy
    (*this) = (P3PmsgField&)rhs;
    P3PmsgCurs& oCurs = ((P3PmsgNode&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.IsNode() )
        (*this) += oCurs.r_node();
      else if ( oCurs.IsList() )
        (*this) += oCurs.r_list();
      else if ( oCurs.IsVect() )
        (*this) += oCurs.r_vect();
      else if ( oCurs.IsItem() )
        (*this) += oCurs.r_item();
    }

    // Tidy up, and
//AssertValid();rhs.AssertValid();//TODO:Delete debugging
    return *this;
}
P3PmsgNode&
P3PmsgNode::operator = ( const P3PmsgField& rhs )
{
   (P3PmsgField&)*this = rhs;
    return *this;
}
P3PmsgNode&
P3PmsgNode::operator = ( const P2PmsgNodeHdl& rhs )
{
    // To be sure, to be sure
    if ( OBJ__hVBList == (P2PmsgHANDLE)rhs.uiParam1 &&
         OBJ__aVBLock ==     rhs.uiParam2    )
      return *this;
    if ( !r_Object().IsNode() )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Invalid in overloaded context" )
            -> Throw();
    Nullify ( );
    Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
            , rhs.uiParam3 );
    return *this;
}
P3PmsgNode&
P3PmsgNode::operator = ( const P3PmsgObject& rhs )
{
    // To be sure, to be sure
    if ( OBJ__hVBList == rhs.m_hVBList  &&
         OBJ__aVBLock == rhs.m_aVBLock     )
      return *this;
    if ( !rhs.IsNode() )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Invalid in overloaded context" )
            -> Throw();
    Nullify ( );
    m_oObject = rhs;
    //Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
    //        , rhs.uiParam3 );
    return *this;
}

P3PmsgNode&
P3PmsgNode::operator += ( const P3PmsgNode& rhs )
{
    // Locals
    VBLsize     nSizeofItem;
    VBLaddr     aItem__;

    // Establish placeholder item
    nSizeofItem = VBLockItem_Sizeof ( OBJ__uVBLock )
                + rhs.P3PmsgNode::Sizeof ( OBJ__uVBLock );
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock     *pVBLock = (VBLock *)OBJ__Msg2Phys(aItem__);
//ASSERT(VBLock_IsItem(pVBLock));
    //VBLockNode *pNode   = GetVBLockNode();
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(pVBLock), VBLock_Node );
    VBLockNode_Init ( OBJ__uVBLock, VBLock_pNode(pVBLock), AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , rhs.c_name(), rhs.P3PmsgName::Sizeof() );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, rhs.DataType()
                    , rhs.P3PmsgData::Sizeof() );
//P2PmsgHeap_AssertValid(m_hVBList);

    // Insert created item
    if ( GetAccess(AttrField_SORT) )
      P2PmsgNode_SortinItem ( this, rhs.r_name(), aItem__ );
    else
      P2PmsgNode_LinkinItem ( this
                            , VBLockNode_GetLast(OBJ__VBLock->oHdr.uVBLockDefs,P3PmsgNode__GetVBLockNode(this))
                            , aItem__
                            , 0 );
//P2PmsgHeap_AssertValid(m_hVBList);
//AssertValid();//TODO:LJM delete, testing
//rhs.AssertValid();//TODO:LJM delete, testing

    // Recursive copy
    P3PmsgNode oNode ( OBJ__hVBList, aItem__, nSizeofItem );
               oNode = (P3PmsgField&)rhs;
//AssertValid();//TODO:LJM delete, testing
//oNode.AssertValid();
//ASSERT(oNode.Sizeof(m_uVBLock)==rhs.Sizeof(m_uVBLock));
    P3PmsgCurs& oCurs = ((P3PmsgNode&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
//oCurs.AssertValid();
//P2PmsgHeap_AssertValid(m_hVBList);
//ASSERT(oCurs.Item()==i);
      if ( oCurs.IsNode() )
        oNode += oCurs.r_node();
      else if ( oCurs.IsList() )
        oNode += oCurs.r_list();
      else if ( oCurs.IsVect() )
        oNode += oCurs.r_vect();
      else if ( oCurs.IsItem() )
        oNode += oCurs.r_item();
      else
        ASSERT(0);
//P2PmsgHeap_AssertValid(m_hVBList);
//oNode.GetCurs().AssertValid();
//oCurs.AssertValid();
//oNode.AssertValid();
//AssertValid();//TODO:LJM delete, testing
//UINT nuItemsNode1=oNode.GetCount();
    }

    // Tidy up, and
//AssertValid();//TODO:LJM delete, testing
//rhs.AssertValid();//TODO:LJM delete, testing
    return *this;
}

#define pVBLockTHIS ((VBLock *)m_oObject.GetVBLock())

P3PmsgNode&
P3PmsgNode::operator += ( const P3PmsgList& rhs )
{
ASSERT(rhs.r_Object().IsList());
    //VBLock     *pVBLockThis = (VBLock *)Msg2Phys ( m_aNode );
    VBLsize     nSizeofItem;
    VBLaddr     aItem__;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( OBJ__uVBLock )
                + rhs.P3PmsgList::Sizeof ( OBJ__uVBLock );
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock     *pVBLock = (VBLock *)OBJ__Msg2Phys(aItem__);
    //VBLockList *pList   = GetVBLockList();
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(pVBLock), VBLock_List );
    VBLockList_Init ( OBJ__uVBLock, VBLock_pList(pVBLock), AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , rhs.c_name(), rhs.P3PmsgName::Sizeof() );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, rhs.DataType()
                    , rhs.P3PmsgData::Sizeof() );
    VBLockNode *pNode = P3PmsgNode__GetVBLockNode(this);

    // Insert created item
    if ( GetAccess(AttrField_SORT) )
      P2PmsgNode_SortinItem ( this, rhs.r_name(), aItem__ );
    else
      P2PmsgNode_LinkinItem ( this
                            , VBLockNode_GetLast(OBJ__VBLock->oHdr.uVBLockDefs,pNode)
                            , aItem__
                            , 0 );

    P3PmsgList oList ( OBJ__hVBList, aItem__, nSizeofItem );
//oList.AssertValid();
               oList = rhs;
//oList.AssertValid();
//AssertValid();
//rhs.AssertValid();
    return *this;
}

P3PmsgNode&
P3PmsgNode::operator += ( const P3PmsgVect& rhs )
{
ASSERT(rhs.r_Object().IsNode());
    //VBLock     *pVBLockThis = (VBLock *)Msg2Phys ( m_aNode );
    VBLsize     nSizeofItem;
    VBLaddr     aItem__;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( OBJ__uVBLock )
                + rhs.P3PmsgVect::Sizeof ( OBJ__uVBLock );
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock     *pVBLock = (VBLock *)OBJ__Msg2Phys(aItem__);
    //VBLockList *pVect   = GetVBLockVect();
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(pVBLock), VBLock_Vect );
    VBLockVect_Init ( OBJ__uVBLock, VBLock_pVect(pVBLock), 32, AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , rhs.c_name(), rhs.P3PmsgName::Sizeof() );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, rhs.DataType()
                    , rhs.P3PmsgData::Sizeof() );
    VBLockNode *pNode = P3PmsgNode__GetVBLockNode(this);

    // Insert created item
    if ( GetAccess(AttrField_SORT) )
      P2PmsgNode_SortinItem ( this, rhs.r_name(), aItem__ );
    else
      P2PmsgNode_LinkinItem ( this
                            , VBLockNode_GetLast(OBJ__VBLock->oHdr.uVBLockDefs,pNode)
                            , aItem__
                            , 0 );

    P3PmsgVect oVect ( OBJ__hVBList, aItem__, nSizeofItem );
oVect.AssertValid();
               oVect = rhs;
oVect.AssertValid();
AssertValid();
rhs.AssertValid();
    return *this;
}

P3PmsgNode&
P3PmsgNode::operator += ( const P3PmsgField& rhs )
{
ASSERT(!rhs.r_Object().IsNode());
    //VBLock     *pVBLockThis = (VBLock *)Msg2Phys ( m_aNode );
    VBLsize     nSizeofItem;
    VBLaddr     aItem__;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( pVBLockTHIS->oHdr.uVBLockDefs )
                + rhs.P3PmsgField::Sizeof();
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys(aItem__);
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(pVBLock), VBLock_Field );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , rhs.c_name(), rhs.P3PmsgName::Sizeof() );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, rhs.DataType()
                    , rhs.P3PmsgData::Sizeof() );
    //VBLock_pItem(pVBLock) -> uVBLockItem = VBLockItem_Field;
    //VBLock_pName(pVBLock) -> u.vBlob08.nBlobSize = rhs.c_size();
    //VBLock_pName(pVBLock) -> u.vBlob08.nBlobUsed = rhs.c_size();
ASSERT(VBLock_IsItem(pVBLock));
ASSERT(VBLockItem_Sizenn(pVBLock)>=nSizeofItem);
    VBLockNode *pNode   = P3PmsgNode__GetVBLockNode(this);

    // Insert created item
    if ( GetAccess(AttrField_SORT) )
      P2PmsgNode_SortinItem ( this, rhs.r_name(), aItem__ );
    else
      P2PmsgNode_LinkinItem ( this
                            , VBLockNode_GetLast(pVBLockTHIS->oHdr.uVBLockDefs,pNode)
                            , aItem__
                            , 0 );
      //aField = aItem__ + VBLockItem_uos(pVBLock->oHdr.uVBLock,VBLock_pItem(pItem));

    P3PmsgField oField ( OBJ__hVBList, aItem__, nSizeofItem );
//oField.AssertValid();
                oField = rhs;
//oField.AssertValid();
//AssertValid();
//rhs.AssertValid();
    return *this;
}
P3PmsgField&
P3PmsgNode::operator [] ( LPCTNAM lpszItemName )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszItemName)
            -> Message(L"Item [%s] does not exist", lpszItemName )
            -> Throw();
    if ( m_pCurs->IsNode() )
      return m_pCurs->r_node ( );
    if ( m_pCurs->IsList() )
      return m_pCurs->r_list ( );
    if ( m_pCurs->IsItem() )
      return m_pCurs->r_item ( );
    ASSERT(0);
    return m_pCurs->r_item();
}

P3PmsgNode::operator P2PmsgNodeHdl ( )
{
    VBLaddr& aVBLock = OBJ__aVBLock;
    P2PmsgNodeHdl oHdl = { (UINT_PTR)OBJ__hVBList, aVBLock, P2PmsgHeap_Sizeof(OBJ__hVBList,aVBLock) };
    return oHdl;
}

//
//  Navigation
P3PmsgObject
P3PmsgNode::SelectObject ( LPCTNAM lpszObjectName )
{
    return P3Pmsg_SelectObject ( &r_Object(), lpszObjectName );
}
P3PmsgField
P3PmsgNode::Select ( LPCTNAM lpszItemName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
      return P3PmsgField();
    return *m_pCurs;
}
P3PmsgField&
P3PmsgNode::SelectItem ( LPCTNAM lpszItemName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
    {
      ASSERT(0);//TODO:Delete-me
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszItemName)
            -> Message(L"Item [%s] does not exist", lpszItemName )
            -> Throw();
    }
    return *m_pCurs;
}

P3PmsgNode&
P3PmsgNode::SelectNode ( LPCTNAM lpszNodeName )
{
    if (  m_pCurs == nullptr)
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszNodeName) ||
         !m_pCurs->IsNode()              )
    {
      ASSERT(0);//TODO:Delete-me
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszNodeName)
            -> Message(L"Node [%s] does not exist", lpszNodeName )
            -> Throw();
    }
    return m_pCurs->r_node ( );
}

P3PmsgField&
P3PmsgNode::DeclareItem ( LPCTNAM lpszFieldname, const P3PmsgData& oData, bool bUpdate )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszFieldname) )
    { // This sequence needs optimising
      (*this) += P3PmsgField ( lpszFieldname, oData );
      m_pCurs->Goto(lpszFieldname);
      return m_pCurs->r_item();
    }
    if ( bUpdate )
      m_pCurs->r_item().r_data() = oData;
    return m_pCurs->r_item();
}
P3PmsgNode&
P3PmsgNode::DeclareNode ( LPCTNAM lpszNodename, const P3PmsgData& oData, bool bUpdate )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszNodename) )
    { // This sequence needs optimising
      (*this) += P3PmsgNode ( lpszNodename, oData );
      m_pCurs->Goto(lpszNodename);
      return m_pCurs->r_node();
    }
    if ( bUpdate )
      m_pCurs->r_item().r_data() = oData;
    return m_pCurs->r_node();
}

bool
P3PmsgNode::Exists ( LPCTNAM lpszItemName ) const
{
    if ( IsVoid() )
      return false;
    return !P3Pmsg_SelectObject(&r_Object(),lpszItemName).IsVoid();
    //if ( !m_pCurs )
    //  m_pCurs = new P3PmsgCurs ( *this );
    //return m_pCurs->Goto(lpszItemName);
}

bool
P3PmsgNode::Delete ( LPCTNAM lpszItemName )
{
    // Initialisation
    if ( IsVoid() )
      return false;
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgNode of all items
    if ( !m_pCurs->Goto(lpszItemName) )
      return false;
    m_pCurs -> Delete ( );
    return true;
}

void
P3PmsgNode::Truncate ( )
{
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgNode of all items
    while ( m_pCurs->Goto((int)0) )
      m_pCurs -> Delete ( );
    delete m_pCurs;
           m_pCurs = 0;
}

P3PmsgCurs&
P3PmsgNode::r_Curs ( )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    return *m_pCurs;
}*/

///////////////////////////////////////////////////////////////////////
//  Memory management
//  NOTES: Delegates through P2PmsgVBL if not-NULL, otherwise interacts
//         directly with heap
//UINT
//P3PmsgNode::Free ( UINT aVBLockAddr )
//{
//    ASSERT(IsNode());
//    OBJ__Free ( aVBLockAddr );
//    //if ( aVBLockAddr == 0 )
//    //  return 0;
//    //else if ( aVBLockAddr == m_xNode )
//    //  m_xNode = 0;
//    //else if ( OBJ__hVBList )
//    //  VBListFree ( OBJ__hVBList, aVBLockAddr );
//    //else
//    //{
//    //  if ( aVBLockAddr != (UINT)&m_oNode )
//    //    delete [] (char *)aVBLockAddr;
//    //}
//    //if ( aVBLockAddr == m_aNode )
//    //  m_aNode = 0;
//    return 0;
//}
/*void
P3PmsgNode::Drop ( )
{
   ASSERT(r_Object().IsNode());
   if ( IsAttributed() )
     r_Attr().Drop();
   if ( IsDescendant() )
     r_Desc().Drop();
   if ( IsStacked() )
     r_Stck().Drop( );

   P2PmsgField_DropName ( this );
   P2PmsgField_DropData ( this );
   Truncate ( );

   // Isolate parent
   VBLock *pVBLockParent = P2PmsgField_GetVBLockParent(this);
   if ( pVBLockParent )
   {
     VBLockItem *pItemParent = VBLock_pItem ( pVBLockParent );
     if ( VBLock_IsAttr(pVBLockParent) )
     {
       ASSERT(0);
       //P3PmsgAttr oAttr(m_hVBList,0,0);
     }
     else if ( VBLockItem_IsNode(pItemParent) )
     {
       VBLaddr aParent = P2PmsgField_GetVBLockParentnn ( this );
       P3PmsgNode  oNodeParent( OBJ__hVBList, aParent, 0 );
       VBLockNode *pVBLockParent1 = VBLock_pNode((VBLock *)oNodeParent.r_Object().GetVBLock());
       P2PmsgNode_UnLinkItem ( &oNodeParent, pVBLockParent1, OBJ__VBLocknn );
//oNodeParent.AssertValid();
     }
     else
       ASSERT(0);
   }

   // Tidy up, and
   OBJ__Free ( OBJ__aVBLock );
}

/*UINT
P3PmsgNode::AllocItem ( const P3PmsgNode& oNode )
{
    // Locals
    UINT        nSizeofItem;
    UINT        aItem__;
AssertValid();//TODO:LJM delete, testing
oNode.AssertValid();//TODO:LJM delete, testing

    // Establish placeholder item
    nSizeofItem = VBLockItem_Sizeof ( m_uVBLock )
                + oNode.P3PmsgNode::Sizeof ( m_uVBLock );
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock     *pVBLock = (VBLock *)Msg2Phys(aItem__);
ASSERT(VBLock_IsItem(pVBLock));
    //VBLockNode *pNode   = GetVBLockNode();
    VBLockItem_Init ( m_uVBLock, VBLock_pItem(pVBLock), VBLock_Node );
    VBLockNode_Init ( m_uVBLock, VBLock_pNode(pVBLock), VBLockAttr_DEFAULT );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , oNode.c_name(), oNode.P3PmsgName::Sizeof() );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oNode.DataType()
                    , oNode.P3PmsgData::Sizeof() );
P2PmsgHeap_AssertValid(m_hVBList);

    // Tidy up, and
    return aItem__;
}*/

// Addressing and allocations
//P2Pos
//P3PmsgNode::GetP2Pos ( ) const
//{
//    ASSERT(IsNode());
//    return OBJ__aVBLock;
//    //return m_aNode;
//}
//VBLock*
//P3PmsgNode::GetVBLock ( bool ) const
//{
//    //ASSERT(m_aNode==OBJ__aVBLock);
//    return m_oObject.GetVBLock();
//    //return (VBLock *)Msg2Phys ( m_aNode );
//}
//UINT
//P3PmsgNode::GetVBLocknn( ) const
//{
//    ASSERT(IsNode());
//    return m_oObject.GetVBLocknn();
//    //return m_aNode;
//}
//VBLsize
//P3PmsgNode::GetVBLockSize ( ) const
//{
//    return m_oObject.GetVBLockSize();
//    //return m_nNodeSize;
//}
//VBLockNode*
//P3PmsgNode::NewVBLockNode ( int nSizeof )
//{
//ASSERT(0);
//    //m_aNode     = OBJ__Free  ( m_aNode );
//    //m_aNode     = OBJ__Alloc ( VBLock_Node, nSizeof );
//    //m_nNodeSize = nSizeof;
//    //VBLockNode *pNode = GetVBLockNode ( );
//    //            pNode -> uVBLockAttr = 0;
//    //ASSERT(pNode==GetVBLockNode());
//    //return pNode;
//    return 0;
//}
/*VBLockNode*
P3PmsgNode__GetVBLockNode ( const P3PmsgNode *pNode )
{
    //VBLock *pVBLock = (VBLock *)Msg2Phys ( m_aNode );
    VBLock *pVBLock = (VBLock *)pNode->r_Object().GetVBLock();
    return VBLock_pNode ( pVBLock );
}
//UINT
//P3PmsgNode::GetVBLockNodeSize ( ) const
//{
//    return m_oObject.m_nVBLockSize;
//    //return m_nNodeSize;
//}

//VBLockField*
//P3PmsgNode::GetVBLockField ( bool /*bIndirect*//* ) const
//{
//    //VBLock *pVBLock = (VBLock *)Msg2Phys ( m_aNode );
//    VBLock *pVBLock = m_oObject.GetVBLock();
//    ASSERT(VBLock_IsLinked(pVBLock));
//    ASSERT(VBLock_IsAlloc(pVBLock));
//    return VBLock_pField ( pVBLock );
//}
//UINT
//P3PmsgNode::GetVBLockFieldSize ( ) const
//{
//    ASSERT(0);
//    //VBLock *pVBLock = (VBLock *)Msg2Phys ( m_aNode );
//    //UINT    nSizenn = VBLockField_Sizenn ( pVBLock );
//    VBLock& oVBLock = m_oObject.m_oVBLock;
//    //return m_nNodeSize -   P3PmsgNode::Sizeof_u()
//    return m_oObject.GetVBLockSize() -   P3PmsgNode::Sizeof_u()
//                       - ( sizeof(oVBLock) 
//                         -   sizeof(oVBLock.ud)
//                         -   sizeof(oVBLock.ud.oField) );
//}

///////////////////////////////////////////////////////////////////////
//  Troubleshooting
void
P3PmsgNode::AssertValid ( ) const
{
    VBLock     *pVBLock = OBJ__VBLock;
    if ( pVBLock == nullptr )
      return;
    VBLockNode *pNode   = VBLock_pNode ( pVBLock );

    // Header
    int     nItems = VBLockNode_GetItems ( OBJ__uVBLock, pNode );
    int     nFirst;
    VBLaddr aFirst = VBLockNode_GetFirst ( OBJ__uVBLock, pNode, &nFirst );
    int     nLast;
    VBLaddr aLast  = VBLockNode_GetLast  ( OBJ__uVBLock, pNode, &nLast );
    if ( !nItems && (nFirst || aFirst || nLast!=-1 || aLast) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted header nItems=%i aFirst=%i aLast=%i",
                        nItems, aFirst, aLast )
            -> Throw();

    // Delegation
    __super::AssertValid ( );

    // Navigation
    VBLaddr aVBLocknn = OBJ__VBLocknn;
    P3PmsgCurs& oCurs = ((P3PmsgNode *)this)->r_Curs();
                oCurs.AssertValid ( );
	  int i;
    for ( i = 0; oCurs.Goto(i); i++ )
    {
      r_Object().AssertCommon ( oCurs.r_Object() );
      if ( oCurs.IsNode() )
      {
        VBLaddr aVBLockParentnn = P2PmsgField_GetVBLockParentnn(&oCurs.r_node());
        if ( aVBLocknn != aVBLockParentnn )
          EVERR -> Module ( __FUNCTION__ )
                -> Message(L"Corrupted link %s(%i) back to %s(%i)"
                          , oCurs.r_node().c_name(), aVBLocknn
                          , c_name(), aVBLockParentnn )
                -> Throw();
        oCurs.r_node().AssertValid();
      }
      else if ( oCurs.IsList() )
        oCurs.r_list().AssertValid();
      else if ( oCurs.IsVect() )
        oCurs.r_vect().AssertValid();
      else if ( oCurs.IsItem() )
        oCurs.r_item().AssertValid();
      else
        ASSERT(0);
    }
    if ( nItems != i )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted item count actual=%i expected=%i",
                        nItems, i )
            -> Throw();
}

void
P3PmsgNode::Print ( FILE *fd, int nDepthOS, int nDepthOSinc  )
{
    // P2PmsgNode details
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    VBLsize nVBLockSize = Sizeof(OBJ__uVBLock);
    P3Pmsg_fwprintf ( fd, L"%s(%I32i) = (%s %s) %s {\n"
             , c_name()
             , nVBLockSize
             , r_data().ToStringType(), r_data().ToStringDefs()
             , r_data().ToString() );

    // Attributes
    if ( !r_Attr().IsEmpty() )
      r_Attr().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
    // Descendant
    if ( !r_Desc().IsEmpty() )
      r_Desc().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );

    // Recursion
    P3PmsgCurs& oCurs = ((P3PmsgNode *)this)->r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.IsNode() )
        oCurs.r_node().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      else if ( oCurs.IsList() )
        oCurs.r_list().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      else if ( oCurs.IsVect() )
        oCurs.r_vect().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      else if ( oCurs.IsItem() )
        oCurs.r_item().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
    }

    // Tidy up
    for ( int i = 0; i < nDepthOS; i++ )
      fwprintf ( fd, L"  " );
    fwprintf ( fd, L"}\n" );
}*/

///////////////////////////////////////////////////////////////////////
//  Std::List modifiers

/*P2PmsgNodeHdl
P3PmsgNode::PushBack ( const P3PmsgNode& oNode )
{
    // Block allocation and linkage
    VBLsize nSizeofItem = P2PmsgNode_SizeofItem ( OBJ__uVBLock, &oNode );
    VBLaddr aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgNode_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), &oNode );
    P2PmsgNode_LinkinItem ( this
                          , VBLockNode_GetLast(OBJ__VBLock->oHdr.uVBLockDefs,P3PmsgNode__GetVBLockNode(this))
                          , aItem
                          , 0 );
//P2PmsgHeap_AssertValid(m_hVBList);
//AssertValid();//TODO:LJM delete, testing
//oNode.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof( OBJ__hVBList, aItem );
    P3PmsgNode oNodeItem ( OBJ__hVBList, aItem, nSizeofItem );
               oNodeItem = oNode;

    // Tidy up and
    P2PmsgNodeHdl oHdl = { (UINT_PTR)OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    return oHdl;
}

P2PmsgFieldHdl
P3PmsgNode::PushBack ( const P3PmsgField& oField )
{
    // Block allocation and linkage
    P2PmsgHANDLE& hVBList = m_oObject.m_hVBList;
    VBLsize nSizeofItem = P2PmsgField_SizeofItem ( m_oObject.m_uVBLock, &oField );
    VBLaddr aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgField_InitItem  ( (VBLock *)OBJ__Msg2Phys(aItem), &oField );
    P2PmsgNode_LinkinItem ( this
                          , VBLockNode_GetLast(OBJ__VBLock->oHdr.uVBLockDefs,P3PmsgNode__GetVBLockNode(this))
                          , aItem
                          , 0 );
//P2PmsgHeap_AssertValid(m_hVBList);
//AssertValid();//TODO:LJM delete, testing
//oField.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof( hVBList, aItem );
    P3PmsgField oFieldItem ( hVBList, aItem, nSizeofItem );
                oFieldItem = oField;

    // Tidy up and
    P2PmsgFieldHdl oHdl = { (UINT_PTR)hVBList, aItem, P2PmsgHeap_Sizeof(hVBList,aItem) };
    return oHdl;
}*/

///////////////////////////////////////////////////////////////////////
//  Properties

/*P2PmsgNodeHdl
P3PmsgNode::GetP2PmsgNodeHdl ( )
{
    ASSERT(r_Object().IsNode());
    VBLaddr aNode = OBJ__aVBLock;
    P2PmsgNodeHdl oHdl = { (UINT_PTR)OBJ__hVBList, aNode, P2PmsgHeap_Sizeof(OBJ__hVBList,aNode) };
    //P2PmsgNodeHdl oHdl = { (UINT)m_oObject.m_hVBList, m_aNode
    //                     , P2PmsgHeap_Sizeof(m_oObject.m_hVBList,m_aNode) };
    return oHdl;
}
VBLelem
P3PmsgNode::GetCount ( ) const
{
    VBLock     *pVBLock = OBJ__VBLock;
    VBLockNode *pNode   = VBLock_pNode ( pVBLock );
    return VBLockNode_GetItems ( pVBLock->oHdr.uVBLockDefs, pNode );
}
VBLsize
P3PmsgNode::Sizeof ( UCHAR uVBLock ) const
{
    VBLsize nSizeof = VBLockNode_Sizeof ( uVBLock )
                 + P3PmsgField::Sizeof ( );
                 //+ P3PmsgField::Sizeof ( ); //TODO:LJM deprecated
    return nSizeof;
}
//UINT
//P3PmsgNode::Sizeof_u ( ) const
//{
//    /*VBLockNode *pNode = GetVBLockNode ( );
//    if ( pNode->uVBLockAddr == VBLockNode_ADDR08 )
//      return sizeof(m_oNode.u.oNode08);
//    if ( pNode->uVBLockAddr == VBLockNode_ADDR16 )
//      return sizeof(m_oNode.u.oNode16);
//    if ( pNode->uVBLockAddr == VBLockNode_ADDR32 )
//      return sizeof(m_oNode.u.oNode32);*//*
//    ASSERT(0);
//    return 0;
//}
bool
P3PmsgNode::IsDirty ( )
{
    if ( m_bNodeDirty           ||
         P3PmsgField::IsDirty()    )
      return true;
    return false;
}
/*      virtual bool
        IsEmpty ( ) const = 0;*/
//bool
//P3PmsgNode::IsNode ( ) const
//{
//    return true;
//}

//VBLock*
//P2PmsgNode_GetVBLockParent_ ( P3PmsgNode *pNode )
//{
//    UINT aParent = P2PmsgNode_GetVBLockParentnn ( pNode );
//    return (VBLock *)pNode->Msg2Phys(aParent);
//}
//UINT
//P2PmsgNode_GetVBLockParentnn_( P3PmsgNode *pNode )
//{
//    VBLock *pVBLock   = (VBLock *)pNode->GetVBLock();
//    UCHAR   uVBLock   = pVBLock->oHdr.uVBLock;
//    ASSERT(VBLock_IsLinked(pVBLock));
//    VBLockItem *pItem = VBLock_pItem ( pVBLock );
//    return VBLockItem_GetParent ( uVBLock, pItem );
//}*/


///////////////////////////////////////////////////////////////////////
//????????????????????????????????????????????????????????????????????

///////////////////////////////////////////////////////////////////////
//  P3PmsgDesc object manager
//  NOTES: Performs P3PmsgItem descendant operations


//  Constructors and destructor
/*P3PmsgDesc::P3PmsgDesc ( )
{
    RenderThisSafe ( );
}
P3PmsgDesc::P3PmsgDesc ( const P3PmsgDesc& rhs )
{
    RenderThisSafe ( );
	  m_oObject = rhs.m_oObject;
}

P3PmsgDesc::P3PmsgDesc ( P3PmsgField *pField )
{
    RenderThisSafe ( );
    m_pP3PmsgField      = pField;
    VBLaddr aVBLockDesc = P2PmsgDesc_GetDescn ( pField );
    m_oObject.Connectx ( pField->m_oObject.m_hVBList, aVBLockDesc, 0 );
    //m_oObject      = pField -> r_Object();
}

P3PmsgDesc::P3PmsgDesc ( const P3PmsgObject& rhs )
{
    RenderThisSafe ( );
    m_oObject = rhs;
}

P3PmsgDesc::~P3PmsgDesc ( )
{
    Nullify ( );
}
void
P3PmsgDesc::RenderThisSafe ( )
{
    m_pP3PmsgField = 0;
    m_pCurs        = 0;
}

void
P3PmsgDesc::Nullify ( )
{
    m_pP3PmsgField = 0;
    if ( m_pCurs )
      delete m_pCurs;
    m_pCurs        = 0;
    m_oObject.Nullify ( );
}
void
P3PmsgDesc::Connect ( P3PmsgField *pField )
{
    Nullify ( );
    m_pP3PmsgField = pField;
    VBLaddr aVBLockDesc = P2PmsgDesc_GetDescn ( pField );
    m_oObject.Connectx ( pField->m_oObject.m_hVBList, aVBLockDesc, 0 );
}

// Operators
P3PmsgDesc&
P3PmsgDesc:: operator = ( const P3PmsgDesc& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    Truncate ( );
    if ( rhs.IsEmpty() )
      return *this;

    // Implementation
    Create ( );
    m_bDescDirty = true;

    // Recursively drop existing items and copy
    P3PmsgCurs& oCurs = ((P3PmsgDesc&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.IsNode() )
        (*this) += oCurs.r_node();
      else if ( oCurs.IsList() )
        (*this) += oCurs.r_list();
      else if ( oCurs.IsVect() )
        (*this) += oCurs.r_vect();
      else if ( oCurs.IsItem() )
        (*this) += oCurs.r_item();
    }

    // Tidy up, and
//ASSERT(GetCount()==rhs.GetCount());
//AssertValid();rhs.AssertValid();//TODO:Delete debugging
    return *this;
}
P3PmsgDesc&
P3PmsgDesc::operator = ( const P3PmsgObject& rhs )
{
    // To be sure, to be sure
    if ( &m_oObject == &rhs )
      return *this;
    Truncate ( );
    if ( rhs.m_hVBList == NULL )
      return *this;

    // Implementation
    m_oObject = rhs;

    // Tidy up, and
    return *this;
}
P3PmsgDesc&
P3PmsgDesc::operator += ( const P3PmsgNode& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgDesc&
P3PmsgDesc::operator += ( const P3PmsgList& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgDesc&
P3PmsgDesc::operator += ( const P3PmsgVect& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgDesc&
P3PmsgDesc::operator += ( const P3PmsgField& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgNode&
P3PmsgDesc::operator [] ( LPCTNAM lpszName )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszName) )
      EVERR -> MODULE
            -> AFP(lpszName)
            -> Message(L"Item [%s] does not exist", lpszName )
            -> Throw();
    return m_pCurs->r_node ( );
}

P3PmsgDesc::operator bool ( ) const
{
    return r_Object();
}

//  Memory management
void
P3PmsgDesc::Create ( )
{
    //if ( P3PmsgDesc__VBLock(this) ) //TODO:LJM deprecated
    if ( OBJ__aVBLock ) ASSERT(VBLock_IsDesc(OBJ__VBLock));
    if ( OBJ__aVBLock )
      return;
    //TODO:LJM deprecated below
    //UCHAR   uVBLock = m_pP3PmsgField -> r_Object().GetVBLock() -> oHdr.uVBLock;
    //UINT    nSizeof = VBLockItem_Sizeof(uVBLock) + VBLockDesc_Sizeof(uVBLock);
    //UINT    aExtra  = m_pP3PmsgField -> OBJ__Alloc ( VBLock_Desc, nSizeof );
    //VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> OBJ__Msg2Phys(aExtra);
    //TODO:LJM deprecated above
    VBLsize nSizeof = VBLockItem_Sizeof(OBJ__uVBLock) + VBLockDesc_Sizeof(OBJ__uVBLock);
    VBLaddr aDesc;
    if ( m_pP3PmsgField )
      aDesc = m_pP3PmsgField->OBJ__Alloc ( VBLock_Desc, nSizeof );
    else
      aDesc = OBJ__Alloc ( VBLock_Desc, nSizeof );
    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys ( aDesc );
//ASSERT(VBLock_IsAlloc(pVBLock));
    //VBLockItem_Init ( uVBLock, VBLock_pItem(pVBLock), VBLock_Desc );
//ASSERT(VBLock_IsItem(pVBLock));
    VBLockDesc_Init ( OBJ__uVBLock, VBLock_pDesc(pVBLock), VBLockAttr_DEFAULT
                    , m_pP3PmsgField->OBJ__VBLocknn );

//ASSERT(!VBLock_IsLinked(pVBLock));
    if ( m_pP3PmsgField )
    {
      VBLockItem_SetDescn ( OBJ__uVBLock
                          , VBLock_pItem((VBLock *)m_pP3PmsgField->r_Object().GetVBLock())
                          , aDesc );
      Connect ( m_pP3PmsgField );
      ASSERT(m_pP3PmsgField->IsDescendant());
    }
//ASSERT(!VBLock_IsLinked(pVBLock));
    pVBLock -> oHdr.uVBLock |= VBLock_Linked;
//ASSERT(VBLock_IsLinked(pVBLock));
//ASSERT(VBLock_IsDesc(pVBLock));
}

//  Chained reference exposures

const P3PmsgObject&
P3PmsgDesc::r_Object ( ) const
{
    //if ( m_oObject.m_hVBList == 0 )
    //{
    //  P3PmsgObject *pObject = (P3PmsgObject*)&m_oObject;
    //  pObject -> m_hVBList     = P2PmsgHeap_AddRef ( m_pP3PmsgField -> OBJ__hVBList );
    //  pObject -> m_uVBLock     = P2PmsgHeap_Addrnn ( m_oObject.m_hVBList );
    //  pObject -> m_nVBLockSize = 0;//GetVBLockSize();
    //  pObject -> m_aVBLock     = P3PmsgDesc__GetVBLocknn ( this );
    //}
    return m_oObject;
}

void
P3PmsgDesc::Drop ( )
{
    Truncate ( );
    //VBLock *pVBLock     = m_pP3PmsgField -> r_Object().GetVBLock ( );
    //VBLaddr aVBLockDesc = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) ); 
    //if ( aVBLockDesc ) //TODO:LJM deprecated
    if ( OBJ__aVBLock )
    {
      //m_pP3PmsgField -> OBJ__Free ( aVBLockDesc ); //TODO:LJM deprecated
      OBJ__Free ( OBJ__aVBLock );
      if ( m_pP3PmsgField )
      {
        VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> r_Object().GetVBLock ( );
        VBLockItem_SetDescn ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock), 0 );
      }
    }
}

//  Navigation and 
P3PmsgObject
P3PmsgDesc::SelectObject ( LPCTNAM lpszObjectName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszObjectName) )
      return P3PmsgObject();
    return m_pCurs->r_Object();
    //TODO:LJM was return ((P3PmsgField&)*m_pCurs).r_Object();
}
P3PmsgField
P3PmsgDesc::Select ( LPCTNAM lpszItemName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
      return P3PmsgField();
    return *m_pCurs;
}
P3PmsgField&
P3PmsgDesc::SelectItem ( LPCTNAM lpszItemName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
    {
      ASSERT(0);//TODO:Delete-me
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszItemName)
            -> Message(L"Node [%s] does not exist", lpszItemName )
            -> Throw();
    }
    return *m_pCurs;
}
P3PmsgNode&
P3PmsgDesc::SelectNode ( LPCTNAM lpszNodeName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszNodeName) ||
         !m_pCurs->IsNode()              )
    {
      ASSERT(0);//TODO:Delete-me
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszNodeName)
            -> Message(L"Node [%s] does not exist", lpszNodeName )
            -> Throw();
    }
    return m_pCurs->r_node ( );
}
P3PmsgList&
P3PmsgDesc::SelectList ( LPCTNAM lpszListName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszListName) ||
         !m_pCurs->IsList()              )
    {
      ASSERT(0);//TODO:Delete-me
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszListName)
            -> Message(L"List [%s] does not exist", lpszListName )
            -> Throw();
    }
    return m_pCurs->r_list ( );
}
P3PmsgVect&
P3PmsgDesc::SelectVect ( LPCTNAM lpszVectName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszVectName) ||
         !m_pCurs->IsVect()              )
    {
      ASSERT(0);//TODO:Delete-me
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszVectName)
            -> Message(L"Vect [%s] does not exist",   lpszVectName )
            -> Throw();
    }
    return m_pCurs->r_vect ( );
}

P3PmsgField&
P3PmsgDesc::DeclareItem ( LPCTNAM lpszItemName, const P3PmsgData& oData, bool bUpdate )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
    { // This sequence needs optimising
      (*this) += P3PmsgField ( lpszItemName, oData );
      m_pCurs->Goto(lpszItemName);
      return m_pCurs->r_item();
    }
    if ( bUpdate )
      m_pCurs->r_item().r_data() = oData;
    return m_pCurs->r_item();
}
P3PmsgNode&
P3PmsgDesc::DeclareNode ( LPCTNAM lpszNodename, const P3PmsgData& oData, bool bUpdate )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszNodename) )
    { // This sequence needs optimising
      (*this) += P3PmsgNode ( lpszNodename, oData );
      m_pCurs->Goto(lpszNodename);
      return m_pCurs->r_node();
    }
    if ( bUpdate )
      m_pCurs->r_node().r_data() = oData;
    return m_pCurs->r_node();
}
bool
P3PmsgDesc::Exists ( LPCTNAM lpszItemName )
{
    if ( OBJ__aVBLock == NULL )
      return false;
    if (  lpszItemName         == NULL ||
         _ucslen(lpszItemName) <= 0       )
      return false;
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    return m_pCurs->Goto(lpszItemName);
}
bool
P3PmsgDesc::Delete ( LPCTNAM lpszItemName )
{
    if ( OBJ__aVBLock == NULL )
      return false;
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgNode of all items
    if ( !m_pCurs->Goto(lpszItemName) )
      return false;
    m_pCurs -> Delete ( );
    return true;
}
void
P3PmsgDesc::Truncate ( )
{
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgNode of all items
    while ( m_pCurs->Goto((int)0) )
      m_pCurs -> Delete ( );
    delete m_pCurs;
           m_pCurs = 0;
}
P3PmsgCurs&
P3PmsgDesc::r_Curs ( )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    return *m_pCurs;
}
*/

// Addressing and allocations
/*VBLock*
P3PmsgDesc__VBLock  ( const P3PmsgDesc *pDesc )
{
    //VBLock *pVBLock = pDesc -> GetField() -> r_Object().GetVBLock ( );
    //UINT    aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    VBLock *pVBDesc =(VBLock *)pDesc -> r_Object().GetVBLock();
    if ( pVBDesc )
    {
      ASSERT(VBLock_IsLinked(pVBDesc));
      ASSERT(VBLock_IsAlloc(pVBDesc));
    }
    return pVBDesc;
}
VBLock*
P3PmsgDesc__VBLock  ( const P3PmsgField *pField )
{
    VBLock *pVBLock = (VBLock *)pField -> r_Object().GetVBLock ( );
    UINT    aDescn  = VBLockItem_GetDescn ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    if ( !aDescn )
      return 0;
    VBLock *pVBDesc =(VBLock *)pField -> r_Object().Msg2Phys ( aDescn );
    ASSERT(VBLock_IsItem(pVBLock));
    ASSERT(VBLock_IsLinked(pVBDesc));
    ASSERT(VBLock_IsAlloc(pVBDesc));
    return pVBDesc;
}
static VBLaddr
P3PmsgDesc__GetVBLocknn( const P3PmsgDesc *pThis )
{
    VBLock *pVBLock = (VBLock *)pThis -> GetField() -> r_Object().GetVBLock ( );
    UINT    aDescn  = VBLockItem_GetDescn ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    return  aDescn;
}*/

//  Troubleshooting
/*void
P3PmsgDesc::AssertValid ( ) const
{
    //TODO:LJM deprecated VBLock     *pVBLock = P3PmsgDesc__VBLock ( this );
    VBLock *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pVBLock == nullptr )
      return;
    UCHAR       uVBLock = pVBLock->oHdr.uVBLock;
    VBLockDesc *pDesc   = VBLock_pDesc ( pVBLock );

    // Addressing
    UINT uVBLock1 = uVBLock&VBLock_AddrMask;
    if ( m_pP3PmsgField )
    {
      UINT uVBLock2 = m_pP3PmsgField->OBJ__uVBLock&VBLock_AddrMask;
      if ( uVBLock1 != uVBLock2 )
        EVERR -> Module ( __FUNCTION__ )
              -> Message("Corrupted addressing (0x%x vs 0x%x)", uVBLock1, uVBLock2 )
              -> Throw();
    }

    // Header
    int  nItems = VBLockDesc_GetItems ( uVBLock, pDesc );
    int  nFirst;
    UINT aFirst = VBLockDesc_GetFirst ( uVBLock, pDesc, &nFirst );
    int  nLast;
    UINT aLast  = VBLockDesc_GetLast  ( uVBLock, pDesc, &nLast );
    if ( !nItems && (nFirst || aFirst || nLast!=-1 || aLast) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted header nItems=%i aFirst=%i aLast=%i",
                        nItems, aFirst, aLast )
            -> Throw();

    // Navigation
    P3PmsgCurs& oCurs = ((P3PmsgDesc *)this)->r_Curs();
                oCurs.AssertValid ( );
	  int i;
    for ( i = 0; oCurs.Goto(i); i++ )
    {
      oCurs.r_Object().AssertCommon ( r_Object() );
      if ( oCurs.IsNode() )
        oCurs.r_node().AssertValid();
      else if ( oCurs.IsList() )
        oCurs.r_list().AssertValid();
      else if ( oCurs.IsVect() )
        oCurs.r_vect().AssertValid();
      else if ( oCurs.IsItem() )
        oCurs.r_item().AssertValid();
      else
        ASSERT(0);
    }
    if ( nItems != i )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted item count actual=%i expected=%i",
                        nItems, i )
            -> Throw();
}
void
P3PmsgDesc::Print ( FILE *fd, int nDepthOS, int nDepthOSinc )
{
    // P2PmsgDesc details
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, "  " );
    //TODO:LJM deprecated UINT nVBLockSize = VBLock_Hdr_u_SizeNN( P3PmsgDesc__VBLock(this) );
    if ( OBJ__VBLock )
    {
      UINT nVBLockSize = VBLock_Hdr_u_SizeNN( OBJ__VBLock );
      P3Pmsg_fwprintf ( fd, "?(%i) {\n", nVBLockSize );

      // Recursion
      P3PmsgCurs& oCurs = ((P3PmsgDesc *)this)->r_Curs();
      for ( int i = 0; oCurs.Goto(i); i++ )
      {
        if ( oCurs.IsNode() )
          oCurs.r_node().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
        else if ( oCurs.IsList() )
          oCurs.r_list().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
        else if ( oCurs.IsVect() )
          oCurs.r_vect().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
        else if ( oCurs.IsItem() )
          oCurs.r_item().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      }
    } else fwprintf ( fd, "@() {\n" );

      

    // Tidy up
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, "  " );
    P3Pmsg_fwprintf ( fd, "}\n" );
}*/

///////////////////////////////////////////////////////////////////////
//  Std::List modifiers
/*P2PmsgNodeHdl
P3PmsgDesc::PushBack ( const P3PmsgNode& oNode )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    // Block allocation and linkage
    //P3PmsgObject& m_oObject   = (P3PmsgObject&)m_pP3PmsgField -> r_Object();
    //P2PmsgHANDLE& hVBList     =  oObject.m_hVBList;
    //UCHAR&        uVBLock     =  oObject.m_uVBLock;
    UINT          nSizeofItem =  P2PmsgNode_SizeofItem ( OBJ__uVBLock, &oNode );
    UINT          aItem       =  OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgNode_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), &oNode );
    //TODO:LJM deprecated VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    P2PmsgDesc_LinkinItem ( this
                          , VBLockDesc_GetLast(OBJ__uVBLock,pVBLockDesc)
                          , aItem
                          , 0 );
//AssertValid();//TODO:LJM delete, testing
//oNode.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
    P3PmsgNode oNodeItem ( OBJ__hVBList, aItem, nSizeofItem );
               oNodeItem = oNode;

    // Tidy up and
    P2PmsgNodeHdl oHdl = { (UINT_PTR)OBJ_hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    return oHdl;
}

P2PmsgFieldHdl
P3PmsgDesc::PushBack ( const P3PmsgField& oField )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    ASSERT(VBLock_IsDesc(OBJ__VBLock));
    // Block allocation and linkage
    //P3PmsgObject& oObject     = (P3PmsgObject&)m_pP3PmsgField -> r_Object ( );
    //P2PmsgHANDLE& hVBList     =  oObject.m_hVBList;
    //UCHAR&        uVBLock     =  oObject.m_uVBLock;
    UINT          nSizeofItem =  P2PmsgField_SizeofItem ( OBJ__uVBLock, &oField );
    UINT          aItem       =  OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock       *pVBLockItem = (VBLock *)OBJ__Msg2Phys(aItem);
    P2PmsgField_InitItem  ( pVBLockItem, &oField );
    //TODO:LJM deprecated VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    P2PmsgDesc_LinkinItem ( this
                          , VBLockDesc_GetLast(OBJ__uVBLock,pVBLockDesc)
                          , aItem
                          , 0 );
ASSERT(OBJ__uVBLock==(pVBLockItem->oHdr.uVBLock&VBLock_AddrMask));
//AssertValid();//TODO:LJM delete, testing
//oField.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
    P3PmsgField oFieldItem ( OBJ__hVBList, aItem, nSizeofItem );
                oFieldItem = oField;

    // Tidy up and
    P2PmsgFieldHdl oHdl = { (UINT_PTR)OBJ_hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    //ASSERT(hVBList==oObject.m_hVBList);
    //ASSERT(uVBLock==oObject.m_uVBLock);
    return oHdl;
}
P2PmsgListHdl
P3PmsgDesc::PushBack ( const P3PmsgList& oList )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    // Block allocation and linkage
    //P3PmsgObject& oObject     = (P3PmsgObject&)m_pP3PmsgField -> r_Object ( );
    //P2PmsgHANDLE& hVBList     = m_pP3PmsgField -> m_hVBList;
    //UCHAR&        uVBLock     = oObject.m_uVBLock;
    UINT          nSizeofItem = P2PmsgList_SizeofItem ( OBJ__uVBLock, &oList );
    UINT          aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgList_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), &oList );
    //TODO:LJM deprecated VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    P2PmsgDesc_LinkinItem ( this
                          , VBLockDesc_GetLast(OBJ__uVBLock,pVBLockDesc)
                          , aItem
                          , 0 );
//AssertValid();//TODO:LJM delete, testing
//oList.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
    P3PmsgList oListItem ( OBJ__hVBList, aItem, nSizeofItem );
               oListItem = oList;

    // Tidy up, and
    P2PmsgListHdl oHdl = { (UINT_PTR)OBJ_hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    //ASSERT(OBJ__hVBList==oObject.m_hVBList);
    //ASSERT(uVBLock==oObject.m_uVBLock);
    return oHdl;
}
P2PmsgVectHdl
P3PmsgDesc::PushBack ( const P3PmsgVect& oVect )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    // Block allocation and linkage
    //P3PmsgObject& oObject     =(P3PmsgObject&)m_pP3PmsgField -> r_Object ( );
    //P2PmsgHANDLE& hVBList     = oObject.m_hVBList;
    //UCHAR&        uVBLock     = oObject.m_uVBLock;
    UINT          nSizeofItem = P2PmsgVect_SizeofItem ( OBJ__uVBLock, &oVect );
    UINT          aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgVect_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), &oVect );
    //TODO:LJM deprecated below VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    P2PmsgDesc_LinkinItem ( this
                          , VBLockDesc_GetLast(OBJ__uVBLock,pVBLockDesc)
                          , aItem
                          , 0 );
//AssertValid();//TODO:LJM delete, testing
//oVect.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
    P3PmsgVect oVectItem ( OBJ__hVBList, aItem, nSizeofItem );
               oVectItem = oVect;

    // Tidy up, and
    P2PmsgVectHdl oHdl = { (UINT_PTR)OBJ_hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    //ASSERT(hVBList==oObject.m_hVBList);
    //ASSERT(uVBLock==oObject.m_uVBLock);
    return oHdl;
}

// Properties
UINT
P3PmsgDesc::GetCount ( ) const
{
    //TODO:LJM deprecated below VBLock *pVBLock = P3PmsgDesc__VBLock ( this );
    VBLock *pVBLock = OBJ__VBLock;
    if ( pVBLock == NULL )
      return 0;
    VBLockDesc *pDesc   = VBLock_pDesc ( pVBLock );
    return VBLockDesc_GetItems ( pVBLock->oHdr.uVBLock, pDesc );
}
bool
P3PmsgDesc::IsEmpty ( ) const
{
    return GetCount() == 0 ? true : false;
}
P3PmsgField*
P3PmsgDesc::GetField ( ) const
{
    return m_pP3PmsgField;
}

UINT
P2PmsgDesc_GetVBLockParentnn ( const P3PmsgDesc *pDesc )
{
    if ( pDesc->GetField() == 0 )
      return 0;
    //TODO:LJM deprecated below VBLock *pVBLock   = P3PmsgDesc__VBLock ( pDesc );
    VBLock *pVBLock   = ptrVBLOCK(pDesc->GetField()->r_Object()); //->r_Object().GetVBLock();
    UCHAR   uVBLock   = pVBLock->oHdr.uVBLock;
    ASSERT(VBLock_IsLinked(pVBLock));
    return VBLockDesc_GetParent ( uVBLock, VBLock_pDesc(pVBLock) );
}
VBLock*
P2PmsgDesc__GetVBLockParent ( const P3PmsgDesc *pDesc )
{
    UINT aParent = P2PmsgDesc_GetVBLockParentnn ( pDesc );
    return (VBLock *)pDesc->GetField()->r_Object().Msg2Phys(aParent);
}*/
//????????????????????????????????????????????????????????????????????
//////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//  VBLockField, Data and Name Utilities and helpers
//  NOTES: For clarity and simplicity operations isolated in series
//         of self contained static functions

//
//  Hidden addressing utilities
VBLockField*
P2PmsgObject_pField ( const P3PmsgObject& oObject )
{
    VBLock *pVBLock = (VBLock *)oObject.GetVBLock ( );
    ASSERT(VBLock_IsLinked(pVBLock));
    ASSERT(VBLock_IsAlloc(pVBLock));
    return VBLock_pField ( pVBLock );
}
//  What the chain walk reads before it has decided anything: uDataType, which
//  VBLockData_IsChained tests, and uDataAttr beside it. The chain address in the
//  union costs more and is only reached once uDataType has said it is there, so
//  it is bounded at the point of use rather than demanded up front -- a
//  legitimate small blob at the top of an image carries no full union.
static const VBLsize g_nSizeofVBLockDataHdr
    = (VBLsize)( sizeof(VBLockData) - sizeof(((VBLockData *)nullptr)->u) );

static VBLsize
VBLockData_Sizeof_Chain2Next ( UCHAR uVBLock ) noexcept
{
    if ( uVBLock == VBLock_Addr16 ) return (VBLsize)sizeof(VBLaddr16);
    if ( uVBLock == VBLock_Addr32 ) return (VBLsize)sizeof(VBLaddr32);
    if ( uVBLock == VBLock_Addr64 ) return (VBLsize)sizeof(VBLaddr64);
    return (VBLsize)sizeof(VBLaddr64);   // unknown mode: demand the widest,
                                         // GetChain2Next throws on it anyway
}

//
//  Bounds a block against the image before anything is derived from it
//  NOTES: The block's own declared size decides where every offset inside it
//         lands, and VBLock_ChkContained bounds those offsets against exactly
//         that. So a header that declares more of itself than the image holds
//         makes an out-of-image lump look perfectly contained -- which is how
//         `p2p_fuzzframe 0x5EEDF00D --replay 6 73` walked through the
//         containment check and read a name header eighteen bytes past the end
//         of a fifty-seven byte image. Containment answers "inside the block";
//         this answers "and the block is inside the image", and neither is any
//         use without the other.
//       : The block header is bounded first and separately, because reading the
//         declared size means reading the header -- Addr2Phys bounds a block's
//         START, so a block beginning in the last few bytes of an image has a
//         header that runs off the end of it.
//       : Skipped where the block is not in an image at all: an object holding
//         its block inline in m_oVBLock, or anything on a SYSTEM heap.
static void
P2PmsgObject_ChkVBLock ( const P3PmsgObject& oObject, VBLock *pVBLock )
{
    if ( pVBLock == nullptr )
      EVERR->MODULE
           ->Message("Null VBLock: address does not resolve in the image")
           ->Throw();
    if ( !P2PmsgHeap_IsPhysSpan ( oObject.m_hVBList, pVBLock, 1 ) )
      return;
    const VBLsize nSizeofHdr = P2PmsgHeap_Sizeof_Hdr ( oObject.m_hVBList );
    if ( !P2PmsgHeap_IsPhysSpan ( oObject.m_hVBList, pVBLock, nSizeofHdr ) )
      EVERR->MODULE
           ->Message("VBLock header of %u leaves the image", (UINT)nSizeofHdr )
           ->Throw();
    const VBLsize nSizeofBlock = VBLock_Hdr_u_SizeNN ( pVBLock );
    if ( !P2PmsgHeap_IsPhysSpan ( oObject.m_hVBList, pVBLock, nSizeofBlock ) )
      EVERR->MODULE
           ->Message("VBLock of %u leaves the image", (UINT)nSizeofBlock )
           ->Throw();
}

//
//  Bounds a VBLockData that VBLock_pData derived from a block's own header
//  NOTES: VBLock_pData walks offsets that live INSIDE the block -- a field's
//         data sits after its name, and the name's length is read out of the
//         name header. On the load path every one of those bytes came off a
//         socket, so a block can declare two bytes of itself and still hand
//         back a data pointer 158 bytes further on. That is not a programming
//         error, it is a frame someone sent us: `p2p_fuzzframe 0x5EEDF00D
//         --replay 0 10` reads 76 bytes past the end of a 126-byte image in the
//         walk below. ASan sees it, the CRT debug heap notices much later and
//         blames whatever block it landed in, and Release performs the same
//         read and reports nothing at all.
//       : Two bounds, because either alone lets the other through. The lump has
//         to sit inside the block -- VBLock_IsContainedVBLump, the predicate
//         MsgAttr/MsgDesc/MsgList/MsgVect already gate their own sub-lumps on,
//         and which already answers FALSE for the frame above with nobody
//         asking -- AND the block has to sit inside the image, which is what
//         P2PmsgObject_ChkVBLock above establishes.
//       : The image bound applies only where the block itself is in the image.
//         An object carrying its block inline in m_oVBLock is outside one by
//         design, as is anything on a SYSTEM heap; there containment is the
//         whole check.
//
//  Parameters:  const P3PmsgObject& oObject
//               Object the block was resolved through
//
//               VBLock *pVBLock
//               Block VBLock_pData was applied to
//
//               const VBLockData *pData
//               Its result
//
//               VBLsize nSpan
//               Bytes the caller is about to read from pData
static void
P2PmsgObject_ChkVBLockData ( const P3PmsgObject& oObject, VBLock *pVBLock
                           , const VBLockData *pData, VBLsize nSpan )
{
    if ( pData == nullptr )
      EVERR->MODULE
           ->Message("VBLockData does not resolve in the image")
           ->Throw();
    if ( !VBLock_IsContainedVBLump ( pVBLock, pData, nSpan ) )
      EVERR->MODULE
           ->Message("VBLockData +%u leaves its block of %u"
                    , (UINT)nSpan, (UINT)VBLock_Hdr_u_SizeNN(pVBLock) )
           ->Throw();
    if (  P2PmsgHeap_IsPhysSpan ( oObject.m_hVBList, pVBLock, 1 ) &&
         !P2PmsgHeap_IsPhysSpan ( oObject.m_hVBList, pData, nSpan ) )
      EVERR->MODULE
           ->Message("VBLockData +%u leaves the image", (UINT)nSpan )
           ->Throw();
}

VBLockData*
P2PmsgObject_pData ( const P3PmsgObject& oObject, bool bChain )
{
    VBLock     *pVBLock = (VBLock *)oObject.GetVBLock ( );
    P2PmsgObject_ChkVBLock ( oObject, pVBLock );
    VBLockData *pData   = VBLock_pData ( pVBLock );
    P2PmsgObject_ChkVBLockData ( oObject, pVBLock, pData, g_nSizeofVBLockDataHdr );

    while ( bChain && VBLockData_IsChained(pData) )
    {
      // Chained, so the union carries the next address and the span grows by it
      P2PmsgObject_ChkVBLockData ( oObject, pVBLock, pData
                                 , g_nSizeofVBLockDataHdr
                                 + VBLockData_Sizeof_Chain2Next(oObject.m_uVBLock) );
      VBLaddr aChain2Next = VBLockData_GetChain2Next (oObject.m_uVBLock, pData );
      VBLock *pVBLock1 = (VBLock *)oObject.Msg2Phys ( aChain2Next );
      P2PmsgObject_ChkVBLock ( oObject, pVBLock1 );
      pData   = VBLock_pData ( pVBLock1 );
      P2PmsgObject_ChkVBLockData ( oObject, pVBLock1, pData, g_nSizeofVBLockDataHdr );
      pVBLock = pVBLock1;
    }
    return pData;
}
VBLockData*
P2PmsgData_pData ( const P3PmsgData& oData, bool bChain )
{
    return P2PmsgObject_pData ( *oData.p_Object(), bChain );
}
VBLsize
P2PmsgObject_Sizeof_VBLockData ( const P3PmsgObject& oObject, BOOL bChain )
{
    const    UCHAR uVBLock = oObject.m_uVBLock;
    VBLock *pVBLock = (VBLock *)oObject.GetVBLock(); //TODO:LJM deprecated m_aData );
    if ( VBLock_IsData(pVBLock) )      // Raw VBLockData container
    {
      VBLockData *pData = VBLock_pData ( pVBLock );
      while ( bChain && VBLockData_IsChained(pData) )
	    {
        const VBLaddr aChain2Next = VBLockData_GetChain2Next ( uVBLock, pData );
        pVBLock = (VBLock *)oObject.Msg2Phys ( aChain2Next );
        pData   = VBLock_pData ( pVBLock );
      }
      return VBLockData_Sizeof_Alloc ( pVBLock );
    }
    VBLockData *pData = VBLock_pData ( pVBLock ); ///P2PmsgObject_pData ( oObject, false );
    if ( bChain && VBLockData_IsChained(pData) )
    {
      const VBLaddr aChain2Next = VBLockData_GetChain2Next ( uVBLock, pData );
      VBLock *pVBLock1 = (VBLock *)oObject.Msg2Phys ( aChain2Next );
      return VBLockData_Sizeof_Alloc ( pVBLock1 );
    }
    return VBLockData_Sizeof_Alloc(pVBLock);
}

#pragma optimize("",off)
VBLockData*
P2PmsgObject_NewVBLockData ( P3PmsgObject& oObject, VBLsize nSizeof )
{
    VBLockData *pData = P2PmsgObject_pData ( oObject, false );
    UCHAR       uVBLock = oObject.m_uVBLock;
    // Free any legacy chained data blocks
    // NOTES: Consequently chaining is usually limited to single step.
    while ( VBLockData_IsChained(pData) )
    {
      VBLaddr     aData1       = VBLockData_GetChain2Next ( uVBLock, pData );
      VBLock     *pVBLock1     = (VBLock *)oObject.Msg2Phys ( aData1 );
      VBLockData *pData1       = VBLock_pData( pVBLock1 );
      ASSERT(VBLock_IsData(pVBLock1));//TODO:Debugging
      VBLaddr     aChain2Next1 = 0;
      if ( VBLockData_IsChained(pData1) )
        aChain2Next1 = VBLockData_GetChain2Next ( uVBLock, pData1 );
      VBLockData_SetChain2Next ( uVBLock, pData, aChain2Next1 );
      pData -> uDataAttr       = pData1 ->uDataAttr; 
      pData -> uDataType       = pData1 ->uDataType;
      oObject.Free ( aData1 );
    }                          // Alloc's can invalidate all pointers
    VBLaddr aVBLock1       = oObject.AllocVBLock ( VBLock_Data, nSizeof );
    VBLock *pVBLock1       = (VBLock *)oObject.Msg2Phys ( aVBLock1 );
    pVBLock1 -> oHdr.uVBLockDefs |= VBLock_Linked;
            pData          = P2PmsgObject_pData(oObject,false);
    ASSERT(VBLock_IsAlloc(pVBLock1));
    VBLockData *pData1      = VBLock_pData( pVBLock1 );
ASSERT(VBLockData_Sizeof_Alloc(pVBLock1)>=nSizeof);  //confirming allocated >= requested delete-bug-hunting
                                                //mandatory raw confirmation that must be passed
    VBLockData_Init ( pData1, pData->uDataAttr, pData->uDataType, nSizeof );
    ASSERT(VBLock_IsAlloc(pVBLock1)); //delete-bug-hunting
VBLsize nSizeof1 =VBLockData_Sizeof_Alloc(pVBLock1); //delete-bug-hunting
ASSERT(VBLockData_Sizeof_Alloc(pVBLock1)>=nSizeof);  //delete-bug-hunting
ASSERT(VBLock_IsContainedVBLump(pVBLock1,pData1,nSizeof));
VBLsize nSizeof0=VBLockData_Sizeof_Alloc((VBLock*)oObject.GetVBLock()); // delete-bug-hunting
//  What the INLINE lump has to hold from here on is the CHAINED form -- its
//  header and a chain pointer, i.e. VBLockData_Sizeof_Min -- and not the type's
//  full logical payload, which is what the block allocated just above is for.
//  SetChain2Next is the very next statement.
//
//  This used to assert containment of VBLockData_Sizeof, the full payload, and so fired
//  on a legitimate state that only a RETYPE can reach.  A child declared INT32
//  gets 6 bytes of inline data space; retyping it to WSTR16 makes its
//  type-computed size 8; 8 does not fit in 6, so the cell chains -- which is
//  correct, and the value reads back intact.  6 is also exactly Sizeof_Min on a
//  32-bit heap, so the chained form does fit and the check below is the one that
//  was meant.  Measured with an instrumented build: req=8 claim=8 alloc=6,
//  against req=60 claim=10 alloc=10 for the same growth on a cell that was
//  declared as text in the first place -- which is why only retype tripped it.
VBLsize nSizeofChained = VBLockData_Sizeof_Min ( uVBLock );
ASSERT(VBLock_IsContainedVBLump((VBLock*)oObject.GetVBLock(),pData,nSizeofChained));
    VBLockData_SetChain2Next ( uVBLock, pData, aVBLock1 );
    ASSERT(VBLock_IsAlloc(pVBLock1)); //delete-bug-hunting
ASSERT(VBLock_IsContainedVBLump(pVBLock1,pData1,nSizeof));
ASSERT(VBLockData_Sizeof_Alloc(pVBLock1)>=nSizeof);
    ASSERT(VBLock_IsLinked(pVBLock1));
    ASSERT(VBLock_IsAlloc(pVBLock1)); //delete-bug-hunting
    ASSERT(pData==P2PmsgObject_pData(oObject,false)); //TODO:PDelete
    ASSERT(pData1==P2PmsgObject_pData(oObject,true)); //TODO:PDelete
    ASSERT(aVBLock1==VBLockData_GetChain2Next(uVBLock,pData));
    return pData1;
}
#pragma optimize("",on)
VBLsize
P2PmsgObject_VBLockNameSize ( const P3PmsgObject& oObject )
{
    return VBLockName_Sizeof ( oObject.m_uVBLock, P2PmsgObject_pName(oObject) );
}
VBLockName*
P2PmsgObject_pName ( const P3PmsgObject& oObject, bool bIndirect )
{
    //VBLockField *pField = P2PmsgObject_pField ( OBJ__ );
    //VBLockData  *pData  = VBLockField_pData ( pField );
    VBLock     *pVBLock = (VBLock *)oObject.GetVBLock ( );
    ASSERT(VBLock_IsAlloc(pVBLock));
    VBLockName *pName   = VBLock_pName ( pVBLock );

    while ( bIndirect && VBLockName_IsChained(pName) )
    {
      //VBLock *pVBLock1 = (VBLock *)oObject.Msg2Phys ( pName->u.vBlin08.aVBLockAddr );
      VBLaddr aVBLock1 = VBLockName_GetChain2Next ( oObject.m_uVBLock, pName );
      VBLock *pVBLock1 = (VBLock *)oObject.Msg2Phys ( aVBLock1 );
      ASSERT(VBLock_IsAlloc(pVBLock1));
      ASSERT(VBLock_IsLinked(pVBLock1));
      pName = VBLock_pName ( pVBLock1 );
    }
    return pName;
}

//
//  Utilities to calculate effective size of P2PmsgItem's
//  NOTES: 
//
VBLsize
P2PmsgField_SizeofItem ( UCHAR uVBLock, const P3PmsgField& oField, BOOL bChain )
{
//if(uVBLock==3)
//oField.VerifyContainment(); // bug-hunt-delete
    const P3PmsgObject& oObject = oField.r_Object();
    ASSERT(oObject.IsField());
    VBLock      *pVBLock = (VBLock *)oObject.GetVBLock ( );
    VBLsize      nSizeof = VBLockItem_Sizeof  ( uVBLock );
    nSizeof             += VBLockField_Sizeof ( );
    VBLockName  *pName   = P2PmsgObject_pName ( oObject, bChain );
    nSizeof             += max ( VBLockName_Sizenn(uVBLock,pName ), VBLockName_Sizeof_Min(uVBLock) );
    VBLockData  *pData   = P2PmsgObject_pData ( oObject, bChain );
    nSizeof             += VBLockData_Sizeof  ( uVBLock, pData );
    return nSizeof;
}
VBLsize
P2PmsgList_SizeofItem ( UCHAR uVBLock, const P3PmsgList& oList, BOOL bChain )
{
if(uVBLock==3)
oList.VerifyContainment(); // bug-hunt-delete
    const P3PmsgObject& oObject = oList.r_Object();
    ASSERT(oObject.IsList());
    VBLock      *pVBLock  = (VBLock *)oObject.GetVBLock ( );
    VBLsize      nSizeof  = VBLockItem_Sizeof ( uVBLock );
                 nSizeof += VBLockList_Sizeof ( uVBLock );
    // Following is part lifted from P2PmsgField_SizeofItem
    nSizeof             += VBLockField_Sizeof ( );
    VBLockName  *pName   = P2PmsgObject_pName ( oObject, bChain );
    nSizeof             += max ( VBLockName_Sizenn(uVBLock,pName ), VBLockName_Sizeof_Min(uVBLock) );
    VBLockData  *pData   = P2PmsgObject_pData ( oObject, bChain );
    nSizeof             += VBLockData_Sizeof  ( uVBLock, pData );
    return nSizeof;
}
VBLsize
P2PmsgVect_SizeofItem ( UCHAR uVBLock, const P3PmsgVect& oVect )
{
    //  Sized with the SAME expressions P2PmsgVect_InitItem lays the block out with, and
    //  that symmetry is the whole point -- this is the allocation InitItem then writes
    //  into, so anything it derives differently is a block written past its end.
    //
    //  It used to read `VBLockItem_Sizeof(uVBLock) + oVect.P3PmsgVect::Sizeof(uVBLock)`,
    //  which reaches the name through P3PmsgField::Sizeof -> P3PmsgName::Sizeof ->
    //  VBLockName_Sizeof, while InitItem reaches it through P2PmsgObject_pName and takes
    //  max(VBLockName_Sizenn, VBLockName_Sizeof_Min). Two rules for one number. Measured on
    //  the golden_utf16 vect (name "Persisted", uVBLock = VBLock_Addr64) they disagree, and
    //  they disagree BY ARCHITECTURE:
    //
    //      x64    sizing 127, layout 127   -- agree, so the block fits and nothing shows
    //      Win32  sizing  27, layout 127   -- 100 bytes short
    //
    //  On Win32 the item block was therefore allocated 352 bytes, InitItem wrote a 127-byte
    //  name into it, and the VBLockData landed past the end of the 361-byte block. Debug
    //  caught it as the VerifyContainment ASSERT at the foot of P3PmsgName::c_name; Release
    //  ran on and died in __report_gsfailure (0xC0000409); with the containment checks in
    //  P2PmsgObject_ChkVBLockData it surfaces as "VBLockData +2 leaves its block of 361".
    //  Three of the 45 tests failed on Win32 in both configurations because of it.
    //
    //  P2PmsgList_SizeofItem and P2PmsgField_SizeofItem already size this way, additively
    //  and through the same accessors as their Init counterparts. The vect was the only one
    //  of the three that did not, which is why only the vect paths corrupt.
    //
    //  NOTE for the golden image: on x64 and Linux both rules already return 127, so this
    //  returns exactly what it returned before on those and MscsUnitTests/golden_ref.p2p is
    //  unaffected. Only the short Win32 answer changes.
    const P3PmsgObject& oObject = oVect.r_Object ( );
    VBLsize      nSizeofItem = VBLockItem_Sizeof  ( uVBLock );
                 nSizeofItem += VBLockVect_Sizeof ( uVBLock );
                 nSizeofItem += VBLockField_Sizeof ( );
    VBLockName  *pName   = P2PmsgObject_pName ( oObject, TRUE );
                 nSizeofItem += max ( VBLockName_Sizenn(uVBLock,pName)
                                    , VBLockName_Sizeof_Min(uVBLock) );
    VBLockData  *pData   = P2PmsgObject_pData ( oObject, TRUE );
                 nSizeofItem += VBLockData_Sizeof ( uVBLock, pData );
    return nSizeofItem;
}

//
//  Utilities to copy P2PmsgItem's
//  NOTES:
VBLaddr
P2PmsgItem_InitField( VBLock *pVBLock, const P3PmsgField& oField )
{
    //VBLock *pVBLock = (VBLock *)pvVBLock;
    UCHAR   uVBLockDefs = pVBLock->oHdr.uVBLockDefs;
    UCHAR   uVBLock     = pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask;
    VBLockItem_Init ( uVBLockDefs, VBLock_pItem(pVBLock), VBLock_Field );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    // VBLockName is both delicate and pivitol
    VBLockName  *pName   = P2PmsgObject_pName ( oField.r_Object(), TRUE );
    VBLaddr      nSizeofName = max ( VBLockName_Sizenn(uVBLock,pName ), VBLockName_Sizeof_Min(uVBLock) );
    VBLockName_Init ( uVBLock,VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , oField.c_name(), nSizeofName );
    // VBLockData is effectively the remainder
    VBLockData  *pData   = P2PmsgObject_pData ( oField.r_Object(), TRUE );
    VBLaddr      nSizeofData = VBLockData_Sizeof  ( uVBLock, pData );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oField.DataType(), nSizeofData );
    return (VBLaddr)pVBLock;
}

VBLaddr
P2PmsgNode_InitItem ( const P3PmsgField& /*oField*/ )
{
    ASSERT(0);//TODO: This is not completed
    return 0;
}
VBLaddr
P2PmsgList_InitItem ( VBLock *pVBLock, const P3PmsgList& oList )
{
    //VBLock *pVBLock = (VBLock *)pVBLock;
    UCHAR   uVBLockDefs = pVBLock->oHdr.uVBLockDefs;
    UCHAR   uVBLock     = pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask;
    VBLockItem_Init ( uVBLockDefs, VBLock_pItem(pVBLock), VBLock_List );
    VBLockList_Init ( uVBLockDefs, VBLock_pList(pVBLock), AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    // VBLockName is both delicate and pivitol
    VBLockName  *pName   = P2PmsgObject_pName ( oList.r_Object(), TRUE );
    VBLaddr      nSizeofName = max ( VBLockName_Sizenn(uVBLock,pName ), VBLockName_Sizeof_Min(uVBLock) );
    VBLockName_Init ( uVBLock,VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , oList.c_name(), nSizeofName );
    // VBLockData is effectively the remainder
    VBLockData  *pData   = P2PmsgObject_pData ( oList.r_Object(), TRUE );
    VBLaddr      nSizeofData = VBLockData_Sizeof  ( uVBLock, pData );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oList.DataType(), nSizeofData );
    return (VBLaddr)pVBLock;
}
VBLaddr
P2PmsgVect_InitItem ( VBLock *pVBLock, const P3PmsgVect& oVect )
{
    //VBLock *pVBLock = (VBLock *)pVBLock;
    UCHAR   uVBLockDefs = pVBLock->oHdr.uVBLockDefs;
    UCHAR   uVBLock     = pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask;
    VBLockItem_Init ( uVBLockDefs, VBLock_pItem(pVBLock), VBLock_Vect );
    VBLockVect_Init ( uVBLockDefs, VBLock_pVect(pVBLock), 0, VBLockAttr_DEFAULT );
    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
    // VBLockName is both delicate and pivitol
    VBLockName  *pName   = P2PmsgObject_pName ( oVect.r_Object(), TRUE );
    VBLaddr      nSizeofName = max ( VBLockName_Sizenn(uVBLock,pName ), VBLockName_Sizeof_Min(uVBLock) );
    VBLockName_Init ( uVBLock,VBLock_pName(pVBLock), VBLockAttr_DEFAULT
                    , oVect.c_name(), nSizeofName );
    // VBLockData is effectively the remainder
    VBLockData  *pData   = P2PmsgObject_pData ( oVect.r_Object(), TRUE );
    VBLaddr      nSizeofData = VBLockData_Sizeof  ( uVBLock, pData );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oVect.DataType(), nSizeofData );
    return (VBLaddr)pVBLock;
}

//
//  Links passed VBLockItem into VBLockList
//  NOTES: Handles bit addressing translations
//
//  Parameters:  P2PmsgVBL
//               Virtual block list manager
//
//               VBLockList *pVBLockList
//               List into which VBLockList is to be linked
//
//               UINT aItemPrev
//               VBLock address of the previous VBLockItem
//
//               UINT aItem
//               VBLock address of VBLockItem to be linked in
//
//               UINT aItemNext
//               VBLock address of the next VBLockItem
void
P2PmsgList_LinkinItem ( P3PmsgList *pList
                      , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext )
{
    // Locals
    P3PmsgObject& m_oObject = (P3PmsgObject&)pList -> r_Object();
    VBLock     *pItemPrev   = (VBLock *)OBJ__Msg2Phys(aItemPrev);
    VBLock     *pItem       = (VBLock *)OBJ__Msg2Phys(aItem);
    VBLock     *pItemNext   = (VBLock *)OBJ__Msg2Phys(aItemNext);
    VBLock     *pVBLock     =           OBJ__VBLock;
    VBLockList *pVBLockList =           VBLock_pList ( ptrVBLOCK(pList->r_Object()) );

    // Observe addressing model
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr08 )
      goto P08;
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr16 )
      goto P16;
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr32 )
      goto P32;
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr64 )
      goto P64;
    ASSERT(0);
    EVERR->Module ("P2PmsgList")
         ->Message("Internal VBLockList.uVBLockAddr.ua corruption" )
         ->Throw ( );

    // P2PmsgItem - Prev Linkages
P08:VBLock_pItem(pItem)->ua.oItem08.aPrev = (UINT08)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem08.aNext = (UINT08)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N08;
P16:VBLock_pItem(pItem)->ua.oItem16.aPrev = (UINT16)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem16.aNext = (UINT16)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N16;
P32:VBLock_pItem(pItem)->ua.oItem32.aPrev = (UINT32)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem32.aNext = (UINT32)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N32;
P64:VBLock_pItem(pItem)->ua.oItem64.aPrev = (UINT64)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem64.aNext = (UINT64)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N64;

    // P2PmsgItem - Next Linkages
N08:VBLock_pItem(pItem)->ua.oItem08.aNext = (UINT08)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem08.aPrev = (UINT08)aItem;
    goto H08;
N16:VBLock_pItem(pItem)->ua.oItem16.aNext = (UINT16)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem16.aPrev = (UINT16)aItem;
    goto H16;
N32:VBLock_pItem(pItem)->ua.oItem32.aNext = (UINT32)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem32.aPrev = (UINT32)aItem;
    goto H32;
N64:VBLock_pItem(pItem)->ua.oItem64.aNext = (UINT64)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem64.aPrev = (UINT64)aItem;
    goto H64;

    // P2PmsgList - Housekeeping
H08:if ( VBLock_pItem(pItem)->ua.oItem08.aPrev == 0 )
      pVBLockList->u.oList08.aFirst = (UINT08)aItem;
    if ( VBLock_pItem(pItem)->ua.oItem08.aNext == 0 )
      pVBLockList->u.oList08.aLast  = (UINT08)aItem;
    pVBLockList -> u.oList08.nItems++;
    goto END;
H16:if ( VBLock_pItem(pItem)->ua.oItem16.aPrev == 0 )
      pVBLockList->u.oList16.aFirst = (UINT16)aItem;
    if ( VBLock_pItem(pItem)->ua.oItem16.aNext == 0 )
      pVBLockList->u.oList16.aLast  = (UINT16)aItem;
    pVBLockList -> u.oList16.nItems++;
    goto END;
H32:if ( VBLock_pItem(pItem)->ua.oItem32.aPrev == 0 )
      pVBLockList->u.oList32.aFirst = (UINT32)aItem;
    if ( VBLock_pItem(pItem)->ua.oItem32.aNext == 0 )
      pVBLockList->u.oList32.aLast  = (UINT32)aItem;
    pVBLockList -> u.oList32.nItems++;
    goto END;
H64:if ( VBLock_pItem(pItem)->ua.oItem64.aPrev == 0 )
      pVBLockList->u.oList64.aFirst = (UINT64)aItem;
    if ( VBLock_pItem(pItem)->ua.oItem64.aNext == 0 )
      pVBLockList->u.oList64.aLast  = (UINT64)aItem;
    pVBLockList -> u.oList64.nItems++;
    goto END;

    // Tidy up and
END:return;
}

VBLaddr
P2PmsgList_UnLinkItem ( P3PmsgList *pList, VBLockList *pVBLockList, VBLaddr aItem )
{
    P3PmsgObject& m_oObject = (P3PmsgObject&)pList -> r_Object();
    VBLock     *pVBLockPrev = 0;
    VBLock     *pVBLock     = (VBLock *)OBJ__Msg2Phys(aItem);
    VBLockItem *pItem       = VBLock_pItem ( pVBLock );
    VBLock     *pVBLockNext = 0;

    UCHAR   uVBLock     = pVBLock->oHdr.uVBLockDefs;

    VBLaddr aItemPrev   = VBLockItem_GetPrev ( uVBLock, pItem );
    if ( aItemPrev )
      pVBLockPrev = (VBLock *)OBJ__Msg2Phys(aItemPrev);
    VBLaddr aItemNext   = VBLockItem_GetNext ( uVBLock, pItem );
    if ( aItemNext )
      pVBLockNext = (VBLock *)OBJ__Msg2Phys(aItemNext);

    // Item housekeeping
    // NOTES: Remove linkages etc
    if ( aItemPrev )
      VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aItemNext );
    if ( aItemNext )
      VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aItemPrev );

    // List housekeeping
    // NOTES: Counts and linkages etc
    VBLockList_SetItems ( uVBLock, VBLock_pList(OBJ__VBLock), -1, false );
    if ( VBLockList_GetFirst(uVBLock,pVBLockList,0) == aItem )
      VBLockList_SetFirst ( uVBLock, pVBLockList, aItemNext );
    if ( VBLockList_GetLast(uVBLock,pVBLockList,0) == aItem )
      VBLockList_SetLast ( uVBLock, pVBLockList, aItemPrev );

    // Isolate
    VBLockItem_SetPrev  ( uVBLock, pItem, 0 );
    VBLockItem_SetNext  ( uVBLock, pItem, 0 );
    VBLockItem_SetParent( uVBLock, pItem, 0 );
    return aItem;
}

//
//  Links passed VBLockItem into VBLockNode
//  NOTES: Handles bit addressing translations
//
//  Parameters:  P2PmsgVBL
//               Virtual block list manager
//
//               VBLockNode *pVBLockNode
//               Node into which VBLockItem is to be linked
//
//               UINT aItemPrev
//               VBLock address of the previous VBLockItem
//
//               UINT aItem
//               VBLock address of VBLockItem to be linked in
//
//               VBLaddr aItemNext
//               VBLock address of the next VBLockItem
/*void
P2PmsgNode_SortinItem ( P3PmsgNode *pNode, const P3PmsgName& oName
                      , VBLaddr aItem )
{
    // Locals
    P3PmsgCurs oCurs ( *pNode );
    VBLaddr    aItemPrev = 0;
    VBLaddr    aItemNext = 0;
    // Optimisation - Empty list
    if ( pNode->GetCount() <= 0 )
    {
//fwprintf(stdout,"oName=%s\n",oName.c_name());
      P2PmsgNode_LinkinItem ( pNode, 0, aItem, 0 );
      return;
    }

    // Optimisation - First in list
    int  imin = 0;
    if ( oCurs.Goto(imin) && oName < oCurs.r_name() )
    {
//fwprintf(stdout,"oName=%s < r_name(imin)=%s \n", oName.c_name(), oCurs.r_name().c_name() );
      P2PmsgNode_LinkinItem ( pNode, 0, aItem, P3PmsgCurs_GetVBLocknn(oCurs) );
      return;
    }

    // Optimisation - Last in list
    int  imax = pNode->GetCount() - 1;
    if ( oCurs.Goto(imax) && oName > oCurs.r_name() )
    {
//fwprintf(stdout,"r_name(imin)=%s < oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
      P2PmsgNode_LinkinItem ( pNode, P3PmsgCurs_GetVBLocknn(oCurs), aItem, 0 );
      return;
    }

    // Perform binary search by continually narrowing search until just
    // one element remains
    while ( imax > imin+1 )
    {
      int imid = (imin + imax ) /2;
 
      // code must guarantee the interval is reduced at each iteration
      ASSERT(imid < imax);
      // note: 0 <= imin < imax implies imid will always be less than imax
 
      // Reduce the search
      oCurs.Goto ( imid );
      if ( oCurs.r_name() < oName )
        imin = imid + 1;
      else if ( oCurs.r_name() > oName )
        imax = imid - 1;
      else
      {
        ASSERT(0); //TODO:LJM delete V0_0_5 debug hack 
        imin = imid;
      }
    }

      // Insertion before minimum
      //oCurs.Goto(imin);
      //LPCTSTR lpszImin = oCurs.r_name().c_name();
      //LPCTSTR lpszName = oName.c_name();
      //oCurs.Goto(imax);
      //LPCTSTR lpszImax = oCurs.r_name().c_name();
    // Insertion before minimum
    if ( oCurs.Goto(imin) && oName < oCurs.r_name() )
    {
//fwprintf(stdout,"r_name(imin)=%s < oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
      VBLock *pVBLock = P3PmsgCurs_GetVBlock(oCurs);
      aItemPrev = VBLockItem_GetPrev ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      aItemNext = P3PmsgCurs_GetVBLocknn ( oCurs );
    }
    // Insertion before maximum
    else if ( oCurs.Goto(imax) && oName < oCurs.r_name() )
    {
//fwprintf(stdout,"r_name(imax)=%s > oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
      VBLock *pVBLock = P3PmsgCurs_GetVBlock(oCurs);
      aItemPrev = VBLockItem_GetPrev ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      aItemNext = P3PmsgCurs_GetVBLocknn ( oCurs );
    }
    // Insertion after maximum
    else if ( oCurs.Goto(imax) && oCurs.r_name() < oName )
    {
//fwprintf(stdout,"r_name()=%s <= oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
      VBLock *pVBLock = P3PmsgCurs_GetVBlock(oCurs);
      aItemPrev = P3PmsgCurs_GetVBLocknn ( oCurs );
      aItemNext = VBLockItem_GetNext ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    }
    else { ASSERT(0); }
    //
    //if ( oCurs.Goto(imin) && oCurs.r_name() < oName )
    //{
//fwprintf(stdout,"r_name(imin)=%s < oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
    //  VBLock *pVBLock = P3PmsgCurs_GetVBlock(oCurs);
    //  aItemPrev = VBLockItem_GetPrev ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    //  aItemNext = P3PmsgCurs_GetVBLocknn ( oCurs );
    //}

    // Delegation of implementation
    P2PmsgNode_LinkinItem ( pNode, aItemPrev, aItem, aItemNext );
}*/
/*void
P2PmsgNode_LinkinItem ( P3PmsgNode *pNode
                      , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext )
{
    // Locals
    P3PmsgObject& m_oObject = (P3PmsgObject&)pNode->r_Object();
    VBLock     *pItemPrev   = (VBLock *)OBJ__Msg2Phys(aItemPrev);
    VBLock     *pItem       = (VBLock *)OBJ__Msg2Phys(aItem);
    VBLock     *pItemNext   = (VBLock *)OBJ__Msg2Phys(aItemNext);
    VBLock     *pVBLock     =           OBJ__VBLock;
    VBLockNode *pVBLockNode =           P3PmsgNode__GetVBLockNode(pNode); //TODO:LJM deprecated pNode->GetVBLockNode();

    VBLockItem *pItem_nn    = VBLock_pItem(pItem);

    // Observe addressing model
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr08 )
    {
      pItem_nn->ua.oItem08.aParent = (UINT08)OBJ__VBLocknn;
      goto P08;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr16 )
    {
      pItem_nn->ua.oItem16.aParent = (UINT16)OBJ__VBLocknn;
      goto P16;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr32 )
    {
      pItem_nn->ua.oItem32.aParent = (UINT32)OBJ__VBLocknn;
      goto P32;
    }
    EVERR->Module ("P2PmsgNode")
         ->Message("Internal VBLockNode.uVBLockAddr.ua corruption" )
         ->Throw ( );

    // P2PmsgItem - Prev Linkages
P08:pItem_nn->ua.oItem08.aPrev = (UINT08)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem08.aNext = (UINT08)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N08;
P16:pItem_nn->ua.oItem16.aPrev = (UINT16)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem16.aNext = (UINT16)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N16;
P32:pItem_nn->ua.oItem32.aPrev = (UINT32)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem32.aNext = (UINT32)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N32;

    // P2PmsgItem - Next Linkages
N08:pItem_nn->ua.oItem08.aNext = (UINT08)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem08.aPrev = (UINT08)aItem;
    goto H08;
N16:pItem_nn->ua.oItem16.aNext = (UINT16)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem16.aPrev = (UINT16)aItem;
    goto H16;
N32:pItem_nn->ua.oItem32.aNext = (UINT32)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem32.aPrev = (UINT32)aItem;
    goto H32;

    // P2PmsgNode - Housekeeping
H08:if ( pItem_nn->ua.oItem08.aPrev == 0 )
      pVBLockNode->u.oNode08.aFirst = (UINT08)aItem;
    if ( pItem_nn->ua.oItem08.aNext == 0 )
      pVBLockNode->u.oNode08.aLast  = (UINT08)aItem;
    pVBLockNode -> u.oNode08.nItems++;
    goto END;
H16:if ( pItem_nn->ua.oItem16.aPrev == 0 )
      pVBLockNode->u.oNode16.aFirst = (UINT16)aItem;
    if ( pItem_nn->ua.oItem16.aNext == 0 )
      pVBLockNode->u.oNode16.aLast  = (UINT16)aItem;
    pVBLockNode -> u.oNode16.nItems++;
    goto END;
H32:if ( pItem_nn->ua.oItem32.aPrev == 0 )
      pVBLockNode->u.oNode32.aFirst = (UINT32)aItem;
    if ( pItem_nn->ua.oItem32.aNext == 0 )
      pVBLockNode->u.oNode32.aLast  = (UINT32)aItem;
    pVBLockNode -> u.oNode32.nItems++;
    goto END;

    // Tidy up and
END:ASSERT(VBLock_IsLinked(pItem));
    //ASSERT(VBLock_IsAlloc(pItem));
    return;
}*/

///////////////////////////////////////////////////////////////////////
//  Garbage collectors

void
P2PmsgField_DropData   ( P3PmsgField *pField )
{
   VBLockData *pData = P2PmsgObject_pData ( pField->r_Object(), false );  //TODO:LJM deprecated pField -> GetVBLockData ( false );
   if ( pData->uDataType == (UCHAR)~0 )
   {
     UCHAR   uVBLock     = pField->r_Object().m_uVBLock;
     VBLaddr aChain2Next = VBLockData_GetChain2Next ( uVBLock, pData );
     //((P3PmsgObject&)pField -> r_Object()).Free ( pData->u.aVBLockData );
     ((P3PmsgObject&)pField -> r_Object()).Free ( aChain2Next );
     //pData  -> u.aVBLockData = 0;
     VBLockData_SetChain2Next ( uVBLock, pData, 0 );
   }
}
void
P2PmsgField_DropName   ( P3PmsgField *pField )
{
   VBLockName *pName = P2PmsgObject_pName ( pField->r_Object(), false );
   //VBLockName *pName = pField -> GetVBLockName ( false ); //TODO:LJM deprecated
   //UCHAR       uAttr = pName  -> uVBLockAttr; 
   if ( VBLockName_IsChained(pName) )
   {
      VBLaddr aVBLock1 = VBLockName_GetChain2Next ( pField->r_Object().m_uVBLock, pName );
      VBLock *pVBLock1 = (VBLock *)pField->r_Object().Msg2Phys ( aVBLock1 );
      VBLockName_SetChain2Next ( pField->r_Object().m_uVBLock, pName, 0 );
      ((P3PmsgObject&)pField -> r_Object()).Free ( aVBLock1 );
      // new above, old below
      //VBLock *pVBLockName = (VBLock *)pField->r_Object().Msg2Phys(pName->u.vBlin08.aVBLockAddr);  //worktag
      //ASSERT(VBLock_IsLinked(pVBLockName));
      //((P3PmsgObject&)pField -> r_Object()).Free ( pName->u.vBlin08.aVBLockAddr );
      //pName  -> u.vBlin08.aVBLockAddr = 0;
   }
}


///////////////////////////////////////////////////////////////////////
//  Recursive P2Pmsg[Field,Node,Attr]_Merge Utilities and helpers
//  NOTES: For clarity and simplicity operations isolated in series
//         of self contained static functions

/*P3PmsgField&
P2PmsgField_Merge ( P3PmsgField& oField, const P3PmsgField& rhs )
{
    // Field assignment
    (P3PmsgData&)oField = rhs;

    // Attribute assignment
    if ( !((P3PmsgField&)rhs).r_Attr().IsEmpty() )
      P2PmsgAttr_Merge ( oField.r_Attr(P3PmsgField::AttrCMD_Create)
                       , ((P3PmsgField&)rhs).r_Attr() );

    // Tidy up, and
    return oField;
}


P3PmsgAttr&
P2PmsgAttr_Merge ( P3PmsgAttr& oAttr, const P3PmsgAttr& rhs )
{
    // Recursive copy
    P3PmsgCurs& oCurs = ((P3PmsgAttr&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
oCurs.AssertValid();
ASSERT(oCurs.Item()==i);
      if ( oCurs.IsNode() )
      {
        //  COPY - see the P3PmsgCurs widening-ring note in P2PmsgNode_Merge above.
        CString strNodename  = oCurs.r_node().c_name();
        LPCTSTR lpszNodename = strNodename;
        if ( oAttr.Exists(lpszNodename) )
          P2PmsgNode_Merge ( oAttr.SelectNode(lpszNodename), oCurs.r_node() );
        else
          oAttr += oCurs.r_node();
      }
      else if ( oCurs.IsList() )
      {
        ASSERT(0);
        oAttr += oCurs.r_list();
      }
      else if ( oCurs.IsVect() )
      {
        ASSERT(0);
        oAttr += oCurs.r_vect();
      }
      else if ( oCurs.IsField() )
      {
        //  COPY - see the P3PmsgCurs widening-ring note in P2PmsgNode_Merge above.
        CString strFieldname  = oCurs.r_field().c_name();
        LPCTSTR lpszFieldname = strFieldname;
        if ( oAttr.Exists(lpszFieldname) )
          P2PmsgField_Merge ( oAttr.SelectItem(lpszFieldname), oCurs.r_field() );
        else
          oAttr += oCurs.r_field();
      }
      else
        ASSERT(0);
//oAttr.AssertValid();//TODO:LJM delete, testing
    }

    // Tidy up, and
    return oAttr;
}*/

///////////////////////////////////////////////////////////////////////
//  P2Pmsg serialisation helpers
//  NOTES: Perform standard activities

//P3PmsgNode&
//P3PmsgField_SERIALISE ( P3PmsgNode& oNode
//                      , LPCTSTR lpszFieldname, const P3PmsgData& oData
//                      , bool bDscAttr, LPCTSTR lpszDescription )
//{
//    P3PmsgField oField ( lpszFieldname, oData );
//    if ( bDscAttr )
//      oField.r_Attr(P3PmsgField::AttrCMD_Create)
//        += P3PmsgField ( _T("Dsc"), lpszDescription );
//    oNode += oField;
//    return oNode;
//}
//
//P3PmsgAttr&
//P2PmsgAttr_SERIALISE  ( P3PmsgAttr& oAttr, bool bOverwrite
//                      , LPCTSTR lpszFieldname, const P3PmsgData& oData
//                      , bool bDscAttr, LPCTSTR lpszDescription )
//{
//    P3PmsgField oField ( lpszFieldname, oData );
//    if ( bDscAttr )
//      oField.r_Attr(P3PmsgField::AttrCMD_Create)
//        += P3PmsgField ( _T("Dsc"), lpszDescription );
//    if ( !oAttr.Exists(lpszFieldname) )
//      oAttr += oField;
//    else if ( bOverwrite )
//      oAttr.SelectItem(lpszFieldname) = oField;
//    else
//      EVERR->MODULE
//           ->AFP(lpszFieldname)
//           ->Message(_T("Field already exists, no overwrite permission") )
//           ->Throw();
//    return oAttr;
//}

//
//P3PmsgField&
//Decorate4Grid_COLOR ( P3PmsgField& oField, UCHAR ucAttributes
//                    , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription )
//{
//    oField.r_Attr().PushBack ( P3PmsgField ( _T("#Lab"), lpszGridLabel ) );
//    oField.r_Attr().PushBack ( P3PmsgField ( _T("#Dsc"), lpszGridDescription ) );
//    oField.r_Attr().PushBack ( P3PmsgField ( _T("#Typ"), _T("COLOR") ) );
//    return oField;
//}
//
//P3PmsgField&
//Decorate4Grid_FILE ( P3PmsgField& oField, UCHAR ucAttributes
//                   , LPCTSTR lpszGridLabel, LPCTSTR lpszGridDescription )
//{
//    oField.r_Attr().PushBack ( P3PmsgField ( _T("#Lab"), lpszGridLabel ) );
//    oField.r_Attr().PushBack ( P3PmsgField ( _T("#Dsc"), lpszGridDescription ) );
//    oField.r_Attr().PushBack ( P3PmsgField ( _T("#Typ"), _T("FILE") ) );
//    return oField;
//}

///////////////////////////////////////////////////////////////////////
//  P2Pmsg helpers
//  NOTES: Perform standard activities

//
//  Parses item path for root name
//  NOTES: ":Rootname.item@item.etc" parses to "Rootname"
//         "[.]item@item.etc parses to "", and
//         "[@]item.etc parses to ""
//
//  Parameters:  LPCTSTR lpszItemPath
//               Item path to be parsed
//
//  Returns:     CString
//               Parsed root name
CString
P3Pmsg_GetRootname ( LPCTSTR lpszItemPath )
{
    CString strRootname;
    if ( *lpszItemPath != L':' && *lpszItemPath != T_DescDelim )
      return strRootname;
   lpszItemPath++;
    while ( *lpszItemPath && *lpszItemPath != T_DescDelim && *lpszItemPath != T_AttrDelim )
      strRootname += *lpszItemPath++;
    return strRootname;
}

//
//  How many '^' name pItem from the item at aOwner, or 0 if that item's stack
//  does not hold pItem at all.
//
//  The walk is the answer to both questions at once: a snapshot records its
//  OWNER in aParent, not the generation above it, so the distance has to be
//  counted -- and counting it means following the chain, which is also what
//  proves the block is on it. A parent that points at an item for some other
//  reason (a list holding an element, say) simply never matches and comes back
//  zero, which is the caller's signal to carry on as before.
//
//  It is also the test for "am I a snapshot": the three Drop implementations
//  read aParent to decide which collection to unlink themselves from, and a
//  snapshot belongs to none -- MsgStck has already taken it off the aStack
//  chain by then. That code was correct only because a stack block's aParent
//  used to be zero.
Msgcore_EXT VBLsize
P3Pmsg_GetStckDepth ( const P3PmsgField *pItem, VBLaddr aOwner )
{
    if ( pItem == nullptr || aOwner == 0 )
      return 0;
    VBLock *pOwner = (VBLock *)pItem -> r_Object().Msg2Phys ( aOwner );
    if ( pOwner == nullptr || !VBLock_IsItem(pOwner) )
      return 0;

    const UCHAR   uVBLock = pOwner->oHdr.uVBLockDefs;
    const VBLaddr aItem   = pItem->r_Object().GetVBLocknn ( );
    if ( aItem == 0 )
      return 0;

    VBLaddr aStck  = VBLockItem_GetStack ( uVBLock, VBLock_pItem(pOwner) );
    VBLsize nDepth = 1;
    while ( aStck )
    {
      if ( aStck == aItem )
        return nDepth;
      VBLock *pStck = (VBLock *)pItem -> r_Object().Msg2Phys ( aStck );
      if ( pStck == nullptr || !VBLock_IsItem(pStck) )
        return 0;
      aStck = VBLockItem_GetStack ( pStck->oHdr.uVBLockDefs, VBLock_pItem(pStck) );
      nDepth++;
    }
    return 0;
}

CString
P3Pmsg_GetAttrPath ( const P3PmsgField *pField, VBLaddr aParent )
{
    CString strPath;
    VBLock *pParent = (VBLock *)pField -> r_Object().Msg2Phys ( aParent );
    //if ( ( VBLock_IsItem(pParent)                   &&
    //       VBLockItem_IsNode(VBLock_pItem(pParent))    ) ||
    //     VBLock_IsNode(pParent)                             )
    //{
    //  P3PmsgNode oNodeParent( pField->GetP2PmsgHandle(), aParent, 0 );
    //  return P3Pmsg_GetPath ( &oNodeParent );
    //}
    if ( ( VBLock_IsItem(pParent)                    &&
           VBLockItem_IsField(VBLock_pItem(pParent))    ) /*||
         VBLock_IsNode(pParent)*/                              )
    {
      P3PmsgField oFieldParent( pField->GetP2PmsgHandle(), aParent, 0 );
      return P3Pmsg_GetPath ( &oFieldParent );
    }
    if ( VBLock_IsAttr(pParent) )
    {
      VBLockAttr *pAttr    = VBLock_pAttr ( pParent );
      VBLaddr     aAttrPar = VBLockAttr_GetParent ( pParent->oHdr.uVBLockDefs, pAttr );
      if ( aAttrPar )
        strPath += P3Pmsg_GetAttrPath ( pField, aAttrPar );
      strPath += _T("@");
      return strPath;
    }
    if ( ( VBLock_IsItem(pParent)                   &&
           VBLockItem_IsList(VBLock_pItem(pParent))    ) ||
         VBLock_IsList(pParent)                             )
    {
      P3PmsgList oListParent( pField->GetP2PmsgHandle(), aParent, 0 );
      return P3Pmsg_GetPath ( &oListParent );
    }
    ASSERT(0);
    return L"Unknown";
}

CString
P3Pmsg_GetDescPath ( const P3PmsgField *pField, VBLaddr aParent )
{
    CString strPath;
    VBLock *pParent = (VBLock *)pField -> r_Object().Msg2Phys ( aParent );
    //if ( ( VBLock_IsItem(pParent)                   &&
    //       VBLockItem_IsNode(VBLock_pItem(pParent))    ) ||
    //     VBLock_IsNode(pParent)                             )
    //{
    //  P3PmsgNode oNodeParent( pField->GetP2PmsgHandle(), aParent, 0 );
    //  return P3Pmsg_GetPath ( &oNodeParent );
    //}
    if ( ( VBLock_IsItem(pParent)                    &&
           VBLockItem_IsField(VBLock_pItem(pParent))    ) /*||
         VBLock_IsNode(pParent)  */                            )
    {
      P3PmsgField oFieldParent( pField->GetP2PmsgHandle(), aParent, 0 );
      return P3Pmsg_GetPath ( &oFieldParent );
    }
    if ( VBLock_IsAttr(pParent) )
    {
      VBLockAttr *pAttr    = VBLock_pAttr ( pParent );
      VBLaddr     aAttrPar = VBLockAttr_GetParent ( pParent->oHdr.uVBLockDefs, pAttr );
      if ( aAttrPar )
        strPath += P3Pmsg_GetAttrPath ( pField, aAttrPar );
      strPath += L"@";
      return strPath;
    }
    if ( ( VBLock_IsItem(pParent)                   &&
           VBLockItem_IsList(VBLock_pItem(pParent))    ) ||
         VBLock_IsList(pParent)                             )
    {
      P3PmsgList oListParent( pField->GetP2PmsgHandle(), aParent, 0 );
      return P3Pmsg_GetPath ( &oListParent );
    }
    ASSERT(0);
    return L"Unknown";
}

//
//  Fetch absolute path for P3PmsgItem
//
//  Parameters:  const P3PmsgItem *pItem
//               Item for which path is to be evaluated
//
//  Returns:     CString
//               [Rootname:|.]item1.item2@item4.etc
//               [:Rootname|.item0].item1.item2@item3.etc
CString
P3Pmsg_GetPath ( const P3PmsgField *pItem )
{
    CString strPath;
    // Process root
    // NOTES: Defined by absence of parent.
    //      : May be physical root object ":Rootname", or
    //      : Floating item ".item"
    VBLaddr aParent = P2PmsgField_GetVBLockParentnn ( pItem );
    if ( aParent == NULL )
    {
      //if ( pItem->r_Object().IsRoot() )
      //{
      //  strPath  = L":";
      //  strPath += pItem -> c_name();
      //  return strPath;                // Root item
		  //}
      strPath  = L".";
      strPath += pItem -> c_name();
      return strPath;                  // Floating item
    }
    VBLock *pParent = P2PmsgField_GetVBLockParent ( pItem );
    VBLockItem *pParentItem = VBLock_pItem ( pParent );

    //  Process stack parent
    //  NOTES: A snapshot is its owner at an earlier moment, not a child of it,
    //         so its path is the owner's plus one '^' per generation and no
    //         name of its own -- '^' IS the component (§5 of stack_paths.md).
    //       : MsgStck::Push left aParent zero, which the floating-item arm
    //         above reads as "no parent" -- so every snapshot of BHP answered
    //         ".BHP" wherever BHP actually lived, and every GENERATION
    //         answered the same ".BHP". Not a path to this object, and a path
    //         to a different one.
    //       : A snapshot in an image written before that still carries zero
    //         and still comes out of the arm above, unchanged.
    if ( VBLock_IsItem(pParent) )
    {
      const VBLsize nStck = P3Pmsg_GetStckDepth ( pItem, aParent );
      if ( nStck )
      {
        P3PmsgField oOwner ( pItem->GetP2PmsgHandle(), aParent, 0 );
        strPath = P3Pmsg_GetPath ( &oOwner );
        for ( VBLsize i = 0; i < nStck; i++ )
          strPath += T_StckDelim;
        return strPath;                // The name is the owner's, and said
      }                                // once: '^' carries none of its own
    }

    // Process descendent parent
    if ( VBLock_IsDesc(pParent) )
    {
      VBLockDesc *pDesc    = VBLock_pDesc ( pParent );
      VBLaddr     aDescPar = VBLockDesc_GetParent ( pParent->oHdr.uVBLockDefs, pDesc );
      if ( aDescPar )
        strPath += P3Pmsg_GetDescPath ( pItem, aDescPar );
      strPath += pItem->r_Object().IsRoot() ? L":" : L".";
      //strPath += _T("."); //TODO:LJM displaced by above
    }
    //else if ( VBLock_IsNode(pParent)         ||
    //          VBLockItem_IsNode(pParentItem)    )
    //{
    //  P3PmsgNode oNodeParent( pItem->GetP2PmsgHandle(), aParent, 0 );
    //  strPath  = P3Pmsg_GetPath ( &oNodeParent );
    //  strPath += oNodeParent.r_Object().IsRoot() ? L":" : L".";
    //}
    else if ( VBLock_IsList(pParent)         ||
              VBLockItem_IsList(pParentItem)    )
    {
      P3PmsgList oListParent( pItem->GetP2PmsgHandle(), aParent, 0 );
      strPath += P3Pmsg_GetPath ( &oListParent );
      strPath += oListParent.r_Object().IsRoot() ? L":" : L".";
    }
    else if ( VBLock_IsField(pParent)         ||
              VBLockItem_IsField(pParentItem)    )
    {
      P3PmsgField oFieldParent( pItem->GetP2PmsgHandle(), aParent, 0 );
      strPath += P3Pmsg_GetPath ( &oFieldParent );
      strPath += oFieldParent.r_Object().IsRoot() ? L":" : L".";
    }
    else if ( VBLock_IsAttr(pParent) )
    {
      VBLockAttr *pAttr    = VBLock_pAttr ( pParent );
      VBLaddr     aAttrPar = VBLockAttr_GetParent ( pParent->oHdr.uVBLockDefs, pAttr );
      if ( aAttrPar )
        strPath += P3Pmsg_GetAttrPath ( pItem, aAttrPar );
      strPath += T_AttrDelim; //_T("@");
    }
    else
    {
      ASSERT(0);
    }

    // Tidy up, and
    strPath += pItem->c_name();
    return strPath;
}
VBLock*
P2PmsgAttr__GetVBLockParent ( const P3PmsgAttr *pAttr );

CString
P3Pmsg_GetPath ( const P3PmsgAttr *pAttr )
{
    CString strPath;

    // Process parent
    //  A COLLECTION'S PARENT IS AN ITEM -- the item it hangs off -- and every
    //  real item is a VBLock_Item block. VBLock_Field and VBLock_List are
    //  different block TYPES, not item types, which is why every other walk in
    //  this file spells the test "VBLock_IsX(pParent) || VBLockItem_IsX(...)".
    //  So neither of the two arms that used to stand here could ever fire, the
    //  path fell to the ASSERT(0) below them, and what came back was a lone
    //  '@' with no owner in front of it. Both arms also appended a '.' before
    //  that '@', which no spelling of an attribute path has ever carried.
    //
    //  Only if it got that far: P2PmsgAttr_GetVBLockParentnn was reading the
    //  parent out of the owning item's block, so the usual outcome was a
    //  segfault rather than a wrong string.
    VBLaddr aParent = P2PmsgAttr_GetVBLockParentnn ( pAttr );
    if ( aParent )
    {
      VBLock *pParent = P2PmsgAttr__GetVBLockParent ( pAttr );
      //if ( VBLock_IsNode(pParent) )
      //{
      //  P3PmsgNode oNodeParent( pAttr->GetField()->GetP2PmsgHandle(), aParent, 0 );
      //  strPath += P3Pmsg_GetPath ( &oNodeParent );
      //  strPath += _T(".");
      //}
      if ( pParent && VBLock_IsItem(pParent) )
      {
        //  One arm for all three item types: a list and a vector carry
        //  attributes exactly as a field does, and P3PmsgList and P3PmsgVect
        //  both derive from P3PmsgField, so the owner's own path is built the
        //  same way whichever it is (§6).
        P3PmsgField oOwner( pAttr->GetField()->GetP2PmsgHandle(), aParent, 0 );
        strPath += P3Pmsg_GetPath ( &oOwner );
      }
      else
      {
        ASSERT(0);
      }
    }

    // Tidy up, and
    //  '@' with no name after it IS the component. It names the collection
    //  itself, the way '^' with nothing after it names a snapshot (§5), and
    //  P3Pmsg_SplitRootPath keeps it for the same reason.
    strPath += T_AttrDelim; //_T("@");
    return strPath;
}
VBLaddr
P2PmsgDesc_GetVBLockParentnn ( const P3PmsgDesc *pDesc );
VBLock*
P2PmsgDesc__GetVBLockParent ( const P3PmsgDesc *pDesc );

//
//  Builds the path of a DESCENDANT collection
//  NOTES: The mirror of the P3PmsgAttr overload above, and it reads the same
//         way: a collection's parent is the ITEM it hangs off, and the
//         component that names it is its delimiter carrying no name.
//       : §15 is why this can exist at all. A path could not END in a bare
//         '.' until the splitter stopped dropping one, so until then the
//         string this returns would not have resolved -- and §12 declined to
//         emit a path that could not come back, which is the whole reason
//         this overload was missing rather than merely unwritten.
CString
P3Pmsg_GetPath ( const P3PmsgDesc *pDesc )
{
    CString strPath;

    // Process parent
    VBLaddr aParent = P2PmsgDesc_GetVBLockParentnn ( pDesc );
    if ( aParent )
    {
      VBLock *pParent = P2PmsgDesc__GetVBLockParent ( pDesc );
      if ( pParent && VBLock_IsItem(pParent) )
      {
        //  One arm for all three item types, as above: a list and a vector
        //  carry descendants exactly as a field does (§6).
        P3PmsgField oOwner( pDesc->GetField()->GetP2PmsgHandle(), aParent, 0 );
        strPath += P3Pmsg_GetPath ( &oOwner );
      }
      else
      {
        ASSERT(0);
      }
    }

    // Tidy up, and
    strPath += T_DescDelim; //_T(".");
    return strPath;
}

LPCTNAM
ParseObjectPath ( LPCTNAM lpszObjectPath, LPTNAM lpszObjectname, int nObjectnameChars )
{
    // Parse out the immediate object name
    // NOTES: [.|@|^]objectname[.|@|^]objectname[.|@|^]etc
    ZeroMemory ( (void *)lpszObjectname, nObjectnameChars*sizeof(lpszObjectname[0]) );
    LPCTNAM lpszParsedname = lpszObjectPath;
    int     i = 0;
    while ( *lpszParsedname                &&
            *lpszParsedname != T_DescDelim &&
            *lpszParsedname != T_AttrDelim &&
            *lpszParsedname != T_StckDelim &&
            *lpszParsedname != L':'           )
    {
      if ( i >= nObjectnameChars )
        ASSERT(0); //TODO:LJM Throw memory overrun exception
      lpszObjectname[i++] += *lpszParsedname++;
    }
    return lpszParsedname;
}
P3PmsgObject
P3Pmsg_SelectObjectRecurse ( const P3PmsgObject *pObject, LPCTNAM lpszObjectPath )
{
    // Locals;
    TCHAR     nsObjectname[MAX_TNAME_SIZE];
    LPCTNAM lpszParsedname = ParseObjectPath ( lpszObjectPath, nsObjectname, ARRAYSIZE(nsObjectname) );

    //  AN OBJECT THAT NAMES NO BLOCK IS AN ORDINARY ANSWER, NOT A LOGIC ERROR.
    //  r_Attr() on an item that has no attributes hands back one of these, so
    //  "Item@Tag" -- and now "Item@^" -- arrive here with nothing to select
    //  against. Every arm below tests a type this object does not have, so it
    //  used to fall all the way through to the ASSERT(0) at the end of the
    //  function, by way of IsRoot() on the way past, which asserts a SECOND
    //  time on a SYS-heap object (P2PmsgHeap_IsRoot has no arm for one). The
    //  value RETURNED was already right; it was the two assertions that were
    //  wrong, and asking a bare item for an attribute it has not got is not a
    //  debug-build event.
    //
    //  Tested on the ADDRESS and not with IsVoid(), which wants m_hVBList and
    //  m_aVBLock BOTH zero. An empty collection keeps the handle of the heap it
    //  would have been allocated from and carries no block, so it is not void
    //  by that test -- it was the half of the condition that made this look
    //  handled when it was not. GetVBLocknn() is the plain accessor and
    //  dereferences nothing, which matters here: there is nothing to read.
    if ( pObject->IsVoid() || pObject->GetVBLocknn() == 0 )
      return P3PmsgObject();         // Selection path broken

    // P3PmsgNodes
    /*if ( pObject->IsNode() )
    {
      ASSERT(0);
      P3PmsgNode oNode = *pObject;
      if ( *lpszObjectPath == T_DescDelim )
      {
        return P3Pmsg_SelectObjectRecurse ( &oNode.r_Object(), ++lpszObjectPath );
        //lpszObjectPath = ParseObjectPath ( ++lpszObjectPath, strObjectname );
        //if ( oNode.r_name().c_wcsicmp(strObjectname) )
        //  return P3PmsgObject();         // Selection path broken
        //if ( *lpszObjectPath == NULL )
        //  return *pObject;
        //return P3Pmsg_SelectObject ( pObject, lpszObjectPath );
      }
      if ( *lpszObjectPath == T_AttrDelim )
        return P3Pmsg_SelectObjectRecurse ( &oNode.r_Attr().r_Object(), ++lpszObjectPath );
      if ( *lpszObjectPath == T_StckDelim )
      {
        ASSERT(0);
        return P3PmsgObject();
      }
      if ( !oNode.r_Curs().Goto(nsObjectname) )
        return P3PmsgObject();         // Selection path broken;
      if ( *lpszParsedname == 0 )
        return oNode.r_Curs().r_Object();
      return P3Pmsg_SelectObjectRecurse ( &oNode.r_Curs().r_Object(), lpszParsedname );
    }*/

    // Items, lists and vectors
    //  One arm, three item types. The VBLockItem header is the same six
    //  addresses -- aParent, aPrev, aNext, aExtra, aStack, aDescn -- whatever
    //  the ut union under it holds, and P3PmsgList and P3PmsgVect both derive
    //  from P3PmsgField, so '.', '@', '^' and a descendant name mean on a list
    //  or a vector exactly what they mean on a field. Reaching here with one is
    //  ordinary: P3PmsgCurs::Goto connects m_oP3PmsgList / m_oP3PmsgVect for a
    //  match of that type (MsgCurs.cpp), so any path that names a list and then
    //  keeps going arrives in this function with IsList() true. That used to be
    //  an ASSERT(0) below and then a void return -- no component of any kind
    //  resolved against a list, and a vector had no arm at all and fell through
    //  to the ASSERT(0) at the end of the function.
    if ( pObject->IsField() || pObject->IsList() || pObject->IsVect() )
    {
      P3PmsgField oField = *pObject;
      //  '.' with no NAME after it names the DESCENDANT collection, exactly as
      //  '@' with nothing after it names the attribute one below. Both used to
      //  re-enter with an empty path, look for a name that was not there and
      //  answer void (§15).
      //
      //  "No name after it" is not the same as "nothing after it", and the
      //  difference is this arm's half of §17. The '@' arm below hands the
      //  REST of the path to the attribute collection, which is why "Item@^"
      //  reaches the collection's snapshot. This one handed the rest back to
      //  the ITEM, so "Item.^" re-entered here and took the '^' arm -- the
      //  item's own snapshot, not the collection's. Two spellings a single
      //  step apart, meaning objects two levels apart, and only one of them
      //  the one §9 predicts.
      //
      //  So a following DELIMITER goes to the collection and a following NAME
      //  goes back to the item. The second is not a detour: "Item.Last" wants
      //  a descendant by name, and the field arm's own tail already looks one
      //  up -- r_Desc().r_Curs().Goto -- so both routes land on the same
      //  object and the shorter one is left alone.
      if ( *lpszObjectPath == T_DescDelim )
      {
        if ( P3Pmsg_IsPathDelimiter ( ++lpszObjectPath ) )
        {
          P3PmsgObject oDescColl = oField.r_Desc().r_Object();
          if ( oDescColl.GetVBLocknn() == 0 )
            return P3PmsgObject();     // No descendants; selection path broken
          if ( *lpszObjectPath == 0 )
            return oDescColl;
          return P3Pmsg_SelectObjectRecurse ( &oDescColl, lpszObjectPath );
        }
        return P3Pmsg_SelectObjectRecurse ( &oField.r_Object(), lpszObjectPath );
        //lpszObjectPath = ParseObjectPath ( ++lpszObjectPath, nsObjectname, ARRAYSIZE(nsObjectname) );
        //if ( oField.r_name().c_wcsicmp(nsObjectname) )
        //  return P3PmsgObject();         // Selection path broken
        //if ( *lpszObjectPath == NULL )
        //  return *pObject;
        //return P3Pmsg_SelectObjectRecurse ( pObject, lpszObjectPath );
      }
      //  '@' with nothing after it names the COLLECTION, the way '^' with
      //  nothing after it names the snapshot below. Recursing into it with an
      //  empty path looked for a name that was not there and answered void, so
      //  the one object with no other spelling could not be reached -- and it
      //  is the object P3Pmsg_GetPath emits a path for.
      if ( *lpszObjectPath == T_AttrDelim )
      {
        if ( *++lpszObjectPath == 0 )
        {
          P3PmsgObject oAttrColl = oField.r_Attr().r_Object();
          if ( oAttrColl.GetVBLocknn() == 0 )
            return P3PmsgObject();     // No attributes; selection path broken
          return oAttrColl;
        }
        return P3Pmsg_SelectObjectRecurse ( &oField.r_Attr().r_Object(), lpszObjectPath );
      }
      //  '^' follows the item's stack: the aStack address a Push() wrote, so a
      //  path can name a value the field USED to hold. The delimiter has been
      //  in the grammar from the start -- ParseObjectPath stops on it,
      //  P3Pmsg_IsPathDelimiter answers TRUE for it and P3Pmsg_IsValidItemname
      //  rejects it from item names -- but this arm, the one place following it
      //  would have happened, was never written and asserted instead.
      //
      //  Pushes nest (MsgStck::Push re-links the current head onto the new
      //  item), so the delimiter repeats: "Field^" is the item as it stood
      //  before the last push, "Field^^" before the one before that. Whatever
      //  follows is read against the pushed item exactly as it would be against
      //  a live one, because a pushed item IS a whole item -- Push copies name,
      //  data, attributes and descendants -- so "Field^.Child" and
      //  "Field^@Attr" mean inside the snapshot what they mean outside it.
      if ( *lpszObjectPath == T_StckDelim )
      {
        if ( !oField.IsStacked() )
          return P3PmsgObject();       // Nothing pushed; selection path broken
        //  MsgStck keeps one accessor per item type and each THROWS if asked
        //  for the wrong one, so the type has to be re-tested here even though
        //  everything above this line is type-agnostic. All three arms are live:
        //  MsgStck::Push() pushes a list and a vector as well as a field, so
        //  "List^" reaches a real snapshot with its elements in it.
        P3PmsgField& oStacked = pObject->IsList() ? (P3PmsgField&)oField.r_Stck().r_list()
                              : pObject->IsVect() ? (P3PmsgField&)oField.r_Stck().r_vect()
                              :                     (P3PmsgField&)oField.r_Stck().r_item();
        if ( *++lpszObjectPath == 0 )
          return oStacked.r_Object();  // Path ends on the pushed item itself
        return P3Pmsg_SelectObjectRecurse ( &oStacked.r_Object(), lpszObjectPath );
      }
      //if ( oField.r_name().c_wcsicmp(++lpszObjectPath) == 0 )
      //  return *pObject;
      if ( !oField.r_Desc().r_Curs().Goto(nsObjectname) )
      { // Check if renamining name has dot operator "*.pys" example
        if ( oField.r_Desc().r_Curs().Goto(lpszObjectPath) )
          return oField.r_Desc().r_Curs().r_Object();
        return P3PmsgObject();         // Selection path broken;
      }
      if ( *lpszParsedname == 0 )
        return oField.r_Desc().r_Curs().r_Object();
      return P3Pmsg_SelectObjectRecurse ( &oField.r_Desc().r_Curs().r_Object(), lpszParsedname );
    }

    // Attributes
    if ( pObject->IsAttr() )
    {
      P3PmsgAttr oAttr = *pObject;
      //  A COLLECTION HAS NO DESCENDANTS OF ITS OWN, for the same reason the
      //  '@' arm below says it has no attributes: it is not an item. So
      //  "Item@." names nothing -- and it is a well-formed question, reachable
      //  since §15 made a trailing '.' a component, so the answer is the empty
      //  object rather than a debug-build event.
      if ( *lpszObjectPath == T_DescDelim )
        return P3PmsgObject();         // Selection path broken
      //  A COLLECTION HAS NO ATTRIBUTES OF ITS OWN. It is not an item: a
      //  VBLockAttr carries aParent and its members, and nothing else. So
      //  "Item@@Tag" names nothing -- but it is a well-formed question, and
      //  the answer to one of those is the empty object, not an assertion.
      //  Same reasoning as the never-created collection above: what a caller
      //  spells is the caller's business, and only what the library itself
      //  could not have meant is a debug-build event.
      //
      //  Reachable before any of this: "Item@^@Tag" splits cleanly -- "@^" is
      //  the collection at the last push (§9) -- and arrived here to assert
      //  and then return exactly this. §14 legalises the shorter spelling,
      //  which is what made it worth writing down.
      if ( *lpszObjectPath == T_AttrDelim )
        return P3PmsgObject();         // Selection path broken
      //  '^' ON THE COLLECTION ITSELF. An attribute collection has no stack of
      //  its own -- aStack is a VBLockItem field and this block is a
      //  VBLockAttr -- which is why this used to answer "broken path". It is
      //  not the only thing a VBLockAttr carries, though: it carries aParent,
      //  so the item that OWNS the collection can be found, and that item has
      //  a stack, and its snapshot holds a copy of the whole collection --
      //  Push copies name, data, attributes and descendants.
      //
      //  So "Item@^" is the attribute collection as it stood at the last push,
      //  which IS the attribute collection inside the snapshot: "Item@^" and
      //  "Item^@" name the same object. '^' commutes with '@' and with '.',
      //  and it does so for the reason §3 already gives about pushed items --
      //  a snapshot is a whole item, not a fragment of one.
      //
      //  NOTHING IS ADDED TO A VBLockAttr, so this costs no format change and
      //  no image moves. aParent is already there, already maintained
      //  (P3Pmsg_GetPath walks it to build a path), and P3PmsgObject::GetParent
      //  already reads it. Measured: a pushed item's collections point at the
      //  PUSHED item, not back at the live one, so "Item@^^" descends a
      //  generation exactly as "Item^^" does rather than looping.
      //
      //  Note "Item@Attr^" is a different path and always worked: the Goto
      //  below lands on the attribute ITEM, which has a stack of its own, and
      //  the field arm above follows it.
      if ( *lpszObjectPath == T_StckDelim )
      {
        P3PmsgObject oOwner = pObject->GetParent();
        if ( oOwner.IsVoid() )
          return P3PmsgObject();       // Orphaned collection; path broken
        //  Copy-INITIALISED, not assigned: the converting constructor takes a
        //  list and a vect as well as a field, where operator= would throw
        //  "Invalid overloaded context". The field arm above relies on the
        //  same distinction, and §6 is why the owner can be any of the three.
        P3PmsgField oOwnerField = oOwner;
        if ( !oOwnerField.IsStacked() )
          return P3PmsgObject();       // Nothing pushed; path broken
        P3PmsgField& oStacked = oOwner.IsList() ? (P3PmsgField&)oOwnerField.r_Stck().r_list()
                              : oOwner.IsVect() ? (P3PmsgField&)oOwnerField.r_Stck().r_vect()
                              :                   (P3PmsgField&)oOwnerField.r_Stck().r_item();
        P3PmsgObject oStackedAttr = oStacked.r_Attr().r_Object();
        //  A path that ends on a '^' answers the thing the '^' named, which is
        //  the rule the field arm follows too. ("Item@" on its own is a
        //  different case and still answers nothing: it ends on a '@' with an
        //  empty name, and the Goto below is what refuses it.)
        if ( *++lpszObjectPath == 0 )
          return oStackedAttr;
        return P3Pmsg_SelectObjectRecurse ( &oStackedAttr, lpszObjectPath );
      }
      if ( !oAttr.r_Curs().Goto(nsObjectname) )
        return P3PmsgObject();         // Selection path broken;
      if ( *lpszParsedname == 0 )
        return oAttr.r_Curs().r_Object();
      return P3Pmsg_SelectObjectRecurse ( &oAttr.r_Curs().r_Object(), lpszParsedname );
    }

    // Descendants
    if ( pObject->IsDesc() )
    {
      P3PmsgDesc oDesc = *pObject;
      //  T_DescDelim, not T_StckDelim. This is the descendant arm: it steps
      //  over the delimiter and re-enters on the same collection, which is what
      //  the field arm above -- and the node arm above that -- both do for '.'.
      //  It was testing the STACK constant, so "Desc^name" descended and
      //  "Desc.name" fell through to the Goto below carrying the empty name
      //  ParseObjectPath had stopped at, and matched nothing. With '^' now
      //  meaning the stack everywhere else, this could not be left reading the
      //  same character.
      if ( *lpszObjectPath == T_DescDelim )
      {
        return P3Pmsg_SelectObjectRecurse ( &oDesc.r_Object(), ++lpszObjectPath );
        //lpszObjectPath = ParseObjectPath ( ++lpszObjectPath, strObjectname );
        //if ( oNode.r_name().c_wcsicmp(strObjectname) )
        //  return P3PmsgObject();         // Selection path broken
        //if ( *lpszObjectPath == NULL )
        //  return *pObject;
        //return P3Pmsg_SelectObject ( pObject, lpszObjectPath );
      }
      //  A COLLECTION HAS NO ATTRIBUTES OF ITS OWN -- the mirror of the '.'
      //  arm above, and the same reason: it is not an item. "Item.@" is a
      //  well-formed question, reachable since §15 made a bare '.' a
      //  component, and the answer is the empty object.
      if ( *lpszObjectPath == T_AttrDelim )
        return P3PmsgObject();         // Selection path broken
      //  As for attributes, and for the same reasons -- read that arm for the
      //  whole of it. A VBLockDesc has no aStack either, and carries the same
      //  aParent, so a descendant collection's '^' is the descendant collection
      //  inside the owner's snapshot.
      //
      //  This arm is reached by handing a descendant collection to
      //  P3Pmsg_SelectObject directly -- NOT by P3PmsgDesc::SelectObject,
      //  which despite the name never parses a path at all: it is a cursor
      //  Goto by plain name (and is defined twice, identically, in MsgDesc.cpp
      //  and P2Pmsg.cpp). A path that goes THROUGH an item does not come here
      //  either: the field arm's '.' re-enters on the item rather than on its
      //  collection, so "Item.^" is the item's own stack -- the same object,
      //  by the commuting rule above.
      if ( *lpszObjectPath == T_StckDelim )
      {
        P3PmsgObject oOwner = pObject->GetParent();
        if ( oOwner.IsVoid() )
          return P3PmsgObject();       // Orphaned collection; path broken
        P3PmsgField oOwnerField = oOwner;
        if ( !oOwnerField.IsStacked() )
          return P3PmsgObject();       // Nothing pushed; path broken
        P3PmsgField& oStacked = oOwner.IsList() ? (P3PmsgField&)oOwnerField.r_Stck().r_list()
                              : oOwner.IsVect() ? (P3PmsgField&)oOwnerField.r_Stck().r_vect()
                              :                   (P3PmsgField&)oOwnerField.r_Stck().r_item();
        P3PmsgObject oStackedDesc = oStacked.r_Desc().r_Object();
        if ( *++lpszObjectPath == 0 )
          return oStackedDesc;
        return P3Pmsg_SelectObjectRecurse ( &oStackedDesc, lpszObjectPath );
      }
      if ( !oDesc.r_Curs().Goto(nsObjectname) )
        return P3PmsgObject();         // Selection path broken;
      if ( *lpszParsedname == 0 )
        return oDesc.r_Curs().r_Object();
      return P3Pmsg_SelectObjectRecurse ( &oDesc.r_Curs().r_Object(), lpszParsedname );
    }

    // Root
    if ( pObject->IsRoot() )
    {
      ASSERT(0);
    }

    // Tidy up, and
    ASSERT(0);
    return P3PmsgObject();
}
P3PmsgObject
P3Pmsg_SelectObject ( const P3PmsgObject *pObject, LPCTNAM lpszObjectPath )
{
    //  A LEADING '.' means "this component names the object you are standing
    //  on", and the name after it is matched against that object's own. A path
    //  that is nothing BUT a '.' carries no such name: it is the whole
    //  instruction, naming the descendant collection (§15). Sent through the
    //  matching below it was compared against the item's real name, missed,
    //  and came back void.
    //
    //  AND THE SAME IS TRUE OF ANY '.' WITH NO NAME AFTER IT, not only one at
    //  the end of the path. ".^" is a bare '.' and then a '^'; the test here
    //  was "is the path longer than one character", so ".^" took the matching
    //  route, ParseObjectPath stopped on the '^' with an empty name, and the
    //  empty name was compared against the item's real one. Void, for the one
    //  spelling §9's commuting rule most obviously predicts (§17).
    //
    //  P3Pmsg_IsPathDelimiter answers TRUE for the terminator as well as for
    //  the five delimiters, so it is both halves of that question at once.
    if ( *lpszObjectPath != T_DescDelim ||
         P3Pmsg_IsPathDelimiter ( lpszObjectPath + 1 ) )
      return P3Pmsg_SelectObjectRecurse ( pObject, lpszObjectPath );
    //  The WHOLE path is kept. A leading '.' that turns out not to be the
    //  root marker is an ordinary descendant delimiter, and the arms below
    //  have to be able to hand it on with the delimiter still attached.
    LPCTNAM lpszWhole = lpszObjectPath;
    TNAME   nsObjectname[MAX_TNAME_SIZE] = {0};
    lpszObjectPath = ParseObjectPath ( lpszObjectPath + 1, nsObjectname, ARRAYSIZE(nsObjectname) );

    //  THE COLLECTIONS ARE ASKED ABOUT FIRST, for the reason §16 gives: IsAttr
    //  and IsDesc read the block header, which every block has, while IsField,
    //  IsList and IsVect read a VBLockItem's fields out of whatever block is
    //  there. Asked in this order the question never reaches a block that
    //  cannot answer it. The two arms used to stand after the item one and
    //  were reached only because a collection's ut union happened not to look
    //  like a field.
    //
    //  A leading '.' on a COLLECTION was never the root marker: both arms
    //  descend by name, which is what Goto and Exists do. Only the empty
    //  remainder below is new.
    if ( pObject->IsDesc() )
    {
      P3PmsgDesc oDesc = *pObject;
      if ( !oDesc.r_Curs().Goto(nsObjectname) )
        return P3PmsgObject();         // Selection path broken
      if ( *lpszObjectPath == 0 )
        return oDesc.r_Curs().r_Object();
      return P3Pmsg_SelectObjectRecurse ( &oDesc.r_Curs().r_Object(), lpszObjectPath );
    }
    if ( pObject->IsAttr() )
    {
      P3PmsgAttr oAttr = *pObject;
      //  Exists() IS the Goto -- it positions m_pCurs and answers whether it
      //  landed -- so r_Curs() below is the object it found.
      if ( !oAttr.Exists(nsObjectname) )
        return P3PmsgObject();         // Selection path broken
      if ( *lpszObjectPath == 0 )
        return oAttr.r_Curs().r_Object();
      return P3Pmsg_SelectObjectRecurse ( &oAttr.r_Curs().r_Object(), lpszObjectPath );
    }
    //if ( pObject->IsNode() )
    //{
    //  P3PmsgNode oNode = *pObject;
    //  P3PmsgField oField = *pObject;
    //  if ( oField.r_name().c_wcsicmp(nsObjectname) )
    //    return P3PmsgObject();         // Selection path broken
    //}
    //  Lists and vectors take the field test here for the same reason they
    //  share the field arm of the recurse above: the name lives in the
    //  VBLockField the ut union carries whichever of the three it is, so
    //  matching the leading component against it is the same operation. They
    //  used to reach the ASSERT(0) below and then recurse anyway, which meant
    //  a rooted path was never checked against the object it was rooted at.
    if ( pObject->IsField() || pObject->IsList() || pObject->IsVect() )
    {
      //  AND ONLY WHERE A PATH COULD BE ROOTED. P3Pmsg_GetPath says what that
      //  means in its own first comment -- "Process root. NOTES: Defined by
      //  absence of parent" -- and emits ".item1.item2" for a root object and
      //  ".item" for a floating one. Nothing else is where a rooted path
      //  starts, so a leading '.' on an object that HAS a parent cannot be the
      //  root marker and is an ordinary descendant delimiter.
      //
      //  Asserted at every depth instead, it made ".Last" on an item mean "are
      //  you called Last" where the same characters descend in a root path:
      //  ".Store.BHP.Last" resolves, P3Pmsg_SelectObject(&oBHP, L".Last") was
      //  void. That was the last row of §17's agreement sweep still reading NO.
      //
      //  What goes with it: ".BHP.Last" asked OF BHP used to resolve, by
      //  matching BHP's own name and then descending. It is the assertion
      //  reaching where no path is rooted, and it has no caller here. The root
      //  keeps it, which is what P2PmsgMgr::Path2Object relies on to refuse a
      //  path rooted somewhere else.
      if ( pObject->HasParent() )
        return P3Pmsg_SelectObjectRecurse ( pObject, lpszWhole );

      P3PmsgField oField = *pObject;
      if ( oField.r_name().c_wcsicmp(nsObjectname) )
        return P3PmsgObject();         // Selection path broken
      //  AND A MATCH WITH NOTHING AFTER IT IS THE ANSWER. It used to fall
      //  through to the recurse below carrying an empty path, where
      //  ParseObjectPath produced an empty name and the Goto for it matched
      //  nothing. So the library could not resolve the path it emits for a
      //  root: P3Pmsg_GetPath(&mgr) is ".Store", and handing ".Store" back to
      //  the root answered void. Same for a floating item and its ".Floater",
      //  and for either collection reached by a name with nothing after it.
      if ( *lpszObjectPath == 0 )
        return *pObject;
      return P3Pmsg_SelectObjectRecurse ( pObject, lpszObjectPath );
    }
    ASSERT(0);
    return P3Pmsg_SelectObjectRecurse ( pObject, lpszObjectPath );
}

P3PmsgObject
P3Pmsg_GetRoot ( const P3PmsgItem *pItem )
{
    P3PmsgItem oParent = pItem -> r_Object();
    while ( !oParent.r_Object().IsRoot() )
      oParent = oParent.r_Object().GetParentItem();
    return oParent.r_Object();
}

//
//  Checks validity of passed item name
//
//  Parameters:  LPCWSTR lpszItemname
//               Item name to be checked
//
//               wchar_t *pwchar = 0
//               Character upon which check failed
//
//  Returns:     wchar_t
//               Result
//                 TRUE... Valid
//                 FALSE.. Invalid
Msgcore_EXT BOOL
P3Pmsg_IsValidItemname ( LPCWSTR lpszItemname, wchar_t *pwchar )
{
    if ( lpszItemname == nullptr )
      return FALSE;
    int nCount = 0;
    while ( *lpszItemname && nCount++ < 127 )
    {
      wchar_t wcNext = *lpszItemname++;
      if ( wcNext == L'<'  ||
           wcNext == L'>'  ||
           wcNext == L'"'  ||
           wcNext == L'/'  ||
           wcNext == L'\\' ||
           wcNext == L'|'  ||
           wcNext == L'?'  ||
           wcNext == L'*'  ||
           wcNext == L':'  ||
           wcNext == T_RootDelim ||
           wcNext == T_DescDelim ||
           wcNext == T_AttrDelim ||
           wcNext == T_StckDelim    ) {
        if ( pwchar )
          *pwchar = wcNext;
        return FALSE;
      }
    }
    //  The loop examines at most 127 characters, and this line is what decides
    //  whether it stopped because the name ended or because it ran out of budget.
    //  It could not do that as written: `wcNext` was declared TWICE -- once before
    //  the loop and again inside it -- and this test read the outer one, which was
    //  initialised to 0 and never assigned. So it returned TRUE for every name that
    //  contained no forbidden character, however long, and the 127 cap has never
    //  rejected anything. Found by C4456 when item 10 put /W4 on all eight
    //  configurations; see the release-readiness register, Stage D.
    //
    //  Reading the surviving character would be equally wrong in the other
    //  direction -- the loop condition guarantees it is non-zero whenever the loop
    //  ran, so every non-empty name would become invalid. What the cap actually
    //  means is "nothing may be left unexamined", which is this:
    return *lpszItemname ? FALSE : TRUE;
}

//
//  Checks for P3PmsgObject path delimiter
//  NOTES: Moving forwards along path valid delimiters include
//           '\' or, '/' or '.' descendant branch
//           '@' attribute branch
//
//  Parameters:  LPCWSTR lpszObjectPath
//               P3PmsgObject path whose leading character is to checked
//
//  Returns:     BOOL
//                 TRUE.. Path delimiter
//
Msgcore_EXT BOOL
P3Pmsg_IsPathDelimiter ( LPCWSTR lpszObjectPath )
{
   if ( lpszObjectPath == nullptr )
     return FALSE;
   if ( lpszObjectPath[0] == L'.' )
     return TRUE;
   if ( lpszObjectPath[0] == L'@' )
     return TRUE;
   if ( lpszObjectPath[0] == L'\\' )
     return TRUE;
   if ( lpszObjectPath[0] == L'/' )
     return TRUE;
   if ( lpszObjectPath[0] == L'^' )
     return TRUE;
   if ( lpszObjectPath[0] == L'\0' )
     return TRUE;
   return FALSE;
}

//  A DELIMITER WITH NO NAME AFTER IT NAMES THE COLLECTION IT INTRODUCES.
//  That is the whole rule, and every delimiter obeys it: '^' names the pushed
//  value of whatever is to its left, '@' the attribute collection, '.' -- and
//  its aliases '\' and '/' -- the descendant one. Each of those is an object
//  P3Pmsg_GetPath emits a path for and nothing else can spell.
//
//  It arrived a delimiter at a time: '^' with §10, '@' with §12 and then §14,
//  '.' with §15. Each step was argued from the one before it, and stating the
//  rule once is what the last of them is really for.
//
//  A component is seeded with the delimiter that introduces it, so a length of
//  one means exactly "delimiter, no name" -- there is no other way to get here
//  with a single character.
static bool
P3Pmsg__IsBareComponent ( const CString& strItemname )
{
    return strItemname.GetLength() == 1 &&
           ( strItemname[0] == T_StckDelim || strItemname[0] == T_AttrDelim ||
             strItemname[0] == T_DescDelim || strItemname[0] == T_BackSlash ||
             strItemname[0] == T_ForeSlash    );
}

//
//  Splits passed full P3PmsgObject path into its functional components
//  NOTES: Full path format Rootname[/|\|@|^]Componentname[/|\|@|^]etc
//
//  Parameters:  LPCWSTR lpszObjectPath
//               Full P3PmsgObject path
//
//               CString& strRootname
//               Parsed root name
//
//               CList<CString>& oCListItems
//               Parsed list of component names
//
//  Returns:     BOOL
//               Success code
//                 TRUE... OK
Msgcore_EXT BOOL
P3Pmsg_SplitRootPath ( LPCWSTR lpszObjectPath, CString& strRootname
                     , CList<CString>& oCListItems )
{
    LPCWSTR lpszWorkingPath = lpszObjectPath;
    // Parse out the root name
    strRootname.Empty();
    if ( *lpszWorkingPath++ != L'.' )
      return FALSE;
    while ( !P3Pmsg_IsPathDelimiter(lpszWorkingPath) )
      strRootname += *lpszWorkingPath++;

    // Transition
    oCListItems.RemoveAll();
    CString strItemname;
    if ( !isprint(lpszWorkingPath[0]) )
      return TRUE;
    strItemname = *lpszWorkingPath++;

    // Parse out path components
    // NOTES: Always preceeded with [/|\|@|^]
    while ( lpszWorkingPath[0] )
    {
      //  '^' is a delimiter, and it is the only one that introduces no NAME --
      //  it names the pushed value of whatever stands to its left. So where it
      //  follows a delimiter that DOES introduce one, it has not begun a new
      //  component; it has qualified the component being read. "@^Currency" is
      //  a single question -- the attribute Currency as it stood before the
      //  last push -- and P3Pmsg_SelectObject answers it as one. Split at the
      //  '^' it became a component "@" carrying no name at all, the length test
      //  below rejected that, and the WHOLE path came back FALSE.
      //
      //  So the scan takes a '^' while the component is still nothing but
      //  delimiters, and stops at one once a name has been read: "@^^Tag" is
      //  one component, "@Tag^" is two. Pushes nest, hence the repetition.
      bool bNamed = false;
      while ( !P3Pmsg_IsPathDelimiter(lpszWorkingPath) ||
              ( lpszWorkingPath[0] == T_StckDelim && !bNamed ) )
      {
        bNamed = bNamed || lpszWorkingPath[0] != T_StckDelim;
        strItemname += *lpszWorkingPath++;
      }
      //  EVERY lone delimiter is now a component -- refer
      //  P3Pmsg__IsBareComponent -- so this refusal has nothing left to refuse
      //  and is gone. A component is seeded with its own delimiter and the
      //  scan below stops at the next one, so a single character here is
      //  always a delimiter and always names a collection.
      //
      //  What the splitter still refuses is a path that does not begin at a
      //  root, which is the test at the top of this function and the one §10
      //  taught RootPath2Object to honour. An empty NAME is no longer an error
      //  because there is no longer such a thing: ".Root..Alpha" is the root's
      //  descendant collection and then Alpha, which is Alpha -- a redundant
      //  spelling, the way ".Root.Item@.Tag" is a redundant spelling of
      //  "@Tag" (§14), and redundant is not malformed.
//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
if(strItemname.CompareNoCase(L".pya")==0||
   strItemname.CompareNoCase(L".pys")==0||
   strItemname.CompareNoCase(L".pyw")==0  ){
CString strLast=oCListItems.RemoveTail();
strLast += strItemname;
strItemname=strLast;
}
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
      oCListItems.AddTail ( strItemname );
      strItemname.Empty();
      if ( lpszWorkingPath[0] )
        strItemname = *lpszWorkingPath++;
    }
    //  The loop takes a trailing delimiter into strItemname and then exits on
    //  the terminator without adding it. For '.' that discarded nothing -- a
    //  trailing '.' is an empty name, refused above wherever it is not last --
    //  but for '^' it discarded the whole request: ".Root.Item^" asks for the
    //  value Item held before its last push, and it came back as the component
    //  list for ".Root.Item", which is Item itself. A wrong object, silently,
    //  and the shortest way to spell the question.
    //
    //  A trailing '@' reads exactly the same way and was discarded for exactly
    //  as long: ".Root.Item@" asks for Item's ATTRIBUTE COLLECTION, and came
    //  back as Item. The reason given for dropping it was that P3Pmsg_GetPath
    //  emits one and the round-trip had to survive -- but the only overload
    //  that emits a trailing '@' is the one for a collection, which had no
    //  caller and crashed before it got there (§12). A path to an attribute,
    //  "@Currency", has a name after the delimiter and never came through
    //  here. So the drop was protecting a round-trip that could not happen,
    //  and keeping the component is what makes one.
    if ( P3Pmsg__IsBareComponent(strItemname) )
      oCListItems.AddTail ( strItemname );
    return TRUE;
}
//
//  Formats diagnostic message
//
//  Parameters:  FILE *fd
//               Diagnostic message destination
// 
//               LPCTSTR lpszFormat
//               Format specification
//
//               ...
//               Variable argument list associated with above
//
int
P3Pmsg_fwprintf ( FILE *fd, LPCTSTR lpszFormat, ... )
{
    // Encode
    va_list  ap;                       // Variable argument list.
    TCHAR    szDebugMsg[2048];
    int      cSize = 0;

    // Encode message
    va_start ( ap, lpszFormat );
    cSize = vswprintf_s ( szDebugMsg, ARRAYSIZE(szDebugMsg), lpszFormat, ap );
    va_end   ( ap );
    cSize = min ( cSize, ARRAYSIZE(szDebugMsg)-1 );
    szDebugMsg[cSize] = 0;
    CString  strDebugMsg  = szDebugMsg;
ASSERT(cSize<2048);
//ASSERT(AfxCheckMemory());

    // Dump message to debug console
    // NOTES: Delegate through to standard library and consequently the python
    //        debug console
    //      : Wrap in try{} catch{} and avoid debug message trashing sequences
    //      : The text is always passed as an ARGUMENT, never as the format
    //        itself - a dumped payload containing a '%' would otherwise be
    //        read as a conversion specifier and consume a non-existent
    //        variadic argument.
#if defined(_WIN32)
    // Emit wide on Windows.  Narrow output on a stream the application has put
    // into _O_U16TEXT trips a CRT assertion (corecrt_internal_stdio.h), which
    // for an app with a report hook installed takes the whole process down -
    // so any console app doing the usual _setmode(_fileno(stdout),_O_U16TEXT)
    // to get wide console output could not survive a Print() diagnostic.
    // fwprintf is correct against both stream modes on the MSVC CRT.
    // _O_U16TEXT is a Windows-only concept, so the narrow path below is left
    // exactly as it was rather than risk glibc's stream-orientation lock,
    // where a wide write would poison the stream for the application's own
    // narrow output.
    try { cSize = fwprintf ( fd, L"%s", (LPCWSTR)strDebugMsg ); }
    catch ( ... ) { cSize = fwprintf ( fd, L"%s", L"P3Pmsg_Print() message exception\n\r" ); }
#else
    try { cSize = fprintf ( fd, "%s", (LPCSTR)CStringA(strDebugMsg) ); }
    catch ( ... ) { cSize = fprintf ( fd, "%s", "P3Pmsg_Print() message exception\n\r" ); }
#endif
    return cSize;
}

///////////////////////////////////////////////////////////////////////////////
//
//  Private and priviledged files
//  NOTES: Project dependency is encapsulated within the P2Pmsg.obj.  As such
//         existence of these files not publically exposed.  Shipped as part
//         of the P2Pmsg.obj
//       : Cross dependencies make inclusion order important
//#include "Private/P2PmsgVBLock.cpp"
//#include "Private/P2PmsgVBList.cpp"

//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
//  P2Pmsg tagging helpers

Msgcore_EXT INT64
P3Pmsg_GetTStamp ( const P3PmsgItem& oItem )
{
    INT64 tsValue = 0;
    if ( oItem.ATTR.Exists(L"$TStamp$") )
      return oItem.ATTR.SelectItem(L"$TStamp$").c_int64();  // must match the key P3Pmsg_SetTStamp declares
    return 0;
}
Msgcore_EXT INT64
P3Pmsg_SetTStamp ( const P3PmsgItem& oItem, INT64 tsValue, BOOL bRecurse )
{
    if ( tsValue == -1 ) 
      time(&tsValue);   // Get the current time from the 
    oItem.ATTR.DeclareItem ( L"$TStamp$", tsValue, TRUE );
    return tsValue;
}


///////////////////////////////////////////////////////////////////////////
//  P2Pmsg refactoring helpers

//
//  Deletes item(s) if they exist.
//
//  Parameters:  P3PmsgItem& oItem or P3PmsgAttr& oAttr
//               Item from which descendants are to be deleted
//
//               LPCSTR lpszItemName
//               Name of descendant(s) to be deleted
Msgcore_EXT void
P3PmsgRefactor_Delete ( P3PmsgField& oItem, LPCTSTR lpszItemName )
{
    while ( oItem.Exists(lpszItemName) )
      oItem.Delete ( lpszItemName );
}
Msgcore_EXT void
P3PmsgRefactor_Delete ( P3PmsgAttr& oAttr, LPCTSTR lpszItemName )
{
    while ( oAttr.Exists(lpszItemName) )
      oAttr.Delete ( lpszItemName );
}
Msgcore_EXT void
P3PmsgRefactor_DeleteFromParents ( const P3PmsgField& oItem, LPCTSTR lpszItemName )
{
    P3PmsgObject oParentObject = oItem.r_Object().GetParent();
    while ( !oParentObject.IsVoid() )
    {
      P3PmsgItem oItemParent = oParentObject;
      while ( oItemParent.Exists(lpszItemName) )
        oItemParent.Delete ( lpszItemName );
      if ( oParentObject.IsRoot() )
        break;
      oParentObject = oParentObject.GetParent();
    }
}

//
//  Deletes item(s) with qualification attributes if they exist.
//
//  Parameters:  P3PmsgItem& oItem or P3PmsgAttr& oAttr
//               Parent Item or Attr from which children are to be deleted
//
//               LPCTSTR lpszAttribute1
//               First attribute requirement, mandatory
//
//               LPCTSTR lpszAttribute2 = nullptr
//               Second attribute requirement, optional
//
//               LPCTSTR lpszAttribute3 = nullptr
//               Third attribute requirement, optional
//
//  Returns:     int
//               Item deleted count
// 
Msgcore_EXT int
P3PmsgRefactor_DeleteWithQualAttributes ( P3PmsgAttr& oAttr, LPCTSTR lpszAttribute1
                                   , LPCTSTR lpszAttribute2, LPCTSTR lpszAttribute3 )
{
    int        nItems = 0;
    P3PmsgCurs oCurs(oAttr);
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      LPCTSTR lpszName = oCurs.r_name().c_name();
      if ( !oCurs.IsItem()                       ||
           !oCurs.r_attr().Exists(lpszAttribute1)   )
        continue;
      if ( _tcslen(lpszAttribute2) > 0           &&
           !oCurs.r_attr().Exists(lpszAttribute2)   )
        continue;
      if (         lpszAttribute3                &&
           _tcslen(lpszAttribute3) > 0           &&
           !oCurs.r_attr().Exists(lpszAttribute3)   )
        continue;
      oCurs.Delete ( );
      nItems++;
    }
    return nItems;
}

//
//  Rename item(s) if they exist.
//
//  Parameters:  P3PmsgItem& oItem
//               Item from which descendants are to be renamed
//
//               LPCSTR lpszItemName
//               Name of descendant(s) to be renamed
//
//               LPCTSTR lpszItemNew
//               New descendant name
//
Msgcore_EXT void
P3PmsgRefactor_Rename ( P3PmsgField& oItem, LPCTSTR lpszItemName, LPCTSTR lpszItemNew )
{
    //  BOTH names are snapshotted, because this loop spends a ring slot per
    //  sibling: c_wcscmp() widens that sibling's stored name through
    //  p2p_wstr_from_store (Platform/p2pstr.h). A caller passing some other
    //  item's c_name() would have its key recycled after ~16 siblings, and the
    //  recycled slot compares EQUAL to the sibling that overwrote it -- so the
    //  wrong item matches. lpszItemNew is worse: it is consumed INSIDE the loop,
    //  after those comparisons, so a recycled slot renames items to the WRONG
    //  NAME. Note strItemName below was already a copy and the loop used the raw
    //  pointer anyway.
    //const p2p_wkey oItemName ( lpszItemName );
    //const p2p_wkey oItemNew  ( lpszItemNew  );
    CString strItemName = lpszItemName;
    CString strItemNew = lpszItemNew; 
    if ( strItemName.CompareNoCase(strItemNew) == 0 )
      return;
    P3PmsgCurs oCurs(oItem);
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.r_name().c_wcsicmp(strItemName) )
        continue;
      oCurs.r_name().c_name ( strItemNew );
    }
}

Msgcore_EXT void
P3PmsgRefactor_Rename ( P3PmsgAttr& oAttr, LPCTSTR lpszItemName, LPCTSTR lpszItemNew )
{
    //  Attribute twin of the descendant version above, with the same hazard in
    //  both arguments and the same reason: the scan spends a ring slot per
    //  sibling, and lpszItemNew is consumed inside the loop after them.
    //const p2p_wkey oItemName ( lpszItemName );
    //const p2p_wkey oItemNew  ( lpszItemNew  );
    CString strItemName = lpszItemName;
    CString strItemNew  = lpszItemNew;
    if ( strItemName.CompareNoCase(strItemNew) == 0 )
      return;
    P3PmsgCurs oCurs(oAttr);
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.r_name().c_wcsicmp(strItemName) )
        continue;
      oCurs.r_name().c_name ( strItemNew );
    }
}

//
//  Moves item(s) if they exist.
//
//  Parameters:  P3PmsgAttr& oAttrSource
//               Item source
//
//               P3PmsgField& oItemDestin
//               Item destination
//
//               LPCTSTR lpszItemName
//               Item name
//
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgAttr& oAttrSource, P3PmsgField& oItemDestin, LPCTSTR lpszItemName )
{ 
    while ( oAttrSource.Exists(lpszItemName) )
    {
      if ( !oItemDestin.Exists(lpszItemName) )
        oItemDestin += oAttrSource.SelectItem(lpszItemName);
      oAttrSource.Delete ( lpszItemName );
    }
}
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgField& oFieldSource, P3PmsgAttr& oAttrDestin, LPCTSTR lpszItemName )
{ 
    while ( oFieldSource.Exists(lpszItemName) )
    {
      if ( !oAttrDestin.Exists(lpszItemName) )
        oAttrDestin += oFieldSource.SelectItem(lpszItemName);
      oFieldSource.Delete ( lpszItemName );
    }
}
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgField& oFieldSource, P3PmsgField& oItemDestin, LPCTSTR lpszItemName )
{ 
    while ( oFieldSource.Exists(lpszItemName) )
    {
      if ( !oItemDestin.Exists(lpszItemName) )
        oItemDestin += oFieldSource.SelectItem(lpszItemName);
      oFieldSource.Delete ( lpszItemName );
    }
}
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgAttr& oAttrSource, P3PmsgAttr& oAttrDestin, LPCTSTR lpszItemName )
{ 
    while ( oAttrSource.Exists(lpszItemName) )
    {
      if ( !oAttrDestin.Exists(lpszItemName) )
        oAttrDestin += oAttrSource.SelectItem(lpszItemName);
      oAttrSource.Delete ( lpszItemName );
    }
}

//
//  Refactors contained data type
//
//  Parameters:  P3PmsgItem& oItemParent
//               Item source
//
//               LPCTSTR lpszItemname
//               Name of item whose data type is to be changed
//
//               const P3PmsgData& oData
//               Changed item data
//
Msgcore_EXT void
P3PmsgRefactor_DataType ( P3PmsgItem& oItemParent, LPCTSTR lpszItemname, const P3PmsgData& oData )
{ 
    if ( oItemParent.Exists(lpszItemname) )
    {
      P3PmsgItem oItem = oItemParent.SelectItem(lpszItemname).r_Object();
      if ( oItem.r_data().DataType() != oData.DataType() )
        oItem.r_data() = oData;
    }
}

//
//  Refactors contained data type
//
//  Parameters:  P3PmsgItem& oItem
//               P3PmsgAttr& oAttr
//               P3PmsgData& oData
//               Original data source
//
//               LPCTSTR lpszItemName
//               Name of item whose data type is to be changed
// 
//               char ucNewDataType
//               New data type to be set
//
Msgcore_EXT void
P3PmsgRefactor_CastDataType (P3PmsgItem& oItem, LPCTSTR lpszItemName, char ucNewDataType)
{
    if (oItem.Exists(lpszItemName))
    {
      P3PmsgData& oData = oItem.SelectItem(lpszItemName).r_data();
      P3PmsgRefactor_CastDataType(oData, ucNewDataType);
    }
}
Msgcore_EXT void
P3PmsgRefactor_CastDataType (P3PmsgAttr& oAttr, LPCTSTR lpszItemName, char ucNewDataType)
{
    if (oAttr.Exists(lpszItemName))
    {
      P3PmsgData& oData = oAttr.SelectItem(lpszItemName).r_data();
      P3PmsgRefactor_CastDataType(oData, ucNewDataType);
    }
}
Msgcore_EXT void
P3PmsgRefactor_CastDataType (P3PmsgData& oData, char ucNewDataType)
{
    if ( oData.DataType() == ucNewDataType )
      return;
    if ( ucNewDataType == VBLockData_INT32 )
    {
      if ( oData.DataType() == VBLockData_UINT32 ) {
        oData = P3PmsgData( (INT32)oData.c_uint() );
        return; 
      }
    }
    else if ( ucNewDataType == VBLockData_UINT32 )
    {
      if ( oData.DataType() == VBLockData_INT32 ) {
        oData = P3PmsgData( (UINT32)oData.c_int() );
        return; 
      }
    }
    ASSERT(0); //TODO:LJM Implement other data type conversions
}

/////////////////////////////////////////////////
//  P2PCheckPtrs diagnostic
//  NOTES: P2Pmsg uses C++ pointers to memory within allocated VBHeap
//         that are invalidate whenever the VBHeap is reallocated.
//       : Should heap be re-allocated whilst this object resides on the
//         stack and ASSERT(0) will be raised upon destruction

P2PmsgCheckPtrs::P2PmsgCheckPtrs ( const P3PmsgObject& oObject )
{
   m_hVBHeap = oObject.m_hVBList;
   m_nVBHeapAllocSeqnum = P2PmsgHeap_AllocSeqnum ( m_hVBHeap );
}
P2PmsgCheckPtrs::~P2PmsgCheckPtrs ( )
{
   ASSERT ( Check() );
}
BOOL
P2PmsgCheckPtrs::Check ( )
{
   return (m_nVBHeapAllocSeqnum==P2PmsgHeap_AllocSeqnum(m_hVBHeap)) ? TRUE : FALSE;
}
