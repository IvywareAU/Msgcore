// Copyright © 2005-2011, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2Pmsg Virtual-Blocked-Heap utitities
//  NOTES: Manage the VBHeap data structures
//
//
//  Virtual Block Heap (VBHeap) management utilities for the P2P messaging
//  framework.
//  NOTES: This module provides creation, allocation, addressing, and
//         lifecycle management for VBLock-based heaps. It operates as
//         the allocator and ownership layer for all VBLock structures
//         defined in P2PmsgVBLock.h.
//       : A VBHeap maintains a collection of VBLock blocks organised into
//         allocated and free chains, supporting efficient reuse, fragmentation
//         control, and relocation-safe memory handling.
//
//  Architecture Overview
//       : VBHeap (opaque handle) manages a contiguous or image-backed memory
//         region
//       : VBListIOmage / VBListBSTRio define heap root/control structures
//       : VBLock blocks are allocated, linked, freed, and coalesced within
//         the heap
//
//  Heap types:
//       : IOMAGE  : General-purpose heap image (persistable / memory-mapped)
//       : SYS     : System-managed heap (non-persistable, in-memory only)
//       : BSTRio  : Heap with extended BSTR and trigger support
//
//  Key Responsibilities
//       : Heap creation and destruction (Create*, Close, AddRef)
//       : Allocation and deallocation of VBLock blocks (Alloc, Free)
//       : Address translation (VBLaddr ⇄ physical pointer)
//       : Heap sizing, growth, and capacity tracking
//       : Maintenance of free/allocated block chains
//       : Dirty state tracking for persistence control
//
//  Memory Model
//       : All allocations are VBLock-aligned and VBLockHdr-prefixed
//       : Addressing is offset-based (VBLaddr), independent of physical
//         location
//       : Supports multiple addressing modes (8/16/32/64-bit)
//       : Free blocks are coalesced to reduce fragmentation
//       : Heap image may be persisted or reconstructed in-place
//
//  Advanced Features
//       : BSTRio heaps provide extended string/blob management
//       : Trigger mechanism enables event-style notifications tied to VBLock items
//       : Direct access to underlying heap image for serialization or IPC
//
//  Diagnostics & Validation
//       : Heap and block integrity validation (AssertValid, AssertVBlocks)
//       : Allocation consistency checks
//       : Debug support for detecting corruption and containment issues
//
//  SUMMARY
//       : This module underpins all VBLock allocation and must remain consistent
//         with P2PmsgVBLock memory layout definitions
//       : Callers must treat VBLaddr values as opaque offsets, not pointers
//       : Trigger functionality is only available for BSTRio heap types
//       : Designed for high-performance, low-copy message construction
//
#pragma   once
#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif
#include "P2PmsgVBLock.h"
#include "P2PmsgBSTR.h"

///////////////////////////////////////////////////////////////////////////////
//  VBListBSTRio containers and definitions
//  NOTES: Private root for VBListBSTRio structure
//       : Dynamic size dependant upon addressing mode
#pragma pack(push,1)
typedef struct
{
    struct
    {
      UINT32    uDefs1;                // Definitions
      UINT32    uComp2;                // Compliment of above
    } oDefs;
    struct
    {
      VBLsize32 aSize1;                // Allocated size
      VBLsize32 aComp2;                // Compliment of above
    } oSize;
    struct
    {
      VBLaddr32 aAlloc;                // Address of first allocated VBLock
      VBLaddr32 aAllocSize;            // Cummulative size of all allocated VBLock's
      VBLaddr32 aFree;                 // Address of first free VBLock
      VBLaddr32 aFreeLast;             // Address of last  free VBLock
      VBLaddr32 aFreeSize;             // Cummulative size of all free VBLock's
      VBLelem   nAllocEntries;         // Number of allocated entries
      VBLelem   nFreeEntries;          // Number of free entries
      VBLaddr32 aSpare8;
    } oKeys;
    char cTag;
} VBListBSTRio;
#pragma pack(pop)

///////////////////////////////////////////////////////////////////////
//  P2PmsgHeap creation and life cycle management

