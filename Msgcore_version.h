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
//       : Keep the release tag and this file in step: version 3.2.1 is tag
//         v3.2.1. A build whose DLL reports a version no tag matches cannot
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
#define MSGCORE_VERSION_MINOR  2
#define MSGCORE_VERSION_PATCH  1
#define MSGCORE_VERSION_BUILD  0

//  Comma form, for the FILEVERSION / PRODUCTVERSION resource statements,
//  which take four comma-separated words and cannot take a macro expression.
#define MSGCORE_VERSION_COMMAS 3,2,1,0

//  String form. Kept spelled out rather than stringised from the parts above:
//  rc.exe's preprocessor has no reliable ## / # operator support, and a
//  VERSIONINFO string that silently expands to "MSGCORE_VERSION_MAJOR.0.0"
//  would ship without anyone noticing.
#define MSGCORE_VERSION_STRING "3.2.1.0"

//  Packed form, for a consumer that wants to compare rather than display.
//  0x03020100 is 3.2.1.0; the byte order is MAJOR, MINOR, PATCH, BUILD.
#define MSGCORE_VERSION_HEX    0x03020100

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
