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
//  P3P framework attribute definitions and prototypes
//  NOTES: Manages P3PmsgItem attribute operations, effectively
//         an indirection or property branch in the primary P2Pmsg tree.
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

#define  ptrVBLOCK(OBJ)  ((VBLock *)(OBJ).GetVBLock())

///////////////////////////////////////////////////////////////////////
//  P3PmsgAttr hidden definitions
VBLockAttr*
P2PmsgObject_pAttr ( const P3PmsgObject& oObject );

///////////////////////////////////////////////////////////////////////
//????????????????????????????????????????????????????????????????????

///////////////////////////////////////////////////////////////////////
//  P3PmsgAttr object manager
//  NOTES: Performs P3PmsgField attribute operations

//  Constructors and destructor
P3PmsgAttr::P3PmsgAttr ( )
{
    //v TODO:LJM This block added 5/12/2014 to handle isolated P3PmsgAttr's
    VBLock& oVBLock = *(VBLock *)m_oObject.m_oVBLock;       // Delegate object
    ZeroMemory ( &oVBLock, sizeof(oVBLock) );
    VBLock_Init( &oVBLock
               , OBJ__uVBLock|VBLock_Attr|VBLock_Linked|VBLock_Alloc, sizeof(oVBLock) );
    //VBLockItem_Init ( OBJ__uVBLock, VBLock_pItem(&oVBLock), VBLock_Attr );
    VBLockAttr_Init ( OBJ__uVBLock
                    , VBLock_pAttr(&oVBLock), AttrField_DEFAULT, 0 );
    m_pCurs       = 0;
    m_oObject.Connecta ( 0/*m_hVBList*/, (VBLaddr)&oVBLock, sizeof(oVBLock) );
    //^ TODO:LJM This block added 5/12/2014 to handle isolated P3PmsgAttr's
}
P3PmsgAttr::P3PmsgAttr ( const P3PmsgAttr& rhs )
{
	  m_oObject = rhs.m_oObject;
}

P3PmsgAttr::P3PmsgAttr ( P3PmsgField *pField )
{
    m_pP3PmsgField      = pField;
    VBLaddr aVBLockAttr = P2PmsgAttr_GetExtra ( pField );
    m_oObject.Connectx ( pField->m_oObject.m_hVBList, aVBLockAttr, 0 );
    //m_oObject      = pField -> r_Object();
}

P3PmsgAttr::P3PmsgAttr ( const P3PmsgObject& rhs )
{
    m_oObject = rhs;
}

P3PmsgAttr::~P3PmsgAttr ( )
{
    Nullify ( );
}

void
P3PmsgAttr::Nullify ( )
{
    m_pP3PmsgField = 0;
    if ( m_pCurs )
      delete m_pCurs;
    m_pCurs        = 0;
    m_oObject.Nullify ( );
}
void
P3PmsgAttr::Connect ( P3PmsgField *pField )
{
    Nullify ( );
    m_pP3PmsgField = pField;
    VBLaddr aVBLockAttr = P2PmsgAttr_GetExtra ( pField );
    m_oObject.Connectx ( pField->m_oObject.m_hVBList, aVBLockAttr, 0 );
}

// Operators
P3PmsgAttr&
P3PmsgAttr:: operator = ( const P3PmsgAttr& rhs )
{
    // To be sure, to be sure
    if ( this == &rhs )
      return *this;
    Truncate ( );
    if ( rhs.IsEmpty() )
      return *this;

    // Implementation
    Create ( );
    m_bAttrDirty = true;

    // Recursively drop existing items and copy
    P3PmsgCurs& oCurs = ((P3PmsgAttr&)rhs).r_Curs();
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.IsList() )
        (*this) += oCurs.r_list();
      else if ( oCurs.IsVect() )
        (*this) += oCurs.r_vect();
      else if ( oCurs.IsItem() )
        (*this) += oCurs.r_item();
      else { ASSERT(0); }
    }

    // Tidy up, and
    if (m_pP3PmsgField) // TODO:LJM delete-me
      r_Object().AssertCommon(m_pP3PmsgField->r_Object() );
    return *this;
}
P3PmsgAttr&
P3PmsgAttr::operator = ( const P3PmsgObject& rhs )
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
    if (m_pP3PmsgField) // TODO:LJM delete-me
      r_Object().AssertCommon(m_pP3PmsgField->r_Object() );
    return *this;
}
P3PmsgAttr&
P3PmsgAttr::operator += ( const P3PmsgAttr& rhs )
{
    ASSERT(this!=&rhs);
    P3PmsgCurs oCurs((P3PmsgAttr&)rhs);
    for ( int i = 0; oCurs.Goto(i); i++ )
    {
      if ( oCurs.IsVect() )
        *this += oCurs.r_vect();
      else if ( oCurs.IsList() )
        *this += oCurs.r_list();
      else if ( oCurs.IsItem() )
        *this += oCurs.r_item();
      else
        ASSERT(0);
    }
    return *this;
}
P3PmsgAttr&
P3PmsgAttr::operator += ( const P3PmsgList& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgAttr&
P3PmsgAttr::operator += ( const P3PmsgVect& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgAttr&
P3PmsgAttr::operator += ( const P3PmsgItem& rhs )
{
    PushBack ( rhs );
    return *this;
}
P3PmsgItem&
P3PmsgAttr::operator [] ( LPCTNAM lpszName )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszName) )
      EVERR -> MODULE
            -> AFP(lpszName)
            -> Message(L"Item [%s] does not exist", lpszName )
            -> Throw();
    return m_pCurs->r_item ( );
}