P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE ( UCHAR uAddrType, VBLsize nSizeInitial, VBLsize nSizeMax );
P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE ( VBListIOmage *pIOmage );
// Length-validated entry point for untrusted images (file/received), where the
// caller knows the real buffer size. Rejects a declared size larger than the
// buffer before any block walk. Prefer this over the single-arg overload for
// externally-sourced images.
P2PmsgHANDLE
P2PmsgHeap_CreateIOMAGE ( VBListIOmage *pIOmage, VBLsize nBufferLen );
P2PmsgHANDLE
P2PmsgHeap_CreateSYS    ( UCHAR uAddrType, VBLsize nSizeMax );
P2PmsgHANDLE
P2PmsgHeap_CreateBSTRio ( UCHAR uAddrType, VBLsize nSizeInitial, VBLsize nSizeMax );
P2PmsgHANDLE
P2PmsgHeap_CreateBSTRio ( VBListBSTRio *pBSTRio );
// Length-validated entry point for untrusted images (file/received), the twin of
// the IOMAGE overload above and for the same reason. The complement checksums
// P2PmsgHeap_IsBSTRio tests are trivially forgeable, so a crafted oSize.aSize1
// declared LARGER than the real buffer would otherwise be copied into
// VBListHANDLE::nSizeofAlloc -- which is the bound every later
// P2PmsgHeap_Addr2Phys translation is checked against. A forged declared size
// therefore did not merely go unnoticed, it RAISED the ceiling on every
// subsequent offset in the image. Reject it here, before the handle exists.
// Prefer this over the single-arg overload for externally-sourced images.
P2PmsgHANDLE
P2PmsgHeap_CreateBSTRio ( VBListBSTRio *pBSTRio, VBLsize nBufferLen );
P2PmsgHANDLE
P2PmsgHeap_AddRef ( P2PmsgHANDLE hVBHeap );
BOOL
P2PmsgHeap_Close  ( P2PmsgHANDLE hVBHeap );
//  How many holders this heap has -- the count AddRef raises and Close lowers.
//  ONE means the asker is the only holder, and so that nothing else in the
//  process can be looking at anything on this heap.  It counts holders of the
//  HEAP and not names for a block: >1 leaves the question open rather than
//  answering it the other way.  0 for a null handle.
int
P2PmsgHeap_RefCount ( P2PmsgHANDLE hVBHeap ) noexcept;
void
P2PmsgHeap_InitBSTRio ( VBListBSTRio *pBSTRio, VBLsize nSizeofBSTRio, UCHAR uAddrType );
VBLaddr
P2PmsgHeap_AllocSeqnum ( P2PmsgHANDLE hVBHeap );

