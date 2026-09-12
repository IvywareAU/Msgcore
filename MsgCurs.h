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
//  Cursor and recursive traversal utilities for the P2P/P3P messaging
//  framework.
//  NOTES: This module provides navigation, iteration, and structured access to
//         P3PmsgItem hierarchies stored within VBLock-managed heaps. It
//         abstracts low-level address-based traversal into a cursor-oriented
//         interface, enabling safe and intuitive access to fields, lists,
//         vectors, attributes, and descendant structures.
//
//  Architecture Overview
//       : P3PmsgCurs
//         Primary cursor for linear and indexed navigation
//       : P2PmsgRecurs
//         Recursive traversal helper for hierarchical scanning
//
//  The cursor operates over:
//       : P3PmsgItem   (generic node)
//       : P3PmsgField  (name/data pairs)
//       : P3PmsgList   (linked collections)
//       : P3PmsgVect   (indexed collections)
//       : P3PmsgAttr   (descendant attribute heirarchies)
//       : P3PmsgDesc   (descendant hierarchies)
//
//  Key Responsibilities
//       : Sequential navigation (++, --) across items
//       : Direct access via name or index (Goto, Seek)
//       : Exposure of underlying data, name, and container types
//       : Safe casting to field, list, vector, and data representations
//       : Deletion and manipulation of current cursor target
//
//  Recursive Traversal
//  P2PmsgRecurs provides depth-first traversal of message hierarchies:
//       : Supports push/pop traversal state management
//       : Enables scanning of nested attributes and descendants
//       : Maintains traversal context across recursion levels
//       : Allows early termination via Break()
//
//  Data Exposure Model
//       : Cursor acts as a typed view over the current VBLock-backed item
//       : Conversion operators expose current element as 
//         P3PmsgField, P3PmsgList, P3PmsgVect, P3PmsgData, P3PmsgName
//       : Accessors (r_*) provide explicit, reference-based access
//
//  SUMMARY
//       : Built on top of VBLock address-based structures (no raw pointer
//         traversal)
//       : Cursor state is transient and must not outlive underlying heap
//         validity
//       : Strong coupling with P3Pmsg object model and Msgcore infrastructure
//       : Designed for efficient traversal without copying underlying data
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
#include "MsgList.h"
#include "MsgVect.h"
#include "MsgDesc.h"


class P3PmsgField;
class P3PmsgList;
class P3PmsgVect;
class P3PmsgDesc;
class MsgStck;

///////////////////////////////////////////////////////////////////////
//  P2PmsgCurs'or
//  NOTES: Manages P2Pmsg navigation and subsequent data exposure

class Msgcore_EXT P3PmsgCurs
{
      void
        RecycleThis ( );
    // Constructors and destructor
    public:
        P3PmsgCurs ( );
        P3PmsgCurs ( const P3PmsgCurs& rhs );
        P3PmsgCurs ( P3PmsgItem& rhs );
        P3PmsgCurs ( P3PmsgDesc& rhs );
        P3PmsgCurs ( P3PmsgAttr& rhs );
      virtual
       ~P3PmsgCurs ( );

    // Operators
    public:
      virtual P3PmsgCurs&
        operator = ( const P3PmsgCurs& rhs );
      virtual P3PmsgCurs&
        operator = ( const P3PmsgItem& rhs );
      virtual P3PmsgCurs&
        operator ++ ( );
      virtual P3PmsgCurs
        operator ++ ( int );
      virtual P3PmsgCurs&
        operator -- ( );
      virtual
        operator P3PmsgName& ( );
      virtual
        operator P3PmsgField& ( );
      virtual
        operator P3PmsgList& ( );
      virtual
        operator P3PmsgVect& ( );
      virtual
        operator P3PmsgData& ( );

    // Operations
    public:
      void
        Delete ( );

    // Navigation
    public:
      P3PmsgCurs&
        Seek ( );
      bool
        Goto ( LPCTNAM lpszItemName );
      bool
        Goto ( int nElem );

    // Exposure
    public:
      virtual const P3PmsgObject&
        r_Object ( ) const;
      virtual LPCTNAM
        c_wstr ( );
      virtual P3PmsgData&
        r_data ( ) const;
      virtual P3PmsgName&
        r_name ( );
      virtual P3PmsgItem&
        r_item ( ) const;
      virtual P3PmsgList&
        r_list ( );
      virtual P3PmsgVect&
        r_vect ( );
      virtual P3PmsgAttr&
        r_attr ( ) const;
      virtual P3PmsgDesc&
        r_desc ( ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;

    // Properties
    public:
      bool
        IsItem ( );
      //bool
      //  IsNode ( );
      bool
        IsList ( );
      bool
        IsVect ( );
      VBLelem
        Item ( );
      VBLelem
        GetCount ( ) const;
      virtual bool
        IsSoCursor ( );
      virtual bool 
        IsEoCursor ( ) const;

      //  How many references on hVBList this cursor is holding.  Refer
      //  P3PmsgField::HeapHolders.
      int
        HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept;

    // Attributes
    protected:
      P3PmsgItem   *m_pItemParent{nullptr};
      P3PmsgAttr   *m_pP3PmsgAttr{nullptr};
      P3PmsgDesc   *m_pP3PmsgDesc{nullptr};
      P3PmsgField   m_oP3PmsgField;
      P3PmsgList    m_oP3PmsgList;
      P3PmsgVect    m_oP3PmsgVect;
      P3PmsgField  *m_pP3PmsgFoN{nullptr};
      VBLelem       m_nItem{0};
};

///////////////////////////////////////////////////////////////////////
//  P2PmsgRecurs
//  NOTES: Provides recursive P3PmsgItem scanning services

class Msgcore_EXT P2PmsgRecurs
{
      void
        RenderRecursSafe ( );

    // Constructors and destructor
    public:
        P2PmsgRecurs ( const P2PmsgRecurs& rhs );
        P2PmsgRecurs ( P3PmsgItem& rhs );
        P2PmsgRecurs ( P3PmsgAttr& rhs );
      virtual
       ~P2PmsgRecurs ( );

    // Operators
    public:
      virtual P2PmsgRecurs&
        operator = ( const P2PmsgRecurs& rhs );
      virtual P2PmsgRecurs&
        operator ++ ( );

    // Operations
    public:

    // Navigation
    public:
      int
        Push ( );
      int
        Pop ( );
      void
        Break ( );

    // Exposure
    public:
      virtual LPCTNAM
        c_wstr ( );
      virtual P3PmsgAttr&
        r_attr ( ) const;
      virtual P3PmsgData&
        r_data ( ) const;
      virtual P3PmsgName&
        r_name ( );
      virtual P3PmsgItem&
        r_item ( ) const;
      virtual P3PmsgList&
        r_list ( );
      virtual P3PmsgVect&
        r_vect ( );

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;

    // Properties
    public:
      bool
        IsField ( );
      bool
        IsList ( );
      bool
        IsVect ( );
      bool
        IsEoRecurs ( ) const;

    // Attributes
    protected:
      P3PmsgCurs     m_oCurs;
      P2PmsgRecurs  *m_pRecurs{nullptr};
      int            m_cLevel;
};

VBLock*
P3PmsgCurs_GetVBlock ( P3PmsgCurs& oCurs );

VBLsize
P3PmsgCurs_GetVBLocknn ( P3PmsgCurs& oCurs );
