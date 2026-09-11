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
#include "P2PmsgVBLock.h"
//#include "MsgDesc.h" //LJM:TODO Added 1/07/2015
//#include "MsgAttr.h" //LJM:TODO Added 1/07/2015
//#include "MsgStck.h" //LJM:TODO Added 1/07/2015
//#include "P2Peer.h"
//#include "P2PTL.h"

/////////////////////////////////////////////////
//  P2PCheckMemory diagnostic
//  NOTES: Performs AfxCheckMemory() upon creation and destruction
//       : Designed to just sit on the stack

class Msgcore_EXT P2PmsgCheckMemory
{
    public:
      P2PmsgCheckMemory () noexcept
      {
#ifdef _DEBUG
        try { ASSERT(AfxCheckMemory()); } catch (...) {};
#endif     
      }
     ~P2PmsgCheckMemory ( )
      {
#ifdef _DEBUG
       try { ASSERT(AfxCheckMemory()); } catch (...) {};
#endif     
      }
    void
      Check ( )
      {
#ifdef _DEBUG
       try { ASSERT(AfxCheckMemory()); } catch (...) {};
#endif     
      }
};

///////////////////////////////////////////////////////////////////////
//  Convenience characters

#define T_Space     _T(' ')
#define T_Hash      _T('#')
#define T_Period    _T('.')
#define T_Colon     _T(':')
#define T_BackSlash L'\\'
#define T_ForeSlash L'/'
#define T_LF        _T('\n')
#define T_CR        _T('\r')
#define T_CRLF      _T("\r\n")
#define T_RootDelim  L'.'
#define T_DescDelim  L'.'
#define T_AttrDelim  L'@'
#define T_StckDelim  L'^'

#define SelectObject_T(str)      SelectObject(_T(str))
#define SelectItem_T(str)        SelectItem(_T(str))
#define DeclareItem_T(str,data)  DeclareItem(_T(str),data)
//#define DeclareNode_T(str,data)  DeclareNode(_T(str),data)


class P3PmsgField;
class P3PmsgObject;
class Msgcore_EXT P3PmsgData
{
      friend P3PmsgField;
      void
        RenderThisSafe ( );
        P3PmsgData ( const P3PmsgField *pField ); 

    // Constructors and destructor
    public:
        P3PmsgData ( );

        // Deep copy. Without this the compiler generates a shallow one, which
        // copies the owning m_pObject pointer and leaves ~P3PmsgData to delete
        // it twice - the source is left dangling and the second destructor
        // faults. Every other class in this family (P3PmsgField, P3PmsgList,
        // P3PmsgVect, P3PmsgBSTR) already declares one; this was the omission.
        P3PmsgData ( const P3PmsgData& rhs );

        P3PmsgData ( INT08 vuInt08 );
        P3PmsgData ( UINT08 vuInt08 );
        P3PmsgData ( INT16 vInt16 );
        P3PmsgData ( UINT16 vuInt16 );
        P3PmsgData ( INT32 vInt32 );
        P3PmsgData ( UINT32 vuInt32 );
        P3PmsgData ( INT64 vInt64 );
        P3PmsgData ( UINT64 vuInt64 );
        P3PmsgData ( bool vbool );
        P3PmsgData ( long vLong );
        P3PmsgData ( unsigned long vuLong );
        P3PmsgData ( float vFloat );
        P3PmsgData ( double vDouble );
        P3PmsgData ( wchar_t wChar );
		    P3PmsgData ( LPCSTR vString
			             , size_t nLength = 0, UCHAR uVBLockData = VBLockData_BSTR16);
		    P3PmsgData ( LPCWSTR vString
                   , size_t nLength = 0, UCHAR uVBLockData = VBLockData_WSTR16 );
        P3PmsgData ( size_t nLengthMax
                   , LPCTSTR vString
                   , size_t nLength = 0, UCHAR uVBLockData = VBLockData_BSTR16var );
        P3PmsgData ( const void *vObject
                   , VBLsize nSizeof, UCHAR uVBLockData = VBLockData_BLOB16 );
        P3PmsgData ( const GUID& oGUID );

