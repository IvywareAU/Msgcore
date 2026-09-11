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
//  P3PmsgDesc definitions and prototypes
//
#include "stdafx.h"
#include "Propvarutil.h"
#include "P2PmsgVBLock.h"
#include "MsgVBHeap.h"
#include "MsgDesc.h"
#include "P2Pmsg.h"
#include "P2Pmsg_Ext.h"
#include "Msgexception.h"

#pragma warning(disable:26496)

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
//  P3PmsgNode hidden definitions
//VBLaddr
//P2PmsgNode_UnLinkItem ( P3PmsgNode *pNode, VBLockNode *pVBLockNode, VBLaddr aItem );
VBLockDesc*
P2PmsgObject_pDesc ( const P3PmsgObject& oObject );

///////////////////////////////////////////////////////////////////////
//????????????????????????????????????????????????????????????????????

///////////////////////////////////////////////////////////////////////
//  MsgDesc object manager
//  NOTES: Manages MsgItem descendant operations
VBLock*
P3PmsgDesc__VBLock  ( const P3PmsgDesc *pDesc );
VBLaddr
P3PmsgDesc__GetVBLocknn( const P3PmsgDesc *pThis );
VBLock*
P2PmsgDesc__GetVBLockParent ( const P3PmsgDesc *pDesc );


//  Constructors and destructor
P3PmsgDesc::P3PmsgDesc ( ) noexcept
{
}
P3PmsgDesc::P3PmsgDesc ( const P3PmsgDesc& rhs )
{
	  m_oObject = rhs.m_oObject;
}
P3PmsgDesc::P3PmsgDesc ( P3PmsgField *pField )
{
    m_pP3PmsgField      = pField;
    VBLaddr aVBLockDesc = P2PmsgDesc_GetDescn ( pField );
    m_oObject.Connectx ( pField->m_oObject.m_hVBList, aVBLockDesc, 0 );
}
P3PmsgDesc::P3PmsgDesc ( const P3PmsgObject& rhs )
{
    m_oObject = rhs;
}

P3PmsgDesc::~P3PmsgDesc ( )
{
    Nullify ( );
}

void
P3PmsgDesc::Nullify ( )
{
    m_pP3PmsgField = nullptr;
    if ( m_pCurs )
      delete m_pCurs;
    m_pCurs        = nullptr;
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
P3PmsgDesc::operator = ( const P3PmsgDesc& rhs )
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
      if ( oCurs.IsList() )
        (*this) += oCurs.r_list();
      else if ( oCurs.IsVect() )
        (*this) += oCurs.r_vect();
      else if ( oCurs.IsItem() )
        (*this) += oCurs.r_item();
      //else if ( oCurs.IsNode() ) {
      //  (*this) += oCurs.r_node(); ASSERT(FALSE); }
      else
        ASSERT(FALSE);
    }

    // Tidy up, and
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
//P3PmsgDesc&
//P3PmsgDesc::operator += ( const P3PmsgNode& rhs )
//{
//    PushBack ( rhs );
//    return *this;
//}
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
P3PmsgItem&
P3PmsgDesc::operator [] ( LPCTNAM lpszName )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszName) )
      EVERR -> MODULE
            -> AFP(lpszName)
            -> Message(L"Item [%s] does not exist", lpszName )
            -> Throw();
    if ( m_pCurs->IsList() )
      return m_pCurs->r_list ( );
    if ( m_pCurs->IsItem() )
      return m_pCurs->r_item ( );
    //if ( m_pCurs->IsNode() ) {
    //  ASSERT(FALSE); return m_pCurs->r_node ( ); }
    ASSERT(FALSE);
    return m_pCurs->r_item();
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
    if ( OBJ__aVBLock ) { ASSERT(VBLock_IsDesc(OBJ__VBLock)); }
    if ( OBJ__aVBLock )
      return;
    //TODO:LJM deprecated below
    //UCHAR   uVBLock = m_pP3PmsgField -> r_Object().GetVBLock() -> oHdr.uVBLock;
    //UINT    nSizeof = VBLockItem_Sizeof(uVBLock) + VBLockDesc_Sizeof(uVBLock);
    //UINT    aExtra  = m_pP3PmsgField -> OBJ__Alloc ( VBLock_Desc, nSizeof );
    //VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> OBJ__Msg2Phys(aExtra);
    //TODO:LJM deprecated above
    VBLsize nSizeof = VBLockItem_Sizeof(OBJ__uVBLock) + VBLockDesc_Sizeof(OBJ__uVBLock);
    VBLaddr aDesc   = 0;
    if ( m_pP3PmsgField )
      aDesc = m_pP3PmsgField->OBJ__Alloc ( VBLock_Desc, nSizeof );
    else
      aDesc = OBJ__Alloc ( VBLock_Desc, nSizeof );
    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys ( aDesc );
//ASSERT(VBLock_IsAlloc(pVBLock));
    //VBLockItem_Init ( uVBLock, VBLock_pItem(pVBLock), VBLock_Desc );
//ASSERT(VBLock_IsItem(pVBLock));
    VBLockDesc_Init ( OBJ__uVBLock, VBLock_pDesc(pVBLock), AttrField_DEFAULT
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
    pVBLock -> oHdr.uVBLockDefs |= VBLock_Linked;
    ASSERT(m_oObject.AssertValidAddr(aDesc));
}

//  Chained reference exposures