///////////////////////////////////////
//  P2PmsgHeap allocations and addressing
VBLaddr
P2PmsgHeap_Alloc  ( P2PmsgHANDLE hVBHeap, UCHAR uVBLock, VBLsize aSizeof );
VBLaddr
P2PmsgHeap_Free   ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLaddr );
UCHAR
P2PmsgHeap_Addrnn ( P2PmsgHANDLE hVBHeap );
VBLsize
P2PmsgHeap_Sizeof ( P2PmsgHANDLE hVBHeap );
//  Free-block boundary tags are an in-memory accelerator and must not reach a
//  serialised image; Save scrubs them, writes, and restores them. Refer the
//  note on P2PmsgHeap_ScrubFoots in MsgVBHeap.cpp.
void
P2PmsgHeap_ScrubFoots ( P2PmsgHANDLE hVBHeap, bool bRestore ) noexcept;
VBLsize
P2PmsgHeap_Sizeof ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock );
// NO POINTER RETURNED BELOW SURVIVES A MUTATION, and that is a LIFETIME rule,
// not the bounds rule the span note further down is about. The heap addresses
// its blocks by offset and grows by reallocating its base image, so any call
// that can allocate - a declare, a rename, a retype, a data write, a load -
// delete[]s the image these point into and invalidates every one of them.
// Re-resolve per operation; treat "obtain, use, discard" as the unit of work.
//
// This is SECURITY_REVIEW.md H5, and it is stated here because it was not.
// The flat C surface has had it since it was written - numbered rule 2 of the
// Msgcore_c.h preamble, restated at each accessor group, and it goes so far as
// to refuse a positional list walk and pay O(n^2) rather than hand a VBLaddr
// across the ABI. The C++ declarations said nothing, and the careful span
// commentary below made that worse rather than better: a reader just warned in
// detail about one hazard does not go looking for a second.
VBListIOmage*
P2PmsgHeap_pIOmage( P2PmsgHANDLE hVBHeap );
void*
P2PmsgHeap_pImage ( P2PmsgHANDLE hVBHeap );
void*
P2PmsgHeap_Addr2Phys ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLaddr );
// Span-checked translation. Addr2Phys bounds the START of a block against the
// heap; this additionally requires nSpan bytes from that offset to lie inside
// it, which is what a caller about to READ a structure at the returned pointer
// actually needs. The difference matters at the top of the image: an offset one
// byte below the limit passes Addr2Phys and then reads a multi-byte block header
// off the end. Use this wherever the offset came out of the image rather than
// out of an allocation this process made.
void*
P2PmsgHeap_Addr2PhysChk ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLaddr, VBLsize nSpan );
// The same bound stated over a pointer rather than an offset, for the pointers
// VBLock_pData and its siblings compute from a block's own header: there is no
// offset to check at the door there, only a result that may already have left
// the image. Answers rather than throws, because a block held inline in
// P3PmsgObject::m_oVBLock is legitimately outside one. TRUE for SYSTEM heaps.
BOOL
P2PmsgHeap_IsPhysSpan ( P2PmsgHANDLE hVBHeap, const void *pv, VBLsize nSpan ) noexcept;
VBLock*
P2PmsgHeap_Block2Phys( P2PmsgHANDLE hVBHeap, VBLaddr aVBLaddrBlock );
VBLsize
P2PmsgHeap_Sizeof_Hdr ( P2PmsgHANDLE hVBHeap ) noexcept;

///////////////////////////////////////
//  Properties and state
UCHAR
P2PmsgHeap_AddnnBSTRio ( const VBListBSTRio *pBSTRio );
BOOL
P2PmsgHeap_IsIOMAGE ( const void *vpVBHeap );
//  Endian-aware classification of an oSync header. Returns one of
//  VBLockSync_Invalid / _Native / _Legacy / _Swapped / _Gen (byte_order.md
//  §4.2). _Invalid is raised HERE and only here - it means the complement pair
//  failed, i.e. this is not an image at all; VBLock_SyncForm classifies every
//  pattern that gets past that gate.
//  IsIOMAGE() accepts _Native and _Legacy only; use this where a foreign-endian
//  image or a layout this build does not implement needs to be reported as what
//  it is rather than as corruption.
int
P2PmsgHeap_IOMAGEform ( const void *vpVBHeap );
BOOL
P2PmsgHeap_IsBSTRio ( const void *vpVBHeap );
BOOL
P2PmsgHeap_IsRoot ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock );
BOOL
P2PmsgHeap_IsDirty ( P2PmsgHANDLE hVBHeap );
BOOL
P2PmsgHeap_SetDirty ( P2PmsgHANDLE hVBHeap, BOOL bDirty );
VBLaddr
P2PmsgHeap_Connect ( P2PmsgHANDLE hVBHeap );
VBLaddr
P2PmsgHeap_ConnectBSTRio ( P2PmsgHANDLE hVBHeap );