      virtual
       ~P3PmsgData ( );
      void
        Connect ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nDataSize );
      void
        Recreate( P2Pvar_t nP2Pvar, const void *pvdata, VBLsize iDataSize ); 

    // Operators
    public:
      P3PmsgData&
        operator = ( const P3PmsgData& rhs );
      BOOL
        operator == ( const P3PmsgData& rhs );
      BOOL
        operator != ( const P3PmsgData& rhs );

    // Data exposure
    //
    //  THREE FORMS PER SCALAR, AND ONLY ONE OF THEM IS UNSAFE.  The value is
    //  stored in the union of a VBLockData, which lives inside a #pragma
    //  pack(1) image at whatever offset the block walk happened to land on -
    //  roughly half of them odd.  So:
    //
    //    c_short ( ) const     READ.  Returns by value.  Safe: the compiler
    //                          knows the member is packed and emits an
    //                          unaligned load.
    //    c_short ( short )     WRITE.  Copies the bytes in.  Safe for the
    //                          same reason, and it is the one to use.
    //    c_short ( )           WRITE via a bound reference.  *** UNDEFINED
    //                          BEHAVIOUR ***, because a short& asserts an
    //                          alignment the address does not have.  It is
    //                          the library committing the UB on the caller's
    //                          behalf, so unlike the blob pointer below, a
    //                          caller cannot avoid it by copying out.
    //
    //  UBSan says so out loud - "reference binding to misaligned address ...
    //  for type 'short int', which requires 2 byte alignment" - and that is
    //  F-S4-3.
    //
    //  SO THE REFERENCE FORMS ARE GONE RATHER THAN DEPRECATED, and the reason
    //  is that deprecating them would have fixed half the defect.  The reader
    //  is const-qualified; the writer was not - so on a NON-const object
    //  `d.c_short()` selected the reference form even when the caller only
    //  wanted to read, and every read through a non-const P3PmsgData bound the
    //  same misaligned reference.  Removing the overload is what makes those
    //  reads resolve to the const form, by value, safely, with no edit at the
    //  call site.
    //
    //  What that costs is exact and is a COMPILE error, never a silent change:
    //  `d.c_int() = 42;` no longer compiles and becomes `d.c_int(42);`.  Reads
    //  are untouched.  That is why this is removal where F-S5-3 chose
    //  documentation for P2Pc_vBlob's overlay operators - changing those would
    //  have altered WHEN a write lands, which a compiler cannot catch, while
    //  this cannot fail quietly anywhere, including in the trees outside this
    //  solution that cannot be built to check.
    //
    //  Aligning the union instead would be an on-the-wire and on-disk layout
    //  change, which F-S5-3 costed and declined.
    public:
      char
        c_char ( ) const;
      char
        c_char ( char vNew );
      wchar_t
        c_wchar ( ) const;
      wchar_t
        c_wchar ( wchar_t vNew );
      short
        c_short ( ) const;
      short
        c_short ( short vNew );
      int
        c_int ( ) const;
      int
        c_int ( int vNew );
      INT64
        c_int64 ( ) const;
      INT64
        c_int64 ( INT64 vNew );
      //int
      //  c_int ( int iValueDefault ) const;
      unsigned int
        c_uint ( ) const;
      unsigned int
        c_uint ( unsigned int vNew );
      UINT64
        c_uint64 ( ) const;
      UINT64
        c_uint64 ( UINT64 vNew );
      //  c_long has no definition in P2Pmsg.cpp and never had one, so any
      //  caller is already an unresolved external at link time.  Left as it
      //  was rather than given a writer it could not have used.
      long
        c_long ( ) const;
      unsigned int
        c_time ( ) const;
      unsigned int
        c_time ( unsigned int vNew );
      __int64
        c_time64 ( ) const;
      __int64
        c_time64 ( __int64 vNew );
      float
        c_float ( ) const;
      float
        c_float ( float vNew );
      double
        c_double ( ) const;
      double
        c_double ( double vNew );
      bool
        c_bool ( ) const;
      bool
        c_bool ( bool vNew );
      LPCSTR
        c_str ( ) const;
      LPCWSTR
        c_wstr() const;
      //  THE BLOB PAYLOAD POINTER CARRIES NO ALIGNMENT GUARANTEE, and that is
      //  a property of the format rather than of this function.  The image is
      //  #pragma pack(1) end to end: a VBLockHdr is 9 bytes in 64-bit
      //  addressing, VBLockData puts 2 more before its union, and names are
      //  variable-length - so a payload begins at the running sum of all of
      //  that, which is to say at an arbitrary offset.  Roughly half of them
      //  are odd.
      //    Reading the result AS BYTES is always fine.  Reading it through a
      //  type that needs alignment - casting it to wchar_t*, or overlaying a
      //  struct on it - is undefined behaviour.  It happens to work on x86,
      //  it is what UBSan reports on Linux, and it faults outright on a
      //  strict-alignment target.  Use c_vBlobCopy() to read such a payload,
      //  or memcpy it somewhere aligned yourself.
      void*
        c_vBlob ( ) const;
      void*
        c_vBlob ( const void *pvSrc, size_t nCount );
      //  Alignment-safe read of a blob payload: copies min(nCount, stored
      //  size) bytes into pvOut and returns how many it copied, so the caller
      //  reads its own aligned object rather than the image.  Nothing else
      //  differs - same bytes, same order, no widening or conversion.
      size_t
        c_vBlobCopy ( void *pvOut, size_t nCount ) const;
      void*
        c_vGUID ( ) const;
      void*
        c_vGUID ( const void *pvGUID, size_t nSize = 16 );
      void*
        c_memcpy ( const void *pvSrc, size_t nCount );
      LPCWSTR
        c_wcscpy ( LPCWSTR lpszSrc, size_t nCount = 0 );
      //void*
      //  c_memcpy ( const void *pvDst, size_t nCount ) const;
      void*
        c_memset ( int c, size_t nCount ); 
      int
        c_wcscmp ( LPCWSTR lpszCompare ) const;
      int
        c_strcmp ( LPCSTR lpszCompare ) const;
      int
        c_wcsicmp ( LPCWSTR lpszCompareNocase ) const;
      int
        c_stricmp ( LPCSTR lpszCompareNocase ) const;
      size_t
        c_size ( ) const;
      size_t
        c_size_max ( ) const;
      // Width-agnostic read of any integer-family scalar (INT08..UINT64, BOOL)
      // into an INT64, with the unsigned subtypes flagged. Unlike the strict
      // c_int/c_int64 accessors it never throws on a width/sign mismatch; it
      // returns false only when the field is not an integer-family scalar. Added
      // for the FileSystem "value" render path (unsigned/narrow ints have no
      // dedicated public accessor otherwise).
      bool
        ReadAnyInt ( INT64& iOut, bool& bUnsigned ) const;
      P3PmsgData&
        r_data ( ) noexcept { return *this; }
      const P3PmsgObject*
        p_Object ( ) const;

    // Operations
    public:
      void
        Nullify ( );
      LPCTSTR
        ToString ( LPCTSTR lpszFormat = nullptr );
      LPCTSTR
        ToStringDefs ( LPCTSTR lpszFormat = 0 );
      LPCTSTR
        ToStringType ( LPCTSTR lpszFormat = 0 ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual BOOL
        VerifyContainment ( void *pvBlob = nullptr, VBLsize = 0 ) const;
    /*virtual void
        Print ( FILE *fd, int iDepthOS = 0  );*/

    // Properties
    public:
      virtual UCHAR
        SetAttr ( UCHAR ucAttrAdd, UCHAR ucAttrRemove = 0 );
      virtual UCHAR
        GetAttr ( ) const;
      virtual VBLsize
        Sizeof ( ) const;
      bool
        IsDirty ( ) const;
      bool
        IsNull ( ) const;
      UCHAR
        DataType ( ) const;
      VBLock*
        GetContainerVBLock ( bool indirect = true ) const;

    // Attributes
    private:
      mutable 
      P3PmsgObject *m_pObject{nullptr};
      mutable
      TCHAR         m_szToString[256];
      TCHAR         m_szToStrDef[256];
      bool          m_bDataDirty{false};
};
//TCHAR
#ifdef _UNICODE_TNAME
  #define c_tcscmp  c_wcscmp
  #define c_tcsicmp c_wcsicmp
  #define _TL       L
#else
  #define c_tcscmp  c_strcmp
  #define c_tcsicmp c_stricmp
  #define _TL
#endif

#ifdef _UNICODE
  #define t_str     w_str
#else
  #define t_str     c_wstr
#endif

#if P2P_PTR64
#define c_LPARAM    c_int64
#define c_WPARAM    c_uint64
#else // Persisted values are NOT compatible between 32-bit and 64-bit pointer builds
#define c_LPARAM    c_int
#define c_WPARAM    c_uint
#endif
#define c_BOOL      c_int

Msgcore_EXT _variant_t
P2PmsgData_var ( const P3PmsgData& oData );
Msgcore_EXT BOOL
P2PmsgData_var ( P3PmsgData& oData, const _variant_t& var );
Msgcore_EXT BOOL
P3PmsgData_var ( P2PmsgHANDLE hVBListData, P2Pos nP2Pos, _variant_t& var );

#define DataBSTR08(vString) P3PmsgData(vString,0,VBLockData_BSTR08)
#define DataBSTR16(vString) P3PmsgData(vString,0,VBLockData_BSTR16)
#define DataBSTR32(vString) P3PmsgData(vString,0,VBLockData_BSTR32)
#define DataWSTR08(vString) P3PmsgData(vString,0,VBLockData_WSTR08)
#define DataWSTR16(vString) P3PmsgData(vString,0,VBLockData_WSTR16)
#define DataWSTR32(vString) P3PmsgData(vString,0,VBLockData_WSTR32)
#define AllocBLOB08(nSizeBlob) P3PmsgData((void *)0,nSizeBlob,VBLockData_BLOB08)
#define AllocBLOB16(nSizeBlob) P3PmsgData((void *)0,nSizeBlob,VBLockData_BLOB16)
#define AllocBLOB32(nSizeBlob) P3PmsgData((void *)0,nSizeBlob,VBLockData_BLOB32)
#define DataBLOB08(vBlob) P3PmsgData((void *)&vBlob,sizeof(vBlob),VBLockData_BLOB08)
#define DataBLOB16(vBlob) P3PmsgData((void *)&vBlob,sizeof(vBlob),VBLockData_BLOB16)