const P3PmsgObject&
P3PmsgDesc::r_Object ( ) const noexcept
{
    return m_oObject;
}

void
P3PmsgDesc::Drop ( )
{
    Truncate ( );
    // Garbage collection
    // NOTES: Remove linkage prior to releasing allocated block
    if ( OBJ__aVBLock )
    {
      if ( m_pP3PmsgField )
      {  // Clears P2PmsgItem linkage
        VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> r_Object().GetVBLock ( );
        VBLockItem_SetDescn ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock), 0 );
      }  // Remove linkage flag and free
      OBJ__Free ( OBJ__aVBLock );
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
      EVERR -> Module ( __FUNCTION__ )
            //-> AFP(lpszItemName)
            -> Message(L"Item [%s] does not exist", lpszItemName )
            -> Throw();
    }
    return *m_pCurs;
}

P3PmsgList&
P3PmsgDesc::SelectList ( LPCTNAM lpszListName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszListName) ||
         !m_pCurs->IsList()              )
    {
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
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszVectName)
            -> Message(L"Vect [%s] does not exist", lpszVectName )
            -> Throw();
    }
    return m_pCurs->r_vect ( );
}

P3PmsgField&
P3PmsgDesc::DeclareItem ( LPCTNAM lpszItemName, const P3PmsgData& oData, BOOL bUpdate )
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
    if ( bUpdate || 
         m_pCurs->r_item().r_data().IsNull() ||
         m_pCurs->r_item().r_data().DataType() == VBLockData_NULL   )
      m_pCurs->r_item().r_data() = oData;
    return m_pCurs->r_item();
}
//P3PmsgNode&
//P3PmsgDesc::DeclareNode ( LPCTNAM lpszNodename, const P3PmsgData& oData, BOOL bUpdate )
//{
//    ASSERT(0);
//    if ( OBJ__aVBLock == NULL )
//      Create ( );
//    if (  m_pCurs == nullptr )
//      m_pCurs = new P3PmsgCurs ( *this );
//    if ( !m_pCurs->Goto(lpszNodename) )
//    { // This sequence needs optimising
//      (*this) += P3PmsgNode ( lpszNodename, oData );
//      m_pCurs->Goto(lpszNodename);
//      return m_pCurs->r_node();
//    }
//    if ( bUpdate )
//      m_pCurs->r_node().r_data() = oData;
//    return m_pCurs->r_node();
//}
bool
P3PmsgDesc::Exists ( LPCTNAM lpszItemName )
{
    if ( OBJ__aVBLock == NULL )
      return false;
    if (  lpszItemName         == NULL ||
         _tcslen(lpszItemName) <= 0       )
      return false;
    return !P3Pmsg_SelectObject(&r_Object(),lpszItemName).IsVoid();
}
bool
P3PmsgDesc::Delete ( LPCTNAM lpszItemName )
{
    if ( OBJ__aVBLock == NULL )
      return false;
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    // Empty P2PmsgDesc of nominated pItem64
    if ( !m_pCurs->Goto(lpszItemName) )
      return false;
    m_pCurs -> Delete ( );
    return true;
}
bool
P3PmsgDesc::Delete ( P3PmsgItem& oItem )
{
    // Isolate 
    for ( int i = 0; r_Curs().Goto(i); i++ )
    {
      if ( m_pCurs->r_item().GetP2Pos() == oItem.GetP2Pos() )
      {
        m_pCurs->Delete();
        oItem.Nullify();
        break;
      }
    }
    ASSERT(oItem.IsVoid()==true);
    return oItem.IsVoid();
}
void
P3PmsgDesc::Truncate ( )
{
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgDesc of all items
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

// Addressing and allocations
VBLock*
P3PmsgDesc__VBLock  ( const P3PmsgDesc *pDesc )
{
    //VBLock *pVBLock = pDesc -> GetField() -> r_Object().GetVBLock ( );
    //UINT    aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    VBLock *pVBDesc =(VBLock *)pDesc -> r_Object().GetVBLock();
    if ( pVBDesc )
    {
      ASSERT(VBLock_IsLinked(pVBDesc)==true);
      ASSERT(VBLock_IsAlloc(pVBDesc)==true);
    }
    return pVBDesc;
}
VBLock*
P3PmsgDesc__VBLock  ( const P3PmsgField *pField )
{
    VBLock *pVBLock = (VBLock *)pField -> r_Object().GetVBLock ( );
    VBLaddr aDescn  = VBLockItem_GetDescn ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    if ( !aDescn )
      return 0;
    VBLock *pVBDesc =(VBLock *)pField -> r_Object().Msg2Phys ( aDescn );
    ASSERT(VBLock_IsItem(pVBLock));
    ASSERT(VBLock_IsLinked(pVBDesc));
    ASSERT(VBLock_IsAlloc(pVBDesc));
    return pVBDesc;
}
VBLaddr
P3PmsgDesc__GetVBLocknn( const P3PmsgDesc *pThis )
{
    VBLock *pVBLock = (VBLock *)pThis -> GetField() -> r_Object().GetVBLock ( );
    VBLaddr aDescn  = VBLockItem_GetDescn ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    return  aDescn;
}

//  Troubleshooting
void
P3PmsgDesc::AssertValid ( ) const
{
    //TODO:LJM deprecated VBLock     *pVBLock = P3PmsgDesc__VBLock ( this );
    VBLock *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pVBLock == nullptr )
      return;
    // Containment
    m_oObject.AssertValid();
    if ( !VerifyContainment() )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Failed Containment" )
            -> Advice ( L"%s", GetField()->GetPath().GetString() )  
            -> Throw();
    UCHAR       uVBLock = pVBLock->oHdr.uVBLockDefs;
    VBLockDesc *pDesc   = VBLock_pDesc ( pVBLock );

    // Addressing
    UINT uVBLock1 = uVBLock&VBLock_AddrMask;
    if ( m_pP3PmsgField )
    {
      UINT uVBLock2 = m_pP3PmsgField->OBJ__uVBLock&VBLock_AddrMask;
      if ( uVBLock1 != uVBLock2 )
        EVERR -> Module ( __FUNCTION__ )
              -> Message("Corrupted addressing (0x%x vs 0x%x)", uVBLock1, uVBLock2 )
              -> Advice ( L"%s", GetField()->GetPath().GetString() )  
              -> Throw();
    }

    // Header
    int     nItems = VBLockDesc_GetItems ( uVBLock, pDesc );
    int     nFirst;
    VBLaddr aFirst = VBLockDesc_GetFirst ( uVBLock, pDesc, &nFirst );
    int  nLast;
    VBLaddr aLast  = VBLockDesc_GetLast  ( uVBLock, pDesc, &nLast );
    if ( !nItems && (nFirst || aFirst || nLast!=-1 || aLast) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted header nItems=%i aFirst=%i aLast=%i",
                        nItems, aFirst, aLast )
            -> Advice ( L"%s", GetField()->GetPath().GetString() )  
            -> Throw();

    // Navigation
    P3PmsgCurs& oCurs = ((P3PmsgDesc *)this)->r_Curs();
	  int i;
    for ( i = 0; oCurs.Goto(i); i++ )
    {
      oCurs.r_Object().AssertCommon ( r_Object() );
      //if ( oCurs.IsNode() )
      //  oCurs.r_node().AssertValid();
      if ( oCurs.IsList() )
        oCurs.r_list().AssertValid();
      else if ( oCurs.IsVect() )
        oCurs.r_vect().AssertValid();
      else if ( oCurs.IsItem() )
        oCurs.r_item().AssertValid();
      else { ASSERT(FALSE); }
    }
    if ( nItems != i ) {
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted item count expected=%i actual=%i",
                        nItems, i )
            -> Advice ( L"%s", GetField()->GetPath().GetString() )  
            -> Cancel ( );
    }
}
//
//  Verifies P3PmsgDesc object containment and optionally containment of
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
P3PmsgDesc::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // pVBlob containment within VBLock
    VBLock     *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pvBlob != nullptr && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;                    // pVBlob not contained within VBLock

    // VBLockDesc containment
    VBLockDesc *pDesc       = VBLock_pDesc ( pVBLock );
    VBLsize     nSizeofDesc = VBLockDesc_Sizeof ( OBJ__uVBLock );
    if ( !VBLock_IsContainedVBLump(pVBLock,pDesc,nSizeofDesc) )
      return FALSE;                    // pData not contained within VBLock

    // Tidy up and
    return TRUE;
}
void
P3PmsgDesc::Print ( FILE *fd, int nDepthOS, int nDepthOSinc )
{
    // P2PmsgDesc details
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    //TODO:LJM deprecated UINT nVBLockSize = VBLock_Hdr_u_SizeNN( P3PmsgDesc__VBLock(this) );
    if ( OBJ__VBLock )
    {
      VBLsize nVBLockSize = VBLock_Hdr_u_SizeNN( OBJ__VBLock );
      P3Pmsg_fwprintf ( fd, L".(%Ii) {\n", nVBLockSize );

      // Recursion
      P3PmsgCurs& oCurs = ((P3PmsgDesc *)this)->r_Curs();
      for ( int i = 0; oCurs.Goto(i); i++ )
      {
        //if ( oCurs.IsNode() )
        //  oCurs.r_node().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
        if ( oCurs.IsList() )
          oCurs.r_list().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
        else if ( oCurs.IsVect() )
          oCurs.r_vect().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
        else if ( oCurs.IsItem() )
          oCurs.r_item().Print ( fd, nDepthOS+nDepthOSinc, nDepthOSinc );
      }
    } else P3Pmsg_fwprintf ( fd, L"@() {\n" );

    // Tidy up
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    P3Pmsg_fwprintf ( fd, L"}\n" );
}

