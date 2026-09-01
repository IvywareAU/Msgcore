// Copyright © 2026 Khrustal & Mann
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
//  fuzz_recv_image.cpp -- libFuzzer harness over the RECEIVE path: building a
//  heap from an in-memory image and then traversing it.
//
//  WHY THIS EXISTS, and it is the uncomfortable half of the F4-F7 record.
//  Four of this repository's seven memory-safety findings -- F4, F5, F6 and F7
//  -- are on this path, and every one of them was found by `p2p_fuzzframe`,
//  which lives in TargetCore. It is not in this repository and this
//  repository's CI cannot run it. `p2p_fuzzframe` appears here only as comments
//  in P2Pmsg.cpp and MsgVBHeap.cpp naming the replay that found each defect.
//  So until this file existed, the path that produced the MAJORITY of Msgcore's
//  known memory unsafety had no harness of its own, and criterion 2's "two
//  hours cumulative clean" meant two hours of the other path. That is the F3
//  lesson -- a green fuzzer is evidence about where the fuzzer goes -- recurring
//  at the level of the harness inventory rather than the dispatch arm.
//
//  WHAT THIS DRIVES THAT fuzz_iomage_load DOES NOT. That harness calls
//  P2PmsgMgr::Load, which reads a file, dispatches on the tag, constructs the
//  heap and calls Connect. It stops there. The receive path keeps going: having
//  built a heap over bytes a peer sent, it RESOLVES ADDRESSES OUT OF THOSE BYTES
//  and reads through them. That traversal is the whole of F4-F7:
//
//      F4  P2PmsgObject_pData -> VBLockData_IsChained   an unbounded derived
//                                                       pointer, dereferenced
//      F5  the fixed-position structure, bounded by the addressing width
//      F6  the derivation itself, because by the time a pointer comes back
//          the read has already happened
//      F7  the width, against the other place the image declares it
//
//  None of the four is inside an ASSERT. Every one was live in a shipped binary
//  and simply never observed there, which is why this harness builds ReleaseLib
//  like its sibling: that is the configuration a consumer links.
//
//  SAME CORPUS FORMAT AS fuzz_iomage_load, DELIBERATELY. The input is a raw
//  image and nothing else -- no length prefix, no steering block, no framing of
//  this harness's invention. Seeds are therefore interchangeable between the two
//  harnesses and either one's corpus is a legitimate starting point for the
//  other. The alternative -- reserving trailing bytes to steer traversal -- would
//  have made every seed mean something different to each harness and split a
//  shared corpus in half for no gain.
//
//  HOW TRAVERSAL IS STEERED, given that. Out of the image's own bytes. A
//  receiver resolves addresses that arrived inside the frame, so reading the
//  address stream back out of the image is what the real caller does, not a
//  convenience -- and it keeps the fuzzer's coverage feedback pointed at the
//  bytes it is already mutating.
//
//  A NOTE ON WHICH ENTRY POINTS ARE FAIR GAME. Only the ones that are supposed
//  to bound their input. The length-validated overloads
//  (P2PmsgHeap_CreateIOMAGE/CreateBSTRio with nBufferLen, Msgiomage_Duplicate
//  with nBufferLen) exist precisely because an image declares its own extent and
//  the declaration is forgeable; P2PmsgHeap_Addr2PhysChk span-checks; Connectx
//  span-checks since F1. Feeding those a hostile image is testing them at their
//  contract. The single-argument overloads next to them CANNOT check -- a
//  reference carries no length -- and are documented as unsafe for
//  externally-sourced images (see the M4 residual). Crashing them would prove
//  only that the documentation is right, so this harness does not call them.
//
//  OWNERSHIP, which is easy to get wrong and reports false crashes when you do.
//  P2PmsgMgr::Load allocates the image with `new char[]`, and on success the
//  heap takes ownership -- P2PmsgHeap_Close does the `delete[]`. On a throw the
//  caller still owns it. This harness mirrors that contract exactly, because an
//  allocator mismatch or a double-free here would be reported by ASan as a
//  finding in Msgcore when it was a defect in the harness.
//
//  WHAT COUNTS AS A FINDING. The same rule as the sibling harness. Not a
//  refusal: rejecting a malformed image is correct, and most inputs are
//  rejected. Not a thrown P2Pevent*, which is the library's structured refusal.
//  A finding is a crash, an ASan report, or a hang -- memory unsafety reached
//  through a path that was supposed to have validated its way out first.
//
#include <afx.h>
#include <afxwin.h>
#include "P2PmsgMgr.h"
#include "MsgVBHeap.h"
#include "P2Pmsg.h"
#include "P2PmsgVBLock.h"
#include "P2PmsgBSTR.h"
#include "Msgexception.h"
#include <cstdint>
#include <cstring>

