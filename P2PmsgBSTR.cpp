// Copyright © 2005-2010, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2PmsgBSTR definitions and prototypes
//
#include "stdafx.h"
#include "P2PmsgBSTR.h"
#include "Msgexception.h"
#include "MsgVBHeap.h"
#include <ASSERT.h>

//////////////////////////////////////////////////////////////////////
//  Common P2PeerMsg internal names and labels
#define TBSTR_Net L"Net"
#define TBSTR_Sys L"Sys"
#define TBSTR_Evt L"Evt"
#define TBSTR_Wrp L"Wrp"
#define TBSTR_Msg L"Msg"
#define BSTR_INITIAL_SiZE 4048 // was 2048

//void
//VBLockBSTR_Init   ( VBLock *pVBLockBSTR, UCHAR uVBLock, int nSizenn );

//UINT
//VBLockBSTR_Sizeof ( VBLockBSTR *pBSTR, int nSizeof );

///////////////////////////////////////////////////////////////////////
//  Constructors and destructor

P3PmsgBSTR::P3PmsgBSTR ( )
          : m_oItem ( 0, 0, 0 )
{
    //RenderThisSafe ( );
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( GetP2Pmsgnn(), BSTR_INITIAL_SiZE, 0/*was2024*/ );
    Init ( L"BSTR", P3PmsgData(0u) );
    //m_oItem.Connect ( m_hBSTR, 0, 0 );  //TODO:P3Pobject upgrade
}

P3PmsgBSTR::P3PmsgBSTR ( LPCTSTR lpszName, const P3PmsgData& oData )
{
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( GetP2Pmsgnn(), BSTR_INITIAL_SiZE, 0/*was2024*/ );
    Init ( lpszName, oData );
}

P3PmsgBSTR::P3PmsgBSTR ( UCHAR uVBLockAddr, VBLsize nSizeof )
          : m_oItem ( 0, 0, 0 )
{
    //RenderThisSafe ( );
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( uVBLockAddr, nSizeof, 0/*was2024*/ );
    Init ( L"BSTR", P3PmsgData(0u) );
    //m_oItem.Connect ( m_hBSTR, 0, 0 );  //TODO:P3Pobject upgrade
}

P3PmsgBSTR::P3PmsgBSTR ( const P3PmsgBSTR& rhs )
          : m_oItem ( 0, 0, 0 )
{
    //RenderThisSafe ( );
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( rhs.GetP2Pmsgnn(), rhs.Sizeof()+BSTR_INITIAL_SiZE, 0 ); //TODO:LJM +2024 is a fudge
    Init ( rhs.m_oItem.c_name(), ((P3PmsgBSTR&)rhs).m_oItem.r_data() );
  (*this) = rhs;
}

P3PmsgBSTR::P3PmsgBSTR ( const VBListIOmage& oIOmage )
          : m_oItem ( 0, 0, 0 )
{
    //RenderThisSafe ( );
    //int           nIOmageSize = sizeof(VBListIOmage)
    //                          + oIOmage.oSync.uiSync1 & 0x00FFFFFF;
    VBListIOmage *pIOmage     = Msgiomage_Duplicate ( oIOmage );
    //memcpy ( pIOmage, &oIOmage, nIOmageSize );
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( pIOmage );
    //VBLaddr aVBLock = sizeof(pIOmage->oSync);
    //VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_hBSTR, aVBLock );
    m_oItem.Nullify ( );
    m_oItem.Connect ( m_hBSTR, P2PmsgHeap_Connect(m_hBSTR), 0 ); 
    //m_oItem.r_Desc().Print(stdout,1);
    //m_oItem.r_Attr().Print(stdout,1);
}

P3PmsgBSTR::P3PmsgBSTR ( const VBListIOmage& oIOmage, VBLsize nBufferLen )
          : m_oItem ( 0, 0, 0 )
{
    // Both halves of the adoption are length-checked, not just the copy. The
    // duplicate is bounded by nBufferLen; the heap it is then wrapped in is
    // bounded by the size the duplicate actually got, which is the declared
    // size the check above has already proved fits. Passing nBufferLen to the
    // second call instead would be looser than necessary and would let the two
    // checks disagree about how big the image is.
    VBListIOmage *pIOmage = Msgiomage_Duplicate ( oIOmage, nBufferLen );
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( pIOmage
                                      , pIOmage->oSync.uiSync1 & 0x00FFFFFF );
    m_oItem.Nullify ( );
    m_oItem.Connect ( m_hBSTR, P2PmsgHeap_Connect(m_hBSTR), 0 );
}

