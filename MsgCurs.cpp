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
#include "MsgAttr.h"
#include "P2Pmsg_Ext.h"
#include "Msgexception.h"
#include "P2PmsgVBLock.h"
#include "MsgVBHeap.h"
#include "MsgCurs.h"
#include <cwctype>                     // std::towlower / towupper (GotoKey_Init)
#include <vector>                      // GotoFilter_Build


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
//  P2PmsgCurs'or
//  NOTES: Navigates P2Pmsg data structures and generates data
//         exposure objects

//
//  Constructors and destructor
P3PmsgCurs::P3PmsgCurs ( )
{
}
P3PmsgCurs::P3PmsgCurs ( const P3PmsgCurs& rhs )
{
    *this = rhs;
}
P3PmsgCurs::P3PmsgCurs ( P3PmsgItem& rhs )
{
    m_pItemParent  =  new P3PmsgItem ( );
  (*m_pItemParent) = rhs.r_Object();
    m_pP3PmsgDesc  = &m_pItemParent -> r_Desc();
    Goto ( 0 );
}
P3PmsgCurs::P3PmsgCurs ( P3PmsgAttr& rhs )
{
    m_pP3PmsgAttr = &rhs;
    Goto ( 0 );
}
P3PmsgCurs::P3PmsgCurs ( P3PmsgDesc& rhs )
{
    m_pP3PmsgDesc = &rhs;
    Goto ( 0 );
}
P3PmsgCurs::~P3PmsgCurs ( )
{
    // Garbage collection
    if ( m_pItemParent )
      delete m_pItemParent;
}
//
//  References on hVBList held by this cursor
//  NOTES: ALL THREE by-value members, and not just the one m_pP3PmsgFoN is
//         pointing at. Goto connects whichever matches the item it landed on
//         and leaves the other two exactly as the previous Goto left them, so a
//         cursor that has walked past a list and then a field is holding this
//         heap TWICE. Nobody had counted, which is why the caller subtracting
//         one per cursor would have been wrong in the dangerous direction.
//       : m_pItemParent as well -- the P3PmsgItem constructor fills it by
//         assigning the item's object, which AddRefs, and the destructor above
//         frees it.
//       : m_pP3PmsgAttr and m_pP3PmsgDesc are the collection that owns this
//         cursor and are NOT followed; m_pP3PmsgFoN only aliases one of the
//         three members already counted. Refer P3PmsgField::HeapHolders.
int
P3PmsgCurs::HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept
{
    if ( hVBList == 0 )
      return 0;

    int nHolders = m_oP3PmsgField.HeapHolders ( hVBList )
                 + m_oP3PmsgList .HeapHolders ( hVBList )
                 + m_oP3PmsgVect .HeapHolders ( hVBList );
    if ( m_pItemParent != nullptr )
      nHolders += m_pItemParent -> HeapHolders ( hVBList );
    return nHolders;
}
void
P3PmsgCurs::RecycleThis ( )
{
    if ( m_pItemParent )
      delete m_pItemParent;
    m_pItemParent = nullptr;
    m_pP3PmsgAttr = nullptr;
    m_pP3PmsgDesc = nullptr;
    m_pP3PmsgFoN  = nullptr;
    m_nItem       = 0;
}
//
//  Operators
P3PmsgCurs&
P3PmsgCurs::operator = ( const P3PmsgCurs& rhs )
{
    if ( &rhs == this )
      return *this;
    RecycleThis ( );
    m_oP3PmsgField = rhs.m_oP3PmsgField;
    m_oP3PmsgList  = rhs.m_oP3PmsgList;
    m_oP3PmsgVect  = rhs.m_oP3PmsgVect;
    return *this;
}
P3PmsgCurs&
P3PmsgCurs::operator = ( const P3PmsgItem& rhs )
{
    if ( &rhs.r_Desc() == m_pP3PmsgDesc )
      return *this;
    RecycleThis ( );
    m_pItemParent =  new P3PmsgItem ();
   *m_pItemParent = rhs.r_Object();
    m_pP3PmsgDesc = &m_pItemParent -> r_Desc();
    Goto(0);
    return *this;
}
P3PmsgCurs&
P3PmsgCurs::operator -- ( )
{
    if ( m_nItem < 0 )
      EVERR->MODULE
           ->Message("Attempted underflow")
           ->Throw  ( );
    VBLelem nItems = GetCount();
    if ( m_nItem == 0 || nItems == 0 )
    {
      m_nItem      = -1;
      m_pP3PmsgFoN =  0;               // Nothing active
      return *this;
    }
    if ( m_nItem > nItems )
      m_nItem = nItems;
    Goto ( m_nItem - 1 );
    return *this;
}
P3PmsgCurs&
P3PmsgCurs::operator ++ ( )
{
    int nItems = GetCount();
    if ( m_nItem >= nItems )
      EVERR->MODULE
           ->Message("Attempted overflow")
           ->Throw  ( );
    if ( m_nItem >= nItems-1 || nItems == 0 )
    {
      m_nItem      = INT_MAX-1;
      m_pP3PmsgFoN = 0;
      return *this;                    // Nothing active
    } 
    Goto ( m_nItem + 1 );
    return *this;
}
P3PmsgCurs
P3PmsgCurs::operator ++ ( int )
{
    ++(*this);
    return *this;
}

P3PmsgCurs::operator P3PmsgName& ( )
{
    return r_name();
}

P3PmsgCurs::operator P3PmsgField& ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgFoN->r_Object().IsField() )
      EVERR->MODULE
           ->Message("Not a P2PmsgField type object" )
           ->Throw  ( );
    return *m_pP3PmsgFoN;
}
P3PmsgCurs::operator P3PmsgList& ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgFoN->r_Object().IsList() )
      EVERR->MODULE
           ->Message("Not a P2PmsgList type object" )
           ->Throw  ( );
    return dynamic_cast<P3PmsgList&>(*m_pP3PmsgFoN);
}
P3PmsgCurs::operator P3PmsgVect& ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgFoN->r_Object().IsVect() )
      EVERR->MODULE
           ->Message("Not a P2PmsgVect type object" )
           ->Throw  ( );
    return dynamic_cast<P3PmsgVect&>(*m_pP3PmsgFoN);
}

P3PmsgCurs::operator P3PmsgData& ( )
{
    return r_data();
}

///////////////////////////////////////////////////////////////////////
//  Operations

