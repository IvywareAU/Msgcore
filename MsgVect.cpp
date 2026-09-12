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
#include "MsgList.h"
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
#define  OBJ__Alloc    m_oObject.AllocVBLock
#define pOBJ__Alloc    m_pObject->AllocVBLock
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
//  P3PmsgVect object manager
//  NOTES: Acts as VBlockVect wrapper


//
//  Contructors and destructor
P3PmsgVect::P3PmsgVect ( )
          : P3PmsgField ( 0, 0, 0 )
{
    //RenderThisSafe ( );
}
P3PmsgVect::P3PmsgVect ( int nItems, LPCTNAM lpszName, const P3PmsgData& oData )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = P3PmsgField ( lpszName, oData );        // vect's own name + template data
    P3PmsgField oElem ( L"", oData );             // per-slot value template
    for ( int i = 0; i < nItems; i++ )
      InsertAt ( i, oElem );
}
P3PmsgVect::P3PmsgVect ( const P3PmsgVect& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = rhs;
}
P3PmsgVect::P3PmsgVect ( int nItems, const P3PmsgField& oField )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = oField;                                  // vect name/data from template
    for ( int i = 0; i < nItems; i++ )
      InsertAt ( i, oField );
}
P3PmsgVect::P3PmsgVect ( const P2PmsgListHdl& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
    Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
            , rhs.uiParam3 );
}
P3PmsgVect::P3PmsgVect ( P2PmsgHANDLE hVBList, VBLaddr aVect, VBLsize nVectSize )
          : P3PmsgField ( hVBList, 0, 0 )
{
    m_oObject.Connectx ( hVBList, aVect, nVectSize );
    //RenderThisSafe ( );
    ZeroMemory (  m_pP3PmsgData, sizeof(m_pP3PmsgData) );
    m_nCurs     = 0;
    //m_xVect     = aVect;
    //m_aVect     = aVect;
    //m_nVectSize = nVectSize;
}
P3PmsgVect::P3PmsgVect ( const P3PmsgObject& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = rhs;
}
P3PmsgVect::~P3PmsgVect ( )
{
    // Element blocks live in m_oObject's heap and are released when it closes;
    // here we only reclaim the transient cursor wrappers.
    delete m_pP3PmsgType;
    m_pP3PmsgType = nullptr;
    for ( int i = 0; i < MAX_P3PmsgData_Curs; i++ )
      delete m_pP3PmsgData[i];
}
void
P3PmsgVect::RenderThisSafe ( )
{
    m_pP3PmsgType = 0;
    VBLock& oVBLock = *(VBLock *)m_oObject.m_oVBLock;       // Delegate field object
    // The inline delegate buffer must be sized by its actual byte length, NOT
    // sizeof(VBLock): a vect's aAlloc[32] slab is not part of the VBLock union,
    // so sizeof(VBLock) is too small and would place the trailing field/name/
    // data past the recorded block end (tripping every containment check).
    const VBLsize nBufSize = (VBLsize)sizeof(m_oObject.m_oVBLock);
    ZeroMemory ( &oVBLock, nBufSize );
    m_nCurs = 0;
    ZeroMemory (  m_pP3PmsgData, sizeof(m_pP3PmsgData) );
    VBLock_Init( &oVBLock
               , OBJ__uVBLock|VBLock_Item|VBLock_Linked|VBLock_Alloc, nBufSize );
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(&oVBLock), VBLock_Vect );
    VBLockVect_Init ( OBJ__uVBLock
                    , VBLock_pVect(&oVBLock), 0, AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(&oVBLock), AttrField_DEFAULT );
    VBLockName_Init ( OBJ__uVBLock,VBLock_pName(&oVBLock)
                    , VBLockAttr_DEFAULT | VBLockAttr_NULL
                    , 0, sizeof(VBLockField::oVBLockName) );
    VBLockData_Init ( VBLock_pData(&oVBLock)
                    , VBLockAttr_DEFAULT, VBLockData_NULL
                    , sizeof(VBLockField::oVBLockData) );