P3PmsgBSTR::P3PmsgBSTR ( VBListIOmage *pIOmage )
          : m_oItem ( 0, 0, 0 )
{
    //RenderThisSafe ( );
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( pIOmage );
    //VBLaddr aVBLock = sizeof(pIOmage->oSync);
    //VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_hBSTR, aVBLock );
    m_oItem.Connect ( m_hBSTR, P2PmsgHeap_Connect(m_hBSTR), 0 ); 
}
P3PmsgBSTR::P3PmsgBSTR ( VBListIOmage *pIOmage, VBLsize nBufferLen )
          : m_oItem ( 0, 0, 0 )
{
    //  The length-validated form of the constructor above, and the one the
    //  RECEIVE path uses. The difference is not the length check alone: the
    //  two-argument heap create runs the two block walks for real in every
    //  build and refuses on the answer, where the single-argument one runs them
    //  inside ASSERT and therefore not at all in a Release binary. See the
    //  untrusted-gate note in MsgVBHeap.h.
    //
    //  It ADOPTS pIOmage rather than duplicating it, exactly as the
    //  single-argument form does -- the caller's buffer becomes the heap's on
    //  success. On failure the create throws with the image detached, so the
    //  caller still owns it and must free it; that is the contract
    //  P2Peerio::RecvP2PeerMsg relies on.
    m_hBSTR = P2PmsgHeap_CreateIOMAGE ( pIOmage, nBufferLen );
    m_oItem.Connect ( m_hBSTR, P2PmsgHeap_Connect(m_hBSTR), 0 );
}
P3PmsgBSTR::P3PmsgBSTR ( P2PmsgHANDLE hBSTR )
{
    //RenderThisSafe ( );
    m_hBSTR = P2PmsgHeap_AddRef ( hBSTR );
    m_oItem.Connect ( m_hBSTR, 0, 0 );  //TODO:P3Pobject upgrade
    ASSERT(0);//Needs to connect underlying infrastruture
}

P3PmsgBSTR::~P3PmsgBSTR ( )
{
    ReleaseP2Piomage ( );
    if (  m_hBSTR )
      P2PmsgHeap_Close ( m_hBSTR );
    m_hBSTR = 0;
}

//void
//P3PmsgBSTR::RenderThisSafe ( )
//{
//    //m_hBSTR             = 0;
//    //m_pP2PmsgBSTRiomage = 0;
//}

void
P3PmsgBSTR::ResetThisObject ( )
{
}

///////////////////////////////////////////////////////////////////////
//  External initialisation

