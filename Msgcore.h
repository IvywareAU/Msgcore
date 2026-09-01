// Copyright © 2006-2010, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  Implements Msgcore.DLL exported class, function and variable definitions
//  NOTES: To be included with the definitions for any object exported
//         from the Msgcore.DLL  Follows the MFC_EXT_CLASS pattern
//       : Either the Msgcore project or the StdAfx.h file MUST
//         contain the Msgcore_EXPORTS definition.  For further
//         Developer Studio details refer Project Properties >> Config >>
//         C/C++ Preprocessor
//

#pragma once

//  Version identity. Macro-only, and the same header the VERSIONINFO
//  resource reads, so a consumer's compile-time MSGCORE_VERSION_* can never
//  disagree with the FileVersion the shipped DLL reports.
#include "Msgcore_version.h"

//
//  P2Peer data types
typedef unsigned char  UINT08;
typedef void *         P2PmsgHANDLE;   // P2Pmsg handle
typedef unsigned char  P2Pmsgnn_t;     // P2Pmsg 08, 16, 32, 64 addressing
typedef LPCTSTR        LPCTNAM;        // Field naming convention (const)
typedef       LPTSTR   LPTNAM;         // Field naming convention
typedef       TCHAR    TNAME;          // Field name type
#define MAX_TNAME_SIZE 64              // Field name maximum size

///////////////////////////////////////////////////////////////////////////////
//  P2Pmsg pre-definitions
typedef  DWORD_PTR  P2Pos;
#define  INT08      char
#define  UINT08     UCHAR
#define  TIME32     INT32
#define  TIME64     INT64
#define  VBLaddr    UINT_PTR           // Addressing
#define  VBLaddr16  UINT16
#define  VBLaddr32  UINT32
#define  VBLaddr64  UINT64
#define  VBLsize    UINT_PTR           // Allocation sizes
#define  VBLsize32  UINT32
#define  VBLelem    int                // List elements etc

#define  SIZE08_MAX ((UINT08)~0)
#define  SIZE16_MAX ((UINT16)~0)
#define  SIZE32_MAX ((UINT32)~0)
#define  SIZE64_MAX ((UINT64)~0)

const  INT32  INT32_NULL = ((UINT32)~0);
const UINT32 UINT32_NULL = ((UINT32)~0);
const  INT64  INT64_NULL = ((UINT64)~0);
const UINT64 UINT64_NULL = ((UINT64)~0);

///////////////////////////////////////////////////////////
//  Portable 64-bit-pointer-build detection. The original code keyed address-model
//  and persisted-layout choices off MSVC-only macros (_WIN64 / _M_X64), which are
//  undefined on 64-bit Linux/GCC — silently selecting 32-bit addressing and TRUNCATING
//  native heap pointers stored as VBLaddr. Key off pointer width on every toolchain.
#if defined(_WIN64) || defined(_M_X64) || defined(_M_ARM64) \
 || defined(__LP64__) || defined(_LP64) \
 || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
  #define P2P_PTR64 1
#else
  #define P2P_PTR64 0
#endif

///////////////////////////////////////////////////////////
//  win32 and win64 sensitive definitions
#if defined(_WIN64)
    #define _tcstoUINT_PTR _tcstoui64
#else
    #define _tcstoUINT_PTR _tcstoul
#endif

#if defined(Msgcore_STATIC)
  #define Msgcore_EXT
  #define Msgcore_API
#elif defined(Msgcore_EXPORTS)
  #define Msgcore_EXT __declspec(dllexport)
  #define Msgcore_API __declspec(dllexport)
#else
  #define Msgcore_EXT __declspec(dllimport)
  #define Msgcore_API __declspec(dllimport)
#endif

// This class is exported from the Msgcore.dll
class Msgcore_API CMsgcore {
public:
	CMsgcore(void);
	// TODO: add your methods here.
};

extern Msgcore_API int nMsgcore;

Msgcore_API int fnMsgcore(void);

//
//  Triggers
const UINT TRIGGER_INSERT = 1;
const UINT TRIGGER_UPDATE = 2;
const UINT TRIGGER_DELETE = 4;
const UINT TRIGGER_IUD = (TRIGGER_INSERT | TRIGGER_UPDATE | TRIGGER_DELETE);
const UINT TRIGGER_ACTIVE = 8;
const UINT TRIGGER_ALL = ~0u;

//  Headless trigger sink.
//  NOTES: A process-wide-per-manager callback fired by P2PmsgHeap_ProcTriggers
//         in addition to (and independent of) the HWND PostMessage path, so a
//         windowless host (FUSE/daemon) can receive change notifications for
//         armed nodes without a message pump. posP2Pobject is the P2Pos of the
//         changed object; nTriggerType is one of the TRIGGER_* masks (a single
//         bit per invocation). The signature is kept HWND/LPARAM-free and
//         64-bit-clean so it matches the flat C API's msgcore_trigger_fn.
typedef void (*P2PmsgTriggerSink)( void* pUser
                                 , UINT nTriggerType
                                 , unsigned long long posP2Pobject );

