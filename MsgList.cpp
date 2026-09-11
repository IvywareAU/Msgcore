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
//  P3PmsgList object manager
//  NOTES: Acts as VBlockList wrapper

//
//  Contructors and destructor
P3PmsgList::P3PmsgList ( )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
}
P3PmsgList::P3PmsgList ( LPCTNAM lpszName, const P3PmsgData& oData )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = P3PmsgField ( lpszName, oData );
}
P3PmsgList::P3PmsgList ( const P3PmsgList& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = rhs;
}
P3PmsgList::P3PmsgList ( const P3PmsgField& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = rhs;
}
P3PmsgList::P3PmsgList ( const P2PmsgListHdl& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
    Connect ( (P2PmsgHANDLE)rhs.uiParam1, rhs.uiParam2
            , rhs.uiParam3 );
}
P3PmsgList::P3PmsgList ( const P3PmsgObject& rhs )
          : P3PmsgField ( 0, 0, 0 )
{
    RenderThisSafe ( );
  (*this) = rhs;
}
P3PmsgList::P3PmsgList ( P2PmsgHANDLE hVBList, VBLaddr aList, VBLsize nListSize )
          : P3PmsgField ( hVBList, 0, 0 )
{
    m_oObject.Connectx ( hVBList, aList, nListSize );
    //RenderThisSafe ( );
    ZeroMemory (  m_pP3PmsgData, sizeof(m_pP3PmsgData) );
    m_nCurs     = 0;
    //m_xList     = aList;
    //m_aList     = aList;
    //m_nListSize = nListSize;
    //OBJ__aVBLock     = aList;
    //m_oObject.m_nVBLockSize = nListSize;
}
P3PmsgList::~P3PmsgList ( )
{
    //if ( !OBJ__hVBList &&
    //      OBJ__aVBLock    )
    //  OBJ__aVBLock = OBJ__Free ( OBJ__aVBLock );
    //if ( !OBJ__hVBList &&
    //      m_aList      )
    //  OBJ__Free ( m_aList );
    for ( int i = 0; i < MAX_P3PmsgData_Curs; i++ )
      delete m_pP3PmsgData[i];
}
void
P3PmsgList::RenderThisSafe ( )
{
    //m_uVBLock  = VBLock_Addr32;
    VBLock& oVBLock = *(VBLock *)m_oObject.m_oVBLock;       // Delegate field object
    ZeroMemory ( &oVBLock, sizeof(oVBLock) );
    m_nCurs = 0;
    ZeroMemory (  m_pP3PmsgData, sizeof(m_pP3PmsgData) );
    VBLock_Init( &oVBLock
               , OBJ__uVBLock|VBLock_Item|VBLock_Linked, sizeof(oVBLock) );
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(&oVBLock), VBLock_List );
    VBLockList_Init ( OBJ__uVBLock
                    , VBLock_pList(&oVBLock), AttrField_DEFAULT );
    VBLockField_Init( VBLock_pField(&oVBLock), AttrField_DEFAULT );
    VBLockName_Init ( OBJ__uVBLock,VBLock_pName(&oVBLock)
                    , VBLockAttr_DEFAULT | VBLockAttr_NULL
                    , 0, sizeof(VBLockField::oVBLockName) );
    VBLockData_Init ( VBLock_pData(&oVBLock)
                    , VBLockAttr_DEFAULT, VBLockData_NULL
                    , sizeof(VBLockField::oVBLockData) );
//ASSERT(VBLockName_Sizenn(VBLock_pName(&m_oList))==sizeof(m_oList.ud.oField.oVBLockName));
    //m_xList     = 0;
    //m_aList     = reinterpret_cast<UINT>(&oVBLock);
    //m_nListSize = sizeof(oVBLock);
    m_oObject.Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock) );
}

