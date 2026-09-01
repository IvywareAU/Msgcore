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


    check_exports.ps1 -- fail if Msgcore.dll's exported surface has drifted from
    the manifests that declare it.

    the release-readiness register, item 11. The README already declares WHICH surface is
    supported -- the flat C ABI is public, the mangled C++ half is toolchain-
    pinned and internal. This is the half that makes the declaration enforceable
    rather than aspirational: a surface nobody measures widens by accident, one
    export at a time, and each accidental export is a promise somebody may link
    against and that 1.x then has to keep.

    THREE MANIFESTS, NOT ONE, because the surface genuinely has three parts and
    collapsing them would hide the distinction that matters.

        exports-flat.manifest        282 flat msgcore_* names. THE SUPPORTED
                                     SURFACE. Measured identical on x64 and
                                     Win32 -- so it is checked against both, and
                                     that check is also what keeps it portable.
                                     A change here is an ABI change to the thing
                                     consumers are told to use: it bumps
                                     Msgcore_version.h in the same commit.

        exports-cxx-x64.manifest     700 mangled C++ names per platform. The
        exports-cxx-win32.manifest   INTERNAL surface -- whole-class MFC
                                     extension-DLL export, pinned to the exact
                                     MSVC toolset, MFC version, CRT model and
                                     _ITERATOR_DEBUG_LEVEL. Per platform because
                                     the two manglings share nothing: the same
                                     982 exports differ in 1340 names between
                                     x64 and Win32, which is the ABI difference
                                     showing through and not a defect.

    WHAT IS RECORDED, and what deliberately is not. Names only, sorted. Not the
    RVA, which changes on every build. Not the ordinal, which the linker assigns
    in name order with no .def to pin it, so inserting one export would renumber
    every export after it and turn a one-line change into a 900-line diff for a
    number nothing links by. Not the demangled spelling, which is derived: it is
    reconstructed with undname only when a diff has to be reported, where it is
    the difference between "??0MsgStck@@QEAA@XZ appeared" and a sentence a
    reviewer can act on.

    THE FOURTH MANIFEST, which this check found rather than assumed -- and which
    item 10 has since retired.

        exports-cxx-win32-sdl.manifest   22 __autoclassinit2 members, added to
                                         the Win32 expectation for Debug ONLY.
                                         REGENERATED EMPTY AND DELETED by item 10.

    The surface was expected to be configuration-independent, because Debug|x64
    and Release|x64 exported identical name sets. Win32 did not: Debug|Win32
    exported 1006 where Release|Win32 exported 984. The 22 extra were
    ?__autoclassinit2@<class>@@QAEXI@Z, one per exported class -- compiler-
    generated member initialisers that /sdl emits and that whole-class export
    then exports along with everything else. SDLCheck was set on Debug|Win32 and
    DebugLib|Win32 and nowhere else, which was exactly the configuration
    inconsistency the release-readiness register, item 10 existed to remove.

    So the ABI of this library was not one ABI, it was a function of the switch
    set, and nobody had measured that before this check ran.

    ITEM 10 HAS LANDED, AND THIS IS THE FILE THAT PROVED IT. All eight
    configurations now compile at /W4, /std:c++17, /std:c17 and /sdl. Because
    /sdl is on everywhere, the 22 synthesised members are emitted by every
    configuration and therefore belong in the BASE manifests rather than in a
    supplement: win32 and x64 each declare 724 mangled names where they declared
    702, and all four DLL configurations export 1006 where three of them exported
    984. The supplement regenerated EMPTY -- the observable proof item 10 asked
    for, rather than an assertion that the edit was made -- and it has been
    deleted along with the code that read it.

    -Configuration IS KEPT rather than removed. It no longer selects an
    expectation, because there is now only one. It records which configuration
    produced the DLL being measured, and CI still passes all four, so
    "the exported surface does not depend on the switch set" is a claim re-earned
    on every push instead of one that was true on the day it was checked. If a
    future switch change re-splits the table, the four runs disagree and this
    check fails -- which is the whole reason not to collapse it to one.

    WHAT IT CANNOT SEE. That an export is USED -- the manifest says the surface
    has not shifted, never that it is the right surface. Narrowing it is item
    11's other half and belongs to a 2.0.0.

    Usage:
        pwsh tools/ci/check_exports.ps1 -Dll out\x64\Release\Msgcore.dll -Platform x64 -Configuration Release
        pwsh tools/ci/check_exports.ps1 -Dll out\Win32\Debug\Msgcore.dll  -Platform Win32 -Configuration Debug
        pwsh tools/ci/check_exports.ps1 -Dll ... -Platform x64 -Configuration Release -Regenerate