class Msgcore_EXT P3PmsgTime : public P3PmsgData
{
    // Constructors and destructor
    public:
        P3PmsgTime ( );
        P3PmsgTime ( __int64 iTime );
        P3PmsgTime ( const P3PmsgTime& rhs );
      virtual
       ~P3PmsgTime ( );
    // Operators
      P3PmsgTime&
        operator = ( const P3PmsgTime& rhs );
};


class Msgcore_EXT P3PmsgName
{
      friend P3PmsgField;
      void
        RenderThisSafe ( );
        P3PmsgName ( const P3PmsgField *pField ); 

    // Constructors and destructor
    public:
        P3PmsgName ( ) noexcept;

        P3PmsgName ( LPCTNAM lpszName, size_t nSize = 0 );
      virtual
       ~P3PmsgName ( );

    // Operators
    public:
      P3PmsgName&
        operator = ( const P3PmsgName& rhs );
      bool
        operator == ( const P3PmsgName& rhs ) const;
      bool
        operator == ( LPCTSTR rhs ) const;
      bool
        operator < ( const P3PmsgName& rhs ) const;
      bool
        operator > ( const P3PmsgName& rhs ) const;

    // Name exposure
    public:
      LPCTNAM
        c_name ( LPCTNAM lpszName, size_t nSize = 0 );
	    LPCTNAM
        c_name ( ) const;
      UCHAR
        c_size ( ) const;
      int
        c_strcmp ( LPCSTR lpszCompare ) const;
      int
        c_stricmp ( LPCSTR lpszCompareNocase ) const;
      int
        c_wcscmp ( LPCWSTR lpszCompare ) const;
      int
        c_wcsicmp ( LPCWSTR lpszCompareNocase ) const;
      int
        c_wcsicmpWC ( LPCWSTR lpszCompareNocase ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      BOOL
        VerifyContainment ( void *pvBlob = 0, VBLsize nSizeofBlob = 0 ) const;
    /*virtual void
        Print ( FILE *fd, int iDepthOS = 0  );*/

    // Properties
    public:
      virtual VBLsize
        Sizeof ( ) const;
      bool
        IsDirty ( ) const noexcept;
      VBLock*
        GetContainerVBLock ( bool bIndirect ) const;

    // Attributes
    private:
      P3PmsgObject *m_pObject{nullptr};
      bool          m_bNameDirty{false};
};

///////////////////////////////////////////////////////////////////////////////
//  P2PmsgObject containers and definitions
//  NOTES: Base class for VBLock objects

#define ROBJ r_Object()
#define ObjectCheckID_DataSizeGT9 1

class Msgcore_EXT P3PmsgObject
{
    // Constructors and destructers
    public:
        P3PmsgObject ( ) noexcept;

        P3PmsgObject ( UCHAR uVBLock );

        P3PmsgObject ( const P3PmsgObject& rhs );
      virtual
       ~P3PmsgObject ( );

      virtual void
        Nullify ( );
      void
        ConnectVBLock ( );
      void
        Connect ( const P3PmsgObject& rhs );
      void
        Connectx ( P2PmsgHANDLE hVBList, VBLaddr xVBLock, VBLsize nVBLockSize );
      void
        Connecta ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nVBLockSize );

    // Operators
    public:
      P3PmsgObject&
        operator = ( const P3PmsgObject& rhs );
      bool
        operator == ( const P3PmsgObject& rhs ) const;
      bool
        operator != ( const P3PmsgObject& rhs ) const;

      //  `if ( oObject )` and nothing else.  EXPLICIT because the implicit
      //  form made `oA == oB` compile as `(int)(bool)oA == (int)(bool)oB` --
      //  "are we both non-void" wearing the spelling of "are we the same
      //  item".  Refer P3PmsgField::operator == for the measurement.
      explicit
        operator bool ( ) const noexcept;

    // Memory management
    // NOTES: Referenced in VBLock units
    public:
      VBLaddr
        AllocVBLock ( UCHAR uVBLockType, VBLsize nItemSize, bool bZero = true );
      VBLaddr
        RehomeInlineItem ( );

      //  Give this object its own copy of whatever its inline VALUE block
      //  points at.  memcpy duplicates the block; a value that has outgrown
      //  the block keeps its payload elsewhere and only chains to it, and
      //  copying the chain is not copying the value.  Refer the implementation.
      void
        PrivatiseInlineChain ( );

      //  Give back what this object's inline VALUE block points at.  The other
      //  half of PrivatiseInlineChain: that one allocates the copy's payload
      //  block on the heap the two objects SHARE, and a shared heap does not go
      //  away when the copy does.  Refer the implementation.
      void
        ReleaseInlineChain ( );
      VBLaddr
        Free ( VBLaddr aVBLockAddr );
      void*
        Msg2Phys ( VBLaddr aVBLockAddr ) const;
      VBLsize
        Msg2Size ( VBLaddr aVBLockAddr ) const;
      VBLsize
        GetVBLockSize ( ) const;