void
P3PmsgList::Connect ( P2PmsgHANDLE hVBList, VBLaddr aList, VBLsize nListSize )
{
    //Reset ( );
    //m_xList     = aList;
    //m_aList     = aList;
    //m_nListSize = nListSize;

    // Propagate
    // Audited: P3PmsgField::Connect -> P3PmsgObject::Connectx -> Connecta
    // already sets m_aVBLock (=aList) and m_nVBLockSize (=nListSize) - and also
    // m_xVBLock - so the former direct assignments here were redundant.
    P3PmsgField::Connect ( hVBList, aList, nListSize );
}
VBLaddr
P3PmsgList::GetHeadPos ( ) const
{
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList ( );
    return VBLockList_GetFirst ( OBJ__uVBLock, pList );
}
P3PmsgData&
P3PmsgList::GetNext ( VBLaddr& aPos )
{
    VBLock     *pVBLock = (VBLock *)OBJ__Msg2Phys(aPos);
ASSERT(VBLock_IsLinked(pVBLock));
ASSERT(VBLock_IsAlloc(pVBLock));
    VBLockItem *pItem   = VBLock_pItem(pVBLock);

    // Instantiate
    m_nCurs = (m_nCurs + 1) % MAX_P3PmsgData_Curs;
    if ( m_pP3PmsgData[m_nCurs] == 0 )
      m_pP3PmsgData[m_nCurs] = new P3PmsgData ( );
    m_pP3PmsgData[m_nCurs] -> Connect ( OBJ__hVBList, aPos, 0 );

    // Tidy up, and
    aPos = VBLockItem_GetNext ( OBJ__uVBLock, pItem );
    return *m_pP3PmsgData[m_nCurs];
}
P3PmsgList&
P3PmsgList::AddListHead ( const P3PmsgData& oData )
{
    // Locals
    VBLaddr  aItem__;
    VBLaddr  nSizeofItem;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( OBJ__uVBLock )
                + oData.P3PmsgData::Sizeof();
    aItem__     = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys(aItem__);
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(pVBLock), VBLock_Data );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oData.DataType()
                    , oData.P3PmsgData::Sizeof() );

    // Copy
    VBLockData_Copy ( VBLock_pData(pVBLock), oData.P3PmsgData::Sizeof()
                    , P2PmsgData_pData(oData), oData.P3PmsgData::Sizeof() );
                    //, oData.GetVBLockData(), oData.P3PmsgData::Sizeof() ); //TODO:LJM deprecated

    // Prepend P3PmsgData item
    VBLockList *pList   = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList();
    P2PmsgList_LinkinItem ( this
                          , 0
                          , aItem__
                          , VBLockList_GetFirst(OBJ__uVBLock,pList) );
ASSERT(VBLock_IsItem(pVBLock));
ASSERT(VBLock_IsLinked(pVBLock));
    return *this;
}

