// Copyright © 2007-2013, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//
//  P3Pmsg attribute definitions and prototypes
//  NOTES: Low-level attribute container for the P3P messaging framework.
//         Optional P3PmsgItem qualifications and/or properties. Denoted
//         by '@' path character.  All P3PmsgItem's support Attributes.
//       : Contains list of P3PmsgItem's that may contain further recursive
//         Attributes or Descendant P3PmsgItem's.
//       : P3PmsgAttr provides a VBLock-backed interface for managing attribute
//         collections attached to P3PmsgField instances. Internally, attributes
//         are stored as packed, binary addressable elements within a contiguous
//         VBLock heap, enabling efficient transport, persistence, and
//         binary VBLOck inter-module exchange.
//       : Each attribute collection forms a node in a hierarchical structure
//         where elements may be:
//           -Fields (P3PmsgField)
//           -Lists  (P3PmsgList)
//           -Vectors(P3PmsgVect)
//           -Nested attribute/object trees
//       : Linkage between elements is maintained via VBLaddr offsets, allowing
//         insertion, removal, and sorting without pointer invalidation.
//       : Key Responsibilities
//           -Manage creation/destruction of attribute storage within VBLock heaps
//           -Provide linking, unlinking, and sorted insertion of attribute items
//           -Maintain referential integrity across packed memory blocks
//           -Support recursive traversal via P3PmsgCurs
//           -Enable merging of attribute sets at the memory structure level
//  SUMMARY
//       : Designed for high-performance message passing with minimal copying
//       : All relationships are offset-based (VBLaddr), not raw pointers
//       : Tight coupling with P2PmsgVBLock and Msgcore memory model
//       : Caller must ensure containment validity when manipulating raw blobs
//
#pragma once

#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif

#include "Msgcore.h"
#include "wtypes.h"
#include "comutil.h"

#include "P2Pmsg.h"
#include "P2PmsgVBLock.h"

#define ATTR r_Attr()

class P3PmsgField;
class P3PmsgList;
class P3PmsgVect;
class P3PmsgCurs;
class P3PmsgDesc;
class MsgStck;
class Msgcore_EXT P3PmsgAttr
{
    friend P3PmsgCurs;

    // Constructors and destructor
    public:
      P3PmsgAttr ( );

      P3PmsgAttr ( const P3PmsgAttr& rhs );

      P3PmsgAttr ( P3PmsgField *pField );

      P3PmsgAttr ( const P3PmsgObject& rhs );

    virtual
     ~P3PmsgAttr ( );

    void
      Nullify ( ); 
    void
      Connect ( P3PmsgField *pField );

    // Operators
    public:
      P3PmsgAttr&
        operator  = ( const P3PmsgAttr& rhs );
      P3PmsgAttr&
        operator  = ( const P3PmsgObject& rhs );
      P3PmsgAttr&
        operator += ( const P3PmsgAttr& rhs );
      P3PmsgAttr&
        operator += ( const P3PmsgList& rhs );
      P3PmsgAttr&
        operator += ( const P3PmsgVect& rhs );
      P3PmsgAttr&
        operator += ( const P3PmsgItem& rhs );
      virtual P3PmsgItem&
        operator [] ( LPCTNAM lpszName );

      //  `if ( oAttr )` and nothing else.  Refer P3PmsgObject::operator bool.
      explicit
        operator bool ( ) const;

    // Chained reference exposures
    public:
      const P3PmsgObject&
        r_Object ( ) const noexcept;

      //  How many references on hVBList this collection and the cursor it owns
      //  are holding.  Refer P3PmsgField::HeapHolders.
      int
        HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept;

    // Memory management
    public:
      virtual void
        Create ( );
      virtual void
        Drop ( );

    // Navigation and 
    public:
      P3PmsgField
        Select      ( LPCTNAM lpszItemName );
      P3PmsgObject
        SelectObject( LPCTNAM lpszObjectname );
      P3PmsgField&
        SelectItem  ( LPCTNAM lpszItemName );
      P3PmsgList&
        SelectList  ( LPCTNAM lpszListName );
      P3PmsgVect&
        SelectVect  ( LPCTNAM lpszVectName );

    P3PmsgField&
      DeclareItem ( LPCTNAM lpszFieldName, const P3PmsgData& oData, bool bUpdate = false );
    bool
      Exists ( LPCTNAM lpszItemName );
    bool
      Delete ( LPCTNAM lpszItemName );
    void
      Truncate ( );
    P3PmsgCurs&
      r_Curs ( );

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual BOOL
        VerifyContainment ( void *pvBlob = nullptr, VBLsize = 0 ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1 );

    // Std::List modifiers
    public:
      P2PmsgFieldHdl
        PushBack ( const P3PmsgField& oField );
      P2PmsgListHdl
        PushBack ( const P3PmsgList& oList );
      P2PmsgVectHdl
        PushBack ( const P3PmsgVect& oVect );

    // Properties
    public:
      UCHAR
        SetPermissions ( UCHAR uPermissionAdd, UCHAR uPermissionRemove = 0 );
      UCHAR
        GetPermissions ( UCHAR uPermissionMask = ~(UCHAR)0 );
      VBLelem
        GetCount ( ) const;
      virtual bool
        IsEmpty ( ) const;
      P3PmsgField*
        GetField ( ) const;

    // Attributes
    protected:
      P3PmsgObject  m_oObject;
      P3PmsgField  *m_pP3PmsgField{nullptr};
      P3PmsgCurs   *m_pCurs{nullptr};
      bool          m_bAttrDirty{false};
};

// P3PmsgAttr static manipulators
// NOTES: These functions operate at the memory structure level, manipulating
//        raw VBLaddr offsets and VBLock structures. Caller must ensure validity
//        of addresses and containment when using these functions.  Use the
//        P3PmsgAttr class methods for higher-level attribute management with
//        better built-in safety checks.
VBLaddr
P2PmsgAttr_GetExtra  ( P3PmsgField *pField );
void
P2PmsgAttr_LinkinItem( P3PmsgAttr *pAttr
                     , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext );
void
P2PmsgAttr_SortinItem ( P3PmsgAttr *pAttr, const P3PmsgName& oName
                      , VBLaddr aItem );
VBLaddr
P2PmsgAttr_UnLinkItem( P3PmsgObject *pObject, VBLockAttr *pVBLockAttr, VBLaddr aItem );
VBLaddr
P2PmsgAttr_GetVBLockParentnn ( const P3PmsgAttr *pAttr );

Msgcore_EXT P3PmsgAttr&
P2PmsgAttr_Merge ( P3PmsgAttr& oAttr, const P3PmsgAttr& rhs );