    // Addressing and allocations
    public:
      virtual P2Pos
        GetP2Pos ( ) const noexcept;
      virtual char*
        GetVBLock  ( ) const;
      virtual VBLaddr
        GetVBLocknn( ) const;
      virtual P3PmsgObject
        GetParent ( ) const;
      virtual P3PmsgObject
        GetParentItem ( ) const;
      virtual P3PmsgObject
        GetObject ( P2Pos nP2Pos ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      BOOL
        AssertCommon ( const P3PmsgObject& rhs ) const;
      BOOL
        AssertValidAddr ( VBLaddr aVBLockAddr );
      BOOL
        AssertCondition ( int nConditionId ) const;
    /*virtual void
        Print ( FILE *fd, int iDepthOS = 0  );*/

    // Properties
    public:
      bool
        IsEmpty ( ) const noexcept { return m_nVBLockSize==0 ? true : false; }
      bool
        IsVoid ( ) const noexcept;

      //  Is the item INSIDE me, or on a heap where others can name it?
      //  That, and not "is there a heap", is what decides whether a write
      //  through this object can be seen anywhere else.  Refer the
      //  implementation for what it does not settle.
      bool
        IsInline ( ) const noexcept;

      //  Can anything other than me see a write through me?  TRUE is a
      //  guarantee that nothing can; FALSE says only that the question is
      //  open.  That is IsInline's guarantee widened to cover the storage
      //  rather than the block -- refer the implementation for the row it
      //  covers and the one it does not.
      bool
        IsSole ( ) const noexcept;
      bool
        IsData ( ) const;
      bool
        IsName ( ) const;
      bool
        IsField ( ) const;
      //bool
      //  IsNode ( ) const;
      bool
        IsList ( ) const;
      bool
        IsVect ( ) const;
      bool
        IsAttr ( ) const;
      bool
        IsDesc ( ) const;
      bool
        IsStck ( ) const;
      bool
        IsRoot ( ) const;
      BOOL
        IsVBLaddrnn ( UCHAR uVBLock ) const noexcept;
      BOOL
        IsContainedVBLump ( void *pVBLaddr, VBLsize nSizeofVBLump = 0 ) const noexcept;
      BOOL
        HasParent ( ) const;

    // Attributes
    public:
      P2PmsgHANDLE  m_hVBList{0};
      VBLaddr       m_oVBLock[88]{0}; // TODO:LJM was WIN32 2022/02/04
      VBLaddr       m_xVBLock{0};
      VBLaddr       m_aVBLock{0};
      VBLsize       m_nVBLockSize{0};
      UCHAR         m_uVBLock{VBLock_Addrxx};
};


/////////////////////////////////////////////////
//  P2PCheckPtrs diagnostic
//  NOTES: P2Pmsg uses C++ pointers to memory within allocated VBHeap
//         that are invalidate whenever the VBHeap is reallocated.
//       : Should heap be re-allocated whilst this object resides on the
//         stack and ASSERT(0) will be raised upon destruction

class Msgcore_EXT P2PmsgCheckPtrs
{
    public:
      P2PmsgCheckPtrs ( const P3PmsgObject& oObject );
     ~P2PmsgCheckPtrs ( );
    BOOL
      Check ( );
    private:
      VBLaddr      m_nVBHeapAllocSeqnum{0};
      P2PmsgHANDLE m_hVBHeap{0}; // VBHeap to check
};

///////////////////////////////////////////////////////////////////////
//  P2PmsgField class
//  NOTES: VBlockData wrapper
VBLockData*
P2PmsgObject_pData  ( const P3PmsgObject& oObject, bool bIndirect = true );
VBLockData*
P2PmsgData_pData    ( const P3PmsgData& oData, bool bIndirect = true );
VBLsize
P2PmsgObject_Sizeof_VBLockData( const P3PmsgObject& oObject, BOOL bChain = TRUE );
VBLockData*
P2PmsgObject_NewVBLockData ( P3PmsgObject& oObject, VBLsize nSizeof );
VBLockName*
P2PmsgObject_pName  ( const P3PmsgObject& oObject, bool bIndirect = true );
VBLsize
P2PmsgObject_VBLockNameSize( const P3PmsgObject& oObject );
Msgcore_EXT BOOL
P3PmsgObject_IsCommon ( const P3PmsgObject *pObject1, const P3PmsgObject *pObject2 ) noexcept;


class P3PmsgList;
class P3PmsgVect;
//class P3PmsgNode;
class P3PmsgCurs;
class P3PmsgAttr;
class P3PmsgDesc;
class MsgStck;
class Msgcore_EXT P3PmsgField : public P3PmsgName, public P3PmsgData
{
      friend P3PmsgList; friend P3PmsgVect;// friend P3PmsgNode;
      friend P3PmsgAttr;
      friend P3PmsgDesc;
      friend MsgStck;
      friend P3PmsgCurs;
      void
        RenderThisSafe ( );
    public:
      typedef enum
      {
        AttrCMD_Get         = 0,
        AttrCMD_Create      = 1,
        //AttrCMD_DropOnEmpty = 3,
      } AttrCMD_e;

    // Constructors and destructor
    public:
        P3PmsgField ( );

        P3PmsgField ( const P3PmsgField& rhs );

        P3PmsgField ( LPCTNAM lpszName, size_t nSize = 0 );

        P3PmsgField ( LPCTNAM lpszName, const P3PmsgData& oData );

        P3PmsgField ( const P2PmsgFieldHdl& hField );

        P3PmsgField ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nFieldSize );

        P3PmsgField ( const P3PmsgObject& rhs );
      virtual
       ~P3PmsgField ( );

      virtual void
        Nullify ( ); 
      virtual void
        Connect ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nFieldSize );
      void
        RewindCurs ( );

    // Operators
    public:
      P3PmsgField&
        operator = ( const P3PmsgField& rhs );
      P3PmsgField&
        operator = ( const P3PmsgData& rhs );
      P3PmsgField&
        operator = ( const P3PmsgName& rhs );
      P3PmsgField&
        operator = ( const P2PmsgFieldHdl& rhs );
      P3PmsgField&
        operator  = ( const P3PmsgObject& rhs );
      P3PmsgField&
        operator += ( const P3PmsgList& rhs );
      P3PmsgField&
        operator += ( const P3PmsgVect& rhs );
      P3PmsgField&
        operator += ( const P3PmsgField& rhs );
      //  Compare by NAME.  const since [2026-09-11]: P3PmsgName declares both
      //  of its comparisons const, this hides them, and a const field could
      //  therefore not be compared to a name at all -- C2678, while every
      //  meaningless comparison below compiled.
      bool
        operator == ( LPCTNAM lpszName ) const;

      //  Compare by IDENTITY -- do these two denote the SAME item?  That is
      //  P3PmsgObject's question and this asks it of the object.  For the
      //  value, which is a different question, ask r_data() and r_name().
      bool
        operator == ( const P3PmsgField& rhs ) const;
      bool
        operator != ( const P3PmsgField& rhs ) const;

      virtual P3PmsgField&
        operator [] ( LPCTNAM lpszName );

        operator P3PmsgData& ( );

      //  `if ( oField )` and nothing else.  Refer P3PmsgObject::operator bool.
      explicit
        operator bool ( ) const;

    // Chained reference exposures
    public:
      P3PmsgName&
        r_name ( ) const;
      P3PmsgData&
        r_data ( ) const;
      P3PmsgAttr&
        r_Attr ( AttrCMD_e eAttrCmd = AttrCMD_Get ) const;
      P3PmsgDesc&
        r_Desc ( AttrCMD_e eAttrCmd = AttrCMD_Get ) const;
      MsgStck&
        r_Stck ( ) const;
      const P3PmsgObject&
        r_Object ( ) const;

    // Memory management
    public:
      virtual void
        Drop ( );

    // Navigation and 
    public:
      P3PmsgField&
        SelectItem  ( LPCTNAM lpszItemName );
      P3PmsgObject
        SelectObject( LPCTNAM lpszObjectName );
      P3PmsgField&
        DeclareItem ( LPCTNAM lpszFieldName, const P3PmsgData& oData, BOOL bUpdate = FALSE );
      //P3PmsgNode&
      //  DeclareNode ( LPCTNAM lpszNodeName, const P3PmsgData& oData, bool bUpdate = false );
      virtual bool
        Exists ( LPCTNAM lpszItemName ) const;
      virtual bool
        Delete ( LPCTNAM lpszItemName );
      virtual void
        Truncate ( );

    // Addressing and allocations
    public:
      P2Pos
        GetP2Pos ( ) const;

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      BOOL
        VerifyContainment ( void *pvBlob = 0, VBLsize nSizeofBlob = 0 ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1  );

    // Properties
    public:
      P2PmsgFieldHdl
        GetP2PmsgFieldHdl ( );
      UCHAR
        SetAccess ( UCHAR uAccessAdd, UCHAR uAccessRemove = 0 );
      UCHAR
        GetAccess ( UCHAR uAccessMask = ~(UCHAR)0 );
      virtual VBLsize
        Sizeof ( ) const;
      virtual bool
        IsVoid ( ) const;

      //  Is this field's item stored inside the field?  If it is, a write
      //  through it reaches nobody.  Refer P3PmsgObject::IsInline.
      virtual bool
        IsInline ( ) const;

      //  Can a write through this field be seen anywhere but here?  TRUE
      //  guarantees not.  Refer P3PmsgObject::IsSole.
      virtual bool
        IsSole ( ) const;

      //  How many references on hVBList this field and the sub-objects it owns
      //  are holding.  What IsSole subtracts; refer its implementation.
      int
        HeapHolders ( P2PmsgHANDLE hVBList ) const noexcept;
      virtual bool
        IsDirty ( ) const;
      virtual bool
        IsStacked ( ) const;
      virtual bool
        IsAttributed ( ) const;
      virtual bool
        IsDescendant ( ) const;
      P2PmsgHANDLE
        GetP2PmsgHandle ( ) const;
      CString
        GetPath ( ) const;  

    // Attributes
    protected:
      mutable
      P3PmsgObject  m_oObject;
      mutable
      P3PmsgAttr   *m_pP3PmsgAttr{nullptr};
      mutable
      P3PmsgDesc   *m_pP3PmsgDesc{nullptr};
      mutable
      MsgStck      *m_pMsgStck{nullptr};
      bool          m_bFieldDirty{false};
};
typedef P3PmsgField P3PmsgItem;