P3PmsgData&
P3PmsgList::GetTail ( )
{
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList ( );
    VBLaddr aTail = VBLockList_GetLast ( OBJ__uVBLock, pList );

    // Instantiate
    m_nCurs = (m_nCurs + 1) % MAX_P3PmsgData_Curs;
    if ( m_pP3PmsgData[m_nCurs] == 0 )
      m_pP3PmsgData[m_nCurs] = new P3PmsgData ( );
    m_pP3PmsgData[m_nCurs] -> Connect ( OBJ__hVBList, aTail, 0 );
    return *m_pP3PmsgData[m_nCurs];
}
VBLaddr
P3PmsgList::GetTailPos ( ) const
{
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList ( );
    return VBLockList_GetLast ( OBJ__uVBLock, pList );
}
P3PmsgData&
P3PmsgList::GetPrev ( VBLaddr& aPos )
{
    // Mirror of GetNext: walks the aPrev links, from GetTailPos() towards the
    // head, and leaves aPos 0 once the head has been returned.
    VBLock     *pVBLock = (VBLock *)OBJ__Msg2Phys(aPos);
ASSERT(VBLock_IsLinked(pVBLock));
ASSERT(VBLock_IsAlloc(pVBLock));
    VBLockItem *pItem   = VBLock_pItem(pVBLock);

    // Instantiate
    m_nCurs = (m_nCurs + 1) % MAX_P3PmsgData_Curs;
    if ( m_pP3PmsgData[m_nCurs] == 0 )
      m_pP3PmsgData[m_nCurs] = new P3PmsgData ( );
    m_pP3PmsgData[m_nCurs] -> Connect ( OBJ__hVBList, aPos, 0 );

    // Tidy up, and
    aPos = VBLockItem_GetPrev ( OBJ__uVBLock, pItem );
    return *m_pP3PmsgData[m_nCurs];
}
P3PmsgList&
P3PmsgList::AddListTail ( const P3PmsgData& oData )
{
    // Locals
    //VBLaddr  aItem__;
    VBLsize  nSizeofItem;

    // Allocation
    nSizeofItem = VBLockItem_Sizeof ( OBJ__uVBLock )
                + oData.P3PmsgData::Sizeof();
    VBLaddr aItem__ = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys(aItem__);
    VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(pVBLock), VBLock_Data );
    VBLockData_Init ( VBLock_pData(pVBLock)
                    , VBLockAttr_DEFAULT, oData.DataType()
                    , oData.P3PmsgData::Sizeof() );

    // Copy
    VBLockData_Copy ( VBLock_pData(pVBLock), oData.P3PmsgData::Sizeof()
                    , P2PmsgData_pData(oData), oData.P3PmsgData::Sizeof() );
                    //, oData.GetVBLockData(), oData.P3PmsgData::Sizeof() ); //TODO:LJM deprecated

    // Prepend P3PmsgData item
    VBLockList *pList   = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList();
    P2PmsgList_LinkinItem ( this
                          , VBLockList_GetLast(OBJ__uVBLock,pList)
                          , aItem__
                          , 0 );
ASSERT(VBLock_IsItem(pVBLock));
ASSERT(VBLock_IsLinked(pVBLock));
//ASSERT(AfxCheckMemory());
    return *this;
}
void
P3PmsgList::DropHead ( )
{
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList ( );
    VBLaddr     aItem = VBLockList_GetFirst ( OBJ__uVBLock,pList );
    P2PmsgList_UnLinkItem ( this, pList, aItem );
}
void
P3PmsgList::DropTail ( )
{
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) );
    VBLaddr     aItem = VBLockList_GetLast ( OBJ__uVBLock, pList );
    P2PmsgList_UnLinkItem ( this, pList, aItem );
}

//
//P3PmsgField&
//P3PmsgNode::AddField( LPCTSTR lpszName, P3PmsgData& oData )
//{
//  (*this) += P3PmsgField(lpszName, oData );
//    return *this;
//}

VBLelem
P3PmsgList::GetCount ( )
{
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList ( );
    return VBLockList_GetItems ( OBJ__uVBLock, pList );
}


//  Operators
P3PmsgList&
P3PmsgList::operator = ( const P3PmsgList& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    Truncate ( );                     // Empties existin content

    // Recursively drop existing items and copy
    dynamic_cast<P3PmsgField&>(*this) = rhs;
    VBLaddr aItemNext = rhs.GetHeadPos();
    while ( aItemNext )
      AddListTail ( ((P3PmsgList&)rhs).GetNext(aItemNext) );

    // Tidy up, and
//AssertValid();rhs.AssertValid();//TODO:Delete debugging
    m_bListDirty = true;
ASSERT(r_Object().IsList());
    return *this;
}
P3PmsgList&
P3PmsgList::operator = ( const P3PmsgField& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    Truncate ( );                     // Empties existing content
    dynamic_cast<P3PmsgField&>(*this) = rhs;
ASSERT(r_Object().IsList());
    return *this;
}
P3PmsgList&
P3PmsgList::operator = ( const P3PmsgObject& rhs )
{
    // To be sure, to be sure
    if ( &r_Object() == &rhs )
      return *this;
    m_oObject = rhs;
    //dynamic_cast<P3PmsgObject&>(*this) = rhs;
ASSERT(r_Object().IsList());
    return *this;
}

P3PmsgList&
P3PmsgList::operator += ( const P3PmsgData& rhs )
{
    // Delegate
    AddListTail ( rhs );
    return *this;
}


