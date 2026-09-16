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
// Linux port: the Win32 socket/IOCP/file surface is routed through the platform
// shim layer. On _WIN32 platform.h is pure pass-through (WinSock2/ws2tcpip/mswsock/
// windows + atlstr), so the Windows build is unchanged.
//
// Platform/ is PART OF THIS REPOSITORY, at Platform/. It was a sibling repository
// between 2026-08-20 and 2026-09-03, and a vendored copy of one before that. What
// changed is not the layout but the ownership: the upstream repository is retired,
// so this tree is the only Platform there is.
//
// That is what makes a copy safe this time, and it is worth being precise about why
// the last one was not. Vendoring failed in 6962947 for ONE reason -- there were TWO
// physical copies, this one and the parent tree's, and a quoted include always
// resolves relative to the citing file. A single translation unit reached two
// p2ptypes.h, which #pragma once cannot deduplicate, and the Linux subdirectory build
// died in ~40 redefinition errors until MSGCORE_PLATFORM_FROM_PARENT was added to
// choose between them. Windows never saw it, because the win-compat forwarders that
// pull in the second copy are generated only on Linux. There is no second copy to
// choose between now -- MSCS/Platform/ is deleted and the parent tree adds
// Msgcore/Platform as its p2pplatform -- so that macro is not coming back.
//
// A quoted include resolves relative to THIS file first, so "Platform/..." needs no
// -I and no project setting: Msgcore(2026).vcxproj carries no
// AdditionalIncludeDirectories at all and needs none.
//
// The other two consumers reach this same single copy from where they sit, as
// "../Msgcore/Platform/..." -- Targetcore/stdafx.h and MscsUnitTests/stdafx.h. And a
// clone of THIS repository alone compiles again, which is the property the sibling
// layout had to buy back with a two-repository CI checkout and a cross-repository PAT.
#include "Platform/platform.h"

#ifdef _WIN32
#include <atlstr.h>
#include <AfxMt.h>                     // Multi-tasking
#include <AfxTempl.h>                  // Templates
#else
#include "Platform/mfcshim.h"          // CObject/CList/CMap/CString/ASSERT on Linux
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