ASSERT(VBLockName_Sizenn(OBJ__uVBLock,VBLock_pName(&oVBLock))==sizeof(VBLockField::oVBLockName));
    m_xVect     = 0;
    m_aVect     = reinterpret_cast<VBLaddr>(&oVBLock);
    m_nVectSize = nBufSize;
    m_oObject.Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, nBufSize );
}
///*void
//P3PmsgNode::Reset ( )
//{
//    if ( !m_pVBL  &&
//          m_aNode    )
//      OBJ__Free ( m_aNode );
//    if (  m_pCurs )
//      delete m_pCurs;
//    m_pCurs = 0;
//    __super::Reset ( );
//}*/
void
P3PmsgVect::Connect ( P2PmsgHANDLE hVBList, VBLaddr aVect, VBLsize nVectSize )
{
    //Reset ( );
    //m_xVect     = aVect;
    //m_aVect     = aVect;
    //m_nVectSize = nVectSize;

    // Propagate
    // Audited: P3PmsgField::Connect -> P3PmsgObject::Connectx -> Connecta
    // already sets m_aVBLock (=aVect) and m_nVBLockSize (=nVectSize) - and also
    // m_xVBLock - so the former direct assignments here were redundant.
    P3PmsgField::Connect ( hVBList, aVect, nVectSize );
}
/*UINT
P3PmsgVect::GetHeadPos ( ) const
{
    VBLockList *pList = GetVBLockList ( );
    return VBLockList_GetFirst ( m_uVBLock, pList );
}*/
/*P3PmsgData&
P3PmsgVect::GetNext ( UINT& aPos )
{
    VBLock     *pVBLock = (VBLock *)Msg2Phys(aPos);
ASSERT(VBLock_IsLinked(pVBLock));
ASSERT(VBLock_IsAlloc(pVBLock));
    VBLockItem *pItem   = VBLock_pItem(pVBLock);

    // Instantiate
    m_nCurs = (m_nCurs + 1) % MAX_P3PmsgData_Curs;
    if ( m_pP3PmsgData[m_nCurs] == 0 )
      m_pP3PmsgData[m_nCurs] = new P3PmsgData ( );
    m_pP3PmsgData[m_nCurs]->Connect ( m_hVBList, aPos, 0 );

    // Tidy up, and
    aPos = VBLockItem_GetNext ( m_uVBLock, pItem );
    return *m_pP3PmsgData[m_nCurs];
}*/
/*P3PmsgVect&
P3PmsgVect::AddListHead ( const P3PmsgData& oData )
{
    // Locals
    UINT  aItem__;
    UINT  nSizeofItem;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( m_uVBLock )
                + oData.P3PmsgData::Sizeof();
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock *pVBLock = (VBLock *)Msg2Phys(aItem__);
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oData.DataType()
                    , oData.P3PmsgData::Sizeof() );

    // Copy
    VBLockData_Copy ( VBLock_pData(pVBLock), oData.P3PmsgData::Sizeof()
                    , oData.GetVBLockData(), oData.P3PmsgData::Sizeof() );

    // Prepend P3PmsgData item
    VBLockList *pList   = GetVBLockList();
    P2PmsgList_LinkinItem ( this
                          , 0
                          , aItem__
                          , VBLockList_GetFirst(m_uVBLock,pList) );
    return *this;
}*/

/*P3PmsgData&
P3PmsgList::GetTail ( )
{
    VBLockList *pList = GetVBLockList ( );
    UINT aTail = VBLockList_GetLast ( m_uVBLock, pList );

    // Instantiate
    m_nCurs = (m_nCurs + 1) % MAX_P3PmsgData_Curs;
    if ( m_pP3PmsgData[m_nCurs] == 0 )
      m_pP3PmsgData[m_nCurs] = new P3PmsgData ( );
    m_pP3PmsgData[m_nCurs]->Connect ( m_hVBList, aTail, 0 );
    return *m_pP3PmsgData[m_nCurs];
}*/
/*UINT
P3PmsgList::GetTailPos ( ) const
{
    VBLockList *pList = GetVBLockList ( );
    return VBLockList_GetLast ( m_uVBLock, pList );
}*/
/*P3PmsgVect&
P3PmsgVect::AddListTail ( const P3PmsgData& oData )
{
    // Locals
    UINT  aItem__;
    UINT  nSizeofItem;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( m_uVBLock )
                + oData.P3PmsgData::Sizeof();
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock *pVBLock = (VBLock *)Msg2Phys(aItem__);
    VBLockItem_Init ( m_uVBLock, VBLock_pItem(pVBLock), VBLock_Data );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oData.DataType()
                    , oData.P3PmsgData::Sizeof() );

    // Copy
    VBLockData_Copy ( VBLock_pData(pVBLock), oData.P3PmsgData::Sizeof()
                    , oData.GetVBLockData(), oData.P3PmsgData::Sizeof() );

    // Prepend P3PmsgData item
    VBLockList *pList   = GetVBLockList();
    P2PmsgList_LinkinItem ( this
                          , VBLockList_GetLast(m_uVBLock,pList)
                          , aItem__
                          , 0 );
ASSERT(VBLockList_GetLast(this->m_uVBLock,pList,0)==aItem__);
ASSERT(VBLock_IsItem(pVBLock));
ASSERT(VBLock_IsLinked(pVBLock));
    return *this;
}*/
//void
//P3PmsgList::DropHead ( )
//{
//    VBLockList *pList = GetVBLockList ( );
//    UINT        aItem = VBLockList_GetFirst ( m_uVBLock,pList );
//    P2PmsgList_UnLinkItem ( this, pList, aItem );
//}

//
//P3PmsgField&
//P3PmsgNode::AddField( LPCTSTR lpszName, P3PmsgData& oData )
//{
//  (*this) += P3PmsgField(lpszName, oData );
//    return *this;
//}

//UINT
//P3PmsgVect::GetCount ( )
//{
//    VBLockVect *pVect = GetVBLockVect ( );
//    return VBLockVect_GetItems ( m_uVBLock, pVect );
//}


//  Operators
P3PmsgVect&
P3PmsgVect::operator = ( const P3PmsgVect& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    Truncate ( );                      // Empties existing content

    // Copy the field-level identity (name/data/attr/desc); this leaves the
    // (now empty) vect header intact, exactly as P3PmsgList::operator= relies on.
    dynamic_cast<P3PmsgField&>(*this) = rhs;

    // Recursively copy each element by index.  rhs owns a separate cursor, so
    // its r_item() references stay valid across our InsertAt()/Goto() calls.
    VBLelem nItems = ((P3PmsgVect&)rhs).GetCount();
    for ( VBLelem i = 0; i < nItems; i++ )
      InsertAt ( i, ((P3PmsgVect&)rhs).r_item(i) );

    // Tidy up, and
    m_bVectDirty = true;
ASSERT(r_Object().IsVect());
    return *this;
}
P3PmsgVect&
P3PmsgVect::operator = ( const P3PmsgField& rhs )
{
    dynamic_cast<P3PmsgField&>(*this) = rhs;
ASSERT(r_Object().IsVect());
    return *this;
}
P3PmsgVect&
P3PmsgVect::operator = ( const P3PmsgObject& rhs )
{
    if ( &r_Object() == &rhs )
      return *this;
    m_oObject = rhs;
ASSERT(r_Object().IsVect());
    return *this;
}