#>
[CmdletBinding()]
param(
    # The built Msgcore.dll to measure. The Lib configurations produce a static
    # library and no DLL, so only Debug|<plat> and Release|<plat> have one.
    [Parameter(Mandatory)][string]$Dll,

    # Which mangled-name manifest to compare against. The flat manifest is
    # compared against both, which is what keeps the two platforms in step.
    [Parameter(Mandatory)][ValidateSet('x64', 'Win32')][string]$Platform,

    # Which configuration built $Dll. Since item 10 unified the switch set, no
    # configuration differs from any other and this selects nothing; it is kept
    # because CI measures all four and a disagreement between them is now a
    # finding rather than the expected state. Required rather than sniffed from
    # the path, so a caller cannot mislabel a run by renaming a directory.
    [Parameter(Mandatory)][ValidateSet('Debug', 'Release')][string]$Configuration,

    # Rewrite the manifests from this DLL. A deliberate export change updates
    # them in the same commit as the change and the version bump.
    [switch]$Regenerate
)

$ErrorActionPreference = 'Stop'

$repo = (git rev-parse --show-toplevel).Trim()
Set-Location $repo

$flatManifest = 'tools/ci/exports-flat.manifest'
$cxxManifest  = "tools/ci/exports-cxx-$($Platform.ToLowerInvariant()).manifest"

# There is no /sdl supplement any more; item 10 put /sdl on all eight
# configurations, so its 22 synthesised members are in the base manifests and
# exports-cxx-win32-sdl.manifest regenerated empty and was deleted. See header.

$failed = $false
function Fail([string]$file, [string]$msg) {
    $script:failed = $true
    Write-Host "  $msg"
    if ($env:GITHUB_ACTIONS) { Write-Host "::error file=$file::$msg" }
}

# ------------------------------------------------------------- the tools -----

function Find-VCTool([string]$name) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { Write-Error "vswhere.exe not found at $vswhere"; exit 2 }
    $vc = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
    if (-not $vc) { Write-Error 'vswhere found no Visual Studio installation.'; exit 2 }
    $tool = Get-ChildItem -Path (Join-Path $vc 'VC\Tools\MSVC') -Recurse -Filter $name -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match 'Hostx64\\x64' } | Select-Object -First 1
    if (-not $tool) { Write-Error "$name not found under $vc"; exit 2 }
    return $tool.FullName
}

if (-not (Test-Path $Dll)) {
    Write-Error "no such DLL: $Dll (build Debug|$Platform or Release|$Platform first)"
    exit 2
}

$dumpbin = Find-VCTool 'dumpbin.exe'

# ------------------------------------------------------------ measure it -----

# dumpbin's export rows are:   <ordinal> <hint> <RVA> <name>[ = <target> (<demangled>)]
# The RVA column is absent for a forwarded export, which Msgcore has none of --
# but matching it loosely rather than assuming it keeps the parse honest if one
# ever appears, instead of silently dropping the row and under-counting.
$names = [System.Collections.Generic.List[string]]::new()
$demangled = @{}
$rows = 0
foreach ($line in (& $dumpbin /nologo /exports $Dll)) {
    if ($line -match '^\s+(?<ord>\d+)\s+(?<hint>[0-9A-Fa-f]+)\s+(?:(?<rva>[0-9A-Fa-f]{8})\s+)?(?<name>\S+)') {
        $rows++
        # Capture before the next -match: a successful match REPLACES $Matches
        # wholesale, so reading .name after the demangle match reads null.
        $name = $Matches.name
        $names.Add($name)
        if ($line -match '\((?<d>.+)\)\s*$') { $demangled[$name] = $Matches.d }
    }
}

if ($rows -eq 0) {
    Write-Error "parsed no exports out of $Dll -- dumpbin output format changed, or this is not an export-bearing DLL"
    exit 2
}

$all  = @($names | Sort-Object -Unique)
$flat = @($all | Where-Object { $_ -like 'msgcore_*' })
$cxx  = @($all | Where-Object { $_ -notlike 'msgcore_*' })

Write-Host "$Dll ($Platform): $($all.Count) exports -- $($flat.Count) flat msgcore_*, $($cxx.Count) mangled C++"

# ------------------------------------------------------------ regenerate -----

function Write-Manifest([string]$path, [string[]]$symbols, [string[]]$preamble) {
    $lines = @($preamble) + @('') + @($symbols)
    Set-Content -Path $path -Value $lines -Encoding utf8NoBOM
    Write-Host "Rewrote $path ($($symbols.Count) symbols)."
}