CWinApp theApp;   // MFC runtime anchor, as in fuzz_iomage_load.cpp

namespace {

// Bound the traversal rather than following the image wherever it points. A
// crafted image can describe a cycle, and an unbounded walk turns that into a
// libFuzzer timeout -- which is reported as a hang and triaged as a finding,
// costing a session to discover it was the harness looping and not the parser.
// Cycles in the block graph are a real defect class, but they belong to the
// structural validators below, which detect them and refuse; they are not this
// walk's job to discover by hanging.
const int kMaxProbes = 64;

// Read a 32-bit address out of the image at a byte offset, host order, without
// reading past the end. Host order because that is how the library reads its
// own addresses -- byte_order.md is about the sync header, not about the
// addresses inside an image already accepted as native-endian.
inline bool AddrAt ( const uint8_t *data, size_t size, size_t off, VBLaddr *out )
{
    if ( off + sizeof(UINT32) > size ) return false;
    UINT32 v = 0;
    std::memcpy ( &v, data + off, sizeof(v) );
    *out = static_cast<VBLaddr>(v);
    return true;
}

// Everything below can throw the library's structured refusal, and a refusal is
// the expected outcome for most inputs. Swallowing it per-probe rather than
// per-input matters: one address being refused says nothing about the next one,
// and aborting the walk at the first refusal would leave most of the traversal
// unfuzzed on almost every input.
template <typename F>
inline void Probe ( F fn )
{
    try                       { fn ( ); }
    catch ( P2Pevent *e )     { if ( e ) e->Cancel ( false ); }
    catch ( CException *e )   { if ( e ) e->Delete ( ); }
}

// The traversal proper: the part Load does not do and RecvP2PeerMsg does.
void WalkHeap ( P2PmsgHANDLE hHeap, const uint8_t *data, size_t size )
{
    // 1. The structural validators. F7 and the root fixups live at the end of
    //    AssertValidIOMAGE; F2 and F3 were both inside the block-chain walk
    //    these drive. They are `Assert`-NAMED but they are ordinary functions
    //    that run in Release -- item 19's point, and the reason calling them
    //    from a ReleaseLib harness is meaningful rather than a no-op.
    Probe ( [&] { P2PmsgHeap_AssertValidIOMAGE ( hHeap ); } );
    Probe ( [&] { P2PmsgHeap_AssertVBlocks     ( hHeap ); } );
    Probe ( [&] { P2PmsgHeap_AssertValid       ( hHeap ); } );

    // 2. The heap's own idea of its root and extent. Sizeof and Connect read
    //    fields the image declared, so they are wire-controlled too.
    VBLaddr aRoot = 0;
    Probe ( [&] { aRoot = P2PmsgHeap_Connect ( hHeap ); } );
    Probe ( [&] { P2PmsgHeap_Sizeof     ( hHeap ); } );
    Probe ( [&] { P2PmsgHeap_Sizeof_Hdr ( hHeap ); } );

    // 3. Address translation over addresses that came out of the image. This is
    //    the bound F4-F7 are all about: an offset read from the wire, turned
    //    into a pointer. Addr2PhysChk is the span-checking form and is the one
    //    a receiver should use; both are driven because the unchecked form is
    //    still expected to reject an address outside the arena.
    for ( int i = 0; i < kMaxProbes; ++i )
    {
        VBLaddr a = 0;
        if ( !AddrAt ( data, size, static_cast<size_t>(i) * sizeof(UINT32), &a ) )
            break;

        Probe ( [&] { P2PmsgHeap_Addr2PhysChk ( hHeap, a, sizeof(VBLock) ); } );
        Probe ( [&] { P2PmsgHeap_Addr2Phys    ( hHeap, a ); } );
        Probe ( [&] { P2PmsgHeap_IsRoot       ( hHeap, a ); } );
        Probe ( [&] { P2PmsgHeap_Sizeof       ( hHeap, a ); } );

        // 4. The object layer, which is where F4 actually bit:
        //    P2PmsgObject_pData hands its result straight to
        //    VBLockData_IsChained, which dereferences it. Connectx span-checks
        //    since F1, so connecting a wire address through it is testing the
        //    guard rather than walking around it.
        Probe ( [&] {
            P3PmsgObject oObject;
            oObject.Connectx ( hHeap, a, sizeof(VBLock) );
            if ( oObject )
            {
                VBLockData *pData = P2PmsgObject_pData ( oObject );
                if ( pData )
                {
                    VBLockData_IsChained ( pData );
                    VBLockData_IsBlob    ( pData );
                    VBLockData_IsBSTR    ( pData );
                    VBLockData_IsWSTR    ( pData );
                }
            }
        } );
    }

    // 4b. MUTATION, which is where the rest of the unchecked translations live.
    //     Traversal alone reaches the accessors; it never reaches the allocator.
    //     P2PmsgHeap_Alloc/Free walk and rewrite the free list, and the free
    //     list of an image-backed heap came off the wire like everything else --
    //     so P2PmsgHeap_ResizeIOMAGE/ResizeBSTRio, P2PmsgHeapIO_Split and
    //     P2PmsgHeap_CollateIOMAGE/CollateBSTRio all translate a wire-derived
    //     link with the unchecked Addr2Phys, and four of those translations are
    //     followed by a WRITE rather than a read. F8 was that pattern on a read.
    //
    //     This is the reason a receiver mutates at all: it does not just read a
    //     frame, it builds from one. Sizes are drawn from the image so the
    //     fuzzer can steer them, and bounded to something a 2 KB arena can
    //     plausibly serve -- an allocation that simply fails is not interesting
    //     and spends the run on rejection instead of on the free-list paths.
    //     GROWTH IS CAPPED, and the cap is load-bearing rather than tidiness.
    //     An image declaring Addr32 gets an nSizeofMax near 4 GB, and the
    //     allocator grows an arena by 20% of itself when it needs room -- so a
    //     handful of allocations against a large declared arena walk the RSS up
    //     in hundreds of megabytes a step. Uncapped, this harness hit
    //     libFuzzer's 4 GB rss_limit and exited 71 with no artifact, which
    //     presents as a finding and is not one. Bound the arena before asking
    //     for more of it, and keep the requests small: the free-list paths this
    //     exists to reach are walked by ANY alloc/free pair, not by a large one.
    const VBLsize kArenaCeiling = 1u << 20;      // 1 MB
    for ( int i = 0; i < 8; ++i )
    {
        VBLsize nArena = 0;
        Probe ( [&] { nArena = P2PmsgHeap_Sizeof ( hHeap ); } );
        if ( nArena == 0 || nArena > kArenaCeiling )
            break;

        VBLaddr aSize = 0;
        if ( !AddrAt ( data, size, static_cast<size_t>(i) * 8, &aSize ) )
            break;
        const VBLsize nAlloc = static_cast<VBLsize>( aSize % 64 ) + 1;

        VBLaddr aNew = 0;
        Probe ( [&] { aNew = P2PmsgHeap_Alloc ( hHeap, VBLock_Item, nAlloc ); } );
        if ( aNew )
        {
            //  Size it through the guarded path, then hand it back. Free is the
            //  half that collates, which is the half with the writes.
            Probe ( [&] { P2PmsgHeap_Sizeof ( hHeap, aNew ); } );
            Probe ( [&] { P2PmsgHeap_Free   ( hHeap, aNew ); } );
        }
    }
    Probe ( [&] { P2PmsgHeap_AssertValid ( hHeap ); } );

    // 5. The root itself, if the heap yielded one, through the same accessors.
    //    Kept separate from the loop above because this address is the library's
    //    own answer rather than the fuzzer's, and a defect reachable from it is
    //    reachable without the image naming a single address.
    if ( aRoot )
    {
        Probe ( [&] { P2PmsgHeap_Addr2PhysChk ( hHeap, aRoot, sizeof(VBLock) ); } );
        Probe ( [&] {
            P3PmsgObject oRoot;
            oRoot.Connectx ( hHeap, aRoot, sizeof(VBLock) );
            if ( oRoot )
            {
                VBLockData *pData = P2PmsgObject_pData ( oRoot );
                if ( pData ) VBLockData_IsChained ( pData );
            }
        } );
    }
}

}   // namespace