//
//  Msgcore UNICODE definitions
#define _ucscmp  strcmp
#define _ucsncmp strncmp
#define _ucsicmp stricmp
#define _ucslen  strlen

#define _U(arg)  _T(arg)
#define _N(arg)  L##arg

///////////////////////////////////////
//  NULL value management
#define Msgcore_NULL_DBLE -1.7976931348623158e+308
#define MsgcoreIsNULL_DBLE(dValue) (dValue<=Msgcore_NULL_DBLE)
                                       // Psuedo double NULL value

///////////////////////////////////////
//  UNICODE helpers

Msgcore_EXT int
wmemicmp ( const wchar_t *pwSrc, const wchar_t *pwDst, size_t count ) noexcept;
Msgcore_EXT bool
MsgcoreWildcard ( LPCTSTR lpszWildcard, LPCTSTR lpszName ) noexcept;

///////////////////////////////////////////////////////////////////////
//  VBLock container and header definitions
//  NOTES: Fundamental unit of memory allocation and subsequent 
//         fragmentation.  De-fragmentaion occurs through collation
//         of free'd adjacent VBLock's
//
#define VBLock_AddrMask 0x03           // Bits addressing  (Bits 0-1)
#define VBLock_Addr08      0           //    8Bit  addressing
#define VBLock_Addr16      1           //   16Bit  addressing
#define VBLock_Addr32      2           //   32Bit  addressing
#define VBLock_Addr64      3           //   64Bit  addressing
#if P2P_PTR64
#define VBLock_Addrxx VBLock_Addr64    // Default  addressing (64bit builds)
#else
#define VBLock_Addrxx VBLock_Addr32    // Default  addressing (32bit builds)
#endif

///////////////////////////////////////////////////////////////////////
//  IOMAGE synchronisation-word endian sentinel  (byte_order.md §4)
//  NOTES: VBListIOmage::oSync is the pair
//           uiSync1 = size(bits 0-23) | uAddrType(bits 24-25)
//           uiSync2 = ~uiSync1
//         The complement relation ALONE cannot detect a byte-order
//         mismatch: complement is per-bit, byte swap is a bit
//         permutation, and the two commute - so a complement pair
//         stays a complement pair when read by a foreign-endian peer.
//         The swapped size then drove an out-of-bounds block walk that
//         surfaced as heap corruption, far from the real cause.
//       : uAddrType only occupies bits 0-1 of the top byte
//         (VBLock_AddrMask), so bits 2-7 were always zero and never
//         read. The sentinel lives there: sizeof(VBListIOmage) is
//         unchanged and the complement invariant is preserved.
//       : The top byte is the MOST significant, hence the LAST byte in
//         memory little-endian. A big-endian reader lands it in the
//         LEAST significant position - so a swapped image carries the
//         sentinel in its LOW byte, which is what makes the mismatch
//         diagnosable rather than merely fatal.
#define VBLock_SyncMask 0xFC           // Endian sentinel  (Bits 2-7 of the oSync top byte)

///////////////////////////////////////////////////////////////////////
//  LAYOUT GENERATION  (TargetCore's versioning note, §6, gate 2)
//  NOTES: That note is NAMED rather than linked: it lives in the sibling
//         TargetCore repository, so a path from here would not resolve.
//       : These six bits are also the message image's ONLY version story,
//         and they are its only one because nothing else in the header has
//         room: bits 0-23 are the size and bits 24-25 the addressing mode,
//         so a dedicated version field would have to move the very layout
//         it exists to describe.
//       : A generation code names the BODY layout that follows this header.
//         Change anything inside a message - align a payload, reorder a
//         field, widen a length - and neither the size nor the addressing
//         mode moves, so a peer reads a plausible header and parses garbage.
//         Stamping a new generation is what turns that into a refusal, and
//         it costs the format no bytes because these bits are already spent.
//       : ONE code is defined, and it is the one this build writes. Every
//         other non-zero pattern classifies as VBLockSync_Gen - "a layout
//         this build does not implement" - and is refused with its code in
//         the diagnostic. That is a FALLBACK, not a registry, and the
//         difference is what it costs. An enumerated registry would name
//         generation 2 and ONLY generation 2, would say nothing about
//         generation 3, and would spend 1/64 of the byte-order diagnosis in
//         byte_order.md §4.4 on every code it enumerated. The fallback names
//         every future generation and spends nothing.
//       : What the fallback gives up is telling a layout nobody defined from
//         one somebody will: crafted bytes report as an unimplemented
//         generation rather than as corruption. Both are refusals, and
//         neither is reachable by accident - the complement pair in
//         P2PmsgHeap_IOMAGEform is the structural filter and it runs FIRST,
//         so a word reaches this classifier only by already being a valid
//         pair (2^-32 for random bytes).
#define VBLock_SyncGen1 0xA4           // Generation 1: the layout as shipped
#define VBLock_SyncGenNow VBLock_SyncGen1  // The generation this build stamps
#define VBLock_SyncBits VBLock_SyncGenNow  // Historical spelling of the above,
                                       // kept because byte_order.md §4 names
                                       // it (top byte = 0xA4..0xA7)

