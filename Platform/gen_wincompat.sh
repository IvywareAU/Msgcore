#!/usr/bin/env bash
# Copyright © 2026 Khrustal & Mann
#              MELBOURNE, VICTORIA, AUSTRALIA, 3000
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# Generate Platform/win-compat/ — Linux-only compatibility headers.
# Each is named exactly like the Windows/MFC/MSVC header the legacy code includes,
# and forwards to the real platform shim. Placed on the Linux include path ONLY, so
# scattered `#include <afxwin.h>` / `<windows.h>` etc. resolve with no legacy edits.
# On Windows this directory is NOT on the include path (real SDK/MFC is used).
set -eu
cd "$(dirname "$0")"
mkdir -p win-compat
cd win-compat

fwd() { printf '#pragma once\n#include "%s"\n' "$1" > "$2"; }
empty() { printf '#pragma once\n' > "$1"; }

# --- MFC -> mfcshim.h (CObject/CList/CMap/CString/ASSERT) ---
for h in afx.h afxwin.h afxext.h afxole.h afxdisp.h afxodlgs.h afxcmn.h afxdlgs.h \
         afxmt.h AfxMt.h afxtempl.h AfxTempl.h afxcoll.h afxstr.h; do
  fwd "../mfcshim.h" "$h"
done

# --- ATL string -> p2pstr.h (CString) ---
fwd "../p2pstr.h" atlstr.h
fwd "../p2pstr.h" AtlStr.h
fwd "../p2pstr.h" cstringt.h
fwd "../p2pstr.h" tchar.h
fwd "../p2pstr.h" Tchar.h
fwd "../p2pstr.h" TChar.h

# --- Win32 core / winsock -> the FULL platform umbrella. On Windows <windows.h> and
#     <WinSock2.h> transitively provide the whole Win32 surface (scalars, CRITICAL_SECTION,
#     CreateFile/FILE_SHARE_*, sockets, ...). A consumer TU that includes only <windows.h>
#     (e.g. a test stdafx.h that does not pull ../Platform/platform.h itself) must get that
#     same surface, so forward to platform.h (types + thread + file + iocp + sock + ...),
#     not just p2ptypes.h. Idempotent (#pragma once); safe for the low-level winnt/windef too.
for h in windows.h Windows.h WinSock2.h winsock2.h Winsock2.h ws2tcpip.h Ws2tcpip.h \
         mswsock.h MSWSock.h Mswsock.h winbase.h winnt.h minwindef.h windef.h \
         winerror.h winuser.h wingdi.h; do
  fwd "../platform.h" "$h"
done

# --- COM / OLE / RPC / SDK type headers -> p2ptypes (concrete COM types shimmed on demand) ---
for h in wtypes.h wtypesbase.h WTypes.h oaidl.h oleauto.h OleAuto.h unknwn.h Unknwn.h \
         objidl.h ObjIdl.h rpc.h rpcndr.h RpcNdr.h guiddef.h GuidDef.h propidl.h \
         propsys.h PropSys.h Propvarutil.h propvarutil.h propkey.h combaseapi.h; do
  fwd "../p2ptypes.h" "$h"
done

# --- SDK version headers -> empty (macros only) ---
for h in SDKDDKVer.h sdkddkver.h WinSDKVer.h winsdkver.h TargetVer.h targetver.h \
         Targetver.h; do
  empty "$h"
done

# --- Windows Service Control Manager -> p2psvc.h (SCM types/consts + Linux no-op stubs) ---
for h in winsvc.h WinSvc.h Winsvc.h; do
  fwd "../p2psvc.h" "$h"
done

# --- COM utilities / shell / process APIs -> types (declare specifics on demand) ---
for h in comutil.h comdef.h psapi.h Psapi.h ShlObj_core.h shlobj_core.h shlobj.h \
         ShlObj.h objbase.h ole2.h; do
  fwd "../p2ptypes.h" "$h"
done

# --- assert case variants -> <cassert> ---
for h in Assert.h ASSERT.h; do printf '#pragma once\n#include <cassert>\n' > "$h"; done

# --- MSVC internal <xstring> -> <string> ---
printf '#pragma once\n#include <string>\n' > xstring

# --- MSVC CRT low-level I/O -> POSIX (temporary; folds into p2pfile in Phase 1) ---
cat > io.h <<'XEOF'
#pragma once
// MSVC <io.h> low-level file ops mapped to POSIX. Minimal; extend as usage surfaces.
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>
#define _open   ::open
#define _close  ::close
#define _read   ::read
#define _write  ::write
#define _lseek  ::lseek
#define _unlink ::unlink
#define _access ::access
#define _fileno ::fileno
XEOF
fwd "io.h" corecrt_io.h
fwd "io.h" corecrt_io.h

echo "win-compat headers: $(ls | wc -l)"
