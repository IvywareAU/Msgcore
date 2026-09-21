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
//  P2Peer message definitions and prototypes
//
#include "stdafx.h"
#include "P2PmsgVBLock.h"
#include "Msgexception.h"
#include <Assert.h>

//  Sizing VBLockHdr's, eliminates need to contract empty objects
const VBLockHdr  soHdr  = { 0 };
const VBLockName soName = { 0 };
const VBLockData soData = { 0 };
const VBLockList soList = { 0 };
const VBLockVect soVect = { 0 };
//const VBLockNode soNode = { 0 };
const VBLockAttr soAttr = { 0 };
const VBLockDesc soDesc = { 0 };

///////////////////////////////////////////////////////////////////////
//  VBLockRoot utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockRoot
//         structure pointers

void
VBLockRoot_Init ( VBLockRoot *pRoot, UCHAR uVBLock, UINT nSizenn )
{
    pRoot -> uVBLock = uVBLock;
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16   &&
         nSizenn == (nSizenn&0xFFFF)   )
    {
      ZeroMemory( &pRoot->u.oRoot16, sizeof(pRoot->u.oRoot16) );
      pRoot->u.oRoot16.nSizeAlloc = static_cast<UINT16>(nSizenn);
      return;
    }
    if ( uVBLock == VBLock_Addr64            &&
         nSizenn == (nSizenn&0xFFFFFFFFFFFF)   )
    {
      ZeroMemory( &pRoot->u.oRoot64, sizeof(pRoot->u.oRoot64) );
      pRoot->u.oRoot64.nSizeAlloc = static_cast<UINT64>(nSizenn);
      return;
    }
    if ( uVBLock == VBLock_Addr32       &&
         nSizenn == (nSizenn&0xFFFFFFFF)   )
    {
      ZeroMemory( &pRoot->u.oRoot32, sizeof(pRoot->u.oRoot32) );
      pRoot->u.oRoot32.nSizeAlloc = static_cast<UINT32>(nSizenn);
      return;
    }
    if ( uVBLock == VBLock_Addr08 &&
         nSizenn == (nSizenn&0xFF)   )
    {
      ASSERT(0);
      ZeroMemory( &pRoot->u.oRoot08, sizeof(pRoot->u.oRoot08) );
      pRoot->u.oRoot08.nSizeAlloc = static_cast<UINT08>(nSizenn);
      return;
    }
    EVERR->Module ("%s(pRoot,uVBLock=%02x,nSizenn=%i", __FUNCTION__
                  , uVBLock, nSizenn )
         ->Message("Invalid addressing parameters")
         ->Throw();
}

VBLaddr
VBLockRoot_GetFirst ( const VBLockRoot *pRoot, int *pnItem )
{
    // Observe addressing model
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr64 )
      return pRoot->u.oRoot64.aFirst;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->u.oRoot32.aFirst;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->u.oRoot16.aFirst;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->u.oRoot08.aFirst;
    ASSERT(0);
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0;
}
VBLockRoot*
VBLockRoot_SetFirst ( VBLockRoot *pRoot, VBLaddr aItemFirst )
{
    // Observe addressing model
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock    == VBLock_Addr64              &&
         aItemFirst == (aItemFirst&0xFFFFFFFFFFFF)   )
    {
      pRoot->u.oRoot64.aFirst = (UINT64)aItemFirst;
      return pRoot;
    }
    if ( uVBLock    == VBLock_Addr32          &&
         aItemFirst == (aItemFirst&0xFFFFFFFF)   )
    {
      pRoot->u.oRoot32.aFirst = (UINT32)aItemFirst;
      return pRoot;
    }
    if ( uVBLock    == VBLock_Addr16      &&
         aItemFirst == (aItemFirst&0xFFFF)   )
    {
      pRoot->u.oRoot16.aFirst = (UINT16)aItemFirst;
      return pRoot;
    }
    if ( uVBLock    == VBLock_Addr08    &&
         aItemFirst == (aItemFirst&0xFF)   )
    {
      pRoot->u.oRoot08.aFirst = (UINT08)aItemFirst;
      return pRoot;
    }
    ASSERT (0);
    EVERR->Module ( "%s(pRoot=%lp, aItemFirst=%Ii)", __FUNCTION__
                  , (VBLaddr)pRoot, aItemFirst )
         ->Message("Internal VBLockRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return 0;
}
VBLaddr
VBLockRoot_GetLast  ( const VBLockRoot *pRoot, int *pnItem )
{
    // Observe addressing model
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr64 )
    {
      if ( pnItem )
        *pnItem = (VBLaddr)pRoot->u.oRoot64.nItems - 1;
      return pRoot->u.oRoot64.aLast;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      if ( pnItem )
        *pnItem = pRoot->u.oRoot32.nItems - 1;
      return pRoot->u.oRoot32.aLast;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      if ( pnItem )
        *pnItem = pRoot->u.oRoot08.nItems - 1;
      return pRoot->u.oRoot08.aLast;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      if ( pnItem )
        *pnItem = pRoot->u.oRoot16.nItems - 1;
      return pRoot->u.oRoot16.aLast;
    }
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (UINT)~0;
}
VBLockRoot*
VBLockRoot_SetLast ( VBLockRoot *pRoot, VBLaddr aItemLast )
{
    // Observe addressing model
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr32         &&
         aItemLast == (aItemLast&(UINT32)~0u)   )
    {
      pRoot->u.oRoot32.aLast = (UINT32)aItemLast;
      return pRoot;
    }
    // The masks below were written as (UINT16)~0u / (UINT08)~0u, which is what
    // raised C4310 "cast truncates constant value" twice at /W4. The warning was
    // correct about the expression and wrong about the intent: ~0u is 0xFFFFFFFF,
    // and narrowing it to UINT16 to build a 0xFFFF mask works only because the
    // truncation lands where it was wanted. Written as the literal the mask
    // actually is, the guard reads as the range check it has always been and the
    // compiler stops flagging it. The LOGIC is unchanged and was already right --
    // which is worth saying plainly, because this is the pattern MsgAttr.cpp was
    // missing entirely (finding M5), three files away from where it was written.
    if ( uVBLock   == VBLock_Addr16      &&
         aItemLast == (aItemLast&0xFFFFu)   )
    {
      pRoot->u.oRoot16.aLast = (UINT16)aItemLast;
      return pRoot;
    }
    if ( uVBLock   == VBLock_Addr64             &&
         aItemLast == (aItemLast&0xFFFFFFFFFFFF)   )
    {
      pRoot->u.oRoot64.aLast = (UINT64)aItemLast;
      return pRoot;
    }
    if ( uVBLock   == VBLock_Addr08    &&
         aItemLast == (aItemLast&0xFFu)   )
    {
      pRoot->u.oRoot08.aLast = (UINT08)aItemLast;
      return pRoot;
    }
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return 0;
}
VBLaddr
VBLockRoot_GetItems  ( const VBLockRoot *pRoot )
{
    // Observe addressing model
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr64 )
      return pRoot->u.oRoot64.nItems;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->u.oRoot32.nItems;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->u.oRoot16.nItems;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->u.oRoot08.nItems;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (UINT)~0;
}
VBLockRoot*
VBLockRoot_SetItems ( VBLockRoot *pRoot, int nItems, bool bAbsolute )
{
    if ( bAbsolute && nItems < 0 )
      EVERR->Module ( "%s(pRoot,nItems=%i,bAbsolute=%i)", __FUNCTION__
                    , nItems, bAbsolute )
           ->Message("Invalid parameter (nItems<0)" )
           ->Throw ( );
    else if ( !bAbsolute )
      nItems += VBLockRoot_GetItems ( pRoot );

    // Observe addressing model
    UINT  uItems  = static_cast<UINT>(nItems);
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32      &&
         uItems  == (uItems&0xFFFFFFFF)   )
    {
      pRoot->u.oRoot32.nItems = static_cast<UINT32>(uItems);
      return pRoot;
    }
    if ( uVBLock == VBLock_Addr64          &&
         uItems  == (uItems&0xFFFFFFFFFFFF)   )
    {
      pRoot->u.oRoot64.nItems = static_cast<UINT64>(uItems);
      return pRoot;
    }
    if ( uVBLock == VBLock_Addr16  &&
         uItems  == (uItems&0xFFFF)   )
    {
      pRoot->u.oRoot16.nItems = static_cast<UINT16>(uItems);
      return pRoot;
    }
    if ( uVBLock == VBLock_Addr08 &&
         uItems  == (uItems&0xFF)    )
    {
      pRoot->u.oRoot08.nItems = static_cast<UINT08>(uItems);
      return pRoot;
    }
    EVERR->Module ( "%s(pRoot,nItems=%i,bAbsolute=%i)", __FUNCTION__
                  , nItems, bAbsolute )
         ->Message("Internal VBLockRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pRoot;
}

///////////////////////////////////////////////////////////////////////
//  VBLock utilities and helpers
//  NOTES: Self contained operations that work on raw VBLock pointers

void*
VBLock_pud ( const VBLock *pVBLock )
{
    auto *pud = (char *)pVBLock + VBLock_Sizeof_Hdr(pVBLock);
    return pud;
}

void*
VBLock_ud_vpData ( const VBLock *pVBLock )
{
    // Observe addressing model
    UCHAR uVBLockAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return (char *)pVBLock + sizeof (pVBLock->oHdr.u.nSize32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return (char *)pVBLock + sizeof (pVBLock->oHdr.u.nSize64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return (char *)pVBLock + sizeof (pVBLock->oHdr.u.nSize16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return (char *)pVBLock + sizeof (pVBLock->oHdr.u.nSize08);
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , pVBLock->oHdr.uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

//
//  Initialises a pre-allocated VBLock entry allocated within VBHeap.
//
//  Parameters:  VBLock *pVBLock
//               Physical address of allocated VBHeap entry
//
//               UCHAR uVBLock
//               VBLock definition parameters
//
//               VBLsize nSizeof
//               Allocated total size of the VBHeap entry.
void
VBLock_Init ( VBLock *pVBLock, UCHAR uVBLock, VBLsize nSizeof )
{
    // Observe addressing model
    pVBLock -> oHdr.uVBLockDefs = uVBLock | VBLock_Alloc;
    uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32        &&
         nSizeof == (nSizeof&0xFFFFFFFF)    )
      pVBLock   -> oHdr.u.nSize32  = (UINT32)nSizeof;
    else if ( uVBLock == VBLock_Addr16    &&
              nSizeof == (nSizeof&0xFFFF)    )
      pVBLock -> oHdr.u.nSize16  = (UINT16)nSizeof;
    else if ( uVBLock == VBLock_Addr08 &&
              nSizeof == (nSizeof&0xFF)    )
      pVBLock   -> oHdr.u.nSize08  = (UINT08)nSizeof;
    else if ( uVBLock == VBLock_Addr64       &&
         nSizeof == (nSizeof&0xFFFFFFFFFFFF)    )
      pVBLock   -> oHdr.u.nSize64  = nSizeof;
    else EVERR->Module ( "%s(%x, %i)", __FUNCTION__
                       , uVBLock, nSizeof )
              ->Message("Internal VBLock.uVBLock corruption" )
              ->Throw ( );
}

VBLsize
VBLock_Hdr_u_SizeNN ( const VBLock *pVBLock )
{
    // Observe addressing model
    const UCHAR uVBLockAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLock->oHdr.u.nSize32;
    if ( uVBLockAddr == VBLock_Addr64 )
      return (UINT)pVBLock->oHdr.u.nSize64;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLock->oHdr.u.nSize16;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLock->oHdr.u.nSize08;
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , pVBLock->oHdr.uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

VBLsize
VBLock_Sizeof_Hdr ( const VBLock *pVBLock )
{
    // Observe addressing model
    UCHAR uVBLockAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    VBLsize nSizeof_Hdr = sizeof(soHdr) - sizeof(soHdr.u);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof_Hdr + sizeof(soHdr.u.nSize32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof_Hdr + (VBLsize)sizeof(soHdr.u.nSize64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof_Hdr + sizeof(soHdr.u.nSize16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof_Hdr + sizeof(soHdr.u.nSize08);
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , pVBLock->oHdr.uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

VBLsize
VBLock_Sizeof_Hdr ( UCHAR uVBLockDefs )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLockDefs & VBLock_AddrMask;
    VBLsize nSizeof_Hdr = sizeof(soHdr) - sizeof(soHdr.u);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof_Hdr + sizeof(soHdr.u.nSize32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof_Hdr + (VBLsize)sizeof(soHdr.u.nSize64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof_Hdr + sizeof(soHdr.u.nSize16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof_Hdr + sizeof(soHdr.u.nSize08);
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

UINT
VBLock_Sizeof_Hdr_ud ( const VBLock *pVBLock )
{
    // Observe addressing model
    UINT  nSizeofHdr = sizeof(soHdr) - sizeof(soHdr.u);
    UCHAR uVBLockAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLock->oHdr.u.nSize32
         - ( nSizeofHdr + sizeof(soHdr.u.nSize32) );
    if ( uVBLockAddr == VBLock_Addr64 )
      return (UINT)pVBLock->oHdr.u.nSize64
         - ( nSizeofHdr + sizeof(soHdr.u.nSize64) );
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLock->oHdr.u.nSize16
         - ( nSizeofHdr + sizeof(soHdr.u.nSize16) );
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLock->oHdr.u.nSize08
         - ( nSizeofHdr + sizeof(soHdr.u.nSize08) );
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , pVBLock->oHdr.uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

//
//  Calculates sizeof VBLockName component contained within
//  the passed VBLock
VBLsize
VBLock_Sizeof_Name ( const VBLock *pVBLock)
{
    const UCHAR   uVBLock          = pVBLock->oHdr.uVBLockDefs;
    const UCHAR   uVBLock_TypeMask = uVBLock & VBLock_TypeMask;
    const VBLsize nSizeof_VBLock   = VBLock_Hdr_u_SizeNN(pVBLock);
    const VBLaddr nVBLockAddrBegin = (VBLaddr)pVBLock;
    const VBLaddr nVBLockAddrEnd   = nVBLockAddrBegin + nSizeof_VBLock - 1;
    const VBLsize nSizeof_Hdr      = VBLock_Sizeof_Hdr(pVBLock);
    if ( (uVBLock_TypeMask) == VBLock_Name )
      return nSizeof_VBLock - nSizeof_Hdr;

    if ( (uVBLock_TypeMask) == VBLock_Field )
    {
      VBLockName *pName = VBLock_pName ( pVBLock );
      return VBLockName_Sizeof_Alloc ( uVBLock, pName );
    }
    if ( (uVBLock_TypeMask) == VBLock_Item )
    {
      VBLockItem *pVBLockItem = VBLock_pItem ( pVBLock );
      UCHAR uItemType = pVBLockItem -> uItemType & VBLock_TypeMask;
      if ( VBLockItem_IsField(pVBLockItem) )
      {
        VBLockField *pField = VBLock_pField ( pVBLock );
        VBLockName  *pName  = VBLockField_pName ( pField );
        return VBLockName_Sizeof_Alloc ( uVBLock, pName );
      }
      if ( VBLockItem_IsList(pVBLockItem) )
      {
        VBLockField *pList = VBLock_pField ( pVBLock );
        VBLockName  *pName = VBLockField_pName ( pList );
        return VBLockName_Sizeof_Alloc ( uVBLock, pName );
      }
      if ( VBLockItem_IsVect(pVBLockItem) )
      {
        VBLockField *pVect = VBLock_pField ( pVBLock );
        VBLockName  *pName = VBLockField_pName ( pVect );
        return VBLockName_Sizeof_Alloc ( uVBLock, pName );
      }
      if ( VBLockItem_IsData(pVBLockItem) )
      {
        ASSERT(0);
        return 0;
      }
      ASSERT(0);
      return ~0;
    }
    if ( (uVBLock_TypeMask) == VBLock_List )
    {
      ASSERT(0);
      return ~0;
    }
    if ( (uVBLock_TypeMask) == VBLock_Vect )
    {
      ASSERT(0);
      return ~0;
    }
    ASSERT(0);
VBLock_IsAlloc(pVBLock);
VBLock_IsLinked(pVBLock);
    EVERR->MODULE
         ->Message("Unknown block type 0x%02x [Address=0x%02x],[Heap=0x%02x]"
                  ,(uVBLock&VBLock_TypeMask), (uVBLock&VBLock_AddrMask), (uVBLock&VBLock_HeapMask) ) 
         ->Throw();
    return 0;
}

bool
VBLock_IsAddr  ( const VBLock *pVBLock, UCHAR uVBLock ) noexcept
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == uVBLock )
      return true;
    return false;
}

bool
VBLock_IsType  ( const VBLock *pVBLock, UCHAR uVBLock ) noexcept
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == uVBLock )
      return true;
    return false;
}

bool
VBLock_IsFree( const VBLock *pVBLock ) noexcept
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == 0 )
      return true;
    return false;
}

bool
VBLock_IsLinked( const VBLock *pVBLock ) noexcept
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_Linked) == VBLock_Linked )
      return true;
    return false;
}

bool
VBLock_IsAlloc ( const VBLock *pVBLock ) noexcept
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_Alloc) == VBLock_Alloc )
      return true;
    return false;
}

//  Asserts passed VBLaddr is contained within the scope of the
//  VBLock managed by this Object
//  NOTES: Covers all P2PmsgHANDLE types
//
//  Parameters:  VBLock *pVBLock
//               Containing VBLock
// 
//               void *pVBLaddr
//               Address to be checked
//
//  Returns:     BOOL
//                 TRUE... Contained address
//                 FALSE.. Invalid address
BOOL
VBLock_IsContainedVBLump ( VBLock *pVBLock, const void *pVBLaddr, VBLaddr nSizeofVBLump )
{
    if ( !VBLock_IsAlloc(pVBLock) ) {
      ASSERT(0);
      return FALSE; }
    const VBLaddr aVBLumpBegin = (VBLaddr)pVBLaddr;
    const VBLaddr aVBLockBegin = (VBLaddr)pVBLock;
    if ( aVBLumpBegin < aVBLockBegin ) {
      const VBLaddr dDelta = aVBLockBegin - aVBLumpBegin;
      return FALSE;
    }
    const VBLaddr aVBLockEnd   = aVBLockBegin + VBLock_Hdr_u_SizeNN ( pVBLock ) - 1;
    ASSERT(aVBLockBegin<=aVBLockEnd);
    if ( aVBLumpBegin > aVBLockEnd ) {
      const VBLaddr dDelta = aVBLumpBegin - aVBLockEnd;
      return FALSE;
    }
    if ( nSizeofVBLump > 0 ) {
      const VBLaddr aVBLumpEnd = aVBLumpBegin + nSizeofVBLump - 1;
      if ( aVBLumpEnd > aVBLockEnd ) {
        const VBLaddr dDelta = aVBLumpEnd - aVBLockEnd;
        VBLock_Hdr_u_SizeNN ( pVBLock );//TODO:LJM debugging only
        return FALSE;
      }
    }
    return TRUE;
}

//
//  Throws unless a lump derived from a block's own header stays inside it
//  NOTES: The VBLock_pXxx family walks offsets that live INSIDE the block, and
//         on the load path every one of those bytes came off a socket. Checking
//         what the family RETURNS is too late: VBLockName_Sizenn dereferences
//         the name header to find its own length, so an item header a few bytes
//         short puts that header past the end of the image and the read has
//         already happened by the time a pointer comes back.
//         `p2p_fuzzframe 0x5EEDF00D --replay 6 73` reads eighteen bytes past a
//         fifty-seven byte image exactly there. So the derivation is bounded
//         step by step instead, against the block that owns it.
//       : pOwner is optional through the family. The many callers that hold a
//         sub-structure rather than a block pass nothing and are unaffected;
//         VBLock_pData passes the block, which is the load path.
//       : Does not use VBLock_IsContainedVBLump. That one requires the block to
//         be flagged Alloc and ASSERTs when it is not, which is the right
//         precondition for a caller placing a lump and the wrong one here --
//         a hostile image is free to clear the flag, and answering "not
//         contained" for the wrong reason would report the wrong defect.
void
VBLock_ChkContained ( const VBLock *pOwner, const void *pv, VBLsize nSpan
                    , LPCSTR lpszWhere )
{
    if ( pOwner == nullptr )
      return;
    const VBLaddr aBlock    = (VBLaddr)pOwner;
    const VBLaddr aBlockEnd = aBlock + VBLock_Hdr_u_SizeNN ( pOwner );
    const VBLaddr aLump     = (VBLaddr)pv;
    if ( pv != nullptr && aLump >= aBlock && aLump <= aBlockEnd &&
         nSpan <= aBlockEnd - aLump )
      return;
    EVERR->Module ( lpszWhere )
         ->Message ( "VBLock lump +%u leaves its block of %u"
                   , (UINT)nSpan, (UINT)VBLock_Hdr_u_SizeNN(pOwner) )
         ->Throw ( );
}