//  Does uBits - ALREADY masked with VBLock_SyncMask - name a layout this build
//  implements? Exactly one code does. This is a function rather than an
//  equality written inline because it is also the test the SWAP arm applies to
//  the LOW byte: what counts as "a code we would have written" is the whole of
//  what a byte-order mismatch is detected BY.
inline int
VBLock_SyncIsGen ( UINT32 uBits )
{
    return uBits == VBLock_SyncGenNow;
}

//  The raw generation code out of an oSync word, for diagnostics. A CODE and
//  not an ordinal - generation 1 is 0xA4 - so report it in hex and do no
//  arithmetic on it.
inline UINT08
VBLock_SyncGenCode ( UINT32 uiSync1 )
{
    return (UINT08)((uiSync1 >> 24) & VBLock_SyncMask);
}

//  Classification of an oSync word. Test order is deliberate:
//  Native -> Legacy -> Swapped -> Gen.
//  Legacy precedes Swapped so a valid PRE-sentinel image is never misdiagnosed
//  as byte-swapped (that would reject good stored data). Swapped precedes Gen
//  because Gen is the FALLBACK: put it any earlier and it swallows every
//  byte-swapped image, since a swapped word's top byte is the SIZE's low byte
//  and is non-zero far more often than not. See byte_order.md §4.2/§4.4.
//  VBLockSync_Invalid is not returned here and cannot be - every pattern is
//  classified. It belongs to P2PmsgHeap_IOMAGEform, which fails the complement
//  pair before this is ever reached. One meaning, one place.
#define VBLockSync_Invalid 0           // Complement pair failed - not an image
#define VBLockSync_Native  1           // Current build, same endianness
#define VBLockSync_Legacy  2           // Pre-sentinel image, same endianness
#define VBLockSync_Swapped 3           // Sentinel in the low byte -> foreign endianness
#define VBLockSync_Gen     4           // A layout generation this build does not
                                       // implement -> refused, code included

inline int
VBLock_SyncForm ( UINT32 uiSync1 )
{
    const UINT32 uHi = (uiSync1 >> 24) & VBLock_SyncMask;
    if ( uHi == VBLock_SyncGenNow )
      return VBLockSync_Native;
    if ( uHi == 0 )
      return VBLockSync_Legacy;
    if ( VBLock_SyncIsGen ( uiSync1 & VBLock_SyncMask ) )
      return VBLockSync_Swapped;
    return VBLockSync_Gen;
}

//  Addressing mode out of an oSync word, sentinel bits removed.
inline UINT08
VBLock_SyncAddr ( UINT32 uiSync1 )
{
    return (UINT08)((uiSync1 >> 24) & VBLock_AddrMask);
}

//  Stamp size + addressing mode + sentinel into an oSync word.
inline UINT32
VBLock_SyncMake ( UINT32 nSizeof, UINT08 uAddrType )
{
    return ( nSizeof & 0x00FFFFFF )
         | ( (UINT32)( VBLock_SyncGenNow | ( uAddrType & VBLock_AddrMask ) ) << 24 );
}

#define VBLock_TypeMask 0x3C           // Types            (Bits 2-5)
#define VBLock_Item        4           //   Item
#define VBLock_Heap        8           //   Heap
#define VBLock_Root       12           //   Root
#define VBLock_BSTR       16           //   BSTR
#define VBLock_Field      20           //   Field
#define VBLock_Node       24           //   Node
#define VBLock_Blob       28           //   Blob
#define VBLock_Data       32           //   Data
#define VBLock_Name       36           //   Name
#define VBLock_Stack      40           //   Stack
#define VBLock_List       44           //   List
#define VBLock_Vect       48           //   Vector
#define VBLock_Attr       52           //   Attributes
#define VBLock_Desc       56           //   Descendants
#define VBlock_Spare3     60           //   Spare2

#define VBLock_HeapMask 0xC0           // Heap Status      (Bits 6-7)
#define VBLock_Alloc      64           //   Allocated      (Bit  6) 256
#define VBLock_Linked    128           //   Linked         (Bit  7) 512

#define VBLock_Reserved 0xFC00         // Reserved         (Bits 8-15)

#pragma pack(push,1)
typedef struct VBLockHdr__             // Mandatory VBLock header
{
    UINT08  uVBLockDefs;               // Refer VBLock definitions above
    union
    {
      UINT08     nSize08;
      UINT16     nSize16;
      UINT32     nSize32;
      UINT64     nSize64;
    } u;                               // Absolute size of block
} VBLockHdr;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct VBLockk__
{
    VBLockHdr   oHdr;                  // Universal VBLock header
                                       // Content beyond here
} xVBLockk;
#pragma pack(pop)

///////////////////////////////////////////////////////////
//  Debugging

#define P2PASSERT(a) VERIFY(a)