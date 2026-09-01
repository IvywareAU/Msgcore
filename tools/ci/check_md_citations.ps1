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


    check_md_citations.ps1 -- fail if a tracked file points at a document the
    reader cannot open.

    the release-readiness register, item 14, hygiene job (a). B7 closed with the observation
    that every measurement of it had been taken by grepping for four documents
    somebody had thought to list, and that scanning for *any* .md citation instead
    found four more sites against two documents nobody had listed -- one of them
    in a shipped header, sending the reader to a file that was never published,
    for the rule that a live handle dies on a heap relocation.

    So this check is deliberately not keyed to a list of documents. It finds every
    .md token in every tracked text file and asks whether it resolves. A citation
    audit keyed to names you already know cannot find the citation you did not
    know about; that is the whole lesson of B7 and the reason this lives in CI.

    HOW A CITATION IS RESOLVED. Two attempts, and either one succeeds:

      1. relative to the citing file -- a "../"-prefixed form, which from a
         first-level subdirectory resolves to this repository's root
      2. relative to the repository root -- a bare filename cited from anywhere

    The second is not laxness. Most citations here are written as bare filenames
    in prose ("see LINKAGE.md"), which a reader resolves by looking in the obvious
    place, and the obvious place is the root. Requiring path-correct citations in
    a comment would fail honest text and teach people to silence the check.

    Comparison is CASE-SENSITIVE, because git is. This repository has Readme.md at
    the root and README.md inside tests/fuzz/ and tools/hooks/, so a check that
    folded case would accept a citation that resolves on Windows and 404s on the
    GitHub web view and on any Linux clone.

    THE ALLOWLIST. Some citations cannot resolve and should not. The published design
    notes name sibling MSCS components, which the README says outright is worth
    more than a stripped-down note. And the release-readiness register discusses unpublished
    documents as its subject matter rather than pointing at them.

    Each of those is one line in md-citations.allow with a reason. The list is
    keyed on (citing file, token as written), not on the document name, so
    allowing the internal, unpublished security review to be discussed in one
    document does not permit a new source file to start citing it.

    A stale entry -- one whose citation is gone -- is reported but does not fail,
    the same way a warning count under baseline is a notice rather than an error.
    Somebody cleaned something up; the list should shrink to match, and telling
    them so is more useful than breaking their build.

    Usage:
        pwsh tools/ci/check_md_citations.ps1
        pwsh tools/ci/check_md_citations.ps1 -Verbose   # also list what resolved
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repo = (git rev-parse --show-toplevel).Trim()
if (-not $repo) { Write-Error 'not inside a git repository'; exit 2 }
Set-Location $repo

$allowFile = 'tools/ci/md-citations.allow'

# The allowlist names the very tokens it exempts, so scanning it would report
# every one of them against itself. It is the only exclusion, and it is stated
# out loud in the report below rather than being a silent hole in the sweep.
$excluded = @($allowFile)

# ---------------------------------------------------------------- inputs ----

$tracked = [System.Collections.Generic.HashSet[string]]::new(
    [string[]](git ls-files | ForEach-Object { $_ -replace '\\', '/' }),
    [System.StringComparer]::Ordinal)

$allowed = @{}      # "file`0token" -> reason
$allowSeen = @{}    # same key -> did we actually meet this citation
if (Test-Path $allowFile) {
    $lineNo = 0
    foreach ($line in Get-Content $allowFile) {
        $lineNo++
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith('#')) { continue }
        # <citing file> -> <token as written>  [# reason]
        if ($t -notmatch '^(?<f>\S+)\s*->\s*(?<d>\S+?)\s*(?:#\s*(?<why>.*))?$') {
            Write-Error "${allowFile}:${lineNo}: cannot parse: $t"
            exit 2
        }
        $key = ($Matches.f -replace '\\', '/') + "`0" + $Matches.d
        $allowed[$key] = if ($Matches.why) { $Matches.why } else { '(no reason given)' }
        $allowSeen[$key] = $false
    }
}

# ------------------------------------------------------------ resolution ----