void
P3PmsgCurs::Delete ( )
{
    // Locals
    //UINT        aItem = 0;
    //VBLockNode *pNode = 0;
    //if ( m_pP3PmsgNode )
    //  pNode = P3PmsgNode__GetVBLockNode(m_pP3PmsgNode); //TODO:LJM deprecated m_pP3PmsgNode -> GetVBLockNode();
    VBLockAttr *pAttr = 0;
    if ( m_pP3PmsgAttr )
      pAttr = VBLock_pAttr ( (VBLock *)m_pP3PmsgAttr->r_Object().GetVBLock() );
      //TODO:LJM deprecated above pAttr = VBLock_pAttr ( P3PmsgAttr__VBLock(m_pP3PmsgAttr) );
    VBLockDesc *pDesc = 0;
    if ( m_pP3PmsgDesc )
      pDesc = VBLock_pDesc ( (VBLock *)m_pP3PmsgDesc->r_Object().GetVBLock() );

    // Empty P2PmsgList of all items
    if ( IsList() )
    {
      r_list().Truncate ( );
      //if ( m_pP3PmsgNode )
      //  P2PmsgNode_UnLinkItem ( m_pP3PmsgNode, pNode
      //                        , r_list().r_Object().GetVBLocknn() );
      if ( m_pP3PmsgDesc )
        P2PmsgDesc_UnLinkItem ( (P3PmsgObject*)&m_pP3PmsgDesc->r_Object(), pDesc
                              , r_list().r_Object().GetVBLocknn() );
      else if ( m_pP3PmsgAttr )
        P2PmsgAttr_UnLinkItem ( (P3PmsgObject*)&m_pP3PmsgAttr->r_Object(), pAttr
                              , r_list().r_Object().GetVBLocknn() );
      else { ASSERT(0); }
      r_list().Drop ( );
      m_pP3PmsgFoN = 0;
      m_nItem      = 0;  //TODO:LJM Added 08-08-2016
    }
    // Empty P2PmsgVect of all items
    else if ( IsVect() )
    {
      r_vect().Truncate ( );
      //if ( m_pP3PmsgNode )
      //  P2PmsgNode_UnLinkItem ( m_pP3PmsgNode, pNode
      //                        , r_vect().r_Object().GetVBLocknn() );
      if ( m_pP3PmsgDesc )
        P2PmsgDesc_UnLinkItem ( (P3PmsgObject*)&m_pP3PmsgDesc->r_Object(), pDesc
                              , r_vect().r_Object().GetVBLocknn() );
      else if ( m_pP3PmsgAttr )
        P2PmsgAttr_UnLinkItem ( (P3PmsgObject*)&m_pP3PmsgAttr->r_Object(), pAttr
                              , r_vect().r_Object().GetVBLocknn() );
      else { ASSERT(FALSE); }
      r_vect().Drop ( );
      m_pP3PmsgFoN = 0;
      m_nItem      = 0;  //TODO:LJM Added 08-08-2016
    }
    // Empty P2PmsgItem all contents
    else if ( IsItem() )
    {
      r_item().r_Desc().Truncate();
      r_item().r_Attr().Truncate();
      if ( m_pP3PmsgDesc )
        P2PmsgDesc_UnLinkItem ( (P3PmsgObject*)&m_pP3PmsgDesc->r_Object(), pDesc
                              , r_item().OBJ__VBLocknn );
      else if ( m_pP3PmsgAttr )
        P2PmsgAttr_UnLinkItem ( (P3PmsgObject*)&m_pP3PmsgAttr->r_Object(), pAttr
                              , r_item().OBJ__VBLocknn );
      else { ASSERT(FALSE); }
      r_item().Drop ( );
      m_pP3PmsgFoN = 0;
      m_nItem      = 0;  //TODO:LJM Added 08-08-2016
      m_oP3PmsgField.Nullify();
    }
    // Should not happen
    else { ASSERT(FALSE); }
}

//
//  M3: Goto's sibling walk, ruling siblings OUT without a handle
//  NOTES: Goto compared every sibling by connecting a full handle to it
//         (P3PmsgField::Connect), widening its name (c_name(): on Linux into a
//         thread-local std::wstring ring) and calling _wcsicmp (wcscasecmp +
//         towlower per character on Linux). Profiled, that was ~70% of a
//         512-sibling lookup on Linux (MSCS performance log, F-P1-1).
//       : THIS CAN ONLY SAY "DIFFERENT". A sibling it proves different is
//         skipped without being connected; anything else -- a likely match, or
//         a name it cannot judge -- goes through the original Connect and
//         c_wcsicmp, which alone decides a hit. So a hit, and which sibling is
//         hit (first match wins), is decided exactly as before.
//       : When it may judge. The key is 0..63 units of 7-bit ASCII without
//         NUL, the stored name is unchained, and the current locale folds 'I'
//         and 'i' into each other (a Turkic locale does not, and _wcsicmp
//         follows the locale). Then, up to the first unit that is NUL or
//         above 0x7F, both strings are plain ASCII on both platforms -- the
//         Linux widen maps those units one to one -- and _wcsicmp's verdict on
//         them is A-Z/a-z folding. The scan stops at the first such unit and
//         answers "cannot judge", because a non-ASCII unit may fold to ASCII
//         (U+212A KELVIN SIGN lowers to 'k') and an embedded NUL ends
//         _wcsicmp's string early. The terminator at nBlobUsed is guaranteed:
//         P2PmsgHeap_pNameChk refuses an image name without one.
//       : The name is bounded by P2PmsgHeap_pNameChk, the same checks c_name()
//         applies, so a malformed sibling is refused exactly as before.
namespace
{
struct GotoKey
{
    P2PWCHAR wcFold[64];  // the key, ASCII lower-cased
    size_t   nUnits = 0;
    bool     bJudge = false;
};

inline P2PWCHAR
GotoKey_FoldAscii ( unsigned c ) noexcept
{
    return (P2PWCHAR)( ( c >= 'A' && c <= 'Z' ) ? c + ('a' - 'A') : c );
}

void
GotoKey_Init ( GotoKey &oKey, LPCWSTR lpszKey )
{
    oKey.bJudge = false;
    if ( std::towlower ( L'I' ) != L'i' || std::towupper ( L'i' ) != L'I' )
      return;                          // a locale that folds ASCII its own way
    size_t n = 0;
    for ( ; lpszKey[n] != 0; ++n )
    {
      if ( n >= 63 || (unsigned)lpszKey[n] > 0x7F )
        return;
      oKey.wcFold[n] = GotoKey_FoldAscii ( (unsigned)lpszKey[n] );
    }
    oKey.nUnits = n;
    oKey.bJudge = true;
}

//  true only when the stored name certainly differs from the key under _wcsicmp
bool
GotoKey_Differs ( const GotoKey &oKey, const VBLockName *pName ) noexcept
{
    if ( VBLockName_IsChainedInl ( pName ) )
      return false;                    // the body is in another block
    const size_t n = pName->u.vBlob08.nBlobUsed;
    //  Byte-wise: the name sits in a packed struct, so its units need not be
    //  2-aligned (C4366). memcpy of one unit compiles to a plain load.
    const unsigned char *pb = reinterpret_cast<const unsigned char*>(pName)
                            + offsetof ( VBLockName, u.vBlob08.cBlob );
    for ( size_t i = 0; i < n; ++i )
    {
      P2PWCHAR wc;
      std::memcpy ( &wc, pb + i * sizeof(P2PWCHAR), sizeof(wc) );
      const unsigned c = wc;
      if ( c == 0 || c > 0x7F )
        return false;                  // cannot judge from here on
      if ( i >= oKey.nUnits )
        return true;                   // the key has ended, the name has not
      if ( GotoKey_FoldAscii ( c ) != oKey.wcFold[i] )
        return true;
    }
    return n != oKey.nUnits;           // the name has ended; does the key?
}
}

