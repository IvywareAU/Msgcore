// Copyright © 2005-2015, 2023, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2Pmsg Virtual-Blocked-Heap utitities
//  NOTES: Manage the VBHeap data structures
//
#include "stdafx.h"
#include "P2PmsgVBLock.h"
#include "MsgVBHeap.h"
#include "Msgexception.h"
#include <ASSERT.h>
#include <atomic>
//  Sizing VBLockHdr's, eliminates need to contract empty objects
const VBLockHdr  soHdr  = { 0 };
const VBLockName soName = { 0 };
const VBLockData soData = { 0 };
const VBLockList soList = { 0 };
const VBLockVect soVect = { 0 };
//const VBLockNode soNode = { 0 };
const VBLockAttr soAttr = { 0 };
const VBLockDesc soDesc = { 0 };

#define XCtrl_Hdr

///////////////////////////////////////////////////////////////////////
//  VBHeap container, definitions and helpers
//  NOTES: Fundamental unit of memory allocation and subsequent 
//         fragmentation.  De-fragmentaion occurs through collation
//         of free'd adjacent VBHeapBlock's

#pragma pack(push,1)
template < typename SIZE__ >
struct VBHeapNN__
{   // VBHeap blocks, minimum unit of allocation
    SIZE__  nPrev;
    SIZE__  nNext;
    char    cData;
};
typedef VBHeapNN__ <UINT08> VBHeap08;
typedef VBHeapNN__ <UINT16> VBHeap16;
typedef VBHeapNN__ <UINT32> VBHeap32;
typedef VBHeapNN__ <UINT64> VBHeap64;

typedef struct VBHeap_
{                                      // Universal header
    VBLockHdr    oHdr;                 // VBLockHdr is common to both VBHeap & VBLock's
    union
    {                                  // Heap management
      VBHeap08    oHeap08;             // According to VBHeap address mode
      VBHeap16    oHeap16;
      VBHeap32    oHeap32;
      VBHeap64    oHeap64;
    } ud;                              // VBLock equivalent is VBLock::ud
} VBHeap;
#pragma pack(pop)

#define VBHeap_IsLinked(pVBHeap) VBLock_IsLinked((VBLock*)pVBHeap)
#define VBHeap_IsFree(pVBHeap) VBLock_IsFree((VBLock*)pVBHeap)
#define VBHeap_IsAddr(pVBHeap,uAddrType) VBLock_IsAddr((VBLock*)pVBHeap,uAddrType)
#define VBHeap_IsAlloc(pVBHeap) VBLock_IsAlloc((VBLock*)pVBHeap)
#define VBHeap_Sizenn(pVBHeap) VBLock_Hdr_u_SizeNN((VBLock*)pVBHeap)

#define VBList2PhysVBHeap(hVBList,aVBHeap) ((VBHeap*)P2PmsgHeap_Block2Phys(hVBList,aVBHeap))


void*
VBHeap_pud ( VBHeap *pVBHeap )
{
    char *pud = (char *)pVBHeap + sizeof(pVBHeap->oHdr) - sizeof(pVBHeap->oHdr.u);
    UCHAR uVBLockAddr = pVBHeap->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr64 )
      return pud += sizeof(pVBHeap->oHdr.u.nSize64);
    if ( uVBLockAddr == VBLock_Addr32 )
      return pud += sizeof(pVBHeap->oHdr.u.nSize32);
    if ( uVBLockAddr == VBLock_Addr16 )
      return pud += sizeof(pVBHeap->oHdr.u.nSize16);
    if ( uVBLockAddr == VBLock_Addr08 )
      return pud += sizeof(pVBHeap->oHdr.u.nSize08);
    ASSERT(0);
    return nullptr;
}

void
VBHeap_Init ( VBHeap *pVBHeap, UCHAR uVBLockDefs, VBLaddr nSizenn )
{
    // Observe addressing model
    pVBHeap -> oHdr.uVBLockDefs = uVBLockDefs | VBLock_Alloc;
    VBLaddr uAddr = uVBLockDefs & VBLock_AddrMask;
    if ( uAddr == VBLock_Addr32         &&
         nSizenn == (nSizenn&0xFFFFFFFF)    )
    {
      auto *pVBHeap32 = static_cast<VBHeap32*>( VBHeap_pud(pVBHeap) );
      pVBHeap -> oHdr.u.nSize32  = UINT32(nSizenn);
      pVBHeap32 -> nPrev = 0;
      pVBHeap32 -> nNext = 0;
    }
    else if ( uAddr == VBLock_Addr64 )
    {
      auto *pVBHeap64 = static_cast<VBHeap64*>(VBHeap_pud(pVBHeap));
      pVBHeap -> oHdr.u.nSize64  = UINT64(nSizenn);
      pVBHeap64 -> nPrev = 0;
      pVBHeap64 -> nNext = 0;
    }
    else if ( uAddr == VBLock_Addr16      &&
              nSizenn == (nSizenn&0xFFFF)    )
    {
      auto *pVBHeap16 = static_cast<VBHeap16*>( VBHeap_pud(pVBHeap) );
      pVBHeap -> oHdr.u.nSize16  = UINT16(nSizenn);
      pVBHeap16 -> nPrev = 0;
      pVBHeap16 -> nNext = 0;
    }
    else if (uAddr == VBLock_Addr08      &&
             nSizenn == (nSizenn & 0xFF)    )
    {
      auto* pVBHeap08 = static_cast<VBHeap08*>(VBHeap_pud(pVBHeap));
      pVBHeap->oHdr.u.nSize08 = UINT08(nSizenn);
      pVBHeap08->nPrev = 0;
      pVBHeap08->nNext = 0;
    }
    else EVERR->Module ( "%s(%x, %llu)", __FUNCTION__
                       , uAddr, nSizenn )
              ->Message("Internal VBLock.uVBLock corruption" )
              ->Throw ( );
}

VBLaddr
VBHeap_GetPrev ( VBHeap *pVBHeap, VBLelem *pnItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = pVBHeap->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( pnItem )
      (*pnItem)--;
    if ( uVBLockAddr == VBLock_Addr64 )
      return (VBLaddr)((VBHeap64 *)VBHeap_pud(pVBHeap)) -> nPrev;
    if ( uVBLockAddr == VBLock_Addr32 )
      return ((VBHeap32 *)VBHeap_pud(pVBHeap)) -> nPrev;
    if ( uVBLockAddr == VBLock_Addr16 )
      return ((VBHeap16 *)VBHeap_pud(pVBHeap)) -> nPrev;
    if ( uVBLockAddr == VBLock_Addr08 )
      return ((VBHeap08 *)VBHeap_pud(pVBHeap)) -> nPrev;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBLock.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBHeap*
VBHeap_SetPrev ( VBHeap *pVBHeap, VBLaddr aItemPrev )
{
    // Observe addressing model
    UCHAR uVBLockAddr = pVBHeap->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32         &&
         aItemPrev   == (aItemPrev&(UINT32)~0)   )
    {
      ((VBHeap32 *)VBHeap_pud(pVBHeap))->nPrev = static_cast<UINT32>(aItemPrev);
      return pVBHeap;
    }
    if ( uVBLockAddr == VBLock_Addr64         &&
         aItemPrev   == (aItemPrev&(UINT64)~0)   )
    {
      ((VBHeap64 *)VBHeap_pud(pVBHeap))->nPrev = static_cast<UINT64>(aItemPrev);
      return pVBHeap;
    }
    if ( uVBLockAddr == VBLock_Addr16         &&
         aItemPrev   == (aItemPrev&(UINT16)~0)   )
    {
      ((VBHeap16 *)VBHeap_pud(pVBHeap))->nPrev = static_cast<UINT16>(aItemPrev);
      return pVBHeap;
    }
    if ( uVBLockAddr == VBLock_Addr08         &&
         aItemPrev   == (aItemPrev&(UINT08)~0)   )
    {
      ((VBHeap08 *)VBHeap_pud(pVBHeap))->nPrev = static_cast<UINT08>(aItemPrev);
      return pVBHeap;
    }
    EVERR->Module ( "%s(%llu)", __FUNCTION__)
         ->Message("Internal VBLock.uVBLockAddr=%i corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return nullptr;
}

VBLaddr
VBHeap_GetNext ( VBHeap *pVBHeap, VBLelem *pnItem )
{
    // Observe addressing model
    UCHAR uVBLockAddr = pVBHeap->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( pnItem )
      (*pnItem)++;
    if ( uVBLockAddr == VBLock_Addr32 )
      return ((VBHeap32 *)VBHeap_pud(pVBHeap)) -> nNext;
    if ( uVBLockAddr == VBLock_Addr64 )
      return (VBLaddr)((VBHeap64 *)VBHeap_pud(pVBHeap)) -> nNext;
    if ( uVBLockAddr == VBLock_Addr16 )
      return ((VBHeap16 *)VBHeap_pud(pVBHeap)) -> nNext;
    if ( uVBLockAddr == VBLock_Addr08 )
      return ((VBHeap08 *)VBHeap_pud(pVBHeap)) -> nNext;
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockItem.uVBLockAddr=%llu corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return (UINT)~0;
}
VBHeap*
VBHeap_SetNext ( VBHeap *pVBHeap, VBLaddr aItemNext )
{
    // Observe addressing model
    UCHAR uVBLockAddr = pVBHeap->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLockAddr == VBLock_Addr32         &&
         aItemNext   == (aItemNext&(UINT32)~0)   )
    {
      ((VBHeap32 *)VBHeap_pud(pVBHeap)) -> nNext = static_cast<UINT32>(aItemNext);
      return pVBHeap;
    }
    if ( uVBLockAddr == VBLock_Addr64         &&
         aItemNext   == (aItemNext&(UINT64)~0)   )
    {
      ((VBHeap64 *)VBHeap_pud(pVBHeap)) -> nNext = static_cast<UINT64>(aItemNext);
      return pVBHeap;
    }
    if ( uVBLockAddr == VBLock_Addr16         &&
         aItemNext   == (aItemNext&(UINT16)~0)   )
    {
      ((VBHeap16 *)VBHeap_pud(pVBHeap)) -> nNext = static_cast<UINT16>(aItemNext);
      return pVBHeap;
    }
    if ( uVBLockAddr == VBLock_Addr08          &&
         aItemNext   == (aItemNext&(UINT08)~0)   )
    {
      ((VBHeap08 *)VBHeap_pud(pVBHeap)) -> nNext = static_cast<UINT08>(aItemNext);
      return pVBHeap;
    }
    EVERR->Module ( __FUNCTION__ )
         ->Message("Internal VBLockNode.uVBLockAddr=%llu corruption"
                  , uVBLockAddr )
         ->Throw ( );
    return pVBHeap;
}

///////////////////////////////////////////////////////////////////////
//  VBHeapRoot container, definitions and helpers
//  NOTES: Fundamental unit of memory allocation and subsequent 
//         fragmentation.  De-fragmentaion occurs through collation
//         of free'd adjacent VBHeapBlock's
#pragma pack(push,1)
template < typename SIZE__ >
struct VBHeap_CtrlKeysNN__
{   // Serialised VBHeap control keys and counters
    SIZE__  aAlloc;                  // Address of first allocated VBLock
    SIZE__  aAllocSize;              // Cummulative size of all allocated VBLock's
    SIZE__  nAllocItems;             // Number of allocated entries
    SIZE__  aFree;                   // Address of first free VBLock
    SIZE__  aFreeLast;               // Address of last  free VBLock
    SIZE__  aFreeSize;               // Cummulative size of all free VBLock's
    SIZE__  nFreeItems;              // Number of free items
    SIZE__  aSpare8;
};
typedef VBHeap_CtrlKeysNN__ <UINT08>  VBHeap_CtrlKeys08;
typedef VBHeap_CtrlKeysNN__ <UINT16>  VBHeap_CtrlKeys16;
typedef VBHeap_CtrlKeysNN__ <UINT32>  VBHeap_CtrlKeys32;
typedef VBHeap_CtrlKeysNN__ <UINT64>  VBHeap_CtrlKeys64;
typedef VBHeap_CtrlKeysNN__ <VBLaddr> VBHeap_CtrlKeys;

typedef struct VBHeap_CtrlKeys_
{
    UINT16    nSize;                   // Size of this header
    UINT16    uVBLock;                 // Addressing mode
    union
    {
      VBHeap_CtrlKeys08 oRoot08;       // Control keys
      VBHeap_CtrlKeys16 oRoot16;
      VBHeap_CtrlKeys32 oRoot32;
      VBHeap_CtrlKeys64 oRoot64;
      VBHeap_CtrlKeys   oRoot;         // Compiled bit size (default)
    } ud;                              // Contents, refer uVBLock
} VBHeapRoot;
#pragma pack(pop)

//
//  VBHeapIOMAGE containers and definitions
//  NOTES: Private root for VBHeapIOMAGE structure, header mimics VBHeapIOmage
//       : Dynamic size dependant upon addressing mode.  Optimised for remote
//         exchanges and P2PeerMsg refections etc.
#pragma pack(push,1)
typedef struct
{
    struct
    {
      UINT32  uiSync1;                 // size | addressing | endian sentinel
      UINT32  uiSync2;                 // Compliment of above
    } oSync;                           // Mimics VBListIOmage (see P2PmsgBSTR.h)

    VBHeapRoot oRoot;                  // Root header
} VBHeapIOMAGE;
//#define VBHeapIOmage VBHeapIOMAGE
#pragma pack(pop)

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockHeap manipulation
void
P2PmsgHeap_InitIOMAGE ( VBHeapIOMAGE *pIOMAGE, VBLsize nSizeofIOMAGE, UCHAR uAddrType );
bool
P2PmsgHeap_AssertValidFree ( P2PmsgHANDLE hP2PmsgHeap, VBLaddr aVBLock );
void
VBHeapRoot_Init    ( VBHeapRoot *pRoot, UCHAR uVBLock, VBLsize nSizenn );
VBLsize
VBHeapRoot_Sizeof  ( VBHeapRoot *pRoot ) { return pRoot->nSize; }
//  The image bytes a root header occupies under a given addressing mode
//  NOTES: VBHeapRoot_Sizeof reads nSize out of the root itself, which is the
//         wrong source when the root has not yet been shown to BE there -- the
//         field sits inside the very bytes in question. This derives the same
//         figure from the addressing width, which arrives in the sync word and
//         is range-checked before anything else is read. Same arithmetic as
//         VBHeapRoot_Init, which is what writes nSize in the first place.
static VBLsize
VBHeapRoot_Sizeof_Addrnn ( UCHAR uAddrType ) noexcept
{
    const VBLsize nHdr = sizeof(VBHeapRoot) - sizeof(VBHeapRoot::ud);
    uAddrType = uAddrType & VBLock_AddrMask;
    if ( uAddrType == VBLock_Addr08 ) return nHdr + sizeof(VBHeapRoot::ud.oRoot08);
    if ( uAddrType == VBLock_Addr16 ) return nHdr + sizeof(VBHeapRoot::ud.oRoot16);
    if ( uAddrType == VBLock_Addr32 ) return nHdr + sizeof(VBHeapRoot::ud.oRoot32);
    if ( uAddrType == VBLock_Addr64 ) return nHdr + sizeof(VBHeapRoot::ud.oRoot64);
    return nHdr + sizeof(VBHeapRoot::ud);   // unknown mode: demand the widest
}
VBLaddr
VBHeapRoot_GetAlloc ( const VBHeapRoot *pRoot, int *pnItem = 0 );
VBHeapRoot*
VBHeapRoot_SetAlloc ( VBHeapRoot *pRoot, VBLaddr aAlloc );
VBLsize
VBHeapRoot_GetAllocSize ( const VBHeapRoot *pRoot );
VBLsize
VBHeapRoot_SetAllocSize ( VBHeapRoot *pRoot, VBLsize aAllocSize, bool bAbsolute = false );
VBLelem
VBHeapRoot_GetAllocItems( const VBHeapRoot *pRoot );
VBLelem
VBHeapRoot_SetAllocItems( VBHeapRoot *pRoot, VBLelem nItems, bool bAbsolute = false );
VBLaddr
VBHeapRoot_GetFree ( const VBHeapRoot *pRoot, VBLelem *pnItem = 0 );
VBHeapRoot*
VBHeapRoot_SetFree ( VBHeapRoot *pRoot, VBLaddr aFree );
VBLaddr
VBHeapRoot_GetFreeLast ( const VBHeapRoot *pRoot, VBLelem *pnItem = 0 );
VBHeapRoot*
VBHeapRoot_SetFreeLast ( VBHeapRoot *pRoot, VBLaddr aFreeLast );
VBLsize
VBHeapRoot_GetFreeSize ( const VBHeapRoot *pRoot );
VBLsize
VBHeapRoot_SetFreeSize ( VBHeapRoot *pRoot, VBLaddr aFreeSize, bool bAbsolute = false );
VBLelem
VBHeapRoot_GetFreeItems( const VBHeapRoot *pRoot );
VBLelem
VBHeapRoot_SetFreeItems( VBHeapRoot *pRoot, VBLelem nItems, bool bAbsolute = false );

void
VBHeapRoot_Init ( VBHeapRoot *pRoot, UCHAR uVBLock, VBLsize nSizenn )
{
    pRoot -> uVBLock = uVBLock;
    uVBLock = uVBLock & VBLock_AddrMask;
    pRoot -> uVBLock = uVBLock;
    pRoot -> nSize = sizeof(VBHeapRoot) - sizeof(VBHeapRoot::ud);
    if ( uVBLock == VBLock_Addr32        &&
         nSizenn == (nSizenn&0xFFFFFFFF)    )
    {
      ZeroMemory( &pRoot->ud.oRoot32, sizeof(pRoot->ud.oRoot32) );
      pRoot -> nSize += sizeof(VBHeapRoot::ud.oRoot32);
      return;
    }
    if ( uVBLock == VBLock_Addr64            &&
         nSizenn == (nSizenn&0xFFFFFFFFFFFF)    )
    {
      ZeroMemory( &pRoot->ud.oRoot64, sizeof(pRoot->ud.oRoot64) );
      pRoot -> nSize += sizeof(VBHeapRoot::ud.oRoot64);
      return;
    }
    if ( uVBLock == VBLock_Addr16    &&
         nSizenn == (nSizenn&0xFFFF)    )
    {
      ZeroMemory( &pRoot->ud.oRoot16, sizeof(pRoot->ud.oRoot16) );
      pRoot -> nSize += sizeof(VBHeapRoot::ud.oRoot16);
      return;
    }
    if ( uVBLock == VBLock_Addr08  &&
         nSizenn == (nSizenn&0xFF)    )
    {
      ASSERT(0);
      ZeroMemory( &pRoot->ud.oRoot08, sizeof(pRoot->ud.oRoot08) );
      pRoot -> nSize += sizeof(VBHeapRoot::ud.oRoot08);
      return;
    }
    EVERR->Module ("%s(pRoot,uVBLock=%02x,nSizenn=%i", __FUNCTION__
                  , uVBLock, nSizenn )
         ->Message("Invalid addressing parameters")
         ->Throw();
}

VBLsize
VBHeapRoot_GetAlloc ( const VBHeapRoot *pRoot, int *pnItem )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.aAlloc;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.aAlloc;
    if ( uVBLock == VBLock_Addr64 )
      return pRoot->ud.oRoot64.aAlloc;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.aAlloc;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}
VBHeapRoot*
VBHeapRoot_SetAlloc ( VBHeapRoot *pRoot, VBLaddr aAlloc )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock  == VBLock_Addr32       &&
         aAlloc   == (aAlloc&0xFFFFFFFF)    )
    { pRoot->ud.oRoot32.aAlloc = static_cast<UINT32>(aAlloc); return pRoot; }
    if ( uVBLock  == VBLock_Addr16   &&
         aAlloc   == (aAlloc&0xFFFF)    )
    { pRoot->ud.oRoot16.aAlloc = static_cast<UINT16>(aAlloc); return pRoot; }
    if ( uVBLock == VBLock_Addr08 &&
         aAlloc  == (aAlloc&0xFF)    )
    { pRoot->ud.oRoot08.aAlloc = static_cast<UINT08>(aAlloc); return pRoot; }
    if ( uVBLock  == VBLock_Addr64               &&
         aAlloc   == (aAlloc&0xFFFFFFFFFFFFFFFF)    )
    { pRoot->ud.oRoot64.aAlloc = static_cast<UINT64>(aAlloc); return pRoot; }
    ASSERT (0);
    EVERR->Module ( "%s(pRoot=%lp, aAlloc=%Ii)", __FUNCTION__
                  , pRoot, aAlloc )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return 0;
}

VBLsize
VBHeapRoot_GetAllocSize ( const VBHeapRoot *pRoot )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.aAllocSize;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.aAllocSize;
    if ( uVBLock == VBLock_Addr64 )
      return pRoot->ud.oRoot64.aAllocSize;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.aAllocSize;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0;
}
VBLsize
VBHeapRoot_SetAllocSize ( VBHeapRoot *pRoot, VBLsize aAllocSize, bool bAbsolute )
{
    if ( bAbsolute && aAllocSize < 0 )
      EVERR->Module ( "%s(pRoot,aAllocSize=%i,bAbsolute=%i)", __FUNCTION__
                    , aAllocSize, bAbsolute )
           ->Message("Invalid parameter (nItems<0)" )
           ->Throw ( );
    else if ( !bAbsolute )
      aAllocSize += VBHeapRoot_GetAllocSize ( pRoot );

    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr32          &&
         aAllocSize == (aAllocSize&0xFFFFFFFF)    )
    { pRoot->ud.oRoot32.aAllocSize = (UINT32)aAllocSize; return pRoot->ud.oRoot32.aAllocSize; }
    if ( uVBLock   == VBLock_Addr16      &&
         aAllocSize == (aAllocSize&0xFFFF)    )
    { pRoot->ud.oRoot16.aAllocSize = (UINT16)aAllocSize; return pRoot->ud.oRoot16.aAllocSize; }
    if ( uVBLock   == VBLock_Addr08 &&
         aAllocSize == (aAllocSize&0xFF)     )
    { pRoot->ud.oRoot08.aAllocSize = (UINT08)aAllocSize; return pRoot->ud.oRoot08.aAllocSize; }
    if ( uVBLock   == VBLock_Addr64 )
    { pRoot->ud.oRoot64.aAllocSize = (UINT64)aAllocSize; return (VBLsize)pRoot->ud.oRoot64.aAllocSize; }
    ASSERT (0);
    EVERR->Module ( "%s(pRoot=%i, aAllocSize=%i)", __FUNCTION__
                  , (VBLaddr)pRoot, aAllocSize )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLsize)~0;
}

VBLelem
VBHeapRoot_GetAllocItems ( const VBHeapRoot *pRoot )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.nAllocItems;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.nAllocItems;
    if ( uVBLock == VBLock_Addr64 )
      return (VBLelem)pRoot->ud.oRoot64.nAllocItems;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.nAllocItems;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLelem)~0;
}
VBLelem
VBHeapRoot_SetAllocItems ( VBHeapRoot *pRoot, VBLelem nItems, bool bAbsolute )
{
    if ( bAbsolute && nItems < 0 )
      EVERR->Module ( "%s(pRoot,nItems=%i,bAbsolute=%i)", __FUNCTION__
                    , nItems, bAbsolute )
           ->Message("Invalid parameter (nItems<0)" )
           ->Throw ( );
    else if ( !bAbsolute )
      nItems += VBHeapRoot_GetAllocItems ( pRoot );

    // Observe addressing model
    UINT  uItems  = static_cast<UINT>(nItems);
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32      &&
         uItems  == (uItems&0xFFFFFFFF)   )
    {
      pRoot->ud.oRoot32.nAllocItems = static_cast<UINT32>(uItems);
      return nItems;
    }
    if ( uVBLock == VBLock_Addr16  &&
         uItems  == (uItems&0xFFFF)   )
    {
      pRoot->ud.oRoot16.nAllocItems = static_cast<UINT16>(uItems);
      return nItems;
    }
    if ( uVBLock == VBLock_Addr08 &&
         uItems  == (uItems&0xFF)    )
    {
      pRoot->ud.oRoot08.nAllocItems = static_cast<UINT08>(uItems);
      return nItems;
    }
    if ( uVBLock == VBLock_Addr64 )
    {
      pRoot->ud.oRoot64.nAllocItems = static_cast<UINT64>(uItems);
      return nItems;
    }
    EVERR->Module ( "%s(pRoot,nItems=%i,bAbsolute=%i)", __FUNCTION__
                  , nItems, bAbsolute )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return nItems;
}

VBLaddr
VBHeapRoot_GetFree ( const VBHeapRoot *pRoot, int *pnItem )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.aFree;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.aFree;
    if ( uVBLock == VBLock_Addr64 )
      return pRoot->ud.oRoot64.aFree;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.aFree;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}
VBHeapRoot*
VBHeapRoot_SetFree ( VBHeapRoot *pRoot, VBLaddr aFree )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock  == VBLock_Addr32      &&
         aFree    == (aFree&0xFFFFFFFF)    )
    { pRoot->ud.oRoot32.aFree = (UINT32)aFree; return pRoot; }
    if ( uVBLock  == VBLock_Addr16 &&
         aFree   == (aFree&0xFFFF)     )
    { pRoot->ud.oRoot16.aFree = (UINT16)aFree; return pRoot; }
    if ( uVBLock == VBLock_Addr08 &&
         aFree   == (aFree&0xFF)     )
    { pRoot->ud.oRoot08.aFree = (UINT08)aFree; return pRoot; }
    if ( uVBLock  == VBLock_Addr64 )
    { pRoot->ud.oRoot64.aFree = (UINT64)aFree; return pRoot; }
    ASSERT (0);
    EVERR->Module ( "%s(pRoot=%i, aFree=%i)", __FUNCTION__
                  , (VBLaddr)pRoot, aFree )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return 0;
}
VBLaddr
VBHeapRoot_GetFreeLast ( const VBHeapRoot *pRoot, int *pnItem )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( pnItem )
      *pnItem = 0;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.aFreeLast;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.aFreeLast;
    if ( uVBLock == VBLock_Addr64 )
      return pRoot->ud.oRoot64.aFreeLast;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.aFreeLast;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0;
}
VBHeapRoot*
VBHeapRoot_SetFreeLast ( VBHeapRoot *pRoot, VBLaddr aFreeLast )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr32          &&
         aFreeLast == (aFreeLast&0xFFFFFFFF)    )
    { pRoot->ud.oRoot32.aFreeLast = (UINT32)aFreeLast; return pRoot; }
    if ( uVBLock   == VBLock_Addr16      &&
         aFreeLast == (aFreeLast&0xFFFF)    )
    { pRoot->ud.oRoot16.aFreeLast = (UINT16)aFreeLast; return pRoot; }
    if ( uVBLock   == VBLock_Addr08 &&
         aFreeLast == (aFreeLast&0xFF)     )
    { pRoot->ud.oRoot08.aFreeLast = (UINT08)aFreeLast; return pRoot; }
    if ( uVBLock   == VBLock_Addr64 )
    { pRoot->ud.oRoot64.aFreeLast = (UINT64)aFreeLast; return pRoot; }
    ASSERT (0);
    EVERR->Module ( "%s(pRoot=%Ii, aFreeLast=%Ii)", __FUNCTION__
                  , (VBLaddr)pRoot, aFreeLast )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return 0;
}

VBLsize
VBHeapRoot_GetFreeSize ( const VBHeapRoot *pRoot )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.aFreeSize;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.aFreeSize;
    if ( uVBLock == VBLock_Addr64 )
      return (VBLsize)pRoot->ud.oRoot64.aFreeSize;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.aFreeSize;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return ~0u;
}
VBLaddr
VBHeapRoot_SetFreeSize ( VBHeapRoot *pRoot, VBLsize aFreeSize, bool bAbsolute )
{
    if ( bAbsolute && aFreeSize < 0 )
      EVERR->Module ( "%s(pRoot,aFreeSize=%i,bAbsolute=%i)", __FUNCTION__
                    , aFreeSize, bAbsolute )
           ->Message("Invalid parameter (nItems<0)" )
           ->Throw ( );
    else if ( !bAbsolute )
      aFreeSize += VBHeapRoot_GetFreeSize ( pRoot );

    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock   == VBLock_Addr32          &&
         aFreeSize == (aFreeSize&0xFFFFFFFF)    )
    { pRoot->ud.oRoot32.aFreeSize = (UINT32)aFreeSize; return pRoot->ud.oRoot32.aFreeSize; }
    if ( uVBLock   == VBLock_Addr16      &&
         aFreeSize == (aFreeSize&0xFFFF)    )
    { pRoot->ud.oRoot16.aFreeSize = (UINT16)aFreeSize; return pRoot->ud.oRoot16.aFreeSize; }
    if ( uVBLock   == VBLock_Addr08 &&
         aFreeSize == (aFreeSize&0xFF)     )
    { pRoot->ud.oRoot08.aFreeSize = (UINT08)aFreeSize; return pRoot->ud.oRoot08.aFreeSize; }
    if ( uVBLock   == VBLock_Addr64 )
    { pRoot->ud.oRoot64.aFreeSize = (UINT64)aFreeSize; return (VBLaddr)pRoot->ud.oRoot64.aFreeSize; }
    ASSERT (0);
    EVERR->Module ( "%s(pRoot=%i, aFreeSize=%i)", __FUNCTION__
                  , (VBLaddr)pRoot, aFreeSize )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLaddr)~0;
}

VBLelem
VBHeapRoot_GetFreeItems ( const VBHeapRoot *pRoot )
{
    // Observe addressing model
    UINT16 uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return pRoot->ud.oRoot32.nFreeItems;
    if ( uVBLock == VBLock_Addr16 )
      return pRoot->ud.oRoot16.nFreeItems;
    if ( uVBLock == VBLock_Addr64 )
      return (VBLelem)pRoot->ud.oRoot64.nFreeItems;
    if ( uVBLock == VBLock_Addr08 )
      return pRoot->ud.oRoot08.nFreeItems;
    EVERR->Module ( __FUNCTION__)
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return (VBLelem)~0;
}
VBLelem
VBHeapRoot_SetFreeItems ( VBHeapRoot *pRoot, VBLelem nItems, bool bAbsolute )
{
    if ( bAbsolute && nItems < 0 )
      EVERR->Module ( "%s(pRoot,nItems=%i,bAbsolute=%i)", __FUNCTION__
                    , nItems, bAbsolute )
           ->Message("Invalid parameter (nItems<0)" )
           ->Throw ( );
    else if ( !bAbsolute )
      nItems += VBHeapRoot_GetFreeItems ( pRoot );

    // Observe addressing model
    UINT  uItems  = static_cast<UINT>(nItems);
    UCHAR uVBLock = pRoot->uVBLock & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32      &&
         uItems  == (uItems&0xFFFFFFFF)   )
    {
      pRoot->ud.oRoot32.nFreeItems = static_cast<UINT32>(uItems);
      return nItems;
    }
    if ( uVBLock == VBLock_Addr16  &&
         uItems  == (uItems&0xFFFF)   )
    {
      pRoot->ud.oRoot16.nFreeItems = static_cast<UINT16>(uItems);
      return nItems;
    }
    if ( uVBLock == VBLock_Addr08 &&
         uItems  == (uItems&0xFF)    )
    {
      pRoot->ud.oRoot08.nFreeItems = static_cast<UINT08>(uItems);
      return nItems;
    }
    if ( uVBLock == VBLock_Addr64 )
    {
      pRoot->ud.oRoot64.nFreeItems = static_cast<UINT64>(uItems);
      return nItems;
    }
    EVERR->Module ( "%s(pRoot,nItems=%i,bAbsolute=%i)", __FUNCTION__
                  , nItems, bAbsolute )
         ->Message("Internal VBHeapRoot.uVBLockAddr=%i corruption"
                  , uVBLock )
         ->Throw ( );
    return nItems;
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgHeap containers and definitions
//  NOTES: Fundamental unit of memory allocation and subsequent 

