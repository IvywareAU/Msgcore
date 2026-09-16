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
//  Platform layer — umbrella header.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §4).
//
//  This is the single include the legacy stdafx.h files will route through (a Phase-1
//  build-gated step, NOT done yet). Including it on _WIN32 is equivalent to including the
//  real windows/winsock/mfc headers, so wiring it in leaves the Windows build unchanged.
//
//  Include order matters on Windows: WinSock2 before windows.h. p2ptypes.h enforces it.
//
#pragma once

#include "p2pexport.h"
#include "p2ptypes.h"   // Win32 scalars + HANDLE/SOCKET model (or real headers on Win32)
#include "p2pstr.h"     // CString subset / TCHAR
#include "p2pthread.h"  // threads, critical sections, events
#include "p2pfile.h"    // sync + async file I/O
#include "p2piocp.h"    // IOCP over io_uring  (the core)
#include "p2psock.h"    // socket shims
#include "p2pserial.h"  // serial shims
#include "p2pcrypto.h"  // crypto backend selector

//  mfcshim.h is intentionally NOT pulled here: on Windows real MFC comes via stdafx.h,
//  and on Linux the MFC façades are included only by the TUs that need them, to keep the
//  "subset only" surface tight (§4.3).
