// Copyright © 2007-2015, 2022, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  Core VBLock (Virtual Block) memory model and data structure definitions
//  for the P2P messaging framework.
//  NOTES: This module defines the fundamental packed memory layout used
//         to store, link, and transport all P2Pmsg entities. All objects
//         (fields, lists, vectors, attributes, descendants, and data)
//         are represented as VBLock structures within a contiguous or
//         fragmented memory heap.
//       : Each allocated block is prefixed with a VBLockHdr and accessed
//         via relative addresses (VBLaddr), enabling relocation-safe,
//         pointer-free structures suitable for serialization, IPC, and
//         persistent storage.
//
//  Architecture Overview
//       : VBLock
//         Fundamental allocation unit (header + union payload)
//       : VBLockItem
//         Generic node supporting linkage and hierarchy
//       : VBLockField
//         Name/Data pair container
//       : VBLockList
//         Linked list container of items
//       : VBLockVect
//         Indexed vector container
//       : VBLockAttr
//         Attribute collection attached to items
//       : VBLockDesc
//         Descendant (child) collection
//       : VBLockRoot
//         Heap root and allocation tracking
//
//  Supporting structures:
//       : VBLockData
//         Typed data storage (scalars, strings, blobs, GUIDs)
//       : VBLockName
//         Variable-length name storage
//       : VBLockStack
//         Push/pop traversal support
//
//  Memory Model
//       : All structures are tightly packed binary (#pragma pack(1))
//       : Relationships are maintained via VBLaddr offsets, not pointers
//       : Supports 8, 16, 32, and 64-bit addressing modes via templated layouts
//       : Variable-sized data (BSTR, WSTR, BLOB) stored using embedded headers
//       : Chaining is used to avoid relocation of large data blocks
//       : Fragmentation is handled through allocation and coalescing of adjacent
//         VBLock blocks within the heap.
//
//  Key Capabilities
//       : Address-based navigation (next, prev, parent, child, attribute links)
//       : Dynamic sizing and allocation of heterogeneous data types
//       : Efficient in-place modification without pointer invalidation
//       : Support for recursive hierarchical message structures
//       : Blob and string handling with variable-length encoding
//       : File persistence via direct VBLock read/write operations
//
//  SUMMMARY
//       : This file underpins all higher-level P2Pmsg and P3Pmsg abstractions
//       : Strong coupling with Msgcore and heap management subsystems
//       : Callers must ensure containment and address validity when operating
//         on raw VBLock memory
//       : Designed for performance-critical environments with minimal copying
//       : Use with caution: low-level manipulation of VBLock structures requires
//         careful attention to memory layout, alignment, and referential
//         integrity.  Use the high-level P3Pmsg* classes and methods where
//         possible to avoid errors.
// 
//
#pragma once
#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif

#include "Msgcore.h"
#include "wtypes.h"

//  P2PWCHAR: storage-pinned wide char for packed VBLock/wire layouts.
//  Pinned to 16-bit so serialized VBHeap images are
//  bit-identical across platforms. On Windows wchar_t is already 16-bit, so
//  P2PWCHAR == wchar_t keeps the on-disk layout byte-for-byte unchanged; on
//  Linux (where wchar_t is 32-bit) it pins to char16_t.
#ifdef _WIN32
typedef wchar_t  P2PWCHAR;
#else
typedef char16_t P2PWCHAR;
#endif

///////////////////////////////////////////////////////////////////////
//  VBLockHdr definitions
//  NOTES: Internalised memory blocks MUST be prefixed with this
//         object

//
//  P3PmsgData constraints
typedef unsigned char P2Pcons_t;
typedef unsigned char VBLockDataAttr;
#define VBLockAttr_READ     (1<<0)     // Read
#define VBLockAttr_WRITE    (1<<1)     // Write
#define VBLockAttr_AMEND    (1<<2)     // Ammend
#define VBLockAttr_DROP     (1<<3)     // Delete
#define VBLockAttr_NULLABLE (1<<4)
#define VBLockAttr_NULL     (1<<7)     // Null entry
#define VBLockAttr_DEFAULT ((1<<0)|(1<<1)|(1<<2)|(1<<4))

///////////////////////////////////////////////////////////////////////////////
//  P2PmsgData container and definitions
//  NOTES: Base class for VBLockData structures
//       : Intended for standalone or derived instanciation
typedef unsigned char P2Pvar_t;        // P2PmsgData variable type
typedef unsigned char VBLockDataType;
#define VBLockData_NULL       0
                                       // Entry type Builtin
#define VBLockData_INT08      1        //   char
#define VBLockData_UINT08     2        //   unsigned char
#define VBLockData_INT16      3        //   short
#define VBLockData_UINT16     4        //   unsigned short
#define VBLockData_INT32      5        //   int
#define VBLockData_UINT32     6        //   unsigned int
#define VBLockData_INT64      7        //   long
#define VBLockData_UINT64     8        //   unsigned long
#define VBLockData_FLOAT      9        //   float
#define VBLockData_DOUBLE    10        //   double
#define VBLockData_TIME32    11        //   time 32 bits
#define VBLockData_TIME64    12        //   time 64 bits
#define VBLockData_BOOL      13        //   bool
#define VBLockData_WCHAR     14        //   wchar_t
                                       // Entry type BSTR - ASCII
