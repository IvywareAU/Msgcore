# Security policy

## Reporting a vulnerability

Please do **not** open a public issue for a suspected vulnerability. Report it through
**GitHub private vulnerability reporting** — the *Report a vulnerability* button on this
repository's *Security* tab. That channel is private to the maintainers until an advisory
is published, and it keeps the report attached to the code it concerns.

> **One setup step, and it must be done before this file is published:** private
> vulnerability reporting is off by default. Enable it under *Settings → Code security →
> Private vulnerability reporting*. Until it is on, the button this file tells you to press
> is not on the page, and a policy that names a channel which does not exist is worse than
> one that names none.

Include in the report:

- the version or commit you tested,
- which surface you reached it through (the C++ classes, or the flat C ABI in
  `Msgcore_c.h`),
- the input that triggers it — a stored image file is the most useful single artefact,
- what you observed: a crash and its faulting address, a heap corruption report, a read
  or write outside an allocation, or an assertion.

Expect an acknowledgement within a week. There is no bounty programme.

## What is in scope

Msgcore reads binary images whose header declares their own extent. That is the shape of
parser that rewards hostile input, and it is where reports are most valuable:

- `P2PmsgMgr::Load` and everything it reaches, given a **malformed or hostile store
  file**. This is the primary attack surface.
- The IOMAGE header path — `P2Piomage_ValidateSync`, `P2PmsgHeap_CreateIOMAGEn`,
  `Msgiomage_Duplicate` — where a declared size is trusted against a real buffer.
- The heap and block allocator (`MsgVBHeap.cpp`, `P2PmsgVBLock.cpp`): offset arithmetic,
  free-list manipulation, and address-width narrowing.
- The flat C ABI (`Msgcore_c.h`): handle validation, and any input from a caller that
  reaches memory it should not.

## What is NOT in scope, and will not be treated as a vulnerability

**Msgcore has not been reviewed as a network-facing parser, and is not hardened for one.**
It is a local structured-store library. It reads files that the process running it is
expected to have chosen. Deployments that feed it bytes arriving from an untrusted peer
are outside the threat model this code was written and reviewed against — that is a
statement about the code's assurance level, not an invitation to do it.

Also out of scope:

- **The exported C++ class surface used across a toolchain boundary.** It is
  toolchain-pinned by construction and is not a supported interface; see the README.
  **The flat C ABI in `Msgcore_c.h` is the only supported production surface for
  external consumers in 1.x.**
- **The COM server.** It is no longer part of this repository: `c88f9f5` moved `com/` to
  the `MsgFacade` repository, and nothing here builds, ships or registers it. It never had
  a security review while it was here, and being moved did not give it one, so it remains
  unreviewed code wherever it is depended on — report anything found in it against the
  repository that now holds it. The scope such a review would have to cover is written
  down in the release-readiness register (item 16b) and travels with it.
- **Mixing static and dynamic linkage in one process.** This corrupts data by design of
  the offset-addressed heap, silently and with no link error. `LINKAGE.md` documents
  the mechanism and the rule. A report that this corrupts data is a report that the
  documentation was not followed.
- **Concurrent access to one store without external locking.** Msgcore is not internally
  synchronised — `MsgVBHeap.cpp` contains no lock of any kind — and the caller owns
  serialisation. Data races arising from unsynchronised concurrent use are a contract
  violation, not a defect.
- **Using a pointer from `Addr2Phys` across a mutation.** Heap growth reallocates the
  base image and every previously returned raw pointer dangles. Re-resolve from the
  offset.

## Known limitations we already know about

Publishing these rather than waiting to be told:

