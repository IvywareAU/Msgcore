# Platform/ — the Win32→POSIX shim layer for Msgcore + Targetcore

Maps the Win32 subset Msgcore + Targetcore actually use, so one source tree builds on both
Windows (genuine IOCP / CNG / MFC) and Linux (io_uring / OpenSSL 3 / pthreads).

This is the **platform layer** of the Linux port plan, §4. The `§`-references throughout these
headers cite that plan, which lives in the parent MSCS tree and is not published — read them
as provenance, not as links. They name the plan rather than pointing at it for exactly that
reason: a citation a reader cannot open is worse than a citation that simply says where the
argument came from.

## The one rule

> On `_WIN32`, every shim header `#include`s the real Windows header and defines nothing
> that alters the ABI. **The Windows binary stays bit-for-bit the current architecture.**
> On Linux the shims implement *exactly* the inventoried subset — anything outside it is a
> deliberate compile error.

Every header holds that rule structurally: a `#if defined(_WIN32)` branch that includes the
genuine SDK/ATL/MFC header and stops (`p2ptypes.h:34`, `p2psock.h:33`, `p2pexport.h:34`,
`mfcshim.h:33`), and an `#else` branch carrying the Linux subset.

The legacy `stdafx.h` files **do** route through `platform.h` — that was the Phase-1 wiring
step and it is done: `Msgcore/stdafx.h:53` (via `MSGCORE_PLATFORM_FROM_PARENT`, see
*Vendoring*) and `Targetcore/stdafx.h:32`, plus `mfcshim.h` on Linux only. Because
`platform.h` is pass-through on Windows, that wiring leaves the `(2026).vcxproj` output
unchanged.

## What is here