#define VBLockData_BSTR08    16        //   08 bits
#define VBLockData_BSTR08var 17        //   16 bits variable
#define VBLockData_BSTR16    18        //   32 bits
#define VBLockData_BSTR16var 19        //   08 bits variable
#define VBLockData_BSTR32    20        //   16 bits
#define VBLockData_BSTR32var 21        //   32 bits variable
                                       // Entry type WSTR - UNICODE
#define VBLockData_WSTR08    24        //   08 bits
#define VBLockData_WSTR08var 25        //   16 bits variable
#define VBLockData_WSTR16    26        //   32 bits
#define VBLockData_WSTR16var 27        //   08 bits variable
#define VBLockData_WSTR32    28        //   16 bits
#define VBLockData_WSTR32var 29        //   32 bits variable
                                       // Entry type Blob
#define VBLockData_BLOB08    32        //   08 bits
#define VBLockData_BLOB08var 33        //   08 bits variable
#define VBLockData_BLOB16    34        //   16 bits
#define VBLockData_BLOB16var 35        //   16 bits variable
#define VBLockData_BLOB32    36        //   32 BLOB
#define VBLockData_BLOB32var 37        //   32 BLOB variable
                                       // Reserved for future use
#define VBLockData_GUID      47        //   16 byte GUID
#define VBLockData_EODefs    48

#pragma pack(push,1)
template < typename SIZE__ >
struct VBLob__var
{ 
    SIZE__  nBlobSize;       // Characters (as defined by cBlob)
    SIZE__  nBlobUsed;       //   as above
    P2PWCHAR cBlob;          //   units (char or wchar) -- 16-bit pinned (§4.2)
};
typedef VBLob__var <UINT08> VBLob08;
typedef VBLob__var <UINT16> VBLob16;
typedef VBLob__var <UINT32> VBLob32;
typedef VBLob__var <UINT64> VBLob64;

template < typename SIZE__ >
struct VBLin__var
{ 
    SIZE__    nBlobSize;
    VBLaddr32 aVBLockAddr;
};
typedef VBLin__var <UINT08> VBLin08;
typedef VBLin__var <UINT16> VBLin16;
typedef VBLin__var <UINT32> VBLin32;
typedef VBLin__var <UINT32> VBLin64;

template < typename SIZE__ >
struct VBLnx__var
{ 
    SIZE__    nBlobSize;
    union
    {
      VBLaddr16 aChain2Next16;
      VBLaddr32 aChain2Next32;
      VBLaddr64 aChain2Next64;
    } u;
};
typedef VBLnx__var <UINT08> VBLnx08;
typedef VBLnx__var <UINT16> VBLnx16;
typedef VBLnx__var <UINT32> VBLnx32;
typedef VBLnx__var <UINT32> VBLnx64;

typedef struct
{
    UCHAR  uDataType;
    UCHAR  uDataAttr;
    union
    {
      INT08          vInt08;
      UINT08         vuInt08;
      INT16          vInt16;
      UINT16         vuInt16;
      INT32          vInt32;
      UINT32         vuInt32;
      TIME32         vTime32;
      INT64          vInt64;
      UINT64         vuInt64;
      TIME64         vTime64;
      wchar_t        vwChar;
      float          vFloat;
      double         vDouble;
      bool           vBool;
      GUID           vGUID;
      VBLob08        vBlob08;
      VBLob16        vBlob16;
      VBLob32        vBlob32;
      VBLob64        vBlob64;
      VBLaddr32      aVBLockDataas;   // TODO:LJM was VBLaddr
                                    // P3PmdgHANDLE Address of next chained data item
      VBLaddr16      aChain2Next16; // NOTES: Sometimes when data size chains it's
      VBLaddr32      aChain2Next32; //        easier to create a new data object rather
      VBLaddr64      aChain2Next64; //        than displace the whole P2PmsgItem in memory
      VBLaddr64      aChain2Nextxx; //        These chains are subsequently copy displaced
      char           aAlloc[126];
    } u;
} VBLockData;
#pragma pack(pop)


///////////////////////////////////////////////////////////////////////////////
//  P2PmsgName containers and definitions
//  NOTES: Base class for VBLockName structures
//       : Intended for standalone or derived instanciation
#pragma pack(push,1)
typedef struct
{
    UCHAR  uVBLockAttr;
    union
    {
      VBLob08  vBlob08;
      VBLin08  vBlin08;
      VBLnx08  vBlnx08;
      P2PWCHAR aAlloc[63];   // 16-bit pinned (§4.2): sizes the inline-name capacity;
                             // keeps VBLockName layout identical Win/Linux
    } u;
} VBLockName;
#pragma pack(pop)

///////////////////////////////////////////////////////////////////////
//  VBLockField container and definitions
//  NOTES: Base class for VBLockField structures
//       : Intended for standalone or derived instanciation
#pragma pack(push,1)
typedef struct
{
    UCHAR       uAccessAttr;
    VBLockName  oVBLockName;
    VBLockData  oVBLockData;
} VBLockField;
struct VBLock_;
#pragma pack(pop)

typedef struct
{
    UINT_PTR uiParam1;
    UINT_PTR uiParam2;
    UINT_PTR uiParam3;
} P2PmsgFieldHdl;

const UCHAR AttrField_HIDDEN  =  1;    // Hidden, known access only
const UCHAR AttrField_NOCOPY  =  2;    // Not copyable, time and space dependant
const UCHAR AttrField_SORT    =  4;    // Sorted
const UCHAR AttrField_TOUCH   =  8;    // Touched item bit
const UCHAR AttrField_EXPAND  = 64;    // Expanded item