extern "C" int LLVMFuzzerTestOneInput ( const uint8_t *data, size_t size )
{
    // P2PmsgMgr::Load refuses anything under sizeof(VBListBSTRio) before it
    // looks at the tag, so an input below that floor cannot reach the dispatch
    // in a real caller either. Match it rather than inventing a floor.
    if ( size < sizeof(VBListBSTRio) ) return 0;

    // Mirror Load's allocator exactly -- see the ownership note in the header
    // comment. The buffer is `new char[]` because P2PmsgHeap_Close will
    // `delete[]` it once the heap owns it.
    char *pImage = nullptr;
    try { pImage = new char[size]; }
    catch ( ... ) { return 0; }
    std::memcpy ( pImage, data, size );

    P2PmsgHANDLE hHeap = nullptr;

    try {
        // The tag dispatch, as P2PmsgMgr::Load performs it. Both arms take the
        // length-validated overload: `size` is the REAL extent of the buffer,
        // which is the number a receiver knows and the image cannot forge.
        if ( P2PmsgHeap_IsBSTRio ( pImage ) )
        {
            hHeap = P2PmsgHeap_CreateBSTRio (
                        reinterpret_cast<VBListBSTRio *>(pImage),
                        static_cast<VBLsize>(size) );
            pImage = nullptr;               // ownership transferred
        }
        else if ( P2PmsgHeap_IsIOMAGE ( pImage ) )
        {
            hHeap = P2PmsgHeap_CreateIOMAGE (
                        reinterpret_cast<VBListIOmage *>(pImage),
                        static_cast<VBLsize>(size) );
            pImage = nullptr;               // ownership transferred

            // The duplicate path, which is the M4 residual's closure and takes
            // the same hostile bytes. Only the length-taking overload: the
            // reference-only one cannot check and is documented unsafe.
            Probe ( [&] {
                const P2Piomage *pSrc =
                    reinterpret_cast<const P2Piomage *>(
                        P2PmsgHeap_Addr2Phys ( hHeap, 0 ) );
                if ( pSrc )
                {
                    P2Piomage *pDup =
                        Msgiomage_Duplicate ( *pSrc, static_cast<VBLsize>(size) );
                    if ( pDup ) P2Piomage_Release ( pDup );
                }
            } );
        }
        else
        {
            // Neither tag. A real caller throws here; there is nothing to walk.
            delete[] pImage;
            return 0;
        }

        if ( hHeap ) WalkHeap ( hHeap, data, size );
    }
    catch ( P2Pevent *e ) {
        // The library's structured refusal -- the correct outcome for most
        // inputs. Cancel() releases it; leaking one per rejected input would
        // exhaust the heap long before a real finding.
        if ( e ) e->Cancel ( false );
    }
    catch ( CException *e ) {
        if ( e ) e->Delete ( );
    }
    catch ( ... ) {
    }

    // Ownership again: if a create threw, the transfer never happened and this
    // is still ours. If one succeeded, pImage is null and Close does the free.
    delete[] pImage;
    if ( hHeap ) Probe ( [&] { P2PmsgHeap_Close ( hHeap ); } );

    return 0;
}