bool
VBLock_IsName ( const VBLock *pVBLock ) noexcept
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Name )
      return true;
    return false;
}
VBLockName*
VBLock_pName ( const VBLock *pVBLock )
{
    UCHAR uVBLock          = pVBLock->oHdr.uVBLockDefs;
    UCHAR uVBLock_TypeMask = uVBLock & VBLock_TypeMask;
    if ( (uVBLock_TypeMask) == VBLock_Name )
      return (VBLockName *)VBLock_pud(pVBLock);
    if ( (uVBLock_TypeMask) == VBLock_Field )
      return VBLockField_pName ( VBLock_pField(pVBLock) );
    //if ( (uVBLock_TypeMask) == VBLock_Node )
    //  return VBLockNode_pName ( uVBLock, VBLock_pNode(pVBLock) );
    if ( (uVBLock_TypeMask) == VBLock_Item )
      return VBLockItem_pName ( uVBLock, VBLock_pItem(pVBLock) );
    if ( (uVBLock_TypeMask) == VBLock_List )
      return VBLockList_pName ( uVBLock, VBLock_pList(pVBLock) );
    if ( (uVBLock_TypeMask) == VBLock_Vect )
      return VBLockVect_pName ( uVBLock, VBLock_pVect(pVBLock) );
    ASSERT(0);
VBLock_IsAlloc(pVBLock);
VBLock_IsLinked(pVBLock);
    EVERR->MODULE
         ->Message("Unknown block type 0x%02x [Address=0x%02x],[Heap=0x%02x]"
                  ,(uVBLock&VBLock_TypeMask), (uVBLock&VBLock_AddrMask), (uVBLock&VBLock_HeapMask) ) 
         ->Throw();
    return (VBLockName *)0;
}

bool
VBLock_IsData ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Data )
      return true;
    return false;
}
VBLockData*
VBLock_pData ( VBLock *pVBLock )
{
    //  A null block arrives here from WIRE data, not from a programming error.
    //  P3PmsgObject::GetVBLock() resolves the object's address through
    //  P2PmsgHeap_Addr2Phys, which is bounded and throws for an address past
    //  the image -- but returns NULL for address zero, and nothing between
    //  there and the dereference below checks for it. A mutated frame that
    //  zeroes an object address therefore faults inside P2Peerio::RecvP2PeerMsg
    //  (`p2p_fuzzframe 0x5EEDF00D --replay 0 30`, an access violation). Reported
    //  as corruption, the way the rest of this file reports it.
    if ( pVBLock == nullptr )
      EVERR->Module ( __FUNCTION__ )
           ->Message( "Null VBLock: object address does not resolve in the image" )
           ->Throw ( );
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Data )
      return (VBLockData *)VBLock_pud(pVBLock);
    //  The block is handed down as the owner from here: everything the two
    //  branches below reach is derived from ITS header, so it is the extent
    //  those derivations have to stay inside. See VBLock_ChkContained.
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Field )
      return VBLockField_pData ( uVBLock, VBLock_pField(pVBLock), pVBLock );
    //if ( (uVBLock&VBLock_TypeMask) == VBLock_Node )
    //  return VBLockNode_pData ( uVBLock, VBLock_pNode(pVBLock) );
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Item )
      return VBLockItem_pDataChk ( uVBLock, VBLock_pItem(pVBLock), pVBLock );
    if ( (uVBLock&VBLock_TypeMask) == VBLock_List )
      return VBLockList_pData ( uVBLock, VBLock_pList(pVBLock) );
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Vect )
      return VBLockVect_pData ( uVBLock, VBLock_pVect(pVBLock) );
    //  No ASSERT(0) here. The block type is a byte off the wire and an unknown
    //  one is a hostile image, not a broken invariant -- and the refusal is
    //  already written, three lines down, unconditionally. Asserting first only
    //  decided that Debug would abort where Release correctly refuses, which is
    //  the whole of D64 in miniature. 160 of these in a 30-minute frame soak.
VBLock_IsAlloc(pVBLock);
VBLock_IsLinked(pVBLock);
    EVERR->MODULE
         ->Message("Unknown block type 0x%02x [Address=0x%02x],[Heap=0x%02x]"
                  ,(uVBLock&VBLock_TypeMask), (uVBLock&VBLock_AddrMask), (uVBLock&VBLock_HeapMask) ) 
         ->Throw();
    return nullptr;
}

bool
VBLock_IsField ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Field )
      return true;
    return false;
}
VBLockField*
VBLock_pField ( const VBLock *pVBLock )
{
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Item )
      return VBLockItem_pField ( uVBLock
                               ,(VBLockItem *)VBLock_pud(pVBLock) );
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Field )
      return (VBLockField *)VBLock_pud(pVBLock);
    //if ( (uVBLock&VBLock_TypeMask) == VBLock_Node ) {
    //  ASSERT(0);
    //  return VBLockNode_pField ( uVBLock
    //                           ,(VBLockNode *)VBLock_pud(pVBLock) );
    //} 
    if ( (uVBLock&VBLock_TypeMask) == VBLock_List )
      return VBLockList_pField ( uVBLock
                               ,(VBLockList *)VBLock_pud(pVBLock) );
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Vect )
      return VBLockVect_pField ( uVBLock
                               ,(VBLockVect *)VBLock_pud(pVBLock) );
    return (VBLockField *)0;
}

bool
VBLock_IsList ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_List )
      return true;
    return false;
}
VBLockList*
VBLock_pList ( const VBLock *pVBLock )
{
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_List )
      return (VBLockList *)VBLock_pud(pVBLock);
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Item )
      return VBLockItem_pList ( uVBLock
                              ,(VBLockItem *)VBLock_pud(pVBLock) );
    return (VBLockList *)0;
}

bool
VBLock_IsItem ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Item )
      return true;
    return false;
}

bool
VBLock_IsAttr ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Attr )
      return true;
    return false;
}
VBLockAttr*
VBLock_pAttr ( VBLock *pVBLock )
{
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Attr )
      return (VBLockAttr *)VBLock_pud(pVBLock);
    ASSERT(VBLock_IsAlloc(pVBLock));
    return (VBLockAttr *)0;
}

bool
VBLock_IsDesc ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Desc )
      return true;
    return false;
}
VBLockDesc*
VBLock_pDesc ( VBLock *pVBLock )
{
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Desc )
      return (VBLockDesc *)VBLock_pud(pVBLock);
    ASSERT(VBLock_IsAlloc(pVBLock));
    return (VBLockDesc *)0;
}

bool
VBLock_IsStck ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Stack )
      return true;
    return false;
}

bool
VBLock_IsRoot ( const VBLock *pVBLock )
{
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_TypeMask) == VBLock_Root )
      return true;
    return false;
}

VBLockItem*
VBLock_pItem ( const VBLock *pVBLock )
{
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Item )
      return (VBLockItem *)VBLock_pud(pVBLock);
    return (VBLockItem *)0;
}

///////////////////////////////////////////////////////////////////////
//  VBLockName utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockName
//         structure pointers

void
VBLockName_Init ( UCHAR uVBLock, VBLockName *pName, UCHAR uVBLockAttr
                , LPCWSTR lpszName, VBLsize nSizenn )
{
    pName -> uVBLockAttr = uVBLockAttr;
    if ( nSizenn < VBLockName_Sizeof_Min(uVBLock) )
      EVERR->Module ( __FUNCTION__)
           ->AFP(uVBLockAttr)->AFP(lpszName)->AFP(nSizenn)
           ->Message("sizeof(VBLockName)=%lli less than minimum=%lli"
                    , nSizenn, VBLockName_Sizeof_Min(uVBLock) )
           ->Throw ( );
    VBLsize nNameSize =   nSizenn          // Raw bytes
                      - ( sizeof(VBLockName) - sizeof(pName->u) )
                      -   sizeof(pName->u.vBlob08);
    if ( (nNameSize&0xFF) != nNameSize )
      EVERR->Module ( "%s(pName, uVBLockAttr, lpszName, nSizenn=%i)", __FUNCTION__
                    , nSizenn )
           ->Message("Invalid effective buffer size=%i (0 to 255)"
                    , nNameSize )
           ->Throw ( );
    pName -> u.vBlob08.nBlobSize = (UINT08)nNameSize/sizeof(P2PWCHAR);
    ASSERT(pName->u.vBlob08.nBlobSize >= 2 );
    size_t nChars = 0;
    if ( lpszName )
      nChars = wcslen(lpszName);
    //  Stored used-count is in UTF-16 units, not code points: an astral char stores as a
    //  surrogate pair (2 units). nUnits == nChars on Windows / for BMP names (§4.2).
    size_t nBlobUsed = p2p_wide_units ( lpszName, nChars );
    if ( nBlobUsed > pName->u.vBlob08.nBlobSize )
      EVERR->Module ( "%s(pName, uVBLockAttr, lpszName, nSizenn=%i)", __FUNCTION__
                    , nSizenn )
           ->Message("Attempted buffer overrun (%i vs %i)"
                    , nBlobUsed, pName->u.vBlob08.nBlobSize )
           ->Throw ( );
    if ( nChars > 0 )
      p2p_store_wide ( &pName->u.vBlob08.cBlob, lpszName, nChars );   // wchar->16-bit, nBlobUsed units (§4.2)
    { wchar_t z = 0; p2p_store_wide ( &pName->u.vBlob08.cBlob + nBlobUsed, &z, 1 ); } // NUL after nBlobUsed units (§4.2)
      pName->u.vBlob08.nBlobUsed         = (UINT08)nBlobUsed;
    ASSERT(VBLockName_Sizenn(uVBLock,pName)==nSizenn);
}

VBLaddr
VBLockName_SetChain2Next ( UCHAR uVBLock, VBLockName *pName, VBLaddr aChain2Next )
{
    if ( uVBLock     == VBLock_Addr16        &&
         aChain2Next == (aChain2Next&0xFFFF)   ) {
      pName->uVBLockAttr = 0x7F;
      return pName->u.vBlnx08.u.aChain2Next16 = (UINT16)aChain2Next;
    }
    if ( uVBLock     == VBLock_Addr32            &&
         aChain2Next == (aChain2Next&0xFFFFFFFF)   ) {
      pName->uVBLockAttr = 0x7F;
      return pName->u.vBlnx08.u.aChain2Next32 = (UINT32)aChain2Next;
    }
    if ( uVBLock     == VBLock_Addr64                 &&
         aChain2Next == (aChain2Next&0xFFFFFFFFFFFF)   ) {
      pName->uVBLockAttr = 0x7F;
      return pName->u.vBlnx08.u.aChain2Next64 = (UINT64)aChain2Next;
    }
    EVERR->Module ( __FUNCTION__ )
           ->Message("Invalid Addr16,32,64 address reference (%x) and (%x)"
                    , uVBLock, aChain2Next )
           ->Throw ( );
    return aChain2Next;
}

VBLaddr
VBLockName_GetChain2Next ( UCHAR uVBLock, const VBLockName *pName )
{
    //ASSERT(pData->uDataType==0xFF);
    if ( pName->uVBLockAttr == 0xFF )
    {
      const VBLaddr aChain2Next = pName->u.vBlin08.aVBLockAddr;
      return aChain2Next;
    }
    if ( uVBLock == VBLock_Addr16 ) {
      const VBLaddr aChain2Next = pName->u.vBlnx08.u.aChain2Next16;
      ASSERT( aChain2Next == (aChain2Next&0xFFFF) );
      return aChain2Next;
    }
    if ( uVBLock == VBLock_Addr32 ) {
      const VBLaddr aChain2Next = pName->u.vBlnx08.u.aChain2Next32;
      ASSERT( aChain2Next == (aChain2Next&0xFFFFFFFF) );
      return aChain2Next;
    }
    if ( uVBLock == VBLock_Addr64 ) {
      const VBLaddr aChain2Next = pName->u.vBlnx08.u.aChain2Next64;
      ASSERT( aChain2Next == (aChain2Next&0xFFFFFFFFFFFF) );
      return aChain2Next;
    }
    EVERR->Module ( __FUNCTION__ )
           ->Message("Invalid Addr16,32,64 address reference (%x)"
                    , uVBLock )
           ->Throw ( );
    return 0;
}
BOOL
VBLockName_IsChained ( const VBLockName *pName ) noexcept
{
    if ( pName->uVBLockAttr == 0xFF )
      return TRUE;                     // Legacy chaining flag
    if ( pName->uVBLockAttr == 0x7F )
      return TRUE;                     // Revised chaining flag
    return FALSE;
}

//
//  The floor a VBLockName may not be smaller than, for an addressing mode
//  NOTES: Masks its argument, which the VBLockRoot_* family a few hundred lines
//         up has always done and this one never did. It matters because of who
//         calls it: VBLock_pData hands the navigation family the block header
//         BYTE, not the mode -- uVBLockDefs carries type in bits 2-5 and heap
//         status in bits 6-7, so a live Addr64 field block arrives here as 0xC7
//         and matched NONE of the three tests below. The function then returned
//         its 1-byte default, and every caller that treats this as a floor was
//         holding a floor of one byte. That includes the VBLock_ChkContained in
//         VBLockField_pData, whose whole job since 2026-08-21 is to bound the
//         name header before Sizenn dereferences it: it was bounding 1 byte
//         where it meant to bound 13. Measured, not reasoned -- gdb on the
//         UNMUTATED case 0 vector shows uVBLock=199 arriving at that call.
//       : Masking cannot refuse anything a valid image contains: a caller that
//         already passed a mode passes it unchanged, and one that passed the
//         raw header now gets the answer for the mode that header declares.
//       : Addr08 keeps the default. It is the mode this library does not write
//         -- VBLockRoot_Init ASSERTs on it -- so inventing a floor for it here
//         would be inventing a number no image has ever been laid out to.
VBLsize
VBLockName_Sizeof_Min ( UCHAR uVBLock ) noexcept
{
    uVBLock = uVBLock & VBLock_AddrMask;
    VBLsize nSizeof_Min = sizeof(VBLockName) - sizeof(VBLockName::u);
    if ( uVBLock == VBLock_Addr16 ) {
      nSizeof_Min += sizeof(VBLockName::u.vBlob08) + sizeof(P2PWCHAR);
      ASSERT( sizeof(soName.u.vBlob08)>=sizeof(soName.u.vBlnx08)-sizeof(soName.u.vBlnx08.u)+sizeof(soName.u.vBlnx08.u.aChain2Next16) );
    }
    else if ( uVBLock == VBLock_Addr32 ) {
      nSizeof_Min += sizeof(VBLockName::u.vBlob08) + sizeof(P2PWCHAR)*2;
      ASSERT( sizeof(soName.u.vBlnx08)>=sizeof(soName.u.vBlob08) );
    }
    else if ( uVBLock == VBLock_Addr64 ) {
      nSizeof_Min += sizeof(VBLockName::u.vBlob08) + sizeof(P2PWCHAR)*4;
      ASSERT( sizeof(soName.u.vBlnx08)>=sizeof(soName.u.vBlob08) );
    }
    return nSizeof_Min;
}

VBLsize
VBLockName_Sizeof ( UCHAR uVBLock, size_t nNameSize )
{
    // cwName discussion
    // NOTES: VBlockName.u.vBlnx is greater than VBlockName.u.vBlob
    //        for VBlockName.u.vBlob.nBlobUsed < 3
    //      : For this reason always size on the basis cwName >= 2|4 (wchar's)
    //      : Masked for the same reason VBLockName_Sizeof_Min is. This is the
    //        rule a name is LAID OUT by; that one is the floor it is CHECKED
    //        against, and the two disagreeing on what mode they were handed is
    //        how a block gets written to one plan and walked to another.
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 ) {
      nNameSize = max ( nNameSize, 2 );
      ASSERT( sizeof(VBLnx08::nBlobSize)+sizeof(VBLnx08::u.aChain2Next32) <= sizeof(soName.u.vBlob08)+sizeof(P2PWCHAR) );
    }
    else if ( uVBLock == VBLock_Addr64 ) {
      nNameSize = max ( nNameSize, 4 );
      ASSERT( sizeof(VBLnx08::nBlobSize)+sizeof(VBLnx08::u.aChain2Next64) <= sizeof(soName.u.vBlob08)+sizeof(P2PWCHAR)*3 );
    }
    else if ( uVBLock == VBLock_Addr16 ) {
      nNameSize = max ( nNameSize, 2 );
      ASSERT( sizeof(VBLnx08::nBlobSize)+sizeof(VBLnx08::u.aChain2Next16) <= sizeof(soName.u.vBlob08) );
    }
   
    ASSERT((nNameSize&0x7F)==nNameSize);
    VBLsize nSizeof  = sizeof(soName) - sizeof(soName.u);
            nSizeof += sizeof(soName.u.vBlob08);
            nSizeof += nNameSize * sizeof(P2PWCHAR);
    const VBLsize nSizeof_Min = VBLockName_Sizeof_Min ( uVBLock );
    if ( nSizeof < nSizeof_Min ) {
      ASSERT(0);
      nSizeof = nSizeof_Min;
    }
    return nSizeof;
}

VBLsize
VBLockName_Sizeof ( UCHAR uVBLock, const VBLockName *pName )
{
    if ( uVBLock == 3 )
      return VBLockName_Sizenn ( uVBLock, pName );
    //VBLockName oName; Actually a bug 
    VBLsize nSizeof = sizeof(soName) - sizeof(soName.u)
                    + max ( (sizeof(soName.u.vBlob08)
                            +pName->u.vBlob08.nBlobUsed)*sizeof(P2PWCHAR)
                          ,  sizeof(soName.u.vBlin08) );
    return nSizeof;
}

//
//  Sizeof the allocation a VBLockName occupies, as the name itself declares it
//  NOTES: PURE. It reads the header and returns a number. It used to repair the
//         header instead: on nBlobSize == 0 it cast the const away and wrote
//         nBlobUsed over it, behind an ASSERT(0). Three things were wrong with
//         that, and the third is the one that matters.
//         (1) nBlobSize == 0 is not a state this library can produce.
//         VBLockName_Init derives it from nSizenn, which it has already floored
//         at VBLockName_Sizeof_Min, so the repair only ever fired on an image
//         that came off a socket -- and repairing a hostile image into agreement
//         with itself is not a repair, it is agreeing with the attacker.
//         (2) nBlobSize is at the same offset in vBlob08, vBlin08 and vBlnx08,
//         so READING it is variant-safe. nBlobUsed is not: in the two chained
//         variants those bytes are the first byte of aChain2Next, so for a
//         chained name the repair copied a piece of an address into a length.
//         (3) The only caller that reached this on the receive path is the
//         ASSERT at the foot of VBLockName_Sizenn, which is compiled out of
//         Release. So a mutation of the image under walk lived inside an
//         assertion, and Debug and Release walked the same bytes DIFFERENTLY --
//         Debug repaired the header and got one extent from the next call,
//         Release got another. A fuzz harness running Debug was not measuring
//         what ships. That is D64; `p2p_fuzzframe 0x5EEDF00D --replay 0 307`
//         and `--replay 11 360` are the inputs that found it.
//       : The invariants the two ASSERTs stated are real. They are now stated
//         once, as a refusal that survives Release, in VBLockName_ChkWellFormed.
VBLsize
VBLockName_Sizeof_Alloc ( UCHAR uVBLock, const VBLockName *pName )
{
    VBLsize nSizeof_Alloc  = sizeof(soName) - sizeof(soName.u)
                           + sizeof(pName->u.vBlob08);
            nSizeof_Alloc += pName->u.vBlob08.nBlobSize * sizeof(P2PWCHAR);
    return nSizeof_Alloc;
}


VBLsize
VBLockName_BlobSize ( VBLockName *pName )
{
    ASSERT(pName->u.vBlob08.nBlobUsed<=pName->u.vBlob08.nBlobSize);
    return pName->u.vBlob08.nBlobSize;
}
VBLsize
VBLockName_BlobUsed ( VBLockName *pName )
{   // Not valid when chained
    ASSERT(!VBLockName_IsChained(pName));
    return pName->u.vBlob08.nBlobUsed;
}

