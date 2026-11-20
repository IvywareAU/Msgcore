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
#define MSGCORE_VERSION_MAJOR  3
#define MSGCORE_VERSION_MINOR  4
#define MSGCORE_VERSION_PATCH  7
#define MSGCORE_VERSION_BUILD  0

//  Comma form, for the FILEVERSION / PRODUCTVERSION resource statements,
//  which take four comma-separated words and cannot take a macro expression.
#define MSGCORE_VERSION_COMMAS 3,4,7,0

//  String form. Kept spelled out rather than stringised from the parts above:
//  rc.exe's preprocessor has no reliable ## / # operator support, and a
//  VERSIONINFO string that silently expands to "MSGCORE_VERSION_MAJOR.0.0"
//  would ship without anyone noticing.
#define MSGCORE_VERSION_STRING "3.4.7.0"

//  Packed form, for a consumer that wants to compare rather than display.
//  0x03040700 is 3.4.7.0; the byte order is MAJOR, MINOR, PATCH, BUILD.
#define MSGCORE_VERSION_HEX    0x03040700

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