//
//  Whether the item a cursor last landed on is still one of ITS collection's
//  children, as the cursor left it. The quick tests in Goto(name) and
//  Goto(int) believe the cursor's current item without looking at the heap;
//  this is what they now ask first.
//  NOTES: THE DEFECT. A cursor belongs to one P3PmsgDesc/P3PmsgAttr object,
//         and two P3PmsgField objects over the same node have two of them. A
//         child deleted through the OTHER one is freed under this cursor,
//         which still stands on it: a free clears the block's type bits and
//         puts it at the head of the free list, and nothing tells the cursor.
//         The quick test then read the freed block's name, the name checks
//         threw, and the walk that would have re-positioned the cursor never
//         ran -- so EVERY lookup through that object threw, for any name, until
//         the block happened to be reused by a well-formed item. The C API
//         turned the throw into a null: msgcore_field_child answered "no such
//         child" for one that exists (3.5.2 and 3.5.3 alike). Found by the
//         p2p_performance M5 review, 2026-11-22; MsgcoreCApiSuite
//         "field_child: a child deleted through another handle answers null".
//       : THE CHECK, all of it read from the block, none of it trusted from the
//         cursor: the block lies inside the heap, is allocated, is an item, has
//         the size the cursor connected, is the kind (field, list, vect) the
//         cursor connected, and names THIS collection as its parent (aParent is
//         the owner's aDescn or aExtra: P2PmsgDesc_LinkinItem,
//         P2PmsgAttr_LinkinItem). A freed block fails "allocated"; one reused
//         by an item under another parent fails "parent". Any failure only
//         sends Goto on to its walk, which is the answer it gave before the
//         quick test existed -- so a false "not here" costs a walk, never a
//         wrong answer.
//       : NOT A GENERATION. A block freed and taken again, at once, by an item
//         of the same size under the SAME parent passes, and then the name
//         compare decides: the same name means it IS that collection's child
//         of that name now. What the cursor's connected objects below it still
//         cache (an old item's descendant cursor) is not re-read here, as it
//         never was.
//       : SYSTEM heaps address raw memory and carry no bound (P2PmsgHeapExtent
//         Open), so a block there cannot be read safely once freed. They keep
//         the quick test as it was: no worse than before, and no better.
//
namespace
{
bool
GotoQuick_StillHere ( const P3PmsgObject &oColl, const P3PmsgField *pFoN, int nKind
                    , bool bVouch ) noexcept
{
    const P3PmsgObject &oItem = pFoN->r_Object ( );
    const P2PmsgHANDLE  hVBList = oItem.m_hVBList;
    if ( hVBList == 0 || hVBList != oColl.m_hVBList ||
         oItem.m_aVBLock == 0 || oColl.m_aVBLock == 0 || oItem.m_nVBLockSize == 0 )
      return false;
    P2PmsgHeapExtent oExtent;
    P2PmsgHeap_GetExtent ( hVBList, &oExtent );
    if ( oExtent.uKind == P2PmsgHeapExtent::Open )
      return !bVouch;                  // SYSTEM heap: as before (see NOTES)
    if ( oExtent.uKind != P2PmsgHeapExtent::Image )
      return false;
    const VBLock *pVBLock = reinterpret_cast<const VBLock *>( oExtent.aImage + oItem.m_aVBLock );
    if ( !P2PmsgHeapExtent_IsSpan ( oExtent, pVBLock, oItem.m_nVBLockSize ) )
      return false;
    //  The bit tests VBLock_IsAlloc, VBLock_IsItem and VBLockItem_IsField/List/
    //  Vect make, inline: this runs on every quick-test hit (measured +15 ns
    //  per hit with them out of line). VBLock_Hdr_u_SizeNN and
    //  VBLockItem_GetParent throw only for an addressing field outside
    //  Addr08..Addr64, and VBLock_AddrMask is two bits, all four of them valid,
    //  so whatever byte a stale address lands on they answer and noexcept holds.
    const UCHAR uDefs = pVBLock->oHdr.uVBLockDefs;
    if ( ( uDefs & VBLock_Alloc ) != VBLock_Alloc ||
         ( uDefs & VBLock_TypeMask ) != VBLock_Item ||
         VBLock_Hdr_u_SizeNN ( pVBLock ) != oItem.m_nVBLockSize )
      return false;
    VBLockItem *pItem = VBLock_pItem ( pVBLock );
    const UCHAR uKind = nKind == 1 ? VBLock_List : nKind == 2 ? VBLock_Vect : VBLock_Field;
    return ( pItem->uItemType & VBLock_TypeMask ) == uKind &&
           VBLockItem_GetParent ( uDefs, pItem ) == oColl.m_aVBLock;
}

//
//  B2: files a name filter for a collection Goto has just walked end to end
//  without a hit (refer P2PmsgHeap_BumpShape, MsgVBHeap.cpp).
//  NOTES: The same chain, bounds and name checks as the walk that preceded it,
//         and nothing has written to the heap since, so it cannot throw where
//         that walk did not. A child the filter cannot judge (chained, NUL or
//         non-ASCII units) means no filter: the walk stays the only answer.
void
GotoFilter_Build ( P2PmsgHANDLE hVBList, VBLaddr aColl, VBLock *pColl, UCHAR uVBLockHeap
                 , const P2PmsgHeapExtent &oExtent, VBLsize nSizeofNameMin
                 , VBLsize nMaxWalk, bool bDesc )
{
    std::vector<UINT32> aHashes;
    const UCHAR uDefs = pColl->oHdr.uVBLockDefs;
    VBLaddr aItem = bDesc ? VBLockDesc_GetFirst ( uDefs, VBLock_pDesc(pColl) )
                          : VBLockAttr_GetFirst ( uDefs, VBLock_pAttr(pColl) );
    VBLsize nWalked = 0;
    while ( aItem )
    {
      if ( nMaxWalk && ++nWalked > nMaxWalk )
        return;                        // the walk would have thrown first
      VBLock *pVBLock = (VBLock *)P2PmsgHeap_Addr2Phys ( hVBList, aItem );
      UINT32 nHash = 0;
      if ( !P2PmsgNameFilter_HashName ( P2PmsgHeap_pNameChkIn ( hVBList, uVBLockHeap
                                                              , oExtent, nSizeofNameMin
                                                              , pVBLock )
                                      , &nHash ) )
        return;
      aHashes.push_back ( nHash );
      aItem = VBLockItem_GetNext ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem ( pVBLock ) );
    }
    P2PmsgHeap_NameFilterBuild ( hVBList, aColl, aHashes.data(), aHashes.size() );
}

