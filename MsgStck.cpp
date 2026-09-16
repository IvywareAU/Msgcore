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
#include "MsgVect.h"
#include "P2Pmsg_Ext.h"
#include "MsgAttr.h"
#include "MsgDesc.h"
#include "MsgStck.h"
#include "Msgexception.h"
#include "P2PmsgVBLock.h"
#include "MsgVBHeap.h"


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
//  MsgStck object manager
//  NOTES: Performs P3PmsgField attribute operations

VBLock*
MsgStck__GetVBLock  ( MsgStck *pThis );
VBLaddr
MsgStck__AllocItem ( MsgStck *pThis, const P3PmsgField& oField );
static void
MsgStck__Unlink ( MsgStck *pThis, VBLaddr aPopped, VBLaddr aBelow );

//UINT
//P2PmsgAttr_GetExtra  ( P3PmsgField *pField );
//void
//P2PmsgAttr_LinkinItem( P3PmsgAttr *pAttr
//                     , UINT aItemPrev, UINT aItem, UINT aItemNext );
//UINT
//P2PmsgAttr_UnLinkItem( P3PmsgAttr *pAttr, VBLockAttr *pVBLockAttr
//                     , UINT aItem );

//  Constructors and destructor
// MsgStck.h declares four constructors; only the P3PmsgField* one below was
// ever defined, so "MsgStck oStck;" -- the default-construct-then-Connect form
// the class's own Connect() exists to serve -- failed to LINK for every
// consumer. Nothing in the repository had tried it, which is why it went
// unnoticed. Every member carries an in-class initialiser, so an empty body is
// the whole of the correct definition.
// The copy constructor and the P3PmsgObject constructor are still declared and
// still undefined; they are left alone here because a stack owns the heap
// objects it has pushed, and what a copy of one should do to that ownership is
// a design question, not an omission to be patched blind.
MsgStck::MsgStck ( )
{
}

MsgStck::MsgStck ( const P3PmsgField *pField )
{
    m_pP3PmsgField = (P3PmsgField *)pField;
}

MsgStck::~MsgStck ( )
{
    Nullify ( );
}

void
MsgStck::Nullify ( ) noexcept
{
    m_pP3PmsgField = 0;
    if ( m_pStckField )
      delete m_pStckField;
    m_pStckField   = 0;
    //if ( m_pStckNode )
    //  delete m_pStckNode;
    //m_pStckNode    = 0;
    if ( m_pStckList )
      delete m_pStckList;
    m_pStckList    = 0;
    if ( m_pStckVect )
      delete m_pStckVect;
    m_pStckVect    = 0;
}
void
MsgStck::Connect ( P3PmsgField *pField ) noexcept
{
    Nullify ( );
    m_pP3PmsgField = pField;
}


//
//  Re-links the stack around the generation Pop is about to drop: the live item
//  takes the generation BELOW the popped one, and the popped one's own aStack
//  is cut.
//  NOTES: BOTH STORES MATTER, and the second was missing. Drop() walks the
//         stack -- P3PmsgField::Drop and P3PmsgList::Drop both end with
//         "if (IsStacked()) r_Stck().Drop()" -- and MsgStck::Drop zeroes every
//         link the rest of the way down. So dropping a popped item that still
//         pointed at the generation below it severed the generations Pop was
//         supposed to leave standing, and leaked their blocks, since
//         MsgStck::Drop unlinks without freeing.
//         Measured before the second store: push "Gen0"/"Gen1"/"Gen2"/"Gen3",
//         pop once, and "^" answered Gen1 while "^^" -- Gen0, which is still
//         allocated and still correct -- answered void. A single push and pop
//         hid it, because there was no generation below to sever, and that is
//         the only shape anything in the tree had exercised.
//       : RE-DERIVED here rather than passed in. The restore above this call
//         allocates (r_Attr(), r_Desc(), AddListTail, InsertAt), an allocation
//         may grow the heap, and a grown heap moves every block in the image.
//         Both VBLock pointers held across it are stale; the two VBLaddr's are
//         not, because they are offsets.
static void
MsgStck__Unlink ( MsgStck *pThis, VBLaddr aPopped, VBLaddr aBelow )
{
    P3PmsgObject& oObject  = (P3PmsgObject&)pThis -> GetField() -> r_Object();
    UCHAR&        uVBLock  = oObject.m_uVBLock;
    VBLock       *pVBLock  = ptrVBLOCK ( oObject );
    VBLock       *pVBLock1 = (VBLock *)oObject.Msg2Phys ( aPopped );
    VBLockItem_SetStack ( uVBLock, VBLock_pItem(pVBLock),  aBelow );
    VBLockItem_SetStack ( uVBLock, VBLock_pItem(pVBLock1), 0      );
    //  Off the chain, so no longer a snapshot of anything. aParent on an item
    //  means "the collection I am linked into", and Push borrows it to mean
    //  "the item I was pushed from" -- which is true only while the block is
    //  on that item's stack. P3PmsgField::Drop reads aParent to decide which
    //  collection to unlink from and ASSERTs on anything else, so the loan has
    //  to be returned here, where the block stops being a snapshot, and not
    //  left for the free to trip over. Same idiom as every other unlink.
    VBLockItem_SetParent ( uVBLock, VBLock_pItem(pVBLock1), 0      );
}

