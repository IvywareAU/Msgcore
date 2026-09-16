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
// SuiteMain.cpp
//
// Entry point for this repository's unit-test runner.
//
// The release-readiness register, item 15 (Stage E). Until this landed, "test coverage" in
// this repository meant one file of fifteen load-path scenarios -- C4LoadTest,
// which is a security regression harness and was never a unit suite. This runner
// carries the two suites from MscsUnitTests that test THIS library and nothing
// else: the C++ surface and the flat C ABI.
//
// WHAT IS DELIBERATELY NOT HERE:
//   * RunTargetcoreSuite -- tests Targetcore, a sibling component this
//     repository does not contain.
//   * The COleTime cases from the upstream Msgcore suite -- DATE2Normalised and
//     Normalised2DATE live in MsgcoreMFC, another component that is not here.
//     They are dropped rather than stubbed: a stub that always passes is worse
//     than an absent case, because it reads like coverage.
//
// A suite that is compiled out must SAY SO. Upstream lost a suite from its
// project file for fourteen days and the omission was invisible, because a
// dropped suite produces output indistinguishable from a clean full run. Every
// guard below has an #else that reports the skip, and tf_runner_finish refuses
// to call a run with nSkipped > 0 a pass.

#include <afx.h>
#include <afxwin.h>

#include <cstdio>

#include "TestFramework.h"

// ---------------------------------------------------------------------------
int main(int /*argc*/, char* /*argv*/[])
{
    if (!tf_runner_startup("Msgcore unit tests"))
        return 1;

    int nSkipped = 0;

    printf("\n[Msgcore]\n");
    RunMsgcoreSuite();

    // The C-API suite needs only Msgcore_c.h. MSGCORE_NO_CAPI drops it; nothing
    // in this tree defines that today, and the guard is kept because the skip
    // must stay visible if anything ever does.
#ifndef MSGCORE_NO_CAPI
    printf("\n[Msgcore C API]\n");
    RunMsgcoreCApiSuite();
#else
    printf("\n[Msgcore C API]       SKIPPED (MSGCORE_NO_CAPI)\n");
    ++nSkipped;
#endif

    return tf_runner_finish(nSkipped);
}
