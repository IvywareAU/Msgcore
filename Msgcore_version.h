// Copyright © 2026 Khrustal & Mann
//              MELBOURNE, VICTORIA, AUSTRALIA, 3000
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
//  Msgcore_version.h - the single source of version identity.
//
//  NOTES: This header is the ONLY place a version number is written. It is
//         consumed by two parties that must never disagree:
//           - Msgcore.rc          -> the DLL's VERSIONINFO resource
//           - Msgcore.h / Msgcore_c.h -> the compile-time macros a consumer
//                                        tests against
//         Bump it here and both of them move together.
//       : A third consumer, com\MsgcoreCom.rc, was listed here until c88f9f5
//         moved com\ to the MsgFacade repository. If that server still builds
//         its VERSIONINFO from a copy of this header, the two version
//         identities are now independent and can drift.
//       : It must stay preprocessor-only. rc.exe compiles it as well as the
//         C++ compiler, and rc.exe understands #define and nothing else - no
//         types, no enums, no inline functions, no const. Anything that is
//         not a macro belongs in another header.
//       : Keep the release tag and this file in step: version 3.4.7 is tag
//         v3.4.7. A build whose DLL reports a version no tag matches cannot
//         be traced back to a source state, which defeats the point.
//       : What that rule is about is a PUBLISHED binary. The number moves here
//         in the same commit that moves the supported surface, so the header
//         and tools\ci\exports-flat.manifest cannot disagree about what 3.1.0
//         contains; the tag is applied when the release ships. aa0366b got
//         that backwards - it tagged v3.1.0 before the release was ready - and
//         355cff5 took both the tag and the number back. The number is free
//         again, and this is the first change since v3.0.0 that actually moves
//         the supported surface.
//
#pragma once

