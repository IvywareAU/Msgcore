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
//  fuzz_iomage_load.cpp -- libFuzzer harness over P2PmsgMgr::Load.
//
//  The release-readiness register, item 14, fourth job. C4 was a parser trusting a size it
//  read out of the very buffer it was parsing, and C4LoadTest.cpp pins exactly
//  one input against exactly that bug. This drives the same entry point with
//  inputs nobody wrote by hand.
//
//  WHY THIS ENTRY POINT. Load is the whole attack surface: it is the only place
//  Msgcore consumes bytes it did not produce. Everything reachable from it --
//  the IsBSTRio/IsIOMAGE tag dispatch, the block walk, MsgVBHeap's free-list
//  reconstruction, the MsgAttr addressing-width decode -- is parsing code
//  reading self-declared sizes, which is the defect class the whole document
//  keeps circling.
//
//  WHY A FILE PER INPUT, which costs perhaps an order of magnitude in exec/s.
//  Load takes a path and opens it itself; there is no memory-buffer overload to
//  target, and inventing one for the fuzzer would mean fuzzing a code path no
//  caller uses. A slower harness over the real entry point beats a fast one
//  over a fiction. (Item 13 adds a length-taking shape to the Duplicate side;
//  if a buffer-taking Load ever follows it, retarget this at once.)
//
//  WHAT COUNTS AS A FINDING. Not "Load returned FALSE" -- rejecting malformed
//  input is the correct behaviour and most inputs will be rejected. Not a
//  thrown P2Pevent* either; that is the library's own structured refusal and
//  the C4 gate itself throws. A finding is a crash, an ASan report, or a hang:
//  memory unsafety reached through a path that was supposed to have validated
//  its way out first.
//
//  A NOTE ON ASSERT, which bounds what this job can prove. Per the security
//  posture section, 277 sites do structural validation inside ASSERT(...),
//  which compiles out of Release. This harness builds ReleaseLib, so those
//  checks are absent -- deliberately, because that is the configuration a
//  consumer links. It means a crash found here may be one a Debug build would
//  have caught politely, and that is the point: item 19 exists because those
//  two builds disagree about what is valid.
//
#include <afx.h>
#include <afxwin.h>
#include "P2PmsgMgr.h"
#include "Msgexception.h"
#include <cstdint>
#include <cstdio>
#include <string>

CWinApp theApp;   // MFC runtime anchor, as in C4LoadTest.cpp

namespace {

// One path per process, not per input. libFuzzer -workers=N forks N processes
// sharing a working directory, so a fixed name would have them overwriting each
// other's input mid-Load and reporting crashes that belong to no single input.
std::wstring TargetPath ( )
{
    wchar_t buf[64];
    swprintf_s ( buf, L"fuzz_input_%lu.iom", ::GetCurrentProcessId ( ) );
    return std::wstring ( buf );
}

const std::wstring g_path = TargetPath ( );

}   // namespace

extern "C" int LLVMFuzzerTestOneInput ( const uint8_t *data, size_t size )
{
    // Under the declared header size Load has nothing to dispatch on; skip
    // rather than spend the corpus on inputs that cannot reach the parser.
    if ( size < 8 ) return 0;

    {
        FILE *f = nullptr;
        if ( _wfopen_s ( &f, g_path.c_str ( ), L"wb" ) != 0 || !f ) return 0;
        fwrite ( data, 1, size, f );
        fclose ( f );
    }

    try {
        P2PmsgMgr mgr;
        mgr.Load ( g_path.c_str ( ) );        // return value deliberately ignored
    }
    catch ( P2Pevent *e ) {
        // The library's structured refusal. Cancel() releases it -- leaking one
        // per rejected input would exhaust the heap long before a real finding.
        if ( e ) e->Cancel ( );
    }
    catch ( CException *e ) {
        // MFC's own throw shape, which the file layer can still raise.
        if ( e ) e->Delete ( );
    }

    return 0;
}