P3PmsgItem&
P3PmsgBSTR::Init ( LPCTNAM lpszMsgName, const P3PmsgData& oItemData )
{
    //UINT       nSizeofItem;
    P3PmsgItem oItem ( lpszMsgName, oItemData );
    UCHAR      uVBLaddrnn  = GetP2Pmsgnn();
    //VBLsize    nSizeof     = oField.Sizeof()
    //                       + VBLockItem_Sizeof(uVBLaddrnn);
    VBLsize    nSizeofItem = VBLockItem_Sizeof ( uVBLaddrnn )
                           + oItem.P3PmsgItem::Sizeof ( );//uVBLaddrnn );
    // TODO:LJM This block needs auditing
    // Older implementation
//ASSERT(AfxCheckMemory());
    //VBLaddr     aVBLock = VBListAlloc ( m_hBSTR, VBLock_Item, nSizeof ); 
    VBLaddr     aVBLock = P2PmsgHeap_Alloc ( m_hBSTR, VBLock_Item, nSizeofItem );   //Displaces above line
    VBLock     *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( m_hBSTR, aVBLock );
                pVBLock -> oHdr.uVBLockDefs |= VBLock_Linked;                  //Added later by LJM
    /*//VBLockNode_Init ( uVBLaddrnn, VBLock_pItem(pVBLock), VBLock_Node );    // Added later by LJM
    VBLockItem *pItem   = VBLock_pItem ( pVBLock );
    VBLockItem_Init ( uVBLaddrnn, pItem, 0xFF );
    VBLockName_Init ( VBLockItem_pName(uVBLaddrnn,pItem)
                    , VBLockAttr_DEFAULT | VBLockAttr_NULL
                    , lpszMsgName, oItem.P3PmsgName::Sizeof() );
    VBLockData_Init ( VBLockItem_pData(uVBLaddrnn,pItem)
                    , VBLockAttr_DEFAULT, oItemData.DataType()
                    , oItemData.Sizeof() );*/
    VBLockItem_Init ( uVBLaddrnn, VBLock_pItem(pVBLock), VBLock_Field );    // Added later by LJM
    VBLockField *pField = VBLock_pField ( pVBLock );
    VBLockField_Init ( pField, 0xFF ); //TODO: LJM node to field cutover 0xFF );
    VBLockName_Init  ( uVBLaddrnn,VBLockField_pName(pField)
                     , VBLockAttr_DEFAULT | VBLockAttr_NULL
                     , lpszMsgName, oItem.P3PmsgName::Sizeof() );
    VBLockData_Init  ( VBLockField_pData(uVBLaddrnn,pField)
                     , VBLockAttr_DEFAULT, oItemData.DataType()
                     , oItemData.Sizeof() );

    m_oItem.Connect ( m_hBSTR, aVBLock, VBLock_Hdr_u_SizeNN(pVBLock) );
    m_oItem.r_data() = oItemData;
//AssertValid();
    return m_oItem;
}
P3PmsgItem&
P3PmsgBSTR::InitData ( P3PmsgData& oDataHdr )
{
    m_oItem += P3PmsgItem ( L"Data", oDataHdr );
    return r_item(VBLockBSTR_MSG);
}
P3PmsgItem&
P3PmsgBSTR::InitItem ( UCHAR uVBLockBSTR, const P3PmsgData& oData )
{
    if ( Exists(uVBLockBSTR) )
      r_item(uVBLockBSTR).r_data() = oData;
    else if ( uVBLockBSTR == VBLockBSTR_NET )
      m_oItem += P3PmsgItem( TBSTR_Net, oData );
    else if ( uVBLockBSTR == VBLockBSTR_MSG )
      m_oItem += P3PmsgItem( TBSTR_Msg, oData );
    else if ( uVBLockBSTR == VBLockBSTR_WRP )
      m_oItem += P3PmsgItem( TBSTR_Wrp, oData );
    else if ( uVBLockBSTR == VBLockBSTR_EVT )
      m_oItem += P3PmsgItem( TBSTR_Evt, oData );
    ASSERT(r_item(uVBLockBSTR).VerifyContainment());
    return r_item ( uVBLockBSTR );
}

///////////////////////////////////////////////////////////////////////
//  Operators

P3PmsgBSTR&
P3PmsgBSTR::operator = ( const P3PmsgBSTR& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    ResetThisObject ( );

    // Assignment
    m_oItem = rhs.m_oItem;

    // Tidy up, and 
    return *this;
}

///////////////////////////////////////////////////////////////////////
//  Operations

///////////////////////////////////////////////////////////////////////
//  IOCP
//  NOTES: Manage contigious images for IOCP message exchanges