//  Does this list denote an item?  (Refer P3PmsgObject::operator bool)
//  NOTES: This returned IsVoid() -- the answer inverted -- so `if ( oList )`
//         was true exactly when there was no list. It has no caller in this
//         solution, which is why nothing had noticed.
P3PmsgList::operator bool ( )
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
bool
P3PmsgList::Delete ( VBLaddr aItem )
{
    // This used to open with a bare ASSERT(0) -- the "nobody has tried this
    // yet" marker, like the commented-out one in the dead P3PmsgNode block
    // above -- which made the function unusable in a debug build while working
    // perfectly in a release one, since the assertion is all that differs.
    // The body is complete and correct: removing an element from the head, the
    // middle or the tail of a mixed-type list each leave the remaining
    // elements in order with their values and declared types intact, verified
    // element by element. The marker outlived whatever doubt produced it.
    VBLockList *pList = VBLock_pList ( ptrVBLOCK(m_oObject) ); //GetVBLockList ( );
    P2PmsgList_UnLinkItem ( this, pList, aItem );
    return true;
}
void
P3PmsgList::Truncate ( )
{
    // Cursors
    for ( int i = 0; i < MAX_P3PmsgData_Curs; i++ )
      delete m_pP3PmsgData[i];
    ZeroMemory ( m_pP3PmsgData, sizeof(m_pP3PmsgData) );

    // Drop all items from list
    while ( GetCount() > 0 )
      DropHead ( );
}
void
P3PmsgList::Drop ( )
{
    ASSERT(r_Object().IsList());
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
      else if ( VBLockItem_IsList(pItemParent) )
      {
        ASSERT(0);
        //VBLaddr aParent = P2PmsgField_GetVBLockParentnn ( this );
        //P3PmsgNode  oNodeParent( OBJ__hVBList, aParent, 0 );
        //VBLockNode *pVBLockNode = VBLock_pNode((VBLock *)oNodeParent.r_Object().GetVBLock());
        //P2PmsgNode_UnLinkItem ( &oNodeParent, pVBLockNode, OBJ__VBLocknn );
//oNodeParent.AssertValid();
      }
      else
        ASSERT(0);
   }

   // Tidy up, and
   OBJ__Free ( OBJ__aVBLock );
}

/////////////////////////////////////////////////////////////////////////
////  Memory management
////  NOTES: Delegates through P2PmsgVBL if not-NULL, otherwise interacts
////         directly with heap
//UINT
//P3PmsgList::Free ( UINT aVBLockAddr )
//{
//    ASSERT(IsList());
//    OBJ__Free ( aVBLockAddr );
//    //if ( aVBLockAddr == 0 )
//    //  return 0;
//    //else if ( aVBLockAddr == m_xList )
//    //  m_xList = 0;
//    //else if ( OBJ__hVBList )
//    //  VBListFree ( OBJ__hVBList, aVBLockAddr );
//    //else
//    //{
//    //  if ( aVBLockAddr != reinterpret_cast<UINT>(&m_oList) )
//    //    delete [] (char *)aVBLockAddr;
//    //}
//    //if ( aVBLockAddr == m_aList )
//    //  m_aList = 0;
//    return 0;
//}

// Addressing and allocations
//VBLock*
//P3PmsgList::GetVBLock ( bool ) const
//{
//    //ASSERT(m_aList==OBJ__aVBLock);
//    return m_oObject.GetVBLock();
//    //return (VBLock *)Msg2Phys ( m_aList );
//}
//UINT
//P3PmsgList::GetVBLocknn( ) const
//{
//    ASSERT(IsList());
//    return m_oObject.GetVBLocknn();
//    //return m_aList;
//}
//VBLsize
//P3PmsgList::GetVBLockSize ( ) const
//{
//    return m_oObject.GetVBLockSize();
//    //return m_nListSize;
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
//VBLockList*
//P3PmsgList::GetVBLockList ( bool ) const
//{
//    //VBLock *pVBLock = (VBLock *)Msg2Phys ( m_aList );
//    VBLock *pVBLock = ptrVBLOCK(m_oObject); //.GetVBLock();
//    return VBLock_pList ( pVBLock );
//}

