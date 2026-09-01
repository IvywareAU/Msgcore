// Copyright © 2007-2010, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2PmsgBSTR implementation
//
#pragma once
#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif

#include "wtypes.h"
//#include "P2Peer.h"
//#include "P2PTL.h"
#include "P2Pmsg.h"
#include "P2PmsgVBLock.h"

using  UINT08 = unsigned char;

///////////////////////////////////////////////////////////////////////
//  VBLockBSTR containers and definitions
//  NOTES: P2Pmsg exchange format, VBLock'ed binary sequence
//
#define VBLockBSTR_ROOT     0          // P2Pmsg root
#define VBLockBSTR_NET  (1<<0)         // P2Pmsg networking
#define VBLockBSTR_SYS  (1<<1)         //        system
#define VBLockBSTR_EVT  (1<<2)         //        events
#define VBLockBSTR_WRP  (1<<3)         //        wrapped VBLockBSTR
#define VBLockBSTR_MSG  (1<<4)         //        application message data
#define VBLockBSTR_SP4  (1<<5)         //        spare4
#define VBLockBSTR_SP5  (1<<6)         //        reserved5
#define VBLockBSTR_SP6  (1<<7)         //        reserved6

#pragma pack(push,1)
typedef struct
{
    VBLockHdr  oHdr;
    VBLock     oNode;
} VBLockBSTR;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct
{
    struct
    {                                  // Bits  0-23 total size of this structure
      UINT32  uiSync1;                 // Bits 24-25 addressing (VBLock_AddrMask)
                                       // Bits 26-31 layout generation, and the
                                       //           endian sentinel (VBLock_SyncMask)
      UINT32  uiSync2;                 // Compliment of above
    } oSync;                           // Build with VBLock_SyncMake, read via
                                       // VBLock_SyncForm/VBLock_SyncAddr - the
                                       // complement pair alone cannot detect a
                                       // byte-order mismatch (byte_order.md §4)
    char cIOmage;
} VBListIOmage;
typedef VBListIOmage P2Piomage;
#pragma pack(pop)
Msgcore_EXT VBListIOmage*
MakeIOmage ( LPCSTR lpcData, UINT32 nDataSize );
//UINT32
//SizeofIOmage ( const VBListIOmage *pIOmage );
Msgcore_EXT P2Piomage*
P2Piomage_Alloc ( const void *pvIOmage, UINT32 nIOmageSize );
//Msgcore_EXT P2Piomage*
//P2Piomage_Alloc ( const P3PmsgNode& oNode );
//Msgcore_EXT P2Piomage*
//P2Piomage_Alloc ( const P3PmsgNode& oField );
Msgcore_EXT P2Piomage*
Msgiomage_Duplicate ( const P2Piomage& oMsgiomage );
// Length-validated duplicate, the shape P2PmsgHeap_CreateIOMAGE already uses.
// The reference-only overload above cannot check the declared size against the
// real extent of what it was handed -- an image declaring 128 KB inside a 4 KB
// buffer is still copied by its declared length -- because a reference carries
// no length. That is the residual on finding M4, and this is its closure: pass
// the buffer you actually own and an over-declared header is rejected instead
// of over-read. Use this for any image that came off a disk or a socket.
Msgcore_EXT P2Piomage*
Msgiomage_Duplicate ( const P2Piomage& oMsgiomage, VBLsize nBufferLen );
Msgcore_EXT P2Piomage*
P2Piomage_Release ( P2Piomage *pP2Piomage );
Msgcore_EXT UINT
P2Piomage_Sizeof ( const P2Piomage *pP2Piomage );
Msgcore_EXT UINT
IOmage_Sizeof ( const P2Piomage *pP2Piomage );
//Msgcore_EXT BOOL
//IOmage_IsPKeySwap ( const P2Piomage *pP2Piomage );

///////////////////////////////////////////////////////////////////////
//  P2PmsgBSTR class
//  NOTES: P2Pmsg virtual blocked list manager
//       : Base class implementation delegates all memory management
//         directly through to heap.  Specialise this class for
//         alternative memory management models
class Msgcore_EXT P3PmsgBSTR
{
      //void
      //  RenderThisSafe ( );
      void
        ResetThisObject ( );

    // Constructors and destructor
    public:
        P3PmsgBSTR ( );

        P3PmsgBSTR ( LPCTSTR lpszName, const P3PmsgData& oData );

        P3PmsgBSTR ( UCHAR uVBLockAddr, VBLsize nSizeof );

        P3PmsgBSTR ( const P3PmsgBSTR& rhs );

        P3PmsgBSTR ( const VBListIOmage& oIOmage );

        // Length-validated form of the constructor above. The reference-only
        // one duplicates and adopts the image on its declared size alone, which
        // is the M4 residual seen from the caller's side; pass the extent of the
        // buffer you actually own and an over-declared header is rejected.
        // Required for any image off a disk or a socket.
        P3PmsgBSTR ( const VBListIOmage& oIOmage, VBLsize nBufferLen );

        P3PmsgBSTR ( VBListIOmage *pIOmage );

        // Length-validated form of the constructor above, and the twin of the
        // reference-taking pair further up. This one ADOPTS the image rather
        // than duplicating it, which is what the receive path needs: the frame
        // buffer is already the caller's to give away. Pass the extent of the
        // buffer you actually own.
        P3PmsgBSTR ( VBListIOmage *pIOmage, VBLsize nBufferLen );

        P3PmsgBSTR ( P2PmsgHANDLE hBSTR );
      virtual
       ~P3PmsgBSTR ( );

   // Initialisation
   public:
      P3PmsgBSTR&
        Init ( P2PmsgHANDLE hBSTR );
      P3PmsgItem&
        Init ( LPCTNAM lpszName, const P3PmsgData& oData );
      //P3PmsgNode&
      //  InitAddr ( );
      P3PmsgItem&
        InitData ( P3PmsgData& oData );
      P3PmsgItem&
        InitItem ( UCHAR uVBLockBSTR, const P3PmsgData& oData );

    // Operators
    public:
      P3PmsgBSTR&
        operator = ( const P3PmsgBSTR& rhs );

    // Completion Port images
    // NOTES: IOCP image preparation and release
    public:
      void*
        PrepareP2Piomage( DWORD uBSTRmask );
      P2Piomage*
        P2Piomage ( );
      UINT
        P2PiomageSize ( );
      void
        ReleaseP2Piomage( );

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1  );

    // Properties
    public:
      P2Pmsgnn_t 
        GetP2Pmsgnn ( ) const;
      static void
        SetDefaultP2Pmsgnn ( P2Pmsgnn_t uP2Pmsgnn );
      static P2Pmsgnn_t
        GetDefaultP2Pmsgnn ( );
      bool
        Exists  ( UCHAR eVBLockBSTR );
      P3PmsgItem&
        r_item  ( UCHAR eVBLockBSTR, bool bCreate = false );
      P3PmsgItem&
        r_datn  ( );
      P3PmsgName&
        r_name  ( );
      P3PmsgData&
        r_data  ( );
      void*
        VBLockBSTR_vp ( );
      VBLsize
        Sizeof ( ) const;
      static VBLsize
        SetDefaultSizeof ( );
      bool
        IsFragmented ( );
      bool
        IsDirty ( );

    // Attributes
    private:
        P2PmsgHANDLE  m_hBSTR{NULL};
        P3PmsgBSTR   *m_pP2PmsgBSTRiomage{nullptr};

        P3PmsgItem    m_oItem;
        //VBLockBSTR    m_oBSTR;
        //VBLockBSTR   *m_pBSTR{nullptr};
        //VBLockBSTR    m_oVBLockBSTR;
        //VBLockBSTR   *m_pVBLockBSTR{nullptr};
};

///////////////////////////////////////////////////////////////////////
//  P3PmsgBSTRnn targeted addressing 
//  NOTES: Usage P3PmsgBSTR32 oBSTR32; etc
template<UCHAR uBSTRnn>
class P3PmsgBSTRnn : public P3PmsgBSTR
{
    public:
        P3PmsgBSTRnn ( ) : P3PmsgBSTR ( uBSTRnn, 2024 ) {};
        P3PmsgBSTRnn ( VBLsize nSizeof ) : P3PmsgBSTR ( uBSTRnn, nSizeof ) {};
      virtual
       ~P3PmsgBSTRnn ( ) {};
};
typedef P3PmsgBSTRnn<VBLock_Addr16> P3PmsgBSTR16;
typedef P3PmsgBSTRnn<VBLock_Addr32> P3PmsgBSTR32;
typedef P3PmsgBSTRnn<VBLock_Addr64> P3PmsgBSTR64;

///////////////////////////////////////////////////////////////////////
//  Persistance extensions
//  NOTES: Manage P3PmsgBSTR disk IO

Msgcore_EXT BOOL
P2PmsgBSTR_Read ( LPCTSTR lpszPathname, P3PmsgBSTR& oBSTR );
Msgcore_EXT BOOL
P2PmsgBSTR_Read ( HANDLE hFile, P3PmsgBSTR& oBSTR );
Msgcore_EXT BOOL
P2PmsgBSTR_Write ( LPCTSTR lpszPathname, const P3PmsgBSTR& oBSTR );
Msgcore_EXT BOOL
P2PmsgBSTR_Write ( HANDLE hFile, const P3PmsgBSTR& oBSTR );
