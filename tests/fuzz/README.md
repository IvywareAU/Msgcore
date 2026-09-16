# tests/fuzz — ASan + libFuzzer over the two paths that consume foreign bytes

The release-readiness register item 14, fourth job. **Two harnesses**, selected with `-Harness`:

| `-Harness` | Entry point | Why |
|---|---|---|
| `iomage_load` (default) | `P2PmsgMgr::Load` | C4 was a parser trusting a size it read out of the buffer it was parsing, and `../C4LoadTest.cpp` pins one input against it. This drives the same entry point with inputs nobody wrote by hand |
| `recv_image` | heap-from-image, **then traversal** | where **F4–F7** lived. All four were found by `p2p_fuzzframe`, which is in Targetcore and cannot run here — so the path behind most of this library's known memory unsafety had no harness of its own until this one |

```powershell
pwsh tests\fuzz\build_run_fuzz.ps1                                   # iomage_load, 60s
pwsh tests\fuzz\build_run_fuzz.ps1 -Harness recv_image -Seconds 3600
pwsh tests\fuzz\build_run_fuzz.ps1 -Triage                           # keep going past crashes
pwsh tests\fuzz\build_run_fuzz.ps1 -Harness recv_image -Repro out\fuzz\artifacts\recv_image\crash-abc123
```