//P3PmsgNode&
//P3PmsgNode::operator += ( const P3PmsgNode& rhs )
//{
//    //VBLock     *pVBLockThis = GetVBLock ( );
//    UINT        nSizeofNode = rhs.Sizeof ( m_uVBLock );
//    UINT        nSizeofItem;
//    UINT        aItem__;
//
//    // Establish placeholder item
//    nSizeofItem = VBLockItem_Sizeof ( m_uVBLock )
//                + rhs.P3PmsgNode::Sizeof ( m_uVBLock );
//    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
//    VBLock     *pVBLock = (VBLock *)Msg2Phys(aItem__);
//    //VBLockNode *pNode   = GetVBLockNode();
//    VBLockItem_Init ( m_uVBLock, VBLock_pItem(pVBLock), VBLockItem_Node );
//    VBLockNode_Init ( m_uVBLock, VBLock_pNode(pVBLock), VBLockAttr_DEFAULT );
//    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
//    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
//                    , rhs.c_name(), rhs.P3PmsgName::Sizeof() );
//    VBLockData_Init ( VBLock_pData(pVBLock)
//                    , VBLockAttr_DEFAULT, rhs.DataType()
//                    , rhs.P3PmsgData::Sizeof() );
//    P2PmsgNode_LinkinItem ( this
//                          , VBLockNode_GetLast(GetVBLock()->oHdr.uVBLock,GetVBLockNode())
//                          , aItem__
//                          , 0 );
//
//    // Recursive copy
//    P3PmsgNode oNode ( m_hVBList, aItem__, nSizeofItem );
//               oNode = (P3PmsgField&)rhs;
//ASSERT(oNode.Sizeof(m_uVBLock)==rhs.Sizeof(m_uVBLock));
//    P3PmsgCurs& oCurs = ((P3PmsgNode&)rhs).GetCurs();
//    for ( int i = 0; oCurs.Goto(i); i++ )
//    {
//      if ( oCurs.IsNode() )
//        oNode += oCurs.r_node();
//      else if ( oCurs.IsField() )
//        oNode += oCurs.r_field();
//    }
//
//    // Tidy up, and
//AssertValid();//TODO:LJM delete, testing
//rhs.AssertValid();//TODO:LJM delete, testing
//    return *this;
//}
//
//#define pVBLockTHIS ((VBLock *)Msg2Phys(this->m_aNode))
//
//P3PmsgNode&
//P3PmsgNode::operator += ( const P3PmsgField& rhs )
//{
//ASSERT(!rhs.IsNode());
//    //VBLock     *pVBLockThis = (VBLock *)Msg2Phys ( m_aNode );
//    UINT        nSizeofItem;
//    UINT        aItem__;
//
//    // Allocation
//    nSizeofItem = VBLockItem_Sizeof ( pVBLockTHIS->oHdr.uVBLock )
//                + rhs.P3PmsgField::Sizeof();
//    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
//    VBLock *pVBLock = (VBLock *)Msg2Phys(aItem__);
//    VBLockItem_Init ( m_uVBLock, VBLock_pItem(pVBLock), VBLockItem_Field );
//    VBLockField_Init( VBLock_pField(pVBLock), AttrField_DEFAULT );
//    VBLockName_Init ( VBLock_pName(pVBLock), VBLockAttr_DEFAULT
//                    , rhs.c_name(), rhs.P3PmsgName::Sizeof() );
//    VBLockData_Init ( VBLock_pData(pVBLock)
//                    , VBLockAttr_DEFAULT, rhs.DataType()
//                    , rhs.P3PmsgData::Sizeof() );
//    //VBLock_pItem(pVBLock) -> uVBLockItem = VBLockItem_Field;
//    //VBLock_pName(pVBLock) -> u.vBlob08.nBlobSize = rhs.c_size();
//    //VBLock_pName(pVBLock) -> u.vBlob08.nBlobUsed = rhs.c_size();
//ASSERT(VBLockItem_Sizenn(pVBLock)==nSizeofItem);
//    VBLockNode *pNode   = GetVBLockNode();
//    P2PmsgNode_LinkinItem ( this
//                          , VBLockNode_GetLast(pVBLockTHIS->oHdr.uVBLock,pNode)
//                          , aItem__
//                          , 0 );
//      //aField = aItem__ + VBLockItem_uos(pVBLock->oHdr.uVBLock,VBLock_pItem(pItem));
//
//    P3PmsgField oField ( m_hVBList, aItem__, nSizeofItem );
//    LPCTSTR lpszName = oField.c_name();
//oField.AssertValid();
//                oField = rhs;
//oField.AssertValid();
//    LPCTSTR lpszName1 = oField.c_name();
//AssertValid();
//rhs.AssertValid();
//    return *this;
//}
//P3PmsgVect&
//P3PmsgVect::operator += ( const P3PmsgData& rhs )
//{
//    // Delegate
//    AddListTail ( rhs );
//    return *this;
//}


//  Does this vector denote an item?  (Refer P3PmsgObject::operator bool)
//  NOTES: Inverted, as P3PmsgList::operator bool was, and equally uncalled.
P3PmsgVect::operator bool ( )
{
    return !IsVoid ( );
}