| File | Purpose | Windows | Linux |
|---|---|---|---|
| `p2pexport.h` | `P2P_EXPORT`/`P2P_IMPORT`/`P2P_LOCAL` visibility (§4, §6.2) | `__declspec` | `visibility("default")` + `-fvisibility=hidden` |
| `p2ptypes.h` | Win32 scalars + the tagged `P2PHandle` HANDLE/SOCKET model, with Win32 handle lifetime — closed handles are retired to a free-list and reused, never freed, so a handle closed under a thread that is still using it reads as closed instead of as freed memory (§4.1) | pass-thru | implemented |
| `p2pstr.h` | `CString` subset over `std::wstring`, `TCHAR`/`_T`, `char16_t` wire pinning (`P2PWCHAR`), UTF-8↔UTF-16 with surrogates, `p2p_wkey` (§4.2) | pass-thru | implemented |
| `mfcshim.h` | `CObject`/`DYNCREATE`/`CList`/`CMap`/`CMutex`/`ASSERT` façades (§4.3) | not used — real MFC | implemented |
| `p2pthread.h` | `CreateThread`→`std::jthread`, `CRITICAL_SECTION`→`std::recursive_mutex` (Win32 CS is recursive), events→`eventfd` (§5.3, §6.1) | pass-thru | implemented |
| `p2pfile.h` | `CreateFile`/`ReadFile`/`WriteFile`, lock-file semantics via `O_EXCL`+`flock`, `\\\.\pipe\…`→AF_UNIX, `\\.\COMx` translation, MSVC low-level CRT I/O (§5, §6.1) | pass-thru | implemented |
| `p2piocp.h` + **`p2piocp.cpp`** | **IOCP over io_uring — the core of the port** (§5): one ring per pump thread (`SINGLE_ISSUER\|DEFER_TASKRUN` + fallback), fd→completion-key table, MPSC posted queue + eventfd for `PostQueuedCompletionStatus`, cross-thread submission with every kernel return inspected (a refused SQE becomes a failed completion, never a lost one), CQE→GQCS translation, `cancel_fd` teardown over a held dup so a cross-thread close cannot race it, live-port registry | pass-thru — real NT IOCP | implemented (links liburing) |
| `p2psock.h` | `socket`/`closesocket`/`WSA*` over the tagged `SOCKET`, `AcceptEx`/`ConnectEx` on the ring, `SO_REUSEADDR` on bind (§4.1, §5.1, §6.2) | pass-thru | implemented |
| `p2pserial.h` | `DCB`/`SetCommState`/`WaitCommEvent`→termios + ring `poll_add`, configurable COM map (`P2P_COM<n>`) (§6.2) | pass-thru | implemented |
| `p2psvc.h` | `WinSvc.h` SCM types/constants + Linux no-op stubs, so `P2PeerService` keeps its call sites and falls through to the console/daemon `Run` path (§6.2) | forwards to real `<WinSvc.h>` | implemented (stubs) |
| `p2pcrypto.h` | Backend **selector** macros only — the crypto itself stays in `P2PCngCrypto` (§6.2, Risk #5) | CNG | OpenSSL 3 |
| `platform.h` | umbrella include for the legacy `stdafx.h` files | done | done |
| `gen_wincompat.sh` | generates `win-compat/` — Linux-only forwarders named after the Windows headers the legacy code includes | not used | configure-time |
| `checks/` | `header_check.cpp` (every reachable header parses) and `iocp_test.cpp` (the §5.2 semantics suite) | header check | both |
| `CMakeLists.txt` | the `p2pplatform` target + the opt-in `platform_iocp_test` | — | — |

**Phase state.** Phases 0–5 are complete and Phase 6 (CMake-on-MSVC parity, `_u8` C-API) is
complete except §7 CI gating; the parent tree's Linux-port progress log is the ledger and the
only place that figure is maintained. For scale: the full parent suite ran **45/46 on Linux**
(2026-08-15) against 43/43 on Windows Release — the shim layer is carrying the real transport,
serial, pipe and crypto paths, not a scaffold.

## How it plugs in

**The umbrella.** `platform.h` pulls types → strings → thread → file → iocp → sock → serial →
crypto, in that order (WinSock2 before windows.h matters on Windows; `p2ptypes.h` enforces it).
`mfcshim.h` is deliberately **not** in the umbrella: on Windows real MFC arrives via `stdafx.h`,
and on Linux only the TUs that need the MFC façades include it, keeping the "subset only"
surface tight (§4.3).

**`win-compat/` (Linux only).** `gen_wincompat.sh` runs at CMake configure time and writes a
directory of headers named exactly like the ones legacy code includes — `afxwin.h`, `atlstr.h`,
`windows.h`, `WinSock2.h`, `wtypes.h`, `WinSvc.h`, `io.h`, `xstring`, … — each forwarding to the
matching shim. That directory is on the include path **only** on Linux (Windows uses the real
SDK/MFC), so no legacy `#include` needs editing. It is generated, not tracked (`.gitignore`).
`p2psvc.h` is reachable *only* this way, through `win-compat/WinSvc.h`.

**Vendoring.** This repository is vendored into the `Msgcore` repository as `Msgcore/Platform/`,
so a clone of Msgcore alone compiles. A quoted include resolves relative to the including file
first, so `Msgcore/stdafx.h`'s `"Platform/platform.h"` always finds the vendored copy and no
`-I` can redirect it — while every generated `win-compat` forwarder is `#include "../p2ptypes.h"`
relative to *itself* and reaches the parent's copy. One TU, two physical files, `#pragma once`
cannot deduplicate them. `MSGCORE_PLATFORM_FROM_PARENT` resolves it: Msgcore's own CMakeLists
sets it in the parent-tree branch (`Msgcore/CMakeLists.txt:54`) and `stdafx.h` then includes
`<platform.h>` instead of the vendored copy.

The vendored tree is pinned per-file by git blob SHA-1 in `Msgcore/tools/ci/platform-vendor.manifest`
and checked on every push by Msgcore's `hygiene` job. **Never edit `Msgcore/Platform/` in
place** — land the change here, then re-vendor and regenerate the manifest in one Msgcore commit
(`check_platform_vendor.ps1 -Upstream ..\Platform -Regenerate`).

## Named pipes -> AF_UNIX

`p2pfile.h` lists the mapping in one line; this is what it actually costs, because a named
pipe and a Unix socket are not the same object and the differences are load-bearing for the
one transport that uses them (`Targetcore/P2PeerConPipe.cpp`).

A pipe name maps to a filesystem path: `\\.\pipe\Name` -> `$XDG_RUNTIME_DIR/p2pmsg/Name.sock`,
falling back to `/tmp/p2pmsg/` when `XDG_RUNTIME_DIR` is unset. `p2p_pipe_path`
(`p2psock.h:371`) keeps only the final component of the name and creates the directory `0700`
best-effort.

| Win32 | Linux | Where |
|---|---|---|
| `CreateNamedPipe` | `socket(AF_UNIX, SOCK_STREAM \| SOCK_CLOEXEC)` -> `unlink` the stale path -> `bind` -> `listen(8)` | `p2psock.h:404` |
| `ConnectNamedPipe(h, ov)` | overlapped -> `p2p_iocp_accept`; synchronous -> blocking `accept()` | `p2psock.h:424` |
| `CreateFile("\\.\pipe\...")` | `p2p_pipe_client_connect` -> `socket` + blocking `connect` | `p2pfile.h:188`, `p2psock.h:387` |
| `DisconnectNamedPipe` | no-op, returns `TRUE` | `p2psock.h:434` |
| `ReadFile` / `WriteFile` | unchanged - the handle is `HKind::Fd`, so it takes the same io_uring path as a socket | `p2pfile.h`, `p2piocp.cpp` |

**The morph is faithful, not a workaround.** `accept()` returns a *new* fd, whereas a Win32
pipe instance *is* the connection. The accept completion therefore closes the placeholder fd
inside the target handle, installs the accepted fd in its place, and copies the listen
socket's completion key onto it (`p2piocp.cpp:460-470`). That is not the shim inventing
semantics: the legacy transport already documents the same behaviour on Windows - *"pipes are
different whereby the listening P2PeerCon morphs into the accepted connection"*
(`Targetcore/P2PeerConPipe.cpp:390`). The shim reproduces a property the design already had.

**Message mode is ignored, and that is safe here.** `PIPE_TYPE_MESSAGE` /
`PIPE_READMODE_MESSAGE` are defined so call sites compile, then discarded - the shim always
uses `SOCK_STREAM`. This is not a compromise: the only caller creates its pipe
`PIPE_TYPE_BYTE | PIPE_READMODE_BYTE` (`Targetcore/P2PeerConPipe.cpp:395`) and frames itself
with length prefixes, so no boundary guarantee is owed. `SOCK_SEQPACKET` would supply one if a
future framing layer ever wanted it.

**Where the substrate shows through.** Six differences that no mapping removes:

1. **One listening instance, so there is a re-arm window.** The transport asks for
   `maxInstances = 2` and re-arms with `CreateListenPipe()` after each accept. On Windows a
   spare instance can already be bound, so a client arriving mid-accept still finds the name.
   Here there is exactly one bound socket, and between the morph closing the listen fd and the
   re-`bind` the path does not exist. See "Known gaps" #4 - read from the code, not reproduced.
2. **Access control moves from a security descriptor to the filesystem.** `CreateNamedPipe`'s
   `SECURITY_ATTRIBUTES` argument is ignored. What protects the endpoint is the `0700`
   directory; the socket file itself takes the process umask. Equivalent in practice for a
   single-user daemon, different in kind for anything multi-user.
3. **Lifetime is filesystem, not kernel.** A Windows pipe vanishes with its last handle; a
   crashed server leaves a stale `.sock`. Hence the unconditional `unlink` before `bind` -
   which also means a second bind to the same name takes the path from the first.
   Already-connected peers are unaffected, since they hold the inode.
4. **Local only.** There is no AF_UNIX equivalent of a pipe over SMB. The client dispatch
   matches `\\.\pipe\` and `//./pipe/` only (`p2pfile.h:188`), so a `\\server\pipe\Name` falls
   through to the ordinary-file path and fails rather than silently resolving to a local
   socket - a failure, but a legible one.
5. **Connect failure is remapped deliberately.** `ENOENT` and `ECONNREFUSED` both become
   `ERROR_FILE_NOT_FOUND` (`p2psock.h:395`) so the client's existing failure path matches
   Win32. Note that the caller neither retries nor uses `WaitNamedPipe`
   (`Targetcore/P2PeerConPipe.cpp:639`) - a failed connect throws.
6. **`SIGPIPE` has no Win32 counterpart.** `p2p_ignore_sigpipe()` (`p2ptypes.h:491`) installs
   `SIG_IGN` so a write to a departed peer returns `EPIPE` - which `win32_from_errno` turns
   into `ERROR_BROKEN_PIPE` - instead of killing the process.

## Build

**In the parent MSCS tree — the canonical path.** `CMakeLists.txt` here has no `project()` and
this repository carries no presets; it is consumed by `add_subdirectory(Platform)`, which defines
the `p2pplatform` INTERFACE target (headers + include dirs). `p2piocp.cpp` is compiled by the
consumers that need the ring (`Targetcore/CMakeLists.txt`) and by the iocp test — not into a
library of its own. From the parent tree:

```sh
cmake --preset windows-msvc          # configure (multi-config: --build --config Debug|Release)
cmake --preset linux-gcc-debug       # configure (Ninja)
cmake --build --preset linux-gcc-debug
ctest  --preset linux-gcc-debug
```

`windows-msvc-debug` / `linux-gcc-debug` also exist as *build* and *test* preset names; the
configure presets are `windows-msvc` and `linux-gcc-*`. The library targets sit behind
`MSCS_BUILD_LIBS` (default `OFF` at the root, `ON` in every preset) and now build on **both**
platforms — CMake+MSVC reached parity in Phase 6. The `(2026).sln`/`.vcxproj` files remain the
shipping Windows build.

**Standalone, from a clone of this repository.** There is no `project()`/preset here, so CMake
cannot configure it alone — but both checks compile directly. From this directory on Linux:

```sh
g++ -std=c++23 -I. checks/header_check.cpp -o header_check && ./header_check
g++ -std=c++23 -I. checks/iocp_test.cpp p2piocp.cpp -luring -pthread -o iocp_test && ./iocp_test
```

Prerequisites (Linux): GCC 15 / C++23, `liburing-dev`, `libssl-dev` (OpenSSL 3), `bash` for
`gen_wincompat.sh`, and `pkg-config` — `MSCS_BUILD_IOCP_TEST` defaults to whether `pkg-config`
finds liburing.

## The checks

- **`checks/header_check.cpp`** — includes the umbrella plus `mfcshim.h` and ODR-uses the
  HANDLE model. On Windows it proves the shims resolve cleanly to the genuine headers (the
  original Phase-0 exit criterion); on Linux it proves the subset parses under GCC/C++23. It is
  the gate that would have caught `adb8f2f` (a missing `CList::FindIndex` stopped Targetcore
  building on Linux entirely). It does **not** reach `p2psvc.h`.
- **`checks/iocp_test.cpp`** — the §5.2 semantics suite: async completion with byte count and
  key, the Fix-4 invariant (data already buffered at submit time still posts a CQE), cross-thread
  `PostQueuedCompletionStatus`, cancellation → `ERROR_OPERATION_ABORTED`, graceful close →
  `ERROR_HANDLE_EOF`, timeout → FALSE + null OVERLAPPED, plus the async file/event/`CloseHandle`
  wiring layered on the ring. Registered as ctest `platform_iocp_test`. Five cases cover the
  failure modes a service meets and a single-threaded suite does not:
  - **cross-thread close** — a connection dropped from a thread that is not the pump still
    surfaces `ERROR_OPERATION_ABORTED` on its own completion key. The suite used to claim ring
    ownership first *specifically* to keep the cancel ahead of the `close()`, which is the one
    ordering a real hub thread never gets;
  - **fd reuse** — the socket that inherits a closed connection's fd number must keep its own
    reads, and the closed one must still abort;
  - **more in-flight ops than the ring can hold** — every OVERLAPPED comes back exactly once,
    carrying its own key, when the completion queue is pushed past its 512 entries;
  - **forced submission failure** — the suite re-execs itself with `P2P_IOCP_FAULT=sqe` so the
    submit-failure recovery runs: on the issuing thread the op is refused synchronously, and
    from another thread it comes back as a failed completion rather than vanishing;
  - **SIGPIPE** — a write to a departed peer reports an error instead of killing the process;
  - **handle lifetime** — a socket closed while another thread is calling on it leaves that
    thread reading a *closed* handle rather than freed memory, a second close fails instead of
    corrupting the free-list, and a retired handle is proven to be reused rather than leaked.
    Its concurrent half is compiled out under ThreadSanitizer, which reports the caller-side
    fd race the case deliberately performs; ASan is what proves the lifetime, and is clean.

Last run 2026-08-20 on Ubuntu 26.04 (kernel 7.0.0-30, g++ 15.2, liburing 2.14, OpenSSL 3.5.5),
using exactly the two command lines above against this commit's tree: `header_check` compile and
run rc=0; `iocp_test` **125 checks, 0 failures — PASS**, and the same suite is clean under
`-fsanitize=address` (125) and `-fsanitize=thread` (123 — two gated, see above).

**CI.** No workflow runs in *this* repository. The shim layer is gated from the Msgcore side —
a `Platform shim layer (Linux / io_uring)` job that compiles and runs both checks against the
vendored copy — which as of 2026-08-18 is committed on Msgcore's `ci/linux-platform-shim` branch
and **not yet pushed**, so nothing enforces this layer per-push today. The parent's full
Windows⇄Linux gate (§7) is blocked on a publishing decision, not on code.

## Known gaps

1. **`_O_TEXT` is defined twice with different values** on Linux — `0` in `p2pfile.h:138`
   (guarded by `#ifndef _O_RDONLY`) and `0x4000` in `p2ptypes.h:567` (guarded by
   `#ifndef _O_U16TEXT`). Different guards, so both fire and the value depends on include order;
   `header_check` compiles with the redefinition warning. Through the umbrella `p2pfile.h` wins
   and the value is harmless, but `0x4000` is `O_DIRECT` on Linux and `_open` passes its flags
   straight through (`p2pfile.h:143`), so a TU that takes `p2ptypes.h` without `p2pfile.h` has a
   live hazard rather than a cosmetic one.
2. **`p2psvc.h` is outside both `P2PPLATFORM_HEADERS` and `header_check.cpp`**, so nothing
   compiles it except a full Linux build that goes through `win-compat/WinSvc.h`.
3. **The per-header `Phase status:` notes in the source comment blocks are stale** — several
   still say "scaffold" for code that shipped in Phases 1–4. The table above is current; those
   notes are not.
4. **A named-pipe server has one listening instance, not two.** `CreateNamedPipe`'s
   `maxInstances` is ignored, and the accept morph closes the listen fd before the transport
   re-arms, so a client connecting in that window gets `ENOENT` -> `ERROR_FILE_NOT_FOUND` and
   the caller throws without retrying. On Windows the second instance absorbs it. The window
   is the accept-completion handler's path back to `CreateListenPipe()`. **Read from the code;
   not reproduced.** The fix is to bind the next listener before surrendering the current fd,
   which needs the morph to hold two fds briefly.
5. **`sun_path` truncation is silent.** `p2p_pipe_client_connect` and `CreateNamedPipeW` both
   `strncpy` into the 108-byte `sockaddr_un::sun_path` (`p2psock.h:392`, `:411`), so a long
   `XDG_RUNTIME_DIR` plus a long pipe name truncates without error - and two names sharing a
   prefix can truncate to the same path. Win32 pipe names have no such limit.
---

# Licence

Platform is licensed under the **Apache License, Version 2.0**. See
[`LICENSE`](LICENSE) for the full text, or <http://www.apache.org/licenses/LICENSE-2.0>.

```
Copyright © 2026 Khrustal & Mann

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

Every tracked file carries that header, and **nothing here is excluded** — unlike the
sibling repositories, this one has no Visual Studio project-template or wizard-generated
file to except. `win-compat/` is generated, not tracked, and is the output of
`gen_wincompat.sh` rather than anyone else's source.

## Dependencies licensed separately

The Linux build links **liburing** (LGPL-2.1 / MIT dual licence) from `p2piocp.cpp` and
selects **OpenSSL 3** (Apache-2.0) as the crypto backend. The Windows build reaches MFC,
ATL, the Visual C++ runtime and the Windows SDK through the `_WIN32` pass-through branches —
all Microsoft, licensed separately, none redistributed here.

This layer is an **independent implementation** of a subset of the Win32 API, written so
unmodified legacy sources compile against POSIX. No Microsoft source, header or sample code
is included in, or was copied into, this repository. See [`NOTICE`](NOTICE).