//
//  Copies the four parts of an item that every item type has in common.
//  NOTES: Piece by piece, and NOT through P3PmsgField::operator=, which copies
//         the stack as well (P2Pmsg.cpp:3235). Inside Push that would duplicate
//         the very generations the push is re-linking; inside Pop it would
//         re-attach the snapshot's chain to the live item. The four assignments
//         below are what the field arm of Push has always done by hand, lifted
//         out so the list and vector arms cannot drift from it.
//       : bRestore is Pop's side of it. Push copies onto a block that
//         P2PmsgItem_InitField / _InitItem has just laid out empty, so an
//         absent collection is already absent; Pop copies onto the LIVE item,
//         which may carry attributes or descendants the snapshot does not, and
//         those have to go.
static void
MsgStck__CopyParts ( P3PmsgField& oDst, P3PmsgField& oSrc, bool bRestore )
{
    oDst.r_name() = oSrc.r_name();
    oDst.r_data() = oSrc.r_data();
    if ( oSrc.IsAttributed() )
      oDst.r_Attr() = oSrc.r_Attr();
    else if ( bRestore )
      oDst.r_Attr().Drop();
    if ( oSrc.IsDescendant() )
      oDst.r_Desc() = oSrc.r_Desc();
    else if ( bRestore )
      oDst.r_Desc().Drop();
}

//
//  Copies a list's elements. P2PmsgList_InitItem lays out an EMPTY list --
//  VBLockList_Init zeroes aFirst, aLast and nItems -- so the payload is not
//  carried by the allocation the way the name and data cells are, and has to be
//  walked over. This is the same loop P3PmsgList::operator= uses, and it relies
//  on the same thing: the source keeps its own cursor, so its GetNext()
//  references stay valid across the destination's AddListTail() calls.
static void
MsgStck__CopyElems ( P3PmsgList& oDst, P3PmsgList& oSrc )
{
    VBLaddr aElem = oSrc.GetHeadPos ( );
    while ( aElem )
      oDst.AddListTail ( oSrc.GetNext(aElem) );
}

//
//  The same for a vector, by index. P3PmsgVect::InsertAt -> AllocElem handles a
//  nested field, list or vector, so an element that is itself a container is
//  deep-copied rather than aliased.
static void
MsgStck__CopyElems ( P3PmsgVect& oDst, P3PmsgVect& oSrc )
{
    VBLelem nItems = oSrc.GetCount ( );
    for ( VBLelem i = 0; i < nItems; i++ )
      oDst.InsertAt ( (int)i, oSrc.r_item((int)i) );
}