void*
P3PmsgBSTR::PrepareP2Piomage ( DWORD uBSTRmask )
{
    // Legacy preparation
    ASSERT(m_pP2PmsgBSTRiomage==0);
    ReleaseP2Piomage ( );

    // Optimised full replica
    UCHAR uMask = 0xFF;
    if ( (uBSTRmask&uMask) == uMask )
    {
      //VBListDefrag ( m_hBSTR );
      m_pP2PmsgBSTRiomage = this;
      return P2PmsgHeap_pIOmage ( m_pP2PmsgBSTRiomage->m_hBSTR );
    }

    // Partial copy as per passed enumeration mask
    m_pP2PmsgBSTRiomage = new P3PmsgBSTR ( GetP2Pmsgnn(), Sizeof() );
    m_pP2PmsgBSTRiomage -> Init ( m_oItem.c_name(), m_oItem.r_data() );
    for ( int e = 0; e < sizeof(uMask)*8; e++ )
    {
      UCHAR uVBLockBSTR_BIT = (UCHAR)(1 << e);
      if ( (uBSTRmask&uVBLockBSTR_BIT) == uVBLockBSTR_BIT &&
            Exists ( uVBLockBSTR_BIT )                       )
        m_pP2PmsgBSTRiomage->m_oItem += r_item ( uVBLockBSTR_BIT );
    }

    // Tidy up, and
    return P2PmsgHeap_pIOmage ( m_pP2PmsgBSTRiomage->m_hBSTR );
}

P2Piomage*
P3PmsgBSTR::P2Piomage ( )
{
    if ( m_pP2PmsgBSTRiomage == NULL )
      return 0;
    return P2PmsgHeap_pIOmage ( m_pP2PmsgBSTRiomage->m_hBSTR );
}

UINT
P3PmsgBSTR::P2PiomageSize ( )
{
    if ( !m_pP2PmsgBSTRiomage  )
      EVERR->MODULE
           ->Message("P2PmsgIOmage not prepared" )
           ->Group("P2P")->Throw();
    // ONE definition of an image's size, not two. This used to read
    //
    //     return (pIOmage->oSync.uiSync1 & 0x00FFFFFF) + sizeof(pIOmage->oSync);
    //
    // and the addition was a DOUBLE COUNT: the 24-bit oSync field is already the
    // whole image, header included. That is not an inference about the format, it
    // is how the rest of the tree uses the field -- P2Piomage_Sizeof() returns it
    // raw and P2Peerio hands that straight to Send() as the byte count; the
    // receive path allocates from it and reads until dwBytes >= it; and
    // IOmage_Sizeof() derives the PAYLOAD from it by SUBTRACTING the header.
    //
    // Every caller uses this as "how many bytes to copy out of P2Piomage()", so
    // all of them read sizeof(oSync) bytes past the image. It hid because
    // P2Piomage() points into the message's own VBHeap, which normally has slack
    // past the image -- the extra bytes were garbage, and harmless garbage. It
    // bites when the message came off the WIRE, because then the heap IS the tight
    // `new char[nSizeof+4]` receive buffer of P2Peerio::RecvP2PeerMsg.
    //
    // That is F-S5-1, and it was not a benign over-read: WrappedResponseFactory
    // copies these bytes into a P3PmsgData that becomes the P2Pmsg_Exception sent
    // BACK TO THE PEER. Adjacent heap, to a remote party, from a buffer whose size
    // the peer chose, on a path the peer triggers by sending something
    // undeliverable. ASan, on the routing hub's pump thread:
    //
    //     READ of size 2040 ... 0 bytes after 2036-byte region
    //     VBLockData_BlobCopy <- WrappedResponseFactory <- ExceptionFactory
    //                         <- P2PeerTarget::RouteP2PeerMsg
    //
    // 2036 is nSizeof+4 so the image is 2032, and 2040 is 2032 + sizeof(oSync).
    // Exactly one header, every time.
    //
    // Delegating rather than deleting the `+`: two expressions for one number are
    // what let these drift apart in the first place, and P2Piomage_Sizeof()
    // validates the sync pair on the way past, which this never did.
    return P2Piomage_Sizeof ( P2PmsgHeap_pIOmage ( m_pP2PmsgBSTRiomage->m_hBSTR ) );
}

void
P3PmsgBSTR::ReleaseP2Piomage ( )
{
    if ( m_pP2PmsgBSTRiomage         &&
         m_pP2PmsgBSTRiomage != this    )
      delete m_pP2PmsgBSTRiomage;
    m_pP2PmsgBSTRiomage = 0;
}

///////////////////////////////////////////////////////////////////////
//  Troubleshooting
void
P3PmsgBSTR::AssertValid ( ) const
{
    // Delegate
    m_oItem.AssertValid ( );
}
void
P3PmsgBSTR::Print ( FILE *fd, int nDepthOS, int nDepthOSinc )
{
    // Delegate
    m_oItem.Print ( fd, nDepthOS, nDepthOSinc );
}