class Msgcore_EXT P3PmsgField16 : public P3PmsgField
{
    using P3PmsgField::P3PmsgField;
    // Constructors
    public:
        P3PmsgField16 ( ) noexcept;
      virtual
       ~P3PmsgField16 ( );
};

Msgcore_EXT P3PmsgField&
P2PmsgField_Merge ( P3PmsgField& oField, const P3PmsgField& rhs );


VBLsize
P2PmsgField_SizeofItem ( UCHAR uVBLock, const P3PmsgField& oField, BOOL bChain = TRUE );
VBLsize
P2PmsgItem_InitField   ( VBLock *pVBLock, const P3PmsgField& oField );
void
P2PmsgField_SwapItem   ( P3PmsgField *pField1, P3PmsgField *pField2 );
void
P2PmsgField_DropData   ( P3PmsgField *pField );
void
P2PmsgField_DropName   ( P3PmsgField *pField );


VBLaddr
P2PmsgField_GetVBLockParentnn ( const P3PmsgField *pField );
VBLock*
P2PmsgField_GetVBLockParent ( const P3PmsgField *pField );

//#define P2N(name) SelectNode(_T(#name))

/*class Msgcore_EXT P3PmsgNode : public P3PmsgField
{
      friend P3PmsgCurs;
      void
        RenderThisSafe ( );

    // Constructors and destructor
    public:
        P3PmsgNode ( );

        P3PmsgNode ( const P3PmsgNode& rhs );

        P3PmsgNode ( LPCTNAM lpszName, int nSize = 0 );

        P3PmsgNode ( LPCTNAM lpszName, const P3PmsgData& oData );

        P3PmsgNode ( const P3PmsgField& oField );

        P3PmsgNode ( const P2PmsgNodeHdl& hNode );

        P3PmsgNode ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nNodeSize );

        P3PmsgNode ( const P3PmsgObject& rhs );
      virtual
       ~P3PmsgNode ( );

      void
        Nullify ( ); 
      void
        Connect ( P2PmsgHANDLE hVBList, VBLaddr aVBLock, VBLsize nNodeSize );
      P3PmsgNode&
        AddNode ( LPCTNAM lpszName, const P3PmsgData& oData );
      P3PmsgField&
        AddField( LPCTNAM lpszName, const P3PmsgData& oData );

    // Operators
    public:
      P3PmsgNode&
        operator  = ( const P3PmsgNode& rhs );
      P3PmsgNode&
        operator  = ( const P3PmsgField& rhs );
      P3PmsgNode&
        operator  = ( const P2PmsgNodeHdl& rhs );
      P3PmsgNode&
        operator  = ( const P3PmsgObject& rhs );
      P3PmsgNode&
        operator += ( const P3PmsgNode& rhs );
      P3PmsgNode&
        operator += ( const P3PmsgList& rhs );
      P3PmsgNode&
        operator += ( const P3PmsgVect& rhs );
      P3PmsgNode&
        operator += ( const P3PmsgField& rhs );
      virtual P3PmsgField&
        operator [] ( LPCTNAM lpszName );

        operator P2PmsgNodeHdl ( );

        operator P3PmsgData& ( );

    // Memory management
    public:
      virtual void
        Drop ( );

    // Navigation and 
    public:
      P3PmsgField
        Select      ( LPCTNAM lpszItemName );
      P3PmsgObject
        SelectObject( LPCTNAM lpszObjectName );
      P3PmsgField&
        SelectItem  ( LPCTNAM lpszItemName );
      P3PmsgNode&
        SelectNode  ( LPCTNAM lpszNodeName );
      P3PmsgVect&
        SelectVect  ( LPCTNAM lpszVectName );

      P3PmsgField&
        DeclareItem ( LPCTNAM lpszFieldName, const P3PmsgData& oData, bool bUpdate = false );
      P3PmsgNode&
        DeclareNode ( LPCTNAM lpszNodeName, const P3PmsgData& oData, bool bUpdate = false );

      //bool
      //  Exists ( UPCSTR lpszItemName ) const;
      bool
        Exists ( LPCTNAM lpszItemName ) const;
      bool
        Delete ( LPCTNAM lpszItemName );
      void
        Truncate ( );

    // Troubleshooting
    public:
      virtual void
        AssertValid ( ) const;
      virtual void
        Print ( FILE *fd, int nDepthOS, int nDepthOSinc = 1 );

    // Std::List modifiers
    public:
      P2PmsgNodeHdl
        PushBack ( const P3PmsgNode& oNode );
      P2PmsgFieldHdl
        PushBack ( const P3PmsgField& oField );
      P2PmsgVectHdl
        PushBack ( const P3PmsgVect& oVect );

    // Exposure
    public:
      P3PmsgCurs&
        r_Curs ( );

    // Properties
    public:
      P2PmsgNodeHdl
        GetP2PmsgNodeHdl ( );
      VBLelem
        GetCount ( ) const;
      virtual VBLsize
        Sizeof ( UCHAR uVBLock ) const;
      virtual bool
        IsDirty ( );

    // Attributes
    protected:
      P3PmsgCurs   *m_pCurs;
      bool          m_bNodeDirty;
};*/