#define P2PmsgHeap_IOMAGE   1
#define P2PmsgHeap_SYSTEM   2
#define P2PmsgHeap_CHUNK    3
#define P2PmsgHeap_BSTRio   4
#define P2PmsgHeap_BSTRio64 5

typedef struct tagSTrigger
{
    UINT           nType;              // INSERT, UPDATE or DELETE
    HWND           hWnd;               // Notification window
    UINT           uiWM_APP_Trigger;
                                       // WPARAM is always P2Pos address
    LPARAM         lParam;             // LPARAM for notification
} STrigger;
typedef CList<STrigger> CListRegTriggers;
typedef CMap<VBLaddr,VBLaddr,CListRegTriggers*,CListRegTriggers*> CMapTriggers;
CMapTriggers*                          // Pre-definition
P2PmsgHeap_DropTriggers ( CMapTriggers *pCMapTriggers );

typedef CMap<VBLaddr,VBLaddr,VBLaddr,VBLaddr> CMapAlloc;
typedef struct
{
    std::atomic<int> nRefCount;   // atomic: AddRef/Close race across pump threads (msg IOMAGE handle share)
    UINT08     uVBListType;
    UINT08     uAddrType;
    VBLsize    nSizeofUsed;
    VBLsize    nSizeofInit;
    VBLsize    nSizeofAlloc;
    VBLsize    nSizeofMax;
    VBLsize    nVBLockMin;
    char      *pVBListBuffer;
    //UINT       aVBListBuffer;
    VBLelem    nFreeEntries;
    BOOL       bEoD;
    BOOL       bDirty;
    // Headless trigger sink (runtime-only; NOT part of the persisted image, which
    // is *pBSTRio/*pIOMAGE). Zero-initialised by the ZeroMemory in every handle
    // allocation below, so a manager with no sink installed leaves this nullptr.
    P2PmsgTriggerSink pfnTriggerSink;
    void*             pTriggerSinkUser;
    // Report budgets for the two collation refusals. Runtime-only, like the
    // sink above, and zero-initialised by the ZeroMemory in every handle
    // allocation below.
    //
    //  WHY BUDGETS AND NOT PLAIN REPORTS: both refusals sit inside a per-block
    //  loop walking structure that came off the wire, so ONE malformed image
    //  reaches them once per block. Reporting each occurrence let a remote peer
    //  drive an unbounded volume of log writes with a single bad image -- the
    //  same shape of defect as the F11 hang itself: attacker-chosen input
    //  consuming an unbounded amount of something.
    //
    //  MEASURED, NOT SUPPOSED. Fuzz run 32043591227 (recv_image, 600s, 58,514
    //  runs) emitted 157,875 report lines and a 27MB step log:
    //
    //      152,692  "Internal address corruption"          96.7%
    //        5,183  "free neighbour declares zero size"     3.3%
    //
    //  Note which one dominates. The zero-size refusal is the new one, and it is
    //  the small half; the address-corruption report has been emitting per block
    //  all along and was simply never measured under fuzzing. Budgeting only the
    //  new line -- the first thing tried here -- cut 3% of the volume, and cut
    //  none of it in practice, because these arrive across many short-lived
    //  heaps rather than many hits on one heap.
    //
    //  So the REFUSALS stay unconditional -- each one is what makes its loop
    //  terminate or stops it acting on damaged structure, and both must run
    //  every time -- while the REPORTS are budgeted to the first per heap, with
    //  P2PmsgHeap_Close emitting the tallies. A malformed image costs a bounded
    //  number of lines, and no occurrence goes unrecorded.
    VBLsize           nCollateZeroSize;
    VBLsize           nCollateCorrupt;
    union
    {
      struct
      {
        VBLaddr         uVBLock0;
        VBLaddr         uVBLock1;
        VBLaddr         uVBLock2;
        CMapAlloc      *pCMapAlloc;
      } SYS;
      struct 
      {
//#ifndef XCtrlKeys
        VBLockRoot      oRoot;         // Deprecated by oCtrlKeys
//#else
        VBHeapRoot     *pRoot;         // VBHeapRoot control keys
//#endif
        VBLaddr         uHiWM;
        VBLaddr         aIOmage;
        VBListIOmage   *pIOmage;
        VBHeapIOMAGE   *pIOMAGE;       // Raw image pointer
      } IOMAGE;
      struct 
      {
        //VBLockRoot    oRoot;
        //VBListBSTRio  oBSTRio;
        CMapTriggers   *pCMapTriggers;
        VBHeap_CtrlKeys oCtrlKeys;     // VBHeap control keys
        VBLaddr         uHiWM;
        VBLaddr         aBSTRio;
        VBListBSTRio   *pBSTRio;       // Raw image pointer
      } BSTRio;
      struct 
      {
        //VBLockRoot    oRoot;
        //VBListBSTRio  oBSTRio;
        CMapTriggers   *pCMapTriggers;
        VBHeap_CtrlKeys oCtrlKeys;     // VBHeap control keys
        VBLaddr         uHiWM;
        VBLaddr         aBSTRio;
        VBListBSTRio   *pBSTRio;       // Raw image pointer
      } BSTRio64;
      struct 
      {
        VBLockRoot    oRoot;
        VBLaddr       uHiWM;
        VBLaddr       aIOmage;
        VBListIOmage *pIOmage;
      } CHUNK;
    } u;
} VBListHANDLE;

//
//  Ceiling on how many blocks a heap could possibly contain: every block
//  carries at least a header, so the image size divided by the header size
//  bounds the count. Used to terminate the free-list walks, whose links come
//  off the wire and can be made to form a cycle. A count suffices -- a cycle
//  must revisit an address, so it must exceed the number of distinct blocks
//  that can exist, and no set of visited addresses need be remembered.
//
static VBLsize
P2PmsgHeap_MaxBlocks ( P2PmsgHANDLE hVBHeap ) noexcept
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBHeap);
    const VBLsize       nHdr    = P2PmsgHeap_Sizeof_Hdr ( hVBHeap );
    if ( nHdr == 0 )
      return 0;
    return pHandle->nSizeofAlloc / nHdr + 1;
}

//  Sizing VBLockHdr's, eliminates need to contract empty objects
//  const VBLockHdr soHdr    = { 0 };
const VBLock    soVBLock = { 0 };

//  F10. How many grow-and-retry passes an allocation gets before the free list
//  is declared not to describe the image. One is the legitimate answer -- a
//  resize that honours the requested size creates a block that satisfies it --
//  so this is one more than necessary and still bounds the geometric runaway
//  that a corrupt free list otherwise produces. See P2PmsgHeap_AllocBSTRio.
static const int kMaxAllocResizes = 2;

//  How many blocks that ALREADY FIT the request the allocator will look at
//  before settling for the best of them.
//
//  This allocator was first-fit from the head of a LIFO free list, and the two
//  together are worse than either: a freed block goes to the head, so the head
//  is whichever block was freed last, and first-fit takes it whatever its size.
//  Free an 800-byte block and then a 50-byte one, ask for 50, and the 800 is
//  split -- the 50 sits on the list untouched, and the next 800-byte request
//  cannot be served by what is left of the block that used to serve it. The
//  boundary tag (refer the note above VBHeap_FootMagic) repairs the case where
//  two such blocks are ADJACENT, by merging them back into one. It cannot
//  repair this one: live data sits between them and no merge is possible, so
//  the only repair left is to choose better among the blocks that exist.
//
//  BOUNDED, because "best fit" across a whole free list is a walk of the whole
//  free list on every single allocation, and this list can be long. Eight is
//  chosen against the list's own order rather than as a round number: the list
//  is LIFO, so the blocks nearest the head are the most recently freed, which
//  in a container being emptied and refilled -- the workload that produced the
//  824-bytes-a-round drift this and the boundary tag were found by -- are
//  exactly the blocks about to be asked for again. A walk of the whole list
//  would spend most of its time on the part least likely to help.
//
//  Blocks too small to serve the request do NOT count against this budget:
//  they were walked past before this change and are walked past after it. What
//  is budgeted is only the walking this change ADDS.
static const VBLsize kMaxFitWalk = 8;

///////////////////////////////////////////////////////////////////////
//  VBHeap private operations
//  NOTES: Used to expose VBHeap size etc

UINT
VBList_VBHeapMin ( UCHAR uVBLaddr )
{
    //VBLock    oVBLock;
    uVBLaddr = uVBLaddr & VBLock_AddrMask;
    if ( uVBLaddr == VBLock_Addr32 )
      return sizeof(VBHeap::oHdr) - sizeof(VBHeap::oHdr.u) + sizeof(VBHeap::oHdr.u.nSize32)
                           + sizeof(VBHeap::ud.oHeap32);
    if ( uVBLaddr == VBLock_Addr64 )
      return sizeof(VBHeap::oHdr) - sizeof(VBHeap::oHdr.u) + sizeof(VBHeap::oHdr.u.nSize64)
                           + sizeof(VBHeap::ud.oHeap64);
    if ( uVBLaddr == VBLock_Addr16 )
      return sizeof(VBHeap::oHdr) - sizeof(VBHeap::oHdr.u) + sizeof(VBHeap::oHdr.u.nSize16)
                           + sizeof(VBHeap::ud.oHeap16);
    if ( uVBLaddr == VBLock_Addr08 )
      return sizeof(VBHeap::oHdr) - sizeof(VBHeap::oHdr.u) + sizeof(VBHeap::oHdr.u.nSize08)
                           + sizeof(VBHeap::ud.oHeap08);
    ASSERT(0);
    return 0;
}

///////////////////////////////////////////////////////////////////////
//  Boundary tags
//  NOTES: This heap could only ever coalesce FORWARDS. Collate merges a freed
//         block with its next physical neighbour, and there was no way to go
//         the other way: a block is found by its address, its header sits at
//         the front, and nothing in the image says how far back the previous
//         block began. So a run of frees reclaimed its space only if it ran
//         high address to low. Low to high -- which is what almost everything
//         does, P3PmsgDesc::Truncate deleting child 0 over and over being the
//         obvious one -- every block's neighbour was still allocated at the
//         moment it was freed, nothing merged, and the list filled with
//         separate blocks that first-fit then walked straight past, because the
//         one that had absorbed the image tail sat at its head and satisfied
//         everything. Measured before this: five children into a container,
//         Truncate, refill, repeat -- 824 bytes a round, for ever.
//
//       : A FOOTER IN THE FREE BLOCK, NOT IN EVERY BLOCK, and that is the whole
//         reason this costs no format change. The classic boundary tag puts a
//         size footer on every block, allocated ones included, which would move
//         every byte of every image and break the golden byte-identity gate
//         along with every .p2p already written. It is not needed: the footer
//         is only ever READ when the predecessor turns out to be free, so it
//         only ever needs to EXIST in a free block -- and a free block's tail
//         is dead space nothing else uses. An allocated block is untouched,
//         byte for byte, so a tree that is built and saved without freeing
//         anything serialises exactly as it did before.
//
//       : A STALE FOOTER CANNOT CAUSE A WRONG MERGE. The tag is a hint and the
//         predecessor's own header is the authority: PrevFree recomputes the
//         candidate address from the recorded size and then requires the block
//         it lands on to declare the same size, the same addressing mode, and
//         to be free, linked and allocated. Anything else -- a footer left
//         behind inside a block that has since been handed out, a coincidence
//         in payload bytes, a heap read off the wire -- fails that and the
//         answer is simply "no predecessor", which is where this started.
//
//       : Blocks too small to hold a footer behind their free-list links do not
//         get one. They keep the old behaviour, which is correct, just not
//         improved.
#define VBHeap_FootMagic 0x46544246u   // 'FBTF', little-endian on disk

#pragma pack(push,1)
typedef struct VBHeapFoot__
{
    UINT32 uMagic;                     // VBHeap_FootMagic
    UINT64 nSize;                      // Size of the block this sits at the end of
} VBHeapFoot;
#pragma pack(pop)

//  Smallest free block that can carry a tag without treading on the nPrev and
//  nNext links at its front.
static inline VBLsize
P2PmsgHeap_FootMin ( UCHAR uAddrType ) noexcept
{
    return VBList_VBHeapMin ( uAddrType ) + sizeof(VBHeapFoot);
}

//  Writes the tag at the end of a free block. Silent no-op when the block is
//  too small to carry one, or is not free.
static void
P2PmsgHeap_StampFoot ( P2PmsgHANDLE hVBList, VBLaddr aVBLock ) noexcept
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( aVBLock == 0 || aVBLock >= pHandle->nSizeofAlloc )
      return;
    VBHeap *pVBLock = VBList2PhysVBHeap ( hVBList, aVBLock );
    if ( pVBLock == nullptr || !VBHeap_IsFree(pVBLock) )
      return;
    const VBLsize nSizeof = VBHeap_Sizenn ( pVBLock );
    if ( nSizeof < P2PmsgHeap_FootMin(pHandle->uAddrType) )
      return;
    if ( aVBLock + nSizeof > pHandle->nSizeofAlloc )
      return;                          // Declared size leaves the image
    VBHeapFoot *pFoot = (VBHeapFoot *)( (char *)pVBLock + nSizeof - sizeof(VBHeapFoot) );
    pFoot -> uMagic = VBHeap_FootMagic;
    pFoot -> nSize  = (UINT64)nSizeof;
}

//  Answers the address of the free block physically BEFORE aVBLock, or 0 when
//  there is none to be had. Every field it reads is cross-checked against the
//  candidate's own header; see the note above on why a stale tag is harmless.
static VBLaddr
P2PmsgHeap_PrevFree ( P2PmsgHANDLE hVBList, VBLaddr aVBLock ) noexcept
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    const VBLsize nHdr = P2PmsgHeap_Sizeof_Hdr ( hVBList );
    if ( nHdr == 0 || aVBLock <= nHdr + sizeof(VBHeapFoot) )
      return 0;                        // No room for a block and a tag before us
    if ( aVBLock > pHandle->nSizeofAlloc )
      return 0;

    const VBHeapFoot *pFoot =
      (const VBHeapFoot *)( (char *)P2PmsgHeap_Addr2Phys(hVBList,aVBLock)
                          - sizeof(VBHeapFoot) );
    if ( pFoot == nullptr || pFoot->uMagic != VBHeap_FootMagic )
      return 0;

    const UINT64 nSize = pFoot -> nSize;
    if ( nSize < P2PmsgHeap_FootMin(pHandle->uAddrType) || nSize >= aVBLock )
      return 0;                        // Nonsense, or it would start before the image
    const VBLaddr aPrev = aVBLock - (VBLaddr)nSize;
    if ( aPrev < nHdr )
      return 0;

    const VBHeap *pPrev = VBList2PhysVBHeap ( hVBList, aPrev );
    if ( pPrev == nullptr )
      return 0;
    if ( !VBHeap_IsAddr(pPrev,pHandle->uAddrType) ||
         !VBHeap_IsAlloc(pPrev)                   ||
         !VBHeap_IsLinked(pPrev)                  ||
         !VBHeap_IsFree(pPrev)                       )
      return 0;
    if ( (UINT64)VBHeap_Sizenn(pPrev) != nSize )
      return 0;                        // The tag and the block disagree: believe the block
    return aPrev;
}

//  Takes the tags back out of the free blocks, and puts them back.
//  NOTES: A TAG MUST NOT REACH AN IMAGE ON DISK. It lives in the tail of a free
//         block, which is dead space in memory but is still written out when
//         the arena is serialised -- and this tree keeps its serialised slack
//         DETERMINISTIC on purpose (the zeroed grown tail in ResizeIOMAGE,
//         byte_order.md 4.2), with MscsUnitTests/golden_ref.p2p gating it byte
//         for byte across operating systems. Persisting the tag would not
//         corrupt anything: free-block contents are never read back, an old
//         build loads a new image and a new build loads an old one, the only
//         cost being no backward merge until the block is freed again. What it
//         WOULD do is make an image saved by a patched build differ from one
//         saved by an unpatched build, which is precisely the drift that gate
//         exists to catch. So the tag stays in memory, where it belongs: Save
//         scrubs, writes, and re-stamps.
//       : Bounded like every other walk of this list -- the links come off the
//         wire and can be made cyclic. Refer P2PmsgHeap_AllocBSTRio.
void
P2PmsgHeap_ScrubFoots ( P2PmsgHANDLE hVBList, bool bRestore ) noexcept
{
    if ( hVBList == nullptr )
      return;
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBLaddr aFree = 0;
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      aFree = VBHeapRoot_GetFree ( pHandle->u.IOMAGE.pRoot );
    else if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      aFree = pHandle->u.BSTRio.pBSTRio->oKeys.aFree;
    else
      return;                          // SYS heaps are never serialised

    const VBLsize nMaxFree = P2PmsgHeap_MaxBlocks ( hVBList );
    VBLsize       nWalked  = 0;
    while ( aFree )
    {
      if ( ++nWalked > nMaxFree )
        return;                        // Cyclic; say nothing, this is a courtesy pass
      VBHeap *pFree = VBList2PhysVBHeap ( hVBList, aFree );
      if ( pFree == nullptr )
        return;
      if ( bRestore )
      {
        P2PmsgHeap_StampFoot ( hVBList, aFree );
      }
      else
      {
        const VBLsize nSizeof = VBHeap_Sizenn ( pFree );
        if ( VBHeap_IsFree(pFree)                             &&
             nSizeof >= P2PmsgHeap_FootMin(pHandle->uAddrType) &&
             aFree + nSizeof <= pHandle->nSizeofAlloc             )
          memset ( (char *)pFree + nSizeof - sizeof(VBHeapFoot), 0, sizeof(VBHeapFoot) );
      }
      aFree = VBHeap_GetNext ( pFree, 0 );
    }
}

UINT
VBList_VBHeapMax ( UINT16 uVBHeapDefs ) noexcept
{
    uVBHeapDefs = uVBHeapDefs & VBLock_AddrMask;
    if ( uVBHeapDefs == VBLock_Addr32 )
      return 0xFFFFFFFF;
    if ( uVBHeapDefs == VBLock_Addr64 )
      return 0xFFFFFFFF;
    if ( uVBHeapDefs == VBLock_Addr16 )
      return 0xFFFF;
    if ( uVBHeapDefs == VBLock_Addr08 )
      return 0xFF;
    ASSERT(0);
    return 0;
}

//
//  The arena ceiling -- finding M6.
//  NOTES: Every VBHeap address is an OFFSET into the arena, stored in a field
//         of the addressing width. The arena may therefore never grow past what
//         that width can name, and VBList_VBHeapMax above is the number that
//         says so. nSizeofMax holds it: the create paths either take it from
//         VBList_VBHeapMax directly or validate a caller's own smaller limit
//         against it.
//       : Nothing enforced that invariant. The only ceiling check in this file
//         (P2PmsgHeap_Alloc) tests nSizeofUsed, which is a DIFFERENT quantity --
//         live allocated bytes. It lags the arena by whatever slack the 20%
//         oversize growth left, and falls again on every Free. An Addr16 store
//         therefore walked its arena to 77833 bytes against a declared limit of
//         65535 while that check passed on every call, because the number it
//         tests was never the number that had to stay bounded.
//       : The consequence is not a failed allocation, it is a silent one. Past
//         the width, an offset stored into a link field is TRUNCATED to a
//         different valid-looking offset -- the evidence is the discarded high
//         bits -- so the free list is corrupted with nothing downstream able to
//         notice. That is finding M5's precondition, one layer down.
//
//  Parameters:  const VBListHANDLE *pHandle
//               Heap whose arena is about to grow
//
//  Returns:     VBLsize
//               Bytes the arena may still grow by, 0 when it is at its ceiling
//
//  NOTES: Computed by subtraction rather than as (nSizeofAlloc+extra > nSizeofMax)
//         on purpose. VBLsize is UINT_PTR, so on Win32 that sum wraps for a large
//         extra and the comparison then passes -- the same overflow shape the
//         check is here to stop.
static VBLsize
P2PmsgHeap_ArenaRoom ( const VBListHANDLE *pHandle ) noexcept
{
    return ( pHandle->nSizeofMax > pHandle->nSizeofAlloc )
         ?   pHandle->nSizeofMax - pHandle->nSizeofAlloc
         :   0;
}

//
//  Clamps a proposed arena growth to the addressing ceiling
//  NOTES: Two different quantities are passed because they fail differently.
//         nSizeofNeeded is the allocation the caller must satisfy; if it does
//         not fit, the heap is full and saying so is the only honest answer.
//         nSizeofExtra is that figure after the 20% oversize bump, which is a
//         performance optimisation -- clamping it is free, and refusing an
//         allocation that fits because the optimisation did not would be a
//         regression rather than a fix.
//
//  Parameters:  const VBListHANDLE *pHandle
//
//               VBLsize nSizeofNeeded
//               Bytes the pending allocation actually requires
//
//               VBLsize nSizeofExtra
//               Bytes the resize would like to add, oversize bump included
//
//  Returns:     VBLsize
//               Permitted growth, >= nSizeofNeeded; throws instead of returning
//               a figure that would not satisfy the caller
static VBLsize
P2PmsgHeap_CapGrowth ( const VBListHANDLE *pHandle
                     , VBLsize nSizeofNeeded, VBLsize nSizeofExtra )
{
    const VBLsize nSizeofRoom = P2PmsgHeap_ArenaRoom ( pHandle );
    if ( nSizeofNeeded > nSizeofRoom )
      EVERR->MODULE
           ->AFP(nSizeofNeeded)->AFP(nSizeofExtra)
           ->Message(L"Attempt to grow P2PmsgHeap arena to 0x%x bytes past the "
                     L"0x%x addressable by its Addr%02u width (0x%x free)"
                    , (UINT)nSizeofNeeded, (UINT)pHandle->nSizeofMax
                    , (UINT)(8u<<(pHandle->uAddrType&VBLock_AddrMask))
                    , (UINT)nSizeofRoom )
           ->Throw();
    if ( nSizeofExtra > nSizeofRoom )
      nSizeofExtra = nSizeofRoom;
    return nSizeofExtra;
}

//
//  Rejects a reconstructed arena wider than its own addressing type
//  NOTES: The growth path above is only half of M6. The create-from-image paths
//         set nSizeofAlloc from the IMAGE and nSizeofMax from the addressing
//         type named in that same image, and never compared the two -- so a
//         crafted header declaring Addr16 over a megabyte-long arena starts life
//         with the invariant already broken, and every offset in it may exceed
//         the width without a single check firing. That is the reachable-from-
//         Load half, which is the half a fuzzer gets to.
//
//  Parameters:  UINT08 uAddrType
//               Addressing width claimed by the image
//
//               VBLsize nSizeofAlloc
//               Arena size declared by the image
//
//               LPCSTR lpszWhere
//               Caller, for the message
static void
P2PmsgHeap_CheckArenaWidth ( UINT08 uAddrType, VBLsize nSizeofAlloc, LPCSTR lpszWhere )
{
    const VBLsize nSizeofMax = VBList_VBHeapMax ( uAddrType );
    if ( nSizeofMax == 0 || nSizeofAlloc > nSizeofMax )
      EVERR->Module ( lpszWhere )
           ->Message ( L"Image declares a 0x%x-byte arena, past the 0x%x "
                       L"addressable by the Addr%02u width it also declares"
                     , (UINT)nSizeofAlloc, (UINT)nSizeofMax
                     , (UINT)(8u<<(uAddrType&VBLock_AddrMask)) )
           ->Throw();
}

//
//  Rejects an unsupported heap addressing width at creation
//  NOTES: This was three copies of ASSERT(uAddrType!=VBLock_Addr08), one in
//         each create-by-width entry point, and Release compiles all three out.
//         A Release caller passing 0 therefore got the configuration its own
//         author had asserted against, and failed later somewhere unrelated --
//         C4LoadTest [12] recorded exactly that, an Addr08 store dying at offset
//         0xb8 with a message about Addr16,32,64 references.
//       : Msgcore_c.h has documented uAddrNN as 16/32/64 (1/2/3) since the flat
//         ABI was written, so this is not a narrowing of the supported surface.
//         It is the code finally agreeing with the header a C caller reads.
//       : The refusal is at heap CREATION only. Addr08 remains a valid width for
//         an individual VBLock inside an image -- every accessor in this file
//         handles one -- and nothing here changes that.
//
//  Parameters:  UCHAR uAddrType
//               Addressing width requested by the caller
static void
P2PmsgHeap_CheckCreateWidth ( UCHAR uAddrType )
{
    if ( (uAddrType&VBLock_AddrMask) == VBLock_Addr08 )
      EVERR->MODULE
           ->AFP(uAddrType)
           ->Message(L"Addr08 is not a supported heap addressing width "
                     L"(use VBLock_Addr16, VBLock_Addr32 or VBLock_Addr64)")
           ->Throw();
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgHeapSys private operations
//  NOTES: Specific P2PmsgHeapSys implementation

BOOL
P2PmsgHeapSYS_Close ( P2PmsgHANDLE hVBList )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    // Recover resources
    if ( pHandle->u.SYS.uVBLock0 )
    {
      delete [] (char *)pHandle->u.SYS.uVBLock0;
      pHandle->u.SYS.uVBLock0 = 0;
    }
    if ( pHandle->u.SYS.uVBLock1 )
    {
      delete [] (char *)pHandle->u.SYS.uVBLock1;
      pHandle->u.SYS.uVBLock1 = 0;
    }
    if ( pHandle->u.SYS.uVBLock2 )
    {
      delete [] (char *)pHandle->u.SYS.uVBLock2;
      pHandle->u.SYS.uVBLock2 = 0;
    }
    if ( pHandle->u.SYS.pCMapAlloc )
    {
      // Recover all P2Pevent resources
      POSITION pos = pHandle->u.SYS.pCMapAlloc->GetStartPosition();
      while ( pos )
      {
        VBLaddr aVBLock = 0;
        pHandle->u.SYS.pCMapAlloc->GetNextAssoc ( pos, aVBLock, aVBLock );
        delete [] (char *)aVBLock;
      }
      delete pHandle->u.SYS.pCMapAlloc;
             pHandle->u.SYS.pCMapAlloc = 0;
    }

    // Tidy up, and
    return 0;
}

bool
P2PmsgHeapSYS_AssertValid ( P2PmsgHANDLE hP2PmsgHeap ) noexcept
{
    // Initialisation
    VBListHANDLE *pHandle = (VBListHANDLE *)hP2PmsgHeap;
    if ( pHandle              == 0                 ||
         pHandle->uVBListType != P2PmsgHeap_SYSTEM    )
      return false;

    // Valid
    return true;
}