///////////////////////////////////////////////////////////////////////
//  Properties

static P2Pmsgnn_t s_uP2Pmsgnn = VBLock_Addr16;
P2Pmsgnn_t
P3PmsgBSTR::GetP2Pmsgnn ( ) const
{
    if ( m_hBSTR )
      return P2PmsgHeap_Addrnn ( m_hBSTR );
    return s_uP2Pmsgnn;
}
void
P3PmsgBSTR::SetDefaultP2Pmsgnn ( P2Pmsgnn_t uP2Pmsgnn )
{
    s_uP2Pmsgnn = uP2Pmsgnn;
}
P2Pmsgnn_t
P3PmsgBSTR::GetDefaultP2Pmsgnn ( )
{
    return s_uP2Pmsgnn;
}

bool
P3PmsgBSTR::Exists ( UCHAR uBSTRmask )
{
    if ( (uBSTRmask&VBLockBSTR_NET) == VBLockBSTR_NET )
      return m_oItem.Exists(TBSTR_Net);
    if ( (uBSTRmask&VBLockBSTR_MSG) == VBLockBSTR_MSG )
      return m_oItem.Exists(TBSTR_Msg);
    if ( (uBSTRmask&VBLockBSTR_WRP) == VBLockBSTR_WRP )
      return m_oItem.Exists(TBSTR_Wrp);
    if ( (uBSTRmask&VBLockBSTR_EVT) == VBLockBSTR_EVT )
      return m_oItem.Exists(TBSTR_Evt);
    if ( (uBSTRmask&VBLockBSTR_SYS) == VBLockBSTR_SYS )
      return m_oItem.Exists(TBSTR_Sys);
    return false;
}

P3PmsgItem&
P3PmsgBSTR::r_item  ( UCHAR eVBLockBSTR, bool bCreate )
{
    if ( eVBLockBSTR == VBLockBSTR_ROOT )
      return m_oItem;
    if ( eVBLockBSTR == VBLockBSTR_NET )
      return m_oItem.SelectItem(TBSTR_Net);
    if ( eVBLockBSTR == VBLockBSTR_MSG )
    {
      if (    bCreate                 &&
           !m_oItem.Exists(TBSTR_Msg)    )
        m_oItem += P3PmsgItem ( TBSTR_Msg, P3PmsgData() );
      return m_oItem.SelectItem(TBSTR_Msg);
    }
    if ( eVBLockBSTR == VBLockBSTR_SYS )
    {
      if (    bCreate                 &&
           !m_oItem.Exists(TBSTR_Sys)    )
        m_oItem += P3PmsgItem ( TBSTR_Sys, P3PmsgData() );
      return m_oItem.SelectItem(TBSTR_Sys);
    }
    if ( eVBLockBSTR == VBLockBSTR_WRP )
      return m_oItem.SelectItem(TBSTR_Wrp);
    if ( eVBLockBSTR == VBLockBSTR_EVT )
      return m_oItem.SelectItem(TBSTR_Evt);
    return m_oItem.SelectItem(L"?");
}

P3PmsgItem&
P3PmsgBSTR::r_datn  ( )
{
    return m_oItem.SelectItem(TBSTR_Msg);
}

P3PmsgName&
P3PmsgBSTR::r_name  ( )
{
    return m_oItem;
}
P3PmsgData&
P3PmsgBSTR::r_data  ( )
{
    return m_oItem;
}

// Declared in P2PmsgBSTR.h beside Sizeof but never defined - any caller was
// an unresolved external at link time. The same one-line delegation to the
// heap that Sizeof and IsDirty are: the packed image this object wraps.
void*
P3PmsgBSTR::VBLockBSTR_vp ( )
{
    return P2PmsgHeap_pIOmage ( m_hBSTR );
}

VBLsize
P3PmsgBSTR::Sizeof ( ) const
{
    return P2PmsgHeap_Sizeof ( m_hBSTR );
}

