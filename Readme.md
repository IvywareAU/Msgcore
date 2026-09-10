# Msgcore

**A hierarchical, offset-addressed structured message store.**

[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](#requirements)
[![Windows](https://img.shields.io/badge/Windows-authoritative%20build-0078D6.svg)](#requirements)
[![Linux](https://img.shields.io/badge/Linux-Ported%20from%20Windows%2C%20not%20the%20reference-brightgreen.svg)](#requirements)
[![Since](https://img.shields.io/badge/since-2000-6f42c1.svg)](#msgcore)

A `P2PmsgMgr` owns a heap. Everything inside it — named fields, their values, their
children, their `@`-qualified attributes, and the list, vector, stack and cursor
containers over them — lives in that one heap and is addressed by **offset, not
pointer**. The whole store is therefore a single contiguous image that can be written to
a file and read back verbatim, with no serialisation pass and no pointer fix-up.

```cpp
#include "P2PmsgMgr.h"

P2PmsgMgr mgr;
mgr.r_name() = L"Settings";
mgr.r_Desc() += P3PmsgField ( L"window" );
mgr.Save ( L"settings.p2p" );          // atomic: temp file, then rename over the target

P2PmsgMgr load;
load.Load ( L"settings.p2p" );         // read-only, shared, header-validated
```

Msgcore is the core of the MSCS message-store family and dates to 2000. It ships as
either an MFC extension DLL or a static archive, and is reachable from C++, from C, and
from Java via Panama. COM/Automation hosts (VBScript, VBA, PowerShell, .NET) reach it
through the COM server, which lives in the `MsgFacade` repository rather than this one.

---

## Status

A fresh clone compiles on its own (see *The platform layer* below): all eight
configurations, plus the tests, with nothing else required — no sibling checkout, no
installed MSCS tree, no environment variables, no `AdditionalIncludeDirectories`. The
Apache-2.0 grant in `LICENSE` is made with the authority of all copyright holders named in
`NOTICE`.

**What this library has not got is a test suite.** One regression test covers the load
path, and the unit suites cover the container, allocator, addressing and flat-ABI surface
at 90 cases and 428 checks — which is real coverage of 38,000 lines of allocator,
container and parser code without being complete coverage of it. CI builds all eight
configurations on every push — the solution as well as the project, so a clone that is
opened in Visual Studio is covered and not merely one built from the command line — and
runs both test programs in both link modes, so the claim in the paragraph above is checked
continuously rather than asserted: every job assembles the two repositories by name into
fresh clones with `WDMSCS_*` cleared from the environment.

A weekly job fuzzes the load path under AddressSanitizer. Within ninety seconds of first
being switched on it found a heap-buffer-overflow in `P2PmsgMgr::Load` — a file whose
declared offsets point outside itself, dereferenced rather than rejected, on the one of
the two load branches that never received the 2026 bounds fix. **That is fixed** (F1 in
The release-readiness register); both branches now check the declared size against the real file,
and the reproducer is a regression case in the test suite.

**Still treat a `.dat` or `.iom` file from a source you do not control as unsafe to load.**
One out-of-bounds read found by ninety seconds of fuzzing has been closed, on a parser
that had never been fuzzed before that. That is one bug fixed, not a hardened path, and
`SECURITY.md` scopes it explicitly: Msgcore has not been reviewed as a network-facing
parser.
The thread-safety rule and the pointer-invalidation rule below are now also stated in
`Msgcore_c.h`, at the declaration groups they govern, so a consumer who reads only the
header still meets them.

So: read it, build it, evaluate it. Before you depend on it in production, read
**the release-readiness register** — it is the honest account of what is
verified, what is merely believed, which security findings are closed and which are not,
and in what order the rest gets fixed. It does not flatter this repository, which is why
it is the first thing to read if you are considering depending on it.

---

## Requirements

| | |
|---|---|
| Toolchain | Visual Studio 2026 (v145), "Desktop development with C++" — or, for Linux, GCC/Clang through the top-level CMake build |
| Libraries | MFC (shared/DLL) on Windows; the `Platform/` shim stands in for it on Linux |
| Platforms | Windows `x64` and `Win32`; Linux `x86-64` |
| Floor | Windows 8.1 (`_WIN32_WINNT=0x0603`) |
| Language | C++17 (`/std:c++17`) and C17 (`/std:c17`) — on all eight `.vcxproj` configurations **and under CMake**, on both platforms |

**The two build systems compiled this library at different language standards until
2026-08-22, and nothing guarded the gap.** The authoritative `Msgcore(2026).vcxproj` sets
`stdcpp17` in all eight configurations — deliberately, in `3f7bf5e` on 2026-08-15, because
"latest" is a moving target across toolset updates and a library that must build the same
way in five years should not have its language level chosen by whichever Visual Studio the
builder happens to have installed. `CMakeLists.txt` had said `cxx_std_23` since 2026-07-08,
five weeks *earlier*, and was never brought in line when that decision was taken.

**It was not a Linux-versus-Windows difference**, which is what made it easy to miss: the
CMake tree's own generated `msgcore.vcxproj` carried `stdcpplatest` against `stdcpp17` in
the authoritative project — same compiler, same sources, same toolset. And the direction was
the dangerous one. CMake was the *more permissive* of the two, so a C++20 or C++23 construct
added here would have compiled green under CMake on both platforms, passed every Linux gate,
and failed only when someone next built the `.sln`. That is the reverse of the usual worry,
where the new build system is the weaker one.

**Closed on 2026-08-22, and closing it took two files rather than one.** Setting
`CXX_STANDARD` on the Msgcore targets could not work on its own: `p2pplatform` declared
`INTERFACE cxx_std_23`, and an INTERFACE compile feature is a transitive *floor* on every
consumer, which overrode the lower standard on the target that linked it. The shim now
declares `INTERFACE cxx_std_17` — the floor its headers actually require — and keeps
`PRIVATE cxx_std_23` for its own translation units, so its freedom is no longer exported as
everyone's obligation. Msgcore then pins to 17 on both platforms.

Verified rather than assumed, because the permissive direction meant post-C++17 code could
already have been sitting in the tree unnoticed: `msgcore` and `msgcore_static` compile at
`-std=c++17` on GCC and `stdcpp17` under MSVC in both the DLL and the static presets, while
`targetcore` stays at `stdcpplatest`/`c++23` as its own project file specifies. Nothing in
this library needed a construct newer than C++17, so the fix cost no source changes.

All eight configurations carry the same diagnostic regime: `/W4`, `/std:c++17`,
`/std:c17` and `/sdl`. They did not always — x64 built at `/W3` with `stdcpplatest` and
`SDLCheck` was on for the two Win32 debug configurations only, so the same source
compiled under four regimes. Unifying them found two real defects and is why all four DLL
configurations now export an identical set of symbols where three of them exported fewer.
Win32 still emits more warnings than x64 under the identical regime, because the residue
is `C4244` truncations and that is a property of the pointer width rather than of the
switches.

## Building

Eight configurations, two shapes:

| Shape | Configurations | Output | Macro |
|---|---|---|---|
| MFC extension DLL | `Debug` / `Release` × `Win32` / `x64` | `Msgcore.dll` + import library | `Msgcore_EXPORTS` |
| Static archive | `DebugLib` / `ReleaseLib` × `Win32` / `x64` | `Msgcore.lib` | `Msgcore_STATIC` |

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild ".\Msgcore(2026).vcxproj" /p:Configuration=Debug /p:Platform=x64
```

Binaries land in `out\<Platform>\<Configuration>\`, intermediates in that directory's
`obj\`. The **import** library does not: the DLL configurations write it to
`$(WDMSCS_LIB)\<Platform>\<Configuration>\`, which `Directory.Build.props` defaults to
this repository's own `lib\`. Set `WDMSCS_LIB` yourself to stage it elsewhere — building
inside a full MSCS tree does exactly that, so the whole family shares one `MSCS\lib\`. The
static configurations copy their archive to the same place as a post-build step but also
leave it in `out\`.

> `Directory.Build.targets` fails the build if `WDMSCS_LIB` is ever empty or relative.
> `<ImportLibrary>` is `$(WDMSCS_LIB)\$(TargetName).lib`, so an empty value resolves
> against the drive root and `link.exe` writes the import library to `C:\` **without a
> warning** — a green build whose consumers then link a stale copy from somewhere else.

> **Read [`LINKAGE.md`](LINKAGE.md) before you link anything.** Every module in one
> process must reach Msgcore the same way — all importing one DLL, or exactly one module
> absorbing the archive. Mixing the two compiles, links, runs, and *then* corrupts data,
> because a `P2Pos` offset minted against one copy's heap resolves against the other's.
> There is no link error to warn you. That document explains the mechanism and gives the
> rule; it is the single most important page here.

## Which surface is supported

Msgcore.dll exports 1016 symbols, and that number is misleading. 282 of them are the flat
`msgcore_*` C functions; the other 734 are mangled C++ names, exported wholesale by
the MFC-extension-DLL model. The three figures are the manifests' as of `8682a0d`, and
they are the same on x64 and Win32 -- measured, not asserted: they are what
`check_exports.ps1` compares against, so a stale count here is a discrepancy the next
reader has to resolve rather than a rounding error.

| Surface | Header | Consumable by | Supported in 1.x |
|---|---|---|---|
| **Flat C ABI** | [`Msgcore_c.h`](Msgcore_c.h) | anyone — C, Java Panama, .NET P/Invoke, FUSE frontends | **Yes** |
| COM / Automation | moved to the `MsgFacade` repository (`c88f9f5`) | VBScript, VBA, PowerShell, .NET | **No — no longer part of this repository, and never security-reviewed** |
| C++ classes | `P2PmsgMgr.h`, `P2Pmsg.h`, … | **only code rebuilt with this exact toolset** | No — toolchain-pinned, internal |

**The flat C ABI is the only supported production surface in 1.x**, and the other two
rows are excluded for different reasons. The mangled C++ half pins every consumer to the
precise MSVC toolset, MFC version, CRT model and `_ITERATOR_DEBUG_LEVEL` that built the
DLL — that is a compatibility exclusion, and it has always been stated. The COM server is
excluded because it is **no longer here**: `c88f9f5` moved it to the `MsgFacade`
repository, where it sits on a layer that already solves the two problems it spent most of
its code on. It was never security-reviewed while it was here, and moving it did not
review it, so a reader who follows it to its new home should carry that with them. Nothing
in this repository builds or ships it.

If you are not compiling Msgcore yourself as part of your own build, use the flat C
header.

That row said *anyone — C, Java Panama, .NET P/Invoke, FUSE frontends* for a long time before
it was true of the first item on the list. Until 2026-08-19 `Msgcore_c.h` used `wchar_t` in
seventy declarations and included nothing that defines it: in C++ `wchar_t` is a keyword, so
every consumer in this solution compiled it without complaint, and MSVC makes it native in C as
well, so the platform where the header is used most was the platform that could not report the
fault. A C compiler on Linux gave *unknown type name 'wchar_t'* seventy times. It now includes
`<wchar.h>`, as its sibling `TargetCore_c.h` always has. It was found by the TargetCore install
gate, which was the first thing anywhere to compile a shipped header from C off Windows — the
export check above measures which names leave the DLL, and cannot see whether the header that
declares them parses.

**Installing.** `cmake --install` stages `msgcore` (the shared library and, on Windows, its
import library), `Msgcore_c.h`, `Msgcore_version.h` — which `Msgcore_c.h` includes on its first
line, so it is not optional — and `LICENSE` + `NOTICE`, which Apache-2.0 §4 requires travel with
the binary. A `find_package(Msgcore)` config package is installed beside them and declares
`Msgcore::msgcore`. The C++ headers are deliberately not installed: they reach `stdafx.h`, which
reaches `Platform/` and MFC. `p2pplatform` is not in the export either — it is an INTERFACE
library holding an include directory and contributes nothing to the artifact, so the link to it
is wrapped in `$<BUILD_INTERFACE:>`.

That surface is now checked rather than described. `tools/ci/check_exports.ps1` diffs the
built DLL's export table against tracked manifests on every push — the 282 flat
`msgcore_*` names separately from the ~700 mangled ones, so a change to the supported
half cannot hide inside churn in the internal half. A surface nobody measures widens one
accidental `dllexport` at a time, and each one is a promise 1.x then has to keep.

Two contracts that the C++ surface does not state and callers must nonetheless honour.
Both are also stated in [`Msgcore_c.h`](Msgcore_c.h) itself, at the declarations they
govern; these are the short forms.

- **Msgcore is not internally synchronised.** `MsgVBHeap.cpp` contains no lock of any
  kind. One store must be serialised by its caller; the COM layer does exactly this,
  one critical section per store.
- **A pointer from `Addr2Phys` does not survive a mutation.** Heap growth reallocates
  the base image, so every raw pointer previously handed out dangles. Re-resolve from
  the offset after any operation that can allocate.

## Repository layout

```
Msgcore.h  Msgcore.cpp        module definitions, export decoration, version macros
Msgcore_version.h             the single source of version identity (see below)
MsgVBHeap.*  P2PmsgVBLock.*   the offset-addressed heap and its block allocator
P2Pmsg.*  P2Pmsg_Ext.*        P3PmsgField / P3PmsgData / names / BSTRs
MsgList.*  MsgVect.*  MsgStck.*  MsgCurs.*  MsgAttr.*  MsgDesc.*
                              the container types over a field's children
P2PmsgMgr.*                   the store manager: Save, Load, triggers, paths
Msgexception.*                the P2Pevent exception and diagnostic system
Msgcore_c.h  Msgcore_c.cpp    the flat C ABI  (Msgcore_c_u8.cpp: UTF-8 entry points)
Kernel32_Ext.*  MsgCollectors.*
tests/                        the unit suites and the C4 load-rejection regressions
Platform/                     the Win32->POSIX shim layer, in this tree (see below)
Directory.Build.props/.targets   the WDMSCS_LIB default and its guard
CMakeLists.txt                configures standalone; the .vcxproj is authoritative on Windows
```

### One runtime file this library reads: `P2Pmsg.cfg`

`Msgexception.cpp` is the only place in Msgcore that opens a file it was not asked to. Once,
lazily, on the first event that needs an answer, it looks for a plain text file named
`P2Pmsg.cfg` beside the host executable and then beside `Msgcore.dll` itself — the second
because a foreign host such as a bare JVM has an executable folder that belongs to somebody
else entirely.

```ini
ErrToMessageBox: 1        # 1/0, on/off, yes/no, true/false
LogFile: errorLog.txt     # relative to THIS file's folder
```

Both settings are optional; the defaults are **the dialog on and no log file**, so with no such
file present nothing differs from before it was understood. Lazily rather than at load time on
purpose: an MFC extension DLL's entry point runs under the loader lock, and opening a file there
is how a deadlock gets built.

`P2Pevent::LoadConfigFile()`, `ConfigFilePath()`, `LogFilePath()`, `SetLogFile()` and
`ConfigDiagnostic()` are the programmatic equivalents, and `P2PMSG_CONFIG` overrides the search
with an explicit path. Two rules govern what the file may do, both stated in full in
`Msgexception.h`: `ErrToMessageBox` can only ever move the policy **towards** text, never back to
the dialog; and a configured `LogFile` loses to a sink installed with `SetTextSink()`. Why either
matters is `TargetCore`'s story rather than this library's — see its `Readme.md`, *Hosting a hub
where nobody can see a dialog* — because the thing a dialog blocks is a hub's pump.

The log file is **UTF-8, no byte order mark, on both platforms**, opened and closed per entry and
serialised across threads. That is a decision taken here rather than left to `fwprintf`, which
puts UTF-16 code units into a byte stream under MSVC and locale-converted narrow text under
glibc — measured, after a test searching for a wide substring passed on one platform and failed
on the other.

### The platform layer: `Platform/`

`stdafx.h` includes `Platform/platform.h`, and on non-Windows `Platform/mfcshim.h`. Every
translation unit includes `stdafx.h`, so this is a hard dependency of the very first file
compiled — and it is **in this repository**, so a clone of this repository alone builds:

```text
Msgcore\
├── Platform\     ← platform.h, mfcshim.h, p2ptypes.h, p2pstr.h, …
└── …             ← the rest of this repository
```

On Windows `platform.h` is pure pass-through: WinSock2, ws2tcpip, mswsock, windows.h and
atlstr, in an order that header enforces. It earns its keep on the Linux port, where the
same include routes to the shim implementations and `mfcshim.h` supplies the `CObject` /
`CList` / `CMap` / `CString` / `ASSERT` subset the legacy sources expect.

Nothing has to be configured for this. A quoted include resolves relative to `stdafx.h`
first, so `"Platform/..."` resolves with no `-I`: `Msgcore(2026).vcxproj` carries no
`AdditionalIncludeDirectories` at all and needs none. Under CMake, the guarded
`add_subdirectory(Platform)` in `CMakeLists.txt` defines `p2pplatform` unless the
surrounding MSCS tree already has.

**This is the only copy of `Platform/`, and that is what makes it safe.** It has been all
three arrangements: vendored here, a sibling repository, and vendored here again. The
sibling layout was adopted on 2026-08-20 because the *first* vendored copy was a second
copy — the parent tree had one too, a quoted include resolves relative to the citing file,
and one Linux translation unit therefore reached two physical `p2ptypes.h`. `#pragma once`
cannot deduplicate two files, so the subdirectory build failed with ~40 `redefinition`
errors until `MSGCORE_PLATFORM_FROM_PARENT` was added to select between them. It also had
to be kept in sync by hand, checked on every push against a manifest of blob hashes.

Neither cost survives, because there is no second copy and no upstream to drift from: the
`Platform` repository is retired, `MSCS/Platform/` is deleted, and the parent tree adds
this directory as its `p2pplatform`. No macro, no manifest, no drift check. The other two
consumers reach this same tree from where they sit, as `"../Msgcore/Platform/..."` —
`TargetCore/stdafx.h` and `MscsUnitTests/stdafx.h`.

What this bought back is CI that checks out one repository. The build, test and fuzz
workflows no longer assemble `mscs/Platform` beside `mscs/Msgcore`, and no longer need the
`MSCS_SOLUTION_TOKEN` secret to read a second private repository. **Every workflow still
compiles the library and runs the suites on every push**; there is simply nothing to
assemble first, and no layout that can fail to be assembled.

## Tests

There are two, and they answer different questions. Both run in both link modes, and CI
runs both in both modes on every push.

```powershell
.\tests\build_run_suite.bat          # unit suites   -- 90 cases / 428 checks (423 in dll)
.\tests\build_run_c4.bat             # load-path security regressions -- 16 scenarios
.\tests\build_run_suite.bat static   # either script takes an explicit link mode
```

**`tests/MsgcoreSuite.cpp` and `tests/MsgcoreCApiSuite.cpp` are the unit suites** — the
C++ container/allocator surface and the flat C ABI respectively. They cover allocator
growth and free-list spill, the addressing widths, cursor and ring mutation during
by-name scans, IOMAGE save/load round-trips, the endian sentinel and the **layout
generation** it also carries, and the flat-ABI
handle registry (a handle the API never issued, a destroyed one, a double destroy and one
of the wrong kind are each refused rather than dereferenced).

**`tests/C4LoadTest.cpp` is a security regression harness, not a unit test.** Each of its
sixteen scenarios pins a specific finding: an IOMAGE or BSTRio header that declares more
than the file holds must be *rejected* by `Load` rather than walked, a validator must not
read off the end while validating, and a store may not declare a wider arena than the
addressing width it also declares. Several assert on *which* check refused, not merely
that one did — because inputs malformed enough to test are usually malformed enough that
something else would reject them anyway.

Everything either script produces goes to `tests\out\`, which `.gitignore` already covers.

The suites were ported from `MSCS\MscsUnitTests\`, which is not published here. What
stayed behind tests something else — TargetCore, and two date-normalisation cases whose
implementation lives in MsgcoreMFC. See the release-readiness register item 15.

## Versioning

`Msgcore_version.h` is the only place a version number is written. `Msgcore.rc` builds its
`VERSIONINFO` resource from it, and `Msgcore.h` and `Msgcore_c.h` expose the same numbers
as compile-time macros — so a consumer's `MSGCORE_VERSION_*` can never disagree with the
`FileVersion` the shipped DLL reports.

```cpp
#if !MSGCORE_VERSION_AT_LEAST(3,0,0)
#  error Msgcore 3.0.0 or later is required
#endif
```

Releases are annotated git tags named `vMAJOR.MINOR.PATCH`, matching the header. The current
release is **`v3.1.0`**. If you have a `Msgcore.dll` and want to know what built it, read its
`FileVersion` and check out the tag of the same number — that correspondence is verified at
tag time against the DLL's own version resource, not assumed from the header.

## Security

Msgcore reads binary images whose headers declare their own extent, which is exactly the
shape of parser that rewards hostile input. It has been reviewed, and several findings
are closed and regression-tested — but it has **not** been reviewed as a network-facing
parser, and it is not hardened for one. See [`SECURITY.md`](SECURITY.md) for the
disclosure process and for a specific statement of what is and is not in scope.

## Documentation

| | |
|---|---|
| [`LINKAGE.md`](LINKAGE.md) | static vs dynamic, and the silent data corruption that mixing them causes |
| [`byte_order.md`](byte_order.md) | which formats carry a real byte-order contract, and the `oSync` endian sentinel — which is also the message image’s **layout generation** field — cited from `Msgcore.h` and `MsgVBHeap.h` |
| The Linux port plan, **in the `Msgcore_ProdDocs` repository** | the IOCP → io_uring port and the platform layer — what `Platform/` exists for; moved out of this repository alongside the release-readiness register, so both are cited by description rather than by a path that no longer resolves here |
| The COM server's own readme and IDL, **in the `MsgFacade` repository** | why the COM layer departs from a 1:1 transliteration in five places, and the COM contract argued at length — moved out of this repository by `c88f9f5`, so they are cited by description rather than by a path that no longer resolves here |
| the release-readiness register | what is and is not ready, and in what order it gets fixed |
| [`SECURITY.md`](SECURITY.md) | disclosure process and threat-model scope |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | how to contribute, and the sign-off requirement |

Both design notes describe MSCS as a whole, so they reference sibling components
(`TargetCore`, `MscsUnitTests`, the DSP and hub subsystems) that are not published here.
Their Msgcore-relevant sections stand on their own; the cross-references are left intact
rather than edited out, because a design note with its context removed is worth less than
one with a few names you cannot look up.

---

# Licence

Msgcore is licensed under the **Apache License, Version 2.0**. See
[`LICENSE`](LICENSE) for the full text, or <http://www.apache.org/licenses/LICENSE-2.0>.

```
Copyright 2000-2026 Ivyware Pty Ltd, Khrustal & Mann

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## Files not covered by the licence

Some files here are Microsoft project-template, wizard-generated or sample-derived files.
They keep Microsoft's own notices and are **not** licensed under Apache 2.0:
`Targetver.h`, `Resource.h`, `Msgcore.rc`, `stdafx.h`, `stdafx.cpp`, and the Visual
Studio solution and project files. `Msgcore.rc` is a mixed file — its `VERSIONINFO`
block is ours and is Apache-licensed. See [`NOTICE`](NOTICE) for the full list and the
one file that carries no header for a mechanical reason.

## Dependencies licensed separately

This library is an MFC extension DLL and links against the Microsoft Foundation Classes,
the Visual C++ runtime, and the Windows SDK (`ws2_32`, `MsWsock`, `Propsys`, and the COM
support runtime) — all licensed by Microsoft, none redistributed here. These are
dependencies, not bundled source.