///////////////////////////////////////////////////////////////////////
//  Std::List modifiers

P2PmsgFieldHdl
P3PmsgDesc::PushBack ( const P3PmsgField& oField )
{
    oField.VerifyContainment();
    oField.r_Object().AssertCondition(ObjectCheckID_DataSizeGT9);
    if ( OBJ__aVBLock == NULL )
      Create ( );
    // Block allocation and linkage
    //P3PmsgObject& oObject     = (P3PmsgObject&)m_pP3PmsgField -> r_Object ( );
    //P2PmsgHANDLE& hVBList     =  oObject.m_hVBList;
    //UCHAR&        uVBLock     =  oObject.m_uVBLock;
    VBLsize       nSizeofItem =  P2PmsgField_SizeofItem ( OBJ__uVBLock, oField, TRUE );
    VBLaddr       aItem       =  OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock       *pVBLockItem = (VBLock *)OBJ__Msg2Phys(aItem);
    P2PmsgItem_InitField  ( pVBLockItem, oField );
    //TODO:LJM deprecated VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    if ( GetPermissions(AttrField_SORT) )
      P2PmsgDesc_SortinItem ( this, oField.r_name(), aItem );
    else
      P2PmsgDesc_LinkinItem ( this
                            , VBLockDesc_GetLast(OBJ__uVBLock,pVBLockDesc)
                            , aItem
                            , 0 );
ASSERT(OBJ__uVBLock==(pVBLockItem->oHdr.uVBLockDefs&VBLock_AddrMask));
//AssertValid();//TODO:LJM delete, testing
//oField.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
    P3PmsgField oFieldItem ( OBJ__hVBList, aItem, nSizeofItem );
    if (!oFieldItem.r_Object().AssertCondition(ObjectCheckID_DataSizeGT9)) // delete-bug-hunting
    {
      ASSERT(0);
      VBLsize nSizeofItem3 = P2PmsgField_SizeofItem ( OBJ__uVBLock, oField, TRUE );
      //VBLaddr aItem1       =  OBJ__Alloc ( VBLock_Item, nSizeofItem3 );
      VBLock       *pVBLockItem1 = (VBLock *)OBJ__Msg2Phys(aItem);
      P2PmsgItem_InitField  ( pVBLockItem1, oField );
      //VBLsize nSizeofData1 = VBLockData_Sizeof_Alloc(pVBLockItem1);
      VBLsize nSizeofItem1 = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
      P3PmsgField oFieldItem1 ( OBJ__hVBList, aItem, nSizeofItem1 );
      //VBLsize nSizeofData2 = VBLockData_Sizeof_Alloc((VBLock*)oFieldItem1.r_Object().GetVBLock());
      //VBLsize nSizeofItem2 = oFieldItem1.r_Object().GetVBLockSize();
    }
    //ASSERT(oFieldItem.VerifyContainment());  //TODO:LJM Bug-Hunt
                oFieldItem = oField;
    //ASSERT(oFieldItem.VerifyContainment());  //TODO:LJM Bug-Hunt

    // Tidy up and
    P2PmsgFieldHdl oHdl = { (UINT_PTR)OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
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
    VBLsize        nSizeofItem = P2PmsgList_SizeofItem ( OBJ__uVBLock, oList );
    VBLaddr        aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgList_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), oList );
    //TODO:LJM deprecated VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    if ( GetPermissions(AttrField_SORT) )
      P2PmsgDesc_SortinItem ( this, oList.r_name(), aItem );
    else
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
    P2PmsgListHdl oHdl = { (UINT_PTR)OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
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
    VBLsize       nSizeofItem = P2PmsgVect_SizeofItem ( OBJ__uVBLock, oVect );
    VBLaddr       aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgVect_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), oVect );
    //TODO:LJM deprecated below VBLockDesc *pVBLockDesc = VBLock_pDesc ( P3PmsgDesc__VBLock(this) );
    VBLockDesc *pVBLockDesc = VBLock_pDesc ( OBJ__VBLock );
    if ( GetPermissions(AttrField_SORT) )
      P2PmsgDesc_SortinItem ( this, oVect.r_name(), aItem );
    else
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
    P2PmsgVectHdl oHdl = { (UINT_PTR)OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    //ASSERT(hVBList==oObject.m_hVBList);
    //ASSERT(uVBLock==oObject.m_uVBLock);
    return oHdl;
}