//
//  Asserts allocated VBLock validity
//  NOTES: Multiple internal structures
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLock
//               Allocated block address to be validated
//
//  Returns:     bResult
//                 true... Validated
//                 false.. Failed
bool
P2PmsgHeap_AssertValidAllocSYS ( P2PmsgHANDLE hP2PmsgHeap, VBLaddr aVBLock )
{
    bool          bResult = true;
    VBListHANDLE *pHandle = (VBListHANDLE *)hP2PmsgHeap;
    if ( IsBadWritePtr((void*)aVBLock,4) )
    {  ASSERT(0); return false; }      // Out of range
    VBLock       *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( hP2PmsgHeap, aVBLock );
    if ( !VBLock_IsLinked(pVBLock) )   // TODO:LJM this hack needs tidying up
    { bResult = false; ASSERT(bResult);if(!VBLock_IsFree(pVBLock)&&VBLock_IsAddr(pVBLock,pHandle->uAddrType)&&VBLock_IsAlloc(pVBLock))pVBLock->oHdr.uVBLockDefs|=VBLock_Linked;}
    if ( !VBLock_IsAlloc(pVBLock) )
    { bResult = false; ASSERT(0); }
    if (  VBLock_IsFree(pVBLock) )
    { bResult = false; ASSERT(0); }
    if ( !VBLock_IsAddr(pVBLock,pHandle->uAddrType) )
    { bResult = false; ASSERT(0); }
    return bResult;
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgHeapIO private operations
//  NOTES: Specific P2PmsgHeapIO implementation

// P2PmsgHeap_ResizeIOMAGE1 was deleted here (item 19, the ASSERT triage).
//
// It opened with a bare ASSERT(0) and then did ninety lines of real heap
// work: reallocating the base image, relinking the free list, rewriting the
// root. The assertion says "this must never be called"; Release compiles it
// out and the function then runs to completion with its own author's belief
// silently discarded. That is the C4 root lesson in one function -- structural
// intent expressed in a form that is absent from every shipped binary.
//
// Resolved by asking whether it is reachable rather than by rewording the
// assertion. It is not: nothing calls it. The only caller-facing name is
// P2PmsgHeap_ResizeIOMAGE (no digit), a separate implementation reached from
// MsgVBHeap.cpp:3251 and :3313, and ResizeIOMAGE1 was not declared in
// MsgVBHeap.h, so it was neither called nor callable from outside this file.
// The ASSERT(0) was right about the function and could not say so in Release.
//
// Deleting rather than promoting the assertion to a throw: a throw would be a
// live guard on a function that cannot be reached, which is a third way of
// writing down the same dead intent. Not exported, so the export manifests do
// not move.

VBLaddr
P2PmsgHeap_ResizeIOMAGE ( P2PmsgHANDLE hVBList, VBLsize nSizeofExtra, bool bOversize )
{
    // Locals
P2PmsgHeap_AssertValidIOMAGE(hVBList);
//P2PmsgHeap_AssertVBlocksBSTRio(hVBHeap);
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    const VBLsize nSizeofNeeded = nSizeofExtra;     // before the oversize bump
    if (  bOversize                                  &&
         (pHandle->nSizeofAlloc*2)/10 > nSizeofExtra    )
      nSizeofExtra = (pHandle->nSizeofAlloc * 2) / 10;

    // Ceiling (M6). Throws when nSizeofNeeded will not fit; clamps the oversize
    // bump when only it will not.
    nSizeofExtra = P2PmsgHeap_CapGrowth ( pHandle, nSizeofNeeded, nSizeofExtra );

    // Old VBListBSTRio
    VBLsize nSizeofOldAlloc = pHandle -> nSizeofAlloc;

    // New VBListBSTRio
    VBLsize nSizeofNewAlloc = nSizeofOldAlloc + nSizeofExtra;
    //VBLsize nSizeNewIOmage  = nSizeofNewAlloc + sizeof(VBListBSTRio);
    VBHeapIOMAGE  *pIOMAGE  = (VBHeapIOMAGE *)new char [nSizeofNewAlloc+4]();   // zero grown tail -> deterministic slack (§4.2)
    memcpy ( pIOMAGE, pHandle->u.IOMAGE.pIOMAGE, nSizeofOldAlloc );
    delete [] (char *)pHandle->u.IOMAGE.pIOMAGE;
    pHandle -> u.IOMAGE.uHiWM   =  nSizeofNewAlloc;
    pHandle -> u.IOMAGE.pIOMAGE =  pIOMAGE;
    // pIOmage MUST also be re-pointed at the new base. It was left dangling at
    // the old base freed just above, and the Close path frees via pIOmage
    // (line ~2574) - so after a single grow, Close double-freed the old base
    // and leaked the new one. Keeping pIOmage == pIOMAGE also fixes the stale
    // reads at P2PmsgHeap_pIOmage / the IsIOMAGE assert.
    pHandle -> u.IOMAGE.pIOmage =  (VBListIOmage *)pIOMAGE;
    pHandle -> u.IOMAGE.aIOmage =  reinterpret_cast<VBLaddr>(pIOMAGE);
    pHandle -> u.IOMAGE.pRoot   = &pHandle -> u.IOMAGE.pIOMAGE->oRoot;
    VBHeapRoot *pRoot = pHandle->u.IOMAGE.pRoot;
    VBHeapRoot_SetFreeSize ( pRoot, nSizeofExtra );
    VBHeapRoot_SetFreeItems ( pRoot, 1 );
    pHandle -> nFreeEntries++;
    pHandle -> nSizeofAlloc        = nSizeofNewAlloc;
    pHandle -> bDirty              = true;

    // Extra assignment as free entry
    // NOTES: Assumes role as last free entry in list
    VBLaddr     aFreeLast    = VBHeapRoot_GetFreeLast ( pRoot );
    VBLaddr     aVBLockExtra = nSizeofOldAlloc;// + sizeof(VBListBSTRio)/* LJM added 12/3/2011*/;
    VBHeap     *pVBLockExtra = VBList2PhysVBHeap ( hVBList, aVBLockExtra );
    VBHeap_Init    ( pVBLockExtra, pHandle->uAddrType, nSizeofExtra );
    VBHeap_SetPrev ( pVBLockExtra, aFreeLast );
    pVBLockExtra -> oHdr.uVBLockDefs |=  VBLock_Linked;
    pVBLockExtra -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    VBHeapRoot_SetFreeLast ( pRoot, aVBLockExtra );
    if ( VBHeapRoot_GetFree(pRoot) == 0 )
      VBHeapRoot_SetFree ( pRoot, aVBLockExtra );

    // Displace existing last entry
    if ( aFreeLast )
    {
      VBHeap *pFreeLast = (VBHeap *)P2PmsgHeap_Addr2Phys ( hVBList, aFreeLast );
      VBHeap_SetNext ( pFreeLast, aVBLockExtra );
    }

    //  The grown tail is a free block like any other, and it is the one most
    //  worth tagging: it is what the block below it merges into.
    P2PmsgHeap_StampFoot ( hVBList, aVBLockExtra );

    // Tidy up, and
ASSERT(VBHeap_IsLinked(pVBLockExtra));
ASSERT(VBHeap_IsAlloc(pVBLockExtra));
ASSERT(VBHeap_IsFree(pVBLockExtra));
//P2PmsgHeap_AssertVBlocksBSTRio(hVBHeap);
//P2PmsgHeap_AssertValidIOMAGE(hVBHeap);
    return nSizeofExtra;
}

VBLaddr
P2PmsgHeapIO_Split ( P2PmsgHANDLE hVBList
                   , VBLaddr aVBLockFree, VBLsize& nSizeof )
{
    // Locals
    VBListHANDLE *pHandle     = static_cast<VBListHANDLE *>(hVBList);
    VBHeap       *pVBLockFree = VBList2PhysVBHeap ( hVBList, aVBLockFree );
    UCHAR         uVBLock     = pVBLockFree->oHdr.uVBLockDefs;
    VBLockRoot   *pRoot       =&pHandle->u.IOMAGE.oRoot;
    VBLaddr       aVBLockPrev = VBHeap_GetPrev ( pVBLockFree, 0 );
    VBLaddr       aVBLockNext = VBHeap_GetNext ( pVBLockFree, 0 );
    ASSERT(0);

ASSERT(VBHeap_IsAddr(pVBLockFree,pHandle->uAddrType));
ASSERT(VBHeap_IsLinked(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(VBHeap_IsFree(pVBLockFree));
    // VBLock adoption
    // NOTES: Insufficient extra
    VBLsize nSizeofFree  = VBHeap_Sizenn(pVBLockFree);
    VBLsize nSizeofExtra = nSizeofFree - nSizeof;
    if ( nSizeofExtra < VBList_VBHeapMin(uVBLock) )
    {
#ifndef XCtrl_Hdr
      if ( VBLockRoot_GetFirst(pRoot) == aVBLockFree )
        VBLockRoot_SetFirst ( pRoot, VBHeap_GetNext(pVBLockFree,0) );
#else
      if ( VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) == aVBLockFree )
        VBHeapRoot_SetFree ( pHandle->u.IOMAGE.pRoot, VBHeap_GetNext(pVBLockFree,0) );
#endif
#ifndef XCtrl_Hdr
      if ( VBLockRoot_GetLast (pRoot) == aVBLockFree )
        VBLockRoot_SetLast  ( pRoot, VBHeap_GetPrev(pVBLockFree,0) );
#else
      if ( VBHeapRoot_GetFreeLast (pHandle->u.IOMAGE.pRoot) == aVBLockFree )
        VBHeapRoot_SetFreeLast ( pHandle->u.IOMAGE.pRoot, VBHeap_GetPrev(pVBLockFree,0) );
#endif

      if ( aVBLockPrev )
        VBHeap_SetNext ( VBList2PhysVBHeap(hVBList,aVBLockPrev), aVBLockNext );
      if ( aVBLockNext )
        VBHeap_SetPrev ( VBList2PhysVBHeap(hVBList,aVBLockNext), aVBLockPrev );
      //pVBLockFree->oHdr.uVBLockDefs &= ~VBLock_Linked;
      //pVBLockFree->oHdr.uVBLockDefs &= ~VBLock_TypeMask;
      pVBLockFree->oHdr.uVBLockDefs &= ~(VBLock_Linked | VBLock_TypeMask); // TODO:ChatGPT replaced above with this hack to eliminate VBLock_TypeMask from free entries, needs tidying up
#ifndef XCtrl_Hdr
      if ( VBLockRoot_GetFirst(pRoot) == aVBLockFree )
        VBLockRoot_SetFirst ( pRoot, aVBLockNext );
#else
      if ( VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) == aVBLockFree )
        VBHeapRoot_SetFree ( pHandle->u.IOMAGE.pRoot, aVBLockNext );
#endif
#ifndef XCtrl_Hdr
      if ( VBLockRoot_GetLast (pRoot) == aVBLockFree )
        VBLockRoot_SetLast  ( pRoot, aVBLockPrev );
#else
      if ( VBHeapRoot_GetFreeLast (pHandle->u.IOMAGE.pRoot) == aVBLockFree )
        VBHeapRoot_SetFreeLast  ( pHandle->u.IOMAGE.pRoot, aVBLockPrev );
#endif
#ifndef XCtrl_Hdr
      VBLockRoot_SetItems( pRoot, -1, false );
#else
      VBHeapRoot_SetFreeItems( pHandle->u.IOMAGE.pRoot, -1, false );
#endif

      nSizeof = nSizeofFree;
      if ( aVBLockFree+nSizeof > pHandle->u.IOMAGE.uHiWM )
        pHandle -> u.IOMAGE.uHiWM = aVBLockFree + nSizeof;
ASSERT(VBHeap_IsAddr(pVBLockFree,pHandle->uAddrType));
ASSERT(!VBHeap_IsLinked(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(VBHeap_IsFree(pVBLockFree));
      return aVBLockFree;
    }

    // VBLock extra
    // NOTES: Sufficient extra exists to create an alternative entry
    //        retaining the reletive position
    VBLaddr  aVBLockExtra = aVBLockFree + nSizeof;
    VBHeap  *pVBLockExtra = VBList2PhysVBHeap ( hVBList, aVBLockExtra );
    VBHeap_Init ( pVBLockExtra, pHandle->uAddrType, nSizeofExtra );
    VBHeap_SetNext ( pVBLockExtra, aVBLockNext );
    VBHeap_SetPrev ( pVBLockExtra, aVBLockPrev );
    pVBLockExtra -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    pVBLockExtra -> oHdr.uVBLockDefs |=  VBLock_Linked;
    if ( VBLockRoot_GetFirst(pRoot) == aVBLockFree )
      VBLockRoot_SetFirst ( pRoot, aVBLockExtra );
    if ( VBLockRoot_GetLast (pRoot) == aVBLockFree )
      VBLockRoot_SetLast  ( pRoot, aVBLockExtra );

    if ( aVBLockPrev )
      VBHeap_SetNext ( (VBHeap*)P2PmsgHeap_Addr2Phys(hVBList,aVBLockPrev), aVBLockExtra );
    if ( aVBLockNext )
      VBHeap_SetPrev ( (VBHeap*)P2PmsgHeap_Addr2Phys(hVBList,aVBLockNext), aVBLockExtra );
ASSERT(VBHeap_IsAddr(pVBLockExtra,pHandle->uAddrType));
ASSERT(VBHeap_IsFree(pVBLockExtra));
ASSERT(VBHeap_IsAlloc(pVBLockExtra));
ASSERT(VBHeap_Sizenn(pVBLockExtra)==nSizeofExtra);

    // Tidy up, and
    VBHeap_Init ( pVBLockFree, uVBLock, nSizeof );
    pVBLockFree -> oHdr.uVBLockDefs &= ~VBLock_Linked;
ASSERT(VBHeap_IsAddr(pVBLockFree,uVBLock&VBLock_AddrMask));
ASSERT(VBHeap_IsFree(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(!VBHeap_IsLinked(pVBLockFree));
    if ( aVBLockFree+nSizeof > pHandle->u.IOMAGE.uHiWM )
      pHandle -> u.IOMAGE.uHiWM = aVBLockFree + nSizeof;
ASSERT(P2PmsgHeap_AssertValidIOMAGE(hVBList));
    return aVBLockFree;
}

//  P2PmsgHeap_AssertValidIOMAGE1 was here, and is deleted (item 19).
//
//  It was the second function in this file to be found by the same question
//  rather than by reading it: is anything calling this? Nothing was. It had no
//  declaration in MsgVBHeap.h, no caller in the tree, and a name ending in "1" --
//  the same three properties P2PmsgHeap_ResizeIOMAGE1 had when item 19 deleted
//  it. Its body was a free-list walk whose four per-block tests were all bare
//  ASSERTs, so in a shipped binary it walked the list and concluded true no
//  matter what it saw; the live twin below is the one that does the work.
//
//  Deleting it rather than hardening it is the cheaper half of the triage: an
//  unreachable validator cannot be made to protect anything, and leaving it
//  meant the next person to harden this file would have paid for it twice.

//  Forward declaration. The definition sits further down with the other span
//  checks; the two IOMAGE walks below need it, and they come first in the file.
static bool
P2PmsgHeap_BlockFits ( P2PmsgHANDLE hP2PmsgHeap, VBLaddr aVBLock );

///////////////////////////////////////////////////////////////////////
//  THE UNTRUSTED GATE - ASSERT, or REFUSE?
//
//  See the long note at the declaration in MsgVBHeap.h for why this exists.
//  In one sentence: the walks below are both a developer aid over a heap this
//  process built and an acceptance test over an image a stranger sent, and a
//  violated invariant means opposite things in the two roles. Inside a gate it
//  is the ordinary answer - refuse, quietly. Outside one it is a bug in this
//  code - assert, exactly as before.
//
//  A DEPTH rather than a flag. P2PmsgHeap_AssertValidIOMAGE calls
//  P2PmsgHeap_AssertValidFree, and P2PmsgHeap_AssertVBlocksIOMAGE calls both,
//  so a gate opened at the top has to still be open at the bottom. Thread-local
//  because the receive path runs one of these per IOCP worker and a shared
//  counter would let one thread's gate silence another thread's assertion.
static thread_local int  s_nUntrustedGate = 0;

bool
P2PmsgHeap_InUntrustedGate ( )
{
    return s_nUntrustedGate > 0;
}

P2PmsgHeap_UntrustedGate::P2PmsgHeap_UntrustedGate ( )
{
    ++s_nUntrustedGate;
}

P2PmsgHeap_UntrustedGate::~P2PmsgHeap_UntrustedGate ( )
{
    --s_nUntrustedGate;
}

//  The three forms every check in the walks below now takes. All three are
//  macros rather than functions so that ASSERT reports the line of the
//  invariant that broke, which is the only thing an assertion here was ever
//  worth.
//
//  VBHEAP_ASSERT0 - drop-in for the bare `ASSERT(0)` that already sits beside a
//                   `bResult = false` or a `return false`. Those checks decide
//                   the verdict today and go on deciding it identically; the
//                   assertion is the only thing the gate removes.
//
//  VBHEAP_DIAG    - a check that was a BARE ASSERT: it fired in a debug build,
//                   said nothing in a release one, and never touched the
//                   verdict either way. Outside a gate that is unchanged, so no
//                   existing caller sees an answer it did not see before.
//                   Inside a gate it becomes a refusal, which is the point -
//                   these are the invariants an attacker breaks, and a gate
//                   that noticed one and returned true anyway would be theatre.
//
//  VBHEAP_SUBWALK - see below.
//
//  bResult must be in scope at every VBHEAP_DIAG and VBHEAP_SUBWALK. That is
//  deliberate: a check in these walks that cannot reach the verdict does not
//  belong in them.
#define VBHEAP_ASSERT0()                                                       \
    do { if ( !P2PmsgHeap_InUntrustedGate() ) ASSERT(0); } while (0)

#define VBHEAP_DIAG(x)                                                         \
    do { if ( !(x) )                                                           \
         { if ( P2PmsgHeap_InUntrustedGate() ) bResult = false;                \
           else                                ASSERT(0); } } while (0)

//  VBHEAP_SUBWALK - one of these walks calling another. The inner walk reports
//  its own failures at its own lines, so there is nothing to assert out here;
//  what it needs is for its verdict to join this one, and only inside a gate,
//  so that no existing caller of the outer walk sees an answer it did not see
//  before. Evaluated into a local first: short-circuiting the call outside a
//  gate would delete the nested walk, which is where most of the checking is.
#define VBHEAP_SUBWALK(x)                                                      \
    do { const bool bWalkOk__ = (x);                                           \
         if ( !bWalkOk__ && P2PmsgHeap_InUntrustedGate() )                     \
           bResult = false; } while (0)

bool
P2PmsgHeap_AssertValidIOMAGE ( P2PmsgHANDLE hP2PmsgHeap )
{
    // Initialisation
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if( pHandle==0                              ||
        pHandle->uVBListType!=P2PmsgHeap_IOMAGE    )
      return true;
    bool bResult = true;
    VBHEAP_DIAG((pHandle->uAddrType&VBLock_AddrMask)==pHandle->uAddrType);
    VBHEAP_DIAG(VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot)<(VBLsize)200000000u);

    // Environmental
    //  The third of these four used to read GetFreeLast on both sides, so it
    //  asked whether zero was zero. Its BSTRio twin has always read aFree on
    //  the right, which is what the pair is for; corrected here to match. It
    //  changes no verdict - "free set, freeLast clear" is what the second line
    //  already rejects - and it is corrected rather than deleted because the
    //  next person to read the pair should not have to work that out again.
    if ( VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) == 0 )
      VBHEAP_DIAG(VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) == 0 );
    if ( VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) )
      VBHEAP_DIAG(VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) );
    if ( VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) == 0 )
      VBHEAP_DIAG(VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) == 0 );
    if ( VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) )
      VBHEAP_DIAG(VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) );

    // Iterate through free entries 
    VBLelem nFreeEntries = 0;
    VBLaddr aFreeSize    = 0;
    VBLaddr aVBLockFree  = VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot);
    //  Bounded for the same reason as the walk in AssertFreeIOMAGE above: the
    //  links come off the wire, and a cycle among in-bounds addresses would
    //  spin here forever. This loop matters more than most in this file — it is
    //  reached from Release code, because several callers invoke
    //  P2PmsgHeap_AssertValidIOMAGE directly rather than inside VBHEAP_DIAG(...).
    const VBLsize nMaxFree = P2PmsgHeap_MaxBlocks ( hP2PmsgHeap );
    while ( aVBLockFree )
    {
      if ( nFreeEntries >= nMaxFree )
        return false;                    // cyclic free list
      nFreeEntries++;
      //  F3. Bound the CONTROL BLOCK before reading it, not just its first byte.
      //  Addr2Phys bounds the start only -- and with `>` rather than `>=`, so a
      //  link equal to nSizeofAlloc translates happily to the last byte of the
      //  image -- while the two lines below read a whole VBHeap header from
      //  there. That is F2's shape exactly, moved one branch across: F2 was this
      //  mistake in the BSTRio walk, and the IOMAGE walk kept it because Stage C
      //  fixed the branch the fuzzer had reached rather than the pattern.
      //
      //  This one is worse than F2 was, in one specific way. F2 was reachable
      //  only in Debug until the load path was made to run the walk in Release.
      //  This walk is ALREADY reached from Release code -- see the note above
      //  nMaxFree, which says so -- because several callers invoke
      //  P2PmsgHeap_AssertValidIOMAGE directly rather than inside VBHEAP_DIAG(...).
      //  So this was live in shipped binaries rather than dormant in them.
      if ( !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLockFree ) )
        return false;
      VBHeap *pVBLock = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLockFree );
      VBHEAP_DIAG((pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask)==pHandle->uAddrType);
      aFreeSize += VBHeap_Sizenn(pVBLock);
      VBHEAP_SUBWALK(P2PmsgHeap_AssertValidFree ( hP2PmsgHeap, aVBLockFree ));

      if ( aVBLockFree==VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) )
        VBHEAP_DIAG(VBHeap_GetPrev(pVBLock,0)==0);
      if ( aVBLockFree==VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) )
        VBHEAP_DIAG(VBHeap_GetNext(pVBLock,0)==0);
      aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
    }
    //  THE FIXUPS DO NOT RUN ON A STRANGER'S IMAGE, and that is the second half
    //  of what the gate is for. Repairing the root's accounting to match what
    //  this walk counted is reasonable maintenance on a heap we own; on an
    //  image that just arrived it is a WRITE into attacker-supplied bytes,
    //  performed by the very function whose job was to decide whether to touch
    //  them at all. The VBHEAP_DIAG above has already refused the image inside
    //  a gate, so there is nothing left here to correct - the caller is about
    //  to discard it.
    VBHEAP_DIAG(aFreeSize==VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot) );
    if ( aFreeSize!=VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot) &&
         !P2PmsgHeap_InUntrustedGate()                                 )
      VBHeapRoot_SetFreeSize(pHandle->u.IOMAGE.pRoot,aFreeSize,true);
    //VBHEAP_DIAG(nFreeEntries==pHandle->nFreeEntries||pHandle->nFreeEntries==0);
    if ( nFreeEntries!=pHandle->nFreeEntries)
      pHandle->nFreeEntries = nFreeEntries;
    VBHEAP_DIAG(VBHeapRoot_GetFreeItems(pHandle->u.IOMAGE.pRoot)==nFreeEntries );
    if ( VBHeapRoot_GetFreeItems(pHandle->u.IOMAGE.pRoot) != nFreeEntries &&
         !P2PmsgHeap_InUntrustedGate()                                       )
      VBHeapRoot_SetFreeItems(pHandle->u.IOMAGE.pRoot,nFreeEntries,true);
    return bResult;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
//  P2PmsgHeapBSTRio private operations
//  NOTES: Generic P2PmsgHeapBSTRio implementation
//       : Preceeded with private helpers and definitions

//
//  Asserts free VBLock validity
//  NOTES: Single internal structure
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLock
//               Free block address to be validated
//
//  Does the WHOLE control block at aVBLock lie inside the image?
//  NOTES: "aVBLock < nSizeofAlloc" bounds the block's first byte and nothing
//         else, so a block starting just under the limit passes and the caller
//         then reads a nPrev/nNext field off the end. That is the same
//         start-only mistake P2PmsgHeap_Addr2PhysChk exists to correct, and it
//         is worth spelling out where it bit: these validators are handed
//         attacker-controlled bytes BY DEFINITION -- validating is their whole
//         job -- so a validator that reads out of bounds while validating is
//         worse than no validator, because it converts a rejection into a
//         crash. Found by the fuzzer within fifteen minutes of the load path
//         being made to run this walk in Release (item 19).
//       : The span depends on the BLOCK's addressing mode, not the heap's, and
//         that mode is one untrusted byte at aVBLock -- in bounds once the
//         start is, which is why the start check still comes first.
static bool
P2PmsgHeap_BlockFits ( P2PmsgHANDLE hP2PmsgHeap, VBLaddr aVBLock )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if ( aVBLock == 0 || aVBLock >= pHandle->nSizeofAlloc )
      return false;
    const VBHeap *pVBLock = (const VBHeap *)P2PmsgHeap_Addr2Phys ( hP2PmsgHeap, aVBLock );
    if ( pVBLock == nullptr )
      return false;
    const UCHAR   uAddr = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    const VBLsize nNeed = VBList_VBHeapMin ( uAddr );
    if ( nNeed == 0 )
      return false;                    // unknown mode: refuse rather than guess
    return aVBLock + nNeed <= pHandle->nSizeofAlloc;
}

//
//  Returns:     bResult
//                 true... Validated
//                 false.. Failed
bool
P2PmsgHeap_AssertValidFree ( P2PmsgHANDLE hP2PmsgHeap, VBLaddr aVBLock )
{
    bool          bResult = true;
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if ( !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLock ) )
    {  VBHEAP_ASSERT0(); return false; }      // Out of range, or straddling the end
    VBHeap       *pVBLock = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLock );
    if ( !VBHeap_IsLinked(pVBLock) )
    { bResult = false; VBHEAP_ASSERT0(); }
    if ( !VBHeap_IsAlloc(pVBLock) )
    { bResult = false; VBHEAP_ASSERT0(); }
    if ( !VBHeap_IsFree(pVBLock) )
    { bResult = false; VBHEAP_ASSERT0(); }
    if ( !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) )
    { bResult = false; VBHEAP_ASSERT0(); }
    VBLaddr aVBLockPrev = VBHeap_GetPrev ( pVBLock, 0 );
    if ( aVBLockPrev && !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLockPrev ) )
    {  VBHEAP_ASSERT0(); return false; }      // Out of range, or straddling the end
    if ( aVBLockPrev )
    {
      VBHeap *pVBLockPrev = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLockPrev );
      if ( !VBHeap_IsLinked(pVBLockPrev) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsAlloc(pVBLockPrev) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsFree(pVBLockPrev) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsAddr(pVBLockPrev,pHandle->uAddrType) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if (  VBHeap_GetNext(pVBLockPrev,nullptr) != aVBLock )
      { bResult = false; VBHEAP_ASSERT0(); }
    }
    VBLaddr aVBLockNext = VBHeap_GetNext ( pVBLock, 0 );
    if ( aVBLockNext && !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLockNext ) )
    {  VBHEAP_ASSERT0(); return false; }      // Out of range, or straddling the end
    if ( aVBLockNext )
    {
      VBHeap *pVBLockNext = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLockNext );
      if ( !VBHeap_IsLinked(pVBLockNext) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsAlloc(pVBLockNext) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsFree(pVBLockNext) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsAddr(pVBLockNext,pHandle->uAddrType) )
      { bResult = false; VBHEAP_ASSERT0(); }
      if (  VBHeap_GetPrev(pVBLockNext,nullptr) != aVBLock )
      { bResult = false; VBHEAP_ASSERT0(); }
    }
    return bResult;
}

//
//  Asserts allocated VBLock validity
//  NOTES: Multiple internal structures
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLock
//               Allocated block address to be validated
//
//  Returns:     bResult
//                 true... Validated
//                 false.. Failed
bool
P2PmsgHeap_AssertValidAllocBSTRio ( P2PmsgHANDLE hP2PmsgHeap, VBLaddr aVBLock )
{
    bool          bResult = true;
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if ( !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLock ) )
    {  VBHEAP_ASSERT0(); return false; }      // Out of range, or straddling the end
    VBLock       *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( hP2PmsgHeap, aVBLock );
    if ( !VBLock_IsAlloc(pVBLock) )
    { bResult = false; VBHEAP_ASSERT0(); }
    //  The repair below sets a flag bit IN THE IMAGE. On a heap we own that is
    //  the hack its comment says it is; on an image off a socket it is the
    //  validator writing to the bytes it was asked to judge, so it is skipped
    //  inside a gate and the block is simply refused.
    if ( !VBLock_IsLinked(pVBLock) )   // TODO:LJM this hack needs tidying up
    { bResult = false; VBHEAP_ASSERT0();if(!P2PmsgHeap_InUntrustedGate()&&!VBLock_IsFree(pVBLock)&&VBLock_IsAddr(pVBLock,pHandle->uAddrType)&&VBLock_IsAlloc(pVBLock))pVBLock->oHdr.uVBLockDefs|=VBLock_Linked;}
    if (  VBLock_IsFree(pVBLock) )
    { bResult = false; VBHEAP_ASSERT0(); }
    if ( !VBLock_IsAddr(pVBLock,pHandle->uAddrType) )
    { bResult = false; VBHEAP_ASSERT0(); }
    return bResult;
}
#define P2PmsgHeap_AssertValidAllocIOMAGE P2PmsgHeap_AssertValidAllocBSTRio

bool
P2PmsgHeap_AssertValidBSTRio ( P2PmsgHANDLE hP2PmsgHeap )
{
    // Initialisation
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if( pHandle==0                              ||
        pHandle->uVBListType!=P2PmsgHeap_BSTRio    )
      return true;
    bool bResult = true;
    VBHEAP_DIAG((pHandle->uAddrType&VBLock_AddrMask)==pHandle->uAddrType);
    VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize<(VBLsize)200000000u);

    // Environmental
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFree == 0 )
      VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast == 0 );
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFree )
      VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast);
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast == 0 )
      VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.aFree == 0 );
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast )
      VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.aFree);

    // Iterate through free entries
    //  F11b on this arm too. The IOMAGE twin of this walk bounds itself against
    //  MaxBlocks and says "cyclic free list" where it does; this one never got
    //  the guard. Same defect, same wire-supplied links, and the asymmetry is
    //  the F3 lesson yet again -- the guard was added to the branch that had
    //  been reached rather than to the pattern. A cycle here spins a VALIDATOR,
    //  which is worse than it sounds: several callers invoke these directly
    //  rather than inside ASSERT, so this is live in a shipped Release binary.
    VBLelem nFreeEntries = 0;
    VBLsize aFreeSize    = 0;
    const VBLsize nMaxFree = P2PmsgHeap_MaxBlocks ( hP2PmsgHeap );
    VBLaddr aVBLockFree  = pHandle->u.BSTRio.pBSTRio->oKeys.aFree;
    while ( aVBLockFree )
    {
      //  The cast is not decoration. VBLelem is `int` and VBLsize is unsigned,
      //  so the bare comparison is C4018 on Win32 -- and copying the IOMAGE
      //  twin's line verbatim copied its warning along with it, which the CI
      //  warning ceiling caught as +1 on all four Win32 configurations while
      //  x64 stayed level. nFreeEntries starts at zero and only increments, so
      //  widening it is exact rather than a silencer.
      //  The twin in P2PmsgHeap_AssertValidIOMAGE still has the unfixed form
      //  and sits inside the 121/138 baseline. Fixing it there means moving four
      //  baselines and the readiness document in the same commit, which belongs
      //  in its own change rather than inside a security fix.
      if ( (VBLsize)nFreeEntries >= nMaxFree )
        return false;                    // cyclic free list
      nFreeEntries++;
      VBHeap *pVBLock = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLockFree );
      aFreeSize += VBHeap_Sizenn(pVBLock);
      VBHEAP_SUBWALK(P2PmsgHeap_AssertValidFree ( hP2PmsgHeap, aVBLockFree ));

      if ( aVBLockFree==pHandle->u.BSTRio.pBSTRio->oKeys.aFree)
        VBHEAP_DIAG(VBHeap_GetPrev(pVBLock,0)==0);
      if ( aVBLockFree==pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast)
        VBHEAP_DIAG(VBHeap_GetNext(pVBLock,0)==0);
      aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
    }
    //  No key fixups inside a gate - see the same note in the IOMAGE twin.
    VBHEAP_DIAG(aFreeSize==pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize);
    if ( aFreeSize!=pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize &&
         !P2PmsgHeap_InUntrustedGate()                            )
      pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize = aFreeSize;
    //VBHEAP_DIAG(nFreeEntries==pHandle->nFreeEntries||pHandle->nFreeEntries==0);
    if ( nFreeEntries!=pHandle->nFreeEntries)
      pHandle->nFreeEntries = nFreeEntries;
    VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries==nFreeEntries );
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries != nFreeEntries &&
         !P2PmsgHeap_InUntrustedGate()                                    )
      pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries = nFreeEntries;
    return bResult;
}