const UCHAR AttrField_DEFAULT = 0;
const UCHAR AttrField_TRIGGER = (AttrField_HIDDEN|AttrField_NOCOPY);

///////////////////////////////////////////////////////////////////////////////
//  VBLockList containers and definitions
//  NOTES: Base class for VBLockList structures
//       : Intended for standalone or derived instanciation
#pragma pack(push,1)
template < typename ADDR__, typename SIZE__ >
struct VBLockList__
{ 
    ADDR__  aFirst;
    ADDR__  aLast;
    SIZE__  nItems;
};
typedef VBLockList__ <UINT08,UINT08> VBLockList08;
typedef VBLockList__ <UINT16,UINT16> VBLockList16;
typedef VBLockList__ <UINT32,UINT32> VBLockList32;
typedef VBLockList__ <UINT64,UINT32> VBLockList64;

typedef struct
{
    UCHAR  uVBLockAttr;
    union
    {
      VBLockList08 oList08;
      VBLockList16 oList16;
      VBLockList32 oList32;
      VBLockList64 oList64;
    } u;
    VBLockField oVBLockField;
} VBLockList;
#pragma pack(pop)

#define MAX_P3PmsgData_Curs 3

typedef struct
{
    UINT_PTR uiParam1;
    UINT_PTR uiParam2;
    UINT_PTR uiParam3;
} P2PmsgListHdl;

///////////////////////////////////////////////////////////////////////////////
//  VBLockAttr containers and definitions
//  NOTES: Base class for VBLockAttr structures
//       : Intended for standalone or derived instanciation
#pragma pack(push,1)
template < typename ADDR__, typename SIZE__ >
struct VBLockAttr__
{
    ADDR__  aParent;
    ADDR__  aFirst;
    ADDR__  aLast;
    SIZE__  nItems;
};
typedef VBLockAttr__ <UINT08,UINT08> VBLockAttr08;
typedef VBLockAttr__ <UINT16,UINT16> VBLockAttr16;
typedef VBLockAttr__ <UINT32,UINT32> VBLockAttr32;
typedef VBLockAttr__ <UINT64,UINT32> VBLockAttr64;

typedef struct
{
    //UCHAR  uVBLockAttr_;
    UCHAR  uPermissions;
    union
    {
      VBLockAttr08 oAttr08;
      VBLockAttr16 oAttr16;
      VBLockAttr32 oAttr32;
      VBLockAttr64 oAttr64;
    } u;
} VBLockAttr;
#pragma pack(pop)

typedef struct
{
  UINT_PTR uiParam1;
  UINT_PTR uiParam2;
  UINT_PTR uiParam3;
} P2PmsgAttrHdl;

///////////////////////////////////////////////////////////////////////////////
//  VBLockVect containers and definitions
//  NOTES: Base class for VBLockVect structures
//       : Intended for standalone or derived instanciation
//#define MAX_P3PmsgData_Curs 3
#pragma pack(push,1)
template < typename ADDR__, typename SIZE__ >
struct VBLockVect__
{ 
    ADDR__  aExtra;
    SIZE__  nItems;
    ADDR__  aAlloc[32];
};
typedef VBLockVect__ <UINT08,UINT08> VBLockVect08;
typedef VBLockVect__ <UINT16,UINT16> VBLockVect16;
typedef VBLockVect__ <UINT32,UINT32> VBLockVect32;
typedef VBLockVect__ <UINT64,UINT32> VBLockVect64;

typedef struct
{
    UCHAR  uVBLockAttr;
    union
    {
      VBLockVect08 oVect08;
      VBLockVect16 oVect16;
      VBLockVect32 oVect32;
      VBLockVect64 oVect64;
    } u;
    VBLockField oVBLockField;
} VBLockVect;
#pragma pack(pop)

typedef struct
{
    UINT_PTR uiParam1;
    UINT_PTR uiParam2;
    UINT_PTR uiParam3;
} P2PmsgVectHdl;

///////////////////////////////////////////////////////////////////////////////
//  VBLockDesc containers and definitions
//  NOTES: Base class for VBLockDesc structures
//       : Intended for standalone or derived instanciation
#pragma pack(push,1)
template < typename ADDR__, typename SIZE__ >
struct VBLockDesc__
{
    ADDR__  aParent;
    ADDR__  aFirst;
    ADDR__  aLast;
    SIZE__  nItems;
};
typedef VBLockDesc__ <UINT08,UINT08> VBLockDesc08;
typedef VBLockDesc__ <UINT16,UINT16> VBLockDesc16;
typedef VBLockDesc__ <UINT32,UINT32> VBLockDesc32;
typedef VBLockDesc__ <UINT64,UINT32> VBLockDesc64;

typedef struct
{
    //UCHAR  uAttributes;
    UCHAR  uPermissions;
    union
    {
      VBLockDesc08 oDesc08;
      VBLockDesc16 oDesc16;
      VBLockDesc32 oDesc32;
      VBLockDesc64 oDesc64;
    } u;
} VBLockDesc;
#pragma pack(pop)

typedef struct
{
    UINT_PTR uiParam1;
    UINT_PTR uiParam2;
    UINT_PTR uiParam3;
} P2PmsgDescHdl;


