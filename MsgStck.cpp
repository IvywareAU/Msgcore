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

    // P3PmsgList
    if ( m_pP3PmsgField->r_Object().IsList() )
    {
      ASSERT(0);
    }

    // P3PmsgVect
    else if ( m_pP3PmsgField->r_Object().IsVect() )
    {
      ASSERT(0);
    }

    // P3PmsgField
    else if ( m_pP3PmsgField->r_Object().IsField() )
    {
      //UINT nSizeofItem = P2PmsgField_SizeofItem ( uVBLock, m_pP3PmsgField );
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

      P3PmsgField oField1 ( oObject.m_hVBList
                          , aVBLock1, VBLock_Hdr_u_SizeNN(pVBLock1) );
      oField1.r_name() = m_pP3PmsgField -> r_name();
      oField1.r_data() = m_pP3PmsgField -> r_data();
      if ( m_pP3PmsgField->IsAttributed() )
        oField1.r_Attr() = m_pP3PmsgField -> r_Attr();
      if ( m_pP3PmsgField->IsDescendant() )
        oField1.r_Desc() = m_pP3PmsgField -> r_Desc();
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

    // P3PmsgNode
    //if ( m_pP3PmsgField->r_Object().IsNode() )
    //{
    //  ASSERT(0);
    //}

    // P3PmsgList
    if ( m_pP3PmsgField->r_Object().IsList() )
    {
      ASSERT(0);
    }

    // P3PmsgVect
    else if ( m_pP3PmsgField->r_Object().IsVect() )
    {
      ASSERT(0);
    }

    // P3PmsgField
    else if ( m_pP3PmsgField->r_Object().IsField() )
    {
      P3PmsgField oField1 ( oObject.m_hVBList
                          , aVBLock1, VBLock_Hdr_u_SizeNN(pVBLock1) );
      m_pP3PmsgField -> r_name() = oField1.r_name();
      m_pP3PmsgField -> r_data() = oField1.r_data();
      if ( oField1.IsAttributed() )
        m_pP3PmsgField -> r_Attr() = oField1.r_Attr();
      else
        m_pP3PmsgField -> r_Attr().Drop();
      if ( oField1.IsDescendant() )
        m_pP3PmsgField -> r_Desc() = oField1.r_Desc();
      else
        m_pP3PmsgField -> r_Desc().Drop();
      VBLockItem_SetStack ( uVBLock, VBLock_pItem(pVBLock), aVBLock2 );
      oField1.Drop();
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

void
MsgStck::Drop ( )
{
    if ( m_pP3PmsgField == nullptr )
      return;
    VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> r_Object().GetVBLock ( );
    VBLaddr aStack = VBLockItem_GetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    if ( !aStack )
      return;
    P3PmsgField oField ( m_pP3PmsgField->OBJ__hVBList, aStack, 0 );
                oField.r_Stck().Drop();
    VBLockItem_SetStack ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock), 0 );
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
