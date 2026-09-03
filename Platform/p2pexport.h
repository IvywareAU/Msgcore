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
//  Platform layer — symbol visibility / DLL export control.
//
//  Part of the Msgcore + TargetCore Linux port (see the Linux port plan §4, §6.1/6.2).
//  Unifies the three legacy export-macro idioms behind one portable spelling.
//
//  On _WIN32  : P2P_EXPORT / P2P_IMPORT expand to __declspec, exactly as today.
//  On Linux   : they expand to __attribute__((visibility("default"))) so the
//               shared objects are built with -fvisibility=hidden + explicit exports
//               (the Linux port plan §6.2, "dllmain.cpp, exports").
//
//  This header defines NOTHING that changes the Windows ABI: the legacy macros
//  (Msgcore_EXT/_API, TargetCore_EXT, MSGCORE_C_API) continue
//  to be defined in their original headers. P2P_EXPORT is offered as the forward
//  spelling for NEW platform-layer symbols only.
//
#pragma once

#if defined(_WIN32)
  #define P2P_EXPORT __declspec(dllexport)
  #define P2P_IMPORT __declspec(dllimport)
  #define P2P_LOCAL
#else
  #define P2P_EXPORT __attribute__((visibility("default")))
  #define P2P_IMPORT __attribute__((visibility("default")))
  #define P2P_LOCAL  __attribute__((visibility("hidden")))
#endif