///////////////////////////////////////////////////////////////////////////////
//  VBLockStack containers and definitions
//  NOTES: Base class for VBLockStack structures
//       : Intended for standalone or derived instanciation
#pragma pack(push,1)
typedef struct
{
    UCHAR     uStackItem;              // Pushed item attributes
    VBLaddr32 aStackItem;              // Address of pushed item
    union
    {
      //VBLockNode   oVBLockNode;
      VBLockField  oVBLockField;
      VBLockList   oVBLockList;
    } u;
} VBLockStack;
#pragma pack(pop)

typedef struct
{
    UINT_PTR uiParam1;
    UINT_PTR uiParam2;
    UINT_PTR uiParam3;
} P2PmsgStckHdl;

///////////////////////////////////////////////////////////////////////
//  P2PmsgItem's
//  NOTES: Generic P2PmsgItem container

#define VBItem_TypeMask 0x3C           // Types            (Bits 2-5)
#define VBItem_Item        4           //   Item
#define VBItem_Spare       8           //   Spare
#define VBItem_List       12           //   List
#define VBItem_Vect       16           //   Vect

#pragma pack(push,1)
template < typename ADDR__ >
struct VBItemNN__
{ 
    //ADDR__  nSize;
    ADDR__  aParent;
    ADDR__  aPrev;
    ADDR__  aNext;
    ADDR__  aExtra;          // Attributes
    ADDR__  aStack;          // Stack
    ADDR__  aDescn;          // Descendants
};
typedef VBItemNN__ <UINT08> VBItem08;
typedef VBItemNN__ <UINT16> VBItem16;
typedef VBItemNN__ <UINT32> VBItem32;
typedef VBItemNN__ <UINT64> VBItem64;

typedef struct
{
    UCHAR uItemType; // uVBLockItem;
  //UCHAR uItemProps;        // Item properties
    union
    {
      VBItem08 oItem08;
      VBItem16 oItem16;
      VBItem32 oItem32;
      VBItem64 oItem64;
    } ua;
    union
    {
      VBLockData   oVBLockData;
      //VBLockNode   oVBLockNode;
      VBLockField  oVBLockField;
      VBLockList   oVBLockList;
      VBLockStack  oVBLockStack;
    } ut;
} VBLockItem;
#pragma pack(pop)

///////////////////////////////////////////////////////////////////////
//  VBLock container and definitions
//  NOTES: Fundamental unit of memory allocation and subsequent 
//         fragmentation.  De-fragmentaion occurs through collation
//         of free'd adjacent VBLock's

#pragma pack(push,1)
typedef struct VBLock_
{                                      // Universal header
    VBLockHdr    oHdr;                 // VBLockHdr is common to both VBHeap & VBLock's
    union
    {
      VBLockItem  oItem;
      VBLockAttr  oAttr;               // Item attributes
      VBLockDesc  oDesc;               // Item descendants
      VBLockName  oName;               // Extended item name
      VBLockData  oData;               // Extended item data
    } ud;                              // Contents, refer uVBLock
} VBLock;
#pragma pack(pop)

///////////////////////////////////////////////////////////////////////
//  VBLockHdr definitions
//  NOTES: Internalised memory blocks MUST be prefixed with this
//         object
#define VBLockHdr_Indirect 1           // Indirection bit

//
//  VBListData and VBLockData containers and macros
//  NOTES:
typedef UCHAR VBLockType;

#define VBLobType_Item    16           //   Item (single)
#define VBLobType_Array   32           //   Array
#define VBLobType_Vector  48           //   Vector
#define VBLobType_List    64           //   List

///////////////////////////////////////////////////////////////////////////////
//  VBLockRoot containers and definitions
//  NOTES: Placeholder for VBListItem (Vect, Fields, etc) data
//       : Supports delegated, contiguous or fragment memory allocation
#pragma pack(push,1)
template < typename ADDR__, typename SIZE__ >
struct VBLockRoot__
{ 
    ADDR__  aFirst;
    ADDR__  aLast;
    ADDR__  nSizeAlloc;
    ADDR__  nSizeUsed;
    SIZE__  nItems;
};
typedef VBLockRoot__ <UINT08,UINT08> VBLockRoot08;
typedef VBLockRoot__ <UINT16,UINT16> VBLockRoot16;
typedef VBLockRoot__ <UINT32,UINT32> VBLockRoot32;
typedef VBLockRoot__ <UINT64,UINT64> VBLockRoot64;

typedef struct
{
    UCHAR  uVBLock;
    UCHAR  uVBLockBSTR;
    UCHAR  uVBLockAttr;
    union
    {
      VBLockRoot08 oRoot08;
      VBLockRoot16 oRoot16;
      VBLockRoot32 oRoot32;
      VBLockRoot64 oRoot64;
    } u;
} VBLockRoot;
#pragma pack(pop)

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockHeap manipulation
void
VBLockRoot_Init    ( VBLockRoot *pRoot, UCHAR uVBLock, UINT nSizenn );
VBLaddr
VBLockRoot_GetFirst( const VBLockRoot *pRoot, int *pnItem = 0 );
VBLockRoot*
VBLockRoot_SetFirst( VBLockRoot *pRoot, VBLaddr aVBLock );
VBLaddr
VBLockRoot_GetLast ( const VBLockRoot *pRoot, int *pnItem = 0 );
VBLockRoot*
VBLockRoot_SetLast ( VBLockRoot *pRoot, VBLaddr aVBLock );
VBLaddr
VBLockRoot_GetItems( const VBLockRoot *pRoot );
VBLockRoot*
VBLockRoot_SetItems( VBLockRoot *pRoot, int nItems, bool bAbsolute = false );