// As VBLockBSTR_vp: declared beside Sizeof and never defined, any caller an
// unresolved external. This one delegates to BSTR_INITIAL_SiZE, the constant
// every constructor above already uses as the size a P3PmsgBSTR gets when
// none is given explicitly - wiring an existing policy rather than inventing
// one. (IsFragmented, declared next to it, was deleted rather than defined:
// unlike this and Sizeof there is no P2PmsgHeap primitive or existing
// constant behind it. P3PmsgVect::IsName's precedent, section 37.)
VBLsize
P3PmsgBSTR::SetDefaultSizeof ( )
{
    return BSTR_INITIAL_SiZE;
}

bool
P3PmsgBSTR::IsDirty ( )
{
    if ( !m_hBSTR )
      return false;
    return P2PmsgHeap_IsDirty ( m_hBSTR ) ? true : false;
}

///////////////////////////////////////////////////////////////////////
//  Helpers

//  Shared oSync gate for the P2Piomage size helpers.
//  NOTES: The complement relation these used to test on its own is endian-blind
//         (complement and byte swap commute), so a foreign-endian header passed
//         it and handed the caller a byte-swapped size. Classify instead, and
//         name the byte-order case explicitly (byte_order.md §4).
static void
P2Piomage_ValidateSync ( const P2Piomage *pP2Piomage )
{
    const int nForm = P2PmsgHeap_IOMAGEform ( pP2Piomage );
    if ( nForm == VBLockSync_Swapped )
      EVERR->MODULE
           ->Message ( "VBListIOmage byte-order mismatch - image written by a "
                       "peer of the opposite endianness" )
           ->Throw();
    //  Named BEFORE the catch-all, and the naming is the whole point of the
    //  fallback classifier: a layout this build does not implement is reported
    //  with its code rather than as "invalid", for EVERY future generation and
    //  not only for one somebody remembered to enumerate
    //  (TargetCore's versioning note, §6.1).
    if ( nForm == VBLockSync_Gen )
      EVERR->MODULE
           ->Message ( "VBListIOmage layout generation 0x%02X is not implemented "
                       "by this build (0x%02X)"
                     , (UINT)VBLock_SyncGenCode ( pP2Piomage->oSync.uiSync1 )
                     , (UINT)VBLock_SyncGenNow )
           ->Throw();
    if ( nForm != VBLockSync_Native && nForm != VBLockSync_Legacy )
      EVERR->MODULE
           ->Message ( "Invalid VBListIOmage" )
           ->Throw();

    //  Lower bound, applied only once the form is known good: a declared size
    //  has to at least cover the header it is declared in. Both writers in this
    //  file (MakeIOmage, P2Piomage_Alloc) emit sizeof(VBListIOmage)+nDataSize,
    //  so sizeof(VBListIOmage) is the true floor and an empty image (nDataSize
    //  == 0) still passes. Mirrors the gate P2PmsgHeap_CreateIOMAGE applies at
    //  MsgVBHeap.cpp, and closes two reads of this field that had no floor:
    //  IOmage_Sizeof's "- sizeof(P2Piomage) + 1" underflows a declared size of
    //  0..7 to ~4 GB, and Msgiomage_Duplicate trusts the same field outright as
    //  a memcpy length.
    const UINT32 nSizeof = pP2Piomage -> oSync.uiSync1 & 0x00FFFFFF;
    if ( nSizeof < sizeof(VBListIOmage) )
      EVERR->MODULE
           ->Message ( "VBListIOmage declared size (%u) is smaller than its own "
                       "header", nSizeof )
           ->Throw();
}

UINT32
P2Piomage_Sizeof ( const P2Piomage *pP2Piomage )
{
    P2Piomage_ValidateSync ( pP2Piomage );
    return pP2Piomage -> oSync.uiSync1 & 0x00FFFFFF;
}
UINT32
IOmage_Sizeof ( const P2Piomage *pP2Piomage )
{
    P2Piomage_ValidateSync ( pP2Piomage );
    return (pP2Piomage -> oSync.uiSync1 & 0x00FFFFFF) - sizeof(P2Piomage) + 1;
}