bool
P2PmsgHeap_AssertVBlocksBSTRio ( P2PmsgHANDLE hP2PmsgHeap )
{
    // Initialisation
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if( pHandle==0                              ||
        pHandle->uVBListType!=P2PmsgHeap_BSTRio    )
      return true;
    bool bResult = true;
    VBHEAP_DIAG((pHandle->uAddrType&VBLock_AddrMask)==pHandle->uAddrType);
    VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize<(VBLsize)200000000u);

    // Iterate through free entries 
    VBLelem nFreeEntries  = 0;
    VBLaddr nFreeSize     = 0;
    VBLelem nAllocEntries = 0;
    VBLaddr nAllocSize    = 0;
    VBLaddr aVBLock = (VBLaddr)&pHandle->u.BSTRio.pBSTRio->cTag - (VBLaddr)pHandle->u.BSTRio.pBSTRio;
    //  Item 19. These per-block tests used to be bare ASSERTs, so the walk
    //  returned true in Release no matter what it saw: the only thing that could
    //  fail it was the step check below. A caller running this to decide whether
    //  to accept an image therefore learned nothing outside a Debug build. They
    //  now feed the result as well as the assertion -- the ASSERT stays because
    //  stopping at the first bad block under a debugger is worth more than a
    //  bool, and the bool is what the shipped binary has.
    //
    //  A zero-filled block area is the case that motivates this. VBLock_Addr08
    //  is 0, so an all-zero header is a structurally VALID 8-bit block of size
    //  0 -- which is why a zero-filled file walks instead of failing, and why
    //  "it did not crash" was never evidence that it had been rejected.
    while ( aVBLock < pHandle->nSizeofAlloc )
    {
      //  The loop condition bounds the block's first byte only. Everything read
      //  below is further in, so the whole control block has to fit before any
      //  of it is touched -- refuse rather than continue, because a block that
      //  runs off the end tells us nothing about where the next one starts.
      if ( !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLock ) )
        return false;
      VBHeap *pVBLock = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLock );
      if ( !VBHeap_IsLinked(pVBLock) ) { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsAlloc(pVBLock)  ) { bResult = false; VBHEAP_ASSERT0(); }
      if ( VBHeap_IsFree(pVBLock) )
      {
        if ( !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) ) { bResult = false; VBHEAP_ASSERT0(); }
        if ( !P2PmsgHeap_AssertValidFree ( hP2PmsgHeap, aVBLock ) ) bResult = false;
        nFreeEntries++;
        nFreeSize += VBHeap_Sizenn(pVBLock);
      }
      else
      {
        if ( !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) ) { bResult = false; VBHEAP_ASSERT0(); }
        if ( !P2PmsgHeap_AssertValidAlloc ( hP2PmsgHeap, aVBLock ) ) bResult = false;
        nAllocEntries++;
        nAllocSize += VBHeap_Sizenn(pVBLock);
      }
      //  Same wire-controlled step as the IOMAGE walk above, same two fatal
      //  values, and the same reason to refuse rather than continue: the fixups
      //  below rewrite the image's own key block from what the walk counted.
      const VBLsize nStep = VBHeap_Sizenn(pVBLock);
      if ( nStep == 0 || nStep > pHandle->nSizeofAlloc )
        return false;                    // corrupt image: do not "correct" the keys
      aVBLock += nStep;
    }
    if ( !P2PmsgHeap_InUntrustedGate() )
    {
      VBHEAP_DIAG(nFreeSize==pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize);
      if ( nFreeSize!=pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize )
        pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize = nFreeSize;
      VBHEAP_DIAG(nAllocSize==pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize);
      if ( nAllocSize!=pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize )
        pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize = nAllocSize;
      //VBHEAP_DIAG(nFreeEntries==pHandle->nFreeEntries||pHandle->nFreeEntries==0);
      if ( nFreeEntries!=pHandle->nFreeEntries)
        pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize = nFreeSize;
      VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries==nFreeEntries );
      if ( pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries != nFreeEntries )
        pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries = nFreeEntries;
      VBHEAP_DIAG(pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries==nAllocEntries );
      if ( pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries != nAllocEntries )
        pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries = nAllocEntries;
    }
    else
    {
      //  INSIDE A GATE the four counters are CHECKED and never written. A key
      //  block that disagrees with the blocks it describes is one of the ways
      //  a forged image announces itself, and rewriting it to agree destroys
      //  the evidence and adopts the image in the same statement.
      if ( nFreeSize      != pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize     ||
           nAllocSize     != pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize    ||
           nFreeEntries   != pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries  ||
           nAllocEntries  != pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries    )
        bResult = false;
    }
    //  Outside a gate the key fixups above stay unconditional even when bResult
    //  is false. They only ever write back what this walk itself counted, and a
    //  caller that rejects on the return value discards the whole heap anyway;
    //  a caller that ignores it is no worse off with counts that match the
    //  blocks than with counts that do not.
    return bResult;
}

bool
P2PmsgHeap_AssertVBlocksIOMAGE ( P2PmsgHANDLE hP2PmsgHeap )
{
    // Initialisation
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hP2PmsgHeap);
    if( pHandle==0                              ||
        pHandle->uVBListType!=P2PmsgHeap_IOMAGE    )
      return false;
    bool bResult = true;
    VBHEAP_DIAG((pHandle->uAddrType&VBLock_AddrMask)==pHandle->uAddrType);
    VBHEAP_DIAG(VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot)<(VBLsize)200000000u);

    // Iterate through free entries 
    VBLelem nFreeEntries  = 0;
    VBLaddr nFreeSize     = 0;
    VBLelem nAllocEntries = 0;
    VBLaddr nAllocSize    = 0;
    VBLaddr aVBLock       = sizeof(VBHeapIOMAGE) - sizeof(VBHeapIOMAGE::oRoot) + VBHeapRoot_Sizeof(pHandle->u.IOMAGE.pRoot);
    //VBLaddr aVBLock = (VBLaddr)&pHandle->u.IOMAGE.pIOMAGE->cTag - (VBLaddr)pHandle->u.IOMAGE.pIOMAGE;
    //  Item 19, and the same treatment its BSTRio twin got in Stage C. These
    //  per-block tests were bare ASSERTs, so in Release the walk returned true
    //  no matter what it saw and only the step check below could fail it -- a
    //  caller running this to decide whether to accept an image learned nothing
    //  outside a Debug build. They now feed the result as well as the assertion.
    while ( aVBLock < pHandle->nSizeofAlloc )
    {
      //  F3, and see the longer note in P2PmsgHeap_AssertValidIOMAGE above. The
      //  loop condition bounds this block's FIRST BYTE; everything read below is
      //  further into a control block that may not fit. Refuse rather than
      //  continue -- a block running off the end says nothing about where the
      //  next one starts.
      //
      //  This does NOT conflict with the "blocks may overhang" note further
      //  down. BlockFits requires the HEADER to fit (aVBLock + VBHeapMin), not
      //  the body; the legitimate last-block case that note describes is about
      //  aVBLock + nStep, which is left alone deliberately.
      if ( !P2PmsgHeap_BlockFits ( hP2PmsgHeap, aVBLock ) )
        return false;
      VBHeap *pVBLock = VBList2PhysVBHeap ( hP2PmsgHeap, aVBLock );
      if ( !VBHeap_IsLinked(pVBLock) ) { bResult = false; VBHEAP_ASSERT0(); }
      if ( !VBHeap_IsAlloc(pVBLock)  ) { bResult = false; VBHEAP_ASSERT0(); }
      if ( VBHeap_IsFree(pVBLock) )
      {
        if ( !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) ) { bResult = false; VBHEAP_ASSERT0(); }
        if ( !P2PmsgHeap_AssertValidFree ( hP2PmsgHeap, aVBLock ) ) bResult = false;
        nFreeEntries++;
        nFreeSize += VBHeap_Sizenn(pVBLock);
      }
      else
      {
        if ( !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) ) { bResult = false; VBHEAP_ASSERT0(); }
        if ( !P2PmsgHeap_AssertValidAlloc ( hP2PmsgHeap, aVBLock ) ) bResult = false;
        nAllocEntries++;
        nAllocSize += VBHeap_Sizenn(pVBLock);
      }
      //  Inside a gate there is nothing further to learn from a block chain
      //  that has already been shown false, and a great deal of time to spend
      //  learning it: `big_coalesced` walks 65 KB of forged chain and used to
      //  trip 79,441 assertions doing it. Stop at the first block that does not
      //  hold up. Outside a gate the walk still runs to the end, because a
      //  developer looking at a heap of ours wants every bad block listed.
      if ( !bResult && P2PmsgHeap_InUntrustedGate() )
        return false;
      //  The step comes off the wire, and two values of it are fatal.
      //
      //  A ZERO step never terminates — this loop is where
      //  `p2p_fuzzframe 0x5EEDF00D --replay 0 2` hangs.
      //
      //  A step LARGER THAN THE WHOLE IMAGE is proof of corruption: no block
      //  can be bigger than the heap containing it. One mutated byte in the top
      //  half of the first block's 32-bit size turns a 168-byte block into a
      //  33,554,600-byte one inside a 2,032-byte heap. Refusing matters more
      //  than the walk does, because the code below OVERWRITES the root's
      //  free/alloc accounting with whatever the walk counted — so a corrupt
      //  image was not merely mis-read, it was ADOPTED.
      //
      //  Deliberately NOT rejected: a block that ends past nSizeofAlloc. That
      //  looks like the obvious invariant and is not one — a legitimate heap in
      //  this tree ends its last block exactly one header past nSizeofAlloc
      //  (measured: step 7012 at address 28 of a 7037-byte image, header 3).
      //  Enforcing "every block fits" broke p2p_expreg on valid traffic.
      const VBLsize nStep = VBHeap_Sizenn(pVBLock);
      if ( nStep == 0 || nStep > pHandle->nSizeofAlloc )
        return false;                    // corrupt image: do not "correct" the root
      aVBLock += nStep;
    }
    if ( !P2PmsgHeap_InUntrustedGate() )
    {
      VBHEAP_DIAG(nFreeSize==VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot));
      if ( nFreeSize!=VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot) )
        VBHeapRoot_SetFreeSize(pHandle->u.IOMAGE.pRoot,nFreeSize,true);
      VBHEAP_DIAG(nAllocSize==VBHeapRoot_GetAllocSize(pHandle->u.IOMAGE.pRoot));
      if ( nAllocSize!=VBHeapRoot_GetAllocSize(pHandle->u.IOMAGE.pRoot) )
        VBHeapRoot_SetAllocSize(pHandle->u.IOMAGE.pRoot,nAllocSize);
      //VBHEAP_DIAG(nFreeEntries==pHandle->nFreeEntries||pHandle->nFreeEntries==0);
      if ( nFreeEntries!=pHandle->nFreeEntries)
        pHandle -> nFreeEntries = nFreeEntries;
      VBHEAP_DIAG(VBHeapRoot_GetFreeItems(pHandle->u.IOMAGE.pRoot)==nFreeEntries );
      if ( VBHeapRoot_GetFreeItems(pHandle->u.IOMAGE.pRoot) != nFreeEntries )
        VBHeapRoot_SetFreeItems(pHandle->u.IOMAGE.pRoot,nFreeEntries);
      VBHEAP_DIAG(VBHeapRoot_GetAllocItems(pHandle->u.IOMAGE.pRoot)==nAllocEntries );
      if ( VBHeapRoot_GetAllocItems(pHandle->u.IOMAGE.pRoot) != nAllocEntries )
        VBHeapRoot_SetAllocItems(pHandle->u.IOMAGE.pRoot,nAllocEntries,true);
    }
    else
    {
      //  THE ROOT IS CHECKED AND NEVER WRITTEN inside a gate. The comment at
      //  the step check above already says what the fixups cost on an untrusted
      //  image -- "a corrupt image was not merely mis-read, it was ADOPTED" --
      //  and this is the other end of that sentence. Four counters that
      //  disagree with the chain they describe are a refusal, not a repair.
      //
      //  nFreeEntries is deliberately compared and NOT copied into
      //  pHandle->nFreeEntries either: that field is the handle's cache of the
      //  root's, and priming it from a chain we are about to reject would leave
      //  a discarded image's arithmetic behind in a live handle.
      if ( nFreeSize    != VBHeapRoot_GetFreeSize  ( pHandle->u.IOMAGE.pRoot ) ||
           nAllocSize   != VBHeapRoot_GetAllocSize ( pHandle->u.IOMAGE.pRoot ) ||
           (VBLelem)VBHeapRoot_GetFreeItems ( pHandle->u.IOMAGE.pRoot )
                        != nFreeEntries                                        ||
           (VBLelem)VBHeapRoot_GetAllocItems( pHandle->u.IOMAGE.pRoot )
                        != nAllocEntries                                          )
        bResult = false;
    }
    //  Outside a gate the root fixups above stay unconditional even when
    //  bResult is false, for the reason the BSTRio twin gives: they only ever
    //  write back what this walk itself counted, and a caller that rejects on
    //  the return value discards the heap anyway.
    return bResult;
}

//
//  Resizes the BSTRio VBListBSTRio, and adds the extra as a free entry
//  NOTES: Assumes extra is added as a single free entry at the end of the
//         existing VBListBSTRio, and that the existing last entry is displaced
//         to accommodate the new free entry.
//       : The underlying VBListBSTRio is both resized and reallocated in
//         memory, and the existing data is copied across. The new free entry is
//         then initialised, and the existing last entry is updated to link to
//         the new free entry. The VBListBSTRio keys are then updated to reflect
//         the new free entry, and the VBListHANDLE is updated to reflect the 
//         new VBListBSTRio, and the new size of the VBListBSTRio.
//       : The VBListHANDLE is also marked as dirty to indicate that the
//         VBListBSTRio has been modified and may need to be written back to
//         the underlying storage.
//       : Any external physical VBHeap addresses to the existing VBListBSTRio
//         are no longer valid after this operation. For this reason they should
//         only transiently exist.
//       : Any internal VBHeap address remain valid. These stored and referenced
//         extensively within the within the VBListBSTRio.
//       
// 
//
//  Parameters:  P2PmsgHANDLE hVBList
//               Handle of VBListBSTRio to be resized
//
//               VBLsize nSizeofExtra
//               Size of extra memory in bytes to be added to VBListBSTRio
//
//               bool bOversize
//                 true... Oversize VBListBSTRio by at least 20% of current
//                         size if extra is insufficient
//
//  Returns:     VBLsize
//               Size of extra memory in bytes added to hVBList
VBLsize
P2PmsgHeap_ResizeBSTRio( P2PmsgHANDLE hVBList, VBLsize nSizeofExtra, bool bOversize )
{
    // Locals
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    // Old VBListBSTRio
    VBLsize nSizeofOldAlloc = pHandle -> nSizeofAlloc;
    const VBLsize nSizeofNeeded = nSizeofExtra;     // before the oversize bump
    if (  bOversize                                  &&
         (nSizeofOldAlloc*2)/10 > nSizeofExtra    )
      nSizeofExtra = (nSizeofOldAlloc * 2) / 10;

    // Ceiling (M6). This is the path the Addr16 measurement in C4LoadTest [12]
    // walked past 65535 -- a P2PmsgMgr is a BSTRio heap, so every grow an
    // ordinary store performs arrives here.
    nSizeofExtra = P2PmsgHeap_CapGrowth ( pHandle, nSizeofNeeded, nSizeofExtra );

    // New VBListBSTRio
    VBLsize nSizeofNewAlloc = nSizeofOldAlloc + nSizeofExtra;
    VBListBSTRio  *pBSTRio  = (VBListBSTRio *)new char [nSizeofNewAlloc+4]();   // zero grown tail -> deterministic serialised slack (§4.2)
    memcpy ( pBSTRio, pHandle->u.BSTRio.pBSTRio, nSizeofOldAlloc );
    delete [] (char *)pHandle->u.BSTRio.pBSTRio;
    pHandle -> u.BSTRio.pBSTRio    =       pBSTRio;
    pHandle -> u.BSTRio.aBSTRio    = reinterpret_cast<VBLaddr>(pBSTRio);
    pHandle -> u.BSTRio.pBSTRio -> oKeys.aFreeSize  += nSizeofExtra;
    pHandle -> u.BSTRio.pBSTRio -> oKeys.nFreeEntries++;
    pHandle -> nFreeEntries++;
    pHandle -> nSizeofAlloc        = nSizeofNewAlloc;
    pHandle -> u.BSTRio.pBSTRio -> oSize.aSize1 =  nSizeofNewAlloc;
    pHandle -> u.BSTRio.pBSTRio -> oSize.aComp2 = ~nSizeofNewAlloc;
    pHandle -> bDirty              = true;

    // Extra assignment as free entry
    // NOTES: Assumes role as last free entry in list
    VBLaddr     aFreeLast    = pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast;
    VBLaddr     aVBLockExtra = nSizeofOldAlloc;// + sizeof(VBListBSTRio)/* LJM added 12/3/2011*/;
    VBHeap     *pVBLockExtra = VBList2PhysVBHeap ( hVBList, aVBLockExtra );
    VBHeap_Init    ( pVBLockExtra, pHandle->uAddrType, nSizeofExtra );
    VBHeap_SetPrev ( pVBLockExtra, aFreeLast );
    VBHeap_SetNext ( pVBLockExtra, 0 );
    pVBLockExtra -> oHdr.uVBLockDefs |=  VBLock_Linked;
    pVBLockExtra -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast = aVBLockExtra;
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFree == 0)
      pHandle->u.BSTRio.pBSTRio->oKeys.aFree = aVBLockExtra;

    // Displace existing last entry
    if ( aFreeLast )
    {
      VBHeap *pFreeLast = (VBHeap *)P2PmsgHeap_Addr2Phys ( hVBList, aFreeLast );
      VBHeap_SetNext ( pFreeLast, aVBLockExtra );
    }

    //  The grown tail is a free block like any other, and it is the one most
    //  worth tagging: it is what the block below it merges into.
    P2PmsgHeap_StampFoot ( hVBList, aVBLockExtra );

    // Tidy up, and
ASSERT(VBHeap_IsLinked(pVBLockExtra));
ASSERT(VBHeap_IsAlloc(pVBLockExtra));
ASSERT(VBHeap_IsFree(pVBLockExtra));
//P2PmsgHeap_AssertVBlocksBSTRio(hVBHeap);
P2PmsgHeap_AssertValidBSTRio(hVBList);
    return nSizeofExtra;
}

void
P2PmsgHeap_IsolateBSTRio ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    // Locals
    VBListHANDLE *pHandle       = static_cast<VBListHANDLE *>(hVBList);
    VBHeap       *pVBLock       = VBList2PhysVBHeap ( hVBList, aVBLock );
    VBLaddr       aVBLockPrev   = VBHeap_GetPrev   ( pVBLock, 0 );
    VBLaddr       aVBLockNext   = VBHeap_GetNext   ( pVBLock, 0 );
    VBLsize       nVBLockSizenn = VBHeap_Sizenn    ( pVBLock );
ASSERT(VBHeap_IsLinked(pVBLock));
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));

    // Allocated list management
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc == aVBLock )
    {
      if ( aVBLockPrev )
        pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc = aVBLockPrev;
      else
        pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc = aVBLockNext;
    }

    // Free entries list management
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFree == aVBLock )
    {
      if ( aVBLockPrev )
        pHandle->u.BSTRio.pBSTRio->oKeys.aFree = aVBLockPrev;
      else
        pHandle->u.BSTRio.pBSTRio->oKeys.aFree = aVBLockNext;
    }
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast == aVBLock )
    {
      if ( aVBLockNext )
        pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast = aVBLockNext;
      else
        pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast = aVBLockPrev;
    }

    // Backwards isolation
    // NOTES: Observe boundary conditions
    if ( aVBLockPrev )
    {
      VBHeap *pVBLockPrev = VBList2PhysVBHeap ( hVBList, aVBLockPrev );
//ASSERT(VBLock_IsLinked(pVBLockPrev));
//ASSERT(VBLock_IsAlloc(pVBLockPrev));
//ASSERT(VBLock_IsAddr(pVBLockPrev,pHandle->uAddrType));
ASSERT(pVBLockPrev!=pVBLock);
      VBHeap_SetNext ( pVBLockPrev, aVBLockNext );
      VBHeap_SetPrev ( pVBLock, 0 );   // Clears backwards reference
    }

    // Forwards isolation
    // NOTES: Observe boundary conditions
    if ( aVBLockNext )
    {
      VBHeap *pVBLockNext = VBList2PhysVBHeap ( hVBList, aVBLockNext );
//ASSERT(VBLock_IsLinked(pVBLockNext));
//ASSERT(VBLock_IsAlloc(pVBLockNext));
//ASSERT(VBLock_IsAddr(pVBLockNext,pHandle->uAddrType));
      ASSERT(pVBLockNext!=pVBLock);
      VBHeap_SetPrev ( pVBLockNext, aVBLockPrev );
      VBHeap_SetNext ( pVBLock, 0 );   // Clears forwards reference
    }

    // Tidy up, and
    // NOTES: One and only space decrements
    //if ( VBLock_IsFree(pVBLock) )
    //  pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  -= VBLock_Hdr_u_SizeNN(pVBLock);
    //else
    //  pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize -= VBLock_Hdr_u_SizeNN(pVBLock);
    pVBLock -> oHdr.uVBLockDefs &= ~VBLock_Linked;
    pVBLock -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    ASSERT(pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize>=nVBLockSizenn);
    pHandle -> u.BSTRio.pBSTRio->oKeys.aFreeSize  -= nVBLockSizenn;
    pHandle -> u.BSTRio.pBSTRio->oKeys.nFreeEntries--;
    pHandle -> nFreeEntries--;
    pHandle -> bDirty = TRUE;
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
}
void
P2PmsgHeap_IsolateIOMAGE ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    // Locals
    VBListHANDLE *pHandle       = static_cast<VBListHANDLE *>(hVBList);
    VBHeap       *pVBLock       = VBList2PhysVBHeap ( hVBList, aVBLock );
    VBLaddr       aVBLockPrev   = VBHeap_GetPrev   ( pVBLock, 0 );
    VBLaddr       aVBLockNext   = VBHeap_GetNext   ( pVBLock, 0 );
    VBLaddr       nVBLockSizenn = VBHeap_Sizenn    ( pVBLock );
ASSERT(VBHeap_IsLinked(pVBLock));
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));

    // Allocated list management
    if ( VBHeapRoot_GetAlloc(pHandle->u.IOMAGE.pRoot) == aVBLock )
    {
      if ( aVBLockPrev )
        VBHeapRoot_SetAlloc ( pHandle->u.IOMAGE.pRoot, aVBLockPrev );
      else
        VBHeapRoot_SetAlloc ( pHandle->u.IOMAGE.pRoot, aVBLockNext );
    }

    // Free entries list management
    if ( VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) == aVBLock )
    {
      if ( aVBLockPrev )
        VBHeapRoot_SetFree ( pHandle->u.IOMAGE.pRoot, aVBLockPrev );
      else
        VBHeapRoot_SetFree ( pHandle->u.IOMAGE.pRoot, aVBLockNext );
    }
    if ( VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) == aVBLock )
    {
      if ( aVBLockNext )
        VBHeapRoot_SetFreeLast ( pHandle->u.IOMAGE.pRoot, aVBLockNext );
      else
        VBHeapRoot_SetFreeLast ( pHandle->u.IOMAGE.pRoot, aVBLockPrev );
    }

    // Backwards isolation
    // NOTES: Observe boundary conditions
    if ( aVBLockPrev )
    {
      VBHeap *pVBLockPrev = VBList2PhysVBHeap ( hVBList, aVBLockPrev );
//ASSERT(VBLock_IsLinked(pVBLockPrev));
//ASSERT(VBLock_IsAlloc(pVBLockPrev));
//ASSERT(VBLock_IsAddr(pVBLockPrev,pHandle->uAddrType));
ASSERT(pVBLockPrev!=pVBLock);
      VBHeap_SetNext ( pVBLockPrev, aVBLockNext );
      VBHeap_SetPrev ( pVBLock, 0 );   // Clears backwards reference
    }

    // Forwards isolation
    // NOTES: Observe boundary conditions
    if ( aVBLockNext )
    {
      VBHeap *pVBLockNext = VBList2PhysVBHeap ( hVBList, aVBLockNext );
//ASSERT(VBLock_IsLinked(pVBLockNext));
//ASSERT(VBLock_IsAlloc(pVBLockNext));
//ASSERT(VBLock_IsAddr(pVBLockNext,pHandle->uAddrType));
      ASSERT(pVBLockNext!=pVBLock);
      VBHeap_SetPrev ( pVBLockNext, aVBLockPrev );
      VBHeap_SetNext ( pVBLock, 0 );   // Clears forwards reference
    }

    // Tidy up, and
    // NOTES: One and only space decrements
    //if ( VBLock_IsFree(pVBLock) )
    //  pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  -= VBLock_Hdr_u_SizeNN(pVBLock);
    //else
    //  pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize -= VBLock_Hdr_u_SizeNN(pVBLock);
    pVBLock -> oHdr.uVBLockDefs &= ~VBLock_Linked;
    pVBLock -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    ASSERT(VBHeapRoot_GetFreeSize(pHandle->u.IOMAGE.pRoot)>=nVBLockSizenn);
    VBHeapRoot_SetFreeSize(pHandle->u.IOMAGE.pRoot,-(int)nVBLockSizenn);
    //pHandle -> u.BSTRio.pBSTRio->oKeys.aFreeSize  -= nVBLockSizenn;
    VBHeapRoot_SetFreeItems(pHandle->u.IOMAGE.pRoot,-1);
    //pHandle -> u.BSTRio.pBSTRio->oKeys.nFreeEntries--;
    pHandle -> nFreeEntries--;
    pHandle -> bDirty = true;
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
}

//
//  Collates adjacent free entries
//  NOTES: Effective garbage collection requires entries to be collated
//         upon release and immediately prior to re-use
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLock
//               Block address of free entry to be collated
//
//  Returns:     VBLsize
//               Size of collated free entry
VBLsize
P2PmsgHeap_CollateBSTRio ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBLock       *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLock );
    UCHAR         uVBLock = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( !VBLock_IsFree(pVBLock) ) 
      EVERR->MODULE->AFP(aVBLock)      // Fundamental logic issue
           ->Message("Attempt to collate non-free entry")->Assert()->Throw();
    pHandle -> bDirty = TRUE;

    // Confirmation
    // NOTES: Next physical block MUST be free and allocated
TOP:VBLsize nSizeof     = VBLock_Hdr_u_SizeNN ( pVBLock );
    VBLaddr aVBLockNext = aVBLock + nSizeof;
    if ( aVBLockNext >= pHandle->nSizeofAlloc )
      return nSizeof;                  // Beyond end of buffer
    VBLock *pVBLockNext = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLockNext );
    UCHAR   uVBLockNext = pVBLockNext->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLock != uVBLockNext ||
         aVBLock == aVBLockNext    )   // Self reference
    {
      //  Budgeted: this is the 96.7% emitter measured in fuzz run 32043591227.
      //  Refer nCollateCorrupt at the handle. The ->Assert() is inside the
      //  budget deliberately -- an assert storm on a malformed image is no more
      //  use than a log storm, and the first occurrence still asserts.
      if ( ++pHandle->nCollateCorrupt == 1 )
        EVERR->MODULE->AFP(aVBLock)
             ->Message("Internal address corruption")->Assert()->Cancel();
      return nSizeof;                  // First do no harm
    }
    if ( VBLock_IsFree(pVBLockNext) )
    {                                  // Only collate free buffer
      //  F11. THIS LOOP MUST MAKE PROGRESS, and nothing here required it to.
      //
      //  The termination argument is that each pass absorbs the next block, so
      //  nSizeof grows, so aVBLockNext advances and eventually leaves the
      //  buffer. It holds only while the absorbed block has a NON-ZERO declared
      //  size, and that size comes out of the image.
      //
      //  With a zero-sized neighbour: nSizeof += 0, aVBLockNext is unchanged,
      //  and the `uVBLockDefs = 0` below makes the neighbour read as FREE again
      //  next pass -- VBLock_IsFree is true when the type bits are zero, which
      //  is exactly what that line writes. So the state at TOP is identical and
      //  the loop never ends.
      //
      //  IT NEEDS uVBLock == VBLock_Addr08 TO SURVIVE A SECOND PASS, and that
      //  is why this went unnoticed. Zeroing the neighbour clears its address
      //  bits, so the uVBLock != uVBLockNext refusal above catches the repeat
      //  for Addr16, Addr32 and Addr64 -- but VBLock_Addr08 IS ZERO, so on an
      //  8-bit-addressed image 0 == 0 and the guard is vacuous. An image
      //  declares its own addressing width, so it selects the one arm where
      //  the existing check does not fire.
      //
      //  Refusing rather than breaking: a free block of no size is not a block,
      //  and the surrounding code treats a malformed neighbour as a report and
      //  a return rather than a throw -- "First do no harm", above. This says
      //  the same thing about the same class of damage.
      if ( VBLock_Hdr_u_SizeNN(pVBLockNext) == 0 )
      {
        //  Report the first occurrence on this heap only; P2PmsgHeap_Close
        //  emits the tally. Refer nCollateZeroSize at the handle for why the
        //  report is budgeted and the refusal below is not.
        if ( ++pHandle->nCollateZeroSize == 1 )
          EVERR->MODULE->AFP(aVBLockNext)
               ->Message(L"Collate: free neighbour declares zero size"
                         L" (uAddrType=%u) -- collation cannot advance"
                        , (UINT)uVBLock )
               ->Cancel();
        //  NO ->Assert() HERE, and the omission is the point. The adjacent
        //  "Internal address corruption" branch has one, so matching it was the
        //  first instinct -- and tools/ci/check_asserts.ps1 rejected it, at
        //  215 -> 217 predicate sites, with the argument that settles it: an
        //  ASSERT is compiled out of the binary anybody ships, so if the
        //  condition matters in Release it must be handled there and if it does
        //  not it does not need asserting. THIS CONDITION MATTERS IN RELEASE.
        //  It is where the F11 hang was, in a ReleaseLib fuzz harness. The
        //  report and the refusal to advance ARE the Release behaviour, and
        //  they run either way; the ASSERT would only have added a Debug-only
        //  dialog to a path whose whole defect was blocking on one.
        return nSizeof;                // First do no harm
      }
      P2PmsgHeap_IsolateBSTRio ( pHandle, aVBLockNext );
      pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize += VBLock_Hdr_u_SizeNN(pVBLockNext);
      if ( uVBLock == VBLock_Addr32 )
        pVBLock->oHdr.u.nSize32 += pVBLockNext->oHdr.u.nSize32;
      else if ( uVBLock == VBLock_Addr64 )
        pVBLock->oHdr.u.nSize64 += pVBLockNext->oHdr.u.nSize64;
      else if ( uVBLock == VBLock_Addr16 )
        pVBLock->oHdr.u.nSize16 += pVBLockNext->oHdr.u.nSize16;
      else if ( uVBLock == VBLock_Addr08 )
        pVBLock->oHdr.u.nSize08 += pVBLockNext->oHdr.u.nSize08;
      pVBLockNext->oHdr.uVBLockDefs = 0;       
      //  The block just grew, so its tag moved with its end.
      P2PmsgHeap_StampFoot ( hVBList, aVBLock );
      goto TOP;
    }
    P2PmsgHeap_StampFoot ( hVBList, aVBLock );
    return nSizeof;
}
VBLsize
P2PmsgHeap_CollateIOMAGE ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBLock       *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLock );
    UCHAR         uVBLock = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( !VBLock_IsFree(pVBLock) ) 
      EVERR->MODULE->AFP(aVBLock)      // Fundamental logic issue
           ->Message("Attempt to collate non-free entry")->Assert()->Throw();
    pHandle -> bDirty = TRUE;
    ASSERT(uVBLock==pHandle->uAddrType);

    // Confirmation
    // NOTES: Next physical block MUST be free and allocated
