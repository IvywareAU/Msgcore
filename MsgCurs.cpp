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

P3PmsgCurs&
P3PmsgCurs::Seek ( )
{
    Goto ( 0 );
    return *this;
}
bool
P3PmsgCurs::Goto ( LPCTNAM lpszItemName )
{
    //  Hold the key still for the whole walk. Every sibling compared below spends
    //  a slot of the widening ring (c_wcsicmp -> c_name -> p2p_wstr_from_store),
    //  so a caller who passed some other item's c_name() would have its key
    //  rewritten from under it after ~16 siblings — and the rewritten key then
    //  compares EQUAL to the sibling that overwrote it, stopping this scan on the
    //  wrong item. Snapshotting in the callee is what makes that unreachable
    //  however Goto is called; see p2p_wkey (Platform/p2pstr.h).
    const p2p_wkey oItemName ( lpszItemName );
    if ( m_pP3PmsgFoN                                         &&
         m_pP3PmsgFoN->r_name().c_wcsicmp(oItemName) == 0        )
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

    m_pP3PmsgFoN = 0;
    while ( aItem )
    {
      ASSERT(GetCount()>0);
      //VBLock *pVBLock = 0;
      if ( m_pP3PmsgDesc )
        pVBLock = (VBLock *)m_pP3PmsgDesc -> r_Object().Msg2Phys(aItem); //TODO:LJM 2013/08/30 was GetField() -> r_Object().Msg2Phys ( aItem );
      else if ( m_pP3PmsgAttr )
        pVBLock = (VBLock *)m_pP3PmsgAttr -> r_Object().Msg2Phys(aItem); //TODO:LJM 2013/08/30 was GetField() -> r_Object().Msg2Phys ( aItem );
      //else if ( m_pP3PmsgNode )
      //  pVBLock = (VBLock *)m_pP3PmsgNode -> r_Object().Msg2Phys(aItem);
      else { ASSERT(0); break; }
      //ASSERT(VBLock_IsAlloc(pVBLock));
      //ASSERT(VBLock_IsLinked(pVBLock));
      VBLockItem *pItem   = VBLock_pItem  ( pVBLock );
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
      //LPCTSTR lpszName = m_pP3PmsgFoN->c_name();
      if ( m_pP3PmsgFoN->r_name().c_wcsicmp(oItemName) == 0 )
        return true;
      aItem = VBLockItem_GetNext ( pVBLock->oHdr.uVBLockDefs, pItem, &m_nItem );
      m_pP3PmsgFoN = 0;
    }

    // Tidy up, and
    m_nItem = 0;
    return false;
}
bool
P3PmsgCurs::Goto ( int nItem )
{
    // To be sure, to be sure
    if ( m_nItem      == nItem   &&
         m_pP3PmsgFoN != nullptr    )
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
    VBLock     *pVBLock = 0;
    VBLockItem *pItem   = 0;
    while ( aItem )
    {
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
LPCTNAM
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
           ->Message(_T("Attempt to push non-P2PmsgItem environment") )
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
           ->Message(_T("Attempt to pop non-pushed environment") )
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
LPCTNAM
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