Everything either one produces goes under `out\fuzz\`, which `.gitignore` already covers.
Work directories and artifacts are **per harness**; `corpus/` is **shared**, because both
harnesses take a raw image as their input and nothing else, so a seed means the same thing
to each and either one's discoveries are legitimate seeds for the other.

## Current state — both green again, and `recv_image` has now found four defects

`iomage_load`'s first ninety-second run found **F1**; that is closed, and the harness has
since run an hour clean on the schedule.

**`recv_image` found a heap-buffer-overflow on its fourth executed unit**, from a mutation
of a tracked seed, about twenty minutes after the harness first compiled. Fixing it exposed
a second. Both are closed:

- **F8** — `P2PmsgHeap_Sizeof(hHeap, aVBLock)` translated a wire offset with the *unchecked*
  `Addr2Phys` and read a multi-byte block header at the result. That is, verbatim, the
  failure the `Addr2PhysChk` declaration in `MsgVBHeap.h` was written to warn about: the
  guard existed and this call site had never adopted it.
- **F9** — `P3PmsgObject::Connectx` ran its F1 span check only when the caller left the size
  to be worked out, so a caller that supplied one skipped the bound entirely and the next
  accessor read the header off the end. Exposed *by* the F8 fix, which is F2's sequencing
  again.

- **F10** — unbounded memory growth in `P2PmsgHeap_AllocBSTRio`. Found by the harness's
  **mutation** phase, added afterwards to reach the allocator: a 65-byte request against a
  1 MB arena drove a single `malloc` past 256 MB. If the image's free list cannot satisfy a
  request, the allocator grows the arena 20% and retries — forever, if the list is corrupt.
  Exhaustion rather than corruption, and reachable by loading a crafted store and writing to
  it. Both allocators now bound the retry.

- **F11** — a **hang**, not a crash, in `P2PmsgHeap_CollateBSTRio`. Found by the schedule on
  2026-08-16 and reported as a libFuzzer *timeout*, which is why it sat for two days looking
  like a flake: a `timeout-` artifact reads as infrastructure noise in a way a `crash-` one
  never does. It is not noise. One 3,487-byte input, a 2,032-byte arena and a 5-byte request
  ran for **over 150 seconds** and was still going.

  Collate absorbs the next physical block and loops. Its termination argument is that the
  absorbed size makes `nSizeof` grow, so the walk eventually leaves the buffer — and that
  size comes out of the image. A **zero-sized** neighbour makes the pass a no-op, and the
  `uVBLockDefs = 0` the loop writes makes that neighbour read as *free* again next pass,
  because `VBLock_IsFree` is true when the type bits are zero. Identical state at `TOP`,
  for ever.

  **`VBLock_Addr08 == 0` is why nothing caught it.** Zeroing the neighbour clears its address
  bits, so the existing `uVBLock != uVBLockNext` refusal fires on the second pass for Addr16,
  Addr32 and Addr64 — but on an 8-bit-addressed image `0 == 0` and the guard is vacuous. An
  image declares its own addressing width, so it picks the one arm where the check does not
  fire. Confirmed rather than reasoned: the fix's diagnostic prints `uAddrType=0`.

  Fixed by requiring the collation to make progress. Both arms refuse a free neighbour of
  zero size. The reproducer now replays in **70 ms**.

- **F11b** — while fixing F11, the **free-list walks in the allocators** turned out to be
  unbounded, and F10 had not bounded them. Read F10's own note: it names a *cyclic* list as a
  cause and then says the walk "falls through" — a cyclic list does not fall through, so
  control never reaches the retry F10 bounded and its counter never increments. The same
  guard was also missing from `P2PmsgHeap_AssertValidBSTRio`, whose IOMAGE twin has had it
  since F3.

  **This is not what fixed F11**, and the label is separate for that reason: with the bound in
  place the counter never fired on the reproducer, and the hang persisted until the Collate
  defect was found. No input has yet been seen to make a free list cyclic. The bound stays
  because the walk was genuinely unbounded, not because anything reached it.

- **F11c** — **found in a green run, by looking at how big its log was.** Run `32043591227`,
  the post-merge confirmation for F11, passed both legs and wrote a **27 MB** step log:
  157,875 report lines over 58,514 runs. The reports come from a per-block loop over structure
  that came off the wire, so one malformed image emits one per block, and a peer can drive an
  unbounded volume of log writes. Same shape as F11 — attacker-chosen input consuming an
  unbounded amount of something — and no gate here looks at report volume, which is why a
  passing run could carry it.

  **The dominant line was not the new guard.** 152,692 of the 157,875 (96.7%) are
  `Internal address corruption`, the branch next door, emitting per block since long before
  F11; the zero-size guard added for F11 accounts for 5,183 (3.3%). It was first reported the
  other way round, off the emitting *function* name rather than the message.

  Fixed by budgeting the **report** and never the refusal: first occurrence per heap, the rest
  counted on the handle, tally emitted by `P2PmsgHeap_Close` — so nothing goes unrecorded, and
  both refusals still run on every block. Reproducer 16 report lines → **6**.

  Measured by run `32046102621` against `32043591227`, both green: `recv_image`'s step log
  **26,991,265 → 7,203,030 chars** and **2.6981 → 0.7830 report lines per run, a 3.45×
  reduction**, with 14,326 tally lines accounting for what is no longer printed individually.
  `iomage_load` emitted 0 report lines in both runs — **the flooding was always one leg's
  problem**, so a green tick on the matrix says nothing about both.

  A local 120 s run put the reduction at 11.3×. It is **3.45×**; the local corpus hits these
  paths with a different distribution of hits-per-heap, and the payoff depends on precisely
  that. Do not size a budget like this from a local corpus.

  **`recv_image` did not get faster — 97 → 76 exec/s.** The reports had been the suspected cause
  of that leg's throughput against `iomage_load`'s 2,448, and bounding them did not raise it. The
  cached corpus also grew from 6 files to 357 between the two runs, so the comparison cannot
  identify the real cause; it establishes only that the reports were not it. Open question, and
  a live one for coverage: the two legs execute at wildly different rates for the same 600 s.

  **Beware `-Repro` when measuring a fix.** `build_run_fuzz.ps1` skips the build when `-Repro`
  is given (`-not $SkipBuild -and -not $Repro`), so a before/after comparison run that way
  measures the same binary twice. It read as "the fix does nothing" for one round here. Build
  with `-BuildOnly` first, then replay.

F8 and F9 are pinned by `MsgcoreSuite::Test_ImageAddressBounds` and their reproducer replays
clean.

**F11 has its seed but not yet a unit case**, and that is a gap in this document's own rule
that the fix and the case arrive together. `Collate` is not exported, so a case has to reach
it through `P2PmsgHeap_Alloc` with a byte-crafted Addr08 image, and the 3,487-byte reproducer
could not be minimised the usual way because `-minimize_crash` needs a binary that still
times out and the fix landed first. What exists instead is
`corpus/f11_collate_nogrow.dat`, which every run replays. Write the case when the layout is
next in hand.

### Why this harness mutates, and why the cap on how much

Traversal reaches the accessors; it never reaches the allocator. The free-list relinking in
`ResizeIOMAGE`, `ResizeBSTRio` and `IO_Split` translates wire-derived links with the
unchecked `Addr2Phys` — and unlike F8, **those translations are followed by writes**. Only
alloc/free gets you there, and a receiver allocates: it does not merely read a frame, it
builds from one.

The mutation phase caps the arena at 1 MB before asking for more and keeps requests under
64 bytes. That cap is load-bearing rather than tidiness: uncapped, growth walks RSS into
libFuzzer's `rss_limit` and the run exits 71 **with no artifact**, which presents as a
finding and tells you nothing. If you are chasing an OOM here, add `-malloc_limit_mb=256` —
it converts "the process grew to 4.7 GB" into a stack naming the single allocation, which is
how F10 was identified.

**Do not suppress a finding to get a green tick.** A fuzz job that is green because it was
told to be is worth less than no fuzz job, because it also tells you the *next* finding is
not there. Exit criterion 2 is blocked on the finding, not on the job.

That the first harness ever pointed at this path found two defects the same day is the
observation the readiness document keeps recording: every instrument stood up here has found
something its plan did not predict. It is also the argument for the next piece of work —
about twenty other unchecked `Addr2Phys` call sites in `MsgVBHeap.cpp` have F8's exact
shape and none of them has been walked.

## What counts as a finding

Not `Load` returning `FALSE`. Not a thrown `P2Pevent*` either — that is the library's own
structured refusal, and the C4 gate itself throws. The harness swallows both, deliberately;
rejecting malformed input is the correct behaviour and most inputs are rejected.

A finding is **a crash, an ASan report, or a hang**: memory unsafety reached through a path
that was supposed to have validated its way out first.

## Layout

| Path | What |
|---|---|
| `fuzz_iomage_load.cpp` | harness — writes the input to a file, calls `P2PmsgMgr::Load` |
| `fuzz_recv_image.cpp` | harness — builds a heap from the input **in memory** and walks it, the way a receiver does. No temp file, so it runs roughly an order of magnitude faster |
| `build_run_fuzz.ps1` | build + run + repro for either, `-Harness` selects; locates its own toolchain like `../build_run_c4.bat` |
| `corpus/` | **seeds only**, tracked. Shared by both harnesses. Not a working corpus |
| `findings/` | minimal reproducers for findings that are still open — **empty again** |

### `corpus/` is a floor, not a working set

The count that used to open this paragraph has been removed, and its history is the reason:
it said "three" when there were already four, and then "five" when F11's seed made six. A
list has to be edited to become wrong; a number goes stale on its own. So the seeds are
listed and not counted:

| Seed | What it covers |
|---|---|
| `valid_store.dat` | a real `P2PmsgMgr::Save` output. Named in item 14 |
| `c4_evil_oversize.iom` | the C4 regression input, header declaring ~16 MB inside 4 KB. Named in item 14 |
| `f1_bstrio_oob.dat` | promoted when F1 was fixed (item 13). The only seed that reaches the **BSTRio** arm |
| `f2_walk_oob.dat` | promoted when F2 was fixed — the block-chain validator's out-of-bounds read |
| `f8_sizeof_hdr_oob.iom` | promoted when **F8 and F9** were fixed. The address at offset 8 is the arena size exactly, which is the case a start-only bound lets through |
| `f11_collate_nogrow.dat` | promoted when **F11** was fixed. A BSTRio image whose width is `VBLock_Addr08`, carrying a free block whose physical neighbour declares **size zero** — the input that made collation loop without advancing. The reproducer unchanged at 3,487 bytes, because minimising a *timeout* needs the pre-fix binary |

Every one after the first two arrived the same way: it crashed something, the something was
fixed, and the bytes stopped crashing and started seeding.

libFuzzer writes newly interesting inputs into the **first** corpus directory it is given,
so the runner passes a scratch directory first and this one after it, read-only. That is
not fussiness — the first ad-hoc run of this harness was pointed straight at `corpus/` and
grew it from 2 files to 29 unreviewed blobs in ninety seconds.

The accumulated corpus lives in the CI cache and is uploaded as a run artifact. Promote
something into `corpus/` only deliberately, and only if you can say what it covers.

### `findings/` holds reproducers, not seeds — and is empty

It held `F1-bstrio-oob.dat`, 49 bytes, which was deliberately *not* passed as a seed: a
crashing seed kills every run during corpus load, so no other path would ever have been
explored.

**F1 is fixed (item 13), and the file has made both moves this section called for.** It is
now `C4LoadTest` scenario `[9]`, byte for byte, which is the pattern the document sets —
the fix and the case that would have caught it in the same commit. And because it no
longer crashes, the objection to seeding it has gone with the crash, so the same bytes now
also sit in `corpus/f1_bstrio_oob.dat`.

That promotion is deliberate and it covers something the other two seeds do not: it is the
only seed that reaches the **BSTRio** arm of `Load`'s dispatch. `c4_evil_oversize.iom` is
hand-built to take the IOMAGE arm — its low byte is `0xFF` precisely so `IsBSTRio` is false
— which is the same blind spot that let F1 live. A seed that only exercises rejection is
still worth keeping: it gives the mutator a foothold in a branch it otherwise has to
rediscover, and it fails loudly if the rejection ever stops happening.

**F8 and F9 made the same round trip, in a day.** The reproducer sat here while they were
open and moved to `corpus/f8_sizeof_hdr_oob.iom` when they closed, and the cases that would
have caught them are `MsgcoreSuite::Test_ImageAddressBounds` — the fix and the case in the
same commit, as above. It went to the unit suite rather than to `C4LoadTest` because these
are not load-path defects: they are reached by resolving an address inside an image that
has already been accepted, which is the receive path's shape, and the suite can call the
two entry points directly.

An empty `findings/` is the state to want. Git does not track empty directories, so it is
held by a `.gitkeep`; when the next finding arrives, its reproducer goes here and this
section says so again.

## Three things about the build that are not obvious

**No clang-cl.** Item 14 assumed `clang-cl -fsanitize=address,fuzzer` was required. It is
not: VS 2026's MSVC ships both, including `clang_rt.fuzzer_MD-x86_64.lib`, so
`cl /fsanitize=address /fsanitize=fuzzer` produces a libFuzzer binary with the same
compiler as the rest of the tree. For an MFC extension library that is worth a great deal
more than a saved download.

**ReleaseLib, not DebugLib**, for two reasons and one of them is uncomfortable. ASan is
incompatible with `/RTC1`, which the Debug configurations set. And Release is what a
consumer links — which also means the 277 `ASSERT()` sites that hold structural validation
are compiled out. So this fuzzes the binary as shipped rather than the one with the checks
in it, and a crash found here may be one a Debug build would have refused politely. That
gap is item 19.

**The library is coverage-instrumented through the `CL` environment variable.** Without it
libFuzzer sees only the harness translation unit — 640 counters, no feedback from inside
the parser — and degrades to random mutation of the seeds. With it, 15,960 counters and a
corpus that grows. `AdditionalOptions` is `ClCompile` *item* metadata, so
`/p:AdditionalOptions=...` on an MSBuild command line goes nowhere; `cl.exe` reads `CL`
itself. The runner asserts the counter count afterwards and fails below 8,000, because a
blind fuzzer still reports a tidy "no crash" and that is a wrong green nobody would notice.