//  Does this attribute collection denote an item?
//  NOTES: Spelled out rather than leaning on P3PmsgObject's conversion, so
//         that the one question reads the same on every class that answers it.
P3PmsgAttr::operator bool ( ) const
{
    return !r_Object().IsVoid ( );
}

//  Memory management
void
P3PmsgAttr::Create ( )
{
    //if ( P3PmsgAttr__VBLock(this) ) //TODO:LJM deprecated
    if ( OBJ__aVBLock ) ASSERT(VBLock_IsAttr(OBJ__VBLock));
    if ( OBJ__aVBLock )
      return;
    VBLsize nSizeof = VBLockItem_Sizeof(OBJ__uVBLock) + VBLockAttr_Sizeof(OBJ__uVBLock);
    VBLaddr aExtra  = 0;
    if ( m_pP3PmsgField )
      aExtra = m_pP3PmsgField->OBJ__Alloc ( VBLock_Attr, nSizeof );
    else
      aExtra = OBJ__Alloc ( VBLock_Attr, nSizeof );
    VBLock *pVBLock = (VBLock *)OBJ__Msg2Phys ( aExtra );
    VBLockAttr_Init ( OBJ__uVBLock, VBLock_pAttr(pVBLock), AttrField_DEFAULT
                    , m_pP3PmsgField?m_pP3PmsgField->OBJ__VBLocknn:OBJ__VBLocknn );

    if ( m_pP3PmsgField )
    {
      VBLockItem_SetExtra ( OBJ__uVBLock
                          , VBLock_pItem((VBLock *)m_pP3PmsgField->r_Object().GetVBLock())
                          , aExtra );
      Connect ( m_pP3PmsgField );
      ASSERT(m_pP3PmsgField->IsAttributed());
    }
    pVBLock -> oHdr.uVBLockDefs |= VBLock_Linked;
}

//  Chained reference exposures

const P3PmsgObject&
P3PmsgAttr::r_Object ( ) const noexcept
{
    return m_oObject;
}

void
P3PmsgAttr::Drop ( )
{
    Truncate ( );
    // Garbage collection
    // NOTES: Remove linkage prior to releasing allocated block
    if ( OBJ__aVBLock )
    {
      if ( m_pP3PmsgField )
      {  // Clears P2PmsgItem linkage
        VBLock *pVBLock = (VBLock *)m_pP3PmsgField -> r_Object().GetVBLock ( );
        VBLockItem_SetExtra ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock), 0 );
      }  // Remove linkage flag and free
      OBJ__Free ( OBJ__aVBLock );
    }
}

//  Navigation and 
P3PmsgObject
P3PmsgAttr::SelectObject ( LPCTNAM lpszObjectName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszObjectName) )
      return P3PmsgObject();
    return m_pCurs->r_Object();
    //TODO:LJM was return ((P3PmsgField&)*m_pCurs).r_Object();
}
P3PmsgField
P3PmsgAttr::Select ( LPCTNAM lpszItemName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
      return P3PmsgField();
    return *m_pCurs;
}
P3PmsgField&
P3PmsgAttr::SelectItem ( LPCTNAM lpszItemName )
{
    if (  m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    if ( !m_pCurs->Goto(lpszItemName) )
    {
      EVERR -> Module ( __FUNCTION__ )
            -> AFP(lpszItemName)
            -> Message(L"Item [%s] does not exist", lpszItemName )
            -> Throw();
    }
    return *m_pCurs;
}
P3PmsgList&
P3PmsgAttr::SelectList ( LPCTNAM lpszListName )
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
P3PmsgAttr::SelectVect ( LPCTNAM lpszVectName )
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
P3PmsgAttr::DeclareItem ( LPCTNAM lpszItemName, const P3PmsgData& oData, bool bUpdate )
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
    if ( bUpdate )
      m_pCurs->r_item().r_data() = oData;
    return m_pCurs->r_item();
}
bool
P3PmsgAttr::Exists ( LPCTNAM lpszItemName )
{
    if ( OBJ__aVBLock == NULL )
      return false;
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    return m_pCurs->Goto(lpszItemName);
}
bool
P3PmsgAttr::Delete ( LPCTNAM lpszItemName )
{
    if ( OBJ__aVBLock == NULL )
      return false;
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgAttr of all items
    if ( !m_pCurs->Goto(lpszItemName) )
      return false;
    m_pCurs -> Delete ( );
    return true;
}
void
P3PmsgAttr::Truncate ( )
{
    // Initialisation
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );

    // Empty P2PmsgAttr of all items
    while ( m_pCurs->Goto((int)0) )
      m_pCurs -> Delete ( );
    delete m_pCurs;
           m_pCurs = 0;
}
P3PmsgCurs&
P3PmsgAttr::r_Curs ( )
{
    if ( m_pCurs == nullptr )
      m_pCurs = new P3PmsgCurs ( *this );
    return *m_pCurs;
}