// Properties
UCHAR
P3PmsgDesc::SetPermissions ( UCHAR uPermissionsAdd, UCHAR uPermissionsRemove )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    VBLockDesc *pDesc = P2PmsgObject_pDesc ( OBJ__ );
                pDesc -> uPermissions &= ~uPermissionsRemove;
                pDesc -> uPermissions |=  uPermissionsAdd;
    return pDesc -> uPermissions;
}
UCHAR
P3PmsgDesc::GetPermissions ( UCHAR uPermissionsMask )
{
    if ( OBJ__aVBLock == NULL )
      return 0u;
    VBLockDesc *pDesc = P2PmsgObject_pDesc ( OBJ__ );
    return pDesc -> uPermissions & uPermissionsMask;
}
VBLelem
P3PmsgDesc::GetCount ( ) const
{
    //TODO:LJM deprecated below VBLock *pVBLock = P3PmsgDesc__VBLock ( this );
    VBLock *pVBLock = OBJ__VBLock;
    if ( pVBLock == NULL )
      return 0;
    VBLockDesc *pDesc   = VBLock_pDesc ( pVBLock );
    return VBLockDesc_GetItems ( pVBLock->oHdr.uVBLockDefs, pDesc );
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

VBLaddr
P2PmsgDesc_GetVBLockParentnn ( const P3PmsgDesc *pDesc )
{
    if ( pDesc->GetField() == 0 )
      return 0;
    //  THE COLLECTION'S OWN BLOCK, not the owning item's -- the same defect
    //  §12 found and fixed in P2PmsgAttr_GetVBLockParentnn, left standing here
    //  because this one had no caller either. GetField()->r_Object() is the
    //  ITEM (P3PmsgDesc holds a pointer back to the field it belongs to, which
    //  is what GetField() means), and reading it through VBLock_pDesc picks
    //  VBLockDesc's fields out of a VBLockItem's ut union: the parent comes
    //  back as whatever bytes lie at that offset, and Msg2Phys of that runs
    //  off the arena.
    //
    //  The collection lives at the item's aDescn, which is what
    //  P3PmsgDesc__GetVBLocknn reads and what MsgDesc's own link routines
    //  write into every child's aParent.
    //TODO:LJM deprecated below VBLock *pVBLock   = P3PmsgDesc__VBLock ( pDesc );
    VBLaddr aVBLockDesc = P3PmsgDesc__GetVBLocknn ( pDesc );
    if ( aVBLockDesc == 0 )
      return 0;                        // No descendants; no collection block
    VBLock *pVBLock   = (VBLock *)pDesc->GetField()->r_Object().Msg2Phys ( aVBLockDesc );
    if ( pVBLock == nullptr || !VBLock_IsDesc(pVBLock) )
      return 0;
    UCHAR   uVBLock   = pVBLock->oHdr.uVBLockDefs;
    ASSERT(VBLock_IsLinked(pVBLock));
    return VBLockDesc_GetParent ( uVBLock, VBLock_pDesc(pVBLock) );
}
VBLock*
P2PmsgDesc__GetVBLockParent ( const P3PmsgDesc *pDesc )
{
    VBLaddr aParent = P2PmsgDesc_GetVBLockParentnn ( pDesc );
    return (VBLock *)pDesc->GetField()->r_Object().Msg2Phys(aParent);
}
//????????????????????????????????????????????????????????????????????
//////////////////////////////////////////////////////////////////////


//
//  Links passed VBLockItem into VBLockDescn
//  NOTES: Handles bit addressing translations
//
//  Parameters:  P2PmsgVBL
//               Virtual block list manager
//
//               VBLockDesc *pVBLockDesc
//               Descendants into which VBLockItem is to be linked
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
P2PmsgDesc_LinkinItem(P3PmsgDesc* pDesc,
                     VBLaddr aItemPrev,
                     VBLaddr aItem,
                     VBLaddr aItemNext)
{
    auto* field = pDesc->GetField();
    auto& obj   = field->r_Object();

    VBLock* pItemPrev = aItemPrev ? (VBLock*)obj.Msg2Phys(aItemPrev) : nullptr;
    VBLock* pItem     = (VBLock*)obj.Msg2Phys(aItem);
    VBLock* pItemNext = aItemNext ? (VBLock*)obj.Msg2Phys(aItemNext) : nullptr;

    VBLock*     pVBLock     = (VBLock*)pDesc->r_Object().GetVBLock();
    VBLockDesc* pVBLockDesc = VBLock_pDesc(pVBLock);
    VBLockItem* pItem_nn    = VBLock_pItem(pItem);

    const UINT addrMode = pVBLock->oHdr.uVBLockDefs & VBLock_AddrMask;
    const auto parent   = P3PmsgDesc__GetVBLocknn(pDesc);

    pItem->oHdr.uVBLockDefs |= VBLock_Linked;

    switch (addrMode)
    {
    case VBLock_Addr64:
    {
        auto* pItem64 = &pItem_nn->ua.oItem64;
        pItem64->aParent = (UINT64)parent;
        pItem64->aPrev   = (UINT64)aItemPrev;
        pItem64->aNext   = (UINT64)aItemNext;

        if (pItemPrev)
            VBLock_pItem(pItemPrev)->ua.oItem64.aNext = static_cast<UINT64>(aItem);
        if (pItemNext)
            VBLock_pItem(pItemNext)->ua.oItem64.aPrev = static_cast<UINT64>(aItem);

        if (pItem64->aPrev == 0)
            pVBLockDesc->u.oDesc64.aFirst = static_cast<UINT64>(aItem);
        if (pItem64->aNext == 0)
            pVBLockDesc->u.oDesc64.aLast  = static_cast<UINT64>(aItem);

        pVBLockDesc->u.oDesc64.nItems++;
        break;
    }
    case VBLock_Addr32:
    {
        auto* pItem32 = &pItem_nn->ua.oItem32;
        pItem32->aParent = (UINT32)parent;
        pItem32->aPrev   = (UINT32)aItemPrev;
        pItem32->aNext   = (UINT32)aItemNext;

        if (pItemPrev)
            VBLock_pItem(pItemPrev)->ua.oItem32.aNext = static_cast<UINT32>(aItem);
        if (pItemNext)
            VBLock_pItem(pItemNext)->ua.oItem32.aPrev = static_cast<UINT32>(aItem);

        if (pItem32->aPrev == 0)
            pVBLockDesc->u.oDesc32.aFirst = static_cast<UINT32>(aItem);
        if (pItem32->aNext == 0)
            pVBLockDesc->u.oDesc32.aLast  = static_cast<UINT32>(aItem);

        pVBLockDesc->u.oDesc32.nItems++;
        break;
    }
    case VBLock_Addr16:
    {
        auto* pItem16 = &pItem_nn->ua.oItem16;
        pItem16->aParent = (UINT16)parent;
        pItem16->aPrev   = (UINT16)aItemPrev;
        pItem16->aNext   = (UINT16)aItemNext;

        if (pItemPrev)
            VBLock_pItem(pItemPrev)->ua.oItem16.aNext = static_cast<UINT16>(aItem);
        if (pItemNext)
            VBLock_pItem(pItemNext)->ua.oItem16.aPrev = static_cast<UINT16>(aItem);

        if (pItem16->aPrev == 0)
            pVBLockDesc->u.oDesc16.aFirst = static_cast<UINT16>(aItem);
        if (pItem16->aNext == 0)
            pVBLockDesc->u.oDesc16.aLast  = static_cast<UINT16>(aItem);

        pVBLockDesc->u.oDesc16.nItems++;
        break;
    }
    case VBLock_Addr08:
    {
        auto* pItem08 = &pItem_nn->ua.oItem08;
        pItem08->aParent = (UINT08)parent;
        pItem08->aPrev   = (UINT08)aItemPrev;
        pItem08->aNext   = (UINT08)aItemNext;

        if (pItemPrev)
            VBLock_pItem(pItemPrev)->ua.oItem08.aNext = static_cast<UINT08>(aItem);
        if (pItemNext)
            VBLock_pItem(pItemNext)->ua.oItem08.aPrev = static_cast<UINT08>(aItem);

        if (pItem08->aPrev == 0)
            pVBLockDesc->u.oDesc08.aFirst = static_cast<UINT08>(aItem);
        if (pItem08->aNext == 0)
            pVBLockDesc->u.oDesc08.aLast  = static_cast<UINT08>(aItem);

        pVBLockDesc->u.oDesc08.nItems++;
        break;
    }
    default:
        EVERR->Module("P2PmsgDesc")
             ->Message("Internal VBLockDesc.uVBLockAddr.ua corruption")
             ->Throw();
    }
    ASSERT(VBLock_IsLinked(pItem));
}
void
P2PmsgDesc_LinkinItem_preChatGPT ( P3PmsgDesc *pDesc
                      , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext )
{
    // Locals
    VBLock     *pItemPrev   = aItemPrev ? (VBLock *)pDesc->GetField()->r_Object().Msg2Phys(aItemPrev) : nullptr;
    VBLock     *pItem       = (VBLock *)pDesc->GetField()->r_Object().Msg2Phys(aItem);
    VBLock     *pItemNext   = aItemNext ? (VBLock *)pDesc->GetField()->r_Object().Msg2Phys(aItemNext) : nullptr;
    //TODO:LJM deprecated below VBLock     *pVBLock     =           P3PmsgDesc__VBLock(pDesc);  //TODO:LJM deprecated pDesc->GetVBLock();
    VBLock     *pVBLock     = (VBLock *)pDesc->r_Object().GetVBLock();
    VBLockDesc *pVBLockDesc = VBLock_pDesc( pVBLock );

  VBLockItem *pItem_nn    = VBLock_pItem(pItem);

    // Observe addressing model
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr64 )
    {
      pItem_nn->ua.oItem64.aParent = (UINT64)P3PmsgDesc__GetVBLocknn(pDesc);
      goto P64;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr32 )
    {
      pItem_nn->ua.oItem32.aParent = (UINT32)P3PmsgDesc__GetVBLocknn(pDesc);
      goto P32;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr16 )
    {
      pItem_nn->ua.oItem16.aParent = (UINT16)P3PmsgDesc__GetVBLocknn(pDesc);
      goto P16;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr08 )
    {
      pItem_nn->ua.oItem08.aParent = (UINT08)P3PmsgDesc__GetVBLocknn(pDesc);
      goto P08;
    }
    EVERR->Module ("P2PmsgDesc")
         ->Message("Internal VBLockDesc.uVBLockAddr.ua corruption" )
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
P64:pItem_nn->ua.oItem64.aPrev = (UINT64)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem64.aNext = (UINT64)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N64;

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
N64:pItem_nn->ua.oItem64.aNext = (UINT64)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem64.aPrev = (UINT64)aItem;
    goto H64;

  // P2PmsgDesc - Housekeeping
H08:if ( pItem_nn->ua.oItem08.aPrev == 0 )
      pVBLockDesc->u.oDesc08.aFirst = (UINT08)aItem;
    if ( pItem_nn->ua.oItem08.aNext == 0 )
      pVBLockDesc->u.oDesc08.aLast  = (UINT08)aItem;
    pVBLockDesc -> u.oDesc08.nItems++;
    goto END;
H16:if ( pItem_nn->ua.oItem16.aPrev == 0 )
      pVBLockDesc->u.oDesc16.aFirst = (UINT16)aItem;
    if ( pItem_nn->ua.oItem16.aNext == 0 )
      pVBLockDesc->u.oDesc16.aLast  = (UINT16)aItem;
    pVBLockDesc -> u.oDesc16.nItems++;
    goto END;
H32:if ( pItem_nn->ua.oItem32.aPrev == 0 )
      pVBLockDesc->u.oDesc32.aFirst = (UINT32)aItem;
    if ( pItem_nn->ua.oItem32.aNext == 0 )
      pVBLockDesc->u.oDesc32.aLast  = (UINT32)aItem;
    pVBLockDesc -> u.oDesc32.nItems++;
    goto END;
H64:if ( pItem_nn->ua.oItem64.aPrev == 0 )
      pVBLockDesc->u.oDesc64.aFirst = (UINT64)aItem;
    if ( pItem_nn->ua.oItem64.aNext == 0 )
      pVBLockDesc->u.oDesc64.aLast  = (UINT64)aItem;
    pVBLockDesc -> u.oDesc64.nItems++;
    goto END;

        // Tidy up and
END:ASSERT(VBLock_IsLinked(pItem));
        //ASSERT(VBLock_IsAlloc(pItem));
        return;
}