TOP:VBLsize nSizeof     = VBLock_Hdr_u_SizeNN ( pVBLock );
    VBLaddr aVBLockNext = aVBLock + nSizeof;
    if ( aVBLockNext >= pHandle->nSizeofAlloc )
      return nSizeof;                  // Beyond end of buffer
    VBLock *pVBLockNext = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLockNext );
    UCHAR   uVBLockNext = pVBLockNext->oHdr.uVBLockDefs & VBLock_AddrMask;
    if ( uVBLock != uVBLockNext ||
         aVBLock == aVBLockNext    )   // Self reference
    {
      //  Budgeted exactly as the BSTRio arm above -- same emitter, same reason,
      //  same shared handle counter.
      if ( ++pHandle->nCollateCorrupt == 1 )
        EVERR->MODULE->AFP(aVBLock)
             ->Message("Internal address corruption")->Assert()->Cancel();
      return nSizeof;                  // First do no harm
    }
    if ( VBLock_IsFree(pVBLockNext) )
    {                                  // Only collate free buffer
      //  F11 on this arm too -- refer P2PmsgHeap_CollateBSTRio above for the
      //  mechanism in full. Identical loop, identical missing requirement, and
      //  the same VBLock_Addr08 == 0 that makes the address-type refusal above
      //  vacuous on the one width an image would choose to reach it.
      if ( VBLock_Hdr_u_SizeNN(pVBLockNext) == 0 )
      {
        //  Budgeted exactly as the BSTRio arm above -- same reason, and the
        //  tally shares the same handle counter.
        if ( ++pHandle->nCollateZeroSize == 1 )
          EVERR->MODULE->AFP(aVBLockNext)
               ->Message(L"Collate: free neighbour declares zero size"
                         L" (uAddrType=%u) -- collation cannot advance"
                        , (UINT)uVBLock )
               ->Cancel();
        //  NO ->Assert() HERE, and the omission is the point. The adjacent
        //  "Internal address corruption" branch has one, so matching it was the
        //  first instinct -- and tools/ci/check_asserts.ps1 rejected it, at
        //  215 -> 217 predicate sites, with the argument that settles it: an
        //  ASSERT is compiled out of the binary anybody ships, so if the
        //  condition matters in Release it must be handled there and if it does
        //  not it does not need asserting. THIS CONDITION MATTERS IN RELEASE.
        //  It is where the F11 hang was, in a ReleaseLib fuzz harness. The
        //  report and the refusal to advance ARE the Release behaviour, and
        //  they run either way; the ASSERT would only have added a Debug-only
        //  dialog to a path whose whole defect was blocking on one.
        return nSizeof;                // First do no harm
      }
      P2PmsgHeap_IsolateIOMAGE ( pHandle, aVBLockNext );
      VBHeapRoot_SetFreeSize ( pHandle->u.IOMAGE.pRoot,VBLock_Hdr_u_SizeNN(pVBLockNext) );
      //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize += VBLock_Hdr_u_SizeNN(pVBLockNext);
      if ( uVBLock == VBLock_Addr32 )
        pVBLock->oHdr.u.nSize32 += pVBLockNext->oHdr.u.nSize32;
      else if ( uVBLock == VBLock_Addr64 )
        pVBLock->oHdr.u.nSize64 += pVBLockNext->oHdr.u.nSize64;
      else if ( uVBLock == VBLock_Addr16 )
        pVBLock->oHdr.u.nSize16 += pVBLockNext->oHdr.u.nSize16;
      else if ( uVBLock == VBLock_Addr08 )
        pVBLock->oHdr.u.nSize08 += pVBLockNext->oHdr.u.nSize08;
      pVBLockNext->oHdr.uVBLockDefs = 0;       
      //  The block just grew, so its tag moved with its end.
      P2PmsgHeap_StampFoot ( hVBList, aVBLock );
      goto TOP;
    }
    P2PmsgHeap_StampFoot ( hVBList, aVBLock );
    return nSizeof;
}

VBLaddr
P2PmsgHeap_SplitAllocBSTRio ( P2PmsgHANDLE hVBList
                            , VBLaddr aVBLockFree, VBLsize& nSizeof )
{
    // Locals
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBHeap       *pVBLockFree = VBList2PhysVBHeap ( hVBList, aVBLockFree );
    UCHAR         uVBLock     = pVBLockFree->oHdr.uVBLockDefs;
    VBLaddr       aVBLockPrev = VBHeap_GetPrev ( pVBLockFree, 0 );
    VBLaddr       aVBLockNext = VBHeap_GetNext ( pVBLockFree, 0 );
    pHandle -> bDirty = true;

ASSERT(VBHeap_IsAddr(pVBLockFree,pHandle->uAddrType));
ASSERT(VBHeap_IsLinked(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(VBHeap_IsFree(pVBLockFree));
//ASSERT(P2PmsgHeap_AssertValidBSTRio(hVBHeap));
    // VBLock adoption
    // NOTES: Insufficient extra to split block
    VBLsize nSizeofFree  = VBHeap_Sizenn(pVBLockFree);
    ASSERT(nSizeofFree>=nSizeof);
    VBLsize nSizeofExtra = nSizeofFree - nSizeof;
    if ( nSizeofExtra < VBList_VBHeapMin(uVBLock) )
    {
      P2PmsgHeap_IsolateBSTRio(hVBList,aVBLockFree);
      nSizeof = nSizeofFree;
      pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize += nSizeof;
      pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries++;
      ASSERT(VBHeap_GetPrev(pVBLockFree,0)==0);
      ASSERT(VBHeap_GetNext(pVBLockFree,0)==0);
//ASSERT(VBHeap_IsAddr(pVBLockFree,pHandle->uAddrType));
//ASSERT(!VBHeap_IsLinked(pVBLockFree));
//ASSERT(VBHeap_IsAlloc(pVBLockFree));
//ASSERT(VBHeap_IsFree(pVBLockFree));
      ASSERT(VBHeap_Sizenn(pVBLockFree)==nSizeof);
      return aVBLockFree;
    }

    // VBLock extra
    // NOTES: Sufficient extra exists to create free entry from
    //        reminants.  After allocated VBLock but still retains
    //        its relative free entries list position
    VBLaddr  aVBLockExtra = aVBLockFree + nSizeof;
    VBHeap  *pVBLockExtra = (VBHeap *)VBList2PhysVBHeap ( hVBList, aVBLockExtra );
    VBHeap_Init ( pVBLockExtra, pHandle->uAddrType, nSizeofExtra );
    VBHeap_SetNext ( pVBLockExtra, aVBLockNext );
    VBHeap_SetPrev ( pVBLockExtra, aVBLockPrev );
    pVBLockExtra -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    pVBLockExtra -> oHdr.uVBLockDefs |=  VBLock_Linked;
    pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize += nSizeof;
    pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries++;
    pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  -= nSizeof;

    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFree == aVBLockFree )
      pHandle->u.BSTRio.pBSTRio->oKeys.aFree = aVBLockExtra;
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast == aVBLockFree )
      pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast = aVBLockExtra;
    //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize -= nSizeofFree;
    //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize += nSizeofExtra;

    if ( aVBLockPrev )
      VBHeap_SetNext ( VBList2PhysVBHeap(hVBList,aVBLockPrev), aVBLockExtra );
    if ( aVBLockNext )
      VBHeap_SetPrev ( VBList2PhysVBHeap(hVBList,aVBLockNext), aVBLockExtra );
    //  A brand new free block, so it needs a tag of its own. Without this the
    //  remainder of every split is invisible to the block after it.
    P2PmsgHeap_StampFoot ( hVBList, aVBLockExtra );
//ASSERT(VBHeap_IsAddr(pVBLockExtra,pHandle->uAddrType));
//ASSERT(VBHeap_IsFree(pVBLockExtra));
//ASSERT(VBHeap_IsAlloc(pVBLockExtra));
//ASSERT(VBHeap_Sizenn(pVBLockExtra)==nSizeofExtra);

    // Tidy up, and
    VBHeap_Init ( pVBLockFree, uVBLock, nSizeof );
    pVBLockFree -> oHdr.uVBLockDefs &= ~VBLock_Linked;
ASSERT(VBHeap_IsAddr(pVBLockFree,uVBLock&VBLock_AddrMask));
ASSERT(VBHeap_IsFree(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(!VBHeap_IsLinked(pVBLockFree));
    //if ( aVBLockFree+nSizeof > pHandle->u.BSTRio.uHiWM )
    //  pHandle -> u.BSTRio.uHiWM = aVBLockFree + nSizeof;
//ASSERT(P2PmsgHeap_AssertValidBSTRio(hVBHeap));
ASSERT(VBHeap_Sizenn(pVBLockFree)==nSizeof);
    return aVBLockFree;
}

VBLaddr
P2PmsgHeap_SplitAllocIOMAGE ( P2PmsgHANDLE hVBList
                            , VBLaddr aVBLockFree, VBLsize& nSizeof )
{
    // Locals
    VBListHANDLE *pHandle     = static_cast<VBListHANDLE *>(hVBList);
    VBHeap       *pVBLockFree = VBList2PhysVBHeap ( hVBList, aVBLockFree );
    UCHAR         uVBLock     = pVBLockFree->oHdr.uVBLockDefs;
    VBLaddr       aVBLockPrev = VBHeap_GetPrev ( pVBLockFree, 0 );
    VBLaddr       aVBLockNext = VBHeap_GetNext ( pVBLockFree, 0 );
    pHandle -> bDirty = true;

ASSERT((uVBLock&VBLock_AddrMask)==pHandle->uAddrType);
ASSERT(VBHeap_IsAddr(pVBLockFree,pHandle->uAddrType));
ASSERT(VBHeap_IsLinked(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(VBHeap_IsFree(pVBLockFree));
//ASSERT(P2PmsgHeap_AssertIOMAGE(hVBHeap));
    // VBLock adoption
    // NOTES: Insufficient extra to split block
    VBLsize nSizeofFree  = VBHeap_Sizenn(pVBLockFree);
    ASSERT(nSizeofFree>=nSizeof);
    VBLsize nSizeofExtra = nSizeofFree - nSizeof;
    if ( nSizeofExtra < VBList_VBHeapMin(uVBLock) )
    {
      P2PmsgHeap_IsolateIOMAGE(hVBList,aVBLockFree);
      nSizeof = nSizeofFree;
      VBHeapRoot_SetAllocSize(pHandle->u.IOMAGE.pRoot,nSizeof);
      //pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize += nSizeof;
      VBHeapRoot_SetAllocItems(pHandle->u.IOMAGE.pRoot,1);
      //pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries++;
      ASSERT(VBHeap_GetPrev(pVBLockFree,0)==0);
      ASSERT(VBHeap_GetNext(pVBLockFree,0)==0);
//ASSERT(VBHeap_IsAddr(pVBLockFree,pHandle->uAddrType));
//ASSERT(!VBHeap_IsLinked(pVBLockFree));
//ASSERT(VBHeap_IsAlloc(pVBLockFree));
//ASSERT(VBHeap_IsFree(pVBLockFree));
      ASSERT(VBHeap_Sizenn(pVBLockFree)==nSizeof);
      return aVBLockFree;
    }

    // VBLock extra
    // NOTES: Sufficient extra exists to create free entry from
    //        reminants.  After allocated VBLock but still retains
    //        its relative free entries list position
    VBLaddr  aVBLockExtra = aVBLockFree + nSizeof;
    VBHeap  *pVBLockExtra = (VBHeap *)VBList2PhysVBHeap ( hVBList, aVBLockExtra );
    VBHeap_Init ( pVBLockExtra, pHandle->uAddrType, nSizeofExtra );
    VBHeap_SetNext ( pVBLockExtra, aVBLockNext );
    VBHeap_SetPrev ( pVBLockExtra, aVBLockPrev );
    pVBLockExtra -> oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    pVBLockExtra -> oHdr.uVBLockDefs |=  VBLock_Linked;
    VBHeapRoot_SetAllocSize(pHandle->u.IOMAGE.pRoot,nSizeof);
    //pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize += nSizeof;
    VBHeapRoot_SetAllocItems(pHandle->u.IOMAGE.pRoot,1);
    //pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries++;
     VBHeapRoot_SetFreeSize(pHandle->u.IOMAGE.pRoot,-(int)nSizeof);
    //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  -= nSizeof;

    if ( VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot) == aVBLockFree )
      VBHeapRoot_SetFree(pHandle->u.IOMAGE.pRoot,aVBLockExtra);
      //pHandle->u.BSTRio.pBSTRio->oKeys.aFree = aVBLockExtra;
    if ( VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) == aVBLockFree )
      VBHeapRoot_SetFreeLast(pHandle->u.IOMAGE.pRoot,aVBLockExtra);
      //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast = aVBLockExtra;

    if ( aVBLockPrev )
      VBHeap_SetNext ( VBList2PhysVBHeap(hVBList,aVBLockPrev), aVBLockExtra );
    if ( aVBLockNext )
      VBHeap_SetPrev ( VBList2PhysVBHeap(hVBList,aVBLockNext), aVBLockExtra );
    //  A brand new free block, so it needs a tag of its own. Without this the
    //  remainder of every split is invisible to the block after it.
    P2PmsgHeap_StampFoot ( hVBList, aVBLockExtra );
//ASSERT(VBHeap_IsAddr(pVBLockExtra,pHandle->uAddrType));
//ASSERT(VBHeap_IsFree(pVBLockExtra));
//ASSERT(VBHeap_IsAlloc(pVBLockExtra));
//ASSERT(VBHeap_Sizenn(pVBLockExtra)==nSizeofExtra);

    // Tidy up, and
    VBHeap_Init ( pVBLockFree, uVBLock, nSizeof );
    pVBLockFree -> oHdr.uVBLockDefs &= ~VBLock_Linked;
ASSERT(VBHeap_IsAddr(pVBLockFree,uVBLock&VBLock_AddrMask));
ASSERT(VBHeap_IsFree(pVBLockFree));
ASSERT(VBHeap_IsAlloc(pVBLockFree));
ASSERT(!VBHeap_IsLinked(pVBLockFree));
ASSERT(VBHeap_IsAddr(pVBLockExtra,uVBLock&VBLock_AddrMask));
//P2PmsgHeap_AssertValidFree(hVBHeap,aVBLockExtra); //TODO:LJM wastes cycles
    //if ( aVBLockFree+nSizeof > pHandle->u.BSTRio.uHiWM )
    //  pHandle -> u.BSTRio.uHiWM = aVBLockFree + nSizeof;
//ASSERT(P2PmsgHeap_AssertValidIOMAGE(hVBHeap));
ASSERT(VBHeap_Sizenn(pVBLockFree)==nSizeof);
    return aVBLockFree;
}

//VBLaddr
//P2PmsgHeapBSTRio_Flush ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock )
//{
//    // Locals
//    VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
//    VBListBSTRio_SetAllocFirst ( pHandle->u.BSTRio.pBSTRio, pHandle->u.BSTRio.aAlloc );
//    VBListBSTRio_SetAllocSize  ( pHandle->u.BSTRio.pBSTRio, pHandle->u.BSTRio.aAllocSize );
//    VBListBSTRio_SetFreeFirst  ( pHandle->u.BSTRio.pBSTRio, pHandle->u.BSTRio.aFree );
//    VBListBSTRio_SetFreeLast   ( pHandle->u.BSTRio.pBSTRio, pHandle->u.BSTRio.aFreeLast );
//    VBListBSTRio_SetFreeSize   ( pHandle->u.BSTRio.pBSTRio, pHandle->u.BSTRio.aFree );
//}

bool
P2PmsgHeap_AssertValid ( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return P2PmsgHeap_AssertValidIOMAGE(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      return P2PmsgHeapSYS_AssertValid(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return P2PmsgHeap_AssertValidBSTRio(hVBList);
    ASSERT(0);
    return false;
}
bool
P2PmsgHeap_AssertVBlocks ( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return P2PmsgHeap_AssertVBlocksIOMAGE(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      return false;
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return P2PmsgHeap_AssertVBlocksBSTRio(hVBList);
    ASSERT(0);
    return false;
}
bool
P2PmsgHeap_AssertValidAlloc ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBHeap);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return P2PmsgHeap_AssertValidAllocIOMAGE(hVBHeap,aVBLock);
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      return P2PmsgHeap_AssertValidAllocSYS(hVBHeap,aVBLock);
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return P2PmsgHeap_AssertValidAllocBSTRio(hVBHeap,aVBLock);
    ASSERT(0);
    return false;
}

///////////////////////////////////////////////////////////////////////
//
//  VBHeap creation
//

P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE1 ( UCHAR uAddrType, VBLsize nSizeInitial, VBLsize nSizeMax )
{
    // To be sure, to be sure
    // Width first: an unsupported width must be reported as one, not as
    // whatever the size arithmetic below happens to say about it. Addr08 caps
    // nSizeMax at 255, so a default nSizeInitial of 2024 used to fail as
    // "nSizeInitial exceeds nSizeMax" -- true, unhelpful, and it named neither
    // the parameter at fault nor the reason.
    P2PmsgHeap_CheckCreateWidth ( uAddrType );
    UINT nVBLockMax = VBList_VBHeapMax(uAddrType);
    if ( nSizeMax == 0 )
      nSizeMax = nVBLockMax;
    if ( nSizeInitial <= 0 )
      nSizeInitial = 2024;
    if ( nSizeInitial > nSizeMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeInitial)->AFP(nSizeMax)
           ->Message("nSizeInitial exceeds nSizeMax")
           ->Throw();
    if ( nSizeMax > nVBLockMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeInitial)->AFP(nSizeMax)
           ->Message("nSizeMax exceeds allowable address limit(%x)"
                    , nVBLockMax )
           ->Throw();

    // Initialisation - VBListHANDLE
ASSERT(nSizeInitial);nSizeInitial+=8;//Delete testing only
    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType    = P2PmsgHeap_IOMAGE;
    pHandle -> uAddrType      = uAddrType & VBLock_AddrMask;
    //pHandle -> aVBListBuffer  = (UINT)pHandle -> pVBListBuffer;
    pHandle -> nSizeofInit    = nSizeInitial;
    pHandle -> nSizeofAlloc   = nSizeInitial;
    pHandle -> nSizeofMax     = nSizeMax;
    pHandle -> nVBLockMin     = VBList_VBHeapMin ( uAddrType );
    pHandle -> nRefCount      = 1;
    pHandle -> bEoD           = true;
    pHandle -> bDirty         = true;

    // Initialisation - IOMAGE
    VBLsize nSizeIOmage = nSizeInitial + sizeof(VBListIOmage);
    pHandle -> u.IOMAGE.pIOmage = (VBListIOmage *)new char [nSizeIOmage+4]();   // zero slack -> byte-identical image (§4.2)
    pHandle -> u.IOMAGE.pIOMAGE = (VBHeapIOMAGE *)pHandle -> u.IOMAGE.pIOmage;
#ifndef XCtrl_Hdr
    pHandle -> u.IOMAGE.uHiWM   = sizeof(VBListIOmage);  //was 0 @ 12/3/2011
#else
    pHandle -> u.IOMAGE.uHiWM   = nSizeIOmage; //LJM 10/01/2017 sizeof(VBHeapIOMAGE);  //was 0 @ 12/3/2011
#endif
    pHandle -> u.IOMAGE.aIOmage = (VBLaddr)pHandle -> u.IOMAGE.pIOmage;
#ifndef XCtrl_Hdr
    VBLockRoot_Init     ( &pHandle->u.IOMAGE.oRoot, uAddrType, nSizeInitial );
    VBListIOmage oIOmage;
    VBLockRoot_SetFirst ( &pHandle->u.IOMAGE.oRoot, sizeof(oIOmage.oSync) );
    VBLockRoot_SetLast  ( &pHandle->u.IOMAGE.oRoot, sizeof(oIOmage.oSync) );
    VBLockRoot_SetItems ( &pHandle->u.IOMAGE.oRoot, 1 );
#else
    pHandle->u.IOMAGE.pRoot = &pHandle->u.IOMAGE.pIOMAGE->oRoot;
    VBHeapRoot_Init  (  pHandle->u.IOMAGE.pRoot, uAddrType, nSizeInitial );
    VBHeapRoot_SetFree      ( pHandle->u.IOMAGE.pRoot, sizeof(VBHeapRoot) );
    VBHeapRoot_SetFreeLast  ( pHandle->u.IOMAGE.pRoot, sizeof(VBHeapRoot) );
    VBHeapRoot_SetFreeItems ( pHandle->u.IOMAGE.pRoot, 1 );
#endif

#ifndef XCtrl_Hdr
    VBHeap *pVBHeap = (VBHeap *)P2PmsgHeap_Addr2Phys ( pHandle, sizeof(VBListIOmage::oSync) );
#else
    VBHeap *pVBHeap = (VBHeap *)P2PmsgHeap_Addr2Phys ( pHandle, sizeof(VBHeapRoot::ud) );
#endif
    VBHeap_Init ( pVBHeap, pHandle->uAddrType, nSizeInitial );
    pVBHeap -> oHdr.uVBLockDefs |= VBLock_Linked;
    P2PmsgHeap_pIOmage ( pHandle );    // Completes synchronisation words
ASSERT(VBHeap_IsAlloc(pVBHeap));
ASSERT(VBHeap_IsLinked(pVBHeap));//TODO: delete
ASSERT(VBHeap_IsFree(pVBHeap));//TODO: delete
ASSERT(VBHeap_IsAddr(pVBHeap,pHandle->uAddrType));
//VBLaddr aConnect=P2PmsgHeap_Connect(pHandle);
//VBLsize aSize   =P2PmsgHeap_Sizeof ( pHandle, aConnect );
    // Tidy up, and
//VBHeapRoot_SetFreeItems ( pHandle->u.IOMAGE.pRoot, 0 ); //TODO:LJM delete debugging only
    return pHandle;
}
P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE ( UCHAR uAddrType, VBLsize nSizeInitial, VBLsize nSizeMax )
{
    // To be sure, to be sure
    P2PmsgHeap_CheckCreateWidth ( uAddrType );      // width before sizes
    VBLaddr nVBLockMax = VBList_VBHeapMax(uAddrType);
    if ( nSizeMax == 0 )
      nSizeMax = nVBLockMax;
    if ( nSizeInitial > nSizeMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeInitial)->AFP(nSizeMax)
           ->Message("nSizeInitial exceeds nSizeMax")
           ->Throw();
    if ( nSizeMax > nVBLockMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeInitial)->AFP(nSizeMax)
           ->Message("nSizeMax exceeds allowable address limit(%x)"
                    , nVBLockMax )
           ->Throw();

    // Initialisation
ASSERT(nSizeInitial);nSizeInitial+=8;//Delete testing only
    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType      = P2PmsgHeap_IOMAGE;
    pHandle -> uAddrType        = uAddrType & VBLock_AddrMask;
    //pHandle -> pVBListBuffer  = 0;
    //pHandle -> aVBListBuffer    = 0; //(UINT)pHandle -> pVBListBuffer;
    //pHandle -> nSizeofUsed    = 0;
    pHandle -> nSizeofInit      = nSizeInitial;
    pHandle -> nSizeofAlloc     = nSizeInitial;
    pHandle -> nSizeofMax       = nSizeMax;
    pHandle -> nVBLockMin       = VBList_VBHeapMin ( uAddrType );
    pHandle -> nRefCount        = 1;
    pHandle -> bEoD             = TRUE;
    pHandle -> bDirty           = TRUE;

    // Initialisation - IOMAGE
    // NOTES: Size rounded up to multiple of sizeof(VBLsize) bytes
    VBLsize nSizeofIOMAGE       =  nSizeInitial
                                + (sizeof(VBLsize) - (nSizeInitial % sizeof(VBLsize))) % sizeof(VBLsize);
    // Value-initialise (zero) the whole arena so serialised heap SLACK — the bytes
    // past the high-water mark that no field ever writes — is deterministically 0 on
    // every platform/config, instead of the allocator's fill (0x00 on Linux, the MSVC
    // debug-CRT 0xCD on Windows). This is what makes the saved image byte-identical
    // cross-OS; the init/memcpy below overwrites the used bytes.
    pHandle -> u.IOMAGE.pIOMAGE = (VBHeapIOMAGE *)new char [nSizeofIOMAGE+sizeof(VBLsize)]();
    pHandle -> u.IOMAGE.uHiWM   = nSizeofIOMAGE; //10/01/2017 sizeof(VBHeapIOMAGE);  //was 0 @ 12/3/2011
    pHandle -> u.IOMAGE.aIOmage = (VBLaddr)pHandle -> u.IOMAGE.pIOMAGE;
    pHandle -> u.IOMAGE.pRoot   =      &pHandle -> u.IOMAGE.pIOMAGE -> oRoot;
    P2PmsgHeap_InitIOMAGE ( pHandle->u.IOMAGE.pIOMAGE, nSizeofIOMAGE, uAddrType );
    //pHandle -> aVBListBuffer    = pHandle -> u.IOMAGE.aIOmage;
    pHandle -> pVBListBuffer    = (char *)pHandle -> u.IOMAGE.pIOMAGE;

    // Tidy up, and
    ASSERT(P2PmsgHeap_AssertValidIOMAGE(pHandle));
    ASSERT(P2PmsgHeap_AssertVBlocksIOMAGE(pHandle));
    return pHandle;
}

P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE ( VBListIOmage *pIOmage, VBLsize nBufferLen )
{
    // Length-validated entry point for untrusted images (e.g. the file loader),
    // where the real allocation size is known. The complement checksum enforced
    // by the single-arg overload is trivially forgeable, so a crafted nSizeof
    // declared LARGER than the actual buffer would otherwise drive an
    // out-of-bounds block walk. Reject that here before delegating.
    if ( nBufferLen < sizeof(pIOmage->oSync) )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE buffer (%u) smaller than header", (UINT)nBufferLen )
           ->Throw();
    // Classify before trusting the size field, so a foreign-endian image is
    // reported as such instead of as a bogus declared size (byte_order.md §4).
    if ( P2PmsgHeap_IOMAGEform(pIOmage) == VBLockSync_Swapped )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE byte-order mismatch - image written by a peer of "
                       "the opposite endianness" )
           ->Throw();
    VBLsize nDeclared = pIOmage->oSync.uiSync1 & 0x00FFFFFF;
    if ( nDeclared > nBufferLen )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE declared size (%u) exceeds buffer (%u)"
                     , (UINT)nDeclared, (UINT)nBufferLen )
           ->Throw();

    //  THE GATE OPENS HERE, before the delegate rather than around the two
    //  walks below, and the position is the point. Everything from this line to
    //  the return is a judgement on bytes a stranger sent: a violated invariant
    //  is the stranger being wrong, not this library being wrong. Inside the
    //  scope the walks REFUSE instead of asserting, they stop at the first bad
    //  block instead of enumerating all of them, and -- the half that is a
    //  security fix rather than a noise fix -- they do not WRITE the image's
    //  root back into agreement with itself on the way past.
    //
    //  It covers the delegate because the delegate ends in two ASSERT-wrapped
    //  walks of its own, which in a debug build would walk this same image
    //  first, shout about it, and then be walked again for real. See the note
    //  where those two lines now stand.
    P2PmsgHeap_UntrustedGate oGate;

    P2PmsgHANDLE hVBList = P2PmsgHeap_CreateIOMAGE ( pIOmage );

    //  Walk the image FOR REAL, in every build (item 19) -- the same change the
    //  BSTRio overload got in Stage C, and for the same reason. The delegate
    //  above ends with
    //      ASSERT(P2PmsgHeap_AssertValidIOMAGE(pHandle));
    //      ASSERT(P2PmsgHeap_AssertVBlocksIOMAGE(pHandle));
    //  so the whole of both validations sits inside the assertion: Release does
    //  not run a reduced version, it does not call them at all. On the untrusted
    //  path that is the C4 mistake exactly, and it survived Stage C only because
    //  Stage C fixed the branch the fuzzer happened to reach. The load dispatch
    //  has two arms and this is the other one.
    //
    //  Only this overload changes. The single-argument one keeps its ASSERTs: it
    //  is for images this process just built, where the walk is a developer aid
    //  rather than a gate, and making every ordinary heap creation pay for a full
    //  block walk would be a real cost for no safety. That is the same split the
    //  BSTRio pair uses.
    //  AND THE ROOT'S OWN aAlloc, which neither walk above ever looks at.
    //  AssertValidIOMAGE walks the FREE chain from the root's aFree, and
    //  AssertVBlocksIOMAGE walks the block area by STEP from the first block;
    //  between them they never ask whether aAlloc - the third link in the root
    //  and the only one a reader follows first - points at an allocated block
    //  at all. P2PmsgHeap_Connect returns exactly that address, so it is what
    //  every P3PmsgObject built from this image connects to.
    //
    //  The BSTRio twin below has checked its three root offsets since F1; this
    //  arm never got the equivalent, which is the same "harden the branch that
    //  was reached rather than the pattern" this file records at F2 and F3. The
    //  check here is the stronger of the two forms available - not "points
    //  inside the image" but "is a valid allocated block", using the same
    //  predicate the readers themselves assert on, so an image that gets past
    //  this cannot make one of them fire.
    //
    //  Zero is legal and means an empty heap.
    const VBLaddr aRootAlloc =
        VBHeapRoot_GetAlloc ( static_cast<VBListHANDLE *>(hVBList)->u.IOMAGE.pRoot );
    if ( !P2PmsgHeap_AssertValidIOMAGE ( hVBList )   ||
         !P2PmsgHeap_AssertVBlocksIOMAGE ( hVBList ) ||
         ( aRootAlloc && !P2PmsgHeap_AssertValidAlloc ( hVBList, aRootAlloc ) ) )
    {
      //  Free the handle but NOT the image, exactly as the BSTRio overload does.
      //  P2PmsgHeap_Close deletes u.IOMAGE.pIOmage (or pIOMAGE), while
      //  P2PmsgMgr::Load's catch deletes the buffer it allocated -- ownership
      //  only transfers once this function returns, so closing a handle that
      //  still points at the image would double-free. Detach first.
      VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
      pHandle->u.IOMAGE.pIOmage = nullptr;
      pHandle->u.IOMAGE.pIOMAGE = nullptr;
      pHandle->pVBListBuffer    = nullptr;
      P2PmsgHeap_Close ( hVBList );
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE block structure is corrupt" )
           ->Throw();
    }
    return hVBList;
}

P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE ( VBListIOmage *pIOmage )
{
    // Parsing
    // Validate at runtime (these were debug-only ASSERTs, compiled out in
    // release): reject a header whose complement checksum is wrong, and require
    // the declared size to exceed the oSync header. Without the lower bound,
    // nSizeof < sizeof(oSync) underflows nSizeofUsed below to a huge value,
    // handing every later block walk an out-of-bounds region.
    // NOTE: the complement checksum is trivially forgeable, so this does NOT
    // stop a crafted oversized nSizeof (declared larger than the real buffer)
    // from driving an out-of-bounds walk; closing that needs the actual buffer
    // length threaded from the callers of this overload and is tracked as the
    // remaining part of the fix.
    // Endian sentinel BEFORE anything is read out of the header: a foreign-endian
    // image passes the complement check (which is endian-blind by construction)
    // and would then hand every check below a byte-swapped size. Report it as the
    // byte-order mismatch it is rather than as corruption (byte_order.md §4).
    if ( P2PmsgHeap_IOMAGEform(pIOmage) == VBLockSync_Swapped )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE byte-order mismatch - image written by a peer of "
                       "the opposite endianness" )
           ->Throw();
    // Likewise before the size is read, and for the same reason one step on:
    // bits 0-23 are the size in generation 1, and nothing promises that of a
    // layout this build has never seen. Refused with its code
    // (TargetCore's versioning note, §6).
    if ( P2PmsgHeap_IOMAGEform(pIOmage) == VBLockSync_Gen )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE layout generation 0x%02X is not implemented by "
                       "this build (0x%02X)"
                     , (UINT)VBLock_SyncGenCode ( pIOmage->oSync.uiSync1 )
                     , (UINT)VBLock_SyncGenNow )
           ->Throw();
    UINT08  uAddrType = VBLock_SyncAddr ( pIOmage->oSync.uiSync1 );
    VBLsize nSizeof   =          pIOmage->oSync.uiSync1&0x00FFFFFF;
    if ( !P2PmsgHeap_IsIOMAGE(pIOmage) || nSizeof <= sizeof(pIOmage->oSync) )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "Invalid IOMAGE (declared size %u)", (UINT)nSizeof )
           ->Throw();
    // Both figures below come out of the same untrusted header and were never
    // compared (M6): uiSync1 carries the arena size in 24 bits and the addressing
    // width in two more, so an image can declare Addr16 over an arena of up to
    // 16 MB. Every offset in such an image may exceed what its own links can
    // hold, which is the corrupt-free-list precondition M5 describes -- reached
    // here without any allocation churn at all, just by saying so in a file.
    P2PmsgHeap_CheckArenaWidth ( uAddrType, nSizeof, __FUNCTION__ );
    // The root control block sits at a fixed offset in the image, and every walk
    // below reads it -- the free-list head, its counters, and the two fixups at
    // the end of AssertValidIOMAGE that WRITE to it. Nothing so far required it
    // to be present: the bound above rejects only a declared size at or below
    // the 8-byte sync word, so an image declaring thirteen bytes was accepted
    // and its root read, then written, past the end of a seventeen-byte buffer.
    // `p2p_fuzzframe 0x5EEDF00D --replay 6 2` is that image. The write is the
    // one the CRT debug heap reports much later and attributes to whichever
    // block the fixup happened to land in, which is why it read as damage with
    // no writer; ASan names it at the instruction.
    const VBLsize nSizeofRoot = sizeof(pIOmage->oSync)
                              + VBHeapRoot_Sizeof_Addrnn ( uAddrType );
    if ( nSizeof < nSizeofRoot )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE declared size %u cannot hold its %u byte root"
                     , (UINT)nSizeof, (UINT)nSizeofRoot )
           ->Throw();
    // The root carries its OWN addressing byte, and every VBHeapRoot_ accessor
    // dispatches on that byte rather than on the one in the sync word. So an
    // image is free to declare Addr08 at the front door -- which is what sized
    // the bound just above -- and Addr64 in the root, and the first accessor to
    // run then reads a 64-bit field from where the 8-bit one ended.
    // `p2p_fuzzframe 0x5EEDF00D --replay 8 110` is that image: it reads two
    // bytes past a fifty-byte one. Both bytes are inside the image and neither
    // can be believed alone, so what is checked is that they AGREE -- which is
    // what makes the size bound above mean anything at all.
    const VBHeapRoot *pRootSync = &( (const VBHeapIOMAGE *)pIOmage ) -> oRoot;
    if ( (pRootSync->uVBLock & VBLock_AddrMask) != (uAddrType & VBLock_AddrMask) )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "IOMAGE root addressing 0x%02x contradicts its header 0x%02x"
                     , (UINT)(pRootSync->uVBLock & VBLock_AddrMask)
                     , (UINT)(uAddrType & VBLock_AddrMask) )
           ->Throw();

    // Initialisation - P2PmsgHANDLE
    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType    = P2PmsgHeap_IOMAGE;
    pHandle -> uAddrType      = uAddrType;
    pHandle -> nSizeofUsed    = nSizeof - sizeof(pIOmage->oSync);
    pHandle -> nSizeofAlloc   = nSizeof;
    pHandle -> nSizeofMax     = VBList_VBHeapMax ( uAddrType );
    pHandle -> nVBLockMin     = VBList_VBHeapMin ( uAddrType );
    pHandle -> nRefCount      = 1;
    pHandle -> bEoD           = true;
    pHandle -> bDirty         = false;

    // Initialisation - IOMAGE
    pHandle -> u.IOMAGE.pIOmage = pIOmage;
    pHandle -> u.IOMAGE.pIOMAGE = (VBHeapIOMAGE *)pIOmage;
    pHandle -> u.IOMAGE.pRoot   = &pHandle->u.IOMAGE.pIOMAGE->oRoot;
    pHandle -> u.IOMAGE.uHiWM   = nSizeof;
    pHandle -> u.IOMAGE.aIOmage = (VBLaddr)pHandle -> u.IOMAGE.pIOmage;
    pHandle -> u.IOMAGE.pRoot   = (VBHeapRoot *)&pHandle->u.IOMAGE.pIOMAGE->oRoot;

    // Tidy up, and
    //  NOT inside a gate. When the length-validated overload above delegates
    //  here it is about to run both of these for real and act on the answer, so
    //  running them here as well would walk the image twice in a debug build
    //  and raise an assertion about data the gate is a moment away from
    //  refusing quietly. Outside a gate this is the developer aid it always
    //  was, over an image this process just built.
    if ( !P2PmsgHeap_InUntrustedGate() )
    {
      ASSERT(P2PmsgHeap_AssertValidIOMAGE(pHandle));
      ASSERT(P2PmsgHeap_AssertVBlocksIOMAGE(pHandle));
    }
    return pHandle;
}

    // Initialisation
