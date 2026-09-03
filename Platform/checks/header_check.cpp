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
//  Phase-0 header self-consistency check.
//
//  Includes every platform-layer header (umbrella + the MFC shim) and touches the
//  tagged-HANDLE model so the types are ODR-used. On _WIN32 this proves the shim
//  headers are pass-through-clean (they resolve to the real windows/winsock/atl headers
//  without conflict); on Linux (later phases) it proves the subset headers parse.
//
//  See the Linux port plan §9 Phase 0 exit criterion.
//
#include "../platform.h"
#include "../mfcshim.h"

int main()
{
    HANDLE h = INVALID_HANDLE_VALUE;
    SOCKET s = INVALID_SOCKET;
    (void)h;
    (void)s;
    DWORD  d = 0;
    (void)d;
    return 0;
}
