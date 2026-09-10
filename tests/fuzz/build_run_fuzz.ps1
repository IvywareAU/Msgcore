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


    build_run_fuzz.ps1 -- build and run the ASan + libFuzzer harness over
    P2PmsgMgr::Load. The release-readiness register, item 14, fourth job.

        pwsh tests\fuzz\build_run_fuzz.ps1                  # build, fuzz 60s
        pwsh tests\fuzz\build_run_fuzz.ps1 -Seconds 3600
        pwsh tests\fuzz\build_run_fuzz.ps1 -BuildOnly
        pwsh tests\fuzz\build_run_fuzz.ps1 -Repro out\fuzz\artifacts\crash-abc123

        pwsh tests\fuzz\build_run_fuzz.ps1 -Harness recv_image
        pwsh tests\fuzz\build_run_fuzz.ps1 -Harness recv_image -Seconds 3600

    NOTES: No clang-cl. The plan in item 14 assumed clang-cl was needed for
           -fsanitize=address,fuzzer, and it is not: VS 2026's own MSVC ships
           both, including clang_rt.fuzzer_MD-x86_64.lib, so `cl /fsanitize=
           address /fsanitize=fuzzer` links a libFuzzer binary with the same
           compiler the rest of the tree uses. That matters more than saving a
           download -- this is an MFC extension library, and MFC under clang-cl
           is a fight nobody needs to have to fuzz a parser.

         : ReleaseLib, not DebugLib. Two reasons, one of them uncomfortable.
           ASan is incompatible with /RTC1, which the Debug configurations set.
           And Release is the configuration a consumer actually links -- which
           also means the 277 ASSERT() sites that do structural validation are
           compiled out, so this fuzzes the binary as shipped rather than the
           one with the checks in it. See item 19.

         : The vendored MFC DLL and the ASan runtime DLL both have to be found
           at RUN time, not just link time. clang_rt.asan_dynamic-x86_64.dll
           lives in the MSVC toolchain's bin directory and is not on a default
           PATH, so a harness launched outside a developer prompt dies with
           STATUS_DLL_NOT_FOUND (0xC0000135) and no message whatsoever. Every
           invocation below therefore goes through vcvars64.

         : Two corpus directories, deliberately. libFuzzer writes newly
           interesting inputs into the FIRST corpus directory it is given, so
           passing the tracked tests\fuzz\corpus alone would have every run
           dirty the working tree with hundreds of unreviewed files. The
           writable one is out\fuzz\work; the tracked one is passed read-only
           after it. Promote from work to corpus by hand, deliberately.

         : Two harnesses, ONE shared seed corpus. -Harness selects which:
           `iomage_load` (default) drives P2PmsgMgr::Load; `recv_image` drives
           the receive path -- building a heap from an in-memory image and then
           traversing it, which is where F4-F7 lived and which nothing in this
           repository could fuzz before. Both take a raw image as their input
           and nothing else, so tests\fuzz\corpus serves both and either one's
           discoveries are legitimate seeds for the other. What is NOT shared is
           out\fuzz\work and out\fuzz\artifacts: those are per-harness, because
           a crash artifact only means something next to the harness that
           produced it, and one shared work directory would have each harness
           re-deriving the other's coverage-uninteresting inputs.
#>
[CmdletBinding()]
param(
    [int]$Seconds = 60,
    [int]$MaxLen  = 65536,
    [switch]$BuildOnly,
    [switch]$SkipBuild,
    # Keep going past a crash and collect distinct ones, rather than stopping at
    # the first. Uses libFuzzer's fork mode. This is how the finding list in
    # The release-readiness register was produced.
    [switch]$Triage,
    # Re-run one artifact and print its report.
    [string]$Repro,
    # Which entry point to fuzz. See the NOTES above.
    [ValidateSet('iomage_load','recv_image')]
    [string]$Harness = 'iomage_load'
)

$ErrorActionPreference = 'Stop'

$repo = (git rev-parse --show-toplevel).Trim()
Set-Location $repo

$outDir    = 'out\fuzz'
# Per-harness work, artifacts and run directories; one shared seed corpus. See
# the two-harnesses note above for why the split falls exactly there.
$workDir   = "$outDir\work\$Harness"
$artifacts = "$outDir\artifacts\$Harness"
$runDir    = "$outDir\run\$Harness"
$seedDir   = 'tests\fuzz\corpus'
$source    = "tests\fuzz\fuzz_$Harness.cpp"
$exe       = "$outDir\fuzz_$Harness.exe"

if (-not (Test-Path $source)) { Write-Error "no harness source at $source"; exit 1 }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Write-Error 'vswhere.exe not found -- install VS 2026 with the C++ workload'; exit 1 }
$vsdir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsdir) { Write-Error 'no VS 2026 C++ toolset found'; exit 1 }
$vcvars = Join-Path $vsdir 'VC\Auxiliary\Build\vcvars64.bat'