MsgStck&
MsgStck::Push ( )
{
    // Displacement P3PmsgField
    P3PmsgObject& oObject = (P3PmsgObject&)m_pP3PmsgField -> r_Object();
    UCHAR&        uVBLock = oObject.m_uVBLock;
    //UINT    aVBLock  = m_pP3PmsgField -> GetVBLocknn();
    VBLock *pVBLock  = ptrVBLOCK(m_pP3PmsgField->r_Object()); // -> r_Object().GetVBLock();
    VBLaddr aVBLock2 = VBLockItem_GetStack ( uVBLock, VBLock_pItem(pVBLock) );

    // P3PmsgNode
    //if ( m_pP3PmsgField->r_Object().IsNode() )
    //{
    //  ASSERT(0);
    //}

    const bool bList = m_pP3PmsgField->r_Object().IsList();
    const bool bVect = m_pP3PmsgField->r_Object().IsVect();
    if ( !bList && !bVect && !m_pP3PmsgField->r_Object().IsField() )
    {
      ASSERT(0);                       // Not an item; nothing to push
      return *this;
    }

    //  ALLOCATION AND LINKING ARE TYPE-INDEPENDENT, and that is why the list
    //  and vector arms below are so short. MsgStck__AllocItem has dispatched on
    //  the item type since it was written -- it sizes with P2PmsgList_SizeofItem
    //  or P2PmsgVect_SizeofItem and lays the block out with the matching
    //  _InitItem -- so a list block was always allocatable; Push simply never
    //  called it for one and asserted instead. The two links are aStack fields
    //  in the VBLockItem header, which every item type carries whatever the ut
    //  union under it holds.
    VBLaddr aVBLock1 = MsgStck__AllocItem ( this, *m_pP3PmsgField );
    VBLock *pVBLock1 = (VBLock *)m_pP3PmsgField -> r_Object().Msg2Phys(aVBLock1);

    // RE-DERIVED, and it must be: pVBLock above was taken BEFORE the
    // allocation on the line above it, and an allocation may grow the heap,
    // which reallocates the base image and moves every block in it. Writing
    // through the stale pointer corrupted whatever now occupied that
    // address -- reliably, once a store was big enough for AllocItem to
    // trigger a growth. Found 2026-08-15 by MsgcoreCom's PushValue.
    pVBLock = ptrVBLOCK ( m_pP3PmsgField->r_Object() );

    VBLockItem_SetStack ( uVBLock, VBLock_pItem(pVBLock),  aVBLock1 );
    VBLockItem_SetStack ( uVBLock, VBLock_pItem(pVBLock1), aVBLock2 );
    pVBLock1 -> oHdr.uVBLockDefs |= VBLock_Linked;

    //  RECORD THE OWNER. aParent was left zero here, and a zero parent is what
    //  P3Pmsg_GetPath reads as a FLOATING item -- so every snapshot of BHP,
    //  whatever its generation and wherever BHP lived, answered ".BHP": not
    //  this object's path, resolving to something else entirely, and the same
    //  string for all of them. Nothing else in the block could be walked to
    //  find the owner, because aStack runs the other way (owner -> newest ->
    //  older) and a snapshot has no back-link along it.
    //
    //  The OWNER, not the generation above: the older generations already
    //  point at it and go on pointing at it, so a push adds this one line and
    //  Pop and Drop have nothing to maintain -- the block they unlink is one
    //  they free. P3Pmsg_GetPath counts the generation by walking the owner's
    //  chain, which also tells it that this really is a snapshot of that item.
    VBLockItem_SetParent ( uVBLock, VBLock_pItem(pVBLock1)
                         , m_pP3PmsgField->r_Object().GetVBLocknn() );

    //  Read the size while pVBLock1 is still fresh: everything below this line
    //  allocates, and the copies are addressed by VBLaddr, not by pointer.
    const VBLsize nSizeof1 = VBLock_Hdr_u_SizeNN ( pVBLock1 );

    // P3PmsgList
    if ( bList )
    {
      P3PmsgList oLive  ( m_pP3PmsgField->r_Object() );
      P3PmsgList oList1 ( oObject.m_hVBList, aVBLock1, nSizeof1 );
      MsgStck__CopyParts ( oList1, oLive, false );
      MsgStck__CopyElems ( oList1, oLive );
    }

    // P3PmsgVect
    else if ( bVect )
    {
      P3PmsgVect oLive  ( m_pP3PmsgField->r_Object() );
      P3PmsgVect oVect1 ( oObject.m_hVBList, aVBLock1, nSizeof1 );
      MsgStck__CopyParts ( oVect1, oLive, false );
      MsgStck__CopyElems ( oVect1, oLive );
    }

    // P3PmsgField
    else
    {
      P3PmsgField oField1 ( oObject.m_hVBList, aVBLock1, nSizeof1 );
      MsgStck__CopyParts ( oField1, *m_pP3PmsgField, false );
    }

    // Tidy up, and
    return *this;
}