function Read-Manifest([string]$path) {
    if (-not (Test-Path $path)) { Write-Error "missing $path (regenerate it: -Regenerate)"; exit 2 }
    @(Get-Content $path | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') } | Sort-Object -Unique)
}

if ($Regenerate) {
    # The flat surface is platform-independent and both platforms regenerate it.
    # If it already exists and this DLL disagrees, that divergence IS the finding
    # -- overwriting it would destroy the evidence and hand back a green tick.
    if (Test-Path $flatManifest) {
        $prior = Read-Manifest $flatManifest
        $d = @(Compare-Object $prior $flat)
        if ($d.Count) {
            Write-Error "$flatManifest already declares a DIFFERENT flat surface ($($prior.Count) symbols) than $Dll exports ($($flat.Count)). The flat ABI is supposed to be identical on x64 and Win32; this says it is not. Resolve that before regenerating."
            exit 2
        }
    }

    Write-Manifest $flatManifest $flat @(
        '# Msgcore exported surface -- the flat C ABI. THE SUPPORTED SURFACE.'
        '#'
        '# Generated by tools/ci/check_exports.ps1 -Regenerate. Names only, sorted.'
        '# Platform-independent: x64 and Win32 export this set identically, and CI'
        '# checks it against both, which is what keeps that true.'
        '#'
        '# A change to this file is an ABI change to the surface consumers are told'
        '# to use. It bumps Msgcore_version.h in the same commit -- additive is a'
        '# minor, removal or a signature change is a major.'
    )
    Write-Manifest $cxxManifest $cxx @(
        "# Msgcore exported surface -- the mangled C++ half, $Platform. INTERNAL."
        '#'
        '# Generated by tools/ci/check_exports.ps1 -Regenerate. Names only, sorted.'
        '# Whole-class MFC extension-DLL export. Every consumer of this half is'
        '# pinned to the exact MSVC toolset, MFC version, CRT model and'
        '# _ITERATOR_DEBUG_LEVEL that built the DLL; see LINKAGE.md.'
        '#'
        '# Per platform because the two manglings share nothing -- this file and its'
        '# x64/Win32 twin describe the same declarations and agree on almost no name.'
        '#'
        '# Policy (the release-readiness register, item 11): this half stays through 1.x.'
        '# Unexporting it is an ABI break and belongs to a 2.0.0. Tracked here so it'
        '# cannot widen silently, not because it is supported.'
    )
    exit 0
}

# --------------------------------------------------------------- compare -----

function Compare-Surface([string]$label, [string]$path, [string[]]$actual, [string]$policy) {
    $expected = @(Read-Manifest $path) | Sort-Object -Unique
    $added   = @($actual   | Where-Object { $_ -notin $expected })
    $removed = @($expected | Where-Object { $_ -notin $actual })

    if (-not $added -and -not $removed) {
        Write-Host "  OK -- $label matches $path ($($expected.Count) symbols)."
        return
    }

    Write-Host ''
    Write-Host "  $label drifted from ${path}:"
    foreach ($s in $added)   { Fail $path "EXPORT ADDED, undeclared: $s$(if ($demangled[$s]) { "  --  $($demangled[$s])" })" }
    foreach ($s in $removed) { Fail $path "EXPORT REMOVED, still declared: $s" }
    Write-Host "  $policy"
}

Write-Host ''
Compare-Surface 'flat C ABI' $flatManifest $flat `
    'This is the supported surface. If the change is intended, regenerate the manifest and bump Msgcore_version.h in the same commit.'
Compare-Surface "mangled C++ ($Platform)" $cxxManifest $cxx `
    'This half is internal but frozen through 1.x. An addition here is usually an accidental dllexport -- check whether the class or member meant to be exported at all. Since item 10 there is no per-configuration supplement: if THIS configuration disagrees with the manifest the other three match, the switch sets have diverged again.'

# ---------------------------------------------------------------- verdict -----

Write-Host ''
if ($failed) {
    Write-Host '  A surface nobody measures widens one export at a time, and every accidental'
    Write-Host '  export is a promise somebody may link against and that 1.x then has to keep.'
    Write-Host '  That is what this check exists to prevent -- so the fix is to decide whether'
    Write-Host '  the change was meant, not to regenerate the manifest until it goes green.'
    exit 1
}
Write-Host "OK -- exported surface matches the manifests: $($flat.Count) flat, $($cxx.Count) mangled."
exit 0