VBLsize
VBLockName_Sizenn ( UCHAR uVBLock, const VBLockName *pName )
{
    //VBLockName oName;
    VBLsize nSizenn = sizeof(soName)
                    - sizeof(soName.u) + sizeof(soName.u.vBlob08);
    if ( pName->uVBLockAttr == 0xFF )
      //nSizenn += max ( (sizeof(oName.u.vBlin08)
      //                        +pName->u.vBlin08.nBlobSize)
      //               ,  sizeof(oName.u.vBlin08) );
      nSizenn += pName->u.vBlin08.nBlobSize*sizeof(P2PWCHAR);
    else if ( pName->uVBLockAttr == 0x7F )
      //nSizenn += max ( (sizeof(oName.u.vBlob08)
      //                        +pName->u.vBlob08.nBlobSize)
      //               ,  sizeof(oName.u.vBlin08) );
      nSizenn += pName->u.vBlnx08.nBlobSize*sizeof(P2PWCHAR);
    else
      //nSizenn += max ( (sizeof(oName.u.vBlob08)
      //                        +pName->u.vBlob08.nBlobSize)
      //               ,  sizeof(oName.u.vBlin08) );
      nSizenn += pName->u.vBlob08.nBlobSize*sizeof(P2PWCHAR);
ASSERT(nSizenn>=VBLockName_Sizeof_Min(uVBLock));
ASSERT(nSizenn==VBLockName_Sizeof_Alloc(uVBLock,pName));
    return nSizenn;
}

//
//  Throws unless a VBLockName header could have been written by this library
//  NOTES: The two ASSERTs at the foot of VBLockName_Sizenn state real
//         invariants and state them where they cannot act: an ASSERT is a
//         Debug-only claim, and the receive path runs in Release. This says
//         the same things once, as a refusal.
//       : nBlobSize == 0 -- VBLockName_Init floors nSizenn at Sizeof_Min and
//         derives nBlobSize from what is left, so zero is a value only a wire
//         image carries. It is refused before anything divides by it or steps
//         over it.
//       : nSizenn < Sizeof_Min -- the layout rule and the navigation rule have
//         to agree about how far a name reaches. P2PmsgField_SizeofItem and
//         P2PmsgItem_InitField lay one out at max(Sizenn,Sizeof_Min);
//         VBLockField_pData walks over it at bare Sizenn. For an image this
//         library wrote those are the same number. For one it did not, the
//         name is laid out to one plan and walked to another, and the
//         VBLockData found on the far side is whatever happens to be there.
//       : nBlobUsed > nBlobSize -- only meaningful unchained, because in the
//         chained variants those bytes are part of aChain2Next rather than a
//         count. Same reason VBLockName_Sizeof_Alloc must not touch them.
//       : Bound the header BEFORE calling this. It dereferences the name, so
//         on the load path VBLock_ChkContained has to have run first -- which
//         is why the call site is next to that one, not inside Sizenn where
//         every in-memory caller would pay for it.
void
VBLockName_ChkWellFormed ( UCHAR uVBLock, const VBLockName *pName
                         , LPCSTR lpszWhere )
{
    if ( pName == nullptr )
      EVERR->Module ( lpszWhere )
           ->Message ( "VBLockName absent where the block declares one" )
           ->Throw ( );
    if ( pName->u.vBlob08.nBlobSize == 0 )
      EVERR->Module ( lpszWhere )
           ->Message ( "VBLockName declares a blob of 0" )
           ->Throw ( );
    //  Sizeof_Alloc, not Sizenn, and the difference is not style: Sizenn ASSERTs
    //  this very floor at its foot, so asking it would trip the trap this call
    //  exists to replace before it could return the refusal. The two compute the
    //  same number -- Sizenn says so itself in its second ASSERT.
    const VBLsize nSizenn = VBLockName_Sizeof_Alloc ( uVBLock, pName );
    const VBLsize nMin    = VBLockName_Sizeof_Min   ( uVBLock );
    if ( nSizenn < nMin )
      EVERR->Module ( lpszWhere )
           ->Message ( "VBLockName of %u below the floor of %u for its mode"
                     , (UINT)nSizenn, (UINT)nMin )
           ->Throw ( );
    if ( !VBLockName_IsChained ( pName ) &&
         pName->u.vBlob08.nBlobUsed > pName->u.vBlob08.nBlobSize )
      EVERR->Module ( lpszWhere )
           ->Message ( "VBLockName uses %u of a blob of %u"
                     , (UINT)pName->u.vBlob08.nBlobUsed
                     , (UINT)pName->u.vBlob08.nBlobSize )
           ->Throw ( );
}

///////////////////////////////////////////////////////////////////////
//  VBLockData utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockData
//         structure pointers
//       : VBLockData structures are usually contained within VBLockItem's
//         etc.  But may exist as standalone VBLock structures chained tp
//         from within the aforementioned VBLockItem's
//       : Following parameters coomon across VBlockData_* series of
//         functions.
//
//  Parameters:  VBLock uVBLock
//               VBLockHeap Addressing mode
//                 VBLock_Addr08
//                 VBLock_Addr16
//                 VBLock_Addr32
//                 VBLock_Addr64
//
//               VBLockData *pData
//               Data structure
//
//               VBLockDataType uDataType
//               Data type contained in above VBLockData structure
//
//               VBLockDataAttr uDataAttr
//               Attributes associated with data
//
void
VBLockData_Init ( VBLockData *pData
                , VBLockDataAttr uDataAttr, VBLockDataType uDataType, VBLsize nSizeof )
{
    // Pre-amble
    pData -> uDataAttr = uDataAttr;
    pData -> uDataType = uDataType;
    //pData -> u.aChain2Nextxx = 0;      // Blocks stray pointers
    // WSTR's
    if ( uDataType >= VBLockData_WSTR08    &&
         uDataType <= VBLockData_WSTR32var    )
      VBLockData_InitBlob ( pData, uDataAttr, uDataType, nSizeof );
    // BSTR's
    else if ( uDataType >= VBLockData_BSTR08    &&
              uDataType <= VBLockData_BSTR32var    )
      VBLockData_InitBlob ( pData, uDataAttr, uDataType, nSizeof );
    // BLOB's
    else if ( uDataType >= VBLockData_BLOB08    &&
              uDataType <= VBLockData_BLOB32var    )
      VBLockData_InitBlob ( pData, uDataAttr, uDataType, nSizeof );
}

VBLaddr
VBLockData_SetChain2Next ( UCHAR uVBLock, VBLockData *pData, VBLaddr aChain2Next )
{
    if ( aChain2Next )
      pData -> uDataType = 0xFF;       // Only upon valid reference
    if ( uVBLock     == VBLock_Addr16        &&
         aChain2Next == (aChain2Next&0xFFFF)   )
      return pData->u.aChain2Next16 = (UINT16)aChain2Next;
    if ( uVBLock     == VBLock_Addr32            &&
         aChain2Next == (aChain2Next&0xFFFFFFFF)   )
      return pData->u.aChain2Next32 = (UINT32)aChain2Next;
    if ( uVBLock     == VBLock_Addr64                 &&
         aChain2Next == (aChain2Next&0xFFFFFFFFFFFF)   )
      return pData->u.aChain2Next64 = (UINT64)aChain2Next;
    EVERR->Module ( __FUNCTION__ )
           ->Message("Invalid Addr16,32,64 address reference (%x) and (%x)"
                    , uVBLock, aChain2Next )
           ->Throw ( );
    return aChain2Next;
}

VBLaddr
VBLockData_GetChain2Next ( UCHAR uVBLock, const VBLockData *pData )
{
    ASSERT(pData->uDataType==0xFF);
    if ( uVBLock == VBLock_Addr16 ) {
      const VBLaddr aChain2Next = pData->u.aChain2Next16;
      ASSERT( aChain2Next == (aChain2Next&0xFFFF) );
      return aChain2Next;
    }
    if ( uVBLock == VBLock_Addr32 ) {
      const VBLaddr aChain2Next = pData->u.aChain2Next32;
      ASSERT( aChain2Next == (aChain2Next&0xFFFFFFFF) );
      return aChain2Next;
    }
    if ( uVBLock == VBLock_Addr64 ) {
      const VBLaddr aChain2Next = pData->u.aChain2Next64;
      ASSERT( aChain2Next == (aChain2Next&0xFFFFFFFFFFFF) );
      return aChain2Next;
    }
    EVERR->Module ( __FUNCTION__ )
           ->Message("Invalid Addr16,32,64 address reference (%x)"
                    , uVBLock )
           ->Throw ( );
    return 0;
}
BOOL
VBLockData_IsChained ( const VBLockData *pData ) noexcept
{
    if ( pData->uDataType == 0xFF )
      return TRUE;                     // Legacy chaining flag
    return FALSE;
}

void
VBLockData_InitBlob ( VBLockData *pData
                    , VBLockDataAttr uDataAttr, VBLockDataType uDataType, VBLsize nSizeof )
{
    VBLsize nSizenn;
    VBLsize nBlobSize    = nSizeof
                         - (sizeof(VBLockData) - sizeof(pData->u) );
    pData -> uDataAttr = uDataAttr;
    pData -> uDataType = uDataType;

    // BSTR's
    if ( uDataType <= VBLockData_BSTR32var )
    {
      if ( uDataType == VBLockData_BSTR08 )
        goto B08;
      if ( uDataType == VBLockData_BSTR08var )
        goto B08;
      if ( uDataType == VBLockData_BSTR16 )
        goto B16;
      if ( uDataType == VBLockData_BSTR16var )
        goto B16;
      if ( uDataType == VBLockData_BSTR32 )
        goto B32;
      if ( uDataType == VBLockData_BSTR32var )
        goto B32;
      ASSERT(0);
    }

    // WSTR's
    if ( uDataType <= VBLockData_WSTR32var )
    {
      if ( uDataType == VBLockData_WSTR08 )
        goto B08;
      if ( uDataType == VBLockData_WSTR08var )
        goto B08;
      if ( uDataType == VBLockData_WSTR16 )
        goto B16;
      if ( uDataType == VBLockData_WSTR16var )
        goto B16;
      if ( uDataType == VBLockData_WSTR32 )
        goto B32;
      if ( uDataType == VBLockData_WSTR32var )
        goto B32;
      ASSERT(0);
    }

    // BLOB's
    if ( uDataType <= VBLockData_BLOB32var )
    {
      if ( uDataType == VBLockData_BLOB08 )
        goto B08;
      if ( uDataType == VBLockData_BLOB08var )
        goto B08;
      if ( uDataType == VBLockData_BLOB16 )
        goto B16;
      if ( uDataType == VBLockData_BLOB16var )
        goto B16;
      if ( uDataType == VBLockData_BLOB32 )
        goto B32;
      if ( uDataType == VBLockData_BLOB32var )
        goto B32;
      ASSERT(0);
    }
    ASSERT(0);
    return;

    // BLOB08 copy
B08:nSizenn    = sizeof(pData->u.vBlob08);
    nBlobSize -= nSizenn;
    if ( nSizeof < nSizenn )
      EVERR->Message("VBLockData(08) undersized (%i vs %i)"
                    , nSizeof, nSizenn )
           ->Throw ( );
    pData->u.vBlob08.nBlobSize = (nBlobSize > SIZE08_MAX) ? SIZE08_MAX : (UCHAR)nBlobSize;
    pData->u.vBlob08.nBlobUsed = 0;
    pData->u.vBlob08.cBlob     = 0;
    //  What the three //ASSERT(VBLockData_Sizenn...) lines in this function
    //  meant is that the declared capacity must never exceed the room actually
    //  left for it.  They cannot be re-enabled as written - that function was
    //  removed around 2022-02-25, refer the two comments at P2Pmsg.cpp:1082 and
    //  :1118 - and they are LEFT DEAD rather than restated live, because
    //  CONTRIBUTING.md rejects structural validation inside ASSERT on sight and
    //  tools/ci/assert-baseline.txt holds the three form counts as ceilings.
    //  The invariant is enforced by the clamp itself: this arm was always
    //  correct; the 16- and 32-bit arms below tested nSizeof and assigned
    //  nBlobSize.
//ASSERT(VBLockData_Sizenn(pData)<=nSizeof);
    return;

    // BLOB16 copy
B16:nSizenn    = sizeof(pData->u.vBlob16);
    nBlobSize -= nSizenn; 
    if ( nSizeof < nSizenn )
      EVERR->Message("VBLockData(16) undersized (%i vs %i)"
                    , nSizeof, nSizenn )
           ->Throw ( );
    //  TESTS nBlobSize, not nSizeof.  It tested nSizeof until 2026-09-19, which
    //  differs from this by exactly the header and this arm's own nSizenn - 8
    //  bytes - so for nSizeof in 65536..65543 the test took its TRUE branch and
    //  wrote SIZE16_MAX as the capacity while the room behind it was nSizeof-8,
    //  i.e. 65528..65535.  The descriptor then declared up to 7 bytes more than
    //  had been allocated, and the var-width types size themselves from exactly
    //  this field (P2Pmsg.cpp:1701), so the over-claim propagated into the next
    //  copy.  Eight values wide, and the BLOB08 arm above had it right all
    //  along, which is what made it a slip rather than a convention.
    pData->u.vBlob16.nBlobSize = (nBlobSize > SIZE16_MAX) ? SIZE16_MAX : (UINT16)nBlobSize;
    pData->u.vBlob16.nBlobUsed = 0;
    pData->u.vBlob16.cBlob     = 0;
//ASSERT(VBLockData_Sizenn(pData)<=nSizeof);
    return;

    // BLOB32 copy
B32:nSizenn    = sizeof(pData->u.vBlob32);
    nBlobSize -= nSizenn;
    if ( nSizeof < nSizenn )
      EVERR->Message("VBLockData(32) undersized (%i vs %i)"
                    , nSizeof, nSizenn )
           ->Throw ( );
    //  Same correction as the 16-bit arm, and for consistency rather than for
    //  exposure: reaching the window here needs nSizeof > 4294967295, which no
    //  caller in this tree can produce.  Left in step with its siblings so the
    //  three arms cannot be read as three different intentions.
    pData->u.vBlob32.nBlobSize = (nBlobSize > SIZE32_MAX) ? SIZE32_MAX : (UINT32)nBlobSize;
    pData->u.vBlob32.nBlobUsed = 0;
    pData->u.vBlob32.cBlob     = 0;
//ASSERT(VBLockData_Sizenn(pData)<=nSizeof);
    return;
}

/*VBLsize
VBLockData_Sizeof_ ( ) noexcept
{
    //VBLockData oData;
    UINT       nSize = sizeof(soData) - sizeof(soData.u);
    return     nSize;
}*/
//##############################################################################
static VBLsize
VBLockData_Sizeof_uv ( UCHAR uVBlock, VBLockDataType uDataType, VBLsize nSizeBlob )
{   
    //VBLockData  oData;
    if ( uDataType == VBLockData_NULL )
      return 0;
    if ( uDataType <  VBLockData_BSTR08 )
    {
      ASSERT(nSizeBlob==0);
      if ( uDataType == VBLockData_INT08 )
        return sizeof(INT08);
      if ( uDataType == VBLockData_UINT08 )
        return sizeof(UINT08);
      if ( uDataType == VBLockData_INT16 )
        return sizeof(INT16);
      if ( uDataType == VBLockData_UINT16 )
        return sizeof(UINT16);
      if ( uDataType == VBLockData_INT32 )
        return sizeof(INT32);
      if ( uDataType == VBLockData_UINT32 )
        return sizeof(UINT32);
      if ( uDataType == VBLockData_INT64 )
        return sizeof(INT64);
      if ( uDataType == VBLockData_UINT64 )
        return sizeof(UINT64);
      if ( uDataType == VBLockData_DOUBLE )
        return sizeof(double);
      // The scalar tags this ladder used to omit. Every one of them is a
      // legal uDataType that VBLockData_Copy already knows how to move, so
      // falling through to ASSERT(0) here sized the copy wrongly for a cell
      // the rest of the library considered perfectly ordinary - TIME64 is
      // what P3PmsgTime constructs.
      if ( uDataType == VBLockData_FLOAT )
        return sizeof(soData.u.vFloat);
      if ( uDataType == VBLockData_TIME32 )
        return sizeof(soData.u.vTime32);
      if ( uDataType == VBLockData_TIME64 )
        return sizeof(soData.u.vTime64);
      if ( uDataType == VBLockData_BOOL )
        return sizeof(soData.u.vBool);
      if ( uDataType == VBLockData_WCHAR )
        return sizeof(soData.u.vwChar);
      ASSERT(0);
    }
    // BSTR's
    if ( uDataType < VBLockData_WSTR08 )
    {
      if ( uDataType == VBLockData_BSTR08    ||
           uDataType == VBLockData_BSTR08var    )
        return sizeof(soData.u.vBlob08) + nSizeBlob;
      if ( uDataType == VBLockData_BSTR16    ||
           uDataType == VBLockData_BSTR16var    )
        return sizeof(soData.u.vBlob16) + nSizeBlob;
      if ( uDataType == VBLockData_BSTR32    ||
           uDataType == VBLockData_BSTR32var    )
        return sizeof(soData.u.vBlob32) + nSizeBlob;
      ASSERT(0);
    }
    // WSTR's
    if ( uDataType < VBLockData_BLOB08 )
    {
      if ( uDataType == VBLockData_WSTR08    ||
           uDataType == VBLockData_WSTR08var    )
        return sizeof(soData.u.vBlob08) + nSizeBlob;
      if ( uDataType == VBLockData_WSTR16    ||
           uDataType == VBLockData_WSTR16var    )
        return sizeof(soData.u.vBlob16) + nSizeBlob;
      if ( uDataType == VBLockData_WSTR32    ||
           uDataType == VBLockData_WSTR32var    )
        return sizeof(soData.u.vBlob32) + nSizeBlob;
      ASSERT(0);
    }
    // BLOB's
    if ( uDataType < VBLockData_EODefs )
    {
      if ( uDataType == VBLockData_BLOB08 )
        return sizeof(soData.u.vBlob08) + nSizeBlob;
      if ( uDataType == VBLockData_BLOB08var )
        return sizeof(soData.u.vBlob08) + nSizeBlob;
      if ( uDataType == VBLockData_BLOB16 )
        return sizeof(soData.u.vBlob16) + nSizeBlob;
      if ( uDataType == VBLockData_BLOB16var )
        return sizeof(soData.u.vBlob16) + nSizeBlob;
      if ( uDataType == VBLockData_BLOB32 )
        return sizeof(soData.u.vBlob32) + nSizeBlob;
      if ( uDataType == VBLockData_BLOB32var )
        return sizeof(soData.u.vBlob32) + nSizeBlob;
      ASSERT(0);
    }
    // Specials
    if ( uDataType == VBLockData_GUID )
      return sizeof(soData.u.vGUID);
    ASSERT(0);
    return 0;
}
//  Bytes of the union this block is actually using
//  NOTES: THE CHAINED FORM IS A FORM, and until §29 this did not know it. A
//         chained block holds an ADDRESS in the union and nothing else -- its
//         type byte is 0xFF, no arm below claims 0xFF, and the fall-through at
//         the bottom of the function is ASSERT(0). Nothing had ever reached it,
//         because while a chain is ONE link long the only block anyone sizes is
//         the payload at the end of it, and that one carries a real type.
//       : P3PmsgData::VerifyContainment sizes EVERY link, so a chain of three
//         asserts twice -- once per block in the middle -- and a Debug build
//         halts there. Measured on a hand-built three-link chain, which is the
//         state an image can deliver and no in-process path can build.
//       : IT CHANGES NO COMPUTED SIZE, and that is worth saying plainly rather
//         than claiming a fix that is larger than it is. VBLockData_Sizeof
//         floors its result at VBLockData_Sizeof_Min -- the header plus exactly
//         this address -- so the number it handed back for a chained block was
//         already right. What was wrong was getting there through an assertion
//         and a zero. The refusal below still stands for a type byte that
//         names nothing at all, which is what it was for.
VBLsize
VBLockData_Sizeof_uv ( UCHAR uVBlock, const VBLockData *pData )
{   
    const VBLockDataType nDataType = pData -> uDataType;
    //VBLockData  oData;
    if ( VBLockData_IsChained ( pData ) )
    {
      if ( uVBlock == VBLock_Addr16 )
        return sizeof(soData.u.aChain2Next16);
      if ( uVBlock == VBLock_Addr32 )
        return sizeof(soData.u.aChain2Next32);
      if ( uVBlock == VBLock_Addr64 )
        return sizeof(soData.u.aChain2Next64);
      ASSERT(0);                       // An addressing width nothing resolves
      return 0;
    }
    if ( nDataType == VBLockData_NULL )
      return 0;
    if ( nDataType <  VBLockData_BSTR08 )
    {
      if ( nDataType == VBLockData_INT08 )
        return sizeof(INT08);
      if ( nDataType == VBLockData_UINT08 )
        return sizeof(UINT08);
      if ( nDataType == VBLockData_INT16 )
        return sizeof(INT16);
      if ( nDataType == VBLockData_UINT16 )
        return sizeof(UINT16);
      if ( nDataType == VBLockData_INT32 )
        return sizeof(INT32);
      if ( nDataType == VBLockData_UINT32 )
        return sizeof(UINT32);
      if ( nDataType == VBLockData_INT64 )
        return sizeof(INT64);
      if ( nDataType == VBLockData_UINT64 )
        return sizeof(UINT64);
      if ( nDataType == VBLockData_DOUBLE )
        return sizeof(double);
      if ( nDataType == VBLockData_FLOAT )
        return sizeof(soData.u.vFloat);
      if ( nDataType == VBLockData_TIME32 )
        return sizeof(soData.u.vTime32);
      if ( nDataType == VBLockData_TIME64 )
        return sizeof(soData.u.vTime64);
      if ( nDataType == VBLockData_BOOL )
        return sizeof(soData.u.vBool);
      if ( nDataType == VBLockData_WCHAR )
        return sizeof(soData.u.vwChar);
      ASSERT(0);
    }
    // BSTR's
    if ( nDataType < VBLockData_WSTR08 )
    {
      if ( nDataType == VBLockData_BSTR08    ||
           nDataType == VBLockData_BSTR08var    )
        return sizeof(soData.u.vBlob08) + pData->u.vBlob08.nBlobUsed;
      if ( nDataType == VBLockData_BSTR16    ||
           nDataType == VBLockData_BSTR16var    )
        return sizeof(soData.u.vBlob16) + pData->u.vBlob16.nBlobUsed;
      if ( nDataType == VBLockData_BSTR32    ||
           nDataType == VBLockData_BSTR32var    )
        return sizeof(soData.u.vBlob32) + pData->u.vBlob32.nBlobUsed;
      ASSERT(0);
    }
    // WSTR's
    if ( nDataType < VBLockData_BLOB08 )
    {
      if ( nDataType == VBLockData_WSTR08    ||
           nDataType == VBLockData_WSTR08var    )
        return sizeof(soData.u.vBlob08) + pData->u.vBlob08.nBlobUsed;
      if ( nDataType == VBLockData_WSTR16    ||
           nDataType == VBLockData_WSTR16var    )
        return sizeof(soData.u.vBlob16) + pData->u.vBlob16.nBlobUsed;
      if ( nDataType == VBLockData_WSTR32    ||
           nDataType == VBLockData_WSTR32var    )
        return sizeof(soData.u.vBlob32) + pData->u.vBlob32.nBlobUsed;
      ASSERT(0);
    }
    // BLOB's
    if ( nDataType <= VBLockData_BLOB32var )
    {
      if ( nDataType == VBLockData_BLOB08 )
        return sizeof(soData.u.vBlob08) + pData->u.vBlob08.nBlobUsed;
      if ( nDataType == VBLockData_BLOB08var )
        return sizeof(soData.u.vBlob08) + pData->u.vBlob08.nBlobSize;
      if ( nDataType == VBLockData_BLOB16 )
        return sizeof(soData.u.vBlob16) + pData->u.vBlob16.nBlobUsed;
      if ( nDataType == VBLockData_BLOB16var )
        return sizeof(soData.u.vBlob16) + pData->u.vBlob16.nBlobSize;
      if ( nDataType == VBLockData_BLOB32 )
        return sizeof(soData.u.vBlob32) + pData->u.vBlob32.nBlobUsed;
      if ( nDataType == VBLockData_BLOB32var )
        return sizeof(soData.u.vBlob32) + pData->u.vBlob32.nBlobSize;
    }
    // Specials
    if ( nDataType == VBLockData_GUID )
      return sizeof(soData.u.vGUID);
    ASSERT(0);
    return 0;
}

//  Sizeof used/minimal VBLockData component
//  NOTES: Independant of the space available in VBLock.  Use 
//         VBLock_Sizeof_VBLockData() function for this value.
//       : VBLockData_Sizeof() <= VBLock_Sizeof_VBLockData()
VBLsize
VBLockData_Sizeof ( UCHAR uVBLock, const VBLockData *pData )
{
    const VBLsize nSizeof    = sizeof(soData) - sizeof(soData.u)
                             + VBLockData_Sizeof_uv ( uVBLock, pData );
    const VBLsize nSizeofMin = VBLockData_Sizeof_Min ( uVBLock );
    return nSizeof > nSizeofMin ? nSizeof : nSizeofMin;
}
//
//  Established size of VBLock required to contain defined uDataType
//  to the nominated VBLockHeap units
//  NOTES: nDataSize only used for Blob type items
VBLsize
VBLockData_Sizeof ( UCHAR uVBLock, VBLockDataType uDataType, VBLsize nSizeBlob )
{
    const VBLsize nSizeof    = sizeof(soData) - sizeof(soData.u)
                             + VBLockData_Sizeof_uv ( uVBLock, uDataType, nSizeBlob );
    const VBLsize nSizeofMin = VBLockData_Sizeof_Min ( uVBLock );
    return nSizeof > nSizeofMin ? nSizeof : nSizeofMin;
}
VBLsize
VBLockData_Sizeof_Min ( UCHAR uVBLock )
{
    if ( uVBLock == VBLock_Addr16 )
      return sizeof(soData) - sizeof(soData.u) + sizeof(soData.u.aChain2Next16);
    if ( uVBLock == VBLock_Addr32 )
      return sizeof(soData) - sizeof(soData.u) + sizeof(soData.u.aChain2Next32);
    if ( uVBLock == VBLock_Addr64 )
      return sizeof(soData) - sizeof(soData.u) + sizeof(soData.u.aChain2Next64);
    ASSERT(0);
    return ~0;
}

//
//  Calculates sizeof VBLockData component contained within passed VBLock
//  NOTES: Allocated space always last component of the VBLock and as such
//         is defined as VBLock space remainder after VBLockName
//
//  Parameters:  VBLock *pVBLock
//
//  Returns:     VBLsize
//               Allocated sizeof VBLockData
VBLsize
VBLockData_Sizeof_Alloc ( VBLock *pVBLock)
{
    const UCHAR   uVBLock          = pVBLock->oHdr.uVBLockDefs;
    const UCHAR   uVBLock_TypeMask = uVBLock & VBLock_TypeMask;
    const VBLsize nSizeof_VBLock   = VBLock_Hdr_u_SizeNN(pVBLock);
    const VBLaddr nVBLockAddrBegin = (VBLaddr)pVBLock;
    const VBLaddr nVBLockAddrEnd   = nVBLockAddrBegin + nSizeof_VBLock - 1;
    const VBLsize nSizeof_Hdr      = VBLock_Sizeof_Hdr(pVBLock);
    if ( (uVBLock_TypeMask) == VBLock_Data ) {
      VBLsize nSizeData = nSizeof_VBLock - nSizeof_Hdr;
ASSERT((uVBLock&VBLock_AddrMask)!=3||nSizeData>=10);
      return nSizeData;
    }

    if ( (uVBLock_TypeMask) == VBLock_Field )
    {
      VBLaddr nAddrData = (VBLaddr)VBLock_pData(pVBLock);
      VBLsize nSizeData = nVBLockAddrEnd - nAddrData + 1;
      //return nSizeof_VBLock - nSizeof_Hdr - VBLockName_Sizeof(uVBLock,pName); // 64-bit cutover original
ASSERT((uVBLock&VBLock_AddrMask)!=3||nSizeData>=10);
      return nSizeData;
    }
    if ( (uVBLock_TypeMask) == VBLock_Item )
    {
      VBLockItem *pVBLockItem = VBLock_pItem ( pVBLock );
      UCHAR uItemType = pVBLockItem -> uItemType & VBLock_TypeMask;
      if ( VBLockItem_IsField(pVBLockItem) )
      {
        VBLaddr nAddrData = (VBLaddr)VBLock_pData(pVBLock);
        VBLsize nSizeData = nVBLockAddrEnd - nAddrData + 1;
ASSERT((uVBLock&VBLock_AddrMask)!=3||nSizeData>=10);
        return nSizeData;
        //VBLsize      nSizeof_Field = VBLockItem_Sizeof_ut ( pVBLock );
        //VBLockField *pField         = VBLock_pField ( pVBLock );
        //VBLaddr      nAddrData      = (VBLaddr)VBLockField_pData ( uVBLock, pField );
        //VBLaddr      nAddrEoField   = (VBLaddr)pField + nSizeof_Field;
        //VBLaddr      nSizeof_Data   = nVBLockAddrEnd - nAddrData + 1;
        //VBLsize      nSizeof_Diff   = nAddrEoField-nAddrData;
        //return VBLsize ( nSizeof_Data );
      }
      if ( VBLockItem_IsList(pVBLockItem) )
      {
        VBLsize      nSizeof_List  = VBLockItem_Sizeof_ut ( pVBLock );
        VBLockList  *pList         = VBLock_pList ( pVBLock );
        VBLaddr      nAddrData     = (VBLaddr)VBLockList_pData ( uVBLock, pList );
        VBLaddr      nAddrEoField   = (VBLaddr)pList + nSizeof_List;
        //VBLaddr      nVBLockAddrEnd = nVBLockAddrBegin + nSizeof_VBLock - 1;
        VBLaddr      nSizeof_Data   = nVBLockAddrEnd - nAddrData + 1;
        VBLsize      nSizeof_Diff   = nAddrEoField-nAddrData;
        //ASSERT(nSizeof_Diff == nSizeof_Data);
ASSERT((uVBLock&VBLock_AddrMask)!=3||nSizeof_Data>=10);
        return VBLsize ( nSizeof_Data );
      }
      if ( VBLockItem_IsVect(pVBLockItem) )
      {
        VBLockVect  *pVect        = VBLock_pVect ( pVBLock );
        VBLaddr      nAddrData    = (VBLaddr)VBLockVect_pData ( uVBLock, pVect );
        VBLsize      nSizeof_Data = (VBLsize)(nVBLockAddrEnd - nAddrData + 1);
ASSERT((uVBLock&VBLock_AddrMask)!=3||nSizeof_Data>=10);
        return nSizeof_Data;
      }
      if ( VBLockItem_IsData(pVBLockItem) )
      {
        ASSERT(0);
        return 0;
      }
      ASSERT(0);
      return ~0;
    }
    if ( (uVBLock_TypeMask) == VBLock_List )
    {
      ASSERT(0);
      return ~0;
    }
    if ( (uVBLock_TypeMask) == VBLock_Vect )
    {
      ASSERT(0);
      return ~0;
    }
    ASSERT(0);
VBLock_IsAlloc(pVBLock);
VBLock_IsLinked(pVBLock);
    EVERR->MODULE
         ->Message("Unknown block type 0x%02x [Address=0x%02x],[Heap=0x%02x]"
                  ,(uVBLock&VBLock_TypeMask), (uVBLock&VBLock_AddrMask), (uVBLock&VBLock_HeapMask) ) 
         ->Throw();
    return 0;
}

bool
VBLockData_IsBlob ( const VBLockData *pData ) noexcept
{
    const VBLockDataType uDataType = pData->uDataType;
    if ( uDataType < VBLockData_BSTR08 )
      return false;                    // Optimisation

    // WSTR's
    if ( uDataType >= VBLockData_WSTR08    &&
         uDataType <= VBLockData_WSTR32var    )
      return true;
    // BSTR's
    if ( uDataType == VBLockData_BSTR08    ||
         uDataType == VBLockData_BSTR08var ||
         uDataType == VBLockData_BSTR16    ||
         uDataType == VBLockData_BSTR16var ||
         uDataType == VBLockData_BSTR32    ||
         uDataType == VBLockData_BSTR32var    )
      return true;
    // BLOB's
    if ( uDataType == VBLockData_BLOB08    ||
         uDataType == VBLockData_BLOB08var ||
         uDataType == VBLockData_BLOB16    ||
         uDataType == VBLockData_BLOB16var ||
         uDataType == VBLockData_BLOB32    ||
         uDataType == VBLockData_BLOB32var    )
        return true;
    return false;
}

bool
VBLockData_IsBSTR ( const VBLockData *pData ) noexcept
{
    const VBLockDataType uDataType = pData->uDataType;

    // BSTR's
    if ( uDataType == VBLockData_BSTR08    ||
         uDataType == VBLockData_BSTR08var ||
         uDataType == VBLockData_BSTR16    ||
         uDataType == VBLockData_BSTR16var ||
         uDataType == VBLockData_BSTR32    ||
         uDataType == VBLockData_BSTR32var    )
      return true;
    return false;
}
bool
VBLockData_IsWSTR ( const VBLockData *pData ) noexcept
{
    const VBLockDataType uDataType = pData->uDataType;

    // WSTR's
    if ( uDataType == VBLockData_WSTR08    ||
         uDataType == VBLockData_WSTR08var ||
         uDataType == VBLockData_WSTR16    ||
         uDataType == VBLockData_WSTR16var ||
         uDataType == VBLockData_WSTR32    ||
         uDataType == VBLockData_WSTR32var   )
    return true;
  return false;
}
void*
VBLockData_pcBlob ( VBLockData *pData )
{
    const VBLockDataType uDataType = pData->uDataType;

    // BSTR's
    if ( uDataType <= VBLockData_BSTR32var )
    {
      if ( uDataType == VBLockData_BSTR08    ||
           uDataType == VBLockData_BSTR08var    )
        return &pData->u.vBlob08.cBlob;
      if ( uDataType == VBLockData_BSTR16    ||
           uDataType == VBLockData_BSTR16var    )
        return &pData->u.vBlob16.cBlob;
      if ( uDataType == VBLockData_BSTR32    ||
           uDataType == VBLockData_BSTR32var    )
        return &pData->u.vBlob32.cBlob;
      ASSERT(0);
    }
    // WSTR's
    if ( uDataType <= VBLockData_WSTR32var )
    {
      if ( uDataType == VBLockData_WSTR08    ||
           uDataType == VBLockData_WSTR08var    )
        return &pData->u.vBlob08.cBlob;
      if ( uDataType == VBLockData_WSTR16    ||
           uDataType == VBLockData_WSTR16var    )
        return &pData->u.vBlob16.cBlob;
      if ( uDataType == VBLockData_WSTR32    ||
           uDataType == VBLockData_WSTR32var    )
        return &pData->u.vBlob32.cBlob;
      ASSERT(0);
    }
    // BLOB's
    if ( uDataType <= VBLockData_BLOB32var )
    {
      if ( uDataType == VBLockData_BLOB08    ||
           uDataType == VBLockData_BLOB08var    )
        return &pData->u.vBlob08.cBlob;
      if ( uDataType == VBLockData_BLOB16    ||
           uDataType == VBLockData_BLOB16var    )
        return &pData->u.vBlob16.cBlob;
      if ( uDataType == VBLockData_BLOB32    ||
           uDataType == VBLockData_BLOB32var    )
        return &pData->u.vBlob32.cBlob;
      ASSERT(0);
    }
    ASSERT(0);
    return 0;
}
VBLsize
VBLockData_BlobUsed ( const VBLockData *pData )
{
    const VBLockDataType uDataType = pData->uDataType;

    // BSTR's
    if ( uDataType <= VBLockData_BSTR32var )
    {
      if ( uDataType == VBLockData_BSTR08 )
        return pData->u.vBlob08.nBlobUsed;
      if ( uDataType == VBLockData_BSTR08var )
        return pData->u.vBlob08.nBlobUsed;
      if ( uDataType == VBLockData_BSTR16 )
        return pData->u.vBlob16.nBlobUsed;
      if ( uDataType == VBLockData_BSTR16var )
        return pData->u.vBlob16.nBlobUsed;
      if ( uDataType == VBLockData_BSTR32 )
        return pData->u.vBlob32.nBlobUsed;
      if ( uDataType == VBLockData_BSTR32var )
        return pData->u.vBlob32.nBlobUsed;
      ASSERT(0);
    }
    // WSTR's
    if ( uDataType <= VBLockData_WSTR32var )
    {
      if ( uDataType == VBLockData_WSTR08 )
        return pData->u.vBlob08.nBlobUsed;
      if ( uDataType == VBLockData_WSTR08var )
        return pData->u.vBlob08.nBlobUsed;
      if ( uDataType == VBLockData_WSTR16 )
        return pData->u.vBlob16.nBlobUsed;
      if ( uDataType == VBLockData_WSTR16var )
        return pData->u.vBlob16.nBlobUsed;
      if ( uDataType == VBLockData_WSTR32 )
        return pData->u.vBlob32.nBlobUsed;
      if ( uDataType == VBLockData_WSTR32var )
        return pData->u.vBlob32.nBlobUsed;
      ASSERT(0);
    }
    // BLOB's
    if ( uDataType <= VBLockData_BLOB32var )
    {
      if ( uDataType == VBLockData_BLOB08 )
        return pData->u.vBlob08.nBlobUsed;
      if ( uDataType == VBLockData_BLOB08var )
        return pData->u.vBlob08.nBlobUsed;
      if ( uDataType == VBLockData_BLOB16 )
        return pData->u.vBlob16.nBlobUsed;
      if ( uDataType == VBLockData_BLOB16var )
        return pData->u.vBlob16.nBlobUsed;
      if ( uDataType == VBLockData_BLOB32 )
        return pData->u.vBlob32.nBlobUsed;
      if ( uDataType == VBLockData_BLOB32var )
        return pData->u.vBlob32.nBlobUsed;
      ASSERT(0);
    }
    ASSERT(0);
    return 0;
}
VBLsize
VBLockData_BlobSize ( const VBLockData *pData )
{
    const VBLockDataType uDataType = pData->uDataType;

    // BSTR's
    if ( uDataType <= VBLockData_BSTR32var )
    {
      if ( uDataType == VBLockData_BSTR08    ||
           uDataType == VBLockData_BSTR08var    )
        return pData->u.vBlob08.nBlobSize;
      if ( uDataType == VBLockData_BSTR16    ||
           uDataType == VBLockData_BSTR16var    )
        return pData->u.vBlob16.nBlobSize;
      if ( uDataType == VBLockData_BSTR32    ||
           uDataType == VBLockData_BSTR32var    )
        return pData->u.vBlob32.nBlobSize;
      ASSERT(0);
    }
    // WSTR's
    if ( uDataType <= VBLockData_WSTR32var )
    {
      if ( uDataType == VBLockData_WSTR08    ||
           uDataType == VBLockData_WSTR08var    )
        return pData->u.vBlob08.nBlobSize;
      if ( uDataType == VBLockData_WSTR16    ||
           uDataType == VBLockData_WSTR16var    )
        return pData->u.vBlob16.nBlobSize;
      if ( uDataType == VBLockData_WSTR32    ||
           uDataType == VBLockData_WSTR32var    )
        return pData->u.vBlob32.nBlobSize;
      ASSERT(0);
    }
    // BLOB's
    if ( uDataType <= VBLockData_BLOB32var )
    {
      if ( uDataType == VBLockData_BLOB08    ||
           uDataType == VBLockData_BLOB08var    )
        return pData->u.vBlob08.nBlobSize;
      if ( uDataType == VBLockData_BLOB16    ||
           uDataType == VBLockData_BLOB16var    )
        return pData->u.vBlob16.nBlobSize;
      if ( uDataType == VBLockData_BLOB32    ||
           uDataType == VBLockData_BLOB32var    )
        return pData->u.vBlob32.nBlobSize;
      ASSERT(0);
    }
    ASSERT(0);
    return 0;
}
//
//  Which length-prefixed family a cell belongs to, and where in it
//  NOTES: BSTR, WSTR and BLOB are laid out identically - 08, 08var, 16, 16var,
//         32, 32var - so the width and the var flag are arithmetic rather than
//         a fourth six-way ladder.  Returns false for anything that is not
//         length-prefixed at all (the scalars, GUID), which is a legitimate
//         answer rather than an error: the callers each decide what to do.
static bool
VBLockData_BlobFamily_ ( VBLockDataType uDataType, VBLockDataType *puBase
                       , int *pnWidth, int *pnVar ) noexcept
{
    VBLockDataType uBase = 0;
    if      ( uDataType >= VBLockData_BSTR08 && uDataType <= VBLockData_BSTR32var )
      uBase = VBLockData_BSTR08;
    else if ( uDataType >= VBLockData_WSTR08 && uDataType <= VBLockData_WSTR32var )
      uBase = VBLockData_WSTR08;
    else if ( uDataType >= VBLockData_BLOB08 && uDataType <= VBLockData_BLOB32var )
      uBase = VBLockData_BLOB08;
    else
      return false;

    const int nOffset = (int)( uDataType - uBase );
    if ( puBase  ) *puBase  = uBase;
    if ( pnWidth ) *pnWidth = nOffset / 2;      // 0 = 08, 1 = 16, 2 = 32
    if ( pnVar   ) *pnVar   = nOffset % 2;      // the 'var' half of each pair
    return true;
}

