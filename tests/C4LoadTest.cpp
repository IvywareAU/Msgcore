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
//  C4LoadTest.cpp - behavioural test for the C4 fix
//  (see commits: "Reject undersized IOMAGE header..." and
//   "Validate IOMAGE declared size against real buffer on load").
//
//  What it proves
//    [1] Save -> Load round-trip through P2PmsgMgr works (Save=OK Load=OK).
//        IMPORTANT: the saving manager keeps the file open EXCLUSIVELY
//        (P2PmsgMgr::Save sets m_hFile and never closes it; default
//        m_dwSharedMode == 0 = no sharing). So the saver must be destroyed
//        (or SharedMode(FILE_SHARE_READ) used) before the file can be
//        re-opened to load it - hence the explicit saver scope below.
//        Without that scope, Load fails at CreateFile with a sharing
//        violation (this is what made an earlier version of this test appear
//        to show a "round-trip failure"; it was a file-lock artifact, not a
//        Save/Load defect).
//    [2] SECURITY PROPERTY (C4): a file whose IOMAGE header declares a size far
//        LARGER than the file is rejected by Load (returns FALSE) instead of
//        driving an out-of-bounds block walk. A/B verified: the PRE-FIX build
//        instead aborts (EXIT=3) on this input; the fixed build rejects it.
//
//    [9-11] SECURITY PROPERTY (F1): the same class of defect on the OTHER branch
//        of Load's dispatch. Load reads the file into one buffer and then
//        chooses between IsBSTRio and IsIOMAGE; only the IOMAGE arm received
//        C4's length check. [9] is the fuzzer's reproducer byte for byte, [10]
//        is C4's own scenario driven through the BSTRio arm, and [11] guards
//        against "fixing" either by refusing all BSTRio files.
//
//  Only the exported P2PmsgMgr API is used. The malicious IOMAGE file in [2] is
//  hand-crafted so Load reaches its IsIOMAGE branch:
//    - IsBSTRio keys on a type tag (P2PmsgHeap_BSTRio == 4) at byte 0; our low
//      byte is 0xFF, so IsBSTRio is false.
//    - IsIOMAGE is a pure complement check (uiSync2 == ~uiSync1), which we set.
//  That steering is exactly why [2] could not have found F1, and why F1 needed
//  a fuzzer rather than another hand-written case: [2] documents the existence
//  of the other branch in the two lines above and then deliberately avoids it.
//  Scenarios [9]-[11] are the other side of that fork.
//
//  Build/run (from this directory): build_run_c4.bat
//
#include <afx.h>
#include <afxwin.h>
#include "P2PmsgMgr.h"
#include "MsgVBHeap.h"
#include "Msgexception.h"
#include <cstdio>
#include <cstring>
#include <vector>

CWinApp theApp;   // MFC runtime anchor (as in MsgcoreTests)

#pragma pack(push,1)
struct OSyncHdr { unsigned int uiSync1; unsigned int uiSync2; };   // mirrors VBListIOmage.oSync

// Mirrors VBListBSTRio (MsgVBHeap.h). 49 bytes packed, which is not a detail:
// it is the smallest input that reaches the BSTRio branch of Load, and it is
// exactly the size the fuzzer converged on when it found F1.
struct BSTRioHdr
{
    unsigned int uDefs1,  uComp2;          // oDefs: byte 0 = type tag, byte 1 = addr mode
    unsigned int aSize1,  aComp2;          // oSize: declared image size + complement
    unsigned int aAlloc, aAllocSize, aFree, aFreeLast, aFreeSize;   // oKeys
    int          nAllocEntries, nFreeEntries;
    unsigned int aSpare8;
    char         cTag;
};
#pragma pack(pop)

// Builds a BSTRio header that passes P2PmsgHeap_IsBSTRio: type tag 4
// (P2PmsgHeap_BSTRio) in byte 0, addressing mode 2 (Addr32) in byte 1, and both
// complement pairs consistent. Everything IsBSTRio tests is forgeable from
// outside, which is the whole point of these two scenarios.
static void MakeBSTRioHdr ( BSTRioHdr *h, unsigned int declaredSize, unsigned int aAlloc )
{
    memset ( h, 0, sizeof(*h) );
    h->uDefs1 = 0x00000204u;               // byte0=4 tag, byte1=2 Addr32
    h->uComp2 = ~h->uDefs1;
    h->aSize1 = declaredSize;
    h->aComp2 = ~h->aSize1;
    h->aAlloc = aAlloc;
}