foreach ($d in @($outDir, $workDir, $artifacts, $runDir)) { New-Item -ItemType Directory -Force $d | Out-Null }

# Native tools write to stderr in the ordinary course of business; without this
# pwsh turns the first such line into a terminating error and the exit code we
# actually care about is never read.
$PSNativeCommandUseErrorActionPreference = $false

function Invoke-InVcvars([string]$cmdline) {
    # Out-Host, not a bare call: everything a PowerShell function writes to the
    # pipeline becomes part of its return value, so without this the compiler's
    # own output is prepended to the exit code and `(Invoke-InVcvars $x) -ne 0`
    # compares an array -- which filters rather than tests, and is truthy the
    # moment cl.exe prints anything at all. A clean build then "fails".
    cmd /c "call `"$vcvars`" >nul 2>&1 && $cmdline" | Out-Host
    return $LASTEXITCODE
}

# ------------------------------------------------------------------ build ----

if (-not $SkipBuild -and -not $Repro) {
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { Write-Error 'MSBuild not found'; exit 1 }

    Write-Host '=== BUILD: Msgcore ReleaseLib|x64 with ASan + coverage ==='

    # Coverage instrumentation on the LIBRARY, which is the whole game. Without
    # it libFuzzer sees only the harness translation unit -- 640 counters, no
    # feedback from inside the parser -- and degrades to random mutation of the
    # seeds. With it: 15,960 counters and a corpus that actually grows. The
    # first measured run went from ft:24 to ft:1946 on the same inputs.
    #
    # Delivered through the CL environment variable rather than an MSBuild
    # property because AdditionalOptions is ClCompile ITEM metadata, so
    # /p:AdditionalOptions=... on the command line goes nowhere. cl.exe reads CL
    # itself and prepends it to every invocation, which reaches every TU in the
    # project without editing the vcxproj -- and editing the vcxproj is the one
    # thing this job must not do, since the eight configurations it defines are
    # what job 1 checks against a baseline.
    $savedCL = $env:CL
    $env:CL = '/fsanitize-coverage=inline-8bit-counters /fsanitize-coverage=edge ' +
              '/fsanitize-coverage=trace-cmp /fsanitize-coverage=trace-div'
    try {
        & $msbuild 'Msgcore(2026).vcxproj' /nologo /m /v:minimal /clp:NoSummary `
            /p:Configuration=ReleaseLib /p:Platform=x64 /p:EnableASAN=true `
            /p:IntDir="$repo\$outDir\obj\" /p:OutDir="$repo\$outDir\"
    }
    finally { $env:CL = $savedCL }

    if ($LASTEXITCODE -ne 0) { Write-Error "ASan library build failed ($LASTEXITCODE)"; exit 1 }
    if (-not (Test-Path "$outDir\Msgcore.lib")) { Write-Error "no $outDir\Msgcore.lib after a successful build"; exit 1 }

    Write-Host ''
    Write-Host '=== BUILD: harness with /fsanitize=address /fsanitize=fuzzer ==='
    # The extra libraries are the ones LINKAGE.md lists for static consumers: an
    # archive carries no import records, so the DLL's dependencies become ours.
    $cl = "cl /nologo /EHsc /MD /std:c++17 /Zc:wchar_t /O1 /Zi" +
          " /fsanitize=address /fsanitize=fuzzer" +
          " /DNDEBUG /D_CONSOLE /D_UNICODE /DUNICODE /D_AFXDLL /DMsgcore_STATIC /D_WIN32_WINNT=0x0603" +
          " /I. /Fo$outDir\obj\ /Fd$outDir\fuzz_$Harness.pdb" +
          " $source" +
          " /link /LIBPATH:$outDir Msgcore.lib MsWsock.lib ws2_32.lib comsuppw.lib Propsys.lib" +
          " /OUT:$exe"
    if ((Invoke-InVcvars $cl) -ne 0) { Write-Error 'harness build failed'; exit 1 }
}

if (-not (Test-Path $exe)) { Write-Error "$exe not built"; exit 1 }
if ($BuildOnly) { Write-Host ''; Write-Host "Built $exe."; exit 0 }

# ------------------------------------------------------------------- run -----

if ($Repro) {
    $full = (Resolve-Path $Repro).Path
    Write-Host "=== REPRO: $full ($Harness) ==="
    # Four levels, not three: $runDir gained a per-harness leaf.
    $rc = Invoke-InVcvars "cd $runDir && ..\..\..\..\$exe `"$full`""
    exit $rc
}

$seeds = (Get-ChildItem $seedDir -File | Measure-Object).Count
Write-Host ''
Write-Host "=== FUZZ [$Harness]: ${Seconds}s, max_len=$MaxLen, $seeds seed(s), $(if ($Triage) {'fork/triage'} else {'stop on first crash'}) ==="

$common = "-max_total_time=$Seconds -max_len=$MaxLen -rss_limit_mb=4096 -print_final_stats=1 -artifact_prefix=$repo\$artifacts\"
if ($Triage) {
    $fuzzArgs = "-fork=1 -ignore_crashes=1 -ignore_timeouts=1 -ignore_ooms=1 $common"
} else {
    $fuzzArgs = $common
}

$log = "$outDir\fuzz_$Harness.log"
cmd /c "call `"$vcvars`" >nul 2>&1 && cd $runDir && ..\..\..\..\$exe $repo\$workDir $repo\$seedDir $fuzzArgs 2>&1" |
    Tee-Object -FilePath $log
$rc = $LASTEXITCODE

# Did the library actually get instrumented? If the CL trick above ever stops
# reaching cl.exe -- a toolchain change, an MSBuild that scrubs the environment
# -- libFuzzer still runs, still reports a tidy "no crash", and is worth almost
# nothing, because it can no longer see inside the parser it is fuzzing. That is
# a wrong green of exactly the kind the warning-count double-counting was, and
# it is invisible unless something counts. The harness TU alone is 640 counters;
# the whole library was 15,960 when this was written.
$counters = 0
if (Test-Path $log) {
    $m = Select-String -Path $log -Pattern '\((?<n>\d+) inline 8-bit counters\)' | Select-Object -First 1
    if ($m) { $counters = [int]$m.Matches[0].Groups['n'].Value }
}
$minCounters = 8000
if ($counters -lt $minCounters) {
    Write-Host ''
    Write-Host "  COVERAGE INSTRUMENTATION MISSING: $counters inline 8-bit counters, expected >= $minCounters."
    Write-Host '  libFuzzer is running blind -- no feedback from inside Msgcore, so it is'
    Write-Host '  mutating the seeds at random and a clean run proves nothing. Check that'
    Write-Host '  the CL environment variable is reaching cl.exe in the library build.'
    if ($env:GITHUB_ACTIONS) { Write-Host "::error::fuzz target has only $counters coverage counters; the library is not instrumented" }
    exit 1
}
Write-Host ''
Write-Host "Coverage: $counters inline 8-bit counters instrumented."

Write-Host ''
$found = @(Get-ChildItem $artifacts -Filter 'crash-*' -ErrorAction SilentlyContinue)
$hangs = @(Get-ChildItem $artifacts -Filter 'timeout-*' -ErrorAction SilentlyContinue)
# oom-* was not counted here until F10, and the omission had teeth: libFuzzer
# writes an oom- artifact and exits 71, so the run reported "0 crash, 0 timeout"
# and then died with "exited 71 with NO ARTIFACT" while the artifact sat in this
# very directory. Memory exhaustion driven by wire data is a finding -- F10 is
# exactly that -- and a runner that can only recognise two of libFuzzer's three
# artifact kinds reports the third as a mystery exit code.
$ooms  = @(Get-ChildItem $artifacts -Filter 'oom-*'     -ErrorAction SilentlyContinue)
Write-Host "Artifacts in ${artifacts}: $($found.Count) crash, $($hangs.Count) timeout, $($ooms.Count) oom."

if ($found.Count -or $hangs.Count -or $ooms.Count) {
    Write-Host ''
    Write-Host '  A crash here is a finding, not a flake. Reproduce one with:'
    Write-Host "      pwsh tests\fuzz\build_run_fuzz.ps1 -Harness $Harness -Repro $artifacts\<name>"
    Write-Host ''
    Write-Host '  Note that a refusal -- Load returning FALSE, or any of these entry points'
    Write-Host '  throwing P2Pevent* -- is NOT a crash; the harness swallows both, and'
    Write-Host '  refusing a malformed image is the correct behaviour. What reached this'
    Write-Host '  point is memory unsafety.'
    if ($ooms.Count) {
        Write-Host ''
        Write-Host '  An oom- artifact is an ALLOCATION driven past the limit, not a leak in'
        Write-Host '  the harness. Re-run the reproducer with -malloc_limit_mb=256 to turn'
        Write-Host '  "the process grew" into a stack naming the single allocation; that is'
        Write-Host '  how F10 was identified.'
    }
    if ($env:GITHUB_ACTIONS) {
        Write-Host "::error::[$Harness] libFuzzer produced $($found.Count) crash, $($hangs.Count) timeout and $($ooms.Count) oom artifact(s) over ${Seconds}s"
    }
    exit 1
}

if ($rc -ne 0 -and -not $Triage) { Write-Error "libFuzzer exited $rc with no artifact"; exit $rc }

Write-Host ''
Write-Host "OK -- ${Seconds}s with no crash, hang or ASan report."
exit 0