//  B2: the end of a whole miss walk over a collection past
//  P2PmsgNameFilter_MinChildren that had no current filter: file one.
//  NOTES: Out of line, and it works the collection's address and block out
//         again rather than having Goto carry them across its walk (Goto
//         carries one flag). Carried, they cost every hit: MSVC kept them live
//         through the loop and lookup_alt read +5% at 8, 64 and 512 siblings,
//         per sibling, under a padding control (MSCS p2p_performance F-P4-12).
//         Nothing writes to the heap during the walk, so the "no filter"
//         answer Goto had before it still holds here.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void
GotoFilter_AfterMiss ( const P3PmsgObject &oObj, bool bDesc, P2PmsgHANDLE hVBList
                     , UCHAR uVBLockHeap
                     , const P2PmsgHeapExtent &oExtent, VBLsize nSizeofNameMin
                     , VBLsize nMaxWalk )
{
    const VBLaddr aColl = oObj.m_aVBLock;
    VBLock *const pColl = (VBLock *)oObj.GetVBLock();
    if ( aColl == 0 || pColl == nullptr )
      return;
    GotoFilter_Build ( hVBList, aColl, pColl, uVBLockHeap, oExtent
                     , nSizeofNameMin, nMaxWalk, bDesc );
}

//  The cursor's side of the question: which collection, which item, which kind.
bool
GotoQuick_Curs ( const P3PmsgObject *pColl, const P3PmsgField *pFoN, int nKind
               , bool bVouch ) noexcept
{
    return pColl != nullptr && pFoN != nullptr &&
           GotoQuick_StillHere ( *pColl, pFoN, nKind, bVouch );
}
}

//  Asked from inside a P3PmsgCurs member, where the protected members are in
//  reach. A macro and not a member function, because every member of this
//  class is exported (whole-class Msgcore_EXT) and the mangled C++ surface is
//  frozen: tools/ci/check_exports.ps1. 3.6.0 adds IsAt to it and nothing else.
#define CURS_STILLHERE(bVouch)                                                  \
    GotoQuick_Curs ( m_pP3PmsgDesc ? &m_pP3PmsgDesc->m_oObject                  \
                   : m_pP3PmsgAttr ? &m_pP3PmsgAttr->m_oObject : nullptr        \
                   , m_pP3PmsgFoN                                               \
                   , m_pP3PmsgFoN == &m_oP3PmsgList ? 1                         \
                   : m_pP3PmsgFoN == &m_oP3PmsgVect ? 2 : 0                     \
                   , bVouch )

//
//  True when the cursor ALREADY stands on a live item of this name. Never
//  walks and never moves the cursor, so a false answer means only "not here",
//  not "absent": ask Goto, or the owner's Exists, for that.
//  NOTES: For a caller that would otherwise ask a fresh walk first (Exists),
//         then this cursor (SelectItem) -- MsgFieldRef's read, p2p_performance
//         M5. When this answers true, Goto(name) answers true from its quick
//         test with the cursor where it is. Throws only where that compare
//         would: a name the checks refuse in a block GotoQuick_StillHere accepts.
//       : FALSE ON A SYSTEM HEAP. GotoQuick_StillHere answers true there because it
//         cannot look (raw memory, no bound), which keeps Goto's quick test as
//         it was. IsAt is asked INSTEAD of a fresh walk, so it may not answer
//         for what it cannot see: there it says "not here" and the caller
//         walks, as before.
bool
P3PmsgCurs::IsAt ( LPCWSTR lpszItemName )
{
    if ( !CURS_STILLHERE ( true ) )
      return false;
    const p2p_wkey oItemName ( lpszItemName ? lpszItemName : L"" );
    return m_pP3PmsgFoN->r_name().c_wcsicmp ( oItemName ) == 0;
}