///////////////////////////////////////////////////////////////////////////////
//  VBLockData containers and definitions
//  NOTES: Placeholder in VBLockItems, Vect, Fields, etc) data
//       : Supports delegated, contiguous or fragment memory allocation

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockData manipulation
void
VBLockData_Init     ( VBLockData *pData );
void
VBLockData_Init     ( VBLockData *pData
                    , UCHAR uVBLockAttr, UCHAR uVBLockData, VBLsize nSizeof );
void
VBLockData_InitBlob  ( VBLockData *pData
                     , VBLockDataAttr uVBLockAttr, VBLockDataType uVBLockData, VBLsize nBlobSize );
VBLaddr
VBLockData_GetChain2Next( UCHAR uVBLock, const VBLockData *pData );
VBLaddr
VBLockData_SetChain2Next( UCHAR uVBLock, VBLockData *pData, VBLaddr aChain2Next );
BOOL
VBLockData_IsChained ( const VBLockData *pData ) noexcept;
VBLsize
VBLockData_Sizeof    ( UCHAR uVBLock, const VBLockData *pData );
VBLsize
VBLockData_Sizeof    ( UCHAR uVBLock, VBLockDataType uDataType, VBLsize nSizeBlob );
VBLsize
VBLockData_Sizeof_uv ( UCHAR uVBLock, const VBLockData *pData );
VBLsize
VBLockData_Sizeof_Min( UCHAR uVBLock );
VBLsize
VBLockData_Sizeof_Alloc( VBLock *pVBLock );
bool
VBLockData_IsBlob    ( const VBLockData *pData ) noexcept;
bool
VBLockData_IsBSTR    ( const VBLockData *pData ) noexcept;
bool
VBLockData_IsWSTR    ( const VBLockData *pData ) noexcept;
void*
VBLockData_pcBlob    ( VBLockData *pData );
VBLsize
VBLockData_BlobSize  ( const VBLockData *pData );
VBLsize
VBLockData_BlobUsed  ( const VBLockData *pData );
VBLsize
VBLockData_BlobMax   ( const VBLockData *pData );
VBLsize
VBLockData_BlobMaxOf ( VBLockDataType uDataType );
VBLockDataType
VBLockData_WidenBlob ( VBLockDataType uDataType, VBLsize nBlobSize );
void*
VBLockData_BlobCopy  ( VBLockData *pData, const void *pvData, VBLsize nBlobSize );
VBLockData*
VBLockData_Copy ( VBLockData *pDataDst, VBLsize nSizeofDst
                , const VBLockData *pDataSrc, VBLsize nSizeofSrc );

///////////////////////////////////////////////////////////////////////
//  VBLockName containers and definitions
//  NOTES: Placeholder in VBLockItems, Vect, Fields, etc) data
//       : Supports delegated, contiguous or fragment memory allocation

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockName manipulation
void
VBLockName_Init ( UCHAR uVBLock, VBLockName *pName, UCHAR uVBLockAttr
                , LPCWSTR lpszName, VBLsize nSizenn );
VBLaddr
VBLockName_GetChain2Next ( UCHAR uVBLock, const VBLockName *pName );
VBLaddr
VBLockName_SetChain2Next ( UCHAR uVBLock, VBLockName *pName, VBLaddr aChain2Next );
BOOL
VBLockName_IsChained ( const VBLockName *pName ) noexcept;
VBLsize
VBLockName_Sizeof_Min ( UCHAR uVBLock ) noexcept;
VBLsize
VBLockName_Sizeof ( UCHAR uVBLock, size_t nNameSize );
VBLsize
VBLockName_Sizeof ( UCHAR uVBLock, const VBLockName *pName ); //Suspect implementation
VBLsize
VBLockName_Sizenn ( UCHAR uVBLock, const VBLockName *pName );
VBLsize
VBLockName_Sizeof_Alloc ( UCHAR uVBLock, const VBLockName *pName );
// The invariants VBLockName_Sizenn only ASSERTs, asked where they can act:
// throws unless the header is one VBLockName_Init could have written. Bound the
// header first -- this dereferences it. See the notes on the definition (D64).
void
VBLockName_ChkWellFormed ( UCHAR uVBLock, const VBLockName *pName
                         , LPCSTR lpszWhere );
VBLsize
VBLockName_BlobSize ( const VBLockName *pName );
VBLsize
VBLockName_BlobUsed ( const VBLockName *pName );

///////////////////////////////////////////////////////////////////////
//  VBLockField containers and definitions
//  NOTES: VBLock'ed container for VBLock(Name, Data, etc)
//       : Supports delegated, contiguous or fragment memory allocation

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockField manipulation
void
VBLockField_Init   ( VBLockField *pField, UCHAR uVBLockAttr );
VBLockName*
VBLockField_pName  ( const VBLockField *pField );
// pOwner is the block these offsets were read out of, where there is one. Pass
// it for anything that arrived as an image and the derivation is bounded against
// it step by step (VBLock_ChkContained); omit it and nothing changes.
VBLockData*
VBLockField_pData  ( UCHAR uVBLock, const VBLockField *pField
                   , const VBLock *pOwner = nullptr );
