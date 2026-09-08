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
//       : Keep the release tag and this file in step: version 3.1.0 is tag
//         v3.1.0. A build whose DLL reports a version no tag matches cannot
//         be traced back to a source state, which defeats the point.
//
#pragma once

//  Component version. MAJOR.MINOR.PATCH is the released identity; BUILD is
//  reserved for a CI build counter and is 0 for a hand-built binary.
//
//  3.1.0.0. The 3.x number was set by the project rather than derived from
//  this tree's own release history: the development identity that preceded it
//  here was 1.0.0, and no binary carrying it was published. It is deliberately
//  INDEPENDENT of TargetCore's, which happened to share 3.0.0 and is under no
//  obligation to keep doing so - TargetCore links Msgcore but does not ship
//  as it, and a shared number would force a lockstep release neither wants.
//
//  MINOR rather than MAJOR, deliberately. Since v3.0.0 the SUPPORTED surface
//  - the flat C ABI of Msgcore_c.h - has not moved at all: that header has no
//  diff against the tag and tools\ci\exports-flat.manifest is byte-identical.
//  The mangled C++ export half DID break: _N() is gone from Msgcore.h, and
//  P3PmsgData's c_bool / c_char / c_int / ... accessors traded a T& return for
//  a by-value return taking a setter argument, which removed twelve exported
//  symbols rather than adding to them. SECURITY.md's "Supported versions"
//  section calls that half toolchain-pinned and internal, so it sits outside
//  what this number promises - but a C++ consumer pinned to 3.0.0 has to
//  recompile, and that is written down here rather than left to be discovered
//  at link time.
#define MSGCORE_VERSION_MAJOR  3
#define MSGCORE_VERSION_MINOR  1
#define MSGCORE_VERSION_PATCH  0
#define MSGCORE_VERSION_BUILD  0

//  Comma form, for the FILEVERSION / PRODUCTVERSION resource statements,
//  which take four comma-separated words and cannot take a macro expression.
#define MSGCORE_VERSION_COMMAS 3,1,0,0

//  String form. Kept spelled out rather than stringised from the parts above:
//  rc.exe's preprocessor has no reliable ## / # operator support, and a
//  VERSIONINFO string that silently expands to "MSGCORE_VERSION_MAJOR.0.0"
//  would ship without anyone noticing.
#define MSGCORE_VERSION_STRING "3.1.0.0"

//  Packed form, for a consumer that wants to compare rather than display.
//  0x03010000 is 3.1.0.0; the byte order is MAJOR, MINOR, PATCH, BUILD.
#define MSGCORE_VERSION_HEX    0x03010000

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
