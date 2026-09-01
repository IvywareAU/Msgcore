// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#ifdef _WIN32
#include "targetver.h"

// TODO: Added by LJM to minimise compiler warning
#pragma warning(disable:4251)
#pragma warning(disable:26496)         // const expressions

#include <afx.h>
#include <afxwin.h>
#include <comutil.h>
#include <afxole.h>         // MFC OLE classes
#endif // _WIN32

// Linux port: the Win32 socket/IOCP/file surface is routed through the platform
// shim layer. On _WIN32 platform.h is pure pass-through (WinSock2/ws2tcpip/mswsock/
// windows + atlstr), so the Windows build is unchanged.
//
// Platform/ is a SIBLING repository, not a subdirectory of this one -- the same
// arrangement TargetCore uses (TargetCore/stdafx.h:32). The expected layout is
//
//     <parent>/
//     |-- Platform/     the Win32->POSIX shim layer
//     `-- Msgcore/      this repository
//
// A quoted include resolves relative to THIS file first, so "../Platform/..."
// reaches the sibling with no -I and no project setting: Msgcore(2022).vcxproj
// carries no AdditionalIncludeDirectories at all and needs none.
//
// This replaced a copy of Platform/ vendored INSIDE this repository. That copy
// bought one thing -- a lone clone compiled -- and cost a permanent hazard: two
// physical p2ptypes.h on one include path, which #pragma once cannot deduplicate,
// so the subdirectory build died in ~40 redefinition errors until
// MSGCORE_PLATFORM_FROM_PARENT was added to choose between them. One copy needs
// no such macro and cannot drift. What replaced the standalone-clone property is
// a two-repository checkout in CI -- see Readme.md, "The sibling dependency".
#include "../Platform/platform.h"

#ifdef _WIN32
#include <atlstr.h>
#include <AfxMt.h>                     // Multi-tasking
#include <AfxTempl.h>                  // Templates
#else
#include "../Platform/mfcshim.h"       // CObject/CList/CMap/CString/ASSERT on Linux
#endif // _WIN32

// TODO: reference additional headers your program requires here
#define _CRT_RAND_S
#include <memory>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <map>
#include <list>
#include <vector>
#ifdef _WIN32
#include <xstring>                     // Added for VS2019 - preceedes <string> (MSVC only)
#endif
#include <string>
#include <iostream>

/// make_unique
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
////

//
//  P2Peer Visual Studio 2002, 2005, 2008 langauge extensions to faciltate
//  backwards compatiblity for VS2010+
//  NOTES: VS2002, 2005, 2008 release requirement, may be dropped from
//         subsequent code base and VS2010+ implementations
#ifdef _WIN32
#if _MFC_VER <= 0x0999
#define   nullptr NULL
#endif
#endif // _WIN32
