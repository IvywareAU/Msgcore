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
//  Descendant container and manipulation utilities for the P3P messaging
//  framework.
//  NOTES: P3PmsgDesc provides a VBLock-backed interface for managing descendant
//         (child) P3PmsgItem collections associated with a P3PmsgField. It
//         represents the hierarchical structure of message content, enabling
//         construction and navigation of nested object graphs.
//       : Descendants form ordered collections of items that may themselves
//         contain attributes and further descendants, supporting fully
//         recursive message structures.
//
//  Architecture Overview
//       : P3PmsgDesc
//         High-level descendant container interface
//       : VBLockDesc
//         Underlying packed memory representation (offset-based)
//  Each descendant collection may contain:
//       : Fields (P3PmsgField)
//       : Lists  (P3PmsgList)
//       : Vectors(P3PmsgVect)
//       : Nested descendant hierarchies
//  Integration:
//       : Works alongside P3PmsgAttr (attributes)
//       : Traversed via P3PmsgCurs (cursor layer)
//
//  Key Responsibilities
//       : Creation and destruction of descendant containers
//       : Insertion, deletion, and truncation of child items
//       : Name-based and indexed selection of contained elements
//       : Composition via operator overloading (+=)
//       : Maintenance of ordering and sorted insertion
//       : Exposure of cursor-based traversal (P3PmsgCurs)
//
//  Memory Model
//       : Backed by VBLockDesc structures within VBLock heaps
//       : Relationships maintained via VBLaddr offsets (no raw pointers)
//       : Supports recursive containment and hierarchical linking
//       : Underlying storage is packed and relocation-safe
//
//  Low-Level Operations
//  Static P2PmsgDesc_* functions operate directly on VBLock structures:
//       : Link/unlink items via address manipulation
//       : Sorted insertion based on item names
//       : Direct descendant retrieval and swapping
//  These functions bypass higher-level safety checks and require the caller
//  to ensure address validity and containment integrity.
//
//  Diagnostics & Validation
//       : Structure validation (AssertValid)
//       : Containment verification against raw memory regions
//       : Debug printing of hierarchical content
//
//  SUMMARY:
//       : Descendants represent the primary hierarchical structure of P3P messages
//       : Strong coupling with P3PmsgField, P3PmsgItem, and VBLockDesc
//       : Designed for efficient in-place manipulation without copying
//       : Use class methods for safe operations; static functions for low-level control
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
#include "MsgCurs.h"

class P3PmsgList;
class P3PmsgVect;
class P3PmsgCurs;
class P3PmsgAttr;
class MsgStck;
class P3PmsgField;
class P3PmsgDesc;


#define DESC r_Desc()
class Msgcore_EXT P3PmsgDesc
{
      friend P3PmsgCurs;

    // Constructors and destructor
    public:
        P3PmsgDesc ( ) noexcept;

        P3PmsgDesc ( const P3PmsgDesc& rhs );

        P3PmsgDesc ( P3PmsgField *pField );

        P3PmsgDesc ( const P3PmsgObject& rhs );

      virtual
       ~P3PmsgDesc ( );

      void
        Nullify ( ); 
      void
        Connect ( P3PmsgField *pField );

    // Operators
    public:
      P3PmsgDesc&
        operator  = ( const P3PmsgDesc& rhs );
      P3PmsgDesc&
        operator  = ( const P3PmsgObject& rhs );
      P3PmsgDesc&
        operator += ( const P3PmsgList& rhs );
      P3PmsgDesc&
        operator += ( const P3PmsgVect& rhs );
      P3PmsgDesc&
        operator += ( const P3PmsgField& rhs );
      virtual P3PmsgItem&
        operator [] ( LPCTNAM lpszName );

        operator bool ( ) const;

    // Chained reference exposures
    public:
      const P3PmsgObject&
        r_Object ( ) const noexcept;

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
        DeclareItem ( LPCTNAM lpszFieldName, const P3PmsgData& oData, BOOL bUpdate = false );
      bool
        Exists ( LPCTNAM lpszItemName );
      bool
        Delete ( LPCTNAM lpszItemName );
      bool
        Delete ( P3PmsgItem& oItem );
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
        SetPermissions ( UCHAR uPermissionsAdd, UCHAR uPermissionsRemove = 0 );
      UCHAR
        GetPermissions ( UCHAR uPermissionsMask = ~(UCHAR)0 );
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
      bool          m_bDescDirty{false};
};

// P3PmsgDesc static manipulators
// NOTES: These functions operate at the memory structure level, manipulating
//        raw VBLaddr offsets and VBLock structures. Caller must ensure validity
//        of addresses and containment when using these functions.  Use the
//        P3PmsgDesc class methods for higher-level descendant management with
//        better built-in safety checks.
VBLaddr
P2PmsgDesc_GetDescn  ( P3PmsgField *pField );
void
P2PmsgDesc_LinkinItem( P3PmsgDesc *pDesc, VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext );
VBLaddr
P2PmsgDesc_UnLinkItem( P3PmsgObject *pObject, VBLockDesc *pVBLockDesc, VBLaddr aItem );
void
P2PmsgDesc_SortinItem ( P3PmsgDesc *pDesc, const P3PmsgName& oName
                      , VBLaddr aItem );
Msgcore_EXT void
P2PmsgDesc_Swap ( P3PmsgItem& oItem1, P3PmsgItem& oItem2 );
Msgcore_EXT BOOL
P2PmsgDesc_WCDelete ( P3PmsgDesc& oDesc, LPCTSTR lpszWildcard );
