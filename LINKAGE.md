# Msgcore linkage: all static or all dynamic

Msgcore builds in two shapes, and **a single process must commit to exactly one of
them**. Mixing them inside one process compiles, links, runs, and then corrupts data.

| Shape | Configurations | Output | Macro |
|---|---|---|---|
| MFC extension DLL | `Debug` / `Release` × `Win32` / `x64` | `Msgcore.dll` + import lib | `Msgcore_EXPORTS` |
| Static archive | `DebugLib` / `ReleaseLib` × `Win32` / `x64` | `Msgcore.lib` | `Msgcore_STATIC` |

Both land in `out\<Platform>\<Configuration>\` and are staged to
`$(WDMSCS_LIB)` = `MSCS\lib\<Platform>\<Configuration>\`.

---

## The rule

> Every module in a process that touches Msgcore must reach it the same way.
> Either they all import it from one `Msgcore.dll`, or there is exactly one module in
> the process and it absorbs the archive.

## Why: the heap is the shared object

Msgcore is a heap manager. A `P2PmsgMgr` owns a `VBHeap`, and every `P2Pos` handed out
is an **offset into that heap**, not a pointer. Handles, cursors, descriptors and field
references are only meaningful against the heap they came from.

Two statically linked copies of Msgcore in one process means two heaps and two
allocators. A `P2Pos` minted by module A and resolved by module B indexes B's heap at A's
offset: it will usually resolve to *something*, silently, and that something is the wrong
object. Free it and you corrupt B's heap. This is why the failure mode is data
corruption rather than an access violation.

The same split applies to Msgcore's process-wide state, one copy per linking module:

| State | Where | What it holds |
|---|---|---|
| `g_HWnd`, `s_uiWM_USER`, `s_dwEventFilter`, `s_dwUserKey` | `Msgexception.cpp` | the `Register4P2Pevents` HWND notification registration |
| `s_pfnP2PeventCB`, `s_nP2PeventSinkID`, `s_dwSinkP2PeventFilter`, `s_dwSinkUserKey` | `Msgexception.cpp` | the headless event-sink registration |
| `s_nEventNo` | `Msgexception.cpp` | monotonic event serial |
| `WM_P2PeventNOTN` | `Msgexception.cpp` (exported) | the notification message id |
| `g_bP2Pmsg_AssertValid` | `P2Pmsg.h` (exported) | global validation switch |
| `nMsgcore` | `Msgcore.h` (exported) | module sentinel |

Register an event sink through one copy and the other copy never fires it: diagnostics
and triggers go quiet with no error, which is worse than a crash because it looks like
"nothing went wrong".

## What mixing actually does

- **`P2Pos` values cross heaps** and resolve to the wrong object, or to garbage.
- **Event sinks and trigger callbacks stop firing** for whichever copy did not receive
  the registration.
- **`g_bP2Pmsg_AssertValid` becomes per-copy**, so turning validation on protects only
  half the process.
- **Heap teardown double-frees or leaks**, depending on which copy unwinds first.

None of this is a link error. Pick a shape per process and hold it.

## Corollaries

**1. Static Msgcore pairs with static Targetcore.** Targetcore sits directly on this
heap; see `Targetcore/LINKAGE.md`, which carries the same rule for the hub and pump
state. Pair `DebugLib`↔`DebugLib`, `ReleaseLib`↔`ReleaseLib`.

**2. A consumer of the archives must define BOTH macros:**

```
/DMsgcore_STATIC /DTargetcore_STATIC
```

Defining only one compiles the other core's headers in `dllimport` mode. Most symbols
still resolve against the archive, but only behind a wall of `LNK4217`, and **inline
members of `dllimport`-decorated classes do not resolve at all** —
`P2Pevent::SetFParam(LPCTNAM, const P3PmsgItem&)` is defined inline in `Msgexception.h`,
so a `dllimport` view emits a call to an export the static build never produced and the
link dies on `LNK2001`.

**3. A static archive records no dependencies.** The consumer links them:

```
MsWsock.lib ws2_32.lib comsuppw.lib Propsys.lib      (comsuppwd.lib in Debug)
```

**4. MFC and CRT must match.** The Lib configurations keep `UseOfMfc=Dynamic` and `/MD`,
so the archive must be absorbed by a `/MD` consumer using shared MFC. A consumer that
includes `<afx.h>` under `/MD` also needs `/D_AFXDLL`.

**5. `_AFXEXT` is not defined in the Lib configurations.** It declares an MFC *extension
DLL*; an archive is not a module and joins no `CDynLinkLibrary` resource chain. Msgcore
has no `DllMain` (it is commented out in `Msgcore.cpp`), and `Msgcore.rc` holds only a
`VERSIONINFO` block and one string, so nothing is lost.

## Choosing

**Static** fits a single-module process: CLI tools, test harnesses, a self-contained
daemon. It removes DLL deployment, allows cross-boundary inlining, and lets the linker
drop unreferenced code.

**Dynamic** is required whenever more than one module in the process touches the core,
and for every non-C++ consumer — the `com\` ATL layer, the facade, .NET and PowerShell,
the PHP extension, Java Panama, the Python SDK. Those bind to an exported surface at
runtime; an archive gives them nothing.

When in doubt, use the DLL. It is the shape everything in this tree was built against,
and it is the only shape that is safe by construction.