//P3PmsgField&
//P3PmsgNode::operator [] ( LPCTSTR lpszItemName )
//{
//    if ( !m_pCurs )
//      m_pCurs = new P3PmsgCurs ( *this );
//    if ( !m_pCurs->Goto(lpszItemName) )
//      EVERR -> Module ( __FUNCTION__"(%s)", lpszItemName )
//            -> Message(L"Item [%s] does not exist", lpszItemName )
//            -> Throw();
//    if ( m_pCurs->IsField() )
//      return m_pCurs->r_field ( );
//    return m_pCurs->r_node ( );
//}
//
////
////  Navigation
//
//P3PmsgNode&
//P3PmsgNode::SelectNode ( LPCTSTR lpszNodeName )
//{
//    if ( !m_pCurs )
//      m_pCurs = new P3PmsgCurs ( *this );
//    if ( !m_pCurs->Goto(lpszNodeName) ||
//         !m_pCurs->IsNode()              )
//    {
//      ASSERT(0);//TODO:Delete-me
//      EVERR -> Module ( __FUNCTION__"(%s)", lpszNodeName )
//            -> Message(L"Node [%s] does not exist", lpszNodeName )
//            -> Throw();
//    }
//    return m_pCurs->r_node ( );
//}
//
//bool
//P3PmsgNode::Exists ( LPCTSTR lpszItemName )
//{
//    if ( !m_pCurs )
//      m_pCurs = new P3PmsgCurs ( *this );
//    return m_pCurs->Goto(lpszItemName);
//}
//
//
//  Free the sub-allocations owned by an element block before it is released.
//  A data/field element is a single contiguous VBLock (name+data embedded),
//  so freeing the block suffices; a nested list/vect owns separately-allocated
//  child blocks that must be truncated first to avoid leaks.
void
P3PmsgVect::FreeElemDeep ( VBLaddr aElem )
{
    if ( !aElem )
      return;
    VBLock     *pElem = (VBLock *)OBJ__Msg2Phys ( aElem );
    VBLockItem *pItem = VBLock_pItem ( pElem );
    if ( VBLockItem_IsList(pItem) )
    { P3PmsgList oChild ( OBJ__hVBList, aElem, 0 ); oChild.Truncate(); }
    else if ( VBLockItem_IsVect(pItem) )
    { P3PmsgVect oChild ( OBJ__hVBList, aElem, 0 ); oChild.Truncate(); }
    OBJ__Free ( aElem );
}

bool
P3PmsgVect::Delete ( VBLelem nElem )
{
    VBLelem nItems = GetCount ( );
    if ( nElem < 0 || nElem >= nItems )
      return false;

    FreeElemDeep ( GetSlot(nElem) );

    // Compact the logical slot sequence left over the hole (chain-aware).
    for ( VBLelem i = nElem; i < nItems-1; i++ )
      SetSlot ( i, GetSlot(i+1) );
    SetSlot ( nItems-1, 0 );

    VBLockVect *pHead = VBLock_pVect ( ptrVBLOCK(m_oObject) );
    VBLockVect_SetItems ( OBJ__uVBLock, pHead, nItems-1 );   // total count in head

    delete m_pP3PmsgType; m_pP3PmsgType = nullptr;
    m_bVectDirty = true;
    return true;
}
void
P3PmsgVect::Truncate ( )
{
    // Cursors
    delete m_pP3PmsgType; m_pP3PmsgType = nullptr;
    for ( int i = 0; i < MAX_P3PmsgData_Curs; i++ )
      { delete m_pP3PmsgData[i]; m_pP3PmsgData[i] = nullptr; }

    // Drop every element block (chain-aware) ...
    VBLelem nItems = GetCount ( );
    for ( VBLelem i = 0; i < nItems; i++ )
      FreeElemDeep ( GetSlot(i) );

    // ... then release the aExtra continuation blocks and unchain them.
    VBLockVect *pHead = VBLock_pVect ( ptrVBLOCK(m_oObject) );
    VBLaddr aCont = VBLockVect_GetExtra ( OBJ__uVBLock, pHead );
    while ( aCont )
    {
      VBLockVect *pCont = VBLock_pVect ( (VBLock *)OBJ__Msg2Phys(aCont) );
      VBLaddr     aNext = VBLockVect_GetExtra ( OBJ__uVBLock, pCont );
      OBJ__Free ( aCont );
      aCont = aNext;
    }
    pHead = VBLock_pVect ( ptrVBLOCK(m_oObject) );
    VBLockVect_SetExtra ( OBJ__uVBLock, pHead, 0 );
    VBLockVect_SetItems ( OBJ__uVBLock, pHead, 0 );
    m_bVectDirty = true;
}

//P3PmsgCurs&
//P3PmsgNode::GetCurs ( )
//{
//    if ( !m_pCurs )
//      m_pCurs = new P3PmsgCurs ( *this );
//    return *m_pCurs;
//}
//