VBLsize
VBLockField_Dataos ( VBLockField *pField );
VBLsize
VBLockField_Sizeof ( );
VBLsize
VBLockField_Sizeof ( UCHAR uVBLock, const VBLockField *pField, BOOL bChain = FALSE );
//VBLsize
//VBLockField_Sizenn ( UCHAR uVBLock, VBLockField *pField );
VBLsize
VBLockField_Sizenn ( const VBLock_ *pVBLock );

///////////////////////////////////////////////////////////////////////
//  VBLockList containers and definitions
//  NOTES: Recursive VBLock'ed container for lists of VBLock_Data items
//       : Supports delegated, contiguous or fragment memory allocation

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockList manipulation
void
VBLockList_Init     ( UCHAR uVBLock, VBLockList *pList, UCHAR uVBLockAttr );
VBLaddr
VBLockList_GetFirst ( UCHAR uVBLock, VBLockList *pList, VBLelem *pnItem = nullptr );
VBLockList*
VBLockList_SetFirst ( UCHAR uVBLock, VBLockList *pList, VBLaddr aItemFirst );
VBLaddr
VBLockList_GetLast  ( UCHAR uVBLock, VBLockList *pList, VBLelem *pnItem = nullptr );
VBLockList*
VBLockList_SetLast  ( UCHAR uVBLock, VBLockList *pList, VBLaddr aItemLast );
VBLelem
VBLockList_GetItems ( UCHAR uVBLock, VBLockList *pList );
VBLockList*
VBLockList_SetItems ( UCHAR uVBLock, VBLockList *pList, VBLelem nItems, bool bAbsolute = false );
VBLockField*
VBLockList_pField   ( UCHAR uVBLock, const VBLockList *pList );
VBLockName*
VBLockList_pName    ( UCHAR uVBLock, const VBLockList *pList );
VBLockData*
VBLockList_pData    ( UCHAR uVBLock, const VBLockList *pList );

VBLaddr
VBLockList_Sizeof   ( UCHAR uVBLock );
VBLaddr
VBLockList_Sizenn   ( const VBLockList *pList );

///////////////////////////////////////////////////////////////////////
//  VBLockVect containers and definitions
//  NOTES: Recursive VBLock'ed container for lists of VBLock_Data items
//       : Supports delegated, contiguous or fragment memory allocation

//
//  Static manipulators
//  NOTES: Provide address sensitive VBLockVect manipulation
void
VBLockVect_Init     ( UCHAR uVBLock, VBLockVect *pVect, VBLelem nItems, UCHAR uVBLockAttr );
VBLelem
VBLockVect_GetItems ( UCHAR uVBLock, const VBLockVect *pVect );
VBLockField*
VBLockVect_pField   ( UCHAR uVBLock, const VBLockVect *pVect );
VBLockName*
VBLockVect_pName    ( UCHAR uVBLock, const VBLockVect *pVect );
VBLockData*
VBLockVect_pData    ( UCHAR uVBLock, const VBLockVect *pVect );

VBLsize
VBLockVect_Sizeof   ( UCHAR uVBLock );
VBLsize
VBLockVect_Sizenn   ( const VBLockVect *pVect );

// Element-slot manipulators (aAlloc[] holds up to VBLockVect_MaxInline
// element VBLock addresses inline; aExtra chains a continuation block).
#define VBLockVect_MaxInline 32
VBLockVect*
VBLockVect_SetItems ( UCHAR uVBLock, VBLockVect *pVect, VBLelem nItems );
VBLaddr
VBLockVect_GetAlloc ( UCHAR uVBLock, const VBLockVect *pVect, VBLelem nElem );
void
VBLockVect_SetAlloc ( UCHAR uVBLock, VBLockVect *pVect, VBLelem nElem, VBLaddr aElem );
VBLaddr
VBLockVect_GetExtra ( UCHAR uVBLock, const VBLockVect *pVect );
void
VBLockVect_SetExtra ( UCHAR uVBLock, VBLockVect *pVect, VBLaddr aExtra );

///////////////////////////////////////////////////////////////////////
//  VBLockStack containers and definitions
//  NOTES: Push-Pop VBLock stack container for VBlockVect, Field and
//         List items

///////////////////////////////////////////////////////////////////////
//  VBLockAttr containers and definitions
//  NOTES: Attributes container for VBlockVect, Field and List items

//
//  Static Attributes manipulators
//  NOTES: Provide address sensitive VBLockList manipulation
void
VBLockAttr_Init     ( UCHAR uVBLock, VBLockAttr *pAttr, UCHAR uVBLockAttr, VBLaddr aParent );
VBLaddr
VBLockAttr_GetFirst ( UCHAR uVBLock, VBLockAttr *pAttr, VBLelem *pnItem = 0 );
VBLockAttr*
VBLockAttr_SetFirst ( UCHAR uVBLock, VBLockAttr *pAttr, VBLaddr aItemFirst );
VBLaddr
VBLockAttr_GetLast  ( UCHAR uVBLock, VBLockAttr *pAttr, VBLelem *pnItem = 0 );
VBLockAttr*
VBLockAttr_SetLast  ( UCHAR uVBLock, VBLockAttr *pList, VBLaddr aItemLast );
VBLelem
VBLockAttr_GetItems ( UCHAR uVBLock, VBLockAttr *pAttr );
VBLockAttr*
VBLockAttr_SetItems ( UCHAR uVBLock, VBLockAttr *pAttr, VBLelem nItems, bool bAbsolute = false );
VBLaddr
VBLockAttr_GetParent( UCHAR uVBLock, VBLockAttr *pAttr );
VBLockAttr*
VBLockAttr_SetParent( UCHAR uVBLock, VBLockAttr *pAttr, VBLaddr aAttrParent );