/*    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType    = P2PmsgHeap_BSTRio;
    pHandle -> uAddrType      = P2PmsgHeap_AddnnBSTRio ( pBSTRio );
    pHandle -> pVBListBuffer  = (char *)pBSTRio;
    pHandle -> aVBListBuffer  = (VBLaddr)pBSTRio;
    pHandle -> nSizeofUsed    = pBSTRio->oKeys.aAllocSize;
    //pHandle -> nSizeofInit    = 0;
    pHandle -> nSizeofAlloc   = pBSTRio->oSize.aSize1;
    pHandle -> nSizeofMax     = VBList_VBHeapMax ( pHandle->uAddrType );
    pHandle -> nVBLockMin     = VBList_VBHeapMin ( pHandle->uAddrType );
    pHandle -> nRefCount      = 1;
    pHandle -> bDirty         = false;

    // Initialisation - BSTRio
    pHandle -> u.BSTRio.pCMapTriggers = 0;
    pHandle -> u.BSTRio.pBSTRio = pBSTRio;
    pHandle -> u.BSTRio.uHiWM   = pBSTRio->oKeys.aAllocSize;
    pHandle -> u.BSTRio.aBSTRio = (UINT)pHandle -> u.BSTRio.pBSTRio;
    //VBLockRoot_Init ( &pHandle->u.BSTRio.oRoot, uAddrType
    //                , sizeof(pHandle->u.BSTRio.oRoot) );

    // TODO: Delete below sequence vvvv
    // NOTES: Swapover gludge
    if ( pBSTRio->oKeys.aAlloc == 0 )
      pBSTRio->oKeys.aAlloc = (VBLaddr)&pBSTRio->cTag - (VBLaddr)pBSTRio;
    if ( pBSTRio->oKeys.aAlloc == pBSTRio->oKeys.aFree )
      pBSTRio->oKeys.aAlloc = 0;
    if ( pBSTRio->oKeys.aFreeSize>100000000)
    {
      ASSERT(0);//TODO:LJM delete this block
      pBSTRio->oKeys.aFreeSize = pHandle->nSizeofAlloc-pBSTRio->oKeys.aAllocSize-sizeof(VBListBSTRio)+1;
    }
    // TODO: Delete above sequence ^^^

    // Tidy up, and
    ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle));
    return pHandle;*/

P2PmsgHANDLE
P2PmsgHeap_CreateSYS ( UCHAR uAddrType, VBLsize nSizeMax )
{
#if P2P_PTR64
ASSERT(uAddrType==VBLock_Addr64);
#else
 ASSERT(uAddrType==VBLock_Addr32);
#endif
    // To be sure, to be sure
    VBLsize nVBLockMax = VBList_VBHeapMax(uAddrType);
    if ( nSizeMax == 0 )
      nSizeMax = nVBLockMax;
    if ( nSizeMax > nVBLockMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeMax)
           ->Message("nSizeMax exceeds allowable address limit(%x)"
                    , nVBLockMax )
           ->Throw();

    // Initialisation
    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType  = P2PmsgHeap_SYSTEM;
    pHandle -> uAddrType    = uAddrType;
    pHandle -> nSizeofMax   = nSizeMax;
    pHandle -> nVBLockMin   = VBList_VBHeapMin ( uAddrType );
    pHandle -> nRefCount    = 1;
    pHandle -> bEoD         = true;
    pHandle -> bDirty       = true;

    // Tidy up, and
    return pHandle;
}

P2PmsgHANDLE
P2PmsgHeap_CreateBSTRio ( UCHAR uAddrType, VBLsize nSizeInitial, VBLsize nSizeMax )
{
    // To be sure, to be sure
    P2PmsgHeap_CheckCreateWidth ( uAddrType );      // width before sizes
    VBLsize nVBLockMax = VBList_VBHeapMax(uAddrType);
    if ( nSizeMax == 0 )
      nSizeMax = nVBLockMax;
    if ( nSizeInitial > nSizeMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeInitial)->AFP(nSizeMax)
           ->Message("nSizeInitial exceeds nSizeMax")
           ->Throw();
    if ( nSizeMax > nVBLockMax )
      EVERR->MODULE
           ->AFP(uAddrType)->AFP(nSizeInitial)->AFP(nSizeMax)
           ->Message("nSizeMax exceeds allowable address limit(%x)"
                    , nVBLockMax )
           ->Throw();

    // Initialisation
ASSERT(nSizeInitial);nSizeInitial+=8;//Delete testing only
    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType      = P2PmsgHeap_BSTRio;
    pHandle -> uAddrType        = uAddrType & VBLock_AddrMask;
    //pHandle -> pVBListBuffer  = 0;
    //pHandle -> aVBListBuffer    = 0; //(UINT)pHandle -> pVBListBuffer;
    //pHandle -> nSizeofUsed    = 0;
    pHandle -> nSizeofInit      = nSizeInitial;
    pHandle -> nSizeofAlloc     = nSizeInitial;
    pHandle -> nSizeofMax       = nSizeMax;
    pHandle -> nVBLockMin       = VBList_VBHeapMin ( uAddrType );
    pHandle -> nRefCount        = 1;
    pHandle -> bEoD             = true;
    pHandle -> bDirty           = true;

    // Initialisation - BSTRio
    // NOTES: Size rounded up to multiple of sizeof(VBLsize) bytes
    VBLsize nSizeofBSTRio       =  nSizeInitial
                                + (sizeof(VBLsize) - (nSizeInitial % sizeof(VBLsize))) % sizeof(VBLsize);
    // Value-initialise (zero) the whole BSTRio arena. A BSTRio heap serialises its
    // ENTIRE allocation (P2PmsgHeap_Sizeof returns nSizeofAlloc), so every unused byte
    // past the live entries lands in the saved image. Zeroing here makes that slack
    // deterministically 0 on every platform/config instead of the allocator's fill
    // (0x00 on Linux, MSVC debug-CRT 0xCD on Windows) — the fix that makes the golden
    // .p2p byte-identical cross-OS. Live entries overwrite the
    // used region during the build.
    pHandle -> u.BSTRio.pBSTRio = (VBListBSTRio *)new char [nSizeofBSTRio+sizeof(VBLsize)]();
    pHandle -> u.BSTRio.uHiWM   = sizeof(VBListBSTRio);  //was 0 @ 12/3/2011
    pHandle -> u.BSTRio.aBSTRio = (VBLaddr)pHandle -> u.BSTRio.pBSTRio;
    P2PmsgHeap_InitBSTRio ( pHandle->u.BSTRio.pBSTRio, nSizeofBSTRio, uAddrType );
    pHandle -> u.BSTRio.pCMapTriggers = 0;
    //pHandle -> aVBListBuffer    = pHandle -> u.BSTRio.aBSTRio;
    pHandle -> pVBListBuffer    = (char *)pHandle -> u.BSTRio.pBSTRio;
    //VBListIOmage oIOmage;
    //VBLockRoot_SetFirst ( &pHandle->u.BSTRio.oRoot, sizeof(oIOmage.oSync) );
    //VBLockRoot_SetLast  ( &pHandle->u.BSTRio.oRoot, sizeof(oIOmage.oSync) );
    //VBLockRoot_SetItems ( &pHandle->u.BSTRio.oRoot, 1 );

    //VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( pHandle, sizeof(oIOmage.oSync) );
    //VBLock_Init ( pVBLock, pHandle->uAddrType, nSizeInitial );
    //pVBLock -> oHdr.uVBLock |= VBLock_Linked;
//ASSERT(VBLock_IsAlloc(pVBLock));
//ASSERT(VBLock_IsLinked(pVBLock));//TODO: delete
//ASSERT(VBLock_IsFree(pVBLock));//TODO: delete
//ASSERT(VBLock_IsAddr(pVBLock,pHandle->uAddrType));

    // Tidy up, and
    //  Not inside a gate -- see the IOMAGE twin for why.
    if ( !P2PmsgHeap_InUntrustedGate() )
      ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle));
    return pHandle;
}

P2PmsgHANDLE
P2PmsgHeap_CreateBSTRio ( VBListBSTRio *pBSTRio, VBLsize nBufferLen )
{
    // Length-validated entry point for untrusted images -- the twin of
    // P2PmsgHeap_CreateIOMAGE(pIOmage,nBufferLen), added because the loader's
    // BSTRio branch never had one. That asymmetry is finding F1: P2PmsgMgr::Load
    // reads a file into one buffer and then dispatches on IsBSTRio / IsIOMAGE,
    // and only the IOMAGE arm received the C4 fix. The BSTRio arm carried an
    // offset straight out of the file through to a dereference.
    //
    // Why the declared size is the load-bearing check, and not just one more
    // sanity test: the single-arg overload copies oSize.aSize1 into
    // VBListHANDLE::nSizeofAlloc, and nSizeofAlloc is the ceiling every later
    // P2PmsgHeap_Addr2Phys translation is tested against. So a forged aSize1
    // does not slip past the bounds check -- it BECOMES the bounds check, and
    // raises it to whatever the attacker chose. Validating it here is what makes
    // every one of those downstream tests mean something.
    if ( nBufferLen < sizeof(VBListBSTRio) )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "BSTRio buffer (%u) smaller than header", (UINT)nBufferLen )
           ->Throw();
    // Classify before trusting any size out of the header, for the same reason
    // the IOMAGE path classifies before reading its declared size.
    if ( !P2PmsgHeap_IsBSTRio(pBSTRio) )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "Not BSTRio heap type" )
           ->Throw();
    const VBLsize nDeclared = pBSTRio->oSize.aSize1;
    if ( nDeclared > nBufferLen )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "BSTRio declared size (%u) exceeds buffer (%u)"
                     , (UINT)nDeclared, (UINT)nBufferLen )
           ->Throw();
    if ( nDeclared < sizeof(VBListBSTRio) )
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "BSTRio declared size (%u) is smaller than its own header"
                     , (UINT)nDeclared )
           ->Throw();
    // The root offsets are the first thing the caller will follow, and they are
    // read out of the same untrusted bytes, so each must at least point inside
    // the declared image. 0 is legal throughout and means "none" -- the swapover
    // fixup in the single-arg overload below both relies on that and produces it.
    //
    // The floor is offsetof(cTag), NOT sizeof(VBListBSTRio). Those differ by one
    // and the difference is load-bearing: cTag is the first byte of the block
    // area, so a store's first block legitimately begins AT offset 48 in a
    // 49-byte header. An earlier draft of this check used sizeof() and rejected
    // every genuine store ever saved -- caught within a minute by scenario [1]
    // of C4LoadTest, which is the argument for having a round-trip case sitting
    // next to the adversarial ones.
    //
    // Only the lower bound and "inside the image" are enforced here. Whether the
    // block HEADER fits is checked where the header is actually read, by
    // P2PmsgHeap_Addr2PhysChk -- one check at the point of use beats two checks
    // that can disagree, and the read site is the only place that knows how many
    // bytes it is about to take.
    const VBLaddr aBlocks = offsetof ( VBListBSTRio, cTag );
    struct { const char *pszName; VBLaddr aOffset; }
    aoRoots[] = { { "aAlloc",    pBSTRio->oKeys.aAlloc    }
                , { "aFree",     pBSTRio->oKeys.aFree     }
                , { "aFreeLast", pBSTRio->oKeys.aFreeLast } };
    for ( int i = 0; i < ARRAYSIZE(aoRoots); i++ )
    {
      const VBLaddr aOffset = aoRoots[i].aOffset;
      if ( aOffset == 0 )
        continue;
      if ( aOffset < aBlocks || aOffset >= nDeclared )
        EVERR->Module ( __FUNCTION__ )
             ->Message ( "BSTRio root %s (0x%x) does not point inside the "
                         "declared image (%u)"
                       , aoRoots[i].pszName, (UINT)aOffset, (UINT)nDeclared )
             ->Throw();
    }

    //  The gate, for the same reasons the IOMAGE twin gives at the same point.
    P2PmsgHeap_UntrustedGate oGate;

    P2PmsgHANDLE hVBList = P2PmsgHeap_CreateBSTRio ( pBSTRio );

    // Walk the block chain FOR REAL, in every build (item 19). The delegate
    // above ends with ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle)) -- the
    // whole validation call sits inside the assertion, so Release does not run
    // a reduced version of it, it does not run it at all. On the untrusted path
    // that is the C4 mistake exactly: the structure is checked in the build
    // nobody ships.
    //
    // Only this overload is changed. The single-argument one keeps its ASSERT,
    // because it is for images this process just built, where the walk is a
    // developer aid and not a gate -- and because running it there would make
    // every ordinary heap creation pay for a full block walk.
    if ( !P2PmsgHeap_AssertVBlocksBSTRio ( hVBList ) )
    {
      // Free the handle but NOT the image. P2PmsgHeap_Close deletes
      // u.BSTRio.pBSTRio, and P2PmsgMgr::Load's catch deletes the buffer it
      // allocated -- ownership only transfers once this function returns, so
      // closing a handle still pointing at it would double-free. Detach first.
      VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
      pHandle->u.BSTRio.pBSTRio = nullptr;
      pHandle->pVBListBuffer    = nullptr;
      P2PmsgHeap_Close ( hVBList );
      EVERR->Module ( __FUNCTION__ )
           ->Message ( "BSTRio block structure is corrupt" )
           ->Throw();
    }
    return hVBList;
}

P2PmsgHANDLE
P2PmsgHeap_CreateBSTRio ( VBListBSTRio *pBSTRio )
{
    // To be sure, to be sure
    // NOTE: this overload cannot check the declared size against the real
    // buffer, because it is not given one. It is safe for an image this process
    // built; for anything read off a disk or a socket use the two-argument
    // overload above.
    if ( !P2PmsgHeap_IsBSTRio(pBSTRio) )
      EVERR->MODULE
           ->Message("Not BSTRio heap type")
           ->Throw();
    if ( !P2PmsgHeap_AssertValidBSTRio(pBSTRio) )
      EVERR->MODULE
           ->Message("Corrupted BSTRio heap")
           ->Throw();
    // The BSTRio half of the same check (M6). aSize1 and the addressing width
    // both come off the wire; nSizeofMax below is derived from the second and
    // nSizeofAlloc from the first, so without this they can disagree from the
    // first instruction the handle exists.
    P2PmsgHeap_CheckArenaWidth ( P2PmsgHeap_AddnnBSTRio(pBSTRio)
                               , pBSTRio->oSize.aSize1, __FUNCTION__ );

    // Initialisation
    VBListHANDLE *pHandle = new VBListHANDLE;
    ZeroMemory ( pHandle, sizeof(VBListHANDLE) );
    pHandle -> uVBListType    = P2PmsgHeap_BSTRio;
    pHandle -> uAddrType      = P2PmsgHeap_AddnnBSTRio ( pBSTRio );
    pHandle -> pVBListBuffer  = (char *)pBSTRio;
    //pHandle -> aVBListBuffer  = (VBLaddr)pBSTRio;
    pHandle -> nSizeofUsed    = pBSTRio->oKeys.aAllocSize;
    //pHandle -> nSizeofInit    = 0;
    pHandle -> nSizeofAlloc   = pBSTRio->oSize.aSize1;
    pHandle -> nSizeofMax     = VBList_VBHeapMax ( pHandle->uAddrType );
    pHandle -> nVBLockMin     = VBList_VBHeapMin ( pHandle->uAddrType );
    pHandle -> nRefCount      = 1;
    pHandle -> bDirty         = false;

    // Initialisation - BSTRio
    pHandle -> u.BSTRio.pCMapTriggers = 0;
    pHandle -> u.BSTRio.pBSTRio = pBSTRio;
    pHandle -> u.BSTRio.uHiWM   = pBSTRio->oKeys.aAllocSize;
    pHandle -> u.BSTRio.aBSTRio = (VBLaddr)pHandle -> u.BSTRio.pBSTRio;
    //VBLockRoot_Init ( &pHandle->u.BSTRio.oRoot, uAddrType
    //                , sizeof(pHandle->u.BSTRio.oRoot) );

    // TODO: Delete below sequence vvvv
    // NOTES: Swapover gludge
    if ( pBSTRio->oKeys.aAlloc == 0 )
      pBSTRio->oKeys.aAlloc = (VBLaddr)&pBSTRio->cTag - (VBLaddr)pBSTRio;
    if ( pBSTRio->oKeys.aAlloc == pBSTRio->oKeys.aFree )
      pBSTRio->oKeys.aAlloc = 0;
    if ( pBSTRio->oKeys.aFreeSize>200000000)
    {
      ASSERT(0);//TODO:LJM delete this block
      pBSTRio->oKeys.aFreeSize = pHandle->nSizeofAlloc-pBSTRio->oKeys.aAllocSize-sizeof(VBListBSTRio)+1;
    }
    // TODO: Delete above sequence ^^^

    // Tidy up, and
    ASSERT(P2PmsgHeap_AssertVBlocksBSTRio(pHandle));
    return pHandle;
}

VBListBSTRio*
P2PmsgHeap_pBSTRio( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType != P2PmsgHeap_BSTRio )
      EVERR->MODULE -> AFP((VBLaddr)hVBList)
           ->Message("Expected P2PmsgHeapBSTRio type" )
           ->Throw();
    return pHandle -> u.BSTRio.pBSTRio;
}

//
//  Increments reference count for P2PmsgHANDLE
//
P2PmsgHANDLE
P2PmsgHeap_AddRef ( P2PmsgHANDLE hVBHeap )
{
    // Preamble
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBHeap);
                  pHandle -> nRefCount++;
    return hVBHeap;
}

BOOL
P2PmsgHeap_Close ( P2PmsgHANDLE hVBList )
{
    // Preamble
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    // Decide the free off the RETURNED decrement, not a re-read: a racy re-read
    // lets two threads both see 0 and double-delete (mirrors the m_cRef fix).
    int nRef = --pHandle->nRefCount;
    if ( nRef > 0 )
      return nRef;

    // System Heap
    // NOTES: Recursively and randomly linked from system heap
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      P2PmsgHeapSYS_Close ( hVBList );

    // 
    // NOTES: Fully self contained heap
    else if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
    {
      if ( pHandle -> u.IOMAGE.pIOmage )
        delete [] pHandle -> u.IOMAGE.pIOmage;
      else if ( pHandle -> u.IOMAGE.pIOMAGE )
        delete [] pHandle -> u.IOMAGE.pIOMAGE;
    }

    // BSTRio Heap
    // NOTES: Fully self contained heap
    else if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
    {
      delete [] pHandle -> u.BSTRio.pBSTRio;
      if ( pHandle->u.BSTRio.pCMapTriggers )
        delete P2PmsgHeap_DropTriggers ( pHandle->u.BSTRio.pCMapTriggers );
    }

    // Chunk Heap
    // NOTES: Fully self contained heap
    else if ( pHandle->uVBListType == P2PmsgHeap_CHUNK )
      delete [] pHandle -> u.CHUNK.pIOmage;
    else { ASSERT(0); }

    // Collation tallies. Each refusal reports its first occurrence and counts
    // the rest (refer nCollateZeroSize / nCollateCorrupt at the handle); this is
    // where the rest get accounted for, so a suppressed report is never a lost
    // one. Emitted only when occurrences WERE suppressed -- the first of each is
    // already on the record, and a clean heap says nothing at all.
    //
    // Safe here because this runs once per heap: the refcount decrement above
    // returns before this point on every share but the last.
    if ( pHandle->nCollateCorrupt > 1 )
      EVERR->MODULE
           ->Message(L"Collate: internal address corruption %u times on this"
                     L" heap -- reported once, refused every time"
                    , (UINT)pHandle->nCollateCorrupt )
           ->Cancel();
    if ( pHandle->nCollateZeroSize > 1 )
      EVERR->MODULE
           ->Message(L"Collate: free neighbour declared zero size %u times"
                     L" on this heap -- reported once, refused every time"
                    , (UINT)pHandle->nCollateZeroSize )
           ->Cancel();

    // Tidy up, and
    delete pHandle;                    // Garbage collection
    return 0;
}

UCHAR
P2PmsgHeap_Addrnn ( P2PmsgHANDLE hVBHeap )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBHeap);
    return pHandle -> uAddrType;   
}

void*
P2PmsgHeap_pImage ( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return P2PmsgHeap_pIOmage ( hVBList );
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return pHandle -> u.BSTRio.pBSTRio;
    ASSERT(0);
    return 0;
}

VBLsize
P2PmsgHeap_Sizeof( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return pHandle -> nSizeofUsed;
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return pHandle -> nSizeofAlloc;
    ASSERT(0);
    return 0;
}

//
//  Sizing an arbitrary block by its address
//  NOTES: F8. Bound the HEADER before reading it, not just its first byte.
//         This is P2PmsgHeap_BlockFits' problem again -- the third time this
//         file has recorded it -- and it is fixed here the same way, for the
//         same reason its comment gives. Addr2Phys bounds the START only, and
//         with `>` rather than `>=`, so aVBLock == nSizeofAlloc translated to
//         one past the end of the image and VBLock_Hdr_u_SizeNN then read the
//         addressing byte from there. A 1-byte read exactly 0 bytes past a
//         2032-byte image, which is the report the receive-path fuzzer produced
//         on its fourth executed unit.
//       : The MsgVBHeap.h declaration of P2PmsgHeap_Addr2PhysChk describes this
//         crash in advance -- "an offset one byte below the limit passes
//         Addr2Phys and then reads a multi-byte block header off the end" -- so
//         the guard existed and this call site had simply never adopted it.
//         What was missing was not the knowledge; it was anything that walked
//         this entry point with a hostile address.
//       : The span depends on the BLOCK's addressing mode, not the heap's, and
//         that mode is one untrusted byte at aVBLock -- in bounds once the start
//         is, which is why the start check still comes first. Same order, and
//         the same reasoning, as P2PmsgHeap_BlockFits.
//       : Throwing, not returning 0. This function already threw for an address
//         past the limit, via Addr2Phys; an address AT the limit now takes the
//         same exit instead of returning a size read out of bounds. Every caller
//         is already inside the load path's try/catch or an equivalent.
//       : NOT a general fix. About twenty other unchecked Addr2Phys call sites
//         in this file have the same shape, and F3's lesson applies: a defect
//         fixed on one branch of a fork is not closed until the others have been
//         looked at. They have not been. See the release-readiness register, F8.
//
static VBLsize
VBList_VBLockHdrMin ( UCHAR uVBLaddr )
{
    uVBLaddr = uVBLaddr & VBLock_AddrMask;
    if ( uVBLaddr == VBLock_Addr32 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize32);
    if ( uVBLaddr == VBLock_Addr64 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize64);
    if ( uVBLaddr == VBLock_Addr16 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize16);
    if ( uVBLaddr == VBLock_Addr08 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize08);
    return 0u;                           // unknown mode: refuse rather than guess
}

VBLsize
P2PmsgHeap_Sizeof ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    const bool bImage = ( pHandle->uVBListType == P2PmsgHeap_IOMAGE ||
                          pHandle->uVBListType == P2PmsgHeap_BSTRio    );

    //  Start bound, strictly. An address equal to the arena size is one past the
    //  last byte and can never begin a block.
    if ( bImage && aVBLock >= pHandle->nSizeofAlloc )
      EVERR->MODULE
           ->Message(L"P2PmsgHeap block address[0x%x] is not inside an image of 0x%0x"
                    , aVBLock, pHandle->nSizeofAlloc )
           ->Throw();

    VBLock *pVBLock  = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLock );
    if ( pVBLock == nullptr )
      return 0;                          // aVBLock <= 0; Addr2Phys's own answer

    //  Span bound, from the block's own declared mode -- readable now the start
    //  is bounded, and untrusted, so an unknown mode is refused rather than
    //  guessed at.
    if ( bImage )
    {
      const VBLsize nNeed = VBList_VBLockHdrMin ( pVBLock->oHdr.uVBLockDefs );
      if ( nNeed == 0 || aVBLock + nNeed > pHandle->nSizeofAlloc )
        EVERR->MODULE
             ->Message(L"P2PmsgHeap block header at[0x%x] overruns image of 0x%0x"
                      , aVBLock, pHandle->nSizeofAlloc )
             ->Throw();
    }

    return VBLock_Hdr_u_SizeNN ( pVBLock );
}

//
//  Fetches IOMAGE pointer
//  NOTES: Completes internal addressing etc.  Subsequent heap activities
//         will invalidate such house keeping
//
//  Parameters:  P2PmsgHANDLE hP2PmsgHANDLE
//               Handle for which memory object is to be retrieved
//
//  Returns:     VBListIOmage
//               Contiguous image pointer
VBListIOmage*
P2PmsgHeap_pIOmage ( P2PmsgHANDLE hVBList )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType != P2PmsgHeap_IOMAGE )
      EVERR->MODULE -> AFP((VBLaddr)hVBList)
           ->Message("Expected P2PmsgHeap_IOmage type" )
           ->Group("P2P")->Throw();
    VBHeapIOMAGE *pIOmage = pHandle -> u.IOMAGE.pIOMAGE;
// Overflow guard. NOTE the two CreateIOMAGE paths count nSizeofUsed differently: fresh heaps count
// heap-area allocations only, while CreateIOMAGE(VBListIOmage*) (a message reconstructed from a
// wire image) sets nSizeofUsed = image size - sizeof(oSync) with uHiWM = image size. The previous
// form (nSizeofUsed+sizeof(VBHeapIOMAGE)-1 <= uHiWM) therefore failed for EVERY reconstructed
// message re-serialised for another hop (multi-hop P2PeerHub::RouteP2PeerMsg forwarding) or reply
// (ResponseFactory on a received message), where it reduced to sizeof(VBHeapIOMAGE) <= 9. This
// form holds with equality on that path and is implied by the previous form on fresh heaps.
ASSERT(pHandle->nSizeofUsed+sizeof(pIOmage->oSync)<=pHandle -> u.IOMAGE.uHiWM);
    // Synchronisation header
    // NOTES: VBLock_SyncMake stamps the endian sentinel into bits 2-7 of the
    //        top byte alongside the addressing mode (byte_order.md §4). The
    //        complement invariant below is unchanged - the sentinel is what
    //        makes the pair asymmetric under a byte swap, which the complement
    //        alone never was.
    pIOmage -> oSync.uiSync1  =  VBLock_SyncMake ( (UINT32)(pHandle -> u.IOMAGE.uHiWM)
                                                 , pHandle -> uAddrType );
    pIOmage -> oSync.uiSync2  = ~pIOmage -> oSync.uiSync1;

    // Control keys
    // NOTES: Used to re-establish heap parameters
    VBHeapIOMAGE *pIOMAGE = pHandle -> u.IOMAGE.pIOMAGE;
    if ( pIOMAGE == nullptr )
      return (VBListIOmage*)pIOmage;
    //pIOMAGE -> oKeys.aAlloc       = pHandle->u.IOMAGE.oCtrlKeys.aAlloc;
    //pIOMAGE -> oKeys.aAllocSize   = pHandle->u.IOMAGE.oCtrlKeys.aAllocSize;
    //pIOMAGE -> oKeys.aFree        = pHandle->u.IOMAGE.oCtrlKeys.aFree;
    //pIOMAGE -> oKeys.aFreeLast    = pHandle->u.IOMAGE.oCtrlKeys.aFreeLast;
    //pIOMAGE -> oKeys.aFreeSize    = pHandle->u.IOMAGE.oCtrlKeys.aFreeSize;
    //pIOMAGE -> oKeys.nAllocEntries= pHandle->u.IOMAGE.oCtrlKeys.nAllocEntries;
    //pIOMAGE -> oKeys.nFreeEntries = pHandle->u.IOMAGE.oCtrlKeys.nFreeEntries;
    //pIOMAGE -> oKeys.aSpare8      = pHandle->u.IOMAGE.oCtrlKeys.aSpare8;