//  Component version. MAJOR.MINOR.PATCH is the released identity; BUILD is
//  reserved for a CI build counter and is 0 for a hand-built binary.
//
//  4.0.0.0, a MAJOR on 3.8.1. Stands alone: no Targetcore release is needed, but
//  every C++ consumer must be REBUILT against these headers. The flat C ABI is
//  unchanged (285 exports, the same names on x64 and Win32), as are the image
//  format and saved files. What moves is the mangled C++ half: every class
//  layout below changes size, and three P3PmsgObject members are exported.
//
//  THE HANDLE DIET (MSCS performance log P5 / plan M4). A field handle carried
//  1.8 KB of buffers it rarely used, and a cursor carried three of them:
//    - P3PmsgData's two ToString buffers (2 x 256 WCHAR) are allocated on the
//      first ToString / ToStringDefs and freed with the object; the pointer the
//      call returns lives exactly as long as it did.
//    - P3PmsgObject's 704-byte inline item block (m_oVBLock[88]) became a
//      pointer, m_pInline, allocated only when an item is rendered into it.
//      Freed blocks go to a per-thread cache of up to 16 (about 11 KB a thread
//      at most) and come back zeroed, as the array was.
//    - A scalar P3PmsgData renders into an embedded P3PmsgObject instead of a
//      new one per value.
//    - P3PmsgCurs no longer default-renders its Field, List and Vect members,
//      and its copy assignment now Nullifies them rather than copying them:
//      the cursor re-renders them on the next Goto, as it always had to.
//    - P3PmsgVect::Goto reuses its element wrapper while the element kind is
//      unchanged, rather than deleting and re-creating it per element.
//  Sizes (x64): P3PmsgField 1,856 -> 200 B, P3PmsgCurs 5,736 -> 768,
//  P3PmsgData 1,048 -> 88, P3PmsgObject 752 -> 56, P3PmsgBSTR 1,880 -> 224.
//  New exports (whole-class export ships them, private or not):
//  P3PmsgObject::InlineVBLock, ::IsInlineAddr, ::kInlineSize; nothing removed.
//  Measured against 3.8.1, median of interleaved rounds, each side its own
//  build: msgcore_bench Windows 39 of 76 cases >= 3% faster, 1 slower
//  (vect_walk -43..-57%, lookup_miss -16..-23%, build_nested -6..-14%,
//  build_flat -3..-4%); Linux 18 faster, 5 slower by 3-4% on sub-3 us cases;
//  allocations per op down wherever non-zero (build_flat 512: 539 -> 25).
//  MsgFacade bench, every layer down: cpp -4.6%, ref -3.1%, C API -4.5%,
//  facade -3.6%, COM -3.2%. Targetcore perf_ab one-way latency -3..-6% and
//  throughput +7..+13% at 64 B to 32 KB. The one slower micro-case, a 32 KB
//  recv_adopt on Windows (+50%, page faults in the NT heap around the buffer),
//  does not reach a real receive.
//  MAJOR: class layouts and the mangled C++ surface change; the flat ABI and
//  every on-disk and on-wire byte do not.
//
//  BELOW IS THE 3.8.1 RATIONALE.
//
//  3.8.1.0, a PATCH on 3.8.0. Build-only: no source line changes. Stands alone;
//  the flat ABI, the mangled C++ exports, every class layout, the image format
//  and saved files are unchanged.
//
//  /QIntel-jcc-erratum ON EVERY OPTIMIZED MSVC BUILD (p2p_performance F-P4-13).
//  Skylake-derived Intel cores (SKX102) stop caching decoded jumps that cross
//  or end on a 32-byte boundary, so Msgcore's timings moved with code layout.
//  The vcxproj sets IntelJCCErratum for every non-Debug configuration; CMake
//  adds the flag to msgcore and msgcore_static for MSVC (not clang-cl) only.
//  Measured on the reference i7-8700, Release x64, msgcore_bench, four padded
//  layouts per side, 10 interleaved rounds: 13 cases faster at every layout
//  (build_flat -8..-10%, lookup_alt 512 -17.8%, recv_adopt 512 -7.2%), none
//  slower at every layout; the layout spread shrinks (build_flat 512 5.9% ->
//  1.0%). Msgcore.dll grows 4.7%. Not measured on CPUs without the erratum,
//  where the padding is expected to cost little. Linux GCC builds unchanged.
//  PATCH: same code, different instruction placement.
//
//  BELOW IS THE 3.8.0 RATIONALE.
//
//  3.8.0.0, a MINOR on 3.7.1. Stands alone: no Targetcore release is needed. The
//  flat ABI, the mangled C++ exports, every class layout, the image format and
//  saved files are unchanged; nothing new is exported.
//
//  THE "DEFINITELY NEW" NAME FILTER (MSCS performance log F-P4-12, plan M3
//  option A). Building a message was quadratic: each DeclareItem first proves
//  the name is new by walking every sibling, so n fields cost n^2/2 compares.
//  A heap now keeps, per collection of 32 or more children, a small hash set of
//  its children's names. A lookup whose key is not in a CURRENT set returns
//  "absent" without walking. A miss walk over a collection with no current set
//  files one; DeclareItem keeps it current across its own append.
//    - Currency is a heap-wide shape stamp, bumped by everything that could
//      add or change a name (links, renames, name resizes, swaps, frees,
//      inline-chain moves, Attacheap). A set is believed only while it carries
//      the heap's current stamp: any bump retires every set on the heap, and
//      a retired set only costs the walk it would have skipped.
//    - Only keys the M3 sibling test can judge (0..63 units of 7-bit ASCII, no
//      NUL, in a locale that folds I/i) are filtered; any child it cannot
//      judge (chained, non-ASCII) means no set for that collection.
//    - Runtime-only: the sets live in the heap handle, never in the image.
//    - A Debug build (or -DP2P_NAME_FILTER_CHECK) walks anyway after an
//      "absent" and throws NAME-FILTER-MISMATCH if the name is there.
//  Measured (median of 7 interleaved rounds): build_flat 512 fields -69%
//  Windows / -74% Linux, now linear in n; lookup_miss 512 -96%. At 64 fields
//  or fewer the added work is +0.3..+0.7% retired instructions (xperf PMC,
//  layout-independent); wall-clock there moves with code layout on the
//  reference i7-8700 (JCC erratum), not with this change.
//  MINOR: a new capability with an observable cost profile, no surface change.
//
//  BELOW IS THE 3.7.1 RATIONALE.
//
//  3.7.1.0, a PATCH on 3.7.0, paired with Targetcore 3.10.1. The flat ABI, the
//  mangled C++ exports, every class layout, the image format and saved files are
//  unchanged.
//
//  BOUNDED OBJECT-GRAPH TRAVERSAL (MSCS performance log F-P6-6). The checks that
//  accept a received image prove its BLOCK structure (MsgVBHeap, F12-F14); they
//  do not walk the OBJECT GRAPH those blocks form. A frame could pass them and
//  still carry a forged graph - a sibling aNext/aPrev chain linked into a loop,
//  or a descendant/attribute pointer aimed back at an ancestor. Reading one hung
//  the reader (P3PmsgCurs::Goto walked the sibling loop forever - reached by
//  GetSource() on every inbound message) or overflowed the stack (the AssertValid
//  family and the deep-copy recurse through descendants without a depth limit).
//  Both are now bounded: the two Goto loops stop at the heap's byte extent (a
//  chain cannot have more items than the heap has bytes), and the tree recursion
//  at a fixed nesting depth; a forged graph raises a P2Pevent instead. A
//  well-formed message is unaffected. PATCH: a bug fix, no surface change.
//  Targetcore 3.10.1 is what calls the now-safe AssertValid on every received
//  message, at its ingress gate.
//
//  BELOW IS THE 3.7.0 RATIONALE.
//
//  3.7.0.0, a MINOR on 3.6.2, paired with Targetcore 3.10.0. Two changes; the
//  flat ABI, the mangled C++ exports, every class layout, the image format and
//  saved files are unchanged.
//
//  (1) g_bP2Pmsg_AssertValid DEFAULTS TO FALSE IN RELEASE (MSCS performance log
//  F-P6-5). It was true, and Targetcore's post paths read it to validate every
//  outbound message: 25% of all CPU in a two-process round trip. Debug is
//  unchanged (true). The environment variable P2P_ASSERT_VALID, anything but
//  "0", turns it back on, read once at load; so does setting the variable.
//  Nothing in Msgcore reads it and it never covered bytes off a wire: the
//  ingress checks on an adopted image do not consult it. MINOR, not PATCH,
//  because a Release process that relied on the default sees less checking of
//  the messages it builds itself.
//
//  (2) Platform/p2pshm.h, NEW: the same-host shared-memory primitives under
//  Targetcore 3.10.0's transport (two byte rings in one sealed mapping, park
//  words, a wake per side that lands in a completion port, and handing both to
//  the peer process). Header-only, no exported symbol; Msgcore itself does not
//  include it.
//
//  BELOW IS THE 3.6.2 RATIONALE.
//
//  3.6.2.0, a PATCH on 3.6.1: BUILD SETTINGS ONLY, no source change. The
//  Visual Studio project's Release and ReleaseLib configurations were
//  compiled /Ob1 (InlineFunctionExpansion=OnlyExplicitInline) where the
//  CMake build is /Ob2; they are /Ob2 (AnySuitable) now. Measured (MSCS
//  performance log F-P4-8; same perf_ab.exe, 3.6.1 code): the msbuild DLLs
//  read one-way 105.0 us and 21.9k msg/s against the CMake DLLs' 88.7 us
//  and 27.9k, and /Ob1 alone reproduced the gap. With Targetcore 3.7.4 (the
//  same fix) the msbuild DLLs read ~94 us and ~26.4k msg/s.
//
//  /sdl STAYS, AND IT IS PART OF THE C++ EXPORT TABLE. The ~4% left is /sdl
//  (measured alone). Turning it off was tried and refused by CI: /sdl's
//  class-member initialisation makes the compiler emit __autoclassinit2 for
//  each exported class, 23 per architecture in the manifest, and a consumer
//  compiled with /sdl (the Visual Studio default) calls it through the
//  import table on every new. Dropping /sdl removes those exports, so it is
//  an export removal, not a build setting, and it waits for a release that
//  can carry one. The CMake DLL is built without /sdl and has never exported
//  them. Exports and layouts unchanged here.
//
//  BELOW IS THE 3.6.1 RATIONALE.
//
//  3.6.1.0, a PATCH on 3.6.0 (the MSCS performance plan's T7, step 1). Two
//  function bodies; the flat ABI, the mangled C++ exports, every class layout,
//  the image format and saved files are unchanged.
//
//  THE LOOKUP'S DESCRIPTOR AND CURSOR ARE ON THE STACK. The field arm of
//  P3Pmsg_SelectObjectRecurse copies the field into a temporary and looked the
//  name up through r_Desc().r_Curs() on the copy: a heap P3PmsgDesc and a heap
//  P3PmsgCurs per path step, freed at the end of the scope. Every Exists() and
//  one-level SelectObject() paid both. They are locals now; the Goto, the
//  retry for a stored name containing a delimiter, and the answers are the
//  same. On the Targetcore message path (P2PeerMsg's address accessors) that
//  was ~40 of ~86 allocations per delivered message (heaptrack, finding
//  F-P4-7).
//
//  LINUX: drain_queue (Platform/p2piocp.cpp) built a std::deque per
//  GetQueuedCompletionStatus for a list that is almost always empty, and
//  libstdc++'s deque allocates when constructed. A vector now.
//
//  RESULT (5 rounds alternated, same binaries, base = 3.6.0 built in the same
//  tree): allocations per message, Linux perf_ab 86 -> 34 one-way, 79 -> 34
//  pipelined; B5 fan 147 -> 51. Windows perf_ab throughput +5.7% (CPU/msg
//  98 -> 93 us), one-way latency -4.0%; fan C=64 thr_last +4.9%. Linux VM
//  perf_ab throughput +3.2%, latency flat.
//
//  BELOW IS THE 3.6.0 RATIONALE.
//
//  3.6.0.0, a MINOR on 3.5.3 (the MSCS performance plan's M5, and the defect
//  its review found). The flat ABI is unchanged (285). The mangled C++ half
//  gains ONE export, P3PmsgCurs::IsAt (773 on x64 and Win32, the export tables
//  compared whole); no class layout moves. MINOR, not PATCH, because
//  MsgFieldRef.hpp -- header-only, compiled into the consumer -- now calls
//  IsAt, so a consumer built against 3.6.0 headers cannot load a 3.5.x DLL.
//
//  THE DEFECT, older than the 3.x line. A cursor belongs to one P3PmsgDesc or
//  P3PmsgAttr object, and two P3PmsgField objects over one node have two of
//  them. A child deleted through the OTHER object was freed under this one's
//  cursor, which still stood on it -- a free clears the type bits and puts the
//  block at the head of the free list, and nothing tells other cursors. The
//  quick tests in P3PmsgCurs::Goto believed the cursor without looking:
//    - Goto(name) read the freed block's name, the name checks threw, and the
//      walk never ran. EVERY lookup through that object threw, for any name,
//      until the block happened to be reused by a well-formed item. The C API
//      turned the throw into a null, so msgcore_field_child answered "no such
//      child" for one that exists -- 3.5.2 and 3.5.3 alike.
//    - Goto(int) returned the freed block; the unit suite failed fast
//      (0xC0000409) when a test read it.
//  Both quick tests now ask GotoQuick_StillHere first (MsgCurs.cpp): the
//  block is inside the heap, allocated, an item of the kind and size the
//  cursor connected, and names THIS collection as its parent. Any failure
//  sends Goto on to its walk, so a false "not here" costs a walk and never a
//  wrong answer. A SYSTEM heap (raw memory, no bound to read against) keeps
//  the old quick test. Not a generation: a block taken again at once by an
//  item of the same size under the same parent passes, and the name compare
//  then decides, as it did.
//
//  M5. MsgFieldRef's read was Exists() then SelectItem(): the path
//  resolver's fresh-cursor walk of every sibling, then the parent's own
//  cursor -- which already stood there on a repeated read. Has() now asks
//  P3PmsgCurs::IsAt first: the parent's cursor on a live child of that name,
//  no walk, no move. Only for a plain name (1..31 characters, none of the
//  path grammar's . @ ^ :), where the walk's answer is the same; any other
//  name, and any SYSTEM heap, asks the walk as before. An Exists() that
//  misses still moves no cursor.
//
//  RESULT (msgcore_bench, 3.5.3 and 3.6.0 interleaved, 7 rounds, p50, Windows
//  Release): fieldref_read of the last of N siblings 729 ns -> 286 ns at 1,
//  1.05 us -> 286 ns at 8, 2.55 us -> 286 ns at 64, 13.46 us -> 285 ns at 512
//  (47x, flat in N). The check's cost, on every quick-test hit: lookup_repeat
//  112 -> 126 ns; lookup_alt and lookup_miss +4-5% at 8 siblings, within 3%
//  from 64; writes +3-4%. Linux VM (3.5.2 base, 5 rounds): fieldref_read
//  38.0 -> 1.07 us at 512, lookup_repeat 455 -> 486 ns.
//  MscsUnitTests: "cursor: a child freed through another object is not
//  believed", "cursor: IsAt answers from where the cursor stands, and never
//  moves it", "MsgFieldRef: reads ask the cursor first, with the walk's
//  answers", "field_child: a child deleted through another handle answers
//  null" (the last fails on 3.5.2 and 3.5.3).
//
//  BELOW IS THE 3.5.3 RATIONALE.
//
//  3.5.3.0, a PATCH on 3.5.2 (the MSCS performance plan's M11: the layer
//  tax). One function body in Msgcore_c.cpp; the flat ABI and the mangled C++
//  exports are unchanged (285 / 772, the export tables compared whole).
//
//  msgcore_field_child WALKED TWICE. It was Exists() and then SelectItem():
//  the path resolver's walk to ask, then the descendant cursor's Goto to
//  fetch. B9 (msgfacade_bench) measured 1.5 us at 8 siblings and 15.2 us at
//  512 - ten times the C++ lookup. It is now r_Desc().SelectObject(name):
//  the same cursor and the same Goto, a void object on a miss instead of a
//  throw. The answer was "Exists AND Goto found it", and for any name the
//  core accepts (its validator refuses the path grammar's '.' and '@') the
//  resolver's walk is that same Goto, so the second alone decides. A list or
//  vect child still answers null: SelectItem returned the cursor as a
//  P3PmsgField&, and that conversion refuses one.
//
//  RESULT (B9 capi column, 3.5.2 and 3.5.3 interleaved, 7 rounds, p50,
//  Windows Release): field_child 1.58 us -> 468 ns at 8 siblings, 3.17 us ->
//  466 ns at 64, 15.2 us -> 484 ns at 512; a root-to-leaf walk through it at
//  depth 16 32.7 -> 17.1 us. MscsUnitTests' "field_child: hit, miss, case,
//  list/vect child, 512 siblings, repeats" passes on both versions.
//
//  BELOW IS THE 3.5.2 RATIONALE.
//
//  3.5.2.0, a PATCH on 3.5.1 (the MSCS performance plan's M7: the empty
//  message). The flat ABI and the mangled C++ exports are unchanged (285 / 772):
//  the new function is not exported, and P3PmsgDesc gains a friend
//  declaration, which moves neither its layout nor its exports.
//
//  MEASURED FIRST, AND THE PLAN WAS WRONG ABOUT WHERE THE COST WAS. It
//  proposed one allocation for handle and arena, no zeroing, a per-thread
//  arena pool. perf on the empty message (msgcore_bench --profile msg_new):
//  creating the heap is ~5%; 59% is InitItem adding the MSG item through the
//  generic object model - a name lookup on a root with no children (13%), a
//  temporary standalone field (8%), and PushBack copying it in (35%).
//
//  WHAT IT DOES:
//   - P3PmsgBSTR::Exists and r_item's create check ask the root's own cursor
//     (BSTR_HasSkeletonItem), and a root with no children answers without
//     one. P3PmsgField::Exists reached the same Goto through
//     P3Pmsg_SelectObject's path parse, field copy and cursor build; the
//     skeleton names are plain names, so the answer cannot differ.
//   - InitItem appends an absent skeleton item with P2PmsgDesc_AppendField:
//     PushBack ( P3PmsgField ( name, value ) ) without the temporary. Every
//     size term PushBack derived from the temporary is derived from what the
//     temporary was made of (refer MsgDesc.cpp); the value is copied once.
//     Sorted collections and over-long names take the generic route.
//
//  SAME BYTES. MscsUnitTests Test_SkeletonFastPath builds 36 messages both
//  ways (Addr16/32/64 x empty, scalars, short/long/narrow/8-bit strings,
//  blobs 10 / 300 / 3,000 B, the last growing the heap) and compares the
//  images whole: identical on MSVC Debug and Release and on gcc Release.
//
//  RESULT (create + destroy, msgcore_bench msg_new, 3.5.1 and 3.5.2 run
//  interleaved, 7 rounds, p50): Windows Release 5,471 -> 3,283 ns (-40%),
//  Linux 10.4 -> 6.3 us (-40%), allocations 9 -> 7; build_flat at 1 field
//  -24% / -27%. The heap and root alone (heap_only) do not move, 1.5 / 2.6 us.
//  What is left there is P3PmsgBSTR::Init's own temporary (31% of it) - sized
//  at the temporary's addressing width, which is Addr64 on x64 and Addr32 on
//  Win32, so removing it means reproducing a layout that differs by build
//  architecture. Not done here.
//
//  A WINDOWS-ONLY recv_adopt DRIFT THAT IS NOT THIS CHANGE: adopting the same
//  bytes through the same, unchanged code measures +4% at 1 field to +19% at
//  512 on Windows, and is flat on Linux. Ruled out in turn, each run
//  interleaved: the images (byte-identical at 0/1/8/64/512 fields), CRT heap
//  state (it holds in a fresh process timing adoption alone), the new
//  include, the uncalled new function, a stale object (a fresh full build
//  repeats it). One unused function added to this translation unit brings
//  adoption back to 3.5.1's time, so it is code alignment in the validating
//  walk, moved by the link. Not chased further here.
//
//  BELOW IS THE 3.5.1 RATIONALE.
//
//  3.5.1.0, a PATCH on 3.5.0 (the MSCS performance plan's M6: arena growth).
//  tools\ci\exports-flat.manifest and both exports-cxx manifests match
//  unchanged (285 flat, 772 mangled); no header line a consumer compiles
//  against moved.
//
//  WHAT IT DOES. A MESSAGE heap (IOMAGE) that must grow now doubles instead of
//  adding a fifth: P2PmsgHeap_GrowStepIOMAGE, used by P2PmsgHeap_ResizeIOMAGE's
//  oversize bump; the allocation's own need still wins when it is bigger, and
//  P2PmsgHeap_CapGrowth still clamps the step to the addressing ceiling.
//  Measured by MscsUnitTests msgcore_bench's new growth case (8 KB fields,
//  Windows Release, p50 of three interleaved rounds against 1.2x and 1.5x
//  builds of the same source): to 64 KB, 8 -> 4 regrows, 3.8x -> 1.1x the live
//  size copied, 106 -> 95 us; to 1 MB, 23 -> 8, 5.3x -> 1.2x, 1,978 -> 1,312 us
//  (1.5x: 12 regrows, 1,373 us). build_flat at 512 fields does not move: there
//  the cost is the sibling insert (M3).
//
//  WHY IT IS FREE ON THE WIRE. Since 3.5.0 the send path trims the free tail
//  (TrimP2Piomage), so the extra slack lives only while a message is built.
//  Targetcore 3.7.1 also trims before a wrap factory embeds a message, which
//  was the one other place a message's whole arena left the process.
//
//  STORES ARE NOT TOUCHED. P2PmsgHeap_ResizeBSTRio keeps a fifth: Save writes
//  a store's whole arena, slack included, and golden_ref.p2p is that file byte
//  for byte (plan §10.1 decision 3). Store growth is the plan's M8.
//
//  BELOW IS THE 3.5.0 RATIONALE.
//
//  3.5.0.0, a MINOR on 3.4.7 (the MSCS performance plan's M1: live bytes on the wire).
//  The flat C ABI is unchanged (exports-flat.manifest matches, 285); the
//  mangled C++ half GAINS ONE EXPORT, P3PmsgBSTR::TrimP2Piomage (772). That
//  half is called internal, but Targetcore 3.7.0 calls this one and will not
//  load against a 3.4.x DLL, and a PATCH number would hide exactly that.
//
//  WHAT IT DOES. The frame of an unsealed message is its whole heap image, and
//  the image's length is uHiWM - the arena size, not a used mark - so a 64-byte
//  message went out as 2,032 bytes, ~1.4 KB of it one free block at the end.
//  P2PmsgHeap_TrimIOMAGE takes that block (two after a grow) off the free list
//  through IsolateIOMAGE and pulls the heap's end back to where it began. It
//  finds the block from the END through its boundary tag (PrevFree, which
//  believes the block's own header over the tag), so it costs a constant, a
//  stale tag cannot trim a live block, and a tail with no tag is left alone.
//
//  NO RECEIVER CHANGES. The image carries no capacity, only its length in
//  oSync; the adopter checks that the free list and a linear walk fit inside
//  that length and agree with the root's counts, which the unlink keeps exact.
//  That is the shape CompactSealed has sent since 3.4.4 and the shape the C++23
//  bridge's writer sends old peers. Save does not trim: saved images and
//  golden_ref.p2p are byte-identical (plan §10.1 decision 3).
//
//  ONE CHANGE OUTSIDE THE NEW FUNCTION, found by measuring it. CreateIOMAGE
//  rounds the arena (uHiWM, and the free block it lays down) up to a multiple
//  of 8, but kept the size it was asked for in nSizeofAlloc. A heap created at
//  any other size - every CompactSealed copy - therefore had a free tail
//  ending up to 7 bytes past its own extent, StampFoot would not tag it, and
//  the trim could not find it: sealed frames stayed at 1,344 bytes. The heap's
//  extent is now the rounded arena. The image bytes do not change, and every
//  receiver of the image already used the rounded length as its extent.
//
//  Gated by MscsUnitTests Test_WireTrim and its twin in tests/MsgcoreSuite.cpp:
//  the trimmed image is adopted inside P2PmsgHeap_UntrustedGate (the wire's
//  checks, root never repaired), both ends allocate past the trimmed end, a
//  heap of an odd size trims, and a grown heap trims to a valid image. One
//  int: 2,056 -> 307 bytes. Through Targetcore 3.7.0 (perf_fanout, 64 B):
//  unsealed frames 2,032 -> 629, sealed 1,344 -> 1,058, both platforms.
//
//  BELOW IS THE 3.4.7 RATIONALE.
//
//  3.4.7.0, a PATCH on 3.4.6, and strictly one: tools\ci\exports-flat.manifest
//  and exports-cxx-x64.manifest both match unchanged (285 flat, 771 mangled),
//  no Msgcore_c.h line and no exported class layout moved. The heap handle that
//  gained a field is private to MsgVBHeap.cpp; P2PmsgHeapExtent, which gained
//  one too, is internal and not exported, as are the new free functions.
//
//  A SECURITY FIX FIRST. VBLockData_Copy believed the lengths in the SOURCE
//  value's header. A *var BLOB is sized by its capacity and was copied by its
//  used length, so a received value declaring used > capacity wrote past the
//  destination, with the bytes after it in the received image -- the peer's --
//  as the payload; a BSTR/WSTR *var was zero-filled to its capacity past a
//  destination sized by its used length; and nothing bounded the read, since
//  P2PmsgObject_pData bounds a value's header and not the blob behind it.
//  Reachable on the relay path: copying a received message (P3PmsgBSTR's copy
//  constructor, as Targetcore's redirect does) copies every value through it.
//  Now VBLockData_CopyBound refuses used > capacity, a read past the source and
//  a write past the destination, and records an oversized capacity as the
//  room; P2PmsgData_SizeofReadable caps every caller's source size by the room
//  the value's own block really holds. Values this library writes copy
//  unchanged. Found by the independent security review of the change below
//  (MSCS performance log, finding F-P1-9). Pinned by Test_ValueCopyBounds.
//  The same review named the setters' side, and it had the same shape:
//  c_vBlob, c_memcpy and c_wcscpy grew a value only when its DECLARED
//  capacity was too small, then wrote up to it, so a receiver setting a
//  received value that overstated its capacity wrote past the block; c_memset
//  filled the whole declared capacity. P2PmsgData_BlobFits now also asks the
//  block, a value that does not fit is grown as before, and c_memset refuses.
//  Pinned by the case "setting a received value that overstates its capacity
//  grows it" -- checked by restoring the old test, which crashes the suite.
//
//  M2's TRUSTED BIT. A heap one of the EMPTY constructors made (CreateIOMAGE /
//  CreateSYS / CreateBSTRio by size) holds no byte this library did not lay
//  out, so in Release P2PmsgHeap_pNameChkIn skips the per-access name bounds
//  on it -- they guard against a stranger's bytes. The bit is bBuiltHere in the
//  handle, never in the image; it is never set on a heap adopted from bytes
//  and never set after creation, and a creation path that forgets it leaves
//  the heap checked. Received messages are checked exactly as before. Debug
//  still checks everywhere. Measured at 512 siblings, against 3.4.6, on a
//  message built in-process: a lookup -25..-29% on Windows, -15..-16% on
//  Linux; building a 512-field message -22% / -13%; a received message
//  unchanged. Pinned by Test_TrustedBitScope; Test_NameReadBounds now corrupts
//  a received heap, where the checks still run in every build.
//
//  BELOW IS THE 3.4.6 RATIONALE.
//
//  3.4.6.0, a PATCH on 3.4.5, and strictly one: tools\ci\exports-flat.manifest
//  and exports-cxx-x64.manifest both match unchanged (285 flat, 771 mangled),
//  no Msgcore_c.h line and no class layout moved. P2PmsgHeap_pNameChkIn is an
//  internal free function and is not exported.
//
//  THE BOUNDS READ ONCE PER WALK. P2PmsgHeap_pNameChk re-read the heap's image
//  extent (P2PmsgHeap_GetExtent, out of line) and the addressing mode's minimum
//  name size for every sibling a lookup stepped over, though both are the same
//  for the whole walk. P2PmsgHeap_pNameChkIn takes them from its caller;
//  P3PmsgCurs::Goto reads them once before its loop, which only connects
//  cursors and compares names, so nothing can move or grow the image under
//  them. pNameChk is GetExtent + Sizeof_Min + pNameChkIn: one copy of the
//  checks, none dropped, loosened or reordered. Measured at 512 siblings on a
//  received heap, against 3.4.5: a lookup -3% on Windows, -7% on Linux; a miss
//  -9% on both; building a 512-field message -2% / -7% (MSCS performance log,
//  finding F-P1-8).
//
//  BELOW IS THE 3.4.5 RATIONALE.
//
//  3.4.5.0, a PATCH on 3.4.4, and strictly one: tools\ci\exports-flat.manifest
//  and exports-cxx-x64.manifest both match unchanged (285 flat, 771 mangled),
//  no Msgcore_c.h line and no class layout moved. P2PmsgHeap_pNameChk, now
//  declared in P2Pmsg.h, is an internal free function and is not exported.
//
//  A LOOKUP JUDGED EVERY SIBLING THE EXPENSIVE WAY. Goto(name) connected a
//  cursor to each sibling, widened its name (on Linux into a ring of wide
//  strings) and ran a locale-aware compare. Now a pre-filter reads the stored
//  name straight from the image, after the same P2PmsgHeap_pNameChk bounds,
//  and compares ASCII case-folded. It can only answer "different": any sibling
//  it cannot rule out -- a possible match, a chained name, any unit above 0x7F
//  or NUL, a key of 64 units or more, a locale where I and i do not fold to each
//  other -- goes through the original connect-and-compare, which decides every
//  hit. So the first match still wins and the case rules are those of
//  _wcsicmp / wcscasecmp exactly (the Kelvin sign and sharp s included). A
//  Debug build re-checks every skip against the original compare and throws on
//  a disagreement.
//
//  DeclareItem walked the siblings twice on an insert: once to prove the name
//  new, once more to find what it had just appended. PushBack appends last,
//  so it now goes to the last item and confirms the name, falling back to the
//  full walk if that ever fails. Same for P3PmsgAttr::DeclareItem.
//
//  Measured at 512 siblings on a received heap, against 3.4.4: a lookup 12.1
//  -> 6.4 us on Windows (-47%), 70.6 -> 19.5 us on Linux (-72%); a miss and
//  FieldRef reads the same share; building a 512-field message 13.25 -> 4.38
//  ms on Windows (-67%), 74.3 -> 12.9 ms on Linux (-83%). Allocations are
//  unchanged (MSCS performance log, findings F-P1-5 and F-P1-6). New suite
//  case "name lookup: first match wins, ASCII and non-ASCII, SYS and IOMAGE
//  heaps" pins it -- checked by making the pre-filter case-sensitive, which
//  fails it in Debug and Release.
//
//  BELOW IS THE 3.4.4 RATIONALE.
//
//  3.4.4.0, a PATCH on 3.4.3, and strictly one: tools\ci\exports-flat.manifest
//  and exports-cxx-x64.manifest both match unchanged (285 flat, 771 mangled),
//  no Msgcore_c.h line and no class layout moved. The new helpers are internal
//  free functions and header inlines, none of them exported.
//
//  THE NAME CHECKS COST MORE CALLS THAN COMPARES. P2PmsgObject_pNameChk runs
//  for every sibling a name lookup steps over, and it spent ~20 out-of-line
//  calls doing a few compares each: four IsPhysSpan re-reading the heap handle
//  for the same image bounds, three Hdr_u_SizeNN, and the predicates inside
//  ChkContained and ChkWellFormed, across three translation units with no LTO.
//  Now the image extent (new P2PmsgHeap_GetExtent, which IsPhysSpan is rebuilt
//  on) and the block size are read once per sibling, and every predicate is
//  tested inline (P2PmsgHeapExtent_IsSpan, VBLock_IsContainedIn,
//  VBLockName_IsWellFormedInl and two twins). A failing test calls the original
//  throwing check, which refuses with its own message: no check is dropped or
//  loosened, the order and the image-only scope are unchanged, and the suite's
//  refusal cases (name outside its block for all four block kinds, no
//  terminator, capacity past the block, chain to 0) pass unchanged. Measured at
//  512 siblings on a received heap: a lookup 15.4 -> 12.1 us on Windows (-21%),
//  80.1 -> 70.7 us on Linux (-12%); a miss and FieldRef reads the same
//  (MSCS performance log, finding F-P1-4).
//
//  BELOW IS THE 3.4.3 RATIONALE.
//
//  3.4.3.0, a PATCH on 3.4.2, and strictly one: the only source change is
//  one condition in P3Pmsg_SelectObjectRecurse (P2Pmsg.cpp), so no export,
//  no Msgcore_c.h line and no class layout moved.
//
//  A MISSED NAME WALKED ITS SIBLINGS TWICE. When Goto(name) misses, the field
//  arm retries Goto with the whole rest of the path, so a stored name that
//  itself holds a delimiter ("Log.pys") is still found when "Log" is not. For
//  a plain name the rest of the path IS the name -- ParseObjectPath copies it
//  verbatim up to the first delimiter -- and Goto always walks from the first
//  sibling, so the retry was the same full walk again, certain to miss. Every
//  Exists() / SelectObject() miss paid it, and Targetcore asks Exists() on the
//  message path (GetScopeOrDestin). The retry now runs only when a delimiter
//  is left (*lpszParsedname != 0): same answers, one walk. Measured at 512
//  siblings on a received heap: a miss 62.5 -> 30.2 us on Windows, 304 -> 155
//  us on Linux (MSCS performance log, finding F-P1-3). New suite case "Exists()
//  finds a stored dotted name; a plain miss stays a miss" pins the retry --
//  checked by disabling it, which fails two of its checks.
//
//  BELOW IS THE 3.4.2 RATIONALE.
//
//  3.4.2.0, a PATCH on 3.4.1: tools\ci\exports-flat.manifest and
//  exports-cxx-x64.manifest match the built Debug|x64 DLL unchanged (285 flat,
//  771 mangled), Msgcore_c.h is untouched and no class layout moved.
//
//  It sizes a NAME by its length. Every free-standing P3PmsgName is born with
//  the full sizeof(VBLock::ud.oName) blob, and P2PmsgField/List/Vect_SizeofItem
//  and their InitItem twins laid the copy out at VBLockName_Sizenn -- the
//  CAPACITY of that blob, not nBlobUsed -- so every item in an image carried a
//  127-byte name slot, "a" as much as a 63-char name. One rule now does it,
//  P2PmsgName_SizeofLaidOut (used length, floored at Sizeof_Min; a chained stub
//  keeps the capacity rule), and SizeofItem, InitItem and the root item in
//  P2PmsgMgr::Attacheap all go through it -- the root had sized with one rule
//  and laid out with another, which the smaller names turned into an overrun.
//
//  Not a reader-side format change: a name is walked by the nBlobSize in its
//  own header, so a 3.4.1 image loads, reads and takes new items under 3.4.2
//  (checked against the old golden_ref.p2p), and a 3.4.2 image is the same
//  format with smaller blocks. What DOES change is the bytes a writer emits:
//  MscsUnitTests/golden_ref.p2p was regenerated. New suite case "a field's
//  name takes the room its name needs, not a fixed slot".
//
//  BELOW IS THE 3.4.1 RATIONALE.
//
//  3.4.1.0, a PATCH on 3.4.0, and strictly one: tools\ci\exports-flat.manifest,
//  both exports-cxx-*.manifest files and Msgcore_c.h are byte-identical to v3.4.0,
//  no class layout moved, and a Windows build compiles to the same code.
//
//  It carries ANDROID. P2PeerLan's Android app (P2PeerLanAndroid, phase A0) runs
//  the cores natively through the NDK, and Android's seccomp policy blocks
//  io_uring for apps, so Platform/ gains a second Linux backend for the p2piocp
//  hooks: p2piocp_epoll.cpp, the same completion model over epoll -- ordered
//  per-fd queues, every completion delivered, all-or-error writes, synchronous
//  cancel before close. Targetcore's P2P_IO_BACKEND chooses it (the default on
//  ANDROID); io_uring stays the default elsewhere. Pinned by
//  platform_iocp_epoll_test (125/125 on the box, ASan and TSan clean) and by the
//  full ctest built on epoll (100/101; the one a timing flake that passes alone).
//
//  And four fixes bionic and clang found, none observable on Windows. MSVC size
//  prefixes in a format (%I64, %I32, %I) are rewritten to ll / none / z before
//  formatting, narrow and wide: glibc read I as a flag and survived, bionic did
//  not know it, so every later argument shifted and P3PmsgField::Print's
//  "%s(%Ii)" faulted. p2p_pipe_path falls back $XDG_RUNTIME_DIR -> $TMPDIR ->
//  /tmp, since Android has no /tmp. CList gains const GetNext/GetPrev, and
//  sys/timeb.h is included only where it exists.
//
//  BELOW IS THE 3.4.0 RATIONALE.
//
//  3.4.0.0, a MINOR on 3.3.0, and this time compelled: the flat surface grew.
//  tools\ci\exports-flat.manifest gains two names, msgcore_field_get_inode and
//  msgcore_mgr_inode2field, and nothing in it was removed or changed signature.
//  exports-cxx-*.manifest is unchanged and no exported class layout moved; the
//  heap handle that gained two fields is private to MsgVBHeap.cpp.
//
//  It carries three data-loss fixes, found by asking the library to do what a
//  file system does (the AST use case's fsops suite):
//
//  Move onto an existing name destroyed the source. P3PmsgRefactor_Move copied
//  only when the destination lacked the name and deleted from the source
//  either way, so msgcore_field_move_child onto a taken name kept the
//  destination and lost the source -- and with several same-named items in the
//  source it moved one and deleted the rest. It is now refused, before either
//  half, and exactly one item moves otherwise.
//
//  Move into the item's own subtree overflowed the stack (0xC00000FD): the copy
//  recursed into itself. It is now refused the same way. TreeFs had refused
//  both itself; MsgFacade's Move reached them unguarded. Refused moves throw in
//  C++ and make msgcore_field_move_child return 0 with nothing changed.
//
//  A p2pos outlives its node. The allocator recycles blocks, so a p2pos held
//  across a delete resolves, silently, to whatever took the block -- which
//  Msgcore_c.h documented as a contract rather than fixed. The two new calls
//  are the fix: an inode is the p2pos tagged with the block's free count and a
//  per-heap stamp, and inode2field refuses one whose tags no longer match. It
//  is runtime state, never saved, so the image format did not change; an inode
//  is valid for one open of a store, as C++23's epochs are. The p2pos calls
//  are unchanged. Pinned by MsgcoreCApiSuite Test_CApi_Inode, and the two moves
//  by Test_CApi_MoveChild.
//
//  Guard a portable call with MSGCORE_VERSION_AT_LEAST(3,4,0).
//
//  BELOW IS THE 3.3.0 RATIONALE.
//
//  3.3.0.0, a MINOR on 3.2.1, taken by choice rather than compelled, for the
//  same reason as 3.2.0 below. tools\ci\exports-flat.manifest, both
//  exports-cxx-*.manifest files and Msgcore_c.h are byte-identical to v3.2.1,
//  and no class layout moved: the only source change is MsgFieldRef.hpp.
//
//  What grew is attribute syntax in the field-access layer, in both its forms.
//  Static: MSG_FIELD_ATTRS ( id, type, AttrView ), MSG_NODE_ATTRS ( id, View,
//  AttrView ) and MSG_FIELD_NODE_ATTRS ( id, type, View, AttrView ) add
//  Attrs(), a typed view of the member's '@' side, so
//  s->Temperature.Attrs()->Unit = L"Celsius" is checked at compile time like
//  any MSG_FIELD. Dynamic: MsgLiterals::operator""_attr, a C++11 user-defined
//  literal, makes a name a MsgAttrName, and [] given one steps to '@' --
//  Field ( mgr, L"Temperature" )[L"Unit"_attr]. It is the same step as the
//  existing Attr(), whose meaning did not change. The literal's 63-unit cap is
//  checked on the write, since C++17 cannot static_assert on its length.
//
//  Code that needs it should test `#ifdef MSG_FIELD_ATTRS`, not
//  MSGCORE_VERSION_AT_LEAST(3,3,0), as with MSG_FIELD_NODE in 3.2.0.
//
//  BELOW IS THE 3.2.1 RATIONALE.
//
//  3.2.1.0, a PATCH on 3.2.0, and strictly one: tools\ci\exports-flat.manifest,
//  both exports-cxx-*.manifest files and Msgcore_c.h are byte-identical to
//  v3.2.0, and no class layout moved.
//
//  It carries one fix to heap accounting on the receive path. A heap rebuilt
//  from a wire image counted the image's free bytes as used, so the first
//  allocation into a received message -- a reply or relay that adds to it --
//  overran the guard in P2PmsgHeap_pIOmage. In Debug that is an ASSERT box,
//  and it is what hung TargetFacade's FacadeSmokeTest on a declined broadcast;
//  in Release the guard is compiled out and the only effect was a high
//  nSizeofUsed against the nSizeofMax early check. Pinned by MsgcoreSuite
//  Test_ReceivedImageReserialises.
//
//  BELOW IS THE 3.2.0 RATIONALE.
//
//  3.2.0.0, a MINOR on 3.1.3, taken by choice rather than compelled. The covered
//  surface did not move: tools\ci\exports-flat.manifest, both
//  exports-cxx-*.manifest files and Msgcore_c.h are byte-identical to v3.1.3,
//  so a PATCH would have been allowed. What grew is the C++ field-access layer
//  in MsgFieldRef.hpp, which 3.1.2's rationale below already says "informs a
//  MINOR without compelling one": typed views now nest to any depth.
//  MSG_NODE ( id, View ) declares a child that is a branch and
//  MSG_FIELD_NODE ( id, type, View ) one that also holds a value, so
//  obj->window->font->size = 11 reads and writes through one chain of ->, with
//  a type of its own at every level. It is header-only -- no export, no class
//  layout -- and nothing existing changed meaning; the one edit to old code
//  lets a node be assigned from another node of the same type.
//
//  Taken because the number is what a C++ consumer reads first, and "3.1.3"
//  would say nothing changed. It also gives the layer as a whole the MINOR that
//  3.1.2 shipped it without. Code that needs nested views should test
//  `#ifdef MSG_FIELD_NODE` rather than MSGCORE_VERSION_AT_LEAST(3,2,0): the
//  macro names the feature, and the header can be copied without the DLL.
//
//  BELOW IS THE 3.1.3 RATIONALE.
//
//  3.1.3.0, a PATCH on 3.1.2, and strictly one: tools\ci\exports-flat.manifest,
//  exports-cxx-x64.manifest and Msgcore_c.h are byte-identical to v3.1.2.
//
//  What it carries is three receive-path memory-safety fixes, all reachable by
//  a peer sending a malformed frame. F12: VBLock_pData handed its owner down
//  the Field and Item branches and not List and Vect, so the D64 bound was
//  present and vacuous there -- a heap read past the image, found by the
//  scheduled recv_image fuzz run. F13: no name-chain walk was bounded at all;
//  a name chained to address 0 faulted P3PmsgName::c_size, and names were read
//  as strings with no terminator check. P2PmsgObject_pNameChk now bounds every
//  hop. F14: P2PmsgObject_CopyHeapVBLock memcpy'd a block whose address and
//  length both came from the image. VBLockList_pData / VBLockVect_pData gained
//  a defaulted owner parameter; neither is exported, so no symbol moved.
//
//  BELOW IS THE 3.1.2 RATIONALE.
//
//  3.1.2.0, a PATCH on 3.1.1, and a PATCH in the strict sense again: the
//  covered surface did not move. tools\ci\exports-flat.manifest and
//  Msgcore_c.h are byte-identical to v3.1.1.
//
//  What the release carries is two defect fixes and a header. The fixes: a
//  P3PmsgField constructor that threw after RenderThisSafe (DeclareItem with
//  a name over 63 UTF-16 units) left its render aliased onto its own member,
//  and the unwind freed it twice -- heap corruption; and ParseObjectPath
//  overran its stack buffer on a path component of 64 or more units (found
//  under ASan). The header is MsgFieldRef.hpp, field access by name --
//  Field(item, L"name") = v, MSG_FIELD typed views, Child() and Attrs()
//  anchors, short and MsgTime -- HEADER-ONLY, so it adds no export and moves
//  no class layout. It is C++ surface, which the policy does not cover, so it
//  informs a MINOR without compelling one; a PATCH says what changed for a
//  consumer of the covered surface, which is nothing. Platform/p2ptypes.h
//  gains ERROR_CONNECTION_REFUSED for the Linux build.
//
//  BELOW IS THE 3.1.1 RATIONALE.
//
//  3.1.1.0, a PATCH on 3.1.0, and a PATCH in the strict sense: the covered
//  surface did not move at all. tools\ci\exports-flat.manifest is unchanged
//  and the only edit to Msgcore_c.h since v3.1.0 is two comment lines
//  respelling TargetCore as Targetcore. What the release carries is four
//  defect fixes behind that surface - the BLOB16 clamp tested nSizeof and
//  assigned nBlobSize, so a var-typed descriptor could claim up to seven
//  bytes more room than was allocated; the C++ heap declarations never said a
//  pointer dies at the next mutation; every event class but DEBUG and TRACE
//  printed as [ERROR] on a console; and a manual-reset wait published no
//  happens-before edge, so teardown rode on a Sleep. The first two ship with
//  a gate rather than a comment.
//
//  It is NOT numbered in step with Targetcore, which releases 3.2.0 alongside
//  this. That is the independence below working as intended rather than an
//  oversight: Targetcore earned a MINOR from a break in its own session
//  cypher, nothing in this component changed to match, and a shared number
//  would have had to invent one.
//
//  BELOW IS THE 3.1.0 RATIONALE, kept because it is where the 3.x number and
//  the MINOR it carried are explained.
//
//  3.1.0.0. The 3.x number was set by the project rather than derived from
//  this tree's own release history: the development identity that preceded it
//  here was 1.0.0, and no binary carrying it was published. It is deliberately
//  INDEPENDENT of Targetcore's, which happened to share 3.0.0 and is under no
//  obligation to keep doing so - Targetcore links Msgcore but does not ship
//  as it, and a shared number would force a lockstep release neither wants.
//
//  MINOR, because the SUPPORTED surface GREW and nothing on it moved. The flat
//  C ABI of Msgcore_c.h gains exactly one function against v3.0.0 -
//  msgcore_field_is_sole - and no declaration, signature, type or contract
//  already there is touched. That is the textbook MINOR shape in both
//  directions: a consumer built against 3.0.0 links against a 3.1.0 DLL
//  unchanged, and a consumer built against 3.1.0 that calls the new export
//  cannot link against a 3.0.0 DLL. PATCH would be a lie in the second half.
//
//  And the bump is load-bearing rather than ceremonial, because
//  MSGCORE_VERSION_AT_LEAST is the ONLY way a portable consumer can guard that
//  call. Left at 3.0.0 the macro would answer the same for a surface that has
//  the export and one that does not, which is the one job it has.
//
//  The mangled C++ export half is still broken against v3.0.0, unchanged by
//  this: _N() is gone from Msgcore.h, and P3PmsgData's c_bool / c_char / c_int
//  / ... accessors traded a T& return for a by-value return taking a setter
//  argument. SECURITY.md's "Supported versions" section calls that half
//  toolchain-pinned and internal, so it sits outside what this number
//  promises - but a C++ consumer pinned to 3.0.0 has to recompile, and that is
//  written down here rather than left to be discovered at link time.
#define MSGCORE_VERSION_MAJOR  4
#define MSGCORE_VERSION_MINOR  0
#define MSGCORE_VERSION_PATCH  0
#define MSGCORE_VERSION_BUILD  0