P3PmsgCurs&
P3PmsgCurs::Seek ( )
{
    Goto ( 0 );
    return *this;
}
bool
P3PmsgCurs::Goto ( LPCWSTR lpszItemName )
{
    //  Hold the key still for the whole walk. Every sibling compared below spends
    //  a slot of the widening ring (c_wcsicmp -> c_name -> p2p_wstr_from_store),
    //  so a caller who passed some other item's c_name() would have its key
    //  rewritten from under it after ~16 siblings — and the rewritten key then
    //  compares EQUAL to the sibling that overwrote it, stopping this scan on the
    //  wrong item. Snapshotting in the callee is what makes that unreachable
    //  however Goto is called; see p2p_wkey (Platform/p2pstr.h).
    //      : A P3PmsgName COPY IS NOT A SNAPSHOT, which is what stood here and is
    //        why three suites failed. It copies the name out of the caller's
    //        storage, but c_name() hands back a RING SLOT (P2Pmsg.cpp:1131 ->
    //        p2p_wstr_from_store), so the key lands back inside the very ring the
    //        snapshot exists to escape -- and it does so even for a key that
    //        arrived SAFE, e.g. the literal every Exists() passes. That made the
    //        wrong answer reachable without a careless caller: any container past
    //        the ring's 16 slots. Measured: 24 siblings, Goto returns true on the
    //        wrong item, and DeclareItem's `if (!Goto(name))` then drops the
    //        append with bUpdate FALSE -- a silent short write.
    //      : NULL is normalised here rather than passed through, because
    //        c_wcsicmp() feeds it straight to _wcsicmp (P2Pmsg.cpp:2223). The
    //        P3PmsgName route tolerated a null key by yielding L""; keep that.
    const p2p_wkey oItemName ( lpszItemName ? lpszItemName : L"" );
    LPCWSTR lpszStackName = oItemName;
    //  GotoQuick_StillHere first: the cursor's item may have been freed under it through
    //  another object (refer GotoQuick_StillHere).
    if ( m_pP3PmsgFoN                                         &&
         CURS_STILLHERE ( false )                             &&
         m_pP3PmsgFoN->r_name().c_wcsicmp(lpszStackName) == 0    )
      return true;
    P2PmsgHANDLE hVBList = 0;
    VBLock      *pVBLock = 0;
    VBLaddr      aItem   = 0;
    if ( m_pP3PmsgDesc )
    {
      hVBList = m_pP3PmsgDesc -> m_oObject.m_hVBList; //TODO:LJM 2013/08/30 was GetField() -> OBJ__hVBList;
      //TODO:LJM deprecated below pVBLock = P3PmsgDesc__VBLock(m_pP3PmsgDesc);  //TODO:LJM deprecated m_pP3PmsgDesc -> GetVBLock();
      pVBLock = (VBLock *)m_pP3PmsgDesc->r_Object().GetVBLock();
      if ( pVBLock == NULL )
        return false;                  // Empty descendants
      //ASSERT(VBLock_IsDesc(pVBLock));
      aItem   = VBLockDesc_GetFirst ( pVBLock->oHdr.uVBLockDefs 
                                    , VBLock_pDesc(pVBLock), &m_nItem );
    }
    else if ( m_pP3PmsgAttr )
    {
      hVBList = m_pP3PmsgAttr -> m_oObject.m_hVBList; //TODO:LJM 2013/08/30 was GetField() -> OBJ__hVBList;
      //TODO:LJM deprecated below pVBLock = P3PmsgAttr__VBLock(m_pP3PmsgAttr);  //TODO:LJM deprecated m_pP3PmsgAttr -> GetVBLock();
      pVBLock = (VBLock *)m_pP3PmsgAttr->r_Object().GetVBLock();
      if ( pVBLock == NULL )
        return false;                  // Empty attributes
      //ASSERT(VBLock_IsAttr(pVBLock));
      aItem   = VBLockAttr_GetFirst ( pVBLock->oHdr.uVBLockDefs 
                                    , VBLock_pAttr(pVBLock), &m_nItem );
    }
    else { ASSERT(FALSE); return false; }

    //  M3: rule siblings out without connecting them (GotoKey, above). Only
    //  with a heap: a container with none addresses raw memory, and the
    //  original path below is all it has ever had.
    GotoKey oKey;
    if ( hVBList )
      GotoKey_Init ( oKey, lpszStackName );
    const UCHAR uVBLockHeap = oKey.bJudge ? P2PmsgHeap_Addrnn ( hVBList ) : 0;
    //  M2b: the bounds every sibling's name is checked against, read once for
    //  the walk. Nothing below writes to the heap -- the loop only connects
    //  cursors and compares names -- so the extent cannot go stale under it.
    P2PmsgHeapExtent oExtent {};
    VBLsize nSizeofNameMin = 0;
    if ( oKey.bJudge )
    {
      P2PmsgHeap_GetExtent ( hVBList, &oExtent );
      nSizeofNameMin = VBLockName_Sizeof_Min ( uVBLockHeap );
    }

    //  B2: the "definitely new" name filter (refer P2PmsgHeap_BumpShape). A
    //  current filter without the key's hash says no child has this name, and
    //  the walk is skipped. No current filter: a miss walk below files one
    //  (GotoFilter_AfterMiss, which works its inputs out again: nothing of
    //  this block may stay live across the walk, refer its NOTES). Only for
    //  containers past P2PmsgNameFilter_MinChildren, where a walk is worth a
    //  map lookup, and only for keys the M3 test can judge.
#ifdef P2P_NAME_FILTER_CHECK
    bool bFilterSkipped = false;
#endif
    bool bFilterBuild = false;         // one flag: the only B2 state the walk carries
    //  A heap no cursor has filed a filter on (most heaps, and every heap whose
    //  lookups all hit) answers None for every collection: no count, no hash,
    //  no Ask. The size test moves to after the walk, where m_nItem already
    //  holds the number of children it passed (F-P4-12: lookup_alt +1.4%
    //  instructions before this).
    if ( oKey.bJudge && !P2PmsgHeap_HasNameFilters ( hVBList ) )
      bFilterBuild = true;
    else if ( oKey.bJudge )
    {
      //  The count straight from the collection's block: GetCount() would find
      //  that block again through two objects, on every walk of every size.
      const VBLaddr aColl = m_pP3PmsgDesc ? m_pP3PmsgDesc->m_oObject.m_aVBLock
                                          : m_pP3PmsgAttr->m_oObject.m_aVBLock;
      const VBLelem nChildren = m_pP3PmsgDesc ? VBLockDesc_GetItems ( pVBLock->oHdr.uVBLockDefs
                                                                    , VBLock_pDesc(pVBLock) )
                                              : VBLockAttr_GetItems ( pVBLock->oHdr.uVBLockDefs
                                                                    , VBLock_pAttr(pVBLock) );
      if ( nChildren >= (VBLelem)P2PmsgNameFilter_MinChildren && aColl != 0 )
      {
        switch ( P2PmsgHeap_NameFilterAsk ( hVBList, aColl
                                          , P2PmsgNameFilter_HashFolded ( oKey.wcFold, oKey.nUnits ) ) )
        {
        case P2PmsgNameFilter_Absent:
#ifdef P2P_NAME_FILTER_CHECK
          bFilterSkipped = true;       // walk anyway, and throw on a hit
          break;
#else
          m_pP3PmsgFoN = 0;
          m_nItem      = 0;
          return false;
#endif
        case P2PmsgNameFilter_None:
          bFilterBuild = true;         // if this walk misses: refer the end
          break;
        case P2PmsgNameFilter_Maybe:
          break;
        }
      }
    }

    //  BOUND THE SIBLING WALK (F-P6-6). aItem advances along aNext links which,
    //  for a message built from a received image, come straight off the wire. A
    //  forged chain that points back on itself spins here forever -- the
    //  confirmed denial of service, because GetSource() on an inbound
    //  application message reaches this loop (SelectObjectRecurse -> Goto) and
    //  P2PeerCon::GateAppMsgInbound reads the source of every frame before it is
    //  dispatched.
    //
    //  THE CEILING IS THE HEAP'S BYTE EXTENT, and the choice matters. A chain
    //  cannot have more items than the heap has bytes -- each GetNext must reach
    //  a distinct in-bounds address -- so nSizeofAlloc is an upper bound no
    //  legitimate acyclic walk can reach, while a cycle reaches it for certain.
    //  It is deliberately NOT the tighter block count (nSizeofAlloc / header):
    //  that count is a true ceiling on the free-list walk, but applied here it
    //  FALSE-POSITIVED on small live heaps (the P2Pevent a refusal itself builds
    //  is walked during Display, and its block count came out as 1), throwing
    //  from inside the throw. A byte extent cannot be reached by a valid chain.
    //
    //  Only an IMAGE heap (IOMAGE / BSTRio, i.e. one built from bytes) is
    //  bounded. A SYSTEM heap is process-built, carries no wire image, reports
    //  no extent, and keeps the unbounded path it has always had -- nothing a
    //  stranger sent ever lands on one. A tripped bound is corruption: it is
    //  thrown, which the receive path turns into a dropped connection.
    P2PmsgHeapExtent oExtentWalk {};
    if ( hVBList )
      P2PmsgHeap_GetExtent ( hVBList, &oExtentWalk );
    const VBLsize nMaxWalk = oExtentWalk.uKind == P2PmsgHeapExtent::Image
                           ? oExtentWalk.nSizeofAlloc : 0;
    VBLsize       nWalked  = 0;
    m_pP3PmsgFoN = 0;
    while ( aItem )
    {
      if ( nMaxWalk && ++nWalked > nMaxWalk )
        EVERR -> Module ( __FUNCTION__ )
              -> Message ( "Cyclic sibling chain in message image "
                           "(walked %u past %u-byte heap)"
                         , (unsigned)nWalked, (unsigned)nMaxWalk )
              -> Throw();
      ASSERT(GetCount()>0);
      //VBLock *pVBLock = 0;
      if ( m_pP3PmsgDesc )
        pVBLock = (VBLock *)m_pP3PmsgDesc -> r_Object().Msg2Phys(aItem); //TODO:LJM 2013/08/30 was GetField() -> r_Object().Msg2Phys ( aItem );
      else if ( m_pP3PmsgAttr )
        pVBLock = (VBLock *)m_pP3PmsgAttr -> r_Object().Msg2Phys(aItem); //TODO:LJM 2013/08/30 was GetField() -> r_Object().Msg2Phys ( aItem );
      else { ASSERT(0); break; }
      //ASSERT(VBLock_IsAlloc(pVBLock));
      //ASSERT(VBLock_IsLinked(pVBLock));
      VBLockItem *pItem   = VBLock_pItem  ( pVBLock );
      if ( oKey.bJudge &&
           GotoKey_Differs ( oKey, P2PmsgHeap_pNameChkIn ( hVBList, uVBLockHeap
                                                         , oExtent, nSizeofNameMin
                                                         , pVBLock ) ) )
      {
#ifdef _DEBUG
        //  Every skip, checked against the compare it replaces. A throw, not
        //  an ASSERT (tools/ci/check_asserts.ps1): a Debug suite then fails
        //  the case that reached it, by name, instead of printing and going on.
        P3PmsgField oCheck;
        oCheck.Connect ( hVBList, aItem, VBLock_Hdr_u_SizeNN ( pVBLock ) );
        if ( oCheck.r_name().c_wcsicmp(lpszStackName) == 0 )
          EVERR->MODULE
               ->Message("Goto skipped a sibling the compare matches (M3 fast path)")
               ->Throw();
#endif
        aItem = VBLockItem_GetNext ( pVBLock->oHdr.uVBLockDefs, pItem, &m_nItem );
        continue;
      }
      UINT    nVBLockSize = VBLock_Hdr_u_SizeNN ( pVBLock );
      //UINT    nVBLockuos   = VBLockItem_uos( pVBLock->oHdr.uVBLock, pItem );
      if ( VBLockItem_IsList(pItem) )
      { // Used by P2Pevents
        //UINT aField     =  aItem + nVBLockuos;
        //UINT nFieldSize =  nVBLockSize - nVBLockuos;
        m_pP3PmsgFoN    = &m_oP3PmsgList;
        m_oP3PmsgList.Connect ( hVBList, aItem, nVBLockSize );
      }
      else if ( VBLockItem_IsVect(pItem) )
      {
        //UINT aField     =  aItem + nVBLockuos;
        //UINT nFieldSize =  nVBLockSize - nVBLockuos;
        //  (removed stray ASSERT(0) placeholder: the vect branch is implemented below,
        //   mirroring the list branch; reloading a vect by name legitimately reaches here)
        m_pP3PmsgFoN    = &m_oP3PmsgVect;
        m_oP3PmsgVect.Connect ( hVBList, aItem, nVBLockSize );
      }
      else if ( VBLockItem_IsField(pItem) )
      {
        //UINT aField     =  aItem + nVBLockuos;
        //UINT nFieldSize =  nVBLockSize - nVBLockuos;
        m_pP3PmsgFoN    = &m_oP3PmsgField;
        m_oP3PmsgField.Connect ( hVBList, aItem, nVBLockSize );
//m_oP3PmsgField.AssertValid();//TODO:LJM Debugging
      }
      else { ASSERT(0);}
      ASSERT(m_pP3PmsgFoN!=nullptr);
      //LPCWSTR lpszName = m_pP3PmsgFoN->c_name();
      if ( m_pP3PmsgFoN->r_name().c_wcsicmp(lpszStackName) == 0 )
      {
#ifdef P2P_NAME_FILTER_CHECK
        //  The filter said no child has this name, and here is one: a writer
        //  that adds a name without bumping the shape stamp. A throw, not an
        //  ASSERT (tools/ci/check_asserts.ps1); EVERR reports in whichever width
        //  stderr is in (MSCS p2p_performance F-P4-11a).
        if ( bFilterSkipped )
          EVERR->MODULE
               ->Message(L"NAME-FILTER-MISMATCH: the filter ruled out [%s], which is here"
                        , lpszStackName )
               ->Throw();
#endif
        return true;
      }
      aItem = VBLockItem_GetNext ( pVBLock->oHdr.uVBLockDefs, pItem, &m_nItem );
      m_pP3PmsgFoN = 0;
    }

    //  A whole miss walk over a container with no current filter: file one,
    //  in a second pass. Not hashed during the walk above, because most walks
    //  HIT, and a hit would throw the hashes away (measured: lookup_alt +30%
    //  at 64 and 512 siblings when it did). A miss is where the filter pays.
    //  m_nItem: one per child the walk passed (VBLockItem_GetNext counts), so
    //  after a whole miss it is the collection's count.
    if ( bFilterBuild && m_nItem >= (VBLelem)P2PmsgNameFilter_MinChildren )
      GotoFilter_AfterMiss ( m_pP3PmsgDesc ? m_pP3PmsgDesc->m_oObject : m_pP3PmsgAttr->m_oObject
                           , m_pP3PmsgDesc != nullptr, hVBList, uVBLockHeap, oExtent
                           , nSizeofNameMin, nMaxWalk );

    // Tidy up, and
    m_nItem = 0;
    return false;
}
bool
P3PmsgCurs::Goto ( int nItem )
{
    // To be sure, to be sure
    if ( m_nItem      == nItem   &&
         m_pP3PmsgFoN != nullptr &&
         CURS_STILLHERE ( false )   )  // refer GotoQuick_StillHere
      return true;
    m_pP3PmsgFoN  = 0;
    VBLaddr aItem = 0;

ASSERT(nItem>=0);
    // P2PmsgDesc Optimisation
    if ( m_pP3PmsgDesc )
    {
      VBLock *pVBLock4Desc = (VBLock *)m_pP3PmsgDesc->r_Object().GetVBLock();
      if ( pVBLock4Desc )
      {
        UINT cItem = VBLockDesc_GetItems ( pVBLock4Desc->oHdr.uVBLockDefs
                                         , VBLock_pDesc(pVBLock4Desc) );
        if ( nItem <= (int)cItem/2 )
          aItem = VBLockDesc_GetFirst( pVBLock4Desc->oHdr.uVBLockDefs 
                                     , VBLock_pDesc(pVBLock4Desc), &m_nItem );
        else
          aItem = VBLockDesc_GetLast ( pVBLock4Desc->oHdr.uVBLockDefs 
                                     , VBLock_pDesc(pVBLock4Desc), &m_nItem );
      }
    }

    // P2PmsgAttr Optimisation
    else if ( m_pP3PmsgAttr )
    {
      VBLock *pVBLock4Attr = (VBLock *)m_pP3PmsgAttr->r_Object().GetVBLock();
      if ( pVBLock4Attr )
      {
        VBLelem cItem = VBLockAttr_GetItems ( pVBLock4Attr->oHdr.uVBLockDefs
                                            , VBLock_pAttr(pVBLock4Attr) );
        if ( nItem <= (int)cItem/2 )
          aItem = VBLockAttr_GetFirst( pVBLock4Attr->oHdr.uVBLockDefs 
                                     , VBLock_pAttr(pVBLock4Attr), &m_nItem );
        else
          aItem = VBLockAttr_GetLast ( pVBLock4Attr->oHdr.uVBLockDefs 
                                     , VBLock_pAttr(pVBLock4Attr), &m_nItem );
      }
    }

    // Navigation
    //  BOUND THE SIBLING WALK (F-P6-6), the by-index twin of the by-name walk
    //  above and the same byte-extent ceiling -- refer that loop for why the
    //  extent and not the block count. AssertValid() iterates this with
    //  for(i=0; Goto(i); i++), so a forged aNext/aPrev cycle hangs it here.
    //  m_nItem can oscillate around nItem without ever meeting it on a cyclic
    //  chain, so the step counter, not the index, is what terminates.
    P2PmsgHANDLE hVBListWalk = m_pP3PmsgDesc ? m_pP3PmsgDesc->m_oObject.m_hVBList
                            : m_pP3PmsgAttr ? m_pP3PmsgAttr->m_oObject.m_hVBList
                            :                 0;
    P2PmsgHeapExtent oExtentWalk {};
    if ( hVBListWalk )
      P2PmsgHeap_GetExtent ( hVBListWalk, &oExtentWalk );
    const VBLsize nMaxWalk = oExtentWalk.uKind == P2PmsgHeapExtent::Image
                           ? oExtentWalk.nSizeofAlloc : 0;
    VBLsize       nWalked  = 0;
    VBLock     *pVBLock = 0;
    VBLockItem *pItem   = 0;
    while ( aItem )
    {
      if ( nMaxWalk && ++nWalked > nMaxWalk )
        EVERR -> Module ( __FUNCTION__ )
              -> Message ( "Cyclic sibling chain in message image "
                           "(walked %u past %u-byte heap)"
                         , (unsigned)nWalked, (unsigned)nMaxWalk )
              -> Throw();
      //if ( m_pP3PmsgNode )
      //  pVBLock  = (VBLock *)m_pP3PmsgNode->r_Object().Msg2Phys ( aItem );
      if ( m_pP3PmsgDesc )
        pVBLock  = (VBLock *)m_pP3PmsgDesc->r_Object().Msg2Phys ( aItem );
      else if ( m_pP3PmsgAttr )
        pVBLock  = (VBLock *)m_pP3PmsgAttr->r_Object().Msg2Phys ( aItem );
      ASSERT(VBLock_IsItem(pVBLock));
      ASSERT(VBLock_IsLinked(pVBLock));
      ASSERT(VBLock_IsAlloc(pVBLock));
      pItem    = VBLock_pItem  ( pVBLock );
      if ( m_nItem < nItem )
        aItem = VBLockItem_GetNext ( pVBLock->oHdr.uVBLockDefs, pItem, &m_nItem );
      else if ( m_nItem > nItem )
        aItem = VBLockItem_GetPrev ( pVBLock->oHdr.uVBLockDefs, pItem, &m_nItem );
      else
        break;
      pVBLock = 0;
    }
      
    // Located
    if ( aItem )
    {
      VBLsize      nVBLockSize = VBLock_Hdr_u_SizeNN ( pVBLock );
      P2PmsgHANDLE hVBList     = 0;
      //if ( m_pP3PmsgNode )
      //  hVBList = m_pP3PmsgNode -> OBJ__hVBList;
      if ( m_pP3PmsgDesc )
        hVBList = m_pP3PmsgDesc -> OBJ__hVBList;
      else if ( m_pP3PmsgAttr )
        hVBList = m_pP3PmsgAttr -> OBJ__hVBList;
      if ( VBLockItem_IsList(pItem) )
      {
        m_pP3PmsgFoN = &m_oP3PmsgList;
        m_oP3PmsgList.Connect ( hVBList, aItem, nVBLockSize );
      }
      else if ( VBLockItem_IsVect(pItem) )
      {
        m_pP3PmsgFoN = &m_oP3PmsgVect;
        m_oP3PmsgVect.Connect ( hVBList, aItem, nVBLockSize );
      }
      else if ( VBLockItem_IsField(pItem) )
      {
        m_pP3PmsgFoN = &m_oP3PmsgField;
        m_oP3PmsgField.Connect ( hVBList, aItem, nVBLockSize );
      }
      else ASSERT(0);
    }

    // Tidy up, and
    if ( m_pP3PmsgFoN != nullptr )
      return true;
    m_nItem = GetCount(); // TODO:LJM 21/04/2014 was -1
    return false;
}