/*VBLsize
P2PmsgNode_SizeofItem ( UCHAR uVBLock, const P3PmsgNode *pNode );
VBLaddr
P2PmsgNode_InitItem   ( VBLock *pVBLock, const P3PmsgNode *pNode );
void
P2PmsgNode_LinkinItem ( P3PmsgNode *pNode
                      , VBLaddr aItemPrev, VBLaddr aItem, VBLaddr aItemNext );
void
P2PmsgNode_SortinItem ( P3PmsgNode *pNode, const P3PmsgName& oName
                      , VBLaddr aItem );
VBLockNode*
P3PmsgNode__GetVBLockNode ( const P3PmsgNode *pNode );*/

///////////////////////////////////////////////////////////////////////
//  P3PmsgNode hidden definitions
/*VBLaddr
P2PmsgNode_UnLinkItem ( P3PmsgNode *pNode, VBLockNode *pVBLockNode
                      , VBLaddr aItem );

Msgcore_EXT P3PmsgNode&
P2PmsgNode_Merge ( P3PmsgNode& oNode, const P3PmsgNode& rhs );*/
Msgcore_EXT void
P2PmsgNode_Swap  ( P3PmsgField& oField1, P3PmsgField& oField2 );

///////////////////////////////////////////////////////////////////////
//  P2Pmsg serialisation helpers
//  NOTES: Perform standard activities
Msgcore_EXT P3PmsgItem&
P3PmsgField_SERIALISE ( P3PmsgItem& oItem
                      , LPCTNAM lpszFieldname, const P3PmsgData& oData
                      , BOOL bDscAttr, LPCTSTR lpszDescription );

Msgcore_EXT P3PmsgAttr&
P2PmsgAttr_SERIALISE  ( P3PmsgAttr& oAttr, BOOL bOverwrite
                      , LPCTNAM lpszFieldname, const P3PmsgData& oData
                      , BOOL bDscAttr = false, LPCTSTR lpszDescription = 0 );

///////////////////////////////////////////////////////////////////////
//  P2Pmsg object helpers

Msgcore_EXT CString
P3Pmsg_GetRootname ( LPCTSTR lpszItemPath );
Msgcore_EXT CString
P3Pmsg_GetPath ( const P3PmsgField *pField );
Msgcore_EXT CString
P3Pmsg_GetPath ( const P3PmsgAttr *pAttr );
Msgcore_EXT CString
P3Pmsg_GetPath ( const P3PmsgDesc *pDesc );
Msgcore_EXT VBLsize
P3Pmsg_GetStckDepth ( const P3PmsgField *pItem, VBLaddr aOwner );
Msgcore_EXT P3PmsgObject
P3Pmsg_GetRoot ( const P3PmsgItem *pItem );
Msgcore_EXT BOOL
P3Pmsg_IsValidItemname ( LPCWSTR lpszItemname, wchar_t *pwchar = 0 );

Msgcore_EXT P3PmsgObject
P3Pmsg_SelectObject ( const P3PmsgObject *pObject, LPCTNAM lpszObjectPath );

Msgcore_EXT BOOL
P3Pmsg_IsPathDelimiter ( LPCWSTR lpszObjectPath );
Msgcore_EXT BOOL
P3Pmsg_SplitRootPath ( LPCWSTR lpszObjectPath, CString& strRoot
                     , CList<CString>& oCListItems );
Msgcore_EXT int
P3Pmsg_fwprintf ( FILE *fd, LPCTSTR lpszFormat, ... );


///////////////////////////////////////////////////////////////////////
//  P2Pmsg tagging helpers

Msgcore_EXT INT64
P3Pmsg_GetTStamp ( const P3PmsgItem& oItem );
Msgcore_EXT INT64
P3Pmsg_SetTStamp ( const P3PmsgItem& oItem, INT64 tsValue = -1, BOOL bRecurse = FALSE );

///////////////////////////////////////////////////////////////////////////
//  P2Pmsg refactoring helpers

Msgcore_EXT void
P3PmsgRefactor_Delete ( P3PmsgField& oItem, LPCTSTR lpszItemName );
Msgcore_EXT void
P3PmsgRefactor_Delete ( P3PmsgAttr& oItem, LPCTSTR lpszItemName );
Msgcore_EXT void
P3PmsgRefactor_DeleteFromParents ( const P3PmsgField& oItem, LPCTSTR lpszItemName );
Msgcore_EXT int
P3PmsgRefactor_DeleteWithQualAttributes ( P3PmsgAttr& oAttr, LPCTSTR lpszAttribute1
                  , LPCTSTR lpszAttribute2 = nullptr, LPCTSTR lpszAttribute3 = nullptr );
Msgcore_EXT void
P3PmsgRefactor_Rename ( P3PmsgField& oItem, LPCTSTR lpszItemName, LPCTSTR lpszItemNew );
Msgcore_EXT void
P3PmsgRefactor_Rename ( P3PmsgAttr& oAttr, LPCTSTR lpszItemName, LPCTSTR lpszItemNew );
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgField& oItemSource, P3PmsgField& oItemDestin, LPCTSTR lpszItemName );
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgField& oItemSource, P3PmsgAttr& oAttrDestin, LPCTSTR lpszItemName );
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgAttr& oAttrSource, P3PmsgField& oItemDestin, LPCTSTR lpszItemName );
Msgcore_EXT void
P3PmsgRefactor_Move ( P3PmsgAttr& oAttrSource, P3PmsgAttr& oAttrDestin, LPCTSTR lpszItemName );
Msgcore_EXT void
P3PmsgRefactor_DataType ( P3PmsgItem& oItemParent, LPCTSTR lpszItemname, const P3PmsgData& oData );
Msgcore_EXT void
P3PmsgRefactor_CastDataType (P3PmsgItem& oItem, LPCTSTR lpszItemName, char ucNewDataType);
Msgcore_EXT void
P3PmsgRefactor_CastDataType (P3PmsgAttr& oAttr, LPCTSTR lpszAttrName, char ucNewDataType);
Msgcore_EXT void    
P3PmsgRefactor_CastDataType ( P3PmsgData& oData, char ucNewDataType );

///////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------------
extern Msgcore_EXT bool
g_bP2Pmsg_AssertValid;

class P2Pint__
{
    // Constructors and destruction
    public:
        P2Pint__ () {}
        P2Pint__ ( const P3PmsgData& rhs ) { m_oData = rhs; }
        //P2Pint { const P3PmsgField& rhs ) { m_oData = rhs.r_data(); }
      virtual
       ~P2Pint__ () {}
    // Overloaded operators
    public:
      P2Pint__&
        operator = ( const P3PmsgData& rhs ) { if (&m_oData!=&rhs)m_oData = rhs; return *this; };
      P2Pint__&
        operator = ( int vInt ) { m_oData.c_int(vInt); return *this; }
      bool
        operator == ( int vInt ) { return (m_oData.c_int()==vInt)?true:false; }
        operator P3PmsgData& ( ) { return m_oData; }
        operator int () const { return m_oData.c_int(); }
    // Properties
    public:
      P3PmsgData&
        r_data ( ) { return m_oData; }
      const P3PmsgData&
        r_data ( ) const { return m_oData; }
    // Attributes
    public:
        P3PmsgData m_oData;
};