//
//  Largest blob a cell of this TYPE could ever hold
//  NOTES: A property of the type tag and nothing else - the length field is a
//         UINT08, UINT16 or UINT32, so 255 bytes is the ceiling of a BLOB08
//         however much heap is free.  Separated from VBLockData_BlobMax so a
//         caller can ask the question of a type it is CONSIDERING rather than
//         only of the cell it already has, which is what widening needs
VBLsize
VBLockData_BlobMaxOf ( VBLockDataType uDataType )
{
    int nWidth = 0;
    if ( !VBLockData_BlobFamily_ ( uDataType, 0, &nWidth, 0 ) )
    {
      ASSERT(0);
      return 0;
    }
    if ( nWidth == 0 )
      return 0xFF;
    if ( nWidth == 1 )
      return 0xFFFF;
    return 0xFFFFFFFF;
}

VBLsize
VBLockData_BlobMax  ( const VBLockData *pData )
{
    return VBLockData_BlobMaxOf ( pData->uDataType );
}

//
//  The narrowest type in this cell's own family that can express nBlobSize
//  NOTES: Widening only, and only within the family - a BLOB08 becomes a
//         BLOB16, never a BSTR16, and a cell already wide enough is returned
//         unchanged.  The 'var' flag is carried across because it says how the
//         payload is READ, which growing it does not change.
//       : The type tag travels on the wire and every reader dispatches on it
//         (VBLockData_BlobCopy, VBLockData_BlobSize, the serialiser), so a
//         widened cell round-trips without any agreement between the ends.
//       : A type with no length prefix is returned unchanged.  It cannot hold a
//         blob at any width, and saying so is the caller's ceiling check rather
//         than this function's business
VBLockDataType
VBLockData_WidenBlob ( VBLockDataType uDataType, VBLsize nBlobSize )
{
    VBLockDataType uBase  = 0;
    int            nWidth = 0;
    int            nVar   = 0;
    if ( !VBLockData_BlobFamily_ ( uDataType, &uBase, &nWidth, &nVar ) )
      return uDataType;

    while ( nWidth < 2 &&
            VBLockData_BlobMaxOf (
              (VBLockDataType)( uBase + nWidth * 2 + nVar ) ) < nBlobSize )
      nWidth++;

    return (VBLockDataType)( uBase + nWidth * 2 + nVar );
}

void*
VBLockData_BlobCopy ( VBLockData *pData, const void *pvBlob, VBLsize nBlobSize )
{
    const VBLockDataType uDataType = pData->uDataType;
    char  *pcBlob = 0;
            pData -> uDataAttr &= ~VBLockAttr_NULL;

    // BSTR's
    if ( uDataType <= VBLockData_BSTR32var )
    {
      if ( uDataType == VBLockData_BSTR16 )
        goto B16;
      if ( uDataType == VBLockData_BSTR16var )
        goto B16;
      if ( uDataType == VBLockData_BSTR32 )
        goto B32;
      if ( uDataType == VBLockData_BSTR32var )
        goto B32;
      if ( uDataType == VBLockData_BSTR08 )
        goto B08;
      if ( uDataType == VBLockData_BSTR08var )
        goto B08;
      ASSERT(0);
    }
    // WSTR's
    if ( uDataType <= VBLockData_WSTR32var )
    {
      if ( uDataType == VBLockData_WSTR16 )
        goto B16;
      if ( uDataType == VBLockData_WSTR16var )
        goto B16;
      if ( uDataType == VBLockData_WSTR32 )
        goto B32;
      if ( uDataType == VBLockData_WSTR32var )
        goto B32;
      if ( uDataType == VBLockData_WSTR08 )
        goto B08;
      if ( uDataType == VBLockData_WSTR08var )
        goto B08;
      ASSERT(0);
    }
    // BLOB's
    if ( uDataType <= VBLockData_BLOB32var )
    {
      if ( uDataType == VBLockData_BLOB16 )
        goto B16;
      if ( uDataType == VBLockData_BLOB16var )
        goto B16;
      if ( uDataType == VBLockData_BLOB32 )
        goto B32;
      if ( uDataType == VBLockData_BLOB32var )
        goto B32;
      if ( uDataType == VBLockData_BLOB08 )
        goto B08;
      if ( uDataType == VBLockData_BLOB08var )
        goto B08;
      ASSERT(0);
    }
    ASSERT(0);
    return pcBlob;
    // BLOB08 copy
B08:pcBlob = (char *)&pData->u.vBlob08.cBlob;
    if ( pvBlob )
      memcpy ( pcBlob, pvBlob, nBlobSize );
    else
      ZeroMemory ( pcBlob, nBlobSize );
    pData -> u.vBlob08.nBlobUsed = UINT08(nBlobSize);
   *(char *)(pcBlob+nBlobSize)   = 0;
   *(char *)(pcBlob+nBlobSize+1) = 0;
    ASSERT(pData->u.vBlob08.nBlobSize>=pData -> u.vBlob08.nBlobUsed);
    return pcBlob;

    // BLOB16 copy
B16:pcBlob = (char *)&pData->u.vBlob16.cBlob;
    if ( pvBlob )
      memcpy ( pcBlob, pvBlob, nBlobSize );
    else
      ZeroMemory ( pcBlob, nBlobSize );
    pData->u.vBlob16.nBlobUsed = UINT16(nBlobSize);
   *(char *)(pcBlob+nBlobSize)   = 0;
   *(char *)(pcBlob+nBlobSize+1) = 0;
//WCHAR *pcTCHAR=(WCHAR*)pcBlob;
    ASSERT(pData->u.vBlob16.nBlobSize>=pData -> u.vBlob16.nBlobUsed);
    return pcBlob;

    // BLOB32 copy
B32:pcBlob = (char *)&pData->u.vBlob32.cBlob;
    if ( pvBlob )
      memcpy ( pcBlob, pvBlob, nBlobSize );
    else
      ZeroMemory ( pcBlob, nBlobSize );
    pData->u.vBlob32.nBlobUsed = UINT32(nBlobSize);
   *(char *)(pcBlob+nBlobSize)   = 0;
   *(char *)(pcBlob+nBlobSize+1) = 0;
    ASSERT(pData->u.vBlob32.nBlobSize>=pData -> u.vBlob32.nBlobUsed);
    return pcBlob;
}

VBLockData*
VBLockData_Copy (       VBLockData *pDataDst, VBLsize nSizeDst
                , const VBLockData *pDataSrc, VBLsize nSizeSrc )
{
    // Size may vary according to VBLockHeap addressing modes
    const VBLockDataType  uDataType = pDataSrc -> uDataType;
    pDataDst -> uDataType = uDataType;
    pDataDst -> uDataAttr = pDataSrc -> uDataAttr;

    // To be sure, to be sure
    // Destination must be at least as large as the source, or the blob copy
    // (plus its 2-byte terminator) in the B08/B16/B32 paths below overruns
    // pDataDst. This was a debug-only ASSERT (compiled out in release); make it
    // a hard runtime check so a source larger than the destination cannot
    // corrupt the heap.
    //UINT nSizeofSrc = VBLockData_Sizeof ( (VBLockData *)pDataSrc );
    if ( nSizeDst < nSizeSrc )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "VBLockData_Copy destination(%u) smaller than source(%u)"
                     , (UINT)nSizeDst, (UINT)nSizeSrc )
           ->Throw();

    // Built-in-types
    if ( uDataType <  VBLockData_BSTR08 )
    {
      if ( uDataType == VBLockData_NULL )
      {
        VBLsize nSizeof_u = nSizeDst-sizeof(VBLockData)+sizeof(VBLockData::u);
        ZeroMemory ( &pDataDst->u.aAlloc[0], nSizeof_u );
        ASSERT(nSizeof_u>0);
      }
      else if ( uDataType == VBLockData_INT32 )
        pDataDst -> u.vInt32 = pDataSrc -> u.vInt32;
      else if ( uDataType == VBLockData_UINT32 )
        pDataDst -> u.vuInt32 = pDataSrc -> u.vuInt32;
      else if ( uDataType == VBLockData_INT16 )
        pDataDst -> u.vInt16 = pDataSrc -> u.vInt16;
      else if ( uDataType == VBLockData_UINT16 )
        pDataDst -> u.vuInt16 = pDataSrc -> u.vuInt16;
      else if ( uDataType == VBLockData_INT64 )
        pDataDst -> u.vInt64 = pDataSrc -> u.vInt64;
      else if ( uDataType == VBLockData_UINT64 )
        pDataDst -> u.vuInt64 = pDataSrc -> u.vuInt64;
      else if ( uDataType == VBLockData_DOUBLE )
        pDataDst -> u.vDouble = pDataSrc -> u.vDouble;
      else if ( uDataType == VBLockData_FLOAT )
        pDataDst -> u.vFloat = pDataSrc -> u.vFloat;
      else if ( uDataType == VBLockData_TIME64 )
        pDataDst -> u.vTime64 = pDataSrc -> u.vTime64;
      else if ( uDataType == VBLockData_BOOL )
        pDataDst -> u.vBool = pDataSrc -> u.vBool;
      else if ( uDataType == VBLockData_WCHAR )
        pDataDst -> u.vwChar = pDataSrc -> u.vwChar;
      else if ( uDataType == VBLockData_INT08 )
        pDataDst -> u.vInt08 = pDataSrc -> u.vInt08;
      else if ( uDataType == VBLockData_UINT08 )
        pDataDst -> u.vuInt08 = pDataSrc -> u.vuInt08;
      else
        ASSERT(0);
      return pDataDst;
    }

    // BSTR's
    VBLsize nBlobUsed = 0;
    VBLsize nBlobSize = 0;
    if ( uDataType < VBLockData_WSTR08 )
    {
      if ( uDataType == VBLockData_BSTR08 )
      {
        nBlobSize = pDataSrc->u.vBlob08.nBlobUsed;
        goto B08;
      }
      if ( uDataType == VBLockData_BSTR08var )
      {
        nBlobSize = pDataSrc->u.vBlob08.nBlobSize;
        goto B08;
      }
      if ( uDataType == VBLockData_BSTR16 )
      {
        nBlobSize = pDataSrc->u.vBlob16.nBlobUsed;
        goto B16;
      }
      if ( uDataType == VBLockData_BSTR16var )
      {
        nBlobSize = pDataSrc->u.vBlob16.nBlobSize;
        goto B16;
      }
      if ( uDataType == VBLockData_BSTR32 )
      {
        nBlobSize = pDataSrc->u.vBlob32.nBlobUsed;
        goto B32;
      }
      if ( uDataType == VBLockData_BSTR32var )
      {
        nBlobSize = pDataSrc->u.vBlob32.nBlobSize;
        goto B32;
      }
      ASSERT(0);
    }

    // WSTR's
    if ( uDataType < VBLockData_BLOB08 )
    {
      if ( uDataType == VBLockData_WSTR08 )
      {
        nBlobSize = pDataSrc->u.vBlob08.nBlobUsed;
        goto B08;
      }
      if ( uDataType == VBLockData_WSTR08var )
      {
        nBlobSize = pDataSrc->u.vBlob08.nBlobSize;
        goto B08;
      }
      if ( uDataType == VBLockData_WSTR16 )
      {
        nBlobSize = pDataSrc->u.vBlob16.nBlobUsed;
        goto B16;
      }
      if ( uDataType == VBLockData_WSTR16var )
      {
        nBlobSize = pDataSrc->u.vBlob16.nBlobSize;
        goto B16;
      }
      if ( uDataType == VBLockData_WSTR32 )
      {
        nBlobSize = pDataSrc->u.vBlob32.nBlobUsed;
        goto B32;
      }
      if ( uDataType == VBLockData_WSTR32var )
      {
        nBlobSize = pDataSrc->u.vBlob32.nBlobSize;
        goto B32;
      }
      ASSERT(0);
    }

    // BLOB's
    else if ( uDataType <= VBLockData_BLOB32var )
    {
      if ( uDataType == VBLockData_BLOB08 )
      {
        nBlobSize = pDataSrc->u.vBlob08.nBlobUsed;
        goto B08;
      }
      if ( uDataType == VBLockData_BLOB08var )
      {
        nBlobSize = pDataSrc->u.vBlob08.nBlobSize;
        goto B08;
      }
      if ( uDataType == VBLockData_BLOB16 )
      {
        nBlobSize = pDataSrc->u.vBlob16.nBlobUsed;
        goto B16;
      }
      if ( uDataType == VBLockData_BLOB16var )
      {
        nBlobSize = pDataSrc->u.vBlob16.nBlobSize;
        goto B16;
      }
      if ( uDataType == VBLockData_BLOB32 )
      {
        nBlobSize = pDataSrc->u.vBlob32.nBlobUsed;
        goto B32;
      }
      if ( uDataType == VBLockData_BLOB32var )
      {
        nBlobSize = pDataSrc->u.vBlob32.nBlobSize;
        goto B32;
      }
      ASSERT(0);
    }

    // Specials
    else if ( uDataType == VBLockData_GUID )
    {
        pDataDst -> u.vGUID = pDataSrc -> u.vGUID;
        return pDataDst;
    }
    ASSERT(0);
    return 0;

    // BLOB08 copy
B08:nBlobUsed = pDataSrc->u.vBlob08.nBlobUsed;
    {
      memcpy ( &pDataDst->u.vBlob08.cBlob,&pDataSrc->u.vBlob08.cBlob, nBlobUsed );
      if ( nBlobUsed < nBlobSize )
        ZeroMemory ( (char *)&pDataDst->u.vBlob08.cBlob+nBlobUsed, nBlobSize-nBlobUsed );
      pDataDst->u.vBlob08.nBlobSize = (UINT08)nBlobSize;
      pDataDst->u.vBlob08.nBlobUsed = (UINT08)nBlobUsed;
      char *pcBlob08   = (char *)&pDataDst->u.vBlob08.cBlob + nBlobUsed;
           *pcBlob08++ = 0; 
           *pcBlob08   = 0;
ASSERT(nBlobUsed<=nBlobSize);
      return pDataDst;
    }

    // BLOB16 copy
B16:nBlobUsed = pDataSrc->u.vBlob16.nBlobUsed;
    {
      memcpy ( &pDataDst->u.vBlob16.cBlob,&pDataSrc->u.vBlob16.cBlob, nBlobUsed );
      if ( nBlobUsed < nBlobSize )
        ZeroMemory ( (char *)&pDataDst->u.vBlob16.cBlob+nBlobUsed, nBlobSize-nBlobUsed );
      pDataDst->u.vBlob16.nBlobSize = (UINT16)nBlobSize;
      pDataDst->u.vBlob16.nBlobUsed = (UINT16)nBlobUsed;
      char *pcBlob16   = (char *)&pDataDst->u.vBlob16.cBlob + nBlobUsed;
           *pcBlob16++ = 0;
           *pcBlob16   = 0;
ASSERT(nBlobUsed<=nBlobSize);
      return pDataDst;
    }

    // BLOB32 copy
B32:nBlobUsed = pDataSrc->u.vBlob32.nBlobUsed;
    {
      memcpy ( &pDataDst->u.vBlob32.cBlob,&pDataSrc->u.vBlob32.cBlob, nBlobUsed );
      if ( nBlobUsed < nBlobSize )
        ZeroMemory ( (char *)&pDataDst->u.vBlob32.cBlob+nBlobUsed, nBlobSize-nBlobUsed );
      pDataDst->u.vBlob32.nBlobSize = (UINT32)nBlobSize;
      pDataDst->u.vBlob32.nBlobUsed = (UINT32)nBlobUsed;
      char *pcBlob32   = (char *)&pDataDst->u.vBlob32.cBlob + nBlobUsed;
           *pcBlob32++ = 0; 
           *pcBlob32   = 0;
ASSERT(nBlobUsed<=nBlobSize);
      return pDataDst;
    }
}

///////////////////////////////////////////////////////////////////////
//  VBLockField utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockField
//         structure pointers

void
VBLockField_Init  ( VBLockField *pField, UCHAR uAccessAttr )
{
    pField -> uAccessAttr = uAccessAttr;
}

VBLockName*
VBLockField_pName ( const VBLockField *pField )
{
    return &((VBLockField*)pField) -> oVBLockName;
}

VBLockData*
VBLockField_pData ( UCHAR uVBLock, const VBLockField *pField, const VBLock *pOwner )
{
    const VBLockName *pName = &pField -> oVBLockName;
    //  Bounded BEFORE Sizenn reads it. Sizenn dereferences the name header to
    //  find the name's length, so a check on what this function returns runs
    //  after the out-of-bounds read rather than instead of it. Sizeof_Min is
    //  the codebase's own floor for a name and Sizenn asserts against it, so
    //  demanding that much be present rejects nothing a valid image contains.
    VBLock_ChkContained ( pOwner, pName, VBLockName_Sizeof_Min(uVBLock)
                        , __FUNCTION__ );
    //  Then bounded AGAINST ITSELF, once the bytes are known to be there. The
    //  step below turns the name's own declared length into the address of the
    //  VBLockData; ChkContained above says those header bytes are inside the
    //  block, and ChkContained below says the answer is too, but neither says
    //  the length is one this library could have written -- so a zero-length
    //  name puts the data pointer at a fixed small offset and the walk carries
    //  on reading whatever lives there as a VBLockData. That is the second half
    //  of D64: `p2p_fuzzframe 0x5EEDF00D --replay 0 307` arrives here with
    //  nBlobSize == 0 and, in Release, was not stopped by anything.
    VBLockName_ChkWellFormed ( uVBLock, pName, __FUNCTION__ );
    char *pData  = (char *)pName;
          pData += VBLockName_Sizenn ( uVBLock, pName );
    VBLock_ChkContained ( pOwner, pData
                        , (VBLsize)(sizeof(VBLockData)-sizeof(VBLockData::u))
                        , __FUNCTION__ );
    return (VBLockData *)pData;
}

VBLsize
VBLockField_Sizeof ( )
{
    return sizeof(VBLockField) - sizeof(VBLockName) - sizeof(VBLockData);
}

VBLsize
VBLockField_Sizenn ( const VBLock *pVBLock )
{
    VBLsize nSizenn = VBLock_Hdr_u_SizeNN(pVBLock)
                    - ( (VBLsize)VBLock_pField(pVBLock)
                      - (VBLsize)pVBLock );
    return nSizenn;
}

VBLsize
VBLockField_Sizeof ( UCHAR uVBLock, const VBLockField *pField, BOOL bChain )
{
    VBLsize nSizeof = sizeof(VBLockField)
                    - sizeof(pField->oVBLockName)
                    + VBLockName_Sizeof(uVBLock,VBLockField_pName(pField))
                    - sizeof(pField->oVBLockData)
                    + VBLockData_Sizeof(uVBLock,VBLockField_pData(uVBLock,pField));
    return nSizeof;
}

///////////////////////////////////////////////////////////////////////
//  VBLockList utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockList
//         structure pointers

void
VBLockList_Init  ( UCHAR uVBLock, VBLockList *pList, UCHAR uVBLockAttr )
{
    pList -> uVBLockAttr = uVBLockAttr;
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr64 )
    {
      ZeroMemory( &pList->u.oList64, sizeof(pList->u.oList64) );
      return;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      ZeroMemory( &pList->u.oList16, sizeof(pList->u.oList16) );
      return;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      ZeroMemory( &pList->u.oList32, sizeof(pList->u.oList32) );
      return;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      ZeroMemory( &pList->u.oList08, sizeof(pList->u.oList08) );
      return;
    }
    ASSERT(0);
}

VBLaddr
VBLockList_GetFirst ( UCHAR uVBLock, VBLockList *pList, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr16 )
      return pList->u.oList16.aFirst;
    if ( uVBLock == VBLock_Addr32 )
      return pList->u.oList32.aFirst;
    if ( uVBLock == VBLock_Addr64 )
      return pList->u.oList64.aFirst;
    if ( uVBLock == VBLock_Addr08 )
      return pList->u.oList08.aFirst;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockList*