//
//  Memory management
//  NOTES: P3PmsgList has had this since it was written; P3PmsgVect never did,
//         so a vect fell through to P3PmsgField::Drop, which opens with
//         ASSERT(OBJ__IsField()) -- false for a vect -- and then frees the item
//         block WITHOUT Truncate(), leaking every element block and every
//         aExtra continuation behind it. Nothing had dropped a vect before:
//         P3PmsgDesc and P3PmsgAttr drop their children through the base class,
//         and no vect had ever been a pushed stack generation because
//         MsgStck::Push asserted for one. Popping a vect is what needed it.
//       : The body is P3PmsgList::Drop's, with Truncate() doing the
//         type-specific part -- it frees the element blocks through
//         FreeElemDeep and then unchains the aExtra continuations.
//       : THE TYPE TEST THROWS RATHER THAN ASSERTS, and the reason is the
//         first note above. P3PmsgField::Drop already had the same test as
//         ASSERT(OBJ__IsField()), and it did not stop a vect being dropped
//         through it, because an ASSERT is not in the binary that did the
//         dropping. Repeating the pattern one level down would buy exactly
//         as much. It is not decoration either: Truncate() reads the element
//         slot array through VBLock_pVect and hands what it finds to Free, so
//         on a block that is not a vect those are arbitrary bytes taken as
//         addresses. That matters in Release, which is the whole of item 19.
void
P3PmsgVect::Drop ( )
{
    if ( !r_Object().IsVect() )
      EVERR->MODULE
           ->Message(L"Drop on a P3PmsgVect whose block is not a vect")
           ->Throw();
    if ( IsAttributed() )
      r_Attr().Drop();
    if ( IsDescendant() )
      r_Desc().Drop();
    if ( IsStacked() )
      r_Stck().Drop();

    P2PmsgField_DropName ( this );
    P2PmsgField_DropData ( this );
    Truncate ( );

    // Isolate parent
    VBLock *pVBLockParent = P2PmsgField_GetVBLockParent ( this );
    if ( pVBLockParent )
    {
      if ( VBLock_IsAttr(pVBLockParent) )
      {
        VBLockAttr *pAttrParent = VBLock_pAttr(pVBLockParent);
        P2PmsgAttr_UnLinkItem ( &m_oObject, pAttrParent, OBJ__VBLocknn );
      }
      else if ( VBLock_IsDesc(pVBLockParent) )
      {
        VBLockDesc *pDescParent = VBLock_pDesc(pVBLockParent);
        P2PmsgDesc_UnLinkItem ( &m_oObject, pDescParent, OBJ__VBLocknn );
      }
      else
        ASSERT(0);
    }

    // Tidy up, and
    OBJ__Free ( OBJ__aVBLock );
}

//
//  Allocate a fresh element item block and deep-copy oField into it.  The
//  returned VBLock address is stored in the vect's aAlloc[] slot array.
VBLaddr
P3PmsgVect::AllocElem ( const P3PmsgField& oField )
{
    const P3PmsgObject& oObj = oField.r_Object();
    VBLaddr aItem;
    if ( oObj.IsList() )
    {
      P3PmsgList oSrc  ( oObj );
      VBLsize    nSize = P2PmsgList_SizeofItem ( OBJ__uVBLock, oSrc );
      aItem = OBJ__Alloc ( VBLock_Item, nSize );
      P2PmsgList_InitItem ( (VBLock *)OBJ__Msg2Phys(aItem), oSrc );
      MarkElemLinked ( aItem );
      P3PmsgList oElem ( OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) );
                 oElem = oSrc;
    }
    else if ( oObj.IsVect() )
    {
      P3PmsgVect oSrc  ( oObj );
      VBLsize    nSize = P2PmsgVect_SizeofItem ( OBJ__uVBLock, oSrc );
      aItem = OBJ__Alloc ( VBLock_Item, nSize );
      P2PmsgVect_InitItem ( (VBLock *)OBJ__Msg2Phys(aItem), oSrc );
      MarkElemLinked ( aItem );
      P3PmsgVect oElem ( OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) );
                 oElem = oSrc;
    }
    else
    {
      VBLsize nSize = P2PmsgField_SizeofItem ( OBJ__uVBLock, oField, TRUE );
      aItem = OBJ__Alloc ( VBLock_Item, nSize );
      P2PmsgItem_InitField ( (VBLock *)OBJ__Msg2Phys(aItem), oField );
      MarkElemLinked ( aItem );
      P3PmsgField oElem ( OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) );
                  oElem = oField;
    }
    return aItem;
}

//
//  Element blocks are addressed directly through aAlloc[] rather than linked
//  into a chain, so mark them VBLock_Linked (the list gets this via
//  P2PmsgList_LinkinItem).  Must run BEFORE the deep-copy assignment, which
//  itself validates the block through the heap's allocation invariants.
void
P3PmsgVect::MarkElemLinked ( VBLaddr aElem )
{
    ((VBLock *)OBJ__Msg2Phys(aElem))->oHdr.uVBLockDefs |= VBLock_Linked;
}

//
//  Allocate an empty aExtra continuation block (another vect header whose
//  aAlloc[] holds the next VBLockVect_MaxInline element addresses).
VBLaddr
P3PmsgVect::AllocContinuation ( )
{
    P3PmsgVect oEmpty ( 0, L"", P3PmsgData() );   // valid, element-free template
    VBLsize nSize = P2PmsgVect_SizeofItem ( OBJ__uVBLock, oEmpty );
    VBLaddr aItem = OBJ__Alloc ( VBLock_Item, nSize );
    P2PmsgVect_InitItem ( (VBLock *)OBJ__Msg2Phys(aItem), oEmpty );
    MarkElemLinked ( aItem );
    return aItem;
}

//
//  Resolve the element address at a logical index, walking the aExtra chain.
//  Returns 0 if the slot has no continuation block (i.e. never populated).
VBLaddr
P3PmsgVect::GetSlot ( int nElem )
{
    VBLockVect *pVect = VBLock_pVect ( ptrVBLOCK(m_oObject) );
    while ( nElem >= VBLockVect_MaxInline )
    {
      VBLaddr aNext = VBLockVect_GetExtra ( OBJ__uVBLock, pVect );
      if ( !aNext )
        return 0;
      pVect  = VBLock_pVect ( (VBLock *)OBJ__Msg2Phys(aNext) );
      nElem -= VBLockVect_MaxInline;
    }
    return VBLockVect_GetAlloc ( OBJ__uVBLock, pVect, nElem );
}

//
//  Store an element address at a logical index, allocating continuation
//  blocks on demand.  Heap/IOMAGE physical pointers are only valid per
//  Msg2Phys call, so the block is re-resolved by address at each hop.
void
P3PmsgVect::SetSlot ( int nElem, VBLaddr aElem )
{
    VBLaddr aBlock = 0;                       // 0 == head (inline aAlloc[])
    while ( nElem >= VBLockVect_MaxInline )
    {
      VBLockVect *pVect = aBlock ? VBLock_pVect((VBLock *)OBJ__Msg2Phys(aBlock))
                                 : VBLock_pVect(ptrVBLOCK(m_oObject));
      VBLaddr aNext = VBLockVect_GetExtra ( OBJ__uVBLock, pVect );
      if ( !aNext )
      {
        aNext = AllocContinuation ( );        // may create the object heap
        pVect = aBlock ? VBLock_pVect((VBLock *)OBJ__Msg2Phys(aBlock))
                       : VBLock_pVect(ptrVBLOCK(m_oObject));
        VBLockVect_SetExtra ( OBJ__uVBLock, pVect, aNext );
      }
      aBlock = aNext;
      nElem -= VBLockVect_MaxInline;
    }
    VBLockVect *pVect = aBlock ? VBLock_pVect((VBLock *)OBJ__Msg2Phys(aBlock))
                               : VBLock_pVect(ptrVBLOCK(m_oObject));
    VBLockVect_SetAlloc ( OBJ__uVBLock, pVect, nElem, aElem );
}

P3PmsgField&
P3PmsgVect::InsertAt ( int nElem, const P3PmsgField& oField )
{
    VBLelem nItems = GetCount ( );
    if ( nElem < 0 || nElem > nItems )
      nElem = nItems;                        // clamp out-of-range to append

    VBLaddr aElem = AllocElem ( oField );

    // Open a gap at nElem by shifting the logical slot sequence right; SetSlot
    // spills past the inline aAlloc[32] into aExtra continuation blocks.
    for ( VBLelem i = nItems; i > nElem; i-- )
      SetSlot ( i, GetSlot(i-1) );
    SetSlot ( nElem, aElem );

    VBLockVect *pHead = VBLock_pVect ( ptrVBLOCK(m_oObject) );
    VBLockVect_SetItems ( OBJ__uVBLock, pHead, nItems+1 );   // total count in head
    m_bVectDirty = true;

    Goto ( nElem );
    return *m_pP3PmsgType;
}

VBLelem
P3PmsgVect::GetCount ( ) const
{
    VBLockVect *pVect = VBLock_pVect ( ptrVBLOCK(m_oObject) );
    return VBLockVect_GetItems ( OBJ__uVBLock, pVect );
}

int
P3PmsgVect::Goto ( VBLelem nElem )
{
    VBLelem nItems = GetCount ( );
    if ( nElem < 0 || nElem >= nItems )
    {
      delete m_pP3PmsgType; m_pP3PmsgType = nullptr;
      return 0;
    }
    VBLaddr aElem = GetSlot ( nElem );
    if ( !aElem )
    {
      delete m_pP3PmsgType; m_pP3PmsgType = nullptr;
      return 0;
    }

    // Instantiate a cursor wrapper of the element's own kind so that the
    // dynamic_cast in r_list()/r_vect() succeeds for nested containers.
    VBLockItem *pItem = VBLock_pItem ( (VBLock *)OBJ__Msg2Phys(aElem) );
    delete m_pP3PmsgType; m_pP3PmsgType = nullptr;
    if ( VBLockItem_IsList(pItem) )
      m_pP3PmsgType = new P3PmsgList ( );
    else if ( VBLockItem_IsVect(pItem) )
      m_pP3PmsgType = new P3PmsgVect ( );
    else
      m_pP3PmsgType = new P3PmsgField ( );
    m_pP3PmsgType->Connect ( OBJ__hVBList, aElem, 0 );
    m_nElem = nElem;
    return 1;
}

/////////////////////////////////////////////////////////////////////////
////  Memory management
////  NOTES: Delegates through P2PmsgVBL if not-NULL, otherwise interacts
////         directly with heap
//UINT
//P3PmsgVect::Free ( UINT aVBLockAddr )
//{
//    ASSERT(IsVect());
//    OBJ__Free ( aVBLockAddr );
//    //if ( aVBLockAddr == 0 )
//    //  return 0;
//    //else if ( aVBLockAddr == m_xVect )
//    //  m_xVect = 0;
//    //else if ( OBJ__hVBList )
//    //  VBListFree ( OBJ__hVBList, aVBLockAddr );
//    //else
//    //{
//    //  if ( aVBLockAddr != reinterpret_cast<UINT>(&m_oVect) )
//    //    delete [] (char *)aVBLockAddr;
//    //}
//    //if ( aVBLockAddr == m_aVect )
//    //  m_aVect = 0;
//    return 0;
//}

// Addressing and allocations
//VBLock*
//P3PmsgVect::GetVBLock ( bool ) const
//{
//    ASSERT(m_aVect==OBJ__aVBLock);
//    return m_oObject.GetVBLock();
//    //return (VBLock *)Msg2Phys ( m_aVect );
//}
//UINT
//P3PmsgVect::GetVBLocknn( ) const
//{
//    ASSERT(IsVect());
//    return m_oObject.GetVBLocknn();
//    //return m_aVect;
//}
//VBLsize
//P3PmsgVect::GetVBLockSize ( ) const
//{
//    return m_oObject.GetVBLockSize();
//    //return m_nVectSize;
//}
/*VBLockList*
P3PmsgList::NewVBLockList ( int nSizeof )
{
ASSERT(0);
    m_aList     = OBJ__Free  ( m_aList );
    m_aList     = OBJ__Alloc ( VBLock_List, nSizeof );
    m_nListSize = nSizeof;
    VBLockList *pList = GetVBLockList ( );
                pList -> uVBLockAttr = 0;
    ASSERT(pList==GetVBLockList());
    return pList;
}*/
//VBLockVect*
//P3PmsgVect::GetVBLockVect ( bool ) const
//{
//    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys ( m_aVect );
//    return VBLock_pVect ( pVBLock );
//}

VBLsize
P3PmsgVect::GetVBLockVectSize ( ) const
{
    return m_oObject.m_nVBLockSize;
    //return m_nVectSize;
}
//VBLockField*
//P3PmsgVect::GetVBLockField ( bool /*bIndirect*/ ) const
//{
//    //VBLock *pVBLock = (VBLock *)Msg2Phys ( m_aVect );
//    VBLock *pVBLock = m_oObject.GetVBLock();
//    ASSERT(VBLock_IsLinked(pVBLock));
//    ASSERT(VBLock_IsAlloc(pVBLock));
//    return VBLock_pField ( pVBLock );
//}
//UINT
//P3PmsgVect::GetVBLockFieldSize ( ) const
//{
//    ASSERT(0);
//    return 0;
//    //VBLock *pVBLock = (VBLock *)Msg2Phys ( m_aList );
//    //UINT    nSizenn = VBLockField_Sizenn ( pVBLock );
//    //return m_nListSize -   P3PmsgList::Sizeof_u()
//    //                   - ( sizeof(m_oList) 
//    //                     -   sizeof(m_oList.ud)
//    //                     -   sizeof(m_oList.ud.oField) );
//}

/////////////////////////////////////////////////////////////////////////
//  Troubleshooting
void
P3PmsgVect::AssertValid ( ) const
{
    //VBLock     *pVBLock = OBJ__VBLock; //GetVBLock ( );
    //VBLockVect *pVect   = VBLock_pVect ( pVBLock );
    ASSERT(r_Object().IsVect());

    // Header
    //VBLelem  nItems = VBLockVect_GetItems ( OBJ__uVBLock, pVect );
    //int  nFirst;
    /*UINT aFirst = VBLockVect_GetFirst ( m_uVBLock, pVect, &nFirst );
    int  nLast;
    UINT aLast  = VBLockList_GetLast  ( m_uVBLock, pVect, &nLast );
    if ( !nItems && (nFirst || aFirst || nLast!=-1 || aLast) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted header nItems=%i aFirst=%i aLast=%i",
                        nItems, aFirst, aLast )
            -> Throw();*/

    // Delegation
    __super::AssertValid ( );

    //// Navigation
    //P3PmsgCurs& oCurs = ((P3PmsgNode *)this)->GetCurs();
    //for ( int i = 0; oCurs.Goto(i); i++ )
    //{
    //  if ( oCurs.IsNode() )
    //    oCurs.r_node().AssertValid();
    //  else if ( oCurs.IsField() )
    //    oCurs.r_field().AssertValid();
    //}
    //if ( nItems != i )
    //  EVERR -> Module ( __FUNCTION__ )
    //        -> Message("Corrupted item count actual=%i expected=%i",
    //                    nItems, i )
    //        -> Throw();
}
//
//  Verifies P3PmsgVect object containment and optionally containment of
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
P3PmsgVect::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // pVBlob containment within VBLock
    VBLock     *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pvBlob != nullptr && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;                    // pVBlob not contained within VBLock

    // VBLockAttr containment
    VBLockVect *pVect       = VBLock_pVect ( pVBLock );
    VBLsize     nSizeofAttr = VBLockVect_Sizeof ( OBJ__uVBLock );
    if ( !VBLock_IsContainedVBLump(pVBLock,pVect,nSizeofAttr) )
      return FALSE;                    // pData not contained within VBLock

    // Delegate through to P3PmsgField
    P3PmsgField::VerifyContainment();

    // Tidy up and
    return TRUE;
}

void
P3PmsgVect::Print ( FILE *fd, int nDepthOS, int nDepthOSinc  )
{
    // P2PmsgNode details
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"%*s", nDepthOSinc, L"  " );
    VBLsize nVBLockSize = Sizeof(OBJ__uVBLock);
    P3Pmsg_fwprintf ( fd, L"%s(%Ii) = (%s %s) %s {\n"
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

    // Tidy up
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    P3Pmsg_fwprintf ( fd, L"}\n" );
}
//
//  Exposure
P3PmsgData&
P3PmsgVect::r_data ( ) const
{
    return (P3PmsgData&)*this;
}
P3PmsgData&
P3PmsgVect::r_data ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return *m_pP3PmsgType;
}

P3PmsgName&
P3PmsgVect::r_name ( ) const
{
    return (P3PmsgName&)*this;
}
P3PmsgName&
P3PmsgVect::r_name ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return *m_pP3PmsgType;
}