//
//  Exposure
const P3PmsgObject&
P3PmsgCurs::r_Object ( ) const
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgFoN->r_Object();
}
LPCWSTR
P3PmsgCurs::c_wstr ( )
{
    return r_name().c_name();
}

P3PmsgData&
P3PmsgCurs::r_data ( ) const
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return *m_pP3PmsgFoN;
}

P3PmsgName&
P3PmsgCurs::r_name ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return *m_pP3PmsgFoN;
}

P3PmsgList&
P3PmsgCurs::r_list ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgFoN->r_Object().IsList() )
      EVERR->MODULE
           ->Message("Not a P2PmsgList type object" )
           ->Throw ( );
    return dynamic_cast<P3PmsgList&>(*m_pP3PmsgFoN);
}

P3PmsgVect&
P3PmsgCurs::r_vect ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgFoN->r_Object().IsVect() )
      EVERR->MODULE
           ->Message("Not a P2PmsgVect type object" )
           ->Throw ( );
    return dynamic_cast<P3PmsgVect&>(*m_pP3PmsgFoN);
}
P3PmsgItem&
P3PmsgCurs::r_item ( ) const
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return dynamic_cast<P3PmsgField&>(*m_pP3PmsgFoN);
}
P3PmsgAttr&
P3PmsgCurs::r_attr ( ) const
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgFoN->r_Attr();
}
P3PmsgDesc&
P3PmsgCurs::r_desc ( ) const
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgFoN->r_Desc();
}
//  Troubleshooting
void
P3PmsgCurs::AssertValid ( ) const
{
    // Header
    VBLelem nItems = (VBLelem)~0;ASSERT(~0==~0u);
    if ( m_pP3PmsgAttr )
      nItems = m_pP3PmsgAttr -> GetCount();
    else if ( m_pP3PmsgDesc )
      nItems = m_pP3PmsgDesc -> GetCount();
    if (   m_pP3PmsgFoN           &&
         ( m_nItem <  0      ||
           m_nItem >= nItems    )    )
      EVERR -> Module ( __FUNCTION__ )
            -> Message(L"Item counter=%i out of range 0 to %i",
                        m_nItem, nItems )
            -> Throw();

    // Delegation
    if ( m_pP3PmsgFoN )
      m_pP3PmsgFoN -> AssertValid ( );
}