# Textual, not filesystem: we are asking whether git tracks the path, and the
# filesystem would answer a different and more forgiving question on Windows.
function Resolve-RepoPath([string]$dir, [string]$rel) {
    # A List, not an array: popping with $a[0..($a.Count-2)] silently returns the
    # WHOLE array when Count is 1, because 0..-1 counts backwards. So a "../"
    # citation in a Platform header resolved one level too DEEP instead of one
    # level up, and fifteen healthy citations were reported as broken.
    $parts = [System.Collections.Generic.List[string]]::new()
    $combined = if ($dir) { "$dir/$rel" } else { $rel }
    foreach ($seg in ($combined -split '/')) {
        if ($seg -eq '' -or $seg -eq '.') { continue }
        if ($seg -eq '..') {
            if ($parts.Count -eq 0) { return $null }   # escaped the repository
            $parts.RemoveAt($parts.Count - 1)
            continue
        }
        $parts.Add($seg)
    }
    if ($parts.Count -eq 0) { return $null }
    return ($parts -join '/')
}

$pattern = [regex]'[A-Za-z0-9_./\\-]*[A-Za-z0-9_-]\.md'

$violations = @()
$resolvedCount = 0
$scanned = 0

foreach ($file in $tracked) {
    if ($excluded -contains $file) { continue }

    $bytes = [System.IO.File]::ReadAllBytes((Join-Path $repo $file))
    # Binary files (the fuzz corpus, chiefly) would yield matches out of
    # whatever byte sequences happen to spell ".md".
    $probe = [Math]::Min($bytes.Length, 8000)
    if ($probe -gt 0 -and [Array]::IndexOf($bytes, [byte]0, 0, $probe) -ge 0) { continue }

    $scanned++
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    $dir  = [System.IO.Path]::GetDirectoryName($file) -replace '\\', '/'

    $seen = @{}
    $lineNo = 0
    foreach ($line in ($text -split "`r?`n")) {
        $lineNo++
        foreach ($m in $pattern.Matches($line)) {
            $token = $m.Value
            if ($seen.ContainsKey($token)) { continue }
            $seen[$token] = $true

            $norm = $token -replace '\\', '/'
            $asRelative = Resolve-RepoPath $dir  $norm
            $asRoot     = Resolve-RepoPath ''    $norm

            if (($asRelative -and $tracked.Contains($asRelative)) -or
                ($asRoot     -and $tracked.Contains($asRoot))) {
                $resolvedCount++
                continue
            }

            $key = "$file`0$token"
            if ($allowed.ContainsKey($key)) { $allowSeen[$key] = $true; continue }

            $violations += [pscustomobject]@{
                File = $file; Line = $lineNo; Token = $token
                Tried = @($asRelative, $asRoot | Where-Object { $_ } | Select-Object -Unique)
            }
        }
    }
}

# --------------------------------------------------------------- report -----

Write-Host "Scanned $scanned tracked text files; $resolvedCount citations resolved; $($allowed.Count) allowlisted."
Write-Host "Not scanned: $($excluded -join ', ') (names the tokens it exempts)."

$stale = $allowSeen.GetEnumerator() | Where-Object { -not $_.Value }
if ($stale) {
    Write-Host ''
    Write-Host "$($stale.Count) allowlist entr$(if ($stale.Count -eq 1) {'y'} else {'ies'}) no longer needed:"
    foreach ($s in $stale) {
        $f, $d = $s.Key -split "`0"
        Write-Host "    $f -> $d"
        if ($env:GITHUB_ACTIONS) { Write-Host "::notice file=$allowFile::stale allowlist entry: $f -> $d" }
    }
    Write-Host "  The citation is gone; drop the line from $allowFile."
}

if ($violations.Count -eq 0) {
    Write-Host ''
    Write-Host 'OK -- every .md citation resolves, or is allowlisted with a reason.'
    exit 0
}

Write-Host ''
Write-Host "$($violations.Count) citation$(if ($violations.Count -eq 1) {''} else {'s'}) point at a document that is not in the repository:"
Write-Host ''
foreach ($v in $violations) {
    Write-Host ("  {0}:{1}" -f $v.File, $v.Line)
    Write-Host ("      cites : {0}" -f $v.Token)
    Write-Host ("      tried : {0}" -f ($v.Tried -join '  and  '))
    if ($env:GITHUB_ACTIONS) {
        Write-Host "::error file=$($v.File),line=$($v.Line)::cites $($v.Token), which is not in the repository"
    }
}
Write-Host ''
Write-Host '  Three ways out, in order of preference:'
Write-Host '    1. Publish the document, if a reader needs it.'
Write-Host '    2. Reword the citation to describe the source rather than name a'
Write-Host '       file nobody can open -- what the SECURITY_REVIEW citations did.'
Write-Host "    3. If it must stay unfollowable, add a line to $allowFile"
Write-Host '       WITH A REASON. An entry without one is a silenced check.'
exit 1
