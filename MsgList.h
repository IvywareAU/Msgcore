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

class Msgcore_EXT P3PmsgList : public P3PmsgField
{
      void
        RenderThisSafe ( );

    // Constructors and destructor
    public:
        P3PmsgList ( );

        P3PmsgList ( const P3PmsgList& rhs );

        P3PmsgList ( LPCWSTR lpszName, const P3PmsgData& oData );

        P3PmsgList ( const P3PmsgField& oField );

        P3PmsgList ( const P2PmsgListHdl& hList );

        P3PmsgList ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nListSize );

        P3PmsgList ( const P3PmsgObject& rhs );
      virtual
       ~P3PmsgList ( );

      virtual void
        Connect ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nListSize );

    // List operations
    public:
      VBLaddr
        GetHeadPos ( ) const;
      P3PmsgData&
        GetNext ( VBLaddr& aPos );
      P3PmsgList&
        AddListHead ( const P3PmsgData& oData );
      void
        DropHead ( );
      P3PmsgData&
        GetTail ( );
      VBLaddr
        GetTailPos ( ) const;
      P3PmsgData&
        GetPrev ( VBLaddr& aPos );
      P3PmsgList&
        AddListTail ( const P3PmsgData& oData );
      void
        DropTail ( );
      VBLelem
        GetCount ( );

    // Operators
    public:
      P3PmsgList&
        operator  = ( const P3PmsgList& rhs );
      P3PmsgList&
        operator  = ( const P3PmsgField& rhs );
      P3PmsgList&
        operator  = ( const P3PmsgData& rhs );
      P3PmsgList&
        operator = ( const P2PmsgListHdl& rhs );
      P3PmsgList&
        operator = ( const P3PmsgObject& rhs );
      P3PmsgList&
        operator += ( const P3PmsgList& rhs );
      virtual P3PmsgList&
        operator += ( const P3PmsgData& rhs );

        operator P3PmsgData& ( );

      //  `if ( oList )` and nothing else.  Refer P3PmsgObject::operator bool.
      explicit
        operator bool ( );

    // Navigation and 
    public:
      virtual bool
        Delete ( VBLaddr aPos );
      virtual void
        Truncate ( );
      virtual void
        Drop ( );
      UCHAR
        GetVBLockType ( POSITION oCURS );

    // Addressing and allocations
    public:
      virtual VBLsize
        GetVBLockListSize ( ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual BOOL
        VerifyContainment ( void *pvBlob = nullptr, VBLsize = 0 ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1  );

    // Properties
    public:
      P2PmsgListHdl
        GetP2PmsgListHdl ( );
      virtual VBLsize
        Sizeof ( UCHAR uVBLock ) const;
      virtual bool
        IsDirty ( );

      //  How many references on hVBList this list, the sub-objects a field
      //  owns, and the data cursors this list caches are holding.  Refer
      //  P3PmsgField::HeapHolders.
      int
        HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept override;

    // Attributes
    protected:
      UINT          m_nCurs{0};
      P3PmsgData   *m_pP3PmsgData[MAX_P3PmsgData_Curs];
      bool          m_bListDirty{false};
};


VBLsize
P2PmsgList_SizeofItem ( UCHAR uVBLock, const P3PmsgList& oList, BOOL bChain = TRUE );
VBLaddr
P2PmsgList_InitItem   ( VBLock *pVBLock, const P3PmsgList& oList );

void
P2PmsgList_LinkinItem ( P3PmsgList *pList
                      , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext );
VBLaddr
P2PmsgList_UnLinkItem ( P3PmsgList *pList, VBLockList *pVBLockList
                      , VBLaddr aItem );