VBLsize
VBLockAttr_Sizeof   ( UCHAR uVBLock );

//
//  Static Descendant manipulators
//  NOTES: Provide address sensitive VBLockList manipulation
void
VBLockDesc_Init     ( UCHAR uVBLock, VBLockDesc *pDesc, UCHAR uVBLockDesc, VBLaddr aParent );
VBLaddr
VBLockDesc_GetFirst ( UCHAR uVBLock, VBLockDesc *pDesc, VBLelem *pnItem = 0 );
VBLockDesc*
VBLockDesc_SetFirst ( UCHAR uVBLock, VBLockDesc *pDesc, VBLaddr aItemFirst );
VBLaddr
VBLockDesc_GetLast  ( UCHAR uVBLock, VBLockDesc *pDesc, VBLelem *pnItem = 0 );
VBLockDesc*
VBLockDesc_SetLast  ( UCHAR uVBLock, VBLockDesc *pList, VBLaddr aItemLast );
VBLelem
VBLockDesc_GetItems ( UCHAR uVBLock, VBLockDesc *pDesc );
VBLockDesc*
VBLockDesc_SetItems ( UCHAR uVBLock, VBLockDesc *pDesc, VBLelem nItems, bool bAbsolute = false );
VBLaddr
VBLockDesc_GetParent( UCHAR uVBLock, VBLockDesc *pDesc );
VBLockDesc*
VBLockDesc_SetParent( UCHAR uVBLock, VBLockDesc *pDesc, VBLaddr aDescParent );

VBLsize
VBLockDesc_Sizeof   ( UCHAR uVBLock );

///////////////////////////////////////////////////////////////////////
//  VBLockItem containers and definitions
//  NOTES: VBLock'ed list container for VBLock(Lists, Fields, etc)
//         items
//       : Supports delegated, contiguous or fragment memory allocation

//
//  Static operators
//  NOTES: Provide address sensitive VBLockItem manipulation
void
VBLockItem_Init    ( UCHAR uVBLock, VBLockItem *pItem
                   , UCHAR uVBLockItem );
VBLsize
VBLockItem_Sizeof  ( UCHAR uVBLock, VBLockItem *pItem );
VBLsize
VBLockItem_Sizeof  ( UCHAR uVBLock );
VBLsize
VBLockItem_Sizenn  ( UCHAR uVBLock, VBLockItem *pItem );
VBLsize
VBLockItem_Sizenn  ( VBLock_ *pVBLock );
VBLsize
VBLockItem_Sizeud  ( VBLock_ *pVBLock );
VBLsize
VBLockItem_Sizeof_ua ( VBLock *pVBLock );
VBLsize
VBLockItem_Sizeof_ut ( VBLock *pVBLock );

VBLaddr
VBLockItem_GetNext ( UCHAR uVBLock, VBLockItem *pVBLockItem, int *pnItem = 0 );
VBLockItem*
VBLockItem_SetNext ( UCHAR uVBLock, VBLockItem *pItem, VBLaddr aItemNext );
VBLaddr
VBLockItem_GetPrev ( UCHAR uVBLock, VBLockItem *pVBLockItem, int *pnItem = 0 );
VBLockItem*
VBLockItem_SetPrev ( UCHAR uVBLock, VBLockItem *pItem, VBLaddr aItemPrev );
VBLaddr
VBLockItem_GetExtra( UCHAR uVBLock, VBLockItem *pVBLockItem );
VBLockItem*
VBLockItem_SetExtra( UCHAR uVBLock, VBLockItem *pItem, VBLaddr aItemExtra );
VBLaddr
VBLockItem_GetDescn( UCHAR uVBLock, VBLockItem *pVBLockItem );
VBLockItem*
VBLockItem_SetDescn( UCHAR uVBLock, VBLockItem *pItem, VBLaddr aItemDescn );
VBLaddr
VBLockItem_GetStack( UCHAR uVBLock, VBLockItem *pVBLockItem );
VBLockItem*
VBLockItem_SetStack( UCHAR uVBLock, VBLockItem *pItem, VBLaddr aItemXtra );
VBLaddr
VBLockItem_GetParent( UCHAR uVBLock, VBLockItem *pVBLockItem );
VBLockItem*
VBLockItem_SetParent( UCHAR uVBLock, VBLockItem *pItem, VBLaddr aItemParent );

void*
VBLockItem_vpu     ( UCHAR uVBLock, VBLockItem *pVBLockItem );

bool
VBLockItem_IsList  ( const VBLockItem *pVBLockItem );
VBLockList*
VBLockItem_pList   ( UCHAR uVBLock, VBLockItem *pVBLockItem );

bool
VBLockItem_IsVect  ( const VBLockItem *pVBLockItem );
VBLockVect*
VBLockItem_pVect   ( UCHAR uVBLock, VBLockItem *pVBLockItem );