#define P2Ptype_Internals(theClass, c_type, c_func, uTYPE) \
    public: \
        theClass () { m_oData.Recreate(uTYPE,0,0); m_oData.Nullify(); } \
        theClass ( const P3PmsgData& rhs ) { m_oData = rhs; } \
        theClass ( c_type value ) { m_oData.Recreate(uTYPE,0,0); m_oData.c_func(value); } \
      virtual \
       ~theClass () {} \
    public: \
      theClass& \
        operator = ( const P3PmsgData& rhs ) { if (&m_oData!=&rhs)m_oData = rhs; return *this; } \
      theClass& \
        operator = ( const P3PmsgObject& rhs ) { *(P3PmsgObject*)m_oData.p_Object() = rhs; return *this; } \
      theClass& \
        operator = ( c_type value ) { m_oData.c_func(value); return *this; } \
      theClass& \
        operator -= ( c_type value ) { m_oData.c_func(m_oData.c_func()-value); return *this; } \
      theClass& \
        operator -= ( const theClass& rhs ) { m_oData.c_func(m_oData.c_func()-rhs.m_oData.c_func()); return *this; } \
      theClass \
        operator - ( c_type value ) { return m_oData.c_func()-value; } \
      theClass \
        operator - (const theClass& rhs ) { return m_oData.c_func()-rhs.m_oData.c_func(); } \
      theClass& \
        operator += ( c_type value ) { m_oData.c_func(m_oData.c_func()+value); return *this; } \
      theClass& \
        operator += ( const theClass& rhs ) { m_oData.c_func(m_oData.c_func()+rhs.m_oData.c_func()); return *this; } \
      theClass \
        operator + ( c_type value ) { return m_oData.c_func()+value; } \
      theClass \
        operator + (const theClass& rhs ) { return m_oData.c_func()+rhs.m_oData.c_func(); } \
      theClass& \
        operator *= ( c_type value ) { m_oData.c_func(m_oData.c_func()*value); return *this; } \
      theClass& \
        operator *= ( const theClass& rhs ) { m_oData.c_func(m_oData.c_func()*rhs.m_oData.c_func()); return *this; } \
      theClass \
        operator * ( c_type value ) { return m_oData.c_func()*value; } \
      theClass \
        operator * (const theClass& rhs ) { theClass theResult = m_oData.c_func()*rhs.m_oData.c_func(); return theResult; } \
      theClass& \
        operator /= ( c_type value ) { m_oData.c_func(m_oData.c_func()/value); return *this; } \
      theClass& \
        operator /= ( const theClass& rhs ) { m_oData.c_func(m_oData.c_func()/rhs.m_oData.c_func()); return *this; } \
      theClass \
        operator / (const theClass& rhs ) { return m_oData.c_func()/rhs.m_oData.c_func(); } \
      bool \
        operator == ( c_type value ) { return (m_oData.c_func()==value)?true:false; } \
        operator P3PmsgData& ( ) noexcept { return m_oData; } \
        operator c_type () const { return m_oData.c_func(); } \
    public: \
      P3PmsgData& \
        r_data ( ) noexcept { return m_oData; } \
      const P3PmsgData& \
        r_data ( ) const noexcept { return m_oData; } \
      const c_type \
        c_func ( ) const { return m_oData.c_func(); } \
    public: \
        P3PmsgData m_oData;

//class   P2Pint08  { P2Ptype_Internals(P2Pint08,INT08,c_char) };
//class   P2Pint16  { P2Ptype_Internals(P2Pint16,INT16,c_short) };
//class P2Puint16 { P2Ptype_Internals(P2Puint16,UINT16,c_ushort) };
class   P2Pint32  { P2Ptype_Internals(P2Pint32,INT32,c_int,VBLockData_INT32) };
typedef P2Pint32  P2Pint;
class   P2Puint32 { P2Ptype_Internals(P2Puint32,UINT32,c_uint,VBLockData_UINT32) };
typedef P2Puint32 P2Puint;
class   P2Pint64  { P2Ptype_Internals(P2Pint64,INT64,c_time64,VBLockData_INT64) };
class   P2PDWORD  { P2Ptype_Internals(P2PDWORD,DWORD,c_uint,VBLockData_UINT32) };
class   P2Pdouble { P2Ptype_Internals(P2Pdouble,double,c_double,VBLockData_DOUBLE) };
class   P2PBOOL   { P2Ptype_Internals(P2PBOOL,int,c_int,VBLockData_INT32) };
#define c_DATE c_double
class   P2PDATE   { P2Ptype_Internals(P2PDATE,DATE,c_DATE,VBLockData_DOUBLE) };
typedef P2PDWORD  P2Pcolor;
//class   P2Puint32 { P2Ptype_Internals(P2Puint32,UINT32,c_uint) };
//typedef P2Puint32 P2Puint;
//class P2Pint64  { P2Ptype_Internals(P2Pint64,INT64,c_int64) };
//class P2Puint64 { P2Ptype_Internals(P2Puint64,UINT64,c_int64) };
//class   P2Pfloat  { P2Ptype_Internals(P2Pfloat,float,c_float) };
//class   P2Pdouble { P2Ptype_Internals(P2Pdouble,double,c_double) };

/*template<typename type, typename C_TYPE>
class P2Ptype
{
    // Constructors and destruction
    public:
        P2Ptype () { }

        P2Ptype ( const P3PmsgData& rhs )
        {
          m_oData = rhs;
        }

       ~P2Ptype () { }

    // Overloaded operators
    public:
      P2Ptype&
        operator = ( const P3PmsgData& rhs )
        {
          if ( this != &rhs )
            m_oData = rhs;
          return *this;
        }
      P2Ptype&
        operator = ( type c_type )
        {
          m_oData.#C_TYPE() = c_type;
          return P2Ptype;
        }
        operator type ()
        {
          return m_oData;
        }

    // Properties
    public:
      bool
        IsNull ( );
      bool
        IsVoid ( );

    // Attributes
    public:
        P3PmsgData m_oData;
};

typedef P2Ptype<short, short> P2Pshort;
typedef P2Ptype<int, int>   P2Pint;
        //int iJohnson = oNode["johnson"].c_int();*/
//
//  P2Pc_vBlob 
//  NOTES: Primarily used to manage pure data structures within the
//         context of P3PmsgData objects
//
template<class StructType, int BLOBnn=VBLockData_BLOB08>
class P2Pc_vBlob
{
    // Constructors and destruction
    public:
        P2Pc_vBlob ( const P2Pc_vBlob& rhs )
        { *this = rhs; }
        P2Pc_vBlob () : m_oData ( (void*)0, sizeof(StructType), BLOBnn )
        {
          if ( BLOBnn == VBLockData_BLOB08 )
            ASSERT(sizeof(StructType)<=227);
          //m_oData.Recreate ( VBLockData_BLOB08, 0, sizeof(StructType) );
          //m_oData.Nullify ( );
        }
        P2Pc_vBlob ( const P3PmsgData& rhs ) { m_oData = rhs;}