// Addressing and allocations
VBLock*
P3PmsgAttr__VBLock  ( const P3PmsgAttr *pAttr )
{
    //VBLock *pVBLock = pAttr -> GetField() -> r_Object().GetVBLock ( );
    //UINT    aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLock, VBLock_pItem(pVBLock) );
    VBLock *pVBAttr =(VBLock *)pAttr -> r_Object().GetVBLock();
    if ( pVBAttr )
    {
      ASSERT(VBLock_IsLinked(pVBAttr));
      ASSERT(VBLock_IsAlloc(pVBAttr));
    }
    return pVBAttr;
}
VBLock*
P3PmsgAttr__VBLock  ( const P3PmsgField *pField )
{
    VBLock *pVBLock = (VBLock *)pField -> r_Object().GetVBLock ( );
    VBLaddr aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    if ( !aExtra )
      return 0;
    VBLock *pVBAttr =(VBLock *)pField -> r_Object().Msg2Phys ( aExtra );
    ASSERT(VBLock_IsItem(pVBLock));
    ASSERT(VBLock_IsLinked(pVBAttr));
    ASSERT(VBLock_IsAlloc(pVBAttr));
    return pVBAttr;
}
static VBLaddr
P3PmsgAttr__GetVBLocknn( const P3PmsgAttr *pThis )
{
    P3PmsgItem *pItem = pThis -> GetField();
    if ( pItem == nullptr )
      return 0;
    VBLock *pVBLock = (VBLock *)pItem -> r_Object().GetVBLock ( );
    VBLaddr aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLockDefs, VBLock_pItem(pVBLock) );
    return  aExtra;
}

//  Troubleshooting
void
P3PmsgAttr::AssertValid ( ) const
{
    //TODO:LJM deprecated VBLock     *pVBLock = P3PmsgAttr__VBLock ( this );
    VBLock *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pVBLock == nullptr )
      return;
    UCHAR       uVBLock = pVBLock->oHdr.uVBLockDefs;
    VBLockAttr *pAttr   = VBLock_pAttr ( pVBLock );

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
    VBLelem nItems = VBLockAttr_GetItems ( uVBLock, pAttr );
    VBLelem nFirst;
    VBLaddr aFirst = VBLockAttr_GetFirst ( uVBLock, pAttr, &nFirst );
    VBLelem nLast;
    VBLaddr aLast  = VBLockAttr_GetLast  ( uVBLock, pAttr, &nLast );
    if ( !nItems && (nFirst || aFirst || nLast!=-1 || aLast) )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted header nItems=%i aFirst=%i aLast=%i",
                        nItems, aFirst, aLast )
            -> Advice ( L"%s", GetField()->GetPath().GetString() )  
            -> Throw();

    // Navigation
    P3PmsgCurs& oCurs = ((P3PmsgAttr *)this)->r_Curs();
                oCurs.AssertValid ( );
	  int i;
    for ( i = 0; oCurs.Goto(i); i++ )
    {
      oCurs.r_Object().AssertCommon ( r_Object() );
      if ( oCurs.IsList() )
        oCurs.r_list().AssertValid();
      else if ( oCurs.IsVect() )
        oCurs.r_vect().AssertValid();
      else if ( oCurs.IsItem() )
        oCurs.r_item().AssertValid();
      else
        ASSERT(0);
    }
    if ( nItems != i )
      EVERR -> Module ( __FUNCTION__ )
            -> Message("Corrupted item count actual=%i expected=%i",
                        nItems, i )
            -> Advice ( L"%s", GetField()->GetPath().GetString() )  
            -> Throw();
}
//
//  Verifies P3PmsgAttr object containment and optionally containment of
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
P3PmsgAttr::VerifyContainment ( void *pvBlob, VBLsize nSizeofBlob ) const
{
    // pVBlob containment within VBLock
    VBLock     *pVBLock = (VBLock *)m_oObject.GetVBLock();
    if ( pvBlob != nullptr && !VBLock_IsContainedVBLump(pVBLock,pvBlob,nSizeofBlob) )
      return FALSE;                    // pVBlob not contained within VBLock

    // VBLockAttr containment
    VBLockAttr *pAttr       = VBLock_pAttr ( pVBLock );
    VBLsize     nSizeofAttr = VBLockAttr_Sizeof ( OBJ__uVBLock );
    if ( !VBLock_IsContainedVBLump(pVBLock,pAttr,nSizeofAttr) )
      return FALSE;                    // pData not contained within VBLock

    // Tidy up and
    return TRUE;
}