bool
VBLockItem_IsField ( const VBLockItem *pVBLockItem );
//  Msgcore_EXT on the three VBLockItem_p* resolvers below: they refuse an
//  unrecognised (i.e. wire-supplied) uItemType by throwing instead of returning
//  NULL, and MsgcoreSuite checks that directly. p2p_fuzzframe reaches them over
//  the real wire path, but only on Windows -- its assert trap is
//  _CrtSetReportHook, so on glibc the harness aborts at the first ASSERT long
//  before it gets here. Exporting is what lets the guard be tested on BOTH
//  platforms rather than only where the fuzzer runs.
Msgcore_EXT VBLockField*
VBLockItem_pField  ( UCHAR uVBLock, VBLockItem *pVBLockItem );

VBLockItem* //was VBLockNode
VBLockItem_Isolated( VBLockItem *pVBLockItem );

Msgcore_EXT VBLockName*
VBLockItem_pName   ( UCHAR uVBLock, VBLockItem *pVBLockItem );

bool
VBLockItem_IsData  ( const VBLockItem *pVBLockItem );
Msgcore_EXT VBLockData*
VBLockItem_pData   ( UCHAR uVBLock, VBLockItem *pVBLockData );
// Owner-bounded form of the above, and a SEPARATE NAME rather than a defaulted
// third parameter on it, because that one is an exported symbol frozen through
// 1.x: a default argument is source-compatible but NOT binary-compatible, and
// changing the decorated name retires an export somebody may already have
// linked. Deliberately not exported itself -- VBLock_pData is its only caller.
// VBLockField_pData does take its owner as a defaulted parameter, which is safe
// there for the one reason that matters here: it is not exported.
VBLockData*
VBLockItem_pDataChk( UCHAR uVBLock, VBLockItem *pVBLockData
                   , const VBLock *pOwner );

///////////////////////////////////////////////////////////////////////
//  VBLock containers and definitions
//  NOTES: Fundamental unit of memory allocation and subsequent 
//         fragmentation.  De-fragmentaion occurs through collation
//         of free'd adjacent VBLock's

void
VBLock_Init   ( VBLock *pVBLock, UCHAR uVBLock, VBLsize nSizenn );
VBLaddr
VBLock_Sizeof_Hdr ( const VBLock *pVBLock );
VBLaddr
VBLock_Sizeof_Hdr ( UCHAR uVBLockAddr );
VBLsize
VBLock_Hdr_u_SizeNN ( const VBLock *pVBLock );
UINT
VBLock_Sizeof_Hdr_ud ( const VBLock *pVBLock );
VBLsize
VBLock_Sizeof_Name ( const VBLock *pVBLock );
void*
VBLock_ud_vpData ( const VBLock *pVBLock );

bool
VBLock_IsAddr  ( const VBLock *pVBLock, UCHAR uVBLock ) noexcept;
bool
VBLock_IsType  ( const VBLock *pVBLock, UCHAR uVBLock ) noexcept;
bool
VBLock_IsFree  ( const VBLock *pVBLock ) noexcept;
bool
VBLock_IsLinked( const VBLock *pVBLock ) noexcept;
bool
VBLock_IsAlloc ( const VBLock *pVBLock ) noexcept;
BOOL
VBLock_IsContainedVBLump ( VBLock *pVBLock, const void *pVBLaddr, VBLaddr nSizeofVBLaddr = 0 );
// The same question asked one step earlier, and answered by throwing rather
// than returning: bounds a lump a VBLock_pXxx accessor is ABOUT to read, so the
// out-of-bounds read never happens. A null pOwner means the caller has no block
// to bound against and the check is skipped.
void
VBLock_ChkContained ( const VBLock *pOwner, const void *pv, VBLsize nSpan
                    , LPCSTR lpszWhere );

bool
VBLock_IsItem ( const VBLock *pVBLock );
VBLockItem*
VBLock_pItem  ( const VBLock *pVBLock );

bool
VBLock_IsList ( const VBLock *pVBLock );
VBLockList*
VBLock_pList  ( const VBLock *pVBLock );

bool
VBLock_IsVect ( const VBLock *pVBLock );
VBLockVect*
VBLock_pVect  ( const VBLock *pVBLock );

bool
VBLock_IsField ( const VBLock *pVBLock );
VBLockField*
VBLock_pField  ( const VBLock *pVBLock );

bool
VBLock_IsAttr ( const VBLock *pVBLock );
VBLockAttr*
VBLock_pAttr  ( VBLock *pVBLock );

bool
VBLock_IsDesc ( const VBLock *pVBLock );
VBLockDesc*
VBLock_pDesc  ( VBLock *pVBLock );

bool
VBLock_IsRoot ( const VBLock *pVBLock );

bool
VBLock_IsStck ( const VBLock *pVBLock );

bool
VBLock_IsName  ( const VBLock *pVBLock ) noexcept;
VBLockName*
VBLock_pName   ( const VBLock *pVBLock );

bool
VBLock_IsData  ( const VBLock *pVBLock );
VBLockData*
VBLock_pData   ( VBLock *pVBLock );


///////////////////////////////////////////////////////////////////////
//  VBLock file access operations
//  NOTES: Fundamental unit of memory allocation and subsequent 
//         fragmentation.  De-fragmentaion occurs through collation
//         of free'd adjacent VBLock's

//UINT
//VBLock_Sizeof ( HANDLE hFile );
bool
VBLock_Read ( HANDLE hFile, VBLock *pVBLock );
bool
VBLock_Write ( HANDLE hFile, VBLock *pVBLock );