MsgStck&
MsgStck::Pop ( )
{
    // Fetch previously pushed item
    P3PmsgObject& oObject = (P3PmsgObject&)m_pP3PmsgField -> r_Object();
    UCHAR&        uVBLock = oObject.m_uVBLock;
    //UINT    aVBLock  = m_pP3PmsgField -> GetVBLocknn();
    VBLock *pVBLock  = ptrVBLOCK(m_pP3PmsgField->r_Object());// -> r_Object().GetVBLock();
    VBLaddr aVBLock1 = VBLockItem_GetStack ( uVBLock, VBLock_pItem(pVBLock) );
    if ( !aVBLock1 )
      return *this;                    // Nothing pushed
    VBLock *pVBLock1 = (VBLock *)m_pP3PmsgField -> r_Object().Msg2Phys ( aVBLock1 );
    VBLaddr aVBLock2 = VBLockItem_GetStack ( uVBLock, VBLock_pItem(pVBLock1) );
    const VBLsize nSizeof1 = VBLock_Hdr_u_SizeNN ( pVBLock1 );

    // P3PmsgNode
    //if ( m_pP3PmsgField->r_Object().IsNode() )
    //{
    //  ASSERT(0);
    //}

    const bool bList = m_pP3PmsgField->r_Object().IsList();
    const bool bVect = m_pP3PmsgField->r_Object().IsVect();
    if ( !bList && !bVect && !m_pP3PmsgField->r_Object().IsField() )
    {
      ASSERT(0);                       // Not an item; nothing to pop
      return *this;
    }

    // P3PmsgList
    if ( bList )
    {
      //  Restored THROUGH THE CALLER'S OWN OBJECT where there is one. A
      //  P3PmsgList caches up to MAX_P3PmsgData_Curs element cursors, and
      //  Truncate() is what clears them; truncating a second view of the same
      //  block would free the element blocks while leaving the caller's cursors
      //  pointing at them.
      P3PmsgList  oView ( m_pP3PmsgField->r_Object() );
      P3PmsgList *pLive = dynamic_cast<P3PmsgList*>(m_pP3PmsgField);
      P3PmsgList& oLive = pLive ? *pLive : oView;
      P3PmsgList  oList1 ( oObject.m_hVBList, aVBLock1, nSizeof1 );
      oLive.Truncate ( );
      MsgStck__CopyParts ( oLive, oList1, true );
      MsgStck__CopyElems ( oLive, oList1 );
      MsgStck__Unlink    ( this, aVBLock1, aVBLock2 );
      oList1.Drop ( );
    }

    // P3PmsgVect
    else if ( bVect )
    {
      P3PmsgVect  oView ( m_pP3PmsgField->r_Object() );
      P3PmsgVect *pLive = dynamic_cast<P3PmsgVect*>(m_pP3PmsgField);
      P3PmsgVect& oLive = pLive ? *pLive : oView;
      P3PmsgVect  oVect1 ( oObject.m_hVBList, aVBLock1, nSizeof1 );
      oLive.Truncate ( );
      MsgStck__CopyParts ( oLive, oVect1, true );
      MsgStck__CopyElems ( oLive, oVect1 );
      MsgStck__Unlink    ( this, aVBLock1, aVBLock2 );
      oVect1.Drop ( );
    }

    // P3PmsgField
    else
    {
      P3PmsgField oField1 ( oObject.m_hVBList, aVBLock1, nSizeof1 );
      MsgStck__CopyParts ( *m_pP3PmsgField, oField1, true );
      MsgStck__Unlink    ( this, aVBLock1, aVBLock2 );
      oField1.Drop ( );
    }

    // Tidy up, and
    return *this;
}
      /*P3PmsgNode&
        AddNode ( LPCTSTR lpszName, const P3PmsgData& oData );
      P3PmsgField&
        AddField( LPCTSTR lpszName, const P3PmsgData& oData );*/

    // Operators
      /*P3PmsgNode&
        operator  = ( const P3PmsgNode& rhs );
      /*P3PmsgNode&
        operator  = ( const P3PmsgField& rhs );
      P3PmsgNode&
        operator  = ( const P2PmsgNodeHdl& rhs );
      P3PmsgNode&
        operator += ( const P3PmsgList& rhs );
      virtual P3PmsgField&
        operator [] ( LPCTSTR lpszName );

        operator P3PmsgData& ( );*/

// Operators
MsgStck&
MsgStck::operator = ( const MsgStck& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;

    // Null or empty outcome
    // NOTES: Usual or normal outcome
    if ( rhs.IsEmpty() )
    {
      if ( !IsEmpty() )
        Drop ( );
      return *this;
    }

    // Confirm stack exists
    if ( IsEmpty() )
    {
      P3PmsgObject& oObject = (P3PmsgObject&)m_pP3PmsgField->r_Object();
      VBLaddr       aStck   = MsgStck__AllocItem ( this, ((MsgStck&)rhs).r_item() );
      //UCHAR   uVBLock = m_pP3PmsgField -> m_uVBLock;
      VBLock *pVBLock =(VBLock *)m_pP3PmsgField -> r_Object().GetVBLock();
      VBLockItem_SetStack ( oObject.m_uVBLock, VBLock_pItem(pVBLock), aStck );
      VBLock *pVBLockStck = (VBLock *)oObject.Msg2Phys(aStck);
      pVBLockStck -> oHdr.uVBLockDefs |= VBLock_Linked;
ASSERT(VBLock_IsLinked(pVBLockStck));
ASSERT(VBLock_IsAlloc(pVBLockStck));
ASSERT(VBLock_IsItem(pVBLockStck));
    }

    // Assignment
    /*if ( m_pP3PmsgField->r_Object().IsNode() )
      r_node() = ((MsgStck&)rhs).r_node();
    else*/ if ( m_pP3PmsgField->r_Object().IsList() )
      r_list() = ((MsgStck&)rhs).r_list();
    else if ( m_pP3PmsgField->r_Object().IsVect() )
      r_vect() = ((MsgStck&)rhs).r_vect();
    else if ( m_pP3PmsgField->r_Object().IsField() )
      r_item() = ((MsgStck&)rhs).r_item();

    // Tidy up, and
    return *this;
}