VBListIOmage*
MakeIOmage ( LPCSTR lpcData, UINT32 nDataSize )
{
    // Bound nDataSize so sizeof(VBListIOmage)+nDataSize can neither overflow
    // nor exceed the 24-bit oSync size field. Without this a near-UINT32_MAX
    // nDataSize wraps nSizeofIOmage to a tiny value, under-allocating the
    // buffer before the memcpy below overruns the heap.
    if ( nDataSize > 0x00FFFFFF - sizeof(VBListIOmage) )
      EVERR->MODULE
           ->Message ( "IOmage data size (%u) exceeds maximum", nDataSize )
           ->Throw();
    UINT nSizeofIOmage = sizeof(VBListIOmage) + nDataSize;
    VBListIOmage *pIOmage = (VBListIOmage *)new char[nSizeofIOmage+4];
    // Was: uiSync1 = nSizeofIOmage (addressing mode never set) followed by
    //      uiSync2 = ~uiSync2 - complementing uiSync2 against ITSELF, i.e.
    //      against uninitialised new char[] bytes - so every header this
    //      produced failed validation. Unreachable in practice: the only caller
    //      (P2Peerio::PKeyXChangeAck) sits behind ASSERT(0) with its dispatch
    //      commented out. Corrected here because this writer has to stamp the
    //      endian sentinel regardless (byte_order.md §4, §8).
    pIOmage->oSync.uiSync1 = VBLock_SyncMake ( nSizeofIOmage, VBLock_Addr32 );
    pIOmage->oSync.uiSync2 = ~pIOmage->oSync.uiSync1;
    if ( lpcData )
      memcpy ( &pIOmage->cIOmage, lpcData, nDataSize );
    return pIOmage;
}

P2Piomage*
P2Piomage_Alloc ( const void *pvData, UINT32 nDataSize )
{
    // Bound nDataSize so sizeof(VBListIOmage)+nDataSize can neither overflow
    // nor exceed the 24-bit oSync size field (see MakeIOmage). Guards the
    // memcpy below against an under-allocated buffer.
    if ( nDataSize > 0x00FFFFFF - sizeof(VBListIOmage) )
      EVERR->MODULE
           ->Message ( "IOmage data size (%u) exceeds maximum", nDataSize )
           ->Throw();
    UINT nSizeofIOmage = sizeof(VBListIOmage) + nDataSize;
    VBListIOmage *pIOmage = (VBListIOmage *)new char[nSizeofIOmage+4];
    // Size + addressing mode + endian sentinel in one stamp (byte_order.md §4).
    pIOmage -> oSync.uiSync1  =  VBLock_SyncMake ( nSizeofIOmage, VBLock_Addr32 );
    pIOmage -> oSync.uiSync2  = ~pIOmage -> oSync.uiSync1;
    if ( pvData )
      memcpy ( &pIOmage->cIOmage, pvData, nDataSize );
    ASSERT( (pIOmage->oSync.uiSync1+pIOmage->oSync.uiSync2) == ~0 );
    return pIOmage;
}

P2Piomage*
P2Piomage_Alloc ( const P3PmsgItem& oItem )
{
    P2Piomage *pP2Piomage = P2PmsgHeap_pIOmage ( oItem.r_Object().m_hVBList );
    if ( pP2Piomage == nullptr )
      return nullptr;
    return P2Piomage_Alloc(&pP2Piomage->cIOmage,IOmage_Sizeof(pP2Piomage) );
}

