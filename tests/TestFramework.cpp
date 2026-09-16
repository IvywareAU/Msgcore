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
// TestFramework.cpp
//
// The runner lifecycle and assertion bookkeeping for the Msgcore suites.
// See TestFramework.h for provenance and for the one deliberate divergence
// from the upstream copy (no StartupP2Pmsg / WSAStartup -- both live in
// Targetcore, which this repository does not contain).
//
// It owns the process-wide lifecycle the suites rely on:
//   * one CWinApp (MFC requires exactly one per executable),
//   * an assert hook that turns an MFC/CRT ASSERT into a recorded test failure
//     instead of a modal dialog that would hang a headless run.
//
// THAT ASSERT HOOK IS NOT A CONVENIENCE HERE, it is load-bearing, and item 19
// is why. Msgcore is full of ASSERTs that fire on deliberately malformed input;
// a suite that exercises the load path WILL trip them in a Debug build. Without
// the hook the first one opens a modal dialog and the run hangs with no output,
// which is precisely what "the test crashed" looked like from CI before
// C4LoadTest.cpp added the same treatment. Folding them into the current case as
// failures means a Debug run reports WHICH case reached an assertion.

#include <afx.h>
#include <afxwin.h>

#include <crtdbg.h>
#include <cstdio>

#include "TestFramework.h"

// MFC requires exactly one CWinApp instance per executable. This TU is linked
// into the runner exactly once, so the exe gets exactly one.
CWinApp theApp;

// ---------------------------------------------------------------------------
// Framework state
// ---------------------------------------------------------------------------
TestStats g_tf;

static const char* s_currentCase         = "(none)";
static int         s_caseFailureBaseline = 0;

void tf_begin_case(const char* name)
{
    s_currentCase         = name;
    s_caseFailureBaseline = g_tf.failures;
    ++g_tf.cases;
    printf("  - %s\n", name);
    fflush(stdout);
}

void tf_end_case()
{
    if (g_tf.failures > s_caseFailureBaseline)
        ++g_tf.caseFailures;
    s_currentCase = "(none)";
}

void tf_fail(const char* file, int line, const char* expr)
{
    ++g_tf.failures;
    printf("      FAIL [%s]  %s\n", s_currentCase, expr);
    printf("           at %s:%d\n", file, line);
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// Assert trap: fold a debug ASSERT into a failure of the current case and let
// execution continue (return TRUE + retVal 0 = "handled, do not break").
// ---------------------------------------------------------------------------
static int __cdecl AssertReportHook(int nReportType, char* szMsg, int* pnRet)
{
    if (nReportType == _CRT_ASSERT)
    {
        ++g_tf.failures;
        printf("      ASSERT [%s]  %s\n",
               s_currentCase, szMsg ? szMsg : "(no message)");
        fflush(stdout);
        if (pnRet) *pnRet = 0;   // do not invoke the debugger
        return TRUE;             // handled -> continue execution
    }
    return FALSE;                // let other report types flow normally
}

// ---------------------------------------------------------------------------
bool tf_runner_startup(const char* title)
{
    setvbuf(stdout, nullptr, _IONBF, 0);   // unbuffered: survive an abort/crash

    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportHook(AssertReportHook);

    printf("=== %s ===\n", title);
    fflush(stdout);
    return true;
}

// A reduced run is not a pass. Exit codes: 0 = every suite ran and passed,
// 1 = a check failed, 2 = everything that ran passed but suites were compiled
// out. 2 is distinct so a caller can tell "broken" from "incomplete"; both are
// non-zero, so any pass/fail consumer treats them alike.
int tf_runner_finish(int nSkipped)
{
    printf("\n=== Summary ===\n");
    printf("  cases   : %d  (%d with failures)\n", g_tf.cases, g_tf.caseFailures);
    printf("  checks  : %d  (%d failed)\n", g_tf.checks, g_tf.failures);
    printf("  skipped : %d suite(s)\n", nSkipped);
    printf("  result  : %s\n", g_tf.failures ? "FAIL"
                             : nSkipped      ? "INCOMPLETE"
                                             : "PASS");
    if (nSkipped && !g_tf.failures)
        printf("            %d suite(s) were compiled out (see SKIPPED above), so this\n"
               "            run does not prove the tree is green.  Exiting 2.\n", nSkipped);
    fflush(stdout);

    return g_tf.failures ? 1 : (nSkipped ? 2 : 0);
}
