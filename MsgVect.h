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
//#include "P2Peer.h"
//#include "P2PTL.h"


class P3PmsgField;
class P3PmsgList;
class P3PmsgVect;
//class P3PmsgNode;
class P3PmsgCurs;
class P3PmsgDesc;
class MsgStck;


class Msgcore_EXT P3PmsgVect : public P3PmsgField
{
      void
        RenderThisSafe ( );

    // Constructors and destructor
    public:
        P3PmsgVect ( );

        P3PmsgVect ( const P3PmsgVect& rhs );

        P3PmsgVect ( int nElems, LPCTNAM lpszName, const P3PmsgData& oData );

        P3PmsgVect ( int nElems, const P3PmsgField& oField );

        P3PmsgVect ( const P2PmsgListHdl& hList );

        P3PmsgVect ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nListSize );

        P3PmsgVect ( const P3PmsgObject& rhs );
      virtual
       ~P3PmsgVect ( );

      void
        Connect ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nListSize );

    // Operators
    public:
      P3PmsgVect&
        operator  = ( const P3PmsgVect& rhs );
      P3PmsgVect&
        operator  = ( const P3PmsgField& rhs );
      P3PmsgVect&
        operator  = ( const P3PmsgData& rhs );
      P3PmsgVect&
        operator = ( const P2PmsgVectHdl& rhs );
      P3PmsgVect&
        operator = ( const P3PmsgObject& rhs );

        operator P3PmsgData& ( );

        operator bool ( );

    // Navigation and 
    public:
      bool
        Delete ( int nElem );
      void
        Truncate ( );
      virtual void
        Drop ( );
      P3PmsgField&
        InsertAt ( int nElem, const P3PmsgField& oField );
      //P3PmsgNode&
      //  InsertAt ( int nElem, const P3PmsgNode& oNode );
      UCHAR
        GetVBLockType ( POSITION oCURS );
      int
        Goto ( int nElem );
      VBLelem
        GetCount ( ) const;

    // Addressing and allocations
    public:
      virtual VBLsize
        GetVBLockVectSize ( ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual BOOL
        VerifyContainment ( void *pvBlob = nullptr, VBLsize = 0 ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1  );

    // Exposure
    public:
      virtual P3PmsgData&
        r_data ( ) const;
      P3PmsgData&
        r_data ( int nElem );
      virtual P3PmsgName&
        r_name ( ) const;
      P3PmsgName&
        r_name ( int nElem );
      P3PmsgItem&
        r_item ( int nElem );
      P3PmsgList&
        r_list ( int nElem );
      P3PmsgVect&
        r_vect ( int nElem );
      //P3PmsgNode&
      //  r_node ( int nElem );

    // Properties
    public:
      bool
        IsData ( int nElem );
      bool
        IsName ( int nElem );
      bool
        IsField ( int nElem );
      bool
        IsList ( int nElem );
      bool
        IsVect ( int nElem );
      //bool
      //  IsNode ( int nElem );

      P2PmsgVectHdl
        GetP2PmsgVectHdl ( );
      virtual VBLsize
        Sizeof ( UCHAR uVBLock ) const;
      virtual bool
        IsDirty ( );

    // Internal helpers
    private:
      VBLaddr
        AllocElem ( const P3PmsgField& oField );   // deep-copy an element into a fresh item block
      void
        MarkElemLinked ( VBLaddr aElem );          // flag an element block VBLock_Linked
      void
        FreeElemDeep ( VBLaddr aElem );            // release an element block + nested children
      VBLaddr
        GetSlot ( int nElem );                     // element addr at logical index (walks aExtra chain)
      void
        SetSlot ( int nElem, VBLaddr aElem );      // set slot, allocating continuation blocks as needed
      VBLaddr
        AllocContinuation ( );                     // allocate an empty aExtra continuation block

    // Attributes
    protected:
      UINT          m_nCurs;
      P3PmsgField  *m_pP3PmsgType{nullptr};
      UINT          m_nElemMax;
      int           m_nElem;
      P3PmsgData   *m_pP3PmsgData[MAX_P3PmsgData_Curs]{nullptr};
      bool          m_bVectDirty;
      VBLaddr       m_xVect;
      int           m_nVectSize;
      VBLaddr       m_aVect;
};

VBLsize
P2PmsgVect_SizeofItem ( UCHAR uVBLock, const P3PmsgVect& oVect );
VBLaddr
P2PmsgVect_InitItem   ( VBLock *pVBLock, const P3PmsgVect& oVect );