P3PmsgList&
P3PmsgVect::r_list ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgType->r_Object().IsList() )
      EVERR->MODULE
           ->Message("Not a P2PmsgList type object" )
           ->Throw ( );
    return dynamic_cast<P3PmsgList&>(*m_pP3PmsgType);
}

P3PmsgVect&
P3PmsgVect::r_vect ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgType->r_Object().IsVect() )
      EVERR->MODULE
           ->Message("Not a P2PmsgVect type object" )
           ->Throw ( );
    return dynamic_cast<P3PmsgVect&>(*m_pP3PmsgType);
}
P3PmsgItem&
P3PmsgVect::r_item ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    return *m_pP3PmsgType;
}
/*P3PmsgNode&
P3PmsgVect::r_node ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE
           ->Message("Null cursor")
           ->Throw  ( );
    if ( !m_pP3PmsgType->r_Object().IsNode() )
      EVERR->MODULE
           ->Message("Not a P2PmsgNode type object" )
           ->Throw ( );
    return dynamic_cast<P3PmsgNode&>(*m_pP3PmsgType);
}*/

///////////////////////////////////////
// Properties

P2PmsgVectHdl
P3PmsgVect::GetP2PmsgVectHdl ( )
{
    ASSERT(r_Object().IsVect());
    VBLaddr aVect = OBJ__aVBLock;
    P2PmsgVectHdl oHdl = { (UINT_PTR)OBJ__hVBList, aVect, P2PmsgHeap_Sizeof(OBJ__hVBList,aVect) };
    //P2PmsgVectHdl oHdl = { (UINT_PTR)OBJ_hVBList, m_aVect, P2PmsgHeap_Sizeof(OBJ__hVBList,m_aVect) };
    return oHdl;
}
VBLsize
P3PmsgVect::Sizeof ( UCHAR uVBLock ) const
{
    VBLsize nSizeof = VBLockVect_Sizeof ( uVBLock )
                    + P3PmsgField::Sizeof ( );
    return nSizeof;
}
bool
P3PmsgVect::IsDirty ( )
{
    if ( m_bVectDirty           ||
         P3PmsgField::IsDirty()    )
      return true;
    return false;
}
//
//  References on hVBList held by this vect and everything it owns
//  NOTES: THE FIELD'S WALK, PLUS THE ELEMENT CURSOR. §31 asked for the two
//         proofs before anything here could be subtracted, and m_pP3PmsgType
//         carries both: Goto news it -- as a P3PmsgList, a P3PmsgVect or a
//         P3PmsgField, whichever the element is -- deletes the previous one on
//         the way, and ~P3PmsgVect, Delete and Truncate delete it; it is never
//         assigned a pointer from anywhere else. Goto then Connect()s it on
//         OBJ__hVBList, which AddRefs. So there is at most one of them and it
//         is this vect's own.
//       : AND IT IS WALKED AS THE ELEMENT'S OWN KIND, which is why
//         P3PmsgData::HeapHolders is virtual. m_pP3PmsgType is declared
//         P3PmsgField* and a nested container puts a P3PmsgList or a
//         P3PmsgVect in it; those hold cursors of their own one level further
//         in, and a compile-time call would stop above them.
//       : m_pP3PmsgData[] HOLDS NOTHING TODAY, and this is a measurement rather
//         than an assumption. Every site that would fill it -- the vect's own
//         GetNext and GetTail -- is inside a comment block, so the array is
//         zeroed at construction and deleted as nullptrs. It is walked with the
//         element cursor because the count is exact either way, and because a
//         vect that grows those readers back would otherwise go quietly wrong
//         in the unsafe direction.
//       : m_pP3PmsgField on the collections a field owns points back UP and is
//         not followed. Refer P3PmsgField::HeapHolders.
int
P3PmsgVect::HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept
{
    if ( hVBList == 0 )
      return 0;

    int nHolders = P3PmsgField::HeapHolders ( hVBList );
    if ( m_pP3PmsgType != nullptr )
      nHolders += m_pP3PmsgType -> HeapHolders ( hVBList );
    for ( int i = 0; i < MAX_P3PmsgData_Curs; i++ )
      if ( m_pP3PmsgData[i] != nullptr )
        nHolders += m_pP3PmsgData[i] -> HeapHolders ( hVBList );
    return nHolders;
}
///*      virtual bool
//        IsEmpty ( ) const = 0;*/
//bool
//P3PmsgVect::IsVect ( ) const
//{
//    return true;
//}

bool
P3PmsgVect::IsField ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE->AFP(nElem)
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgType->r_Object().IsField() ? true : false;
}

//bool
//P3PmsgVect::IsNode ( int nElem )
//{
//    Goto ( nElem );
//    if ( m_pP3PmsgType == nullptr )
//      EVERR->MODULE->AFP(nElem)
//           ->Message("Null cursor")
//           ->Throw  ( );
//    return m_pP3PmsgType->r_Object().IsNode() ? true : false;
//}

bool
P3PmsgVect::IsList ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE->AFP(nElem)
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgType->r_Object().IsList() ? true : false;
}

bool
P3PmsgVect::IsVect ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE->AFP(nElem)
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgType->r_Object().IsVect() ? true : false;
}
bool
P3PmsgVect::IsData ( int nElem )
{
    Goto ( nElem );
    if ( m_pP3PmsgType == nullptr )
      EVERR->MODULE->AFP(nElem)
           ->Message("Null cursor")
           ->Throw  ( );
    return m_pP3PmsgType->r_Object().IsData() ? true : false;
}