- ~~**`P2PmsgMgr::Load` reads out of bounds on a malformed store file**~~ — **FIXED.**
  This was F1. The load path chooses between two formats; the IOMAGE branch had a
  declared-size-versus-real-buffer check and the BSTRio branch had no length parameter to
  make one from, so a 32-bit offset read out of the file became a pointer that was
  dereferenced unbounded. A 49-byte file reproduced it, found by AddressSanitizer +
  libFuzzer in ninety seconds.

  `P2PmsgHeap_CreateBSTRio` now has the length-taking overload its IOMAGE twin already
  had, and `P2PmsgMgr::Load` passes the file size to both branches. The forged declared
  size mattered more than it first appears: it was copied into the heap handle as the
  bound that every later address translation is checked against, so forging it *raised*
  the ceiling rather than tripping a check. The reproducer is now a regression scenario in
  `tests/C4LoadTest.cpp` and a fuzz seed; the fuzz job is green.

  A second out-of-bounds read (**F2**) was found in the same area shortly afterwards, in
  the block-chain *validator* itself: its bounds checks tested where a block started and
  not whether it fitted, so validating a crafted image read a neighbour's link field past
  the end of the buffer. It was reachable only in Debug builds until the validation was
  made to run in Release, and it is fixed. Both have regression cases in the test suite.

  **A note on what this does and does not settle.** Loading a store file from an untrusted
  source is still outside the threat model above. This closes two out-of-bounds reads
  found within an hour of fuzzing, on a parser that had not been fuzzed at all before —
  which is a statement about how little fuzzing it has had, not about how few defects
  remain. Treat it as two bugs fixed, not as the path being hardened.
- ~~**`Msgiomage_Duplicate` bounds an image's declared size against its own header, but
  not against the real extent of the source buffer**~~ — **FIXED**, by adding a signature
  that can express the bound rather than by changing the old one. It took a
  `const P2Piomage&` and no length, so an image declaring 128 KB inside a 4 KB buffer was
  copied by its declared length; a reference carries no length, so that overload could not
  be made safe, only superseded. `Msgiomage_Duplicate(image, bufferLen)` and
  `P3PmsgBSTR(image, bufferLen)` now exist and reject an over-declared header.

  **The old overloads remain and remain unsafe for untrusted input.** They are still
  correct for an image this process built, which is what every in-repository caller does,
  and removing them would be an ABI break. If you pass Msgcore an image you did not build,
  use the length-taking form.
- **Byte-order classification of a stored image is heuristic.** The `oSync` sentinel
  detects a foreign-endian image in most cases, but a *pre-sentinel* image whose size
  low byte falls in `0x00`–`0x03` or `0xA4`–`0xA7` classifies as
  same-endian rather than byte-swapped. Every 256-byte-aligned size — 512, 1024, 4096 —
  is in that set, so the miss is systematic on the sizes that actually occur rather than
  rare. The full analysis is in an internal design note not published with this
  repository.
  **A third range `0xA8`–`0xAB` was present for one day and is gone.** The sentinel bits
  became a **layout generation** field on 2026-08-20, and the first design reserved a
  second code (`VBLock_SyncGen2` = `0xA8`) that the swap arm then had to look for in the
  low byte, at 1/64 of this diagnosis. On 2026-08-21 that registry was replaced by a
  fallback — any non-current non-zero code is "a layout this build does not implement" —
  which names every future generation instead of one, and puts the diagnosis back at
  **62 of 64**. Refer [`byte_order.md`](byte_order.md) §4.4.
- **A crafted header can be reported as an unimplemented layout generation rather than as
  corruption.** That is the fallback's one concession and it is a message, not a gate:
  `VBLock_SyncForm` no longer returns `VBLockSync_Invalid`, so bytes that form a valid
  complement pair but name no layout this build implements are refused as a generation
  mismatch. Every path that refused them before still refuses them —
  `P2PmsgHeap_IsIOMAGE` accepts `Native` and `Legacy` only, and the wire gate accepts
  `Native` only. An operator reading the log sees a generation code where they previously
  saw "invalid image"; nothing is admitted that was not admitted before.
- **The WIRE no longer accepts a pre-sentinel image and the STORE still does.** Since
  2026-08-20 `P2Peerio::RecvP2PeerMsg` refuses any frame whose layout generation is not
  the current one, before it allocates; `P2PmsgMgr::Load` keeps accepting one from disk
  and raises a warning naming it. A frame is a peer and a peer can be upgraded; a file is
  data somebody already has. The consequence for an integrator: **a peer built before the
  sentinel can no longer talk to one built after it**, which `byte_order.md` §4.3 had
  always said must be true and which is now enforced instead of assumed.
