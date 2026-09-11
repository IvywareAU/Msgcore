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
//  P2Pmsg definitions and prototypes
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
//#include "P2PmsgVBLock.h"
//#include "P2Peer.h"
//#include "P2PTL.h"

///////////////////////////////////////////////////////////////////////////////
//  VBLockStack containers and definitions
//  NOTES: Base class for VBLockNode structures
//       : Intended for standalone or derived instanciation
//#pragma pack(push,1)
//typedef struct
//{
//    UCHAR  uStackItem;                 // Pushed item attributes
//    UINT   aStackItem;                 // Address of pushed item
//    union
//    {
//      VBLockNode   oVBLockNode;
//      VBLockField  oVBLockField;
//      VBLockList   oVBLockList;
//    } u;
//} VBLockStack;
//#pragma pack(pop)
//
//typedef struct
//{
//    UINT uiParam1;
//    UINT uiParam2;
//    UINT uiParam3;
//} P2PmsgStckHdl;

///////////////////////////////////////
//  MsgStck class
//  NOTES: Manages stacking of message items
class P3PmsgField;
class P3PmsgList;
class P3PmsgVect;
//class P3PmsgNode;
class P3PmsgCurs;
class P3PmsgDesc;
class Msgcore_EXT MsgStck
{
    // Constructors and destructor
    public:
        MsgStck ( );

        MsgStck ( const MsgStck& rhs );

        MsgStck ( const P3PmsgField *pField );

        MsgStck ( const P3PmsgObject& rhs );
      virtual
       ~MsgStck ( );

      void
        Nullify ( ) noexcept; 
      void
        Connect ( P3PmsgField *pField ) noexcept;
      virtual MsgStck&
        Push ( );
      virtual MsgStck&
        Pop ( );

    // Operators
    public:
      MsgStck&
        operator  = ( const MsgStck& rhs );

    // Memory management
    public:
      virtual void
        Drop ( );

    // Navigation and 
    public:
      MsgStck&
        Rename ( LPCTNAM lpszName, bool bRecurse = true );

    // Exposure
    public:
      virtual P3PmsgData&
        r_data ( );
      virtual P3PmsgName&
        r_name ( );
      virtual P3PmsgItem&
        r_item ( );
      virtual P3PmsgList&
        r_list ( );
      virtual P3PmsgVect&
        r_vect ( );
      //virtual P3PmsgNode&
      //  r_node ( );

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1 );

    // Properties
    public:
      virtual bool
        IsEmpty ( ) const;
      P3PmsgField*
        GetField ( ) noexcept;

      //  How many references on hVBList this stack is holding.  Refer
      //  P3PmsgField::HeapHolders.
      int
        HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept;

    // Attributes
    protected:
      P3PmsgField  *m_pP3PmsgField{0};

      P3PmsgField  *m_pStckField{0};
      //P3PmsgNode   *m_pStckNode{0};
      P3PmsgList   *m_pStckList{0};
      P3PmsgVect   *m_pStckVect{0};
      bool          m_bStckDirty{false};
};