// Decodes a hex literal into bytes. Used for F2, whose reproducer is a mutated
// blob rather than a shape worth hand-building: its distinguishing property is
// a free-list block positioned so its header straddles the declared end of the
// image, and getting there by hand would mean encoding VBLock's internal flag
// bits into a test that otherwise touches only the public API. Verbatim bytes
// are the honest fixture.
static std::vector<char> FromHex ( const char *hex )
{
    std::vector<char> out;
    for ( const char *p = hex; p[0] && p[1]; p += 2 ) {
        auto nyb = [] ( char c ) -> int {
            if ( c >= '0' && c <= '9' ) return c - '0';
            if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
            if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
            return 0;
        };
        out.push_back ( static_cast<char>( ( nyb(p[0]) << 4 ) | nyb(p[1]) ) );
    }
    return out;
}

static bool WriteAll ( const char *path, const void *p, size_t n )
{
    FILE *f = nullptr; fopen_s ( &f, path, "wb" ); if ( !f ) return false;
    size_t w = fwrite ( p, 1, n, f ); fclose ( f ); return w == n;
}

int main ( )
{
    setvbuf ( stdout, nullptr, _IONBF, 0 );   // unbuffered: survive an abort/crash

#ifdef _DEBUG
    // Route assertion failures to stderr instead of a message box. Without this
    // a failing ASSERT in a Debug build exits 3 with NOTHING printed -- which is
    // what "the test crashed" looked like from CI, and is unactionable. An
    // assertion reached from Load is a finding either way; it should say which
    // one. (Release compiles these sites out entirely, so this changes nothing
    // there -- that disagreement between the two builds is item 19's subject.)
    _CrtSetReportMode ( _CRT_ASSERT, _CRTDBG_MODE_FILE );
    _CrtSetReportFile ( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
    _CrtSetReportMode ( _CRT_ERROR,  _CRTDBG_MODE_FILE );
    _CrtSetReportFile ( _CRT_ERROR,  _CRTDBG_FILE_STDERR );
#endif

    int fail = 0;

    // --- [1] Positive: Save -> Load round-trip ---------------------------
    try {
        BOOL saved = FALSE, loaded = FALSE;
        {   // saver scope: destroying the manager closes its exclusively-held
            // file handle before we re-open the file to load it.
            P2PmsgMgr mgr;
            mgr.r_name() = _N("C4Root");
            mgr.r_Desc() += P3PmsgField ( L"Alpha" );
            mgr.r_Desc() += P3PmsgField ( L"Beta" );
            saved = mgr.Save ( L"c4_roundtrip.dat" );
        }
        {
            P2PmsgMgr mgr2;
            loaded = mgr2.Load ( L"c4_roundtrip.dat" );
        }
        printf ( "[1 ] round-trip      : Save=%s Load=%s\n",
                 saved ? "OK" : "FAIL", loaded ? "OK" : "FAIL" );
        if ( !saved || !loaded ) {
            P2Pevent *pLast = GetP2Pevent();
            if ( pLast ) {
                // GetModule/GetMessage return CString BY VALUE. Passing a class
                // through varargs is undefined; it only appears to work because
                // CStringT's sole data member is the buffer pointer. Cast to
                // LPCWSTR so the temporaries convert explicitly - and so they
                // outlive the call, which a bare cast of the return value does
                // (the temporary lives to the end of the full expression).
                CString strModule = pLast->GetModule();
                CString strMsg    = pLast->GetMessage();
                printf ( "     error: module='%ls'  msg='%ls'\n",
                         (LPCWSTR)strModule, (LPCWSTR)strMsg );
            }
            fail++;
        }
    } catch ( P2Pevent *e ) { printf ( "[1 ] round-trip      : threw (FAIL)\n" ); e->Cancel(); fail++; }

    // --- [2] Negative: oversized-declared IOMAGE must be rejected --------
    {
        std::vector<char> mal ( 4096, 0 );          // >= sizeof(VBListBSTRio); zero body
        OSyncHdr *hdr = reinterpret_cast<OSyncHdr *>( mal.data() );
        hdr->uiSync1 = ( 2u << 24 ) | 0x00FFFFFFu;  // addr-type=Addr32(2), declared ~16 MB
        hdr->uiSync2 = ~hdr->uiSync1;               // complement -> IsIOMAGE true; low byte 0xFF != 4
        WriteAll ( "c4_evil.iom", mal.data(), mal.size() );
        printf ( "[2 ] evil IOMAGE     : file=%zu bytes, header declares %u\n",
                 mal.size(), hdr->uiSync1 & 0x00FFFFFF );

        bool rejected = false;
        try {
            P2PmsgMgr mgr3;
            BOOL ok = mgr3.Load ( L"c4_evil.iom" );
            rejected = !ok;
            printf ( "[2 ] Load(evil)      : %s\n",
                     ok ? "ACCEPTED oversized (FAIL - OOB risk)" : "rejected FALSE (OK)" );
        } catch ( P2Pevent *e ) {
            rejected = true;
            printf ( "[2 ] Load(evil)      : threw -> rejected (OK)\n" ); e->Cancel();
        }
        if ( !rejected ) fail++;
    }

    // --- [3] Share mode: a saved store is readable while the saver holds it
    // Verifies the FILE_SHARE_READ default. With the old exclusive default
    // (m_dwSharedMode == 0) this second open failed with a sharing violation.
    {
        P2PmsgMgr saver;                       // stays alive - holds c4_share.dat open
        saver.r_name() = _N("ShareRoot");
        saver.r_Desc() += P3PmsgField ( L"X" );
        BOOL saved = saver.Save ( L"c4_share.dat" );

        HANDLE hRead = CreateFileW ( L"c4_share.dat", GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
        bool canRead = ( hRead != INVALID_HANDLE_VALUE );
        if ( canRead ) CloseHandle ( hRead );
        printf ( "[3 ] read while saver alive: %s (Save=%s)\n",
                 canRead ? "OK (shared)" : "FAIL (locked)", saved ? "OK" : "FAIL" );
        if ( !saved || !canRead ) fail++;
    }

    // --- [4] Load while a saver still holds the file (concurrent Load) ---
    // Read-only Load shares READ|WRITE, so it coexists with a live saver.
    // Under the old read/write Load this failed with a sharing violation.
    {
        P2PmsgMgr saver;                       // stays alive - holds c4_live.dat
        saver.r_name() = _N("LiveRoot");
        saver.r_Desc() += P3PmsgField ( L"Y" );
        BOOL saved  = saver.Save ( L"c4_live.dat" );
        BOOL loaded = FALSE;
        try {
            P2PmsgMgr reader;
            loaded = reader.Load ( L"c4_live.dat" );
        } catch ( P2Pevent *e ) { e->Cancel(); }
        printf ( "[4 ] Load while saver alive: %s (Save=%s)\n",
                 loaded ? "OK (concurrent)" : "FAIL (locked)", saved ? "OK" : "FAIL" );
        if ( !saved || !loaded ) fail++;
    }

    // --- [5] Save-back after a read-only Load (Save() reopens by name) ----
    // Load retains no handle; Save() with no filename must reopen the
    // remembered file for writing.
    {
        BOOL loaded = FALSE, savedBack = FALSE;
        try {
            P2PmsgMgr mgr;
            loaded = mgr.Load ( L"c4_roundtrip.dat" );
            mgr.r_Desc() += P3PmsgField ( L"AddedAfterLoad" );
            savedBack = mgr.Save ( );          // no filename -> reopen remembered file
        } catch ( P2Pevent *e ) { e->Cancel(); }
        printf ( "[5 ] save-back after load : Load=%s Save()=%s\n",
                 loaded ? "OK" : "FAIL", savedBack ? "OK" : "FAIL" );
        if ( !loaded || !savedBack ) fail++;
    }

    // --- [6] Atomic save: no temp left behind, target is loadable --------
    // Save writes a sibling temp then renames it over the target, so after a
    // successful Save no "<target>.<tid>.tmp" remains and the target loads.
    {
        P2PmsgMgr mgr;
        mgr.r_name() = _N("AtomicRoot");
        mgr.r_Desc() += P3PmsgField ( L"Z" );
        BOOL saved = mgr.Save ( L"c4_atomic.dat" );

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW ( L"c4_atomic.dat.*.tmp", &fd );
        bool tempLeft = ( hFind != INVALID_HANDLE_VALUE );
        if ( hFind != INVALID_HANDLE_VALUE ) FindClose ( hFind );

        BOOL loaded = FALSE;
        try { P2PmsgMgr m2; loaded = m2.Load ( L"c4_atomic.dat" ); }
        catch ( P2Pevent *e ) { e->Cancel(); }

        printf ( "[6 ] atomic save         : Save=%s tempLeft=%s Load=%s\n",
                 saved ? "OK" : "FAIL", tempLeft ? "YES(FAIL)" : "no(OK)",
                 loaded ? "OK" : "FAIL" );
        if ( !saved || tempLeft || !loaded ) fail++;
    }

    // --- [7] Save to an unwritable path fails cleanly (no crash) ---------
    // The temp is a sibling of the target (always same volume); if it cannot
    // be created, Save throws internally and returns FALSE - it never falls
    // back to a non-atomic in-place write, and an existing store is untouched.
    {
        P2PmsgMgr mgr;
        mgr.r_name() = _N("BadRoot");
        BOOL saved = TRUE;
        try { saved = mgr.Save ( L"Z:\\no_such_dir_xyz\\c4_bad.dat" ); }
        catch ( P2Pevent *e ) { saved = FALSE; e->Cancel(); }
        printf ( "[7 ] save to bad path    : %s\n",
                 saved ? "accepted (FAIL)" : "failed cleanly (OK)" );
        if ( saved ) fail++;
    }

    // --- [8] Concurrent-writer lock is respected -------------------------
    // Hold the lock file the way another writer would; a Save must fail to
    // acquire it (after its bounded retry) and return FALSE, not race.
    {
        { P2PmsgMgr m; m.r_name() = _N("LockRoot"); m.Save ( L"c4_lock.dat" ); }
        HANDLE hHeld = CreateFileW ( L"c4_lock.dat.lock", GENERIC_WRITE | DELETE, 0,
                                     nullptr, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr );
        bool held = ( hHeld != INVALID_HANDLE_VALUE );
        BOOL saved = TRUE;
        try { P2PmsgMgr m2; m2.r_name() = _N("LockRoot2"); saved = m2.Save ( L"c4_lock.dat" ); }
        catch ( P2Pevent *e ) { saved = FALSE; e->Cancel(); }
        if ( hHeld != INVALID_HANDLE_VALUE ) CloseHandle ( hHeld );
        printf ( "[8 ] save blocked by lock: held=%s Save=%s\n",
                 held ? "yes" : "no", saved ? "succeeded (FAIL)" : "blocked (OK)" );
        if ( !held || saved ) fail++;
    }

    // --- [9] F1 regression: the fuzzer's reproducer, byte for byte ---------
    // Promoted out of tests\fuzz\findings\ in the commit that fixed F1, which is
    // the rule this suite is meant to follow: a finding is closed when the fix
    // and the case that would have caught it land together.
    //
    // WHY SCENARIO [2] DID NOT CATCH THIS. Read [2]'s construction: it sets the
    // low byte to 0xFF *so that* IsBSTRio is false and Load takes the IOMAGE
    // branch. It documents the existence of the other branch and then does not
    // test it. This input is the mirror image - tag 4, so Load takes the BSTRio
    // branch, the one C4's fix never reached.
    //
    // The header declares 2032 bytes inside a 49-byte file, and aAlloc (0x30)
    // points at the last byte of it, so the nine-byte VBLockHdr read at that
    // offset runs off the end. Pre-fix: heap-buffer-overflow read, ASan abort.
    {
        BSTRioHdr h;
        MakeBSTRioHdr ( &h, 2032, 0x30 );  // declared 2032 >> file 49; aAlloc at EOF
        h.aAllocSize    = 0x10000000;      // as the fuzzer produced them
        h.aFree         = 0x0f000007;
        h.aFreeLast     = 0x30ffffff;
        h.nFreeEntries  = 0x00250000;
        WriteAll ( "c4_f1_bstrio.dat", &h, sizeof(h) );
        printf ( "[9 ] F1 BSTRio       : file=%zu bytes, header declares %u, aAlloc=0x%x\n",
                 sizeof(h), h.aSize1, h.aAlloc );

        bool rejected = false;
        try {
            P2PmsgMgr mgr;
            BOOL ok = mgr.Load ( L"c4_f1_bstrio.dat" );
            rejected = !ok;
            printf ( "[9 ] Load(F1)        : %s\n",
                     ok ? "ACCEPTED (FAIL - OOB read)" : "rejected FALSE (OK)" );
        } catch ( P2Pevent *e ) {
            rejected = true;
            printf ( "[9 ] Load(F1)        : threw -> rejected (OK)\n" ); e->Cancel();
        }
        if ( !rejected ) fail++;
    }

    // --- [10] C4's scenario, through the branch C4's fix never reached -----
    // The same shape as [2] - a small buffer whose header declares a large image
    // - but on the BSTRio side: 4 KB of file declaring 128 KB. Before the
    // length-validated P2PmsgHeap_CreateBSTRio this was not merely unchecked,
    // it was actively harmful: the declared size was copied into the handle as
    // nSizeofAlloc, which is the ceiling every later address translation is
    // tested against, so forging it RAISED the bound rather than tripping it.
    {
        std::vector<char> mal ( 4096, 0 );
        MakeBSTRioHdr ( reinterpret_cast<BSTRioHdr *>( mal.data() ), 128u * 1024u, 0x40 );
        WriteAll ( "c4_bstrio_oversize.dat", mal.data(), mal.size() );
        printf ( "[10] oversize BSTRio : file=%zu bytes, header declares %u\n",
                 mal.size(), 128u * 1024u );

        bool rejected = false;
        try {
            P2PmsgMgr mgr;
            BOOL ok = mgr.Load ( L"c4_bstrio_oversize.dat" );
            rejected = !ok;
            printf ( "[10] Load(oversize)  : %s\n",
                     ok ? "ACCEPTED oversized (FAIL - OOB risk)" : "rejected FALSE (OK)" );
        } catch ( P2Pevent *e ) {
            rejected = true;
            printf ( "[10] Load(oversize)  : threw -> rejected (OK)\n" ); e->Cancel();
        }
        if ( !rejected ) fail++;
    }

    // --- [11] A truthful header over a zero-filled body must be rejected ---
    // Two jobs. It guards against closing F1 by making Load reject BSTRio files
    // outright, which would pass [9] and [10] for the wrong reason: the declared
    // size here EQUALS the real file, so the length gate has nothing to object
    // to and the input has to be refused on its contents or not at all.
    //
    // And it is item 19's regression case. A zero-filled block area is not
    // obviously invalid to this format -- VBLock_Addr08 is 0, so an all-zero
    // header reads as a structurally valid 8-bit block of size 0. The walk that
    // catches it (P2PmsgHeap_AssertVBlocksBSTRio) existed all along, but sat
    // inside ASSERT(...) at its call site and reported its findings through
    // ASSERTs internally, so it neither ran nor concluded anything in Release.
    // Before item 19 this case reached "did not crash" and no further. It now
    // has to be REJECTED, in both builds.
    //
    // The assertion lines printed above this are expected: the per-block checks
    // still ASSERT as well as failing the walk, because stopping at the first
    // bad block under a debugger is worth keeping.
    {
        std::vector<char> mal ( 512, 0 );
        MakeBSTRioHdr ( reinterpret_cast<BSTRioHdr *>( mal.data() ), 512, 0 );
        WriteAll ( "c4_bstrio_honest.dat", mal.data(), mal.size() );

        bool rejected = false;
        try {
            P2PmsgMgr mgr;
            BOOL ok = mgr.Load ( L"c4_bstrio_honest.dat" );
            rejected = !ok;
        } catch ( P2Pevent *e ) { rejected = true; e->Cancel(); }
        printf ( "[11] zero-filled BSTRio : %s\n",
                 rejected ? "rejected (OK)" : "ACCEPTED a zero-filled body (FAIL)" );
        if ( !rejected ) fail++;
    }

    // --- [12] M5: an Addr08 heap must refuse to outgrow its addressing width -
    // Finding M5. An Addr08 heap stores every link (aParent/aPrev/aNext/aFirst/
    // aLast) in a UINT08, and MsgAttr.cpp narrowed to that width with a C-style
    // cast. Past offset 255 the cast does not fail -- it produces a DIFFERENT
    // VALID-LOOKING offset, so the link lands on the wrong block and the free
    // list is silently corrupted. Nothing downstream can notice, because the
    // evidence was the high bits.
    //
    // Declaring fields until the heap passes 255 bytes is the churn the finding
    // describes. The required outcome is a refusal, not a success: an 8-bit heap
    // genuinely cannot address this store, and saying so is the only honest
    // answer. Silently truncating was the bug.
    //
    // A/B: before the fix this loop runs to completion and "succeeds", leaving a
    // corrupt store behind. After it, the declare that crosses the boundary
    // throws.
    // THIS SCENARIO IS ABOUT M6, NOT M5, AND THAT DISTINCTION IS THE FINDING.
    // M5's guard is in MsgAttr.cpp. This case cannot reach it, and establishing
    // why is what produced M6.
    //
    // Three attempts, and what each one established:
    //   - r_Desc(): links through MsgDesc.cpp, never reaches
    //     P2PmsgAttr_LinkinItem at all. Grew to 77 KB and reported a clean pass
    //     while touching none of the code under test.
    //   - Addr08: refused far below its own 255 boundary for an unrelated
    //     reason ("Invalid Addr16,32,64 address reference" at offset 0xb8), so
    //     it never reached the narrowing either. That refusal is now explicit
    //     and up front -- see [12b].
    //   - Addr16 + r_Attr(): the store reports Addrnn=1, so it really is a
    //     16-bit heap -- and it grew its arena to 77833 bytes, past its own
    //     declared nSizeofMax of 65535, without the ceiling check at
    //     P2PmsgHeap_Alloc ever firing.
    //
    // That last measurement was M6: an offset could exceed the addressing width
    // because the ARENA was allowed to. The ceiling check tested nSizeofUsed --
    // live allocated bytes, which lag the arena by the 20% oversize slack and
    // fall again on every Free -- while the quantity that had to stay bounded
    // was nSizeofAlloc, which nothing tested at all.
    //
    // FIXED: P2PmsgHeap_CapGrowth now caps both resize paths against
    // nSizeofMax, clamping the oversize bump and throwing when the allocation
    // itself will not fit. The arena stops AT 65535 and the store then refuses,
    // which is the honest answer -- a 16-bit heap genuinely cannot address more.
    //
    // A/B: before the fix `crossed` came back true at 77833 bytes and the loop
    // ran to completion. After it, the arena is never observed above 0xFFFF and
    // the churn terminates in a refusal rather than in success.
    //
    // AND NOTE WHAT THIS MEANS FOR M5, which is not what the plan predicted.
    // Fixing M6 did not make M5's guard reachable; it made it PERMANENTLY
    // unreachable through the public API. M5 assumes "under churn an offset can
    // exceed the addressing width" -- that assumption was only ever true
    // because the arena could. With the arena bounded by the width, no offset
    // into it can exceed the width, so the narrowing M5 guards cannot truncate.
    // The guard stays as the enforcement of an invariant now maintained one
    // layer down, and there is no churn test to write for it. See
    // The release-readiness register, item 16a.
    {
        bool refused = false, crossed = false;
        unsigned int nReached = 0;
        try {
            P2PmsgMgr mgr ( 1 /*VBLock_Addr16*/, 2024, 0 );
            mgr.r_name() = _N("M5Root");
            for ( int i = 0; i < 20000; i++ ) {
                wchar_t szName[32];
                swprintf_s ( szName, L"Attr%05d", i );
                mgr.r_Attr() += P3PmsgItem ( szName, P3PmsgData((UINT32)i) );
                nReached = (unsigned int)mgr.Sizeof();
                if ( nReached > 0xFFFFu ) { crossed = true; break; }
            }
        } catch ( P2Pevent *e ) {
            refused = true;
            CString strMsg = e->GetMessage();
            printf ( "[12] M6 Addr16 churn : refused at %u bytes -> '%ls'\n",
                     nReached, (LPCWSTR)strMsg );
            e->Cancel();
        } catch ( ... ) {
            refused = true;
            printf ( "[12] M6 Addr16 churn : refused at %u bytes (OK)\n", nReached );
        }

        if ( crossed ) {
            printf ( "[12] M6 Addr16 arena : grew to %u bytes past the 65535 its "
                     "Addr16 width can address (FAIL)\n", nReached );
            fail++;
        }
        // A refusal is required, not merely permitted. 20000 attributes cannot
        // fit in 64 KB, so a run that completes without throwing has stopped
        // enforcing the ceiling somewhere rather than passing this test.
        if ( !refused ) {
            printf ( "[12] M6 Addr16 churn : completed 20000 inserts inside %u "
                     "bytes without refusing (FAIL)\n", nReached );
            fail++;
        }
    }

    // --- [12b] Addr08 is not a supported heap width, in every build ---------
    // It never was: Msgcore_c.h has documented uAddrNN as 16/32/64 since the
    // flat ABI was written, and all three create-by-width entry points carried
    // ASSERT(uAddrType!=VBLock_Addr08). Release compiled all three out, so a
    // Release caller passing 0 got the configuration the author had asserted
    // against and failed later somewhere unrelated -- which is what the second
    // bullet above recorded before anyone recognised it as that.
    //
    // Item 19's category exactly: structural intent expressed in a form absent
    // from every shipped binary. This case exists because a refusal that only
    // happens in Debug is not a refusal.
    // The message matters as much as the refusal. Addr08 caps nSizeMax at 255,
    // so ANY sane initial size trips "nSizeInitial exceeds nSizeMax" first -- a
    // true statement that names the wrong parameter and would let this case
    // pass in a build with no width check in it at all. The guard therefore runs
    // before the size arithmetic, and this asserts that it is what spoke.
    {
        bool    refused = false;
        bool    byWidth = false;
        CString strMsg;
        try {
            P2PmsgMgr mgr ( 0 /*VBLock_Addr08*/, 2024, 0 );
            mgr.r_name() = _N("Addr08Root");
        } catch ( P2Pevent *e ) { refused = true; strMsg = e->GetMessage(); e->Cancel(); }
          catch ( ... )         { refused = true; }
        byWidth = strMsg.Find ( _T("not a supported heap addressing width") ) >= 0;
        printf ( "[12b] Addr08 create  : %s -> '%ls'\n",
                 refused ? "refused" : "ACCEPTED an unsupported width (FAIL)",
                 (LPCWSTR)strMsg );
        if ( !refused ) fail++;
        if ( !byWidth ) {
            printf ( "[12b] Addr08 create  : refused, but not as an unsupported "
                     "width (FAIL - the guard did not run first)\n" );
            fail++;
        }
    }

    // --- [13] F2: the validator must not read off the end while validating --
    // Found by the fuzz job fifteen minutes after item 19 made the block walk
    // run in Release, and it is a finding ABOUT that change: the walk itself
    // reads out of bounds on a crafted image. Before item 19 this was
    // Debug-only, because in Release the walk did not run at all.
    //
    // Every bounds check on the walk's path tested "aVBLock < nSizeofAlloc" --
    // the block's FIRST byte and nothing else. A free-list block starting just
    // under the declared end passes that, and VBHeap_GetPrev then reads its
    // 8-byte nPrev field past the buffer. Exactly the start-versus-span mistake
    // P2PmsgHeap_Addr2PhysChk was added for in item 13, in a function nobody had
    // looked at because it was never running.
    //
    // The lesson is the one worth keeping: a validator handed attacker-controlled
    // bytes is not exempt from the rules it enforces. One that reads out of
    // bounds while validating is worse than no validator, because it turns a
    // rejection into a crash.
    //
    // 519 bytes, declaring 516. Verbatim, and also seeded into tests\fuzz\corpus.
    {
        static const char szF2[] =
        "04020000fbfdffff04020000fbfdfffff0000000000000000000000000000000"
        "000000000000023b000000000000000000da0000ff00000000000000dcffffff"
        "ffffffffffffffffffffffffffffffffffffffff000000000000000000000000"
        "000000000100000f0000003e0000000000000000000000000000000000010000"
        "008000000000fb00000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000004000000"
        "000060000000002c000000000000000000000000006700720020006e0061006d"
        "0065000000000000003f00870000000000000000000000000000000000000000"
        "000000000000000000000020000000023b000000000000000000da0000ffefff"
        "ffffffffffff0200000000d00000000000fb000000000000000051515151510f"
        "0000003e0000000000000000000000000000000000000000008000000000fb00"
        "000000609e000000000000022e313ffa2f000000003000000003010000000000"
        "0000000000000060000000002c59595959595959595959595959595959595959"
        "59595959595959595900870000000000000000000000000000f00700000ff8ff"
        "ff30ffffffffff979680000000bf020000ef022e313ffa2f000000003000002e"
        "313ffa2f000000003000000003010000000082ffffffb4020000030000000000"
        "00000000000060";
        std::vector<char> f2 = FromHex ( szF2 );
        WriteAll ( "c4_f2_walk_oob.dat", f2.data(), f2.size() );
        printf ( "[13] F2 walk OOB     : file=%zu bytes, header declares %u\n",
                 f2.size(), *reinterpret_cast<const unsigned int *>( f2.data() + 8 ) );

        bool rejected = false;
        try {
            P2PmsgMgr mgr;
            BOOL ok = mgr.Load ( L"c4_f2_walk_oob.dat" );
            rejected = !ok;
        } catch ( P2Pevent *e ) { rejected = true; e->Cancel(); }
        printf ( "[13] Load(F2)        : %s\n",
                 rejected ? "rejected (OK)" : "ACCEPTED (FAIL - OOB read in the walk)" );
        if ( !rejected ) fail++;
    }

    // --- [14] M6, load side: an image may not declare more arena than its own
    //          addressing width can address --------------------------------
    // The growth path in [12] is only half of M6. The create-from-image paths
    // take nSizeofAlloc from the IMAGE and nSizeofMax from the addressing width
    // named in the SAME image, and never compared the two -- so a header saying
    // "Addr16" over a 64 KB+ arena starts life with the invariant already
    // broken, no allocation churn required. That is the half a fuzzer reaches.
    //
    // READ THIS BEFORE TRUSTING THE PASS. The assertion here is on the MESSAGE,
    // not on the rejection, and that is deliberate. This body is zero-filled, so
    // the block walk added by item 19 rejects it too -- meaning a bare
    // "was it rejected?" check passes with or without the M6 fix and proves
    // nothing. What changed is WHICH check refuses and how early: the width
    // check runs inside P2PmsgHeap_CreateBSTRio, before the two-argument
    // overload's walk, so a fixed build blames the declared width and an
    // unfixed one blames the block structure.
    //
    // A hand-built body that passes the walk AND crosses the width is not
    // constructible through the public API without encoding VBLock internals
    // into this file; the fuzz corpus is where that case belongs. This scenario
    // pins the check's presence and its ordering, which is what it can honestly
    // claim.
    {
        const unsigned int nDeclared = 0x10001u;        // one byte past Addr16
        std::vector<char> mal ( nDeclared, 0 );
        BSTRioHdr *h = reinterpret_cast<BSTRioHdr *>( mal.data() );
        MakeBSTRioHdr ( h, nDeclared, 0x40 );
        h->uDefs1 = 0x00000104u;                        // byte0=4 tag, byte1=1 Addr16
        h->uComp2 = ~h->uDefs1;
        WriteAll ( "c4_addr16_oversize.dat", mal.data(), mal.size() );
        printf ( "[14] Addr16 arena    : file=%u bytes, header declares Addr16 over %u\n",
                 nDeclared, nDeclared );

        bool     rejected = false;
        bool     byWidth  = false;
        CString  strMsg;
        SetP2Pevent ( 0 );          // clear the thread's last event first
        try {
            P2PmsgMgr mgr;
            BOOL ok = mgr.Load ( L"c4_addr16_oversize.dat" );
            rejected = !ok;
            // Load does not rethrow: catch_pP2Pevent_SetLast parks the event on
            // the thread and returns FALSE, so the reason is read back rather
            // than caught. Without this the message is empty and the ordering
            // assertion below cannot be made at all.
            if ( !ok ) {
                P2Pevent *pLast = GetP2Pevent();
                if ( pLast ) strMsg = pLast->GetMessage();
            }
        } catch ( P2Pevent *e ) {
            rejected = true;
            strMsg   = e->GetMessage();
            e->Cancel();
        }
        // "past the 0x... addressable by the Addr16 width it also declares"
        byWidth = strMsg.Find ( _T("addressable by") ) >= 0;
        printf ( "[14] Load(Addr16)    : %s -> '%ls'\n",
                 rejected ? "rejected" : "ACCEPTED (FAIL)", (LPCWSTR)strMsg );
        if ( !rejected ) fail++;
        if ( !byWidth ) {
            printf ( "[14] Addr16 arena    : rejected, but not by the width check "
                     "(FAIL - M6's load half is not in this build)\n" );
            fail++;
        }
    }

    // --- [15] F3: the IOMAGE arm of the load dispatch must walk its blocks
    //          FOR REAL, and must not read off the end while doing it -------
    // F1 was "the BSTRio arm has no length check"; F2 was "the BSTRio walk reads
    // past the end while validating". Both were fixed on the BSTRio arm. The
    // load dispatch has TWO arms, and this is the other one: the length-taking
    // P2PmsgHeap_CreateIOMAGE delegated to the single-argument overload, which
    // ends with
    //     ASSERT(P2PmsgHeap_AssertValidIOMAGE(pHandle));
    //     ASSERT(P2PmsgHeap_AssertVBlocksIOMAGE(pHandle));
    // -- the whole validation inside the assertion, so a shipped binary did not
    // run a reduced version of it, it did not call it at all. Item 19.
    //
    // The two walks it names had F2's bug as well as being dormant, and one of
    // them was WORSE off than F2 ever was: P2PmsgHeap_AssertValidIOMAGE is
    // already reached from Release code by callers that invoke it directly
    // rather than through ASSERT, so its unguarded header read was live in
    // shipped binaries rather than waiting to be woken. Both now call
    // P2PmsgHeap_BlockFits before dereferencing.
    //
    // READ THIS BEFORE TRUSTING THE PASS, for the same reason [14] says so. The
    // body here is zero-filled, so "was it rejected?" is not the interesting
    // question -- a zero body is refused by more than one check and that much
    // passed before this fix too. The assertion is on WHICH check refuses: the
    // message must come from the block-structure gate, which only exists on the
    // path this scenario is about. An unfixed build rejects with some other
    // reason, or accepts.
    //
    // Note the header is TRUTHFUL -- declared size == file size -- precisely so
    // that [2]'s declared-size check cannot fire and take the credit.
    {
        const unsigned int nDeclared = 4096u;
        std::vector<char> mal ( nDeclared, 0 );
        OSyncHdr *hdr = reinterpret_cast<OSyncHdr *>( mal.data() );
        hdr->uiSync1 = ( 2u << 24 ) | ( nDeclared & 0x00FFFFFFu );  // Addr32, truthful size
        hdr->uiSync2 = ~hdr->uiSync1;
        // The root's OWN addressing byte has to agree with the header's, for the
        // same reason the header is truthful: so that an EARLIER check cannot
        // fire and take the credit. A zero-filled root declares Addr08 against a
        // header declaring Addr32, which the root-consistency check refuses
        // before the block walk is ever reached -- correctly, but that is a
        // different defect (F7) with its own reproducer, and this scenario
        // asserts on WHICH check refuses. VBHeapRoot is private to
        // MsgVBHeap.cpp, so the field is written by offset: oSync is 8 bytes,
        // then UINT16 nSize, then UINT16 uVBLock.
        *reinterpret_cast<unsigned short *>( mal.data() + 10 ) = VBLock_Addr32;
        WriteAll ( "c4_f3_iomage_walk.iom", mal.data(), mal.size() );
        printf ( "[15] F3 IOMAGE walk  : file=%u bytes, header declares %u (truthful)\n",
                 nDeclared, hdr->uiSync1 & 0x00FFFFFF );

        bool    rejected = false;
        bool    byWalk   = false;
        CString strMsg;
        SetP2Pevent ( 0 );          // clear the thread's last event first
        try {
            P2PmsgMgr mgr;
            BOOL ok = mgr.Load ( L"c4_f3_iomage_walk.iom" );
            rejected = !ok;
            if ( !ok ) {
                P2Pevent *pLast = GetP2Pevent();
                if ( pLast ) strMsg = pLast->GetMessage();
            }
        } catch ( P2Pevent *e ) {
            rejected = true;
            strMsg   = e->GetMessage();
            e->Cancel();
        }
        byWalk = strMsg.Find ( _T("block structure") ) >= 0;
        printf ( "[15] Load(F3)        : %s -> '%ls'\n",
                 rejected ? "rejected" : "ACCEPTED (FAIL - IOMAGE walk did not gate)",
                 (LPCWSTR)strMsg );
        if ( !rejected ) fail++;
        if ( !byWalk ) {
            printf ( "[15] F3 IOMAGE walk  : rejected, but not by the block-structure "
                     "gate (FAIL - the IOMAGE arm's walk is not running in this build)\n" );
            fail++;
        }
    }

    printf ( "\nC4 Save/Load test: %s\n", fail == 0 ? "PASS" : "FAIL" );
    return fail == 0 ? 0 : 1;
}