//  Memory management
VBLaddr
MsgStck__AllocItem ( MsgStck *pThis, const P3PmsgField& oField )
{
    VBLaddr aVBLock      = 0;
    VBLock *pVBLock      = 0;
    VBLsize nSizeofItem  = 0;
    //P3PmsgObject& oObject= (P3PmsgObject&)m_pP3PmsgField -> r_Object(); TODO:LJM deprecated
    P3PmsgObject& oObject= (P3PmsgObject&)pThis -> GetField() -> r_Object();
    //UCHAR   uVBLock      = m_pP3PmsgField -> m_uVBLock;
    //  NO ASSERT THAT THE STACK IS EMPTY HERE.  It used to read
    //
    //      ASSERT(MsgStck__GetVBLock(pThis)==0);
    //
    //  which forbids a SECOND push -- and Push is a linked stack that supports
    //  one: it reads the current head into aVBLock2 before allocating and
    //  re-links it onto the new item afterwards, exactly so that pushes nest.
    //  The assert is a leftover from a single-slot era and contradicts the code
    //  three functions above it.
    //
    //  Measured, Release x64 (asserts off), on a text node holding "first":
    //  push, write "second", push, write "third", pop -> "second", pop ->
    //  "first", pop -> S_FALSE.  It nests and unwinds correctly.
    //  Found 2026-08-15 by MsgcoreCom's PushValue via _Msgcore_UseExamplesCom.

    // Allocation P3PmsgField
    if ( oField.r_Object().IsField() )
    {
      const P3PmsgField *pField = &oField;
      nSizeofItem = P2PmsgField_SizeofItem ( oObject.m_uVBLock, *pField );
      aVBLock     = oObject.AllocVBLock ( VBLock_Item, nSizeofItem );
      pVBLock     = (VBLock *)oObject.Msg2Phys(aVBLock);
      P2PmsgItem_InitField  ( pVBLock, *pField );
      return aVBLock;
    }

    // Allocation P3PmsgNode
    /*if ( oField.r_Object().IsNode() )
    {
      const P3PmsgNode *pNode = dynamic_cast<const P3PmsgNode*>(&oField);
      nSizeofItem = P2PmsgNode_SizeofItem ( oObject.m_uVBLock, pNode );
      aVBLock     = oObject.Alloc ( VBLock_Item, nSizeofItem );
      pVBLock     = (VBLock *)oObject.Msg2Phys(aVBLock);
      P2PmsgNode_InitItem  ( pVBLock, pNode );
      return aVBLock;
    }*/

    // Allocation P3PmsgList
    if ( oField.r_Object().IsList() )
    {
      const P3PmsgList *pList = dynamic_cast<const P3PmsgList*>(&oField);
      nSizeofItem = P2PmsgList_SizeofItem ( oObject.m_uVBLock, *pList );
      aVBLock     = oObject.AllocVBLock ( VBLock_Item, nSizeofItem );
      pVBLock     = (VBLock *)oObject.Msg2Phys(aVBLock);
      P2PmsgList_InitItem ( pVBLock, *pList );
      return aVBLock;
    }

    // Allocation P3PmsgVect
    if ( oField.r_Object().IsVect() )
    {
      const P3PmsgVect *pVect = dynamic_cast<const P3PmsgVect*>(&oField);
      nSizeofItem = P2PmsgVect_SizeofItem ( oObject.m_uVBLock, *pVect );
      aVBLock     = oObject.AllocVBLock ( VBLock_Item, nSizeofItem );
      pVBLock     = (VBLock *)oObject.Msg2Phys(aVBLock);
      P2PmsgVect_InitItem ( pVBLock, *pVect );
      return aVBLock;
    }

    // Tidy up, and
    return aVBLock;
}

//
//  Releases every generation on the item's stack.
//  NOTES: IT USED TO FREE NOTHING. It walked to the deepest generation, zeroed
//         each aStack on the way back up, and returned -- so every pushed block,
//         and the name and data blocks hanging off it, stayed allocated with
//         nothing left pointing at them. Every caller wants the storage back:
//         P3PmsgField::Drop, P3PmsgList::Drop and P3PmsgVect::Drop all reach
//         here while dismantling an item, MsgFacade's FacadeNode exposes it as
//         the COM "drop the stack" verb, and Targetcore's P2PeerMsg calls it
//         when it replaces one stack with another.
//         Measured on a single push of a 272-byte item: push, record the
//         snapshot's P2Pos, Drop, push an identical item again, and the second
//         snapshot landed 272 bytes further on instead of in the block the
//         first one had vacated.
//       : HEAD FIRST, WHICH IS WHY THIS IS A LOOP AND NOT A RECURSION. Drop()
//         ends with "if (IsStacked()) r_Stck().Drop()", so simply dropping the
//         head would unwind the chain on its own -- deepest generation first.
//         That is the one order this heap cannot reclaim. Generations are
//         pushed at ascending addresses, and P2PmsgHeap_CollateIOMAGE merges a
//         freed block only with its NEXT physical neighbour; there is no
//         backward merge, because a VBHeap block carries no footer to find its
//         predecessor by. Free deepest-first and every block's next neighbour
//         is still allocated at the moment it is freed, so nothing coalesces:
//         the list ends up holding N separate blocks, only the highest merges
//         with the image tail, and that merged block then sits at the head of
//         the free list and satisfies every subsequent request -- so the other
//         N-1 are never looked at again. Free head-first and each block merges
//         into the span above it that was freed a moment earlier, and the whole
//         chain comes back.
//         Measured, three generations, four rounds of push-push-push-Drop:
//             deepest-first   949 1361 1773 2185   (+412 a round)
//             head-first      949  949  949  949
//         The loop therefore unlinks each generation before dropping it, which
//         is also what stops Drop() recursing underneath us.
//       : THIS IS ORDERING, NOT A HEAP FIX. Any caller that frees adjacent
//         blocks low-to-high loses them the same way -- P3PmsgDesc::Truncate
//         deletes child 0 repeatedly and drifts 824 bytes per truncate-refill
//         round on five children, with no stack involved. Refer stack_paths.md
//         section 8.
//       : BY THE GENERATION'S OWN TYPE. A pushed list is a list, and
//         P3PmsgField::Drop opens ASSERT(OBJ__IsField()) and frees the item
//         block with no Truncate(), which would leak every element in it --
//         the same trap P3PmsgVect::Drop was added to close.
void
MsgStck::Drop ( )
{
    if ( m_pP3PmsgField == nullptr )
      return;
    VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> r_Object().GetVBLock ( );
    if ( pVBLock == nullptr )
      return;
    UCHAR   uVBLock = pVBLock->oHdr.uVBLockDefs;
    VBLaddr aStack  = VBLockItem_GetStack ( uVBLock, VBLock_pItem(pVBLock) );
    if ( !aStack )
      return;

    // Unlink, then free -- head first
    VBLockItem_SetStack ( uVBLock, VBLock_pItem(pVBLock), 0 );

    P2PmsgHANDLE hVBList = m_pP3PmsgField -> OBJ__hVBList;
    VBLaddr      aGen    = aStack;
    while ( aGen )
    {
      VBLock     *pGen  = (VBLock *)m_pP3PmsgField->r_Object().Msg2Phys(aGen);
      VBLockItem *pItem = VBLock_pItem ( pGen );
      VBLaddr     aNext = VBLockItem_GetStack ( pGen->oHdr.uVBLockDefs, pItem );
      VBLockItem_SetStack  ( pGen->oHdr.uVBLockDefs, pItem, 0 );
      //  And the owner it was pushed from -- refer MsgStck__Unlink.
      VBLockItem_SetParent ( pGen->oHdr.uVBLockDefs, pItem, 0 );
      if ( VBLockItem_IsList(pItem) )
      {
        P3PmsgList oGen ( hVBList, aGen, 0 );
        oGen.Drop ( );
      }
      else if ( VBLockItem_IsVect(pItem) )
      {
        P3PmsgVect oGen ( hVBList, aGen, 0 );
        oGen.Drop ( );
      }
      else
      {
        P3PmsgField oGen ( hVBList, aGen, 0 );
        oGen.Drop ( );
      }
      aGen = aNext;
    }
}

//  Navigation and 
MsgStck&
MsgStck::Rename ( LPCTNAM lpszName, bool bRecurse )
{
    if ( IsEmpty() )
      return *this;
    if ( m_pP3PmsgField->r_Object().IsList() )
    {
      r_list().r_name().c_name(lpszName);
      if ( bRecurse )
        r_list().r_Stck().Rename ( lpszName, bRecurse );
    }
    if ( m_pP3PmsgField->r_Object().IsVect() )
    {
      r_vect().r_name().c_name(lpszName);
      if ( bRecurse )
        r_vect().r_Stck().Rename ( lpszName, bRecurse );
    }
    else if ( m_pP3PmsgField->r_Object().IsField() )
    {
      r_item().r_name().c_name(lpszName);
      if ( bRecurse )
        r_item().r_Stck().Rename ( lpszName, bRecurse );
    }
    //else if ( m_pP3PmsgField->r_Object().IsNode() )
    //{
    //  r_node().r_name().c_name(lpszName);
    //  if ( bRecurse )
    //    r_node().r_Stck().Rename ( lpszName, bRecurse );
    //}
    else
      ASSERT(0);
    return *this;
}

//  Addressing and allocations
VBLock*
MsgStck__GetVBLock  ( MsgStck *pThis )
{
    // if ( !m_pP3PmsgField ) //TODO:LJM deprecated below
    if ( !pThis->GetField() )
      return (VBLock *)0;
    //VBLock *pVBLock = m_pP3PmsgField -> r_Object().GetVBLock ( ); //TODO:LJM deprecated below
    VBLock *pVBLock = (VBLock *)pThis -> GetField() -> r_Object().GetVBLock ( );
    ASSERT(VBLock_IsLinked(pVBLock));
    VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    if ( !aStack )
      return 0;
    //VBLock *pVBStck = (VBLock *)m_pP3PmsgField -> r_Object().Msg2Phys ( aStack ); //TODO:LJM deprecated below
    VBLock *pVBStck = (VBLock *)pThis -> GetField() -> r_Object().Msg2Phys ( aStack );
    ASSERT(VBLock_IsItem(pVBStck));
    ASSERT(VBLock_IsLinked(pVBStck));
    ASSERT(VBLock_IsAlloc(pVBStck));
    return  pVBStck;
}

///////////////////////////////////////
//  Exposure

P3PmsgData&
MsgStck::r_data ( )
{
    return m_pP3PmsgField->r_data();
}
P3PmsgName&
MsgStck::r_name ( )
{
    return m_pP3PmsgField->r_name();
}
P3PmsgList&
MsgStck::r_list ( )
{
    if ( m_pP3PmsgField == nullptr || !m_pP3PmsgField->r_Object().IsList() )
      EVERR->MODULE
           ->Message("Not a P2PmsgList type object" )
           ->Throw ( );
    if ( m_pStckList == nullptr )
    {
      VBLock *pVBLock = ptrVBLOCK(m_pP3PmsgField->r_Object());// -> r_Object().GetVBLock ( );
      VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      if ( !aStack )
        EVERR->MODULE
             ->Message("Not a pushed P2PmsgField object" )
             ->Throw ( );
      m_pStckList = new P3PmsgList ( m_pP3PmsgField->OBJ__hVBList, aStack, 0 );
    }
    return *m_pStckList;
}
P3PmsgVect&
MsgStck::r_vect ( )
{
    if ( m_pP3PmsgField == nullptr || !m_pP3PmsgField->r_Object().IsVect() )
      EVERR->MODULE
           ->Message("Not a P2PmsgVect type object" )
           ->Throw ( );
    if ( m_pStckVect == nullptr )
    {
      VBLock *pVBLock = ptrVBLOCK(m_pP3PmsgField->r_Object()); // -> r_Object().GetVBLock ( );
      VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      if ( !aStack )
        EVERR->MODULE
             ->Message("Not a pushed P2PmsgVect object" )
             ->Throw ( );
      m_pStckVect = new P3PmsgVect ( m_pP3PmsgField->OBJ__hVBList, aStack, 0 );
    }
    return *m_pStckVect;
}
P3PmsgItem&
MsgStck::r_item ( )
{
    if ( m_pP3PmsgField == nullptr || !m_pP3PmsgField->r_Object().IsField() )
      EVERR->MODULE
           ->Message("Not a P2PmsgField type object" )
           ->Throw ( );
    if ( m_pStckField == nullptr )
    {
      VBLock *pVBLock = ptrVBLOCK(m_pP3PmsgField->r_Object());// -> r_Object().GetVBLock ( );
      VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      if ( !aStack )
        EVERR->MODULE
             ->Message("Not a pushed P2PmsgField object" )
             ->Throw ( );
      m_pStckField = new P3PmsgField ( m_pP3PmsgField->OBJ__hVBList, aStack, 0 );
    }
    return *m_pStckField;
}
/*P3PmsgNode&
MsgStck::r_node ( )
{
    if ( m_pP3PmsgField == nullptr || !m_pP3PmsgField->r_Object().IsNode() )
      EVERR->MODULE
           ->Message("Not a P2PmsgNode type object" )
           ->Throw ( );
    if ( m_pStckNode == nullptr )
    {
      VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> r_Object().GetVBLock ( );
      VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
      if ( !aStack )
        EVERR->MODULE
             ->Message("Not a pushed P2PmsgNode object" )
             ->Throw ( );
      m_pStckNode = new P3PmsgNode ( m_pP3PmsgField->OBJ__hVBList, aStack, 0 );
    }
    return *m_pStckNode;
}*/

///////////////////////////////////////
//  Troubleshooting
void
MsgStck::AssertValid ( ) const
{
    if ( IsEmpty() )
      return;
    /*if ( m_pP3PmsgField->r_Object().IsNode() )
      ((MsgStck*)this)->r_node().AssertValid();
    else*/ if ( m_pP3PmsgField->r_Object().IsList() )
      ((MsgStck*)this)->r_list().AssertValid();
    else if ( m_pP3PmsgField->r_Object().IsField() )
      ((MsgStck*)this)->r_item().AssertValid();
    else
      ASSERT(0);
}
void
MsgStck::Print ( FILE *fd, int nDepthOS, int nDepthOSinc )
{
    // P2PmsgStck details
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    VBLsize nVBLockSize = VBLock_Hdr_u_SizeNN(MsgStck__GetVBLock(this));
    P3Pmsg_fwprintf ( fd, L"^(%Ii) {\n", nVBLockSize );

    // Instanciation
    VBLock *pVBLock = ptrVBLOCK(m_pP3PmsgField->r_Object()); //.GetVBLock ( );
    VBLaddr aStack  = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    if ( aStack )
    {
      VBLockItem *pItem = VBLock_pItem(pVBLock);
      //if ( VBLockItem_IsNode(pItem) )
      //{
      //  P3PmsgNode oNode ( m_pP3PmsgField->m_oObject.m_hVBList, aStack, 0 );
      //  oNode.Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      //}
      if ( VBLockItem_IsList(pItem) )
      {
        P3PmsgList oList ( m_pP3PmsgField->m_oObject.m_hVBList, aStack, 0 );
        oList.Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      }
      else if ( VBLockItem_IsVect(pItem) )
      {
        P3PmsgVect oVect ( m_pP3PmsgField->m_oObject.m_hVBList, aStack, 0 );
        oVect.Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      }
      else if ( VBLockItem_IsField(pItem) )
      {
        P3PmsgField oField ( m_pP3PmsgField->m_oObject.m_hVBList, aStack, 0 );
        oField.Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      }
    }

    // Tidy up
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    P3Pmsg_fwprintf ( fd, L"}\n" );
}

// Properties
bool
MsgStck::IsEmpty ( ) const
{
    if ( m_pP3PmsgField == nullptr )
      return false;
    return !m_pP3PmsgField -> IsStacked ();
}
P3PmsgField*
MsgStck::GetField ( ) noexcept
{
    return m_pP3PmsgField;
}
//
//  References on hVBList held by this stack
//  NOTES: The three stacked objects, each of which is newed on the OWNING
//         field's heap by r_field/r_list/r_vect and deleted by RecycleThis. A
//         Push alone creates none of them -- §26 measured a push as adding no
//         holder at all and that reading was correct -- so what this counts is
//         a snapshot that has since been READ. That is the state §26 could not
//         reach and recorded as unreachable rather than as costing nothing.
//       : m_pP3PmsgField is the field that OWNS this stack and is not followed.
//         Refer P3PmsgField::HeapHolders.
int
MsgStck::HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept
{
    if ( hVBList == 0 )
      return 0;

    int nHolders = 0;
    if ( m_pStckField != nullptr )
      nHolders += m_pStckField -> HeapHolders ( hVBList );
    if ( m_pStckList  != nullptr )
      nHolders += m_pStckList  -> HeapHolders ( hVBList );
    if ( m_pStckVect  != nullptr )
      nHolders += m_pStckVect  -> HeapHolders ( hVBList );
    return nHolders;
}
