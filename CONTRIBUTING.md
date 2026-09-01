# Contributing to Msgcore

## Before you spend time on a change

This repository is **pre-release and not self-contained** — a fresh clone does not compile.
It needs the sibling `Platform/` repository checked out beside it, and the parent tree's
`Directory.Build.props`, which is not published here. See
[The sibling dependency](Readme.md#the-sibling-dependency-platform) for the expected layout.
The release-readiness register has the current state of that and everything else. Until it
is resolved, an outside contributor cannot build what they are changing, which makes
anything beyond a documentation fix hard to do well.

Issues and reports are welcome regardless. For **security** issues, do not open a public
issue — see [`SECURITY.md`](SECURITY.md).

## Sign your work — the Developer Certificate of Origin

Every commit must carry a `Signed-off-by` line:

```
Signed-off-by: Jane Developer <jane@example.com>
```

`git commit -s` adds it for you. Use your real name and an address you read.

That line means you certify the [Developer Certificate of Origin
1.1](https://developercertificate.org/): that you wrote the contribution or otherwise
have the right to submit it under the Apache License, Version 2.0, and that you
understand the contribution and its record are public and permanent.

**Why this is enforced from the first commit rather than added later.** Only a rights
holder can license code. Once a contribution arrives with no record of who held the
rights and under what terms, the project can no longer answer that question for its own
tree — and the option of ever relicensing, dual-licensing, or granting an exception
closes permanently, because there is nobody identifiable to ask. A sign-off is the
cheapest possible way to keep that answerable. A pull request without one cannot be
merged, no matter how good it is.

## Making a change

1. **One concern per commit.** A refactor and a behaviour change in the same commit
   cannot be reviewed, reverted or bisected independently.
2. **Write the message for someone reading it in five years.** Say what changed and
   *why* — the subject line as an imperative sentence, the body for the reasoning. The
   existing history is the standard to match: "Repair the Msgcore paths nothing had ever
   executed", "Fix the EVWRN macro, which had never compiled". Both say what was wrong.
3. **Build all eight configurations.** `Debug` / `Release` / `DebugLib` / `ReleaseLib`
   × `Win32` / `x64`. They now share one diagnostic regime — `/W4`, `/std:c++17`,
   `/std:c17`, `/sdl` — but they still differ in linkage shape and optimisation, and a
   change that compiles under one can still fail under another. Zero errors is the
   standing bar.
4. **Run both test programs**, in both link modes if you can:
   - `.\tests\build_run_suite.bat` — the unit suites (83 cases / 334 checks in
     `static`, 329 in `dll`: the heap API the F8/F9 cases drive is not exported,
     so those checks have nothing to reach across a DLL boundary).
   - `.\tests\build_run_c4.bat` — the load-path security regressions.

   If your change touches the load path, the heap or the block allocator, **add a
   scenario to `C4LoadTest.cpp`**, and make it assert on *which* check refuses rather
   than merely that something did — malformed input is usually malformed enough that
   an unrelated check fires first, and a scenario that cannot fail on the old code is
   not a regression test. If your change touches a container, a cursor or the flat C
   ABI, add a case to the suites instead.
5. **Do not add a warning.** The counts are held against a per-configuration baseline in
   `.github/workflows/ci.yml` and CI fails when one rises. If you drive a count *down*,
   bank the new baseline in the same commit — a stale ceiling silently re-admits every
   warning between the old figure and the new one.

## House style

Match the file you are editing — it is older than any convention document and it is
internally consistent. In particular:

- Spaces inside parentheses in calls and declarations: `Foo ( a, b )`.
- `NOTES:` comment blocks above a declaration explain *why*, not what. When you fix
  something subtle, leave the reasoning behind in one of these. The security fixes in
  `MakeIOmage` and `P3PmsgName` are the model: the argument for the bound is written
  next to the bound.
- Do not delete a comment that records a hazard just because you fixed the code around
  it. Rewrite it to say what is true now.

## Two things that will get a change rejected on sight

- **Structural validation inside `ASSERT(...)`.** It compiles out of Release, so it is
  not validation — it is a comment with a debug-build side effect. This is precisely how
  the C4 finding happened. If an input must be checked, check it unconditionally.
- **Passing runtime text as a format string.** `Message(szError)` is a format-string
  sink; write `Message(L"%ls", szError)`. Use `%ls`, not `%s` — it means wide in both
  the MSVC and glibc dialects, which `%s` does not.

## Licence

Contributions are accepted under the Apache License, Version 2.0 — see
[`LICENSE`](LICENSE) and [`NOTICE`](NOTICE). By submitting one with a sign-off you
confirm it may be distributed under those terms.