ASSERT(P2PmsgHeap_AssertValidIOMAGE(pHandle));
ASSERT(P2PmsgHeap_AssertVBlocksIOMAGE(pHandle));

//P2PmsgHANDLE hVBListmp=P2PmsgHeap_CreateIOMAGE ( (VBListIOmage*)pIOmage );
//P2PmsgHeap_AssertValid(hVBListmp);
//    VBListHANDLE *pHTmp = (VBListHANDLE *)hVBListmp;pHTmp->u.IOMAGE.pIOMAGE=nullptr;pHTmp->u.IOMAGE.pIOmage=nullptr;
//P2PmsgHeap_Close(hVBListmp);
//P2PmsgHeap_AssertValid(hVBHeap);
    return (VBListIOmage*)pIOmage;
}

//
//  Frees an allocated entry
//  NOTES: Effective garbage collection requires real-time collation upon
//         release
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLock
//               Block address of free entry to be freed
//
//  Returns:     VBLsize
//               Size of free'd and collated entry
VBLaddr
P2PmsgHeap_FreeBSTRio ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    // To be sure, to be sure
    // NOTES: Essentually corrupted heap, not much can be done
    if ( !P2PmsgHeap_AssertValidAlloc(hVBList,aVBLock) )
      EVERR->MODULE->AFP(aVBLock)      // Fundamental logic issue
           ->Message("Attempt to free non-allocated entry")->Assert()->Throw();

    // Locals
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    VBHeap       *pVBHeap = VBList2PhysVBHeap ( hVBList, aVBLock );
    VBLsize       nSizeof = VBHeap_Sizenn ( pVBHeap );

    // Transfer to free entries list
    // NOTES: Allocated isolation is assumed to have been externally performed.
    //      : Place at head of Free Entries list.  Hence first item targeted
    //        for next allocation
    VBHeap_SetPrev ( pVBHeap, 0 );
    VBHeap_SetNext ( pVBHeap, 0 );
    VBLaddr aFree = pHandle->u.BSTRio.pBSTRio->oKeys.aFree;
    if ( aFree )
    {
      VBHeap_SetNext ( pVBHeap, aFree );
      VBHeap *pVBLockFree = VBList2PhysVBHeap ( hVBList, aFree );
      VBHeap_SetPrev ( pVBLockFree, aVBLock );
    }

    // House keeping
    pHandle->u.BSTRio.pBSTRio->oKeys.aFree       = aVBLock;
    if ( pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast == 0 )
      pHandle->u.BSTRio.pBSTRio->oKeys.aFreeLast = aVBLock;
    pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  += nSizeof;
    pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize -= nSizeof;
    pHandle->u.BSTRio.pBSTRio->oKeys.nFreeEntries++;
    pHandle->u.BSTRio.pBSTRio->oKeys.nAllocEntries--;
    pHandle->nFreeEntries++;
    pVBHeap->oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    nSizeof = P2PmsgHeap_CollateBSTRio ( hVBList, aVBLock );

    //  BACKWARDS -- refer P2PmsgHeap_FreeIOMAGE for the whole of it. Same
    //  defect, same repair, and deliberately the same shape: both arms walk
    //  and merge by the same rules, so a fix that lands on one of them and not
    //  the other is how they drift.
    VBLaddr aPrevFree = P2PmsgHeap_PrevFree ( hVBList, aVBLock );
    if ( aPrevFree )
      P2PmsgHeap_CollateBSTRio ( hVBList, aPrevFree );

    // Triggers
    CMapTriggers *pTriggers = pHandle->u.BSTRio.pCMapTriggers;
    if ( pTriggers )
      P2PmsgHeap_ProcTriggers ( hVBList, aVBLock, TRIGGER_DELETE );
    pHandle -> bDirty = true;          // Triggers dirty flag
    return 0;
}

VBLaddr
P2PmsgHeap_FreeIOMAGE ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    // To be sure, to be sure
    // NOTES: Essentually corrupted heap, not much can be done
    if ( !P2PmsgHeap_AssertValidAlloc(hVBList,aVBLock) )
      EVERR->MODULE->AFP(aVBLock)      // Fundamental logic issue
           ->Message("Attempt to free non-allocated entry")->Assert()->Throw();

    // Locals
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    VBHeap       *pVBHeap = VBList2PhysVBHeap ( hVBList, aVBLock );
    VBLsize       nSizeof = VBHeap_Sizenn ( pVBHeap );

    // Transfer to free entries list
    // NOTES: Allocated isolation is assumed to have been externally performed.
    //      : Place at head of Free Entries list.  Hence first item targeted
    //        for next allocation
    VBHeap_SetPrev ( pVBHeap, 0 );
    VBHeap_SetNext ( pVBHeap, 0 );
    VBLaddr aFree = VBHeapRoot_GetFree ( pHandle->u.IOMAGE.pRoot );
    if ( aFree )
    {
      VBHeap_SetNext ( pVBHeap, aFree );
      VBHeap *pVBLockFree = VBList2PhysVBHeap ( hVBList, aFree );
      VBHeap_SetPrev ( pVBLockFree, aVBLock );
    }

    // House keeping
    VBHeapRoot_SetFree ( pHandle->u.IOMAGE.pRoot, aVBLock );
    if ( VBHeapRoot_GetFreeLast(pHandle->u.IOMAGE.pRoot) == 0 )
      VBHeapRoot_SetFreeLast ( pHandle->u.IOMAGE.pRoot,  aVBLock );
    VBHeapRoot_SetFreeSize   ( pHandle->u.IOMAGE.pRoot,  +nSizeof );
    VBHeapRoot_SetAllocSize  ( pHandle->u.IOMAGE.pRoot,  -(int)nSizeof );
    VBHeapRoot_SetFreeItems  ( pHandle->u.IOMAGE.pRoot,  +1 );
    VBHeapRoot_SetAllocItems ( pHandle->u.IOMAGE.pRoot,  -1 );
    pHandle->nFreeEntries++;
    pVBHeap->oHdr.uVBLockDefs &= ~VBLock_TypeMask;
    nSizeof = P2PmsgHeap_CollateIOMAGE ( hVBList, aVBLock );

    //  BACKWARDS, which is the half this heap never had. Collate only ever
    //  looks forward, so the line above merges anything free that sits AFTER
    //  this block and stops. If the block before it is also free, the two are
    //  adjacent and should be one -- and now that a free block carries its size
    //  in its tail, the predecessor can be found. Collating FROM it absorbs
    //  this block by the same forward path, so there is no second merge routine
    //  to keep in step with the first.
    VBLaddr aPrevFree = P2PmsgHeap_PrevFree ( hVBList, aVBLock );
    if ( aPrevFree )
      P2PmsgHeap_CollateIOMAGE ( hVBList, aPrevFree );

    // Tidy up, and
    pHandle -> bDirty = true;          // Triggers dirty flag
    return 0;
}

VBLaddr
P2PmsgHeap_FreeSYS ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    //VBLock       *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBHeap, aVBLock );

    // Remove from map
    // NOTES: Observe cached optimisation
    if ( pHandle->u.SYS.uVBLock0 == aVBLock )
      pHandle->u.SYS.uVBLock0 = 0;
    else if ( pHandle->u.SYS.uVBLock1 == aVBLock )
      pHandle->u.SYS.uVBLock1 = 0;
    else if ( pHandle->u.SYS.uVBLock2 == aVBLock )
      pHandle->u.SYS.uVBLock2 = 0;
    else if ( pHandle->u.SYS.pCMapAlloc )
      pHandle->u.SYS.pCMapAlloc->RemoveKey( aVBLock );
    P2PASSERT(IsBadWritePtr((void*)aVBLock,4)==0);
    delete [] (char *)aVBLock;

    // Tidy up, and
    pHandle -> bDirty = true;
    return 0;
}
VBLaddr
P2PmsgHeap_Free ( P2PmsgHANDLE hVBList, VBLaddr aVBLock )
{
    // Preamble
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE  *pHandle  = (VBListHANDLE *)hVBHeap;
    VBLock        *pVBLock  = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aVBLock );
    ASSERT(VBLock_IsAlloc(pVBLock));
    ASSERT(VBLock_IsLinked(pVBLock));
ASSERT(pHandle->nSizeofUsed>=VBLock_Hdr_u_SizeNN(pVBLock));
    pHandle -> nSizeofUsed -= VBLock_Hdr_u_SizeNN ( pVBLock );
    //if ( pHandle->aVBListBuffer == aVBLock )
    // According to VBHeap type
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      P2PmsgHeap_FreeIOMAGE ( hVBList, aVBLock );
    else if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      P2PmsgHeap_FreeSYS ( hVBList, aVBLock );
    else if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      P2PmsgHeap_FreeBSTRio ( hVBList, aVBLock );
    else { ASSERT(0); }

    // Tidy up, and
    return 0;
}

//
//  Translates a VBHeap address to a physical memory address
//  NOTES: 
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLaddr
//               Address to be translated
//
//  Returns:     void*
//               Translated address
void*
P2PmsgHeap_Addr2Phys ( P2PmsgHANDLE hVBList, VBLaddr aVBLaddr )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
    {
      if ( aVBLaddr <= 0 )
        return nullptr;
      if ( aVBLaddr > pHandle->nSizeofAlloc )
        EVERR->MODULE
             ->Message(L"P2PmsgHeap_IOMAGE address[0x%x] out of range 1 to 0x%0x", aVBLaddr, pHandle->nSizeofAlloc )
             ->Throw();
      void  *vpVBaddr = (void *)(pHandle -> u.IOMAGE.aIOmage + aVBLaddr);
      return vpVBaddr;
    }
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
    {
      if ( aVBLaddr <= 0 )
        return nullptr;
      if ( aVBLaddr > pHandle->nSizeofAlloc )
        EVERR->MODULE
             ->Message(L"P2PmsgHeap_BSTRio address[0x%x] out of range 1 to 0x%0x", aVBLaddr, pHandle->nSizeofAlloc )
             ->Throw();
      void  *vpVBaddr = (void *)(pHandle -> u.BSTRio.aBSTRio + aVBLaddr);
      return vpVBaddr;
    }
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
    {
      //if ( aVBLaddr <= 0x10000 )
      //  EVERR->MODULE
      //       ->Message(L"P2PmsgHeap_BSTRio address[0x%x] out of range", aVBLaddr )
      //       ->Throw();
      void  *vpVBaddr = (void *)aVBLaddr;
      P2PASSERT(vpVBaddr==nullptr||IsBadWritePtr(vpVBaddr,4)==0);
      return vpVBaddr;
    }
    P2PASSERT(0);
    EVERR->MODULE
         ->Message(L"Invalid VBList type 0x%04x", pHandle->uVBListType )
         ->Throw();
    return 0;
}

//
//  Translates a VBHeap address, requiring a whole span to be in bounds
//  NOTES: Addr2Phys bounds the START of a block. That is the right check for an
//         offset this process allocated, where the block's own size is known
//         good, and the wrong one for an offset lifted out of a file: an offset
//         one byte below the limit passes, and the caller then reads a nine-byte
//         VBLockHdr off the end of the image. ASan sees that; a release build
//         does not.
//       : nSpan is what the CALLER is about to read, not what the block claims
//         to be -- the block's claim is exactly the thing that cannot be trusted
//         until its header has been read, which is the chicken-and-egg this
//         function exists to break.
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLaddr
//               Address to be translated
//
//               VBLsize nSpan
//               Bytes the caller will read from the returned pointer
//
//  Returns:     void*
//               Translated address, or nullptr for aVBLaddr 0 (as Addr2Phys)
void*
P2PmsgHeap_Addr2PhysChk ( P2PmsgHANDLE hVBList, VBLaddr aVBLaddr, VBLsize nSpan )
{
    void *vpVBaddr = P2PmsgHeap_Addr2Phys ( hVBList, aVBLaddr );
    if ( vpVBaddr == nullptr )
      return nullptr;

    // Addr2Phys has already rejected aVBLaddr past the limit and thrown for the
    // types that carry one, so only the tail of the span is still unchecked.
    // SYSTEM heaps address raw memory and carry no limit to test against; they
    // are never built from wire data, so there is nothing here to bound.
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType != P2PmsgHeap_IOMAGE &&
         pHandle->uVBListType != P2PmsgHeap_BSTRio    )
      return vpVBaddr;

    if ( aVBLaddr + nSpan > pHandle->nSizeofAlloc )
      EVERR->MODULE
           ->Message(L"P2PmsgHeap address[0x%x]+%u overruns image of 0x%0x"
                    , aVBLaddr, (UINT)nSpan, pHandle->nSizeofAlloc )
           ->Throw();
    return vpVBaddr;
}

//
//  Tests a PHYSICAL pointer, and the span about to be read from it, against
//  the image this heap was built over
//  NOTES: Addr2PhysChk bounds an OFFSET before it becomes a pointer, which is
//         the right shape when the offset itself came off the wire. It is the
//         wrong shape for VBLock_pData and its siblings: those hand back a
//         pointer computed from fields INSIDE a block, so there is no offset to
//         check at the door -- only a pointer that may already have left the
//         image. This is the predicate for that case.
//       : Answers rather than throws. The caller knows whether the pointer it
//         is testing is supposed to be in the image at all -- an object holding
//         its block in P3PmsgObject::m_oVBLock legitimately points outside one.
//       : SYSTEM heaps address raw process memory and are never built from wire
//         data, so they carry no image to bound and every span is in.
//
//  Parameters:  P2PmsgHANDLE hVBList
//
//               const void *pv
//               Pointer to test
//
//               VBLsize nSpan
//               Bytes the caller will read from pv
//
//  Returns:     BOOL
//                 TRUE... pv[0..nSpan) lies inside the image
//                 FALSE.. it does not, or hVBList is null
BOOL
P2PmsgHeap_IsPhysSpan ( P2PmsgHANDLE hVBList, const void *pv, VBLsize nSpan ) noexcept
{
    if ( hVBList == nullptr || pv == nullptr )
      return FALSE;
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBLaddr aImage = 0;
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      aImage = pHandle->u.IOMAGE.aIOmage;
    else if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      aImage = pHandle->u.BSTRio.aBSTRio;
    else
      return TRUE;
    if ( aImage == 0 )
      return FALSE;

    // Addresses run 1..nSizeofAlloc, so aImage itself is not a legal target --
    // the same convention Addr2Phys enforces, kept here so the two agree.
    const VBLaddr aLump = reinterpret_cast<VBLaddr>(pv);
    if ( aLump <= aImage )
      return FALSE;
    const VBLaddr dOffset = aLump - aImage;
    if ( dOffset > pHandle->nSizeofAlloc )
      return FALSE;
    if ( nSpan > pHandle->nSizeofAlloc - dOffset )
      return FALSE;
    return TRUE;
}

//
//  Allocates a VBLock from the VBHeap
//  NOTES: Effective garbage collection requires real-time collation upon
//         release
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//
//               VBLaddr aVBLock
//               Block address of free entry to be freed
//
//  Returns:     VBLsize
//               Size of free'd and collated entry
VBLaddr
P2PmsgHeap_AllocIOMAGE1 ( P2PmsgHANDLE hVBList, VBLsize nSizeof )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBHeap             *pVBLock = 0;

    // Iterate through free entries
    // NOTES: Normal behaviour is to simply allocated and fall
    //        through.  Re-sizing is the exception
    //  F11b, and a note on why this function is bounded at all: NOTHING CALLS
    //  IT. It is unreferenced across the repository -- an earlier draft of
    //  AllocIOMAGE, kept beside it. It is bounded anyway, on both loops, for
    //  one reason: an unbounded twin sitting next to a bounded original is how
    //  the bound gets lost again, by somebody reviving this one or copying from
    //  it. Deleting it would be better still and is a separate decision, since
    //  it is the only surviving record of what the allocator looked like before
    //  the free-list root moved.
    int     nResizes1 = 0;
    VBLsize nWalked   = 0;
    const VBLsize nMaxFree = P2PmsgHeap_MaxBlocks ( hVBList );
#ifndef XCtrlKeys
TOP:nWalked = 0;
    VBLaddr aVBLockFree = VBLockRoot_GetFirst ( &pHandle->u.IOMAGE.oRoot );
#else
TOP:nWalked = 0;
    VBLaddr aVBLockFree = VBHeapRoot_GetFree ( pHandle->u.IOMAGE.pRoot );
#endif
    while ( aVBLockFree )
    {
      if ( ++nWalked > nMaxFree )
        EVERR->MODULE
             ->Message(L"P2PmsgHeap_AllocIOMAGE1: free list exceeds %u blocks"
                      L" -- it is cyclic and does not describe this image"
                      , (UINT)nMaxFree )
             ->Throw();
      //P2PmsgHeapIO_Collate ( hVBHeap, aVBLockFree );
      pVBLock = VBList2PhysVBHeap ( hVBList, aVBLockFree );
ASSERT(VBHeap_IsFree(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsLinked(pVBLock));
      //  Still first-fit, unlike the two live arms, and deliberately so: the
      //  closer-fit walk is an IMPROVEMENT, so a revived copy of this function
      //  would merely allocate the way the library used to. F11b's bound is a
      //  SAFETY property and had to be carried here for the reason its note
      //  gives; a better choice of block does not.
      if ( VBHeap_Sizenn(pVBLock) < nSizeof )
      {
        aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
        continue;
      }

      // Allocated
      ASSERT(aVBLockFree);
      P2PmsgHeapIO_Split ( hVBList, aVBLockFree, nSizeof );
      P2PmsgHeap_AssertValidIOMAGE ( hVBList );
      //memset(pVBLock,0,nSizeof);//TODO:LJM delete
      VBHeap_Init ( pVBLock, pHandle->uAddrType, nSizeof );
ASSERT(P2PmsgHeap_AssertValidIOMAGE(hVBList));
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(!VBHeap_IsLinked(pVBLock));
ASSERT(VBHeap_IsFree(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
      return aVBLockFree;
    }

    // Resize
    // NOTES: Minimal re-sizes result in constant buffer copies and
    //        consequentually compromise performance
    //      : Zero re-sizes result in optimised increments in heap
    //        growth and consequently improved performance 
    ASSERT(aVBLockFree==0);
    //  F10's bound, on this unreferenced twin as well -- refer the note above.
    if ( ++nResizes1 > kMaxAllocResizes )
      EVERR->MODULE
           ->Message(L"P2PmsgHeap_AllocIOMAGE1: %u resizes did not satisfy %u"
                    L" bytes -- free list does not describe this image"
                    , (UINT)nResizes1, (UINT)nSizeof )
           ->Throw();
    if ( P2PmsgHeap_ResizeIOMAGE(hVBList,nSizeof,false) > 0 )
      goto TOP;
    ASSERT(aVBLockFree);
    return aVBLockFree;
}
VBLaddr
P2PmsgHeap_AllocIOMAGE ( P2PmsgHANDLE hVBList, VBLsize& nSizeof )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBHeap       *pVBLock = nullptr;
    ASSERT(P2PmsgHeap_AssertValidIOMAGE(hVBList));

    // Iterate through free entries
    // NOTES: Normal behaviour is to simply allocate and fall
    //        through.  Re-sizing is the exception
//P2PmsgHeap_AssertValidBSTRio(hVBHeap);
    int     nResizes  = 0;             // F10; see AllocBSTRio
    VBLsize nWalked   = 0;             // F11b; see AllocBSTRio
    bool    bFirstFit = false;         // see the re-check below, and kMaxFitWalk
    const VBLsize nMaxFree = P2PmsgHeap_MaxBlocks ( hVBList );
TOP:nWalked = 0;                       // per pass -- the walk restarts at the head
    //  Closer fit, reset per pass. Refer kMaxFitWalk.
    VBLaddr aVBLockBest = 0;
    VBLsize nSizeofBest = 0;
    VBLsize nFits       = 0;
    VBLaddr aVBLockFree = VBHeapRoot_GetFree(pHandle->u.IOMAGE.pRoot);
//TOP:VBLaddr aVBLockFree = pHandle->u.BSTRio.pBSTRio->oKeys.aFree;
    while ( aVBLockFree )
    {
      //  F11b on this arm. The fuzzer reached the BSTRio branch, because that is
      //  the branch its reproducer's tag selects -- and the F3 lesson, restated
      //  by the F2/F7 note in AssertValidIOMAGE, is that fixing the branch the
      //  fuzzer reached is not the same as fixing the pattern. Both arms walk a
      //  wire-supplied list by GetNext, so both are bounded here.
      if ( ++nWalked > nMaxFree )
        EVERR->MODULE
             ->Message(L"P2PmsgHeap_AllocIOMAGE: free list exceeds %u blocks"
                      L" -- it is cyclic and does not describe this image"
                      , (UINT)nMaxFree )
             ->Throw();
      P2PmsgHeap_CollateIOMAGE ( hVBList, aVBLockFree );
      pVBLock = VBList2PhysVBHeap ( hVBList, aVBLockFree );
ASSERT(VBHeap_IsFree(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsLinked(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
      const VBLsize nSizeofFree = VBHeap_Sizenn ( pVBLock );
      if ( nSizeofFree < nSizeof )
      {
        aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
        continue;
      }

      //  CLOSER FIT. A block that cannot be split is taken at once: the
      //  remainder would be too small to be a free block, so SplitAlloc hands
      //  the whole of it over (refer its "VBLock adoption" branch) and nothing
      //  further along the list can better no waste at all.
      if ( bFirstFit ||
           nSizeofFree - nSizeof < VBList_VBHeapMin(pVBLock->oHdr.uVBLockDefs) )
      {
        aVBLockBest = aVBLockFree;
        break;
      }
      if ( aVBLockBest == 0 || nSizeofFree < nSizeofBest )
      {
        aVBLockBest = aVBLockFree;
        nSizeofBest = nSizeofFree;
      }
      if ( ++nFits >= kMaxFitWalk )
        break;
      aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
    }

    if ( aVBLockBest )
    {
      //  THE CHOSEN BLOCK IS RE-CHECKED, because the walk that chose it also
      //  collates, and collating absorbs the block that physically FOLLOWS the
      //  one collated. The free list is in no address order, so a block visited
      //  late in the walk can sit immediately before a block chosen early in
      //  it, and swallow it. An absorbed block has its defs byte zeroed (refer
      //  the assignment in P2PmsgHeap_CollateIOMAGE), so the predicates below
      //  catch it rather than allocating from the middle of another block.
      //
      //  The answer is one more pass with bFirstFit set, which takes the first
      //  block that fits the moment it finds it. THAT pass cannot go stale --
      //  nothing is collated between finding the block and allocating it -- so
      //  the retry is taken at most once, and the throw below cannot be reached
      //  by any heap this allocator built.
      aVBLockFree = aVBLockBest;
      pVBLock     = VBList2PhysVBHeap ( hVBList, aVBLockFree );
      if ( pVBLock == nullptr                         ||
           !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) ||
           !VBHeap_IsAlloc(pVBLock)                   ||
           !VBHeap_IsLinked(pVBLock)                  ||
           !VBHeap_IsFree(pVBLock)                    ||
           VBHeap_Sizenn(pVBLock) < nSizeof              )
      {
        ASSERT(!bFirstFit);
        if ( !bFirstFit )
        {
          bFirstFit = true;
          goto TOP;
        }
        EVERR->MODULE->AFP(aVBLockFree)
             ->Message(L"P2PmsgHeap_AllocIOMAGE: the chosen free block did not"
                      L" survive the walk -- free list does not describe this"
                      L" image" )
             ->Throw();
      }

      // Allocated
      ASSERT(aVBLockFree);
      //P2PmsgHeap_AssertValidBSTRio ( hVBHeap );
      P2PmsgHeap_SplitAllocIOMAGE ( hVBList, aVBLockFree, nSizeof );
      //pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize += nSizeof;
      //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  -= nSizeof;
      //P2PmsgHeap_AssertValidBSTRio ( hVBHeap );
      //memset(pVBLock,0,nSizeof);//TODO:LJM delete
      VBHeap_Init ( pVBLock, pHandle->uAddrType, nSizeof );
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(!VBHeap_IsLinked(pVBLock));
ASSERT(VBHeap_IsFree(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
      if ( VBHeapRoot_GetAlloc(pHandle->u.IOMAGE.pRoot) == 0 )
        VBHeapRoot_SetAlloc(pHandle->u.IOMAGE.pRoot,aVBLockFree);
        //pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc = aVBLockFree;
      ASSERT(P2PmsgHeap_AssertValidIOMAGE(hVBList));
      pHandle -> bDirty = true;
      return aVBLockFree;
    }

    // Resize
    // NOTES: Minimal re-sizes result in constant buffer copies and
    //        consequentually compromise performance
    //      : Zero re-sizes result in optimised increments in heap
    //        growth and consequently improved performance
    //      : F10. The retry is BOUNDED. See P2PmsgHeap_AllocBSTRio for the
    //        finding in full -- this is the same loop on the IOMAGE arm, and
    //        it is bounded here for the reason F3 gives about forks.
    ASSERT(aVBLockFree==0);
    if ( ++nResizes > kMaxAllocResizes )
      EVERR->MODULE
           ->Message(L"P2PmsgHeap_AllocIOMAGE: %u resizes did not satisfy %u bytes"
                    L" -- free list does not describe this image"
                    , (UINT)nResizes, (UINT)nSizeof )
           ->Throw();
    if ( P2PmsgHeap_ResizeIOMAGE(hVBList,nSizeof,true) > 0 )
      goto TOP;
    ASSERT(aVBLockFree);
    ASSERT(P2PmsgHeap_AssertValidIOMAGE(hVBList));
    return aVBLockFree;
}

VBLaddr
P2PmsgHeap_AllocBSTRio ( P2PmsgHANDLE hVBList, VBLsize& nSizeof )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    VBHeap       *pVBLock = 0;

    // Iterate through free entries
    // NOTES: Normal behaviour is to simply allocate and fall
    //        through.  Re-sizing is the exception
    //  The two lines deleted here tested aFreeSize against 100 and 200,000,000
    //  and then assigned pVBLock = 0, which the loop below overwrites before
    //  reading -- so they did nothing at all. They are worth a sentence rather
    //  than a silent deletion: that 200,000,000 appears in six other places in
    //  this file, five of them inside ASSERT, and it is the fingerprint of
    //  somebody watching this allocator run away and bounding the SYMPTOM in a
    //  build where ASSERT still exists. F10 is the cause, bounded below.
    //
    //  F11b. THE WALK ITSELF IS BOUNDED, and F10 did not bound it.
    //
    //  THIS IS NOT WHAT FIXED F11, and the distinction is the point of the
    //  label. F11 was a libFuzzer timeout in this allocator's call tree, and
    //  the first diagnosis was that the free list had been made cyclic here.
    //  It had not: with the bound below in place the counter NEVER FIRED on the
    //  reproducer, and the hang persisted until the real defect was found one
    //  level down in P2PmsgHeap_CollateBSTRio. Read that note for F11 itself.
    //
    //  The bound stays because the walk was genuinely unbounded and the review
    //  that found F11 found this on the way past. Read F10's note at the resize
    //  below and then its parenthesis again: "because the list is empty, OR
    //  CYCLIC, or every block on it declares a size too small -- the walk above
    //  falls through". A cyclic list does NOT fall through. The `continue`
    //  follows GetNext round the cycle for ever, control never reaches the
    //  resize, and nResizes -- the whole of F10 -- never increments. So F10
    //  bounded the path taken when this walk TERMINATES and left the path where
    //  it does not, and the one case its own text names as a cause was the one
    //  case it could not catch. That remains true whether or not any input has
    //  yet been seen to do it.
    //
    //  The bound is a COUNT and needs no visited set: every block carries at
    //  least a header, so MaxBlocks bounds how many distinct blocks can exist
    //  and a cycle must revisit one. Same argument and same helper as the
    //  free-list walk in P2PmsgHeap_AssertValidIOMAGE -- this is that guard
    //  applied to the allocator, not a new idea.
    int     nResizes  = 0;
    VBLsize nWalked   = 0;
    bool    bFirstFit = false;         // see the re-check below, and kMaxFitWalk
    const VBLsize nMaxFree = P2PmsgHeap_MaxBlocks ( hVBList );
TOP:nWalked = 0;                       // per pass -- the walk restarts at the head
    //  Closer fit, reset per pass. Refer kMaxFitWalk.
    VBLaddr aVBLockBest = 0;
    VBLsize nSizeofBest = 0;
    VBLsize nFits       = 0;
    VBLaddr aVBLockFree = pHandle->u.BSTRio.pBSTRio->oKeys.aFree;
    while ( aVBLockFree )
    {
      if ( ++nWalked > nMaxFree )
        EVERR->MODULE
             ->Message(L"P2PmsgHeap_AllocBSTRio: free list exceeds %u blocks"
                      L" -- it is cyclic and does not describe this image"
                      , (UINT)nMaxFree )
             ->Throw();
      P2PmsgHeap_CollateBSTRio ( hVBList, aVBLockFree );
      pVBLock = VBList2PhysVBHeap ( hVBList, aVBLockFree );
ASSERT(VBHeap_IsFree(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(VBHeap_IsLinked(pVBLock));
      const VBLsize nSizeofFree = VBHeap_Sizenn ( pVBLock );
      if ( nSizeofFree < nSizeof )
      {
        aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
        continue;
      }

      //  CLOSER FIT. A block that cannot be split is taken at once: the
      //  remainder would be too small to be a free block, so SplitAlloc hands
      //  the whole of it over (refer its "VBLock adoption" branch) and nothing
      //  further along the list can better no waste at all.
      if ( bFirstFit ||
           nSizeofFree - nSizeof < VBList_VBHeapMin(pVBLock->oHdr.uVBLockDefs) )
      {
        aVBLockBest = aVBLockFree;
        break;
      }
      if ( aVBLockBest == 0 || nSizeofFree < nSizeofBest )
      {
        aVBLockBest = aVBLockFree;
        nSizeofBest = nSizeofFree;
      }
      if ( ++nFits >= kMaxFitWalk )
        break;
      aVBLockFree = VBHeap_GetNext ( pVBLock, 0 );
    }

    if ( aVBLockBest )
    {
      //  THE CHOSEN BLOCK IS RE-CHECKED, because the walk that chose it also
      //  collates, and collating absorbs the block that physically FOLLOWS the
      //  one collated. The free list is in no address order, so a block visited
      //  late in the walk can sit immediately before a block chosen early in
      //  it, and swallow it. An absorbed block has its defs byte zeroed (refer
      //  the assignment in P2PmsgHeap_CollateBSTRio), so the predicates below
      //  catch it rather than allocating from the middle of another block.
      //
      //  The answer is one more pass with bFirstFit set, which takes the first
      //  block that fits the moment it finds it. THAT pass cannot go stale --
      //  nothing is collated between finding the block and allocating it -- so
      //  the retry is taken at most once, and the throw below cannot be reached
      //  by any heap this allocator built.
      aVBLockFree = aVBLockBest;
      pVBLock     = VBList2PhysVBHeap ( hVBList, aVBLockFree );
      if ( pVBLock == nullptr                         ||
           !VBHeap_IsAddr(pVBLock,pHandle->uAddrType) ||
           !VBHeap_IsAlloc(pVBLock)                   ||
           !VBHeap_IsLinked(pVBLock)                  ||
           !VBHeap_IsFree(pVBLock)                    ||
           VBHeap_Sizenn(pVBLock) < nSizeof              )
      {
        ASSERT(!bFirstFit);
        if ( !bFirstFit )
        {
          bFirstFit = true;
          goto TOP;
        }
        EVERR->MODULE->AFP(aVBLockFree)
             ->Message(L"P2PmsgHeap_AllocBSTRio: the chosen free block did not"
                      L" survive the walk -- free list does not describe this"
                      L" image" )
             ->Throw();
      }

      // Allocated
      ASSERT(aVBLockFree);
      //P2PmsgHeap_AssertValidBSTRio ( hVBHeap );
      P2PmsgHeap_SplitAllocBSTRio ( hVBList, aVBLockFree, nSizeof );
      //pHandle->u.BSTRio.pBSTRio->oKeys.aAllocSize += nSizeof;
      //pHandle->u.BSTRio.pBSTRio->oKeys.aFreeSize  -= nSizeof;
      //P2PmsgHeap_AssertValidBSTRio ( hVBHeap );
      //memset(pVBLock,0,nSizeof);//TODO:LJM delete
      VBHeap_Init ( pVBLock, pHandle->uAddrType, nSizeof );
ASSERT(VBHeap_IsAlloc(pVBLock));
ASSERT(!VBHeap_IsLinked(pVBLock));
ASSERT(VBHeap_IsFree(pVBLock));
ASSERT(VBHeap_IsAddr(pVBLock,pHandle->uAddrType));
      if ( pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc == 0 )
        pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc = aVBLockFree;
//P2PmsgHeap_AssertValidBSTRio(hVBList);
      pHandle -> bDirty = true;
      return aVBLockFree;
    }

    // Resize
    // NOTES: Minimal re-sizes result in constant buffer copies and
    //        consequentually compromise performance
    //      : Zero re-sizes result in optimised increments in heap
    //        growth and consequently improved performance 
    //  F10. THE RETRY IS BOUNDED, and it was not.
    //
    //  oKeys.aFree is the free-list head and it comes out of the image like
    //  everything else here. If no block on that list can satisfy nSizeof --
    //  because the list is empty, or cyclic, or every block on it declares a
    //  size too small -- the walk above falls through, this resize grows the
    //  arena by 20% of itself, and `goto TOP` tries again. On a well-formed
    //  heap the retry succeeds immediately, because the resize adds a free
    //  block big enough by construction. On a heap whose free list does not
    //  describe it, the retry fails for the same reason it failed the first
    //  time, and the loop grows 20% per pass: geometric, so a 1 MB arena
    //  reaches gigabytes in about sixty passes and stops only when malloc
    //  does. nSizeofMax is no help -- an Addr32 image sets it near 4 GB.
    //
    //  Found by the receive-path fuzzer's mutation phase: a 65-byte request
    //  against a 1 MB arena drove a single malloc past 256 MB, stack
    //  Alloc -> AllocBSTRio -> ResizeBSTRio. It is memory exhaustion rather
    //  than memory corruption -- a crafted store that is loaded and then
    //  WRITTEN to, which is what any ordinary use of a P2PmsgMgr does.
    //
    //  Two is the bound because one is the answer: a resize that honours
    //  nSizeofNeeded creates a block that satisfies the request, so a second
    //  pass is already suspicious and a third cannot be legitimate.
    ASSERT(aVBLockFree==0);
    if ( ++nResizes > kMaxAllocResizes )
      EVERR->MODULE
           ->Message(L"P2PmsgHeap_AllocBSTRio: %u resizes did not satisfy %u bytes"
                    L" -- free list does not describe this image"
                    , (UINT)nResizes, (UINT)nSizeof )
           ->Throw();
    if ( P2PmsgHeap_ResizeBSTRio(hVBList,nSizeof,true) > 0 )
      goto TOP;
    ASSERT(aVBLockFree);
    return aVBLockFree;
}

VBLaddr
P2PmsgHeap_AllocSYS ( P2PmsgHANDLE hVBList, VBLsize nSizeofBlock )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    VBLaddr       aVBLock = 0;

    // Simply allocate from system heap
    // NOTES: Zeroed in subsequent initialisation sequences
    char *paVBLock = new char [nSizeofBlock];
    aVBLock = reinterpret_cast<VBLaddr>(paVBLock);
    //memset((char*)aVBLock,0,nSizeofBlock);

    // Establish allocations map for this P2PmsgHeapSYS handle
    // NOTES: Observe cached optimisation
    if ( !pHandle->u.SYS.pCMapAlloc )
    {
      if ( pHandle->u.SYS.uVBLock0 == 0 )
        return pHandle->u.SYS.uVBLock0 = aVBLock;
      else if ( pHandle->u.SYS.uVBLock1 == 0 )
        return pHandle->u.SYS.uVBLock1 = aVBLock;
      else if ( pHandle->u.SYS.uVBLock2 == 0 )
        return pHandle->u.SYS.uVBLock2 = aVBLock;
      else
        pHandle->u.SYS.pCMapAlloc = new CMapAlloc();
    }
    pHandle->u.SYS.pCMapAlloc->SetAt ( aVBLock, aVBLock );

    // Tidy up, and
    return aVBLock;
}
//
//  Allocates an entry from the P2PmsgHeap
//  NOTES: Address of allocated entry is only valid with respect
//         to the VBHeap from which its allocated
//       : Refer complimentary P2PmsgHeap_Free() for releasing
//         previously allocated entries.
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//               Handle for VBHeap from which entry is to be allocated
//
//               UCHAR uVBLock
//
//               VBLsize nSizeofVBLock
//               Sizeof VBLock to be allocated
//               NOTES: This may be subsequently increased to minimum
//                      VBHeap allocation size
//
//  Returns:     VBLaddr
//               Address of allocated entry.
VBLaddr
P2PmsgHeap_Alloc ( P2PmsgHANDLE hVBList
                 , UCHAR uVBLock, VBLsize nSizeofBlock )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
                  pHandle -> bDirty = true;
    VBLaddr       aBlock  = 0;
    // TODO:LJM-FIX-ME addition of VBLockHdr::Hdr should be done in P3PmsgObject::Alloc()
    nSizeofBlock += P2PmsgHeap_Sizeof_Hdr ( hVBList );
    if ( nSizeofBlock < pHandle->nVBLockMin )
      nSizeofBlock = pHandle->nVBLockMin;
    ASSERT((uVBLock&VBLock_TypeMask)==uVBLock);

    // To be sure, to be sure
    // NOTES: This is NOT the arena ceiling, and reading it as one is finding M6.
    //        nSizeofUsed is live allocated bytes -- it lags nSizeofAlloc by the
    //        oversize slack and falls again on every Free, so it can sit under
    //        nSizeofMax while the arena is already past it. The invariant that
    //        actually bounds an address lives in P2PmsgHeap_CapGrowth, on the
    //        two paths where the arena grows. Kept here because a request that
    //        cannot fit in the live figure cannot fit in the arena either, and
    //        failing early costs nothing.
    if ( pHandle->nSizeofUsed+nSizeofBlock > pHandle->nSizeofMax )
      EVERR->MODULE
           ->AFP(uVBLock)->AFP(nSizeofBlock)
           ->Message("Attempt to exceed maximum P2PmsgHeap size of %I32i bytes", pHandle->nSizeofMax )
           ->Throw();

    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
    {
      //P2PmsgHeap_AssertValidIOMAGE(hVBHeap);
      aBlock = P2PmsgHeap_AllocIOMAGE ( hVBList, nSizeofBlock );
      pHandle -> nSizeofUsed += nSizeofBlock;
      //P2PmsgHeap_AssertValidIOMAGE(hVBHeap);
      P2PASSERT(aBlock<0x0fffffff);
    }
    else if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
    {
      //P2PmsgHeap_AssertValidBSTRio(hVBHeap);
      aBlock = P2PmsgHeap_AllocBSTRio ( hVBList, nSizeofBlock );
      pHandle -> nSizeofUsed += nSizeofBlock;
      //P2PmsgHeap_AssertValidBSTRio(hVBHeap);
      P2PASSERT(aBlock<0x0fffffff);
    }
    else if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
    {
      //P2PmsgHeapSYS_AssertValid(hVBHeap);
      aBlock = P2PmsgHeap_AllocSYS ( hVBList, nSizeofBlock );
      pHandle -> nSizeofUsed += nSizeofBlock;
      //P2PmsgHeapSYS_AssertValid(hVBHeap);
      P2PASSERT(aBlock<(~(UINT_PTR)0>>1));            // was0xDfffffff);
    }

    VBLock *pBlock = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aBlock );
    VBLock_Init ( pBlock, pHandle->uAddrType|uVBLock, nSizeofBlock );
    return aBlock;
}

BOOL
P2PmsgHeap_IsDirty ( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    return pHandle ? pHandle -> bDirty : FALSE;
}

BOOL
P2PmsgHeap_SetDirty ( P2PmsgHANDLE hVBList, BOOL bDirty )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    if ( pHandle )
      pHandle -> bDirty = bDirty;
    return bDirty;
}
//
//  Retrieves P2PmsgHeap memory offset for handle
//
//  Parameters:  P2PmsgHANDLE hVBHeap
//               Handle for VBHeap from which offset is to be retrieved
//
//  Returns:     VBLaddr
//               P2PmsgHeap offset.
VBLaddr
P2PmsgHeap_AllocSeqnum ( P2PmsgHANDLE hVBHeap )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBHeap);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
    {
      return pHandle -> u.IOMAGE.aIOmage;
    }
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
    {
      return pHandle->u.BSTRio.aBSTRio;
    }
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      return pHandle->u.SYS.uVBLock0;
    ASSERT(0);
    return 0;
}

///////////////////////////////////////////////////////////////////////
//  VBListNode operations
//

///////////////////////////////////////////////////////////////////////
//  VBListItem operations
//



///////////////////////////////////////////////////////////////////////
//  VBListBlock operations
//
VBLock*
P2PmsgHeap_Block2Phys ( P2PmsgHANDLE hVBList, VBLaddr aVBLaddrBlock )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    //  This is the one place every heap walk turns a stored address into a
    //  pointer, and for a received image those addresses come off the WIRE:
    //  block sizes, free-list nNext/nPrev links. The bound was stated here as
    //  an ASSERT, which is compiled out of Release entirely — so in Release the
    //  out-of-range address was translated and handed back, and the caller
    //  dereferenced it. p2p_fuzzframe reaches that with a single mutated byte
    //  (`p2p_fuzzframe 0x5EEDF00D --replay 9 1`, an access violation).
    //
    //  Corruption is reported the way the rest of this layer reports it — by
    //  throwing, as VBLock_Hdr_u_SizeNN does for a bad addressing mode — because
    //  the receive path already treats a throw as "refuse this frame", and there
    //  is no in-band way for a function returning VBLock* to say "no".
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
    {
      if ( aVBLaddrBlock == 0 || aVBLaddrBlock >= pHandle->nSizeofAlloc )
        EVERR->Module ( "%s(%llu)", __FUNCTION__, (unsigned long long)aVBLaddrBlock )
             ->Message( "VBHeap IOMAGE block address outside the image" )
             ->Throw ( );
      return (VBLock *)(pHandle -> u.IOMAGE.aIOmage + aVBLaddrBlock);
    }
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
    {
      if ( aVBLaddrBlock == 0 || aVBLaddrBlock >= pHandle->nSizeofAlloc )
        EVERR->Module ( "%s(%llu)", __FUNCTION__, (unsigned long long)aVBLaddrBlock )
             ->Message( "VBHeap BSTRio block address outside the image" )
             ->Throw ( );
      return (VBLock *)(pHandle->u.BSTRio.aBSTRio + aVBLaddrBlock);
    }
    if ( pHandle->uVBListType == P2PmsgHeap_SYSTEM )
      return (VBLock *)aVBLaddrBlock;
    return nullptr;
}

VBLsize
P2PmsgHeap_Sizeof_Hdr ( P2PmsgHANDLE hVBHeap ) noexcept
{
    // Observe addressing model
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBHeap);
    //VBListHANDLE *pHandle = (VBListHANDLE *)hVBHeap;
    UCHAR         uVBLock = pHandle->uAddrType & VBLock_AddrMask;
    if ( uVBLock == VBLock_Addr32 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize32);
    if ( uVBLock == VBLock_Addr64 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize64);
    if ( uVBLock == VBLock_Addr16 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize16);
    if ( uVBLock == VBLock_Addr08 )
      return sizeof(soHdr) - sizeof(soHdr.u) + sizeof(soHdr.u.nSize08);
    return 0u;
}

///////////////////////////////////////////////////////////////////////////////
//  VBListBSTRio header static manipulators
//  NOTES: Provide address sensitive VBListBSTRio manipulation
void
P2PmsgHeap_InitBSTRio ( VBListBSTRio *pBSTRio
                      , VBLsize nSizeofBSTRio, UCHAR uVBLock )
{
    ASSERT(nSizeofBSTRio>=sizeof(VBListBSTRio));
    ZeroMemory ( pBSTRio, sizeof(VBListBSTRio) );
    UCHAR *pDefs = (UCHAR *)&pBSTRio->oDefs.uDefs1;
    pDefs[0] = P2PmsgHeap_BSTRio;
    pDefs[1] = uVBLock & VBLock_AddrMask;
    pDefs[2] = 0;                      // Spare
    pDefs[3] = 0;                      // Spare
    pBSTRio->oDefs.uComp2 = ~pBSTRio->oDefs.uDefs1;

    pBSTRio->oSize.aSize1 =  nSizeofBSTRio;
    pBSTRio->oSize.aComp2 = ~nSizeofBSTRio;

    ASSERT(nSizeofBSTRio>sizeof(VBListBSTRio)+sizeof(VBLock));
    pBSTRio->oKeys.aFree        = (VBLaddr)&pBSTRio->cTag - (VBLaddr)pBSTRio;
    pBSTRio->oKeys.aFreeLast    = pBSTRio->oKeys.aFree;
    pBSTRio->oKeys.aFreeSize    = nSizeofBSTRio - sizeof(VBListBSTRio) + sizeof(pBSTRio->cTag);
    pBSTRio->oKeys.nFreeEntries = 1;
    VBHeap *pVBHeap = (VBHeap *)&pBSTRio->cTag;
    VBHeap_Init ( pVBHeap, uVBLock, pBSTRio->oKeys.aFreeSize );
    pVBHeap->oHdr.uVBLockDefs  |=  VBLock_Linked;
    pVBHeap->oHdr.uVBLockDefs  |=  VBLock_Alloc;
    pVBHeap->oHdr.uVBLockDefs  &= ~VBLock_TypeMask; // Free entries have no type

static int nCount = 1;
    if ( ++nCount%100 == 0 ) {
      ASSERT(VBHeap_IsAlloc(pVBHeap));
      ASSERT(VBHeap_IsLinked(pVBHeap));//TODO: delete
      ASSERT(VBHeap_IsFree(pVBHeap));//TODO: delete
      ASSERT(VBHeap_IsAddr(pVBHeap,uVBLock));
      P2PmsgHeap_AssertValidBSTRio ( pBSTRio );
      nCount = 1;
    }
}
void
P2PmsgHeap_InitIOMAGE ( VBHeapIOMAGE *pIOMAGE
                      , VBLsize nSizeofIOMAGE, UCHAR uVBLock )
{
    //ZeroMemory ( pIOMAGE, sizeof(VBHeapIOMAGE) );
    VBHeapRoot_Init ( &pIOMAGE->oRoot, uVBLock, 0 );
    VBLaddr aFree     = sizeof(VBHeapIOMAGE) - sizeof(VBHeapIOMAGE::oRoot) + VBHeapRoot_Sizeof(&pIOMAGE->oRoot);
    VBLsize nFreeSize = nSizeofIOMAGE - aFree;
    ASSERT(nFreeSize<nSizeofIOMAGE);

    ASSERT(nSizeofIOMAGE>sizeof(VBHeapIOMAGE)+sizeof(VBLock));
    VBHeapRoot_SetFree ( &pIOMAGE->oRoot, aFree );
    //pBSTRio->oKeys.aFree        = (VBLaddr)&pIOMAGE->cTag - (VBLaddr)pBSTRio;
    VBHeapRoot_SetFreeLast  ( &pIOMAGE->oRoot, aFree );
    //pBSTRio->oKeys.aFreeLast    = pBSTRio->oKeys.aFree;
    VBHeapRoot_SetFreeSize  ( &pIOMAGE->oRoot, nFreeSize, true );
    //pBSTRio->oKeys.aFreeSize    = nSizeofIOMAGE - sizeof(VBListBSTRio) + sizeof(pIOMAGE->cTag);
    VBHeapRoot_SetFreeItems ( &pIOMAGE->oRoot, 1, true );
    //pBSTRio->oKeys.nFreeEntries = 1;
    VBHeap *pVBHeap = (VBHeap *)((VBLaddr)pIOMAGE + aFree);
    VBHeap_Init ( pVBHeap, uVBLock, VBHeapRoot_GetFreeSize(&pIOMAGE->oRoot) );
    //VBHeap_Init ( pVBHeap, uVBLock, pBSTRio->oKeys.aFreeSize );
    pVBHeap->oHdr.uVBLockDefs  |=  VBLock_Linked;
    pVBHeap->oHdr.uVBLockDefs  |=  VBLock_Alloc;
    pVBHeap->oHdr.uVBLockDefs  &= ~VBLock_TypeMask; // Free entries have no type
ASSERT((uVBLock&VBLock_AddrMask)==pIOMAGE->oRoot.uVBLock);
ASSERT((uVBLock&VBLock_AddrMask)==(pVBHeap->oHdr.uVBLockDefs&VBLock_AddrMask));

ASSERT(VBHeap_IsAlloc(pVBHeap));
ASSERT(VBHeap_IsLinked(pVBHeap));//TODO: delete
ASSERT(VBHeap_IsFree(pVBHeap));//TODO: delete
ASSERT(VBHeap_IsAddr(pVBHeap,uVBLock));
    // BUGFIX (Linux port): removed P2PmsgHeap_AssertValidIOMAGE(pIOMAGE) here.
    // It passed the VBHeapIOMAGE* image BUFFER to a validator that expects a
    // VBListHANDLE* (type confusion): AssertValidIOMAGE does static_cast<VBListHANDLE*>
    // and reads uVBListType/uAddrType. It usually early-returned (image byte at the
    // uVBListType offset != P2PmsgHeap_IOMAGE), but ~1-2% of the time (address-dependent
    // image contents) that byte was 0x01 and the uAddrType-offset byte was >3, firing
    // `(uAddrType&0x03)==uAddrType' on a non-handle. Reads were in-bounds of the live
    // image alloc, so ASan/TSan saw nothing. The REAL handle is validated by the caller
    // (P2PmsgHeap_CreateIOMAGE: ASSERT(P2PmsgHeap_AssertValidIOMAGE(pHandle))). Latent on
    // Windows too (Release compiles ASSERT out); exposed on Linux where ASSERT is active.
}

BOOL
P2PmsgHeap_IsBSTRio ( const void *vpBSTRio )
{
    VBListBSTRio *pBSTRio = (VBListBSTRio *)vpBSTRio;
    UCHAR *pDefs = (UCHAR *)&pBSTRio->oDefs.uDefs1;
    if ( pDefs[0] != P2PmsgHeap_BSTRio )
      return FALSE;
    if ( (pBSTRio->oDefs.uDefs1&pBSTRio->oDefs.uComp2) !=  0 ||
         (pBSTRio->oDefs.uDefs1|pBSTRio->oDefs.uComp2) != ~0    )
      return FALSE;
    if ( (pBSTRio->oSize.aSize1&pBSTRio->oSize.aComp2) !=  0 ||
         (pBSTRio->oSize.aSize1|pBSTRio->oSize.aComp2) != ~0    )
      return FALSE;
    return TRUE;
}

//
//  Classifies an IOMAGE synchronisation header
//  NOTES: Separates "not an IOMAGE at all" from "an IOMAGE written by a peer
//         of the opposite endianness", so callers can report the latter as
//         what it is instead of as heap corruption (byte_order.md §4.2).
//       : The complement relation is checked first and is endian-blind by
//         construction; the sentinel carries the byte-order signal.
int
P2PmsgHeap_IOMAGEform ( const void *vpIOmage )
{
    const VBListIOmage *pIOmage = (const VBListIOmage *)vpIOmage;
    if( (pIOmage->oSync.uiSync1&pIOmage->oSync.uiSync2) !=  0 ||
        (pIOmage->oSync.uiSync1|pIOmage->oSync.uiSync2) != ~0    )
      return VBLockSync_Invalid;
    return VBLock_SyncForm ( pIOmage->oSync.uiSync1 );
}

BOOL
P2PmsgHeap_IsIOMAGE ( const void *vpIOmage )
{
    // A byte-swapped image is NOT a usable IOMAGE - reject it here and let the
    // callers that can raise a diagnostic do so via P2PmsgHeap_IOMAGEform.
    const int nForm = P2PmsgHeap_IOMAGEform ( vpIOmage );
    if ( nForm == VBLockSync_Native || nForm == VBLockSync_Legacy )
      return TRUE;
    return FALSE;
}

UCHAR
P2PmsgHeap_AddnnBSTRio ( const VBListBSTRio *pBSTRio )
{
    UCHAR *pDefs = (UCHAR *)&pBSTRio->oDefs.uDefs1;
    return pDefs[1];
}

VBLaddr
P2PmsgHeap_ConnectIOMAGE (  P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    ASSERT(pHandle->uVBListType==P2PmsgHeap_IOMAGE);
    ASSERT(P2PmsgHeap_IsIOMAGE(pHandle->u.IOMAGE.pIOmage));
    return VBHeapRoot_GetAlloc ( pHandle->u.IOMAGE.pRoot );
    //return sizeof(pHandle->u.IOMAGE.pIOmage->oSync);
}

VBLaddr
P2PmsgHeap_ConnectBSTRio (  P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    ASSERT(pHandle->uVBListType==P2PmsgHeap_BSTRio);
    ASSERT(P2PmsgHeap_IsBSTRio(pHandle->u.BSTRio.pBSTRio));
    return pHandle->u.BSTRio.pBSTRio->oKeys.aAlloc;
}

VBLaddr
P2PmsgHeap_Connect ( P2PmsgHANDLE hVBList )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return P2PmsgHeap_ConnectIOMAGE ( hVBList );
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return P2PmsgHeap_ConnectBSTRio ( hVBList );
    ASSERT(0);
    return (VBLaddr)~0u;
}

//
//  Checks for root VBLaddr
//
//  Parameters:  VBLaddr 
//               Address to be checked
//
//  Result:      BOOL
//               Operation result
//                 TRUE... Supplied address is root address
//                 FALSE.. Not root address
BOOL
P2PmsgHeap_IsRoot ( P2PmsgHANDLE hMsgHeap, VBLaddr aVBLock )
{
    VBListHANDLE *pHandle = (VBListHANDLE *)hMsgHeap;
    if ( pHandle->uVBListType == P2PmsgHeap_IOMAGE )
      return P2PmsgHeap_ConnectIOMAGE(hMsgHeap) == aVBLock ? TRUE : FALSE;
    if ( pHandle->uVBListType == P2PmsgHeap_BSTRio )
      return P2PmsgHeap_ConnectBSTRio(hMsgHeap) == aVBLock ? TRUE : FALSE;
    ASSERT(0);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
//  Trouble shooting utilities


///////////////////////////////////////////////////////////////////////
//  P2PmsgHeap triggers

UINT
P2PmsgHeap_CreateTrigger ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, HWND hWnd
                         , UINT uiWM_APP_Trigger, UINT nTypeTrigger, LPARAM lParam )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle->u.BSTRio.pCMapTriggers == nullptr )
      pHandle->u.BSTRio.pCMapTriggers = new CMapTriggers;
    CMapTriggers *pCMapTriggers = pHandle->u.BSTRio.pCMapTriggers;

    // Create and isolate registered triggers list
    CListRegTriggers *pCListRegTriggers;
    if ( !pCMapTriggers->Lookup(aVBLock,pCListRegTriggers) )
    {
      pCMapTriggers -> SetAt(aVBLock, new CListRegTriggers );
      pCMapTriggers -> Lookup ( aVBLock, pCListRegTriggers );
    }
    // Isolate an existing registration
    STrigger *pSTrigger = 0;
    POSITION pos = pCListRegTriggers->GetHeadPosition();
    while ( pos && pSTrigger == nullptr )
    {
      //POSITION posDrop = pos;
      pSTrigger = &pCListRegTriggers -> GetNext ( pos );
      if ( pSTrigger->hWnd  != hWnd         ||
           pSTrigger->nType != nTypeTrigger    )
        pSTrigger = nullptr;
    }
    // Create registration placeholder
    if ( pSTrigger == nullptr )
    {
      STrigger oSTrigger;
      oSTrigger.hWnd  = hWnd;
      oSTrigger.nType = nTypeTrigger;
      pCListRegTriggers -> AddTail ( oSTrigger );
      pSTrigger = &pCListRegTriggers->GetTail();
    }
    pSTrigger -> uiWM_APP_Trigger = uiWM_APP_Trigger;
    pSTrigger -> lParam           = lParam;
    return 0;
}

void
P2PmsgHeap_SetTriggerSink ( P2PmsgHANDLE hVBList, P2PmsgTriggerSink pfn, void* pUser )
{
    VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    if ( pHandle == nullptr )
      return;
    pHandle -> pfnTriggerSink  = pfn;
    pHandle -> pTriggerSinkUser = pUser;
}

UINT
P2PmsgHeap_ProcTriggers ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, UINT nMaskTriggers )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    CMapTriggers *pCMapTriggers = pHandle->u.BSTRio.pCMapTriggers;
    if ( pCMapTriggers == nullptr )
      return 0;
    // Confirm registrations exist
    CListRegTriggers *pCListRegTriggers = nullptr;
    if ( !pCMapTriggers->Lookup ( aVBLock, pCListRegTriggers ) )
      return 0;
    // Process registrations
    UINT nTriggers = 0;
    POSITION pos = pCListRegTriggers->GetHeadPosition();
    while ( pos )
    {
      STrigger& oSTrigger = pCListRegTriggers -> GetNext ( pos );
      if ( (oSTrigger.nType&nMaskTriggers) != oSTrigger.nType )
        continue;
      PostMessage ( oSTrigger.hWnd, oSTrigger.uiWM_APP_Trigger
                  , aVBLock, oSTrigger.lParam );
      // Headless sink: notify a windowless host in the same pass. Fires per
      // registration (so a node armed for INSERT|UPDATE reports each kind
      // distinctly); posP2Pobject is aVBLock, which the trigger contract treats
      // as the object's P2Pos.
      if ( pHandle->pfnTriggerSink != nullptr )
        pHandle->pfnTriggerSink ( pHandle->pTriggerSinkUser
                                , oSTrigger.nType
                                , (unsigned long long)aVBLock );
      nTriggers++;
    }
    // Drop all registrations since P2Pmsg item no longer exists
    if ( (nMaskTriggers&TRIGGER_DELETE) == TRIGGER_DELETE )
    {
      delete pCListRegTriggers;
      pCMapTriggers-> RemoveKey ( aVBLock );
    }
    return nTriggers;
}
UINT
P2PmsgHeap_DropTrigger ( P2PmsgHANDLE hVBList, VBLaddr aVBLock
                       , HWND hWnd, UINT nTriggerTypeMask )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    CMapTriggers *pCMapTriggers = pHandle->u.BSTRio.pCMapTriggers;
    // Confirm registrations exist
    CListRegTriggers *pCListRegTriggers = nullptr;
    if ( !pCMapTriggers                                        ||
         !pCMapTriggers->Lookup ( aVBLock, pCListRegTriggers )    )
      return 0;
    // Process registrations
    UINT nTriggers = 0;
    POSITION pos = pCListRegTriggers->GetHeadPosition();
    while ( pos )
    {
      POSITION posDrop = pos;
      STrigger& oSTrigger = pCListRegTriggers -> GetNext ( pos );
      if ( oSTrigger.hWnd != hWnd )
        continue;
      oSTrigger.nType &= ~nTriggerTypeMask;
      if ( (oSTrigger.nType&nTriggerTypeMask) != oSTrigger.nType )
        continue;
      pCListRegTriggers->RemoveAt ( posDrop );
      nTriggers++;
    }
    // Drop trigger if no registrations exist
    if ( pCListRegTriggers->IsEmpty() )
    {
      delete pCListRegTriggers;
      pCMapTriggers-> RemoveKey ( aVBLock );
    }
    return nTriggers;
}
UINT
P2PmsgHeap_DropTriggers ( P2PmsgHANDLE hVBList, HWND hWnd, UINT nTriggerTypeMask )
{
    const VBListHANDLE *pHandle = static_cast<VBListHANDLE *>(hVBList);
    CMapTriggers *pCMapTriggers = pHandle->u.BSTRio.pCMapTriggers;
    UINT          nTriggers = 0;
    if ( pCMapTriggers == nullptr )
      return nTriggers;
    VBLaddr       uiKey = 0;
    CListRegTriggers *pCListRegTriggers = nullptr;
    POSITION pos = pCMapTriggers->GetStartPosition();
    while ( pos )
    {
      pCMapTriggers -> GetNextAssoc ( pos, uiKey, pCListRegTriggers ); 
      // Process registrations
      POSITION posList = pCListRegTriggers->GetHeadPosition();
      while ( posList )
      {
        POSITION posDrop = posList;
        STrigger& oSTrigger = pCListRegTriggers -> GetNext ( posList );
        if ( oSTrigger.hWnd != hWnd )
          continue;
        if ( (oSTrigger.nType&nTriggerTypeMask) != oSTrigger.nType )
          continue;
        pCListRegTriggers->RemoveAt ( posDrop );
        nTriggers++;
      }
      // Drop trigger if no registrations exist
      if ( pCListRegTriggers->IsEmpty() )
      {
        delete pCListRegTriggers;
        pCMapTriggers-> RemoveKey ( uiKey );
      }
    }
    return nTriggers;
}
CMapTriggers*
P2PmsgHeap_DropTriggers ( CMapTriggers *pCMapTriggers )
{
    if ( pCMapTriggers == nullptr )
      return pCMapTriggers;
    VBLaddr uiKey = 0;
    CListRegTriggers *pCListRegTriggers = nullptr;
    POSITION pos = pCMapTriggers->GetStartPosition();
    while ( pos )
    {
      pCMapTriggers -> GetNextAssoc ( pos, uiKey, pCListRegTriggers ); 
      delete pCListRegTriggers;
    }
    return pCMapTriggers;
}