- ~~**A narrow-addressing store does not enforce its own addressing limit.**~~ **Fixed.**
  An `Addr16` store used to grow past its 65535 ceiling — measured at 77833 bytes — because
  the only guard tested live allocated bytes rather than the arena that actually grows. Past
  the width an offset stored into a link field is truncated to a different valid-looking
  offset, silently corrupting the free list. The arena is now bounded by the width on both
  paths that grow it, and **an image that declares a wider arena than the addressing width
  it also declares is rejected at load** — that second half is the one reachable from a
  file, and it needed no allocation churn at all, only a crafted header.

  `Addr08` is **not** a supported heap addressing width and never was: `Msgcore_c.h` has
  documented `uAddrNN` as 16/32/64 since the flat C ABI was written. It is now refused in
  every build rather than only in Debug. `Addr16` and `Addr32` are supported; `Addr32`
  remains the default and the best-exercised.
- ~~**The IOMAGE arm of the load path never ran its structural walk in a shipped
  binary**~~ — **FIXED.** This was **F3**, and it is F1's lesson repeating on the other
  branch. `P2PmsgMgr::Load` dispatches between two formats. The length-taking
  `P2PmsgHeap_CreateIOMAGE` checked the declared size against the real buffer and then
  delegated to an overload that ended with both of its validators *inside* `ASSERT(...)`
  — so Release did not run a weakened block walk, it ran none. The BSTRio arm had been
  given a real gate; this one had not, because the earlier pass fixed the branch the
  fuzzer happened to reach rather than the pattern.

  Both IOMAGE validators also carried F2's defect — bounds-checking where a block
  *starts* and not whether its control block *fits* — and one of them was worse placed
  than F2 ever was: `P2PmsgHeap_AssertValidIOMAGE` is already called directly from
  Release code rather than only from inside an assertion, so its unguarded header read
  was live in shipped binaries rather than dormant in them. Both now bound the header
  before dereferencing it, the per-block tests feed the walk's return value instead of
  only asserting, and the untrusted overload rejects on the result. Regression scenario
  `[15]`, which asserts on *which* check refuses rather than merely that one did.

  A dead validator, `P2PmsgHeap_AssertValidIOMAGE1`, was deleted in the same pass —
  no caller, no declaration in the header, and four bare-`ASSERT` per-block tests.

- **Structural validation inside `ASSERT` is absent from Release builds.** 260 sites call
  a validation function from inside an `ASSERT`, which means the call itself does not
  happen in a shipped binary. Both arms of the load path's dispatch now run their walks
  in every build and reject on the result, and the counts are held against a baseline so
  they cannot grow. The rest is open work — and the headline figure overstates it: most
  of those 260 wrap cheap inline predicates (`VBHeap_IsAddr`, `VBLock_IsLinked` and
  their kin) or `sizeof`, not whole-image walks, of which about a dozen remain.
- **The COM server left this repository unreviewed.** It postdated the one review this
  code has had and never had one of its own, and `c88f9f5` moved it to `MsgFacade` in that
  state. Moving code does not review it, so this is recorded rather than dropped: the
  scope a review would have to cover — the `VARIANT` conversion boundary, the dispatch
  thread's lifetime and shutdown ordering, connection-point reentrancy, and the
  registration surface — is written down in the release-readiness register under item 16b, and is
  owed by whichever repository ships it.

## Supported versions

`v3.0.0` is tagged and is the first public release. `Msgcore_version.h` reports
`3.0.0.0`, and the built DLLs carry it in their `VERSIONINFO` resource, so a binary can
be traced back to the source state that produced it.

| Version | Supported | Notes |
|---|---|---|
| 3.0.x | **Yes** — fixes land on `master` and ship in the next tag | The flat C ABI only; see the scope sections above |
| < 3.0.0 | No | Development states that were never published; there is nothing to identify them by |

There is one maintained line, and no long-term-support branch: a fix goes onto `master`
and is released by the next tag rather than backported. That is honest for a project with
one maintained line, and this table changes when there is a second.

**The supported surface is narrower than the shipped surface**, and that distinction is
the one to read before depending on anything here: the flat C ABI in `Msgcore_c.h` is
supported, and the mangled C++ export half is shipped by the MFC-extension-DLL model but
is toolchain-pinned and internal.