void
P3PmsgAttr::Print ( FILE *fd, int nDepthOS, int nDepthOSinc )
{
    // P2PmsgAttr details
    for ( int i = 0; i < nDepthOS; i++ )
      P3Pmsg_fwprintf ( fd, L"  " );
    //TODO:LJM deprecated UINT nVBLockSize = VBLock_Hdr_u_SizeNN( P3PmsgAttr__VBLock(this) );
    if ( OBJ__VBLock )
    {
      VBLsize nVBLockSize = VBLock_Hdr_u_SizeNN( OBJ__VBLock );
      P3Pmsg_fwprintf ( fd, L"@(%Ii) {\n", nVBLockSize );

      // Recursion
      P3PmsgCurs& oCurs = ((P3PmsgAttr *)this)->r_Curs();
      for ( int i = 0; oCurs.Goto(i); i++ )
      {
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
P3PmsgAttr::PushBack ( const P3PmsgField& oField )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    ASSERT(!OBJ__VBLock||VBLock_IsAttr(OBJ__VBLock));
    // Block allocation and linkage
    //P3PmsgObject& oObject     = (P3PmsgObject&)m_pP3PmsgField -> r_Object ( );
    //P2PmsgHANDLE& hVBList     =  oObject.m_hVBList;
    //UCHAR&        uVBLock     =  oObject.m_uVBLock;
    VBLsize       nSizeofItem =  P2PmsgField_SizeofItem ( OBJ__uVBLock, oField );
    VBLaddr       aItem       =  OBJ__Alloc ( VBLock_Item, nSizeofItem );
    VBLock       *pVBLockItem = (VBLock *)OBJ__Msg2Phys(aItem);
if(nSizeofItem==76)ASSERT(1);
    P2PmsgItem_InitField  ( pVBLockItem, oField );
    //TODO:LJM deprecated VBLockAttr *pVBLockAttr = VBLock_pAttr ( P3PmsgAttr__VBLock(this) );
    VBLockAttr *pVBLockAttr = VBLock_pAttr ( OBJ__VBLock );
    if ( GetPermissions(AttrField_SORT) )
      P2PmsgAttr_SortinItem ( this, oField.r_name(), aItem );
    else
      P2PmsgAttr_LinkinItem ( this
                            , VBLockAttr_GetLast(OBJ__uVBLock,pVBLockAttr)
                            , aItem
                            , 0 );
ASSERT(OBJ__uVBLock==(pVBLockItem->oHdr.uVBLockDefs&VBLock_AddrMask));
//AssertValid();//TODO:LJM delete, testing
//oField.AssertValid();//TODO:LJM delete, testing
    // Recursive copy
    nSizeofItem = P2PmsgHeap_Sizeof ( OBJ__hVBList, aItem );
    P3PmsgField oFieldItem ( OBJ__hVBList, aItem, nSizeofItem );
                oFieldItem = oField;

    // Tidy up and
    P2PmsgFieldHdl oHdl = { (UINT_PTR)OBJ__hVBList, aItem, P2PmsgHeap_Sizeof(OBJ__hVBList,aItem) };
    //ASSERT(hVBList==oObject.m_hVBList);
    //ASSERT(uVBLock==oObject.m_uVBLock);
    return oHdl;
}
P2PmsgListHdl
P3PmsgAttr::PushBack ( const P3PmsgList& oList )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    // Block allocation and linkage
    //P3PmsgObject& oObject     = (P3PmsgObject&)m_pP3PmsgField -> r_Object ( );
    //P2PmsgHANDLE& hVBList     = m_pP3PmsgField -> m_hVBList;
    //UCHAR&        uVBLock     = oObject.m_uVBLock;
    VBLsize       nSizeofItem = P2PmsgList_SizeofItem ( OBJ__uVBLock, oList );
    VBLaddr       aItem       = OBJ__Alloc ( VBLock_Item, nSizeofItem );
    P2PmsgList_InitItem   ( (VBLock *)OBJ__Msg2Phys(aItem), oList );
    //TODO:LJM deprecated VBLockAttr *pVBLockAttr = VBLock_pAttr ( P3PmsgAttr__VBLock(this) );
    VBLockAttr *pVBLockAttr = VBLock_pAttr ( OBJ__VBLock );
    if ( GetPermissions(AttrField_SORT) )
      P2PmsgAttr_SortinItem ( this, oList.r_name(), aItem );
    else
      P2PmsgAttr_LinkinItem ( this
                            , VBLockAttr_GetLast(OBJ__uVBLock,pVBLockAttr)
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
P3PmsgAttr::PushBack ( const P3PmsgVect& oVect )
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
    //TODO:LJM deprecated below VBLockAttr *pVBLockAttr = VBLock_pAttr ( P3PmsgAttr__VBLock(this) );
    VBLockAttr *pVBLockAttr = VBLock_pAttr ( OBJ__VBLock );
    if ( GetPermissions(AttrField_SORT) )
      P2PmsgAttr_SortinItem ( this, oVect.r_name(), aItem );
    else
      P2PmsgAttr_LinkinItem ( this
                            , VBLockAttr_GetLast(OBJ__uVBLock,pVBLockAttr)
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
P3PmsgAttr::SetPermissions ( UCHAR uPermissionsAdd, UCHAR uPermissionsRemove )
{
    if ( OBJ__aVBLock == NULL )
      Create ( );
    VBLockAttr *pAttr = P2PmsgObject_pAttr ( OBJ__ );
                pAttr -> uPermissions &= ~uPermissionsRemove;
                pAttr -> uPermissions |=  uPermissionsAdd;
    return pAttr -> uPermissions;
}
UCHAR
P3PmsgAttr::GetPermissions ( UCHAR uPermissionsMask )
{
    if ( OBJ__aVBLock == NULL )
      return 0u;
    const VBLockAttr *pAttr = P2PmsgObject_pAttr ( OBJ__ );
    return pAttr -> uPermissions & uPermissionsMask;
}
VBLelem
P3PmsgAttr::GetCount ( ) const
{
    //TODO:LJM deprecated below VBLock *pVBLock = P3PmsgAttr__VBLock ( this );
    VBLock *pVBLock = OBJ__VBLock;
    if ( pVBLock == NULL )
      return 0;
    VBLockAttr *pAttr   = VBLock_pAttr ( pVBLock );
    return VBLockAttr_GetItems ( pVBLock->oHdr.uVBLockDefs, pAttr );
}

bool
P3PmsgAttr::IsEmpty ( ) const
{
    return GetCount() == 0 ? true : false;
}
P3PmsgField*
P3PmsgAttr::GetField ( ) const
{
    return m_pP3PmsgField;
}

VBLaddr
P2PmsgAttr_GetVBLockParentnn ( const P3PmsgAttr *pAttr )
{
    if ( pAttr->GetField() == 0 )
      return 0;
    //  THE COLLECTION'S OWN BLOCK, not the owning item's. GetField()->r_Object()
    //  is the ITEM -- P3PmsgAttr holds a pointer back to the field it belongs
    //  to, which is what GetField() means -- and reading it through
    //  VBLock_pAttr picks VBLockAttr's fields out of a VBLockItem's ut union.
    //  The parent came back as whatever bytes lay at that offset, and
    //  Msg2Phys of that ran off the arena: P3Pmsg_GetPath segfaulted on a
    //  P3PmsgAttr obtained the ordinary way, oItem.r_Attr().
    //
    //  The collection lives at the item's aExtra, which is what
    //  P3PmsgAttr__GetVBLocknn reads and what MsgAttr's own link routines
    //  write into a child's aParent.
    //TODO:LJM deprecated below VBLock *pVBLock   = P3PmsgAttr__VBLock ( pAttr );
    VBLaddr aVBLockAttr = P3PmsgAttr__GetVBLocknn ( pAttr );
    if ( aVBLockAttr == 0 )
      return 0;                        // No attributes; no collection block
    VBLock *pVBLock   = (VBLock *)pAttr->GetField()->r_Object().Msg2Phys ( aVBLockAttr );
    if ( pVBLock == nullptr || !VBLock_IsAttr(pVBLock) )
      return 0;
    UCHAR   uVBLock   = pVBLock->oHdr.uVBLockDefs;
    ASSERT(VBLock_IsLinked(pVBLock));
    return VBLockAttr_GetParent ( uVBLock, VBLock_pAttr(pVBLock) );
}
VBLock*
P2PmsgAttr__GetVBLockParent ( const P3PmsgAttr *pAttr )
{
    VBLaddr aParent = P2PmsgAttr_GetVBLockParentnn ( pAttr );
    return (VBLock *)pAttr->GetField()->r_Object().Msg2Phys(aParent);
}


//
//  Links passed VBLockItem into VBLockAttr
//  NOTES: Handles bit addressing translations
//
//  Parameters:  P2PmsgVBL
//               Virtual block list manager
//
//               VBLockAttr *pVBLockAttr
//               Attributes into which VBLockItem is to be linked
//
//               UINT aItemPrev
//               VBLock address of the previous VBLockItem
//
//               UINT aItem
//               VBLock address of VBLockItem to be linked in
//
//               UINT aItemNext
//               VBLock address of the next VBLockItem
//  Narrows a heap offset to the width an addressing mode stores it in, refusing
//  the narrowing rather than performing it silently when the value does not fit.
//  NOTES: This is finding M5. Every aParent/aPrev/aNext/aFirst/aLast assignment
//         below is a C-style cast to UINT08/UINT16/UINT32, which truncates in
//         silence. On an Addr08 heap an offset above 255 does not fail, it
//         becomes a DIFFERENT VALID-LOOKING offset -- so the link lands on the
//         wrong block and the free list is quietly corrupted. Nothing downstream
//         can detect that, because there is nothing left to detect: the evidence
//         was the high bits, and they are gone.
//       : The pattern is not new to this codebase. VBLockRoot_SetLast
//         (P2PmsgVBLock.cpp) has tested "value == (value & mask)" before every
//         narrowing since it was written, and throws when it fails. MsgAttr
//         simply never adopted it. This is that function's guard, extracted so
//         the fifteen sites below can share one copy.
//       : Addr32 is checked too, not just the two the finding names. VBLaddr is
//         UINT_PTR -- 64-bit on x64 -- so (UINT32) is a narrowing there as well,
//         and that is where the /W4 C4244 count comes from. On a well-formed
//         heap the value always fits and this costs a compare.
template <typename NARROW__>
static NARROW__
P2PmsgAttr_Narrow ( VBLaddr aAddr, const char *pszField )
{
    const NARROW__ nNarrowed = (NARROW__)aAddr;
    if ( (VBLaddr)nNarrowed != aAddr )
      EVERR->Module ( "P2PmsgAttr_LinkinItem" )
           ->Message ( "VBLock offset 0x%llx does not fit the %u-bit addressing "
                       "mode this heap uses (field %s) - the heap has outgrown "
                       "its addressing width"
                     , (unsigned long long)aAddr
                     , (unsigned)(sizeof(NARROW__) * 8), pszField )
           ->Throw ( );
    return nNarrowed;
}

void
P2PmsgAttr_LinkinItem ( P3PmsgAttr *pAttr
                      , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext )
{
    // Locals
    //TODO:LJM displaced 5/12/2014 VBLock     *pItemPrev   = (VBLock *)pAttr->GetField()->r_Object().Msg2Phys(aItemPrev);
    VBLock     *pItemPrev   = (VBLock *)pAttr->r_Object().Msg2Phys(aItemPrev);
    //TODO:LJM displaced 5/12/2014 VBLock     *pItem       = (VBLock *)pAttr->GetField()->r_Object().Msg2Phys(aItem);
    VBLock     *pItem       = (VBLock *)pAttr->r_Object().Msg2Phys(aItem);
    //TODO:LJM displaced 5/12/2014 VBLock     *pItemNext   = (VBLock *)pAttr->GetField()->r_Object().Msg2Phys(aItemNext);
    VBLock     *pItemNext   = (VBLock *)pAttr->r_Object().Msg2Phys(aItemNext);
    //TODO:LJM deprecated below VBLock     *pVBLock     =           P3PmsgAttr__VBLock(pAttr);  //TODO:LJM deprecated pAttr->GetVBLock();
    VBLock     *pVBLock     = (VBLock *)pAttr->r_Object().GetVBLock();
    VBLockAttr *pVBLockAttr = VBLock_pAttr( pVBLock );

    VBLockItem *pItem_nn    = VBLock_pItem(pItem);

    // Observe addressing model
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr08 )
    {
      pItem_nn->ua.oItem08.aParent
        = P2PmsgAttr_Narrow<UINT08> ( P3PmsgAttr__GetVBLocknn(pAttr), "aParent" );
      goto P08;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr16 )
    {
      pItem_nn->ua.oItem16.aParent
        = P2PmsgAttr_Narrow<UINT16> ( P3PmsgAttr__GetVBLocknn(pAttr), "aParent" );
      goto P16;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr32 )
    {
      pItem_nn->ua.oItem32.aParent
        = P2PmsgAttr_Narrow<UINT32> ( P3PmsgAttr__GetVBLocknn(pAttr), "aParent" );
      goto P32;
    }
    if ( (pVBLock->oHdr.uVBLockDefs&VBLock_AddrMask) == VBLock_Addr64 )
    {
      pItem_nn->ua.oItem64.aParent = (UINT64)P3PmsgAttr__GetVBLocknn(pAttr);
      goto P64;
    }
    EVERR->Module ("P2PmsgAttr")
         ->Message("Internal VBLockAttr.uVBLockAddr.ua corruption" )
         ->Throw ( );

    // P2PmsgItem - Prev Linkages
P08:pItem_nn->ua.oItem08.aPrev = P2PmsgAttr_Narrow<UINT08>(aItemPrev,"aPrev");
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem08.aNext = P2PmsgAttr_Narrow<UINT08>(aItem,"aNext");
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N08;
P16:pItem_nn->ua.oItem16.aPrev = P2PmsgAttr_Narrow<UINT16>(aItemPrev,"aPrev");
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem16.aNext = P2PmsgAttr_Narrow<UINT16>(aItem,"aNext");
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N16;
P32:pItem_nn->ua.oItem32.aPrev = P2PmsgAttr_Narrow<UINT32>(aItemPrev,"aPrev");
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem32.aNext = P2PmsgAttr_Narrow<UINT32>(aItem,"aNext");
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N32;
P64:pItem_nn->ua.oItem64.aPrev = (UINT64)aItemPrev;
    if ( pItemPrev )
      VBLock_pItem(pItemPrev)->ua.oItem64.aNext = (UINT64)aItem;
    pItem -> oHdr.uVBLockDefs |= VBLock_Linked;
    goto N64;

    // P2PmsgItem - Next Linkages
N08:pItem_nn->ua.oItem08.aNext = P2PmsgAttr_Narrow<UINT08>(aItemNext,"aNext");
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem08.aPrev = P2PmsgAttr_Narrow<UINT08>(aItem,"aPrev");
    goto H08;
N16:pItem_nn->ua.oItem16.aNext = P2PmsgAttr_Narrow<UINT16>(aItemNext,"aNext");
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem16.aPrev = P2PmsgAttr_Narrow<UINT16>(aItem,"aPrev");
    goto H16;
N32:pItem_nn->ua.oItem32.aNext = P2PmsgAttr_Narrow<UINT32>(aItemNext,"aNext");
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem32.aPrev = P2PmsgAttr_Narrow<UINT32>(aItem,"aPrev");
    goto H32;
N64:pItem_nn->ua.oItem64.aNext = (UINT64)aItemNext;
    if ( pItemNext )
      VBLock_pItem(pItemNext)->ua.oItem64.aPrev = (UINT64)aItem;
    goto H64;

    // P2PmsgAttr - Housekeeping
H08:if ( pItem_nn->ua.oItem08.aPrev == 0 )
      pVBLockAttr->u.oAttr08.aFirst = P2PmsgAttr_Narrow<UINT08>(aItem,"aFirst");
    if ( pItem_nn->ua.oItem08.aNext == 0 )
      pVBLockAttr->u.oAttr08.aLast  = P2PmsgAttr_Narrow<UINT08>(aItem,"aLast");
    pVBLockAttr -> u.oAttr08.nItems++;
    goto END;
H16:if ( pItem_nn->ua.oItem16.aPrev == 0 )
      pVBLockAttr->u.oAttr16.aFirst = P2PmsgAttr_Narrow<UINT16>(aItem,"aFirst");
    if ( pItem_nn->ua.oItem16.aNext == 0 )
      pVBLockAttr->u.oAttr16.aLast  = P2PmsgAttr_Narrow<UINT16>(aItem,"aLast");
    pVBLockAttr -> u.oAttr16.nItems++;
    goto END;
H32:if ( pItem_nn->ua.oItem32.aPrev == 0 )
      pVBLockAttr->u.oAttr32.aFirst = P2PmsgAttr_Narrow<UINT32>(aItem,"aFirst");
    if ( pItem_nn->ua.oItem32.aNext == 0 )
      pVBLockAttr->u.oAttr32.aLast  = P2PmsgAttr_Narrow<UINT32>(aItem,"aLast");
    pVBLockAttr -> u.oAttr32.nItems++;
    goto END;
H64:if ( pItem_nn->ua.oItem64.aPrev == 0 )
      pVBLockAttr->u.oAttr64.aFirst = (UINT64)aItem;
    if ( pItem_nn->ua.oItem64.aNext == 0 )
      pVBLockAttr->u.oAttr64.aLast  = (UINT64)aItem;
    pVBLockAttr -> u.oAttr64.nItems++;
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
P2PmsgAttr_SortinItem ( P3PmsgAttr *pAttr, const P3PmsgName& oName, VBLaddr aItem )
{
    // Locals
    P3PmsgCurs oCurs ( *pAttr );
    VBLaddr aItemPrev = 0;
    VBLaddr aItemNext = 0;
    // Optimisation - Empty list
    if ( pAttr->GetCount() <= 0 )
    {
//fwprintf(stdout,"oName=%s\n",oName.c_name());
      P2PmsgAttr_LinkinItem ( pAttr, 0, aItem, 0 );
      return;
    }

    // Optimisation - First in list
    int  imin = 0;
    if ( oCurs.Goto(imin) && oName < oCurs.r_name() )
    {
//fwprintf(stdout,"oName=%s < r_name(imin)=%s \n", oName.c_name(), oCurs.r_name().c_name() );
      P2PmsgAttr_LinkinItem ( pAttr, 0, aItem, P3PmsgCurs_GetVBLocknn(oCurs) );
      return;
    }

    // Optimisation - Last in list
    int  imax = pAttr->GetCount() - 1;
    if ( oCurs.Goto(imax) && oName > oCurs.r_name() )
    {
//fwprintf(stdout,"r_name(imin)=%s < oName=%s\n",oCurs.r_name().c_name(), oName.c_name() );
      P2PmsgAttr_LinkinItem ( pAttr, P3PmsgCurs_GetVBLocknn(oCurs), aItem, 0 );
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
    P2PmsgAttr_LinkinItem ( pAttr, aItemPrev, aItem, aItemNext );
}

VBLaddr
P2PmsgAttr_UnLinkItem ( P3PmsgAttr *pAttr, VBLockAttr *pVBLockAttr, VBLaddr aItem )
{
    P3PmsgField *pField      = pAttr -> GetField();
    VBLock      *pVBLockPrev = 0;
    VBLock      *pVBLock     =(VBLock *)pField->r_Object().Msg2Phys(aItem);
    ASSERT(VBLock_IsLinked(pVBLock));
    VBLockItem  *pItem       = VBLock_pItem ( pVBLock );
    VBLock      *pVBLockNext = 0;

    UCHAR   uVBLock     = pVBLock->oHdr.uVBLockDefs;

    VBLaddr aItemPrev   = VBLockItem_GetPrev ( uVBLock, pItem );
    if ( aItemPrev )
      pVBLockPrev = (VBLock *)pField -> r_Object().Msg2Phys(aItemPrev);
    VBLaddr aItemNext   = VBLockItem_GetNext ( uVBLock, pItem );
    if ( aItemNext )
      pVBLockNext = (VBLock *)pField -> r_Object().Msg2Phys(aItemNext);

    // Item housekeeping
    // NOTES: Remove linkages etc
    if ( aItemPrev )
      VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aItemNext );
    if ( aItemNext )
      VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aItemPrev );

    // Attr housekeeping
    // NOTES: Counts and linkages etc
    VBLockAttr_SetItems ( uVBLock
                        //TODO:LJM deprecated below, VBLock_pAttr(P3PmsgAttr__VBLock(pAttr))
                        , VBLock_pAttr((VBLock *)pAttr->r_Object().GetVBLock())
                        , -1, false );
    if ( VBLockAttr_GetFirst(uVBLock,pVBLockAttr,0) == aItem )
      VBLockAttr_SetFirst ( uVBLock, pVBLockAttr, aItemNext );
    if ( VBLockAttr_GetLast(uVBLock,pVBLockAttr,0) == aItem )
      VBLockAttr_SetLast ( uVBLock, pVBLockAttr, aItemPrev );

    // Isolate
    VBLockItem_SetPrev  ( uVBLock, pItem, 0 );
    VBLockItem_SetNext  ( uVBLock, pItem, 0 );
    VBLockItem_SetParent( uVBLock, pItem, 0 );
    return aItem;
}

VBLaddr
P2PmsgAttr_UnLinkItem ( P3PmsgObject *pObject, VBLockAttr *pVBLockAttr, VBLaddr aItem )
{
    //P3PmsgField *pField      = pAttr -> GetField();
    VBLock      *pVBLockPrev = 0;
    VBLock      *pVBLock     =(VBLock *)pObject->Msg2Phys(aItem);
    ASSERT(VBLock_IsLinked(pVBLock));
    ASSERT(VBLock_IsAlloc(pVBLock));
    VBLockItem  *pItem       = VBLock_pItem ( pVBLock );
    VBLock      *pVBLockNext = 0;

    UCHAR   uVBLock     = pVBLock->oHdr.uVBLockDefs;

    VBLaddr aItemPrev   = VBLockItem_GetPrev ( uVBLock, pItem );
    if ( aItemPrev )
      pVBLockPrev = (VBLock *)pObject->Msg2Phys(aItemPrev);
    VBLaddr aItemNext   = VBLockItem_GetNext ( uVBLock, pItem );
    if ( aItemNext )
      pVBLockNext = (VBLock *)pObject->Msg2Phys(aItemNext);

    // Item housekeeping
    // NOTES: Remove linkages etc
    if ( aItemPrev )
      VBLockItem_SetNext ( uVBLock, VBLock_pItem(pVBLockPrev), aItemNext );
    if ( aItemNext )
      VBLockItem_SetPrev ( uVBLock, VBLock_pItem(pVBLockNext), aItemPrev );

    // Attr housekeeping
    // NOTES: Counts and linkages etc
    VBLockAttr_SetItems ( uVBLock
                        //TODO:LJM deprecated below, VBLock_pAttr(P3PmsgAttr__VBLock(pAttr))
                        , pVBLockAttr, -1, false );
    if ( VBLockAttr_GetFirst(uVBLock,pVBLockAttr,0) == aItem )
      VBLockAttr_SetFirst ( uVBLock, pVBLockAttr, aItemNext );
    if ( VBLockAttr_GetLast(uVBLock,pVBLockAttr,0) == aItem )
      VBLockAttr_SetLast ( uVBLock, pVBLockAttr, aItemPrev );

    // Isolate
    VBLockItem_SetPrev  ( uVBLock, pItem, 0 );
    VBLockItem_SetNext  ( uVBLock, pItem, 0 );
    VBLockItem_SetParent( uVBLock, pItem, 0 );
    return aItem;
}

VBLaddr
P2PmsgAttr_GetExtra ( P3PmsgField *pField )
{
    VBLock *pVBLock = (VBLock *)pField -> r_Object().GetVBLock();
    VBLaddr aExtra  = VBLockItem_GetExtra ( pVBLock->oHdr.uVBLockDefs
                                          , VBLock_pItem(pVBLock) );
    return aExtra;
}


//
//  Hidden addressing utilities
VBLockAttr*
P2PmsgObject_pAttr ( const P3PmsgObject& oObject )
{
    VBLock *pVBLock = (VBLock *)oObject.GetVBLock ( );
    ASSERT(VBLock_IsLinked(pVBLock));
    ASSERT(VBLock_IsAlloc(pVBLock));
    return VBLock_pAttr ( pVBLock );
}