// Properties
bool
P3PmsgCurs::IsItem ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgFoN->r_Object().IsField() ? true : false;
}

bool
P3PmsgCurs::IsList ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgFoN->r_Object().IsList() ? true : false;
}

bool
P3PmsgCurs::IsVect ( )
{
    if ( m_pP3PmsgFoN == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgFoN->r_Object().IsVect() ? true : false;
}

int
P3PmsgCurs::Item ( )
{
    return m_nItem;
}

VBLelem
P3PmsgCurs::GetCount ( ) const
{
    if ( m_pP3PmsgAttr )
      return m_pP3PmsgAttr->GetCount ( );
    else if ( m_pP3PmsgDesc )
      return m_pP3PmsgDesc->GetCount ( );
    ASSERT(0);
    return 0;
}
bool
P3PmsgCurs::IsSoCursor ( )
{
    return m_nItem <= 0 ? true : false;
}
bool 
P3PmsgCurs::IsEoCursor ( ) const
{
      // End of scan rationalisation
      int nItems = GetCount();
      if ( nItems <= 0 || m_nItem >= nItems-1 )
        return true;
      return false;
}

VBLock*
P3PmsgCurs_GetVBlock ( P3PmsgCurs& oCurs )
{
    if ( oCurs.IsList() )
      return (VBLock *)oCurs.r_list().r_Object().GetVBLock();
    if ( oCurs.IsItem() )
      return (VBLock *)oCurs.r_item().r_Object().GetVBLock();
    ASSERT(FALSE);
    return nullptr;
}

VBLsize
P3PmsgCurs_GetVBLocknn ( P3PmsgCurs& oCurs )
{
    if ( oCurs.IsItem() )
      return oCurs.r_item().r_Object().GetVBLocknn();
    if ( oCurs.IsList() )
      return oCurs.r_list().r_Object().GetVBLocknn();
    ASSERT(FALSE);
    return 0;
}

///////////////////////////////////////////////////////////////////////
//  P2PmsgRecurs
//  NOTES: Provides recursive P2PmsgNode scanning services

//
//  Constructors and destructors
P2PmsgRecurs::P2PmsgRecurs ( const P2PmsgRecurs& rhs )
            : m_oCurs ( rhs.m_oCurs )
{
    // Firstly
    RenderRecursSafe ( );
}

P2PmsgRecurs::P2PmsgRecurs ( P3PmsgItem& rhs )
            : m_oCurs ( rhs )
{
    // Firstly
    RenderRecursSafe ( );
}

P2PmsgRecurs::P2PmsgRecurs ( P3PmsgAttr& rhs )
            : m_oCurs ( rhs )
{
    // Firstly
    RenderRecursSafe ( );
}

P2PmsgRecurs::~P2PmsgRecurs ( )
{
    // Garbage collection
    delete m_pRecurs;
}

void
P2PmsgRecurs::RenderRecursSafe ( )
{
    m_pRecurs = 0;
    m_cLevel  = 0;
}

//  Operators
P2PmsgRecurs&
P2PmsgRecurs::operator = ( const P2PmsgRecurs& rhs )
{
    if ( this == &rhs )
      return *this;

    // Match the copy-constructor semantics: copy the cursor position and reset
    // the recursion chain.  m_pRecurs is exclusively owned (see ~P2PmsgRecurs,
    // which deletes it), so it must be released here rather than shallow-copied
    // - aliasing rhs.m_pRecurs would double-delete.
    delete m_pRecurs;
    m_pRecurs = 0;
    m_oCurs   = rhs.m_oCurs;
    m_cLevel  = 0;

    // Tidy up, and
    return *this;
}

P2PmsgRecurs&
P2PmsgRecurs::operator ++ ( )
{
TOP:P2PmsgRecurs *pThis = this;
    while ( pThis->m_pRecurs )
      pThis = pThis -> m_pRecurs;
    if ( pThis->m_oCurs.IsEoCursor() &&
         pThis->m_cLevel > 0            )
    {
      Pop ( );
      goto TOP;
    }
    pThis -> m_oCurs.Goto ( pThis->m_oCurs.Item() + 1 );
    return *this;
}

//
// Navigation
int
P2PmsgRecurs::Push ( )
{
    if ( m_pRecurs )
      return m_pRecurs -> Push ( );
    if ( !IsField() )
      EVERR->MODULE
           ->Message(L"Attempt to push non-P2PmsgItem environment" )
           ->Throw ( );
    m_pRecurs = new P2PmsgRecurs ( m_oCurs.r_item() );
    m_pRecurs -> m_cLevel = m_cLevel + 1;
  --m_pRecurs -> m_oCurs;
    return m_pRecurs -> m_cLevel;
}

int
P2PmsgRecurs::Pop ( )
{
    if ( m_pRecurs == NULL )
      EVERR->MODULE
           ->Message(L"Attempt to pop non-pushed environment" )
           ->Throw ( );
    if ( m_pRecurs->m_pRecurs )
      return m_pRecurs -> Pop ( ); //TODO:LJM was debugging m_pRecurs -> Pop ( );
    delete m_pRecurs;
           m_pRecurs = 0;
    return m_cLevel;
}

void
P2PmsgRecurs::Break ( )
{
    while ( m_pRecurs )
      Pop ( );
    m_oCurs.Goto(m_oCurs.GetCount());
}
//
//  Exposure
LPCWSTR
P2PmsgRecurs::c_wstr ( )
{
    if ( m_pRecurs )
      return m_pRecurs->c_wstr();
    return m_oCurs.c_wstr();
}

P3PmsgAttr&
P2PmsgRecurs::r_attr ( ) const
{
    if ( m_pRecurs )
      return m_pRecurs->r_attr();
    return m_oCurs.r_attr();
}
P3PmsgData&
P2PmsgRecurs::r_data ( ) const
{
    if ( m_pRecurs )
      return m_pRecurs->r_data();
    return m_oCurs.r_data();
}

P3PmsgName&
P2PmsgRecurs::r_name ( )
{
    if ( m_pRecurs )
      return m_pRecurs->r_name();
    return m_oCurs.r_name();
}

P3PmsgList&
P2PmsgRecurs::r_list ( )
{
    if ( m_pRecurs )
      return m_pRecurs->r_list();
    return m_oCurs.r_list();
}
P3PmsgVect&
P2PmsgRecurs::r_vect ( )
{
    if ( m_pRecurs )
      return m_pRecurs->r_vect();
    return m_oCurs.r_vect();
}
P3PmsgItem&
P2PmsgRecurs::r_item ( ) const
{
    if ( m_pRecurs )
      return m_pRecurs->r_item();
    return m_oCurs.r_item();
}

//  Troubleshooting
void
P2PmsgRecurs::AssertValid ( ) const
{
    m_oCurs.AssertValid ( );
    if ( m_pRecurs )
      m_pRecurs -> AssertValid ( );
}

// Properties
bool
P2PmsgRecurs::IsField ( )
{
    if ( m_pRecurs )
      return m_pRecurs -> IsField ( );
    return m_oCurs.IsItem ( );
}

bool
P2PmsgRecurs::IsList ( )
{
    if ( m_pRecurs )
      return m_pRecurs -> IsList ( );
    return m_oCurs.IsList ( );
}

bool
P2PmsgRecurs::IsVect ( )
{
    if ( m_pRecurs )
      return m_pRecurs -> IsVect ( );
    return m_oCurs.IsVect ( );
}
bool 
P2PmsgRecurs::IsEoRecurs ( ) const
{
    P2PmsgRecurs *pThis = (P2PmsgRecurs *)this;
    if ( m_pRecurs )
      return false;
    if ( !pThis->m_oCurs.IsEoCursor() )
      return false;
    if ( pThis->m_oCurs.Item() >= pThis->m_oCurs.GetCount() )
      return true;
    return false;
}