        P2Pc_vBlob ( const P3PmsgObject& rhs ) { *this = rhs;}

        P2Pc_vBlob ( StructType *pStructType )
        {
          if ( BLOBnn == VBLockData_BLOB08 )
            ASSERT(sizeof(StructType)<=227);
          m_oData.Recreate ( BLOBnn, pStructType, sizeof(StructType) );
        }
        P2Pc_vBlob ( const StructType& oStructType )
        {
          if ( BLOBnn == VBLockData_BLOB08 )
            ASSERT(sizeof(StructType)<=227);
          m_oData.Recreate ( BLOBnn, &oStructType, sizeof(StructType) );
        }
      virtual
       ~P2Pc_vBlob ( ) { }

    // Overloaded operators
    //  THESE THREE OVERLAY StructType DIRECTLY ON THE STORED BYTES, and the
    //  stored bytes carry NO ALIGNMENT GUARANTEE - see c_vBlob() above for why
    //  the format cannot offer one.  If StructType needs alignment (anything
    //  with an int, a pointer, a double or a wchar_t in it - so, most things)
    //  every access through these is undefined behaviour.  It works on x86,
    //  which is why it has always appeared to; UBSan reports it on Linux and a
    //  strict-alignment target faults.  TargetCore's finding F-S5-3.
    //
    //  PREFER Load() and Store() below.  They are the same bytes through a
    //  memcpy, and they are defined everywhere.
    //
    //  These are kept, rather than changed to return a flush-back proxy,
    //  because they are the shape every existing consumer is written against
    //  (P2P_LOGFONT, P2P_LOGPEN, P2P_LOGBRUSH and the chart view tag, all in
    //  MFC-side projects) and a proxy changes WHEN a write lands.  Making them
    //  safe is a separate change, and it wants those projects buildable first.
    public:
      StructType*
        operator->() const { return (StructType *)m_oData.c_vBlob(); }
      
        operator StructType* () { return (StructType *)m_oData.c_vBlob(); }

        operator StructType& () { return *(StructType *)m_oData.c_vBlob(); }

      P2Pc_vBlob&
        operator = ( const P2Pc_vBlob& rhs ) { if ( this != &rhs ) m_oData = rhs.m_oData; return *this; }
      P2Pc_vBlob&   
        operator = ( P3PmsgData& rhs ) { m_oData = rhs.r_data(); return *this; }
      P2Pc_vBlob&
        operator = ( const P3PmsgObject& rhs ) { *(P3PmsgObject*)m_oData.p_Object() = rhs; return *this; }
        operator P3PmsgData& ( ) { return m_oData; }

      P2Pc_vBlob&
        operator = ( const StructType *pStructType )
        {
          m_oData.c_memcpy ( pStructType, sizeof(StructType) );
          return *this;
        }

    // Operations
    public:
     public: 
      P3PmsgData& 
        r_data ( ) { return m_oData; }
      const P3PmsgData& 
        r_data ( ) const { return m_oData; }
      //  The alignment-safe pair, and the one to reach for.  Load() copies the
      //  stored bytes into an object the CALLER aligned; Store() copies them
      //  back.  Identical bytes to the overlay operators above, defined on
      //  every target rather than only on the forgiving ones.
      //    Read-modify-write is Load, change, Store - which also makes the
      //  write a visible act instead of something that happened through an
      //  overlaid reference.
      bool
        Load ( StructType& oOut ) const
        {
          return const_cast<P3PmsgData&>(m_oData).c_vBlobCopy
                   ( &oOut, sizeof(StructType) ) == sizeof(StructType);
        }
      void
        Store ( const StructType& oIn )
        { m_oData.c_memcpy ( &oIn, sizeof(StructType) ); }
      void
        ZeroContent ( ) { ::ZeroMemory(m_oData.c_vBlob(),sizeof(StructType)); }

    // Attributes
    private:
        P3PmsgData m_oData;
};
using P2Pc_vBlob16 = P2Pc_vBlob<struct StructType, VBLockData_BLOB16>;

//
//  P2Pc_str 
//  NOTES: Primarily used to manage strings within the
//         context of P3PmsgData objects
//
template<UCHAR uBSTRnn>
class P2Pc_str
{
    // Constructors and destruction
    public:
        P2Pc_str () : m_oData ( (LPCTSTR*)0, 32, uBSTRnn )
        {
          //m_oData.Recreate ( VBLockData_BLOB08, 0, sizeof(StructType) );
          //m_oData.Nullify ( );
        }
        P2Pc_str ( const P3PmsgData& rhs ) { m_oData = rhs;}

        P2Pc_str ( LPCTNAM lpszString )
        {
          m_oData.Recreate ( uBSTRnn, lpszString, _tcslen(lpszString)*sizeof(TCHAR) );
        }
        //P2Pc_str ( const StructType& oStructType )
        //{
        //  m_oData.Recreate ( uBSTRnn, &oStructype, sizeof(StructType) );
        //}
      virtual
       ~P2Pc_str ( ) { }

    // Overloaded operators
    public:
      //StructType*
      //  operator->() const { return (StructType *)m_oData.c_wstr(); }
      
        operator LPCWSTR () { return m_oData.c_wstr(); }

      //  operator StructType& () { return *(StructType *)m_oData.c_wstr(); }

      P2Pc_str&
        operator = ( const P2Pc_str& rhs ) { m_oData = rhs.m_oData; return *this; }
      P2Pc_str&
        operator = ( P3PmsgData& rhs ) { m_oData = rhs.r_data(); return *this; }
      P2Pc_str&
        operator = ( const P3PmsgObject& rhs ) { *(P3PmsgObject*)m_oData.p_Object() = rhs; return *this; }
        operator P3PmsgData& ( ) { return m_oData; }

      P2Pc_str&
        operator = ( LPCTSTR lpszString )
        {
          m_oData.c_memcpy ( lpszString, wcslen(lpszString)*sizeof(lpszString[0]) );
          return *this;
        }

    // Operations
    public:
     public: 
      P3PmsgData& 
        r_data ( ) { return m_oData; }
      const P3PmsgData& 
        r_data ( ) const { return m_oData; }
      const LPCWSTR
        c_wstr ( ) const { return m_oData.c_wstr(); }

    // Attributes
    private:
        P3PmsgData m_oData;
};
typedef P2Pc_str<VBLockData_WSTR08>    P2Pc_str08;
typedef P2Pc_str<VBLockData_WSTR16>    P2Pc_str16;
typedef P2Pc_str<VBLockData_WSTR32>    P2Pc_str32;
typedef P2Pc_str<VBLockData_WSTR08>    P2Pc_wstr08;
typedef P2Pc_str<VBLockData_WSTR16>    P2Pc_wstr16;
typedef P2Pc_str<VBLockData_WSTR32>    P2Pc_wstr32;