//  Comma form, for the FILEVERSION / PRODUCTVERSION resource statements,
//  which take four comma-separated words and cannot take a macro expression.
#define MSGCORE_VERSION_COMMAS 4,0,0,0

//  String form. Kept spelled out rather than stringised from the parts above:
//  rc.exe's preprocessor has no reliable ## / # operator support, and a
//  VERSIONINFO string that silently expands to "MSGCORE_VERSION_MAJOR.0.0"
//  would ship without anyone noticing.
#define MSGCORE_VERSION_STRING "4.0.0.0"

//  Packed form, for a consumer that wants to compare rather than display.
//  0x03070000 is 3.7.0.0; the byte order is MAJOR, MINOR, PATCH, BUILD.
#define MSGCORE_VERSION_HEX    0x04000000

//  Fixed identity strings shared by both resources.
#define MSGCORE_COMPANY_NAME   "Ivyware Pty Ltd, Khrustal & Mann"
#define MSGCORE_PRODUCT_NAME   "Msgcore"
#define MSGCORE_COPYRIGHT      "Copyright \251 2000-2026 Ivyware Pty Ltd, Khrustal & Mann. " \
                               "Licensed under the Apache License, Version 2.0."

#ifndef RC_INVOKED

//  Compile-time guard for a consumer that needs a minimum version. Not
//  available to rc.exe, which cannot evaluate a function-like macro.
//
//    #if !MSGCORE_VERSION_AT_LEAST(3,0,0)
//    #  error Msgcore 3.0.0 or later is required
//    #endif
//
#define MSGCORE_VERSION_AT_LEAST(maj,min,pat) \
    ( ( (maj) << 24 | (min) << 16 | (pat) << 8 ) <= MSGCORE_VERSION_HEX )

#endif  // RC_INVOKED
