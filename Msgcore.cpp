// Copyright © 2006-2010, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  Defines the entry point for the Msgcore.DLL
//

#include "stdafx.h"
#include "Msgcore.h"
#include <stdlib.h>
//using namespace gsl;
//using namespace std;
/*BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}*/

// This is an example of an exported variable
Msgcore_API int nMsgcore=0;

// This is an example of an exported function.
Msgcore_API int fnMsgcore(void)
{
	return 42;
}

// This is the constructor of a class that has been exported.
// see Msgcore.h for the class definition
CMsgcore::CMsgcore()
{ 
	return; 
}

//
//  Case insensitive wide character comparision
//  NOTES: Emulates _memicmp() equivalent
//
//  Parameters:  wchar_t *pwSrc
//
//               wchar_t *pwDst
//
//               size_t count
//
//  Returns:     Result
//
Msgcore_EXT int
wmemicmp ( const wchar_t *pwSrc, const wchar_t *pwDst, size_t count ) noexcept
{
    // Boundaries
    if ( pwSrc == pwDst || count == 0 )
      return 0;
    if ( pwSrc == nullptr && pwDst )
      return -1;
    if ( pwSrc && pwDst == 0 )
      return +1;

    // Comparsion
    int wSrcu = 0, wDstu = 0;
    while ( count-- > 0 )
    {
      wSrcu = toupper ( *pwSrc++ );
      wDstu = toupper ( *pwDst++ );
      if ( wSrcu != wDstu )
        return wSrcu - wDstu;
    }
    return 0;
}

//
//  Performs generic wildcard pattern matching
//
//
//  Parameters:  P2PmsgID strMsgWildcard
//               Message wildcard
//
//               P2PmsgID strMsgName
//               Message name to be matched against wildcard
//
//  Returns:     bool
//                 true... Message matches wildcard
//                 false.. No match
//
bool
MsgcoreWildcard(LPCWSTR pattern, LPCWSTR text) noexcept
{
    const WCHAR* starPattern = nullptr;
    const WCHAR* starText    = nullptr;
    while ( *text )
    {
        if ( *pattern == L'?' )
        {
            if ( *text == L'.' )
                goto starFallback;
            ++pattern;
            ++text;
        }
        else if (*pattern == L'*')
        {
            starPattern = ++pattern;
            starText    = text;

            if ( !*pattern )
                return true; // trailing '*' matches all
        }
        else if (*pattern == L'#')
        {
            if ( !iswdigit(*text) )
                goto starFallback;

            // consume all digits
            do { ++text; } while (iswdigit(*text));
            ++pattern;
        }
        else if ( *pattern == *text )
        {
            ++pattern;
            ++text;
        }
        else
        {
starFallback:
            if ( !starPattern )
                return false;

            pattern = starPattern;
            text    = ++starText;
        }
    }
    // Consume trailing '*'
    while ( *pattern == L'*' )
        ++pattern;
    return *pattern == 0;
}
bool
MsgcoreWildcard_preChatGPT ( LPCWSTR lpszWildcard, LPCWSTR lpszName ) noexcept
{
    // Locals
    BOOL bStar = FALSE;

    // Implmentation
TOP:LPCWSTR p, s;
    for ( s = lpszName, p = lpszWildcard; *s; ++s, ++p )
    {
      switch (*p)
      {
         case L'?':
            if ( *s == L'.' )
            {
              goto starCheck;
            }
            break;
         case L'*':
            bStar = TRUE;
            lpszName = s, lpszWildcard = p;
            if ( !*++lpszWildcard )
              return TRUE;
            goto TOP;
         case '#':
           if (!isdigit(*s))
             goto starCheck;
           while (isdigit(*(s + 1)))
             s++;
           break;
         default:
            if ( *s != *p )
              goto starCheck;
            break;
      }
   }
   if ( *p == L'*' ) ++p;
   return (!*p);

starCheck:
   if ( !bStar ) return FALSE;
   lpszName++;
   goto TOP;
}