VBLsize
P3PmsgList::GetVBLockListSize ( ) const
{
    return m_oObject.m_nVBLockSize;
    //return m_nListSize;
}

/////////////////////////////////////////////////////////////////////////
//  Troubleshooting
void
P3PmsgList::AssertValid ( ) const
{
    VBLock     *pVBLock = OBJ__VBLock;
    VBLockList *pList   = VBLock_pList ( pVBLock );
    ASSERT(r_Object().IsList());

    // Header
    VBLelem nItems = VBLockList_GetItems ( OBJ__uVBLock, pList );
    VBLelem nFirst;
    VBLaddr aFirst = VBLockList_GetFirst ( OBJ__uVBLock, pList, &nFirst );
    VBLelem nLast;
    VBLaddr aLast  = VBLockList_GetLast  ( OBJ__uVBLock, pList, &nLast );
    if ( !nItems && (nFirst || aFirst || nLast!=-1 || aLast) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted header nItems=%i aFirst=%i aLast=%i",
                        nItems, aFirst, aLast )
            -> Throw();

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
//  Verifies P3PmsgList object containment and optionally containment of
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
P3PmsgList::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // pVBlob containment within VBLock
    VBLock     *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pvBlob != nullptr && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;                    // pVBlob not contained within VBLock

    // VBLockAttr containment
    VBLockList *pList       = VBLock_pList ( pVBLock );
    VBLsize     nSizeofList = VBLockList_Sizeof ( OBJ__uVBLock );
    if ( !VBLock_IsContainedVBLump(pVBLock,pList,nSizeofList) )
      return FALSE;                    // pData not contained within VBLock

    // Delegate through to P3PmsgField
    P3PmsgField::VerifyContainment();

    // Tidy up and
    return TRUE;
}

void
P3PmsgList::Print ( FILE *fd, int nDepthOS, int nDepthOSinc  )
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
    // Attributes
    if ( !r_Desc().IsEmpty() )
      r_Desc().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );

    //// Recursion
    //P3PmsgCurs& oCurs = ((P3PmsgNode *)this)->GetCurs();
    VBLaddr aEntry = GetHeadPos();
    int k = 1;
    while ( aEntry )
    {
      for ( int i = 0; i < nDepthOS+1; i++ )
        P3Pmsg_fwprintf ( fd, L"%*s", nDepthOSinc, L"  " );
      P3PmsgData& oEntry = GetNext ( aEntry );
      P3Pmsg_fwprintf ( fd, L"[%02i] = (%s %s) %s\n"
                      , k++
                      , oEntry.r_data().ToStringType(), oEntry.r_data().ToStringDefs()
                      , oEntry.r_data().ToString() );
    }
    //for ( int i = 0; oCurs.Goto(i); i++ )
    //{
    //  if ( oCurs.IsNode() )
    //    oCurs.r_node().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
    //  else if ( oCurs.IsField() )
    //    oCurs.r_field().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
    //}

    // Tidy up
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    P3Pmsg_fwprintf ( fd, L"}\n" );
}

///////////////////////////////////////
// Properties

P2PmsgListHdl
P3PmsgList::GetP2PmsgListHdl ( )
{
    ASSERT(r_Object().IsList());
    VBLaddr aList = OBJ__aVBLock;
    P2PmsgListHdl oHdl = { (UINT_PTR)OBJ__hVBList, aList, P2PmsgHeap_Sizeof(OBJ__hVBList,aList) };
    //P2PmsgListHdl oHdl = { (UINT_PTR)OBJ__hVBList, m_aList, P2PmsgHeap_Sizeof(OBJ__hVBList,m_aList) };
    return oHdl;
}
VBLsize
P3PmsgList::Sizeof ( UCHAR uVBLock ) const
{
    VBLsize nSizeof = VBLockList_Sizeof ( uVBLock )
                 + P3PmsgField::Sizeof ( );
    return nSizeof;
}
bool
P3PmsgList::IsDirty ( )
{
    if ( m_bListDirty           ||
         P3PmsgField::IsDirty()    )
      return true;
    return false;
}