VBLockList_SetFirst ( UCHAR uVBLock, VBLockList *pVBLockList, VBLaddr aItemFirst )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock    == VBLock_Addr16          &&
         aItemFirst == (aItemFirst&(UINT16)~0)   )
    {
      pVBLockList->u.oList16.aFirst = (UINT16)aItemFirst;
      return pVBLockList;
    }
    if ( uVBLock    == VBLock_Addr32          &&
         aItemFirst == (aItemFirst&(UINT32)~0)   )
    {
      pVBLockList->u.oList32.aFirst = (UINT32)aItemFirst;
      return pVBLockList;
    }
    if ( uVBLock    == VBLock_Addr64              &&
         aItemFirst == (aItemFirst&0xFFFFFFFFFFFF)   )
    {
      pVBLockList->u.oList64.aFirst = (UINT64)aItemFirst;
      return pVBLockList;
    }
    if ( uVBLock    == VBLock_Addr08          &&
         aItemFirst == (aItemFirst&(UINT08)~0)   )
    {
      pVBLockList->u.oList08.aFirst = (UINT08)aItemFirst;
      return pVBLockList;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockList;
}
VBLaddr
VBLockList_GetLast  ( UCHAR uVBLock, VBLockList *pVBLockList, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr08 )
    {
      if ( pnItem )
        *pnItem = pVBLockList->u.oList08.nItems - 1;
      return pVBLockList->u.oList08.aLast;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      if ( pnItem )
        *pnItem = pVBLockList->u.oList16.nItems - 1;
      return pVBLockList->u.oList16.aLast;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      if ( pnItem )
        *pnItem = pVBLockList->u.oList32.nItems - 1;
      return pVBLockList->u.oList32.aLast;
    }
    if ( uVBLock == VBLock_Addr64 )
    {
      if ( pnItem )
        *pnItem = pVBLockList->u.oList64.nItems - 1;
      return pVBLockList->u.oList64.aLast;
    }
    ASSERT(0);
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockList*
VBLockList_SetLast ( UCHAR uVBLock, VBLockList *pVBLockList, VBLaddr aItemLast )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr16     &&
         aItemLast == (aItemLast&0xFFFF)   )
    {
      pVBLockList->u.oList16.aLast = (UINT16)aItemLast;
      return pVBLockList;
    }
    if ( uVBLock   == VBLock_Addr32         &&
         aItemLast == (aItemLast&0xFFFFFFFF)   )
    {
      pVBLockList->u.oList32.aLast = (UINT32)aItemLast;
      return pVBLockList;
    }
    if ( uVBLock   == VBLock_Addr64             &&
         aItemLast == (aItemLast&0xFFFFFFFFFFFF)   )
    {
      pVBLockList->u.oList64.aLast = (UINT64)aItemLast;
      return pVBLockList;
    }
    if ( uVBLock   == VBLock_Addr08   &&
         aItemLast == (aItemLast&0xFF)   )
    {
      pVBLockList->u.oList08.aLast = (UINT08)aItemLast;
      return pVBLockList;
    }
    ASSERT(0);
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockList;
}
VBLelem
VBLockList_GetItems  ( UCHAR uVBLock, VBLockList *pVBLockList )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16 )
      return pVBLockList->u.oList16.nItems;
    if ( uVBLock == VBLock_Addr32 )
      return pVBLockList->u.oList32.nItems;
    if ( uVBLock == VBLock_Addr64 )
      return pVBLockList->u.oList64.nItems;
    if ( uVBLock == VBLock_Addr08 )
      return pVBLockList->u.oList08.nItems;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLelem)~0;
}
VBLockList*
VBLockList_SetItems ( UCHAR uVBLock, VBLockList *pVBLockList, int nItems, bool bAbsolute )
{
    if ( !bAbsolute )
      nItems += VBLockList_GetItems ( uVBLock, pVBLockList );

    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16  &&
         nItems  == (nItems&0xFFFF)   )
    {
      pVBLockList->u.oList16.nItems = (UINT16)nItems;
      return pVBLockList;
    }
    if ( uVBLock == VBLock_Addr64 &&
         nItems  >= 0                )
    {
      pVBLockList->u.oList64.nItems = (UINT64)nItems;
      return pVBLockList;
    }
    if ( uVBLock == VBLock_Addr32 &&
         nItems  >= 0                )
    {
      pVBLockList->u.oList32.nItems = (UINT32)nItems;
      return pVBLockList;
    }
    if ( uVBLock == VBLock_Addr08 &&
         nItems  == (nItems&0xFF)    )
    {
      pVBLockList->u.oList08.nItems = (UINT08)nItems;
      return pVBLockList;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockList;
}

VBLockField*
VBLockList_pField  ( UCHAR uVBLock, const VBLockList *pList )
{
    // Observe addressing model
    char       *pField  = (char *)pList
                        + sizeof(soList)
                        - sizeof(soList.u)
                        - sizeof(soList.oVBLockField);
               uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return (VBLockField *)(pField + sizeof(soList.u.oList32));
    if ( uVBLock == VBLock_Addr64 )
      return (VBLockField *)(pField + sizeof(soList.u.oList64));
    if ( uVBLock == VBLock_Addr16 )
      return (VBLockField *)(pField + sizeof(VBLockList().u.oList16));
    if ( uVBLock == VBLock_Addr08 )
      return (VBLockField *)(pField + sizeof(soList.u.oList08));
    ASSERT(0);
    return 0;
}

VBLockName*
VBLockList_pName ( UCHAR uVBLock, const VBLockList *pList )
{
    VBLockField *pField = VBLockList_pField ( uVBLock, pList );
    return VBLockField_pName ( pField );
}

VBLockData*
VBLockList_pData ( UCHAR uVBLock, const VBLockList *pList )
{
    VBLockField *pField = VBLockList_pField ( uVBLock, pList );
    return VBLockField_pData ( uVBLock, pField );
}

VBLsize
VBLockList_Sizeof ( UCHAR uVBLock )
{
    // Observe addressing model
    //VBLockList oList;
    UCHAR   uVBLockAddr = uVBLock & VBLock_AddrMask;
    VBLsize nSizeof = sizeof(VBLockList)-sizeof(VBLockList::u)-sizeof(VBLockList::oVBLockField);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof + sizeof(VBLockList::u.oList32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof + sizeof(VBLockList::u.oList64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof + sizeof(VBLockList::u.oList16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof + sizeof(VBLockList::u.oList08);
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockList.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}

///////////////////////////////////////////////////////////////////////
//  VBLockVect utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockVect
//         structure pointers

void
VBLockVect_Init  ( UCHAR uVBLock, VBLockVect *pVect, VBLelem nItems, UCHAR uVBLockAttr )
{
    pVect -> uVBLockAttr = uVBLockAttr;
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr08 )
    {
      ZeroMemory( &pVect->u.oVect08, sizeof(pVect->u.oVect08) );
      pVect->u.oVect08.nItems = (UINT08)nItems;
      return;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      ZeroMemory( &pVect->u.oVect16, sizeof(pVect->u.oVect16) );
      pVect->u.oVect16.nItems = (UINT16)nItems;
      return;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      ZeroMemory( &pVect->u.oVect32, sizeof(pVect->u.oVect32) );
      pVect->u.oVect32.nItems = (UINT32)nItems;
      return;
    }
    if ( uVBLock == VBLock_Addr64 )
    {
      ZeroMemory( &pVect->u.oVect64, sizeof(pVect->u.oVect64) );
      pVect->u.oVect64.nItems = (UINT64)nItems;
      return;
    }
    ASSERT(0);
}

VBLelem
VBLockVect_GetItems  ( UCHAR uVBLock, const VBLockVect *pVBLockVect )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16 )
      return pVBLockVect->u.oVect16.nItems;
    if ( uVBLock == VBLock_Addr32 )
      return pVBLockVect->u.oVect32.nItems;
    if ( uVBLock == VBLock_Addr64 )
      return pVBLockVect->u.oVect64.nItems;
    if ( uVBLock == VBLock_Addr08 )
      return pVBLockVect->u.oVect08.nItems;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockVect.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0u;
}

VBLockField*
VBLockVect_pField  ( UCHAR uVBLock, const VBLockVect *pVect )
{
    // Observe addressing model.  The trailing template VBLockField sits
    // immediately after the *active* union variant (mirrors VBLockList_pField;
    // the aAlloc[32] slab makes each variant a different width).
    char       *pField  = (char *)pVect
                        + sizeof(soVect)
                        - sizeof(soVect.u)
                        - sizeof(soVect.oVBLockField);
               uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return (VBLockField *)(pField + sizeof(soVect.u.oVect32));
    if ( uVBLock == VBLock_Addr64 )
      return (VBLockField *)(pField + sizeof(soVect.u.oVect64));
    if ( uVBLock == VBLock_Addr16 )
      return (VBLockField *)(pField + sizeof(soVect.u.oVect16));
    if ( uVBLock == VBLock_Addr08 )
      return (VBLockField *)(pField + sizeof(soVect.u.oVect08));
    ASSERT(0);
    return 0;
}

//
//  Element-slot accessors.  A vect stores up to VBLockVect_MaxInline element
//  VBLock addresses inline in aAlloc[]; nElem beyond that spills to an aExtra
//  continuation block (see P3PmsgVect overflow handling).
VBLockVect*
VBLockVect_SetItems ( UCHAR uVBLock, VBLockVect *pVect, VBLelem nItems )
{
    uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 ) { pVect->u.oVect32.nItems = (UINT32)nItems; return pVect; }
    if ( uVBLock == VBLock_Addr64 ) { pVect->u.oVect64.nItems = (UINT64)nItems; return pVect; }
    if ( uVBLock == VBLock_Addr16 ) { pVect->u.oVect16.nItems = (UINT16)nItems; return pVect; }
    if ( uVBLock == VBLock_Addr08 ) { pVect->u.oVect08.nItems = (UINT08)nItems; return pVect; }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockVect.uVBLockAddr=%i corruption", uVBLock )
         ->Throw ( );
    return pVect;
}
VBLaddr
VBLockVect_GetAlloc ( UCHAR uVBLock, const VBLockVect *pVect, VBLelem nElem )
{
    uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 ) return (VBLaddr)pVect->u.oVect32.aAlloc[nElem];
    if ( uVBLock == VBLock_Addr64 ) return (VBLaddr)pVect->u.oVect64.aAlloc[nElem];
    if ( uVBLock == VBLock_Addr16 ) return (VBLaddr)pVect->u.oVect16.aAlloc[nElem];
    if ( uVBLock == VBLock_Addr08 ) return (VBLaddr)pVect->u.oVect08.aAlloc[nElem];
    ASSERT(0);
    return 0;
}
void
VBLockVect_SetAlloc ( UCHAR uVBLock, VBLockVect *pVect, VBLelem nElem, VBLaddr aElem )
{
    uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 ) { pVect->u.oVect32.aAlloc[nElem] = (UINT32)aElem; return; }
    if ( uVBLock == VBLock_Addr64 ) { pVect->u.oVect64.aAlloc[nElem] = (UINT64)aElem; return; }
    if ( uVBLock == VBLock_Addr16 ) { pVect->u.oVect16.aAlloc[nElem] = (UINT16)aElem; return; }
    if ( uVBLock == VBLock_Addr08 ) { pVect->u.oVect08.aAlloc[nElem] = (UINT08)aElem; return; }
    ASSERT(0);
}
VBLaddr
VBLockVect_GetExtra ( UCHAR uVBLock, const VBLockVect *pVect )
{
    uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 ) return (VBLaddr)pVect->u.oVect32.aExtra;
    if ( uVBLock == VBLock_Addr64 ) return (VBLaddr)pVect->u.oVect64.aExtra;
    if ( uVBLock == VBLock_Addr16 ) return (VBLaddr)pVect->u.oVect16.aExtra;
    if ( uVBLock == VBLock_Addr08 ) return (VBLaddr)pVect->u.oVect08.aExtra;
    ASSERT(0);
    return 0;
}
void
VBLockVect_SetExtra ( UCHAR uVBLock, VBLockVect *pVect, VBLaddr aExtra )
{
    uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 ) { pVect->u.oVect32.aExtra = (UINT32)aExtra; return; }
    if ( uVBLock == VBLock_Addr64 ) { pVect->u.oVect64.aExtra = (UINT64)aExtra; return; }
    if ( uVBLock == VBLock_Addr16 ) { pVect->u.oVect16.aExtra = (UINT16)aExtra; return; }
    if ( uVBLock == VBLock_Addr08 ) { pVect->u.oVect08.aExtra = (UINT08)aExtra; return; }
    ASSERT(0);
}

VBLockName*
VBLockVect_pName ( UCHAR uVBLock, const VBLockVect *pVect )
{
    VBLockField *pField = VBLockVect_pField ( uVBLock, pVect );
    return VBLockField_pName ( pField );
}

VBLockData*
VBLockVect_pData ( UCHAR uVBLock, const VBLockVect *pVect )
{
    VBLockField *pField = VBLockVect_pField ( uVBLock, pVect );
    return VBLockField_pData ( uVBLock, pField );
}

VBLsize
VBLockVect_Sizeof ( UCHAR uVBLock )
{
    // Observe addressing model.  A vect header is fixed-size (the aAlloc[32]
    // slab is always reserved); element count does not affect it.
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    VBLsize nSizeof = sizeof(soVect)-sizeof(soVect.u)-sizeof(soVect.oVBLockField);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof + sizeof(soVect.u.oVect16);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof + sizeof(soVect.u.oVect32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof + sizeof(soVect.u.oVect64);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof + sizeof(soVect.u.oVect08);
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockVect.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}

///////////////////////////////////////////////////////////////////////
//  VBLockNode utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockNode
//         structure pointers

/*void
VBLockNode_Init  ( UCHAR uVBLock, VBLockNode *pNode, UCHAR uVBLockAttr )
{
    pNode -> uVBLockAttr = uVBLockAttr;
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16 )
    {
      ZeroMemory( &pNode->u.oNode16, sizeof(pNode->u.oNode16) );
      return;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      ZeroMemory( &pNode->u.oNode32, sizeof(pNode->u.oNode32) );
      return;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      ZeroMemory( &pNode->u.oNode08, sizeof(pNode->u.oNode08) );
      return;
    }
    ASSERT(0);
}*/

/*VBLaddr
VBLockNode_GetFirst ( UCHAR uVBLock, VBLockNode *pNode, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr16 )
      return pNode->u.oNode16.aFirst;
    if ( uVBLock == VBLock_Addr32 )
      return pNode->u.oNode32.aFirst;
    if ( uVBLock == VBLock_Addr08 )
      return pNode->u.oNode08.aFirst;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0u;
}*/
/*VBLockNode*
VBLockNode_SetFirst ( UCHAR uVBLock, VBLockNode *pVBLockNode, VBLaddr aItemFirst )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock    == VBLock_Addr16          &&
         aItemFirst == (aItemFirst&(UINT16)~0)   )
    {
      pVBLockNode->u.oNode16.aFirst = (UINT16)aItemFirst;
      return pVBLockNode;
    }
    if ( uVBLock    == VBLock_Addr32          &&
         aItemFirst == (aItemFirst&(UINT32)~0)   )
    {
      pVBLockNode->u.oNode32.aFirst = (UINT32)aItemFirst;
      return pVBLockNode;
    }
    if ( uVBLock    == VBLock_Addr08          &&
         aItemFirst == (aItemFirst&(UINT08)~0)   )
    {
      pVBLockNode->u.oNode08.aFirst = (UINT08)aItemFirst;
      return pVBLockNode;
    }
    EVERR->Module ( "%s(%i)", __FUNCTION__)
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockNode;
}*/
/*VBLaddr
VBLockNode_GetLast  ( UCHAR uVBLock, VBLockNode *pVBLockNode, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16 )
    {
      if ( pnItem )
        *pnItem = pVBLockNode->u.oNode16.nItems - 1;
      return pVBLockNode->u.oNode16.aLast;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      if ( pnItem )
        *pnItem = pVBLockNode->u.oNode32.nItems - 1;
      return pVBLockNode->u.oNode32.aLast;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      if ( pnItem )
        *pnItem = pVBLockNode->u.oNode08.nItems - 1;
      return pVBLockNode->u.oNode08.aLast;
    }
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}*/
/*VBLockNode*
VBLockNode_SetLast ( UCHAR uVBLock, VBLockNode *pVBLockNode, VBLaddr aItemLast )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr16     &&
         aItemLast == (aItemLast&0xFFFF)   )
    {
      pVBLockNode->u.oNode16.aLast = (UINT16)aItemLast;
      return pVBLockNode;
    }
    if ( uVBLock   == VBLock_Addr32         &&
         aItemLast == (aItemLast&0xFFFFFFFF)   )
    {
      pVBLockNode->u.oNode32.aLast = (UINT32)aItemLast;
      return pVBLockNode;
    }
    if ( uVBLock   == VBLock_Addr08   &&
         aItemLast == (aItemLast&0xFF)   )
    {
      pVBLockNode->u.oNode08.aLast = (UINT08)aItemLast;
      return pVBLockNode;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockNode;
}*/
/*VBLelem
VBLockNode_GetItems  ( UCHAR uVBLock, VBLockNode *pVBLockNode )
{
ASSERT(0);
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16 )
      return pVBLockNode->u.oNode16.nItems;
    if ( uVBLock == VBLock_Addr32 )
      return pVBLockNode->u.oNode32.nItems;
    if ( uVBLock == VBLock_Addr08 )
      return pVBLockNode->u.oNode08.nItems;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLelem)~0u;
}*/
/*VBLockNode*
VBLockNode_SetItems ( UCHAR uVBLock, VBLockNode *pVBLockNode, VBLelem nItems, bool bAbsolute )
{
ASSERT(0);
    if ( !bAbsolute )
      nItems += VBLockNode_GetItems ( uVBLock, pVBLockNode );

    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16  &&
         nItems  == (nItems&0xFFFF)   )
    {
      pVBLockNode->u.oNode16.nItems = (UINT16)nItems;
      return pVBLockNode;
    }
    if ( uVBLock == VBLock_Addr32 &&
         nItems  >= 0                )
    {
      pVBLockNode->u.oNode32.nItems = (UINT32)nItems;
      return pVBLockNode;
    }
    if ( uVBLock == VBLock_Addr08 &&
         nItems  == (nItems&0xFF)    )
    {
      pVBLockNode->u.oNode08.nItems = (UINT08)nItems;
      return pVBLockNode;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockNode.uVBLockAddr=%02x corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockNode;
}*/

/*VBLockField*
VBLockNode_pField  ( UCHAR uVBLock, VBLockNode *pNode )
{
ASSERT(0);
    // Observe addressing model
    //VBLockNode oNode;
    char       *pField  = (char *)pNode
                        + sizeof(soNode)
                        - sizeof(soNode.u)
                        - sizeof(soNode.oVBLockField);
               uVBLock &= VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr16 )
      return (VBLockField *)(pField + sizeof(soNode.u.oNode16));
    if ( uVBLock == VBLock_Addr32 )
      return (VBLockField *)(pField + sizeof(soNode.u.oNode32));
    if ( uVBLock == VBLock_Addr08 )
      return (VBLockField *)(pField + sizeof(soNode.u.oNode08));
    ASSERT(0);
    return 0;
}*/

//VBLockName*
//VBLockNode_pName ( UCHAR uVBLock, VBLockNode *pNode )
//{
//ASSERT(0);
//    VBLockField *pField = VBLockNode_pField ( uVBLock, pNode );
//    return VBLockField_pName ( pField );
//}

//VBLockData*
//VBLockNode_pData ( UCHAR uVBLock, VBLockNode *pNode )
//{
//    VBLockField *pField = VBLockNode_pField ( uVBLock, pNode );
//    return VBLockField_pData ( uVBLock, pField );
//}

//VBLsize
//VBLockNode_Sizeof ( UCHAR uVBLock )
//{
//ASSERT(0);
    // Observe addressing model
    //VBLockNode oNode;
//    ASSERT(0);
//    UCHAR   uVBLockAddr = uVBLock & VBLock_AddrMask;
//    VBLsize nSizeof = sizeof(soNode)-sizeof(soNode.u)-sizeof(soNode.oVBLockField);
//    if ( uVBLockAddr == VBLock_Addr16 )
//      return nSizeof + sizeof(soNode.u.oNode16);
//    if ( uVBLockAddr == VBLock_Addr32 )
//      return nSizeof + sizeof(soNode.u.oNode32);
//    if ( uVBLockAddr == VBLock_Addr08 )
//      return nSizeof + sizeof(soNode.u.oNode08);
//    EVERR->Module ( __FUNCTION__)
//         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
//                  , uVBLock )
//         ->Throw ( );
//    return (VBLsize)~0;
//}

///////////////////////////////////////////////////////////////////////
//  VBLockAttr utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockAttr
//         structure pointers

void
VBLockAttr_Init  ( UCHAR uVBLock, VBLockAttr *pAttr, UCHAR uPermissions
                 , VBLaddr aParent )
{
    pAttr -> uPermissions = uPermissions;
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr64 )
    {
      ZeroMemory( &pAttr->u.oAttr64, sizeof(pAttr->u.oAttr64) );
      pAttr->u.oAttr64.aParent = (UINT64)aParent;
      ASSERT((aParent&0xFFFFFFFFFFFF)==aParent);
    }
    else if ( uVBLock == VBLock_Addr32 )
    {
      ZeroMemory( &pAttr->u.oAttr32, sizeof(pAttr->u.oAttr32) );
      pAttr->u.oAttr32.aParent = (UINT32)aParent;
      ASSERT((aParent&0xFFFFFFFF)==aParent);
    }
    else if ( uVBLock == VBLock_Addr16 )
    {
      ZeroMemory( &pAttr->u.oAttr16, sizeof(pAttr->u.oAttr16) );
      pAttr->u.oAttr16.aParent = (UINT16)aParent;
      ASSERT((aParent&0xFFFF)==aParent);
    }
    else if ( uVBLock == VBLock_Addr08 )
    {
      ZeroMemory( &pAttr->u.oAttr08, sizeof(pAttr->u.oAttr08) );
      pAttr->u.oAttr08.aParent = (UINT08)aParent;
      ASSERT((aParent&0xFF)==aParent);
    }
    else ASSERT(0);
}

VBLaddr
VBLockAttr_GetFirst ( UCHAR uVBLock, VBLockAttr *pAttr, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr64 )
      return pAttr->u.oAttr64.aFirst;
    if ( uVBLock == VBLock_Addr32 )
      return pAttr->u.oAttr32.aFirst;
    if ( uVBLock == VBLock_Addr16 )
      return pAttr->u.oAttr16.aFirst;
    if ( uVBLock == VBLock_Addr08 )
      return pAttr->u.oAttr08.aFirst;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockAttr.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0u;
}
VBLockAttr*
VBLockAttr_SetFirst ( UCHAR uVBLock, VBLockAttr *pVBLockAttr, VBLaddr aItemFirst )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock    == VBLock_Addr64              &&
         aItemFirst == (aItemFirst&0xFFFFFFFFFFFF)   )
    {
      pVBLockAttr->u.oAttr64.aFirst = (UINT64)aItemFirst;
      return pVBLockAttr;
    }
    if ( uVBLock    == VBLock_Addr32          &&
         aItemFirst == (aItemFirst&(UINT32)~0)   )
    {
      pVBLockAttr->u.oAttr32.aFirst = (UINT32)aItemFirst;
      return pVBLockAttr;
    }
    if ( uVBLock    == VBLock_Addr16          &&
         aItemFirst == (aItemFirst&(UINT16)~0)   )
    {
      pVBLockAttr->u.oAttr16.aFirst = (UINT16)aItemFirst;
      return pVBLockAttr;
    }
    if ( uVBLock    == VBLock_Addr08          &&
         aItemFirst == (aItemFirst&(UINT08)~0)   )
    {
      pVBLockAttr->u.oAttr08.aFirst = (UINT08)aItemFirst;
      return pVBLockAttr;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockAttr.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockAttr;
}
VBLaddr
VBLockAttr_GetLast  ( UCHAR uVBLock, VBLockAttr *pVBLockAttr, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr64 )
    {
      if ( pnItem )
        *pnItem = pVBLockAttr->u.oAttr64.nItems - 1;
      return pVBLockAttr->u.oAttr64.aLast;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      if ( pnItem )
        *pnItem = pVBLockAttr->u.oAttr32.nItems - 1;
      return pVBLockAttr->u.oAttr32.aLast;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      if ( pnItem )
        *pnItem = pVBLockAttr->u.oAttr16.nItems - 1;
      return pVBLockAttr->u.oAttr16.aLast;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      if ( pnItem )
        *pnItem = pVBLockAttr->u.oAttr08.nItems - 1;
      return pVBLockAttr->u.oAttr08.aLast;
    }
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockAttr.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockAttr*
VBLockAttr_SetLast ( UCHAR uVBLock, VBLockAttr *pVBLockAttr, VBLaddr aItemLast )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr64             &&
         aItemLast == (aItemLast&0xFFFFFFFFFFFF)   )
      pVBLockAttr->u.oAttr64.aLast = (UINT64)aItemLast;
    else if ( uVBLock   == VBLock_Addr32         &&
         aItemLast == (aItemLast&0xFFFFFFFF)   )
      pVBLockAttr->u.oAttr32.aLast = (UINT32)aItemLast;
    else if ( uVBLock   == VBLock_Addr16     &&
              aItemLast == (aItemLast&0xFFFF)   )
      pVBLockAttr->u.oAttr16.aLast = (UINT16)aItemLast;
    else if ( uVBLock   == VBLock_Addr08   &&
              aItemLast == (aItemLast&0xFF)   )
      pVBLockAttr->u.oAttr08.aLast = (UINT08)aItemLast;
    else 
      EVERR->Module ( "%s(%i)", __FUNCTION__)
           ->Message("Internal VBLockAttr.uVBLockAddr=%i corruption"
                    , uVBLock )
           ->Throw ( );
    return pVBLockAttr;
}
VBLelem
VBLockAttr_GetItems  ( UCHAR uVBLock, VBLockAttr *pVBLockAttr )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return pVBLockAttr->u.oAttr32.nItems;
    if ( uVBLock == VBLock_Addr64 )
      return pVBLockAttr->u.oAttr64.nItems;
    if ( uVBLock == VBLock_Addr16 )
      return pVBLockAttr->u.oAttr16.nItems;
    if ( uVBLock == VBLock_Addr08 )
      return pVBLockAttr->u.oAttr08.nItems;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockAttr.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0u;
}
VBLockAttr*
VBLockAttr_SetItems ( UCHAR uVBLock, VBLockAttr *pVBLockAttr, VBLelem nItems, bool bAbsolute )
{
    if ( !bAbsolute )
      nItems += VBLockAttr_GetItems ( uVBLock, pVBLockAttr );

    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 &&
         nItems  >= 0                )
      pVBLockAttr->u.oAttr32.nItems = (UINT32)nItems;
    //  `nItems >= 0`, not the `nItems == (nItems&0xFFFFFFFF)` this used to carry.
    //  VBLelem is `int`, so that mask was the identity and the guard could never
    //  fail -- and because the mask promoted the comparison to unsigned, a NEGATIVE
    //  count passed it and then sign-extended through (UINT64) into an enormous
    //  item count. The Addr08 and Addr16 twins below reject negatives correctly
    //  (0xFF and 0xFFFF do not mask to the identity), and Addr32 above already used
    //  `>= 0`; this branch was the only one of the four that was wrong.
    //  Four other sites in this file and MsgVBHeap.cpp spell the guard the same way
    //  and are CORRECT, which is why they are left alone: their operand is VBLaddr /
    //  VBLsize, i.e. UINT_PTR, so on x64 the mask genuinely bounds a 64-bit value to
    //  32 bits. C4389 fired here and not there for exactly that reason -- the
    //  signedness is the defect. Found by item 10's /W4 unification; see
    //  The release-readiness register, Stage D.
    else if ( uVBLock == VBLock_Addr64 &&
              nItems  >= 0                )
      pVBLockAttr->u.oAttr64.nItems = (UINT64)nItems;
    else if ( uVBLock == VBLock_Addr08 &&
              nItems  == (nItems&0xFF)    )
      pVBLockAttr->u.oAttr08.nItems = (UINT08)nItems;
    else if ( uVBLock == VBLock_Addr16  &&
              nItems  == (nItems&0xFFFF)   )
      pVBLockAttr->u.oAttr16.nItems = (UINT16)nItems;
    else 
      EVERR->Module ( __FUNCTION__)
           ->Message("Internal VBLockAttr.uVBLockAddr=%02x corruption"
                    , uVBLock )
           ->Throw ( );
    return pVBLockAttr;
}

VBLsize
VBLockAttr_Sizeof ( UCHAR uVBLock )
{
    // Observe addressing model
    //VBLockAttr oAttr;
    UCHAR   uVBLockAddr = uVBLock & VBLock_AddrMask;
    VBLsize nSizeof = sizeof(soAttr)-sizeof(soAttr.u);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof + sizeof(soAttr.u.oAttr32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof + sizeof(soAttr.u.oAttr64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof + sizeof(soAttr.u.oAttr16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof + sizeof(soAttr.u.oAttr08);
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockAttr.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}

VBLaddr
VBLockAttr_GetParent ( UCHAR uVBLock, VBLockAttr *pVBLockAttr )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockAttr->u.oAttr32.aParent;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pVBLockAttr->u.oAttr64.aParent;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockAttr->u.oAttr16.aParent;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockAttr->u.oAttr08.aParent;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0u;
}

VBLockAttr*
VBLockAttr_SetParent ( UCHAR uVBLock
                     , VBLockAttr *pVBLockAttr, VBLaddr aAttrParent )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr64                 &&
         aAttrParent == (aAttrParent&0xFFFFFFFFFFFF)    )
    {
      pVBLockAttr->u.oAttr64.aParent = (UINT64)aAttrParent;
      return pVBLockAttr;
    }
    if ( uVBLockAddr == VBLock_Addr32           &&
         aAttrParent == (aAttrParent&(UINT32)~0)   )
    {
      pVBLockAttr->u.oAttr32.aParent = (UINT32)aAttrParent;
      return pVBLockAttr;
    }
    if ( uVBLockAddr == VBLock_Addr16           &&
         aAttrParent == (aAttrParent&(UINT16)~0)   )
    {
      pVBLockAttr->u.oAttr16.aParent = (UINT16)aAttrParent;
      return pVBLockAttr;
    }
    if ( uVBLockAddr == VBLock_Addr08           &&
         aAttrParent == (aAttrParent&(UINT08)~0)   )
    {
      pVBLockAttr->u.oAttr08.aParent = (UINT08)aAttrParent;
      return pVBLockAttr;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockNode.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockAttr;
}

///////////////////////////////////////////////////////////////////////
//  VBLockDesc utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockDesc
//         structure pointers

void
VBLockDesc_Init  ( UCHAR uVBLock, VBLockDesc *pDesc, UCHAR uVBLockDesc
                 , VBLaddr aParent )
{
    pDesc -> uPermissions = uVBLockDesc;
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr64 )
    {
      ZeroMemory( &pDesc->u.oDesc64, sizeof(pDesc->u.oDesc64) );
      pDesc->u.oDesc64.aParent = (UINT64)aParent;
      return;
    }
    if ( uVBLock == VBLock_Addr32 )
    {
      ZeroMemory( &pDesc->u.oDesc32, sizeof(pDesc->u.oDesc32) );
      pDesc->u.oDesc32.aParent = (UINT32)aParent;
      return;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      ZeroMemory( &pDesc->u.oDesc16, sizeof(pDesc->u.oDesc16) );
      pDesc->u.oDesc16.aParent = (UINT16)aParent;
      return;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      ZeroMemory( &pDesc->u.oDesc08, sizeof(pDesc->u.oDesc08) );
      pDesc->u.oDesc08.aParent = (UINT08)aParent;
      return;
    }
    ASSERT(0);
}

VBLaddr
VBLockDesc_GetFirst ( UCHAR uVBLock, VBLockDesc *pDesc, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr32 )
      return pDesc->u.oDesc32.aFirst;
    if ( uVBLock == VBLock_Addr64 )
      return pDesc->u.oDesc64.aFirst;
    if ( uVBLock == VBLock_Addr16 )
      return pDesc->u.oDesc16.aFirst;
    if ( uVBLock == VBLock_Addr08 )
      return pDesc->u.oDesc08.aFirst;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0u;
}
VBLockDesc*
VBLockDesc_SetFirst ( UCHAR uVBLock, VBLockDesc *pVBLockDesc, VBLaddr aItemFirst )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock    == VBLock_Addr32          &&
         aItemFirst == (aItemFirst&(UINT32)~0)   )
    {
      pVBLockDesc->u.oDesc32.aFirst = (UINT32)aItemFirst;
      return pVBLockDesc;
    }
    if ( uVBLock    == VBLock_Addr64              &&
         aItemFirst == (aItemFirst&0xFFFFFFFFFFFF)   )
    {
      pVBLockDesc->u.oDesc64.aFirst = (UINT64)aItemFirst;
      return pVBLockDesc;
    }
    if ( uVBLock    == VBLock_Addr16          &&
         aItemFirst == (aItemFirst&(UINT16)~0)   )
    {
      pVBLockDesc->u.oDesc16.aFirst = (UINT16)aItemFirst;
      return pVBLockDesc;
    }
    if ( uVBLock    == VBLock_Addr08          &&
         aItemFirst == (aItemFirst&(UINT08)~0)   )
    {
      pVBLockDesc->u.oDesc08.aFirst = (UINT08)aItemFirst;
      return pVBLockDesc;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return pVBLockDesc;
}
VBLaddr
VBLockDesc_GetLast  ( UCHAR uVBLock, VBLockDesc *pVBLockDesc, int *pnItem )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
    {
      if ( pnItem )
        *pnItem = pVBLockDesc->u.oDesc32.nItems - 1;
      return pVBLockDesc->u.oDesc32.aLast;
    }
    if ( uVBLock == VBLock_Addr64 )
    {
      if ( pnItem )
        *pnItem = pVBLockDesc->u.oDesc64.nItems - 1;
      return pVBLockDesc->u.oDesc64.aLast;
    }
    if ( uVBLock == VBLock_Addr16 )
    {
      if ( pnItem )
        *pnItem = pVBLockDesc->u.oDesc16.nItems - 1;
      return pVBLockDesc->u.oDesc16.aLast;
    }
    if ( uVBLock == VBLock_Addr08 )
    {
      if ( pnItem )
        *pnItem = pVBLockDesc->u.oDesc08.nItems - 1;
      return pVBLockDesc->u.oDesc08.aLast;
    }
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockDesc*
VBLockDesc_SetLast ( UCHAR uVBLock, VBLockDesc *pVBLockDesc, VBLaddr aItemLast )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr32         &&
         aItemLast == (aItemLast&0xFFFFFFFF)   )
      pVBLockDesc->u.oDesc32.aLast = (UINT32)aItemLast;
    else if ( uVBLock   == VBLock_Addr64             &&
              aItemLast == (aItemLast&0xFFFFFFFFFFFF)   )
      pVBLockDesc->u.oDesc64.aLast = (UINT64)aItemLast;
    else if ( uVBLock   == VBLock_Addr16     &&
              aItemLast == (aItemLast&0xFFFF)   )
      pVBLockDesc->u.oDesc16.aLast = (UINT16)aItemLast;
    else if ( uVBLock   == VBLock_Addr08   &&
              aItemLast == (aItemLast&0xFF)   )
      pVBLockDesc->u.oDesc08.aLast = (UINT08)aItemLast;
    else 
      EVERR->Module ( "%s(%i)", __FUNCTION__)
           ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                    , uVBLock )
           ->Throw ( );
    return pVBLockDesc;
}
VBLelem
VBLockDesc_GetItems  ( UCHAR uVBLock, VBLockDesc *pVBLockDesc )
{
    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return pVBLockDesc->u.oDesc32.nItems;
    if ( uVBLock == VBLock_Addr64 )
      return pVBLockDesc->u.oDesc64.nItems;
    if ( uVBLock == VBLock_Addr16 )
      return pVBLockDesc->u.oDesc16.nItems;
    if ( uVBLock == VBLock_Addr08 )
      return pVBLockDesc->u.oDesc08.nItems;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0u;
}
VBLockDesc*
VBLockDesc_SetItems ( UCHAR uVBLock, VBLockDesc *pVBLockDesc, VBLelem nItems, bool bAbsolute )
{
    if ( !bAbsolute )
      nItems += VBLockDesc_GetItems ( uVBLock, pVBLockDesc );
ASSERT(nItems>=0);

    // Observe addressing model
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 &&
         nItems  >= 0                )
      pVBLockDesc->u.oDesc32.nItems = (UINT32)nItems;
    else if ( uVBLock == VBLock_Addr64 &&
              nItems  >= 0                )
      pVBLockDesc->u.oDesc64.nItems = (VBLelem)nItems;
    else if ( uVBLock == VBLock_Addr08 &&
         nItems  == (nItems&0xFF)    )
      pVBLockDesc->u.oDesc08.nItems = (UINT08)nItems;
    else if ( uVBLock == VBLock_Addr16  &&
              nItems  == (nItems&0xFFFF)   )
      pVBLockDesc->u.oDesc16.nItems = (UINT16)nItems;
    else 
      EVERR->Module ( __FUNCTION__)
           ->Message("Internal VBLockDesc.uVBLockAddr=%02x corruption"
                    , uVBLock )
           ->Throw ( );
    return pVBLockDesc;
}

VBLsize
VBLockDesc_Sizeof ( UCHAR uVBLock )
{
    // Observe addressing model
    //VBLockDesc oDesc;
    UCHAR   uVBLockAddr = uVBLock & VBLock_AddrMask;
    VBLsize nSizeof = sizeof(soDesc)-sizeof(soDesc.u);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof + sizeof(soDesc.u.oDesc32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof + sizeof(soDesc.u.oDesc64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof + sizeof(soDesc.u.oDesc16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof + sizeof(soDesc.u.oDesc08);
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}

VBLaddr
VBLockDesc_GetParent ( UCHAR uVBLock, VBLockDesc *pVBLockDesc )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockDesc->u.oDesc32.aParent;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pVBLockDesc->u.oDesc64.aParent;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockDesc->u.oDesc16.aParent;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockDesc->u.oDesc08.aParent;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLsize)~0;
}

VBLockDesc*
VBLockDesc_SetParent ( UCHAR uVBLock
                     , VBLockDesc *pVBLockDesc, VBLaddr aDescParent )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32           &&
         aDescParent == (aDescParent&(UINT32)~0)   )
    {
      pVBLockDesc->u.oDesc32.aParent = (UINT32)aDescParent;
      return pVBLockDesc;
    }
    if ( uVBLockAddr == VBLock_Addr64                &&
         aDescParent == (aDescParent&0xFFFFFFFFFFFF)    )
    {
      pVBLockDesc->u.oDesc64.aParent = (UINT64)aDescParent;
      return pVBLockDesc;
    }
    if ( uVBLockAddr == VBLock_Addr16           &&
         aDescParent == (aDescParent&(UINT16)~0)   )
    {
      pVBLockDesc->u.oDesc16.aParent = (UINT16)aDescParent;
      return pVBLockDesc;
    }
    if ( uVBLockAddr == VBLock_Addr08           &&
         aDescParent == (aDescParent&(UINT08)~0)   )
    {
      pVBLockDesc->u.oDesc08.aParent = (UINT08)aDescParent;
      return pVBLockDesc;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockDesc.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockDesc;
}

///////////////////////////////////////////////////////////////////////
//  VBLockItem utilities and helpers
//  NOTES: Self contained static functions that operate on VBLockItem
//         structure pointers

void
VBLockItem_Init ( UCHAR uVBLock, VBLockItem *pItem, UCHAR uItemType )
{
    pItem -> uItemType = uItemType;
    ASSERT(uItemType==VBLock_Field||uItemType==VBLock_Node||uItemType==VBLock_Vect||uItemType==VBLock_List||uItemType==VBLock_Data);
    uVBLock = uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      ZeroMemory( &pItem->ua.oItem32, sizeof(pItem->ua.oItem32) );
    else if ( uVBLock == VBLock_Addr16 )
      ZeroMemory( &pItem->ua.oItem16, sizeof(pItem->ua.oItem16) );
    else if ( uVBLock == VBLock_Addr08 )
      ZeroMemory( &pItem->ua.oItem08, sizeof(pItem->ua.oItem08) );
    else if ( uVBLock == VBLock_Addr64 )
      ZeroMemory( &pItem->ua.oItem64, sizeof(pItem->ua.oItem64) );
    else ASSERT(0);
}
VBLsize
VBLockItem_Sizeof ( UCHAR uVBLock, VBLockItem *pItem )
{
    VBLsize uSizeof = sizeof(VBLockItem)
                    - sizeof(pItem->ua) - sizeof(pItem->ut);
    if ( VBLockItem_IsField(pItem) )
      uSizeof += VBLockField_Sizeof(uVBLock,VBLockItem_pField(uVBLock,pItem));
    //else if ( VBLockItem_IsNode(pItem) )
    //  uSizeof += VBLockNode_Sizeof (VBLockItem_pNode (uVBLock,pItem));
    else
      return (VBLsize)~0;
  
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr64 )
      return uSizeof + sizeof(pItem->ua.oItem64);
    if ( uVBLockAddr == VBLock_Addr32 )
      return uSizeof + sizeof(pItem->ua.oItem32);
    if ( uVBLockAddr == VBLock_Addr16 )
      return uSizeof + sizeof(pItem->ua.oItem16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return uSizeof + sizeof(pItem->ua.oItem08);
    ASSERT(0);
    return (VBLsize)~0;
}