///////////////////////////////////////
//  Trouble shooting utilities
//
//  THE UNTRUSTED GATE.  The four walks below do two different jobs, and until
//  2026-08-21 they did both of them the same way.
//
//  Job one is a DEVELOPER AID.  A heap this process just built and then walked
//  over is ours; an invariant that does not hold in it is a bug in this code,
//  and ASSERT is the right response - stop here, in a debug build, with the
//  line number of the invariant that broke.
//
//  Job two is a GATE.  The same walks are the acceptance test for an image
//  that arrived off a socket or a disk, and there an invariant that does not
//  hold is not a bug at all: it is a stranger saying something untrue, which
//  is the ordinary case and the whole reason the walk is being run.  Asserting
//  on it is wrong twice over - it fires a developer breakpoint on remote input
//  (89,896 times over one 3,618-frame fuzz run, Stage 1 step 4), and it says nothing at all in a Release build, where ASSERT is
//  gone and the walk therefore had no opinion to give.
//
//  P2PmsgHeap_UntrustedGate is the RAII scope that says which job is running.
//  Inside one, a violated invariant makes the walk RETURN FALSE and raises no
//  assertion; outside one, every check behaves exactly as it did before.  The
//  checks themselves are the same checks - what changes is who is being
//  accused.  The length-validated Create overloads enter the scope for their
//  own walks, so callers holding an untrusted image get the gate by using the
//  entry point that already exists for them.
//
//  Nesting is counted rather than flagged: the IOMAGE walk calls the free-list
//  walk, and a gate around the outer one has to cover the inner one too.
//  Thread-local, because the receive path runs one per IOCP worker.
Msgcore_EXT bool
P2PmsgHeap_InUntrustedGate ( );
class Msgcore_EXT P2PmsgHeap_UntrustedGate
{
    public:
        P2PmsgHeap_UntrustedGate ( );
       ~P2PmsgHeap_UntrustedGate ( );
    private:
        P2PmsgHeap_UntrustedGate ( const P2PmsgHeap_UntrustedGate& );
        P2PmsgHeap_UntrustedGate& operator= ( const P2PmsgHeap_UntrustedGate& );
};

bool
P2PmsgHeap_AssertValid ( P2PmsgHANDLE hVBHeap );
bool
P2PmsgHeap_AssertValidIOMAGE ( P2PmsgHANDLE hVBHeap );
bool
P2PmsgHeap_AssertVBlocks ( P2PmsgHANDLE hVBHeap );
bool
P2PmsgHeap_AssertValidAlloc ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock );
//  THE PURE ONE, and the reason there are two.  P2PmsgHeap_AssertValidAlloc is
//  a developer aid that REPAIRS: where the block it is judging is not Linked it
//  sets the bit and carries on (MsgVBHeap.cpp, the SYS and BSTRio arms), which
//  makes it useless to a caller that wants to refuse on the answer -- promoting
//  such a call out of an ASSERT(...) would start running that write in Release,
//  where the ASSERT had been keeping it out.  Debug and Release leave different
//  bytes in the block for exactly that reason, and that is a defect rather than
//  a design.
//
//  P2PmsgHeap_IsValidAlloc asks the same question with no write on any path and
//  no assertion on any path.  Use it wherever the answer is going to decide
//  something -- item 19's own prescription, a named refusal in every build.
//  Use the Assert form only where the heap is this process's own and a broken
//  invariant is this code's bug.
bool
P2PmsgHeap_IsValidAlloc ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock );

///////////////////////////////////////////////////////////////////////
//  Triggers
//  NOTES: Facility only available for P2PmsgBSTRio type heap
UINT
P2PmsgHeap_CreateTrigger ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock, HWND hWnd
                         , UINT uiWM_APP_Trigger, UINT nTypeTrigger, LPARAM lParam );
UINT
P2PmsgHeap_DropTrigger   ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock
                         , HWND hWnd, UINT nTriggerTypeMask );
UINT
P2PmsgHeap_DropTriggers  ( P2PmsgHANDLE hVBHeap
                         , HWND hWnd, UINT nTriggerTypeMask );
UINT
P2PmsgHeap_ProcTriggers  ( P2PmsgHANDLE hVBHeap, VBLaddr aVBLock, UINT nMaskTriggers );
//  Install (or clear, with pfn == nullptr) the headless trigger sink invoked by
//  P2PmsgHeap_ProcTriggers for every fired registration. Runtime-only handle
//  state; not persisted.
void
P2PmsgHeap_SetTriggerSink ( P2PmsgHANDLE hVBHeap, P2PmsgTriggerSink pfn, void* pUser );