//
//  Links passed VBLockItem into VBLockDesc
//  NOTES: Handles bit addressing translations
//
//  Parameters:  P2PmsgVBL
//               Virtual block list manager
//
//               VBLockDesc *pVBLockNode
//               Node into which VBLockItem is to be linked
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
P2PmsgDesc_SortinItem ( P3PmsgDesc *pDesc, const P3PmsgName& oName
                      , VBLaddr aItem )
{
    // Locals
    P3PmsgCurs oCurs ( *pDesc );
    VBLaddr aItemPrev = 0;
    VBLaddr aItemNext = 0;
    // Optimisation - Empty list
    if ( pDesc->GetCount() <= 0 )
    {
//fwprintf(stdout,"oName=%s\n",oName.c_name());
      P2PmsgDesc_LinkinItem ( pDesc, 0, aItem, 0 );
      return;
    }

    // Optimisation - First in list
    int  imin = 0;
    if ( oCurs.Goto(imin) && oName < oCurs.r_name() )
    {
//fwprintf(stdout,L"oName=%s < r_name(imin)=%s \n", oName.c_name(), oCurs.r_name().c_name() );
      P2PmsgDesc_LinkinItem ( pDesc, 0, aItem, P3PmsgCurs_GetVBLocknn(oCurs) );
      return;
    }

    // Optimisation - Last in list
    int  imax = pDesc->GetCount() - 1;
    if ( oCurs.Goto(imax) && oName > oCurs.r_name() )
    {
//fwprintf(stdout,L"r_name(imin)=%s < oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
      P2PmsgDesc_LinkinItem ( pDesc, P3PmsgCurs_GetVBLocknn(oCurs), aItem, 0 );
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
//fwprintf(stdout,L"r_name(imid=%i)=%s oName=%s imin=%i imax=%i\n",imid, oCurs.r_name().c_name(), oName.c_name(), imin, imax );
      if ( oCurs.r_name() < oName )
        imin = imid + 1;
      else if ( oCurs.r_name() > oName )
        imax = imid - 1;
      else
      {
        // Exact-name match during the insertion search; converge on imid.
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
    P2PmsgDesc_LinkinItem ( pDesc, aItemPrev, aItem, aItemNext );
}

VBLaddr
P2PmsgDesc_UnLinkItem ( P3PmsgDesc *pDesc, VBLockDesc *pVBLockDesc, VBLaddr aItem )
{
    P3PmsgField *pField      = pDesc -> GetField();
    VBLock      *pVBLockPrev = 0;
    VBLock      *pVBLock     =(VBLock *)pField->r_Object().Msg2Phys(aItem);
    ASSERT(VBLock_IsLinked(pVBLock));
    VBLockItem  *pItem       = VBLock_pItem ( pVBLock );
    VBLock      *pVBLockNext = 0;

    UCHAR   uVBLock   = pVBLock->oHdr.uVBLockDefs;

    VBLaddr aItemPrev = VBLockItem_GetPrev ( uVBLock, pItem );
    if ( aItemPrev )
      pVBLockPrev = (VBLock *)pField -> r_Object().Msg2Phys(aItemPrev);
    VBLaddr  aItemNext = VBLockItem_GetNext ( uVBLock, pItem );
    if ( aItemNext )
      pVBLockNext = (VBLock *)pField -> r_Object().Msg2Phys(aItemNext);

    // Item housekeeping
    // NOTES: Remove linkages etc
    if ( aItemPrev )
      VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aItemNext );
    if ( aItemNext )
      VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aItemPrev );

    // Node housekeeping
    // NOTES: Counts and linkages etc
    VBLockDesc_SetItems ( uVBLock, VBLock_pDesc((VBLock *)pDesc->r_Object().GetVBLock()), -1, false );
    if ( VBLockDesc_GetFirst(uVBLock,pVBLockDesc,0) == aItem )
      VBLockDesc_SetFirst ( uVBLock, pVBLockDesc, aItemNext );
    if ( VBLockDesc_GetLast(uVBLock,pVBLockDesc,0) == aItem )
      VBLockDesc_SetLast ( uVBLock, pVBLockDesc, aItemPrev );

    // Isolate
    VBLockItem_SetPrev  ( uVBLock, pItem, 0 );
    VBLockItem_SetNext  ( uVBLock, pItem, 0 );
    VBLockItem_SetParent( uVBLock, pItem, 0 );
    return aItem;
}

VBLaddr
P2PmsgDesc_UnLinkItem ( P3PmsgObject *pObject, VBLockDesc *pVBLockDesc, VBLaddr aItem )
{
    //P3PmsgField *pField      = pDesc -> GetField();
    VBLock      *pVBLockPrev = 0;
    VBLock      *pVBLock     =(VBLock *)pObject->Msg2Phys(aItem);
    ASSERT(VBLock_IsLinked(pVBLock));
    ASSERT(VBLock_IsAlloc(pVBLock));
    VBLockItem  *pItem       = VBLock_pItem ( pVBLock );
    VBLock      *pVBLockNext = 0;

    UCHAR   uVBLock   = pVBLock->oHdr.uVBLockDefs;

    VBLaddr aItemPrev = VBLockItem_GetPrev ( uVBLock, pItem );
    if ( aItemPrev )
      pVBLockPrev = (VBLock *)pObject->Msg2Phys(aItemPrev);
    VBLaddr aItemNext = VBLockItem_GetNext ( uVBLock, pItem );
    if ( aItemNext )
      pVBLockNext = (VBLock *)pObject->Msg2Phys(aItemNext);

    // Item housekeeping
    // NOTES: Remove linkages etc
    if ( aItemPrev )
      VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aItemNext );
    if ( aItemNext )
      VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aItemPrev );

    // Desc housekeeping
    // NOTES: Counts and linkages etc
    VBLockDesc_SetItems ( uVBLock, pVBLockDesc, -1, false );
    if ( VBLockDesc_GetFirst(uVBLock,pVBLockDesc,0) == aItem )
      VBLockDesc_SetFirst ( uVBLock, pVBLockDesc, aItemNext );
    if ( VBLockDesc_GetLast(uVBLock,pVBLockDesc,0) == aItem )
      VBLockDesc_SetLast ( uVBLock, pVBLockDesc, aItemPrev );

    // Isolate
    VBLockItem_SetPrev  ( uVBLock, pItem, 0 );
    VBLockItem_SetNext  ( uVBLock, pItem, 0 );
    VBLockItem_SetParent( uVBLock, pItem, 0 );
    return aItem;
}

VBLaddr
P2PmsgDesc_GetDescn ( P3PmsgField *pField )
{
    VBLock *pVBLock = (VBLock *)pField -> r_Object().GetVBLock();
    VBLaddr aDescn  = VBLockItem_GetDescn ( pVBLock->oHdr.uVBLockDefs
                                          , VBLock_pItem(pVBLock) );
    return aDescn;
}

VBLockDesc*
P2PmsgObject_pDesc ( const P3PmsgObject& oObject )
{
    VBLock *pVBLock = (VBLock *)oObject.GetVBLock ( );
    ASSERT(VBLock_IsLinked(pVBLock)==true);
    ASSERT(VBLock_IsAlloc(pVBLock)==true);
    return VBLock_pDesc ( pVBLock );
}

//
//  Swaps common descendant items
//
//  Parameters:  P3PmsgItem1& oItem1
//
//               P3PmsgItem2& oItem2
//
void
P2PmsgDesc_Swap ( P3PmsgItem& oItem1, P3PmsgItem& oItem2 )
{
    ASSERT(oItem1.r_Object().m_hVBList==oItem2.r_Object().m_hVBList);
    VBLock *pVBLock1 = (VBLock *)oItem1.r_Object().GetVBLock   ( );
    VBLaddr aVBLock1 = oItem1.r_Object().GetVBLocknn ( );
    VBLock *pVBLock2 = (VBLock *)oItem2.r_Object().GetVBLock   ( );
    VBLaddr aVBLock2 = oItem2.r_Object().GetVBLocknn ( );
    VBLock *pParent1 = P2PmsgField_GetVBLockParent ( &oItem1 );
    VBLock *pParent2 = P2PmsgField_GetVBLockParent ( &oItem2 );
    UCHAR   uVBLock  = P2PmsgHeap_Addrnn ( oItem1.GetP2PmsgHandle() );

    // To be sure, to be sure
    if ( pParent1                 != pParent2                 ||
         oItem1.GetP2PmsgHandle() != oItem2.GetP2PmsgHandle()    )
      EVERR->Module ( __FUNCTION__ )
           ->Message("Invalid attempt to swap parents" )
           ->Throw();
    if ( aVBLock1 == aVBLock2 )
      return;

    // Parent Desc(First) housekeeping
    VBLockDesc *pDescParent = VBLock_pDesc ( pParent1 );
    if ( VBLockDesc_GetFirst(uVBLock,pDescParent,0) == aVBLock1 )
      VBLockDesc_SetFirst ( uVBLock, pDescParent, aVBLock2 );
    else if ( VBLockDesc_GetFirst(uVBLock,pDescParent,0) == aVBLock2 )
      VBLockDesc_SetFirst ( uVBLock, pDescParent, aVBLock1 );

    // Parent Desc(Last) housekeeping
    if ( VBLockDesc_GetLast(uVBLock,pDescParent,0) == aVBLock1 )
      VBLockDesc_SetLast ( uVBLock, pDescParent, aVBLock2 );
    else if ( VBLockDesc_GetLast(uVBLock,pDescParent,0) == aVBLock2 )
      VBLockDesc_SetLast ( uVBLock, pDescParent, aVBLock1 );

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
        VBLock *pVBLockPrev = (VBLock *)oItem1.r_Object().Msg2Phys(aVBLock1p);
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
        VBLock *pVBLockNext = (VBLock *)oItem2.r_Object().Msg2Phys(aVBLock1n);
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
        VBLock *pVBLockPrev = (VBLock *)oItem2.r_Object().Msg2Phys(aVBLock2p);
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
        VBLock *pVBLockNext = (VBLock *)oItem2.r_Object().Msg2Phys(aVBLock2n);
        VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aVBLock1 );
      }
    }
    else
      VBLockItem_SetNext ( uVBLock, pVBLockItem1, aVBLock2 );
}

//
//  Performs wildcard delete
//
//  Parameters:  P3PmsgDesc oDesc
//               Descendant object from which items are to be deleted
//
//               LPCTSTR lpszWildcard
//               Wildcard
//
BOOL
P2PmsgDesc_WCDelete ( P3PmsgDesc& oDesc, LPCTSTR lpszWildcard )
{
    int nMatches = 0;
    P3PmsgCurs oCurs ( oDesc );
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
       if ( !MsgcoreWildcard(lpszWildcard,oCurs.r_name().c_name()) )
         continue;
       oCurs.Delete ( );
       nMatches++;
    }
    return nMatches;
}
