<#
    Copyright 2026 Khrustal & Mann
                 MELBOURNE, VICTORIA, AUSTRALIA, 3000

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
    implied. See the License for the specific language governing
    permissions and limitations under the License.


    check_asserts.ps1 -- count the ASSERT-family sites by form and hold the
    count against a tracked baseline.

    the release-readiness register, item 19. C4's root lesson was that structural
    validation living inside ASSERT(...) compiles out of Release, so the check
    is absent from every shipped binary. CONTRIBUTING.md names the pattern as
    reject-on-sight for new code, which stops the count growing only as fast as
    reviewers notice. This measures it.

    THREE FORMS, because they fail differently and only one of them is the
    interesting one:

      marker      ASSERT(0)          -- "unreachable". Release loses a claim
                                        nobody was checking anyway. Mostly
                                        harmless, occasionally a lie (see
                                        P2PmsgHeap_ResizeIOMAGE1, which opened
                                        with one and then did ninety lines of
                                        real work; deleted in item 19).

      predicate   ASSERT(a == b)     -- Release loses the test. The values are
                                        still computed if they have side
                                        effects, which they usually do not.

      callwrap    ASSERT(f(x))       -- THE WORST ONE, and the reason this
                                        script exists. Release does not run a
                                        weakened version of f, it does not call
                                        f AT ALL. A whole structural walk can
                                        sit behind one of these -- and did:
                                        P2PmsgHeap_AssertVBlocksBSTRio was
                                        invoked this way on the load path, so
                                        the block-chain validation that F1's
                                        neighbourhood depends on never executed
                                        in a shipped binary.

    A NOTE ON THE HEADLINE NUMBER. Earlier revisions of the release-readiness register
    reported "277 ASSERT(0) call sites". Counted here, live ASSERT(0) markers
    are 250 and it is the CALLWRAP form that numbers 277. The 277 was real; it
    was measuring the more serious category and calling it the other one.

    SIX OF THE `predicate` COUNT ARE NOT ASSERTIONS, and the figure in
    assert-baseline.txt has carried them since the day it was banked. PowerShell's
    -match is case-INSENSITIVE unless you spell it -cmatch, so the classifier's
    \b(?:ASSERT|P2PASSERT)\s*\( also matches `->Assert()` -- P2Pevent's fluent
    builder, six call sites, all of them in MsgVBHeap.cpp (2525, 2545, 2625, 2644,
    3916, 3973 as of 46a63f8). The inner text after the paren is `)->Throw();`, which
    is neither `0)` nor an identifier, so each lands in `predicate`. They are the
    OPPOSITE of what this script is against: every one sits on a branch that ends
    in ->Throw() or ->Cancel(), which is item 19's own prescription -- assert in
    Debug, refuse in Release. Worse, P2Pevent::Assert is `{ ASSERT(0); return this; }`
    (Msgexception.h:353), already counted once as the `marker` it is, so the six are
    a second count of one site. Read the banked predicate figure as six high.
    NOT FIXED HERE. Spelling it -cmatch drops the number by six in the same breath
    as the stale-baseline re-bank, and two corrections landing in one figure is how
    a baseline stops meaning one thing. It wants its own commit and its own reason.

    WHAT THIS CANNOT DO. Decide whether a given site is load-reachable. That is
    a call-graph question and this is a grep; the baseline stops the counts
    growing, and reducing them is manual triage. Directionality is the point:
    a rising callwrap count is a regression, a falling one is progress.

    Usage:
        pwsh tools/ci/check_asserts.ps1
        pwsh tools/ci/check_asserts.ps1 -Detail callwrap
        pwsh tools/ci/check_asserts.ps1 -Regenerate
#>
[CmdletBinding()]
param(
    # List every site of one form, with file:line. For doing the triage rather
    # than for checking it.
    [ValidateSet('marker', 'predicate', 'callwrap')][string]$Detail,

    # Rewrite the baseline. Do this in the same commit as the change that moved
    # the numbers, and only ever downward without a stated reason.
    [switch]$Regenerate
)

$ErrorActionPreference = 'Stop'

$repo = (git rev-parse --show-toplevel).Trim()
Set-Location $repo

$baselinePath = 'tools/ci/assert-baseline.txt'

# tests/ asserts are the test's own scaffolding and carry none of the shipped-binary
# risk. Platform/ is excluded for a different reason, and not the old one: it was
# excluded as upstream's tree while vendored, dropped when it became a sibling
# repository, and is back in this tree since 2026-09-03 with no upstream left. The
# reason now is that the shim layer's only ASSERT-family match is the mfcshim.h line
# that DEFINES the macro on Linux -- `#define ASSERT(expr) assert(expr)` -- which the
# regex cannot tell from a call and would bank as a `predicate` site. Counting a macro
# definition as an assertion makes the number mean less, and this baseline is only
# worth having while it means one thing. The baseline is unaffected either way: those
# files have never been counted in it.
$files = @(git ls-files '*.cpp' '*.h') |
            Where-Object { $_ -notlike 'Platform/*' -and $_ -notlike 'tests/*' }

$sites = [System.Collections.Generic.List[object]]::new()
foreach ($f in $files) {
    $n = 0
    foreach ($line in (Get-Content -LiteralPath $f)) {
        $n++
        $t = $line.Trim()
        if ($t -notmatch '\b(?:ASSERT|P2PASSERT)\s*\(') { continue }
        if ($t.StartsWith('//')) { continue }        # commented out: not in any build

        $inner = ''
        if ($t -match '\b(?:ASSERT|P2PASSERT)\s*\((?<a>.*)') { $inner = $Matches.a }

        $kind = if ($inner -match '^\s*0\s*\)') { 'marker' }
                elseif ($inner -match '^\s*[A-Za-z_][A-Za-z0-9_:]*\s*\(') { 'callwrap' }
                else { 'predicate' }

        $sites.Add([pscustomobject]@{ File = $f; Line = $n; Kind = $kind; Text = $t })
    }
}

$counts = [ordered]@{}
foreach ($k in 'callwrap', 'marker', 'predicate') {
    $counts[$k] = @($sites | Where-Object { $_.Kind -eq $k }).Count
}

Write-Host "Live ASSERT-family sites (tracked sources, excluding Platform/ and tests/):"
foreach ($k in $counts.Keys) { Write-Host ("  {0,-10} {1}" -f $k, $counts[$k]) }
Write-Host ("  {0,-10} {1}" -f 'TOTAL', $sites.Count)

if ($Detail) {
    Write-Host ''
    Write-Host "--- $Detail sites ---"
    $sites | Where-Object { $_.Kind -eq $Detail } |
        ForEach-Object { Write-Host ("  {0}:{1}  {2}" -f $_.File, $_.Line, $_.Text) }
    exit 0
}

if ($Regenerate) {
    $lines = @(
        '# ASSERT-family baseline. Generated by tools/ci/check_asserts.ps1 -Regenerate.'
        '#'
        '# These are ceilings, not targets. CI fails when a count RISES; a fall is a'
        '# notice and should be banked here in the same commit that earned it, or the'
        '# stale ceiling silently re-admits everything between the two numbers.'
        '#'
        '# See the release-readiness register, item 19 for what each form costs in Release.'
        ''
    ) + @($counts.Keys | ForEach-Object { '{0} {1}' -f $_, $counts[$_] })
    Set-Content -Path $baselinePath -Value $lines -Encoding utf8NoBOM
    Write-Host ''
    Write-Host "Rewrote $baselinePath."
    exit 0
}

if (-not (Test-Path $baselinePath)) { Write-Error "missing $baselinePath (-Regenerate)"; exit 2 }

$expected = @{}
foreach ($line in (Get-Content $baselinePath)) {
    $t = $line.Trim()
    if (-not $t -or $t.StartsWith('#')) { continue }
    if ($t -match '^(?<k>\w+)\s+(?<v>\d+)$') { $expected[$Matches.k] = [int]$Matches.v }
}

$failed = $false
Write-Host ''
foreach ($k in $counts.Keys) {
    if (-not $expected.ContainsKey($k)) { Write-Host "  $k : no baseline entry"; continue }
    $was = $expected[$k]; $now = $counts[$k]
    if ($now -gt $was) {
        $failed = $true
        $msg = "$k ASSERT sites rose from $was to $now"
        Write-Host "  $msg"
        if ($env:GITHUB_ACTIONS) { Write-Host "::error file=$baselinePath::$msg" }
    }
    elseif ($now -lt $was) {
        Write-Host "  $k fell from $was to $now -- bank it: -Regenerate in this commit."
        if ($env:GITHUB_ACTIONS) { Write-Host "::notice::$k ASSERT sites fell from $was to $now" }
    }
    else { Write-Host "  $k at baseline ($now)." }
}

if ($failed) {
    Write-Host ''
    Write-Host '  A new ASSERT around structural validation is a check that will not be in'
    Write-Host '  the binary anyone ships. If the condition matters in Release, throw as the'
    Write-Host '  C4 gate does. If it does not, it does not need asserting either.'
    Write-Host '  CONTRIBUTING.md names this as reject-on-sight.'
    exit 1
}
Write-Host ''
Write-Host 'OK -- no ASSERT-family form has grown.'
exit 0