Msgcore_EXT P2Piomage*
Msgiomage_Duplicate ( const P2Piomage& oMsgiomage )
{
    //P2Piomage *pMsgiomage = P2Piomage_Alloc ( &oMsgiomage.cIOmage, P2Piomage_Sizeof(&oMsgiomage) );
    //return pMsgiomage;

    //  Validate the header BEFORE trusting the length that comes out of it.
    //  The sibling readers in this file (P2Piomage_Sizeof, IOmage_Sizeof) have
    //  gone through P2Piomage_ValidateSync since the endian sentinel landed;
    //  this one alone read the raw field and memcpy'd by it, which is finding
    //  M4 of the internal security review (that document is not published,
    //  because it enumerates findings that are still open; SECURITY.md carries
    //  the part of it that concerns a consumer of this library).
    //  The gate now rejects a garbage or foreign-endian
    //  header - the latter of which would otherwise yield a byte-SWAPPED length
    //  here, e.g. a declared 0x000010 read as 0x100000 - and enforces that the
    //  declared size covers the header.
    //
    //  What it still does NOT do, and cannot at this signature: check the
    //  declared size against the REAL extent of the source. A caller handing us
    //  a reference to a 4 KB image whose header declares 16 MB still over-reads
    //  it, because a reference carries no length. That is the residual on M4,
    //  and it is closed by the overload below rather than here - this signature
    //  cannot be made safe, only deprecated in favour of one that can. Do not
    //  read this gate as making the function safe against a hostile image; use
    //  Msgiomage_Duplicate(image,length) for anything you did not build.
    P2Piomage_ValidateSync ( &oMsgiomage );

    // Parsing
    //UINT08 uAddrType = (UINT08)(oMsgiomage.oSync.uiSync1/0x01000000);
    UINT   nSizeof   =          oMsgiomage.oSync.uiSync1&0x00FFFFFF;
    VBListIOmage *pMsgiomage = (VBListIOmage *)new char[nSizeof+4];
    memcpy ( pMsgiomage, &oMsgiomage, nSizeof );
    return pMsgiomage;
}

Msgcore_EXT P2Piomage*
Msgiomage_Duplicate ( const P2Piomage& oMsgiomage, VBLsize nBufferLen )
{
    //  The M4 residual, closed. Everything checkable from the header alone is
    //  checked by P2Piomage_ValidateSync in the delegate below; the one thing
    //  that is not - the declared size against the real extent of the source -
    //  needs a number only the caller has, so the fix is a signature and not an
    //  algorithm.
    //
    //  Worth stating why the byte-order design note's argument does not rescue
    //  the other overload. It reasons that an over-declared size is caught in
    //  practice by "the size bounds check", which is true of the two readers
    //  that reach P2PmsgHeap_CreateIOMAGEn - that one has nDeclared > nBufferLen.
    //  This reader reaches neither. It had no upper bound at all, which made it
    //  the weakest of the three even after M4's fix landed.
    if ( nBufferLen < sizeof(VBListIOmage) )
      EVERR->MODULE
           ->Message ( "IOmage buffer (%u) smaller than header", (UINT)nBufferLen )
           ->Throw();
    //  Classify first, so a foreign-endian image is named as such rather than
    //  compared by a byte-swapped size (byte_order.md §4).
    P2Piomage_ValidateSync ( &oMsgiomage );
    const UINT32 nDeclared = oMsgiomage.oSync.uiSync1 & 0x00FFFFFF;
    if ( nDeclared > nBufferLen )
      EVERR->MODULE
           ->Message ( "IOmage declared size (%u) exceeds buffer (%u)"
                     , nDeclared, (UINT)nBufferLen )
           ->Throw();
    return Msgiomage_Duplicate ( oMsgiomage );
}

Msgcore_EXT P2Piomage*
P2Piomage_Release ( P2Piomage *pP2Piomage )
{
    // delete [], not delete: P2Piomage_Alloc above builds the image with
    // `new char[nSizeofIOmage+4]`, and every other char-buffer release in the
    // library already spells the array form (MsgVBHeap.cpp:1125, :1249, :1947,
    // :3574 ...). This line was the only one that did not, which is what makes
    // it a typo rather than a considered choice. ASan calls it
    // `alloc-dealloc-mismatch (operator new [] vs operator delete)`; it fired
    // five times across the security label -- p2p_authpsk, p2p_authrelay,
    // p2p_replayguard, p2p_bigreport -- on the ordinary send and drop paths
    // (P2Peerio::Reset -> P2PeerCon::Drop), so it is reached by any connection
    // that closes, not by some exotic input. Found by TargetCore's production
    // plan, Stage 1 step 5 -- the first run of the security label under a
    // sanitiser. (Named rather than linked: that plan is in the sibling
    // TargetCore repository, so a path from here would not resolve.)
    if ( pP2Piomage )
      delete [] (char *)pP2Piomage;
    return nullptr;
}