VBLsize
VBLockItem_Sizeof ( UCHAR uVBLock )
{
    // Observe addressing model
    //VBLockItem oItem;
    UCHAR   uVBLockAddr = uVBLock & VBLock_AddrMask;
    VBLsize nSizeof = sizeof(VBLockItem)-sizeof(VBLockItem::ua)-sizeof(VBLockItem::ut);
    //              - sizeof(oItem.oHdr.u);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeof + sizeof(VBLockItem::ua.oItem64);
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeof + sizeof(VBLockItem::ua.oItem32);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeof + sizeof(VBLockItem::ua.oItem16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeof + sizeof(VBLockItem::ua.oItem08);
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}

VBLsize
VBLockItem_Sizenn ( VBLock *pVBLock )
{
    VBLsize nSizenn = VBLock_Hdr_u_SizeNN(pVBLock)
                    - ( (VBLaddr)VBLock_pItem(pVBLock) - (VBLaddr)pVBLock );
    return nSizenn;
}

VBLsize
VBLockItem_Sizeof_ua ( VBLock *pVBLock )
{
    // Observe addressing model
    UCHAR     uVBLockAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 ) {
      ASSERT(sizeof(VBLockItem::ua.oItem32)<VBLock_Sizeof_Hdr_ud(pVBLock));
      return sizeof(VBLockItem::ua.oItem32); }
    if ( uVBLockAddr == VBLock_Addr16 ) {
      ASSERT(sizeof(VBLockItem::ua.oItem16)<VBLock_Sizeof_Hdr_ud(pVBLock));
      return sizeof(VBLockItem::ua.oItem16); }
    if ( uVBLockAddr == VBLock_Addr08 ) {
      ASSERT(sizeof(VBLockItem::ua.oItem08)<VBLock_Sizeof_Hdr_ud(pVBLock));
      return sizeof(VBLockItem::ua.oItem08); }
    if ( uVBLockAddr == VBLock_Addr64 ) {
      ASSERT(sizeof(VBLockItem::ua.oItem64)<VBLock_Sizeof_Hdr_ud(pVBLock));
      return sizeof(VBLockItem::ua.oItem64); }
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , pVBLock->oHdr.uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

VBLsize
VBLockItem_Sizeof_ut ( VBLock *pVBLock )
{
    // Essentually remainder of VBLock
    UCHAR   uVBLockAddr    = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    VBLsize nSizeof_VBLock = VBLock_Hdr_u_SizeNN ( pVBLock );
    VBLsize nSizeof_Hdr    = VBLock_Sizeof_Hdr ( pVBLock );
    VBLsize nSizeof_ua     = VBLockItem_Sizeof_ua  ( pVBLock );
    return  nSizeof_VBLock - nSizeof_Hdr - nSizeof_ua;
}

VBLsize
VBLockItem_Sizeud ( VBLock *pVBLock )
{
    // Observe addressing model
    VBLsize      nSizeud  = VBLock_Sizeof_Hdr_ud ( pVBLock );
    UCHAR     uVBLockAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return nSizeud - sizeof(VBLockItem::ua.oItem32);
    if ( uVBLockAddr == VBLock_Addr16 )
      return nSizeud - sizeof(VBLockItem::ua.oItem16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return nSizeud - sizeof(VBLockItem::ua.oItem08);
    if ( uVBLockAddr == VBLock_Addr64 )
      return nSizeud - sizeof(VBLockItem::ua.oItem64);
    EVERR->Module ( "%s(%x)", __FUNCTION__
                  , pVBLock->oHdr.uVBLockDefs )
         ->Message("Internal VBLock.uVBLock corruption" )
         ->Throw ( );
    return 0;
}

VBLaddr
VBLockItem_GetPrev ( UCHAR uVBLock, VBLockItem *pVBLockItem, int *pnItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( pnItem )
      (*pnItem)--;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockItem->ua.oItem32.aPrev;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pVBLockItem->ua.oItem64.aPrev;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockItem->ua.oItem16.aPrev;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockItem->ua.oItem08.aPrev;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockItem*
VBLockItem_SetPrev ( UCHAR uVBLock
                   , VBLockItem *pVBLockItem, VBLaddr aItemPrev )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32         &&
         aItemPrev   == (aItemPrev&(UINT32)~0)   )
    {
      pVBLockItem->ua.oItem32.aPrev = (UINT32)aItemPrev;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr64              &&
         aItemPrev   == (aItemPrev&0xFFFFFFFFFFFF)   )
    {
      pVBLockItem->ua.oItem64.aPrev = (UINT64)aItemPrev;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr16         &&
         aItemPrev   == (aItemPrev&(UINT16)~0)   )
    {
      pVBLockItem->ua.oItem16.aPrev = (UINT16)aItemPrev;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr08         &&
         aItemPrev   == (aItemPrev&(UINT08)~0)   )
    {
      pVBLockItem->ua.oItem08.aPrev = (UINT08)aItemPrev;
      return pVBLockItem;
    }
    EVERR->Module ( "%s(%i)", __FUNCTION__)
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockItem;
}

VBLaddr
VBLockItem_GetNext ( UCHAR uVBLock, VBLockItem *pVBLockItem, VBLelem *pnItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( pnItem )
      (*pnItem)++;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockItem->ua.oItem32.aNext;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pVBLockItem->ua.oItem64.aNext;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockItem->ua.oItem16.aNext;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockItem->ua.oItem08.aNext;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockItem*
VBLockItem_SetNext ( UCHAR uVBLock
                   , VBLockItem *pVBLockItem, VBLaddr aItemNext )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr64              &&
         aItemNext   == (aItemNext&0xFFFFFFFFFFFF)   )
    {
      pVBLockItem->ua.oItem64.aNext = (UINT64)aItemNext;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr32         &&
         aItemNext   == (aItemNext&(UINT32)~0)   )
    {
      pVBLockItem->ua.oItem32.aNext = (UINT32)aItemNext;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr16         &&
         aItemNext   == (aItemNext&(UINT16)~0)   )
    {
      pVBLockItem->ua.oItem16.aNext = (UINT16)aItemNext;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr08         &&
         aItemNext   == (aItemNext&(UINT08)~0)   )
    {
      pVBLockItem->ua.oItem08.aNext = (UINT08)aItemNext;
      return pVBLockItem;
    }
    EVERR->Module ( "%s(%i)", __FUNCTION__)
         ->Message("Internal VBLockItrm.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockItem;
}

VBLaddr
VBLockItem_GetExtra ( UCHAR uVBLock, VBLockItem *pVBLockItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pVBLockItem->ua.oItem64.aExtra;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockItem->ua.oItem32.aExtra;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockItem->ua.oItem16.aExtra;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockItem->ua.oItem08.aExtra;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockItem*
VBLockItem_SetExtra ( UCHAR uVBLock
                    , VBLockItem *pVBLockItem, VBLaddr aItemExtra )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32          &&
         aItemExtra  == (aItemExtra&(UINT32)~0)   )
    {
      pVBLockItem->ua.oItem32.aExtra = (UINT32)aItemExtra;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr64               &&
         aItemExtra  == (aItemExtra&0xFFFFFFFFFFFF)   )
    {
      pVBLockItem->ua.oItem64.aExtra = (UINT64)aItemExtra;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr16          &&
         aItemExtra  == (aItemExtra&(UINT16)~0)   )
    {
      pVBLockItem->ua.oItem16.aExtra = (UINT16)aItemExtra;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr08          &&
         aItemExtra  == (aItemExtra&(UINT08)~0)   )
    {
      pVBLockItem->ua.oItem08.aExtra = (UINT08)aItemExtra;
      return pVBLockItem;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockItem;
}

VBLaddr
VBLockItem_GetDescn ( UCHAR uVBLock, VBLockItem *pVBLockItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockItem->ua.oItem32.aDescn;
    if ( uVBLockAddr == VBLock_Addr64 )
      return (VBLaddr)pVBLockItem->ua.oItem64.aDescn;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockItem->ua.oItem16.aDescn;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockItem->ua.oItem08.aDescn;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockItrm.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockItem*
VBLockItem_SetDescn ( UCHAR uVBLock
                    , VBLockItem *pVBLockItem, VBLaddr aItemDescn )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32          &&
         aItemDescn  == (aItemDescn&(UINT32)~0)   )
    {
      pVBLockItem->ua.oItem32.aDescn = (UINT32)aItemDescn;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr64                 &&
         aItemDescn  == (aItemDescn&0xFFFFFFFFFFFFFF)   )
    {
      pVBLockItem->ua.oItem64.aDescn = (UINT64)aItemDescn;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr16          &&
         aItemDescn  == (aItemDescn&(UINT16)~0)   )
    {
      pVBLockItem->ua.oItem16.aDescn = (UINT16)aItemDescn;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr08          &&
         aItemDescn  == (aItemDescn&(UINT08)~0)   )
    {
      pVBLockItem->ua.oItem08.aDescn = (UINT08)aItemDescn;
      return pVBLockItem;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockItem;
}

VBLaddr
VBLockItem_GetStack ( UCHAR uVBLock, VBLockItem *pVBLockItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockItem->ua.oItem32.aStack;
    if ( uVBLockAddr == VBLock_Addr64 )
      return (VBLaddr)pVBLockItem->ua.oItem64.aStack;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockItem->ua.oItem16.aStack;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockItem->ua.oItem08.aStack;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockItem*
VBLockItem_SetStack ( UCHAR uVBLock
                    , VBLockItem *pVBLockItem, VBLaddr aItemStack )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr64                &&
         aItemStack  == (aItemStack&0xFFFFFFFFFFFF)   )
    {
      pVBLockItem->ua.oItem64.aStack = (UINT64)aItemStack;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr16          &&
         aItemStack  == (aItemStack&(UINT16)~0)   )
    {
      pVBLockItem->ua.oItem16.aStack = (UINT16)aItemStack;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr32          &&
         aItemStack  == (aItemStack&(UINT32)~0)   )
    {
      pVBLockItem->ua.oItem32.aStack = (UINT32)aItemStack;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr08          &&
         aItemStack  == (aItemStack&(UINT08)~0)   )
    {
      pVBLockItem->ua.oItem08.aStack = (UINT08)aItemStack;
      return pVBLockItem;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return 0;
}

VBLaddr
VBLockItem_GetParent ( UCHAR uVBLock, VBLockItem *pVBLockItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pVBLockItem->ua.oItem32.aParent;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pVBLockItem->ua.oItem64.aParent;
    if ( uVBLockAddr == VBLock_Addr16 )
      return pVBLockItem->ua.oItem16.aParent;
    if ( uVBLockAddr == VBLock_Addr08 )
      return pVBLockItem->ua.oItem08.aParent;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBLockItem*
VBLockItem_SetParent ( UCHAR uVBLock
                     , VBLockItem *pVBLockItem, VBLaddr aItemParent )
{
    // Observe addressing model
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32           &&
         aItemParent == (aItemParent&(UINT32)~0)   )
    {
      pVBLockItem->ua.oItem32.aParent = (UINT32)aItemParent;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr64               &&
         aItemParent == (aItemParent&0xFFFFFFFFFFFF)   )
    {
      pVBLockItem->ua.oItem64.aParent = (UINT64)aItemParent;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr16           &&
         aItemParent == (aItemParent&(UINT16)~0)   )
    {
      pVBLockItem->ua.oItem16.aParent = (UINT16)aItemParent;
      return pVBLockItem;
    }
    if ( uVBLockAddr == VBLock_Addr08           &&
         aItemParent == (aItemParent&(UINT08)~0)   )
    {
      pVBLockItem->ua.oItem08.aParent = (UINT08)aItemParent;
      return pVBLockItem;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBLockItem;
}

void*
VBLockItem_vpu ( UCHAR uVBLock, VBLockItem *pItem )
{
    char *pud = (char *)pItem
              + sizeof(VBLockItem)
              - sizeof(pItem->ua) - sizeof(pItem->ut);
    UCHAR uVBLockAddr = uVBLock & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32 )
      return pud += sizeof(pItem->ua.oItem32);
    if ( uVBLockAddr == VBLock_Addr64 )
      return pud += sizeof(pItem->ua.oItem64);
    if ( uVBLockAddr == VBLock_Addr64 )
      return pud += sizeof(pItem->ua.oItem64);
    if ( uVBLockAddr == VBLock_Addr16 )
      return pud += sizeof(pItem->ua.oItem16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return pud += sizeof(pItem->ua.oItem08);
    return 0;
}

/*bool
VBLockItem_IsNode ( const VBLockItem *pVBLockItem )
{
    if ( pVBLockItem == NULL )
      return false;
    UCHAR uVBLock = pVBLockItem->uItemType;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Node )
      return true;
    return false;
}
VBLockNode*
VBLockItem_pNode   ( UCHAR uVBLock, VBLockItem *pItem )
{
    return (VBLockNode *)VBLockItem_vpu ( uVBLock, pItem );
}*/

bool
VBLockItem_IsList ( const VBLockItem *pVBLockItem )
{
    if ( pVBLockItem == NULL )
      return false;
    UCHAR uVBLock = pVBLockItem->uItemType;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_List )
      return true;
    return false;
}
VBLockList*
VBLockItem_pList   ( UCHAR uVBLock, VBLockItem *pItem )
{
    return (VBLockList *)VBLockItem_vpu ( uVBLock, pItem );
}

bool
VBLockItem_IsVect ( const VBLockItem *pVBLockItem )
{
    if ( pVBLockItem == NULL )
      return false;
    UCHAR uVBLock = pVBLockItem->uItemType;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Vect )
      return true;
    return false;
}
VBLockVect*
VBLockItem_pVect   ( UCHAR uVBLock, VBLockItem *pItem )
{
    return (VBLockVect *)VBLockItem_vpu ( uVBLock, pItem );
}
VBLockVect*
VBLock_pVect ( const VBLock *pVBLock )
{
    UCHAR uVBLock = pVBLock->oHdr.uVBLockDefs;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Vect )
      return (VBLockVect *)VBLock_pud(pVBLock);
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Item )
      return VBLockItem_pVect ( uVBLock
                              ,(VBLockItem *)VBLock_pud(pVBLock) );
    return (VBLockVect *)0;
}

//
//  The VBLockItem_p* resolvers below each recognise four item types and end
//  in a fall-through the author marked ASSERT(0). ASSERT compiles out of
//  Release (mfcshim.h), so in Release they returned a silent NULL -- and
//  uItemType is WIRE data, four recognised values out of sixteen the type
//  field can hold, so the fall-through is reachable by any peer.
//
//  None of the seventeen call sites tests the result. P2PmsgObject_pData
//  hands it straight to VBLockData_IsChained, which dereferences it:
//  `p2p_fuzzframe 0x5EEDF00D --replay 0 36` faults there with pData=NULL on
//  an item block whose uVBLockDefs is 0xC6 (VBLock_Item, Addr32, Alloc|Linked)
//  carrying an unrecognised uItemType.
//
//  Report it as corruption and throw, the way VBLock_pData and
//  VBLock_ud_vpData already do for the same class of damage -- the framing
//  path rejects the frame instead of dereferencing NULL. A null pItem lands
//  here too: every VBLockItem_Is* predicate returns false for NULL.
//
static void
VBLockItem_ThrowUnknown ( const char *pszFn, const VBLockItem *pItem )
{
    if ( pItem == nullptr )
      EVERR->Module ( "%s", pszFn )
           ->Message( "Null VBLockItem: item address does not resolve in the image" )
           ->Throw ( );
    EVERR->Module ( "%s(%x)", pszFn, (unsigned)pItem->uItemType )
         ->Message( "Unknown VBLockItem type 0x%02x: item block is corrupt"
                  , (unsigned)(pItem->uItemType & VBLock_TypeMask) )
         ->Throw ( );
}

bool
VBLockItem_IsField ( const VBLockItem *pVBLockItem )
{
    if ( pVBLockItem == NULL )
      return false;
    UCHAR uVBLock = pVBLockItem->uItemType;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Field )
      return true;
    return false;
}
VBLockField*
VBLockItem_pField  ( UCHAR uVBLock, VBLockItem *pItem )
{
    if ( VBLockItem_IsField(pItem) )
      return (VBLockField *)VBLockItem_vpu(uVBLock,pItem);
    //if ( VBLockItem_IsNode(pItem) )
    //  return VBLockNode_pField ( uVBLock
    //                      , (VBLockNode *)VBLockItem_vpu(uVBLock,pItem) );
    if ( VBLockItem_IsList(pItem) )
      return VBLockList_pField ( uVBLock
                          , (VBLockList *)VBLockItem_vpu(uVBLock,pItem) );
    if ( VBLockItem_IsVect(pItem) )
      return VBLockVect_pField ( uVBLock
                          , (VBLockVect *)VBLockItem_vpu(uVBLock,pItem) );
    VBLockItem_ThrowUnknown ( __FUNCTION__, pItem );
    return (VBLockField *)0;
}

VBLockName*
VBLockItem_pName ( UCHAR uVBLock, VBLockItem *pItem )
{
    if ( VBLockItem_IsField(pItem) )
      return VBLockField_pName ( VBLockItem_pField(uVBLock,pItem) );
    //if ( VBLockItem_IsNode(pItem) )
    //  return VBLockNode_pName ( uVBLock
    //                          , VBLockItem_pNode(uVBLock,pItem) );
    if ( VBLockItem_IsList(pItem) )
      return VBLockList_pName ( uVBLock
                              , VBLockItem_pList(uVBLock,pItem) );
    if ( VBLockItem_IsVect(pItem) )
      return VBLockVect_pName ( uVBLock
                              , VBLockItem_pVect(uVBLock,pItem) );
    VBLockItem_ThrowUnknown ( __FUNCTION__, pItem );
    return 0;
}

bool
VBLockItem_IsData ( const VBLockItem *pVBLockItem )
{
    if ( pVBLockItem == NULL )
      return false;
    UCHAR uVBLock = pVBLockItem->uItemType;
    if ( (uVBLock&VBLock_TypeMask) == VBLock_Data )
      return true;
    return false;
}
//  The frozen two-argument form. Kept EXACTLY as it was, forwarding, because it
//  is an exported symbol: giving it the owner as a defaulted third parameter is
//  source-compatible and not binary-compatible -- it changes the decorated name,
//  which retires an export somebody may already have linked. The owner-bounded
//  work is VBLockItem_pDataChk below, which is not exported.
VBLockData*
VBLockItem_pData ( UCHAR uVBLock, VBLockItem *pItem )
{
    return VBLockItem_pDataChk ( uVBLock, pItem, nullptr );
}
VBLockData*
VBLockItem_pDataChk ( UCHAR uVBLock, VBLockItem *pItem, const VBLock *pOwner )
{
    if ( VBLockItem_IsField(pItem) )
      return VBLockField_pData ( uVBLock, VBLockItem_pField(uVBLock,pItem)
                               , pOwner );
    //if ( VBLockItem_IsNode(pItem) )
    //  return VBLockNode_pData ( uVBLock
    //                          , VBLockItem_pNode(uVBLock,pItem) );
    if ( VBLockItem_IsData(pItem) )
      return (VBLockData *)VBLockItem_vpu ( uVBLock, pItem );
    if ( VBLockItem_IsList(pItem) )
      return VBLockList_pData ( uVBLock
                              , VBLockItem_pList(uVBLock,pItem) );
    if ( VBLockItem_IsVect(pItem) )
      return VBLockVect_pData ( uVBLock
                              , VBLockItem_pVect(uVBLock,pItem) );
    VBLockItem_ThrowUnknown ( __FUNCTION__, pItem );
    return 0;
}

