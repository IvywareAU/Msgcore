//  Windows platform floor.
//  NOTES: Windows 8.1 (_WIN32_WINNT_WINBLUE), matched to TargetCore/targetver.h.
//         Msgcore itself calls no API newer than Vista; the floor is TargetCore's,
//         set by its CNG ECDH path (BCRYPT_KDF_RAW_SECRET, gated on NTDDI_WINBLUE).
//         TargetCore links Msgcore.dll, so a higher floor here would undo that.
//       : Compile-time gate only; it does not change the PE minimum OS version.
//
#pragma once

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#include <SDKDDKVer.h>
