// Copyright © 2000-2011, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//
//  P2Pevent implementation
//

#include "stdafx.h"
#include "afxwin.h"
#include <sys/timeb.h>   // was <sys\timeb.h> — backslash is non-portable (MSVC accepts /)
#include <cstdlib>       // getenv, for the P2PMSG_NO_UI knob (AFTER stdafx.h: /Yu drops anything before it)
#include <cstdio>        // fopen/fwprintf, for the configured log file
#include <ctime>         // localtime_s / localtime_r, for the log timestamp
#include "Msgcore.h"
//#include "P2Pwin32.h"
#include "Msgexception.h"
#include "MsgList.h"
#include "MsgCurs.h"

///////////////////////////////////////////////////////////////////////
//  Constructors and destructor

static  TCHAR s_szServiceName[32]  = { 0 };
static  DWORD s_nEventNo           =   0;
//  THE DEFAULT NOTIFICATION MASK.  ERROR and WARNING.
//  NOTES: WARNING joined it on 2026-08-20, closing TargetCore's F-S6-4.
//         Until then this was P2Pevotn_ERROR alone, which meant every
//         EVWRN->Display() in these repositories was raised into nothing
//         on every build - the diagnostic ran, formatted its text and its
//         advice, and returned without emitting.  A warning nobody can see
//         does not warn anybody, and the three that exist are all operator
//         MISCONFIGURATIONS with a fix attached:
//           - a relay attestation that could not be signed;
//           - a peer signing logins to a hub that does not require them;
//           - P2PeerioBSTR, which carries cleartext, being constructed.
//       : The usual argument against warnings on by default is flooding.
//         It does not apply here and that is not luck: every one of those
//         three ends in ->Display()->SetLast(), which reports once per
//         connection rather than once per message, and each was written
//         that way deliberately.  A misconfigured hub costs one line.
//       : It is a behaviour change for every existing host, which is why it
//         is a decision of its own and not a rider on the fix that found it.
//         The consuming project's versioning policy carries it as an
//         unreleased break rather than a silent one.  An application
//         that does not want them says so in one call, the same way it
//         always could:  P2Pevent::Configure(P2PeventCfg_REMMASK,
//                                            P2Pevotn_WARNING);
static  DWORD s_dwP2Pevotn_MASK    = ( P2Pevotn_ERROR | P2Pevotn_WARNING );
UINT    WM_P2PeventNOTN = WM_APP + 0x01E0;

#pragma pack(push,1)
typedef struct                         // P2PeventNode details
{
    BYTE         eClass;               // Event class
    time_t       dwEpoch;              // Event epoch
    DWORD        nEventNo;             // Event number
    HRESULT      hr;
} P2PeventNode;
#pragma pack(pop)

//
//  Parameters:  const P2Pevent& rhs
//               Self reference
//
P2Pevent::P2Pevent ( )
        : P3PmsgItem ( L"P2Pevent", AllocBLOB08(sizeof(P2PeventNode) ) )
{
}
P2Pevent::P2Pevent ( const P2Pevent& rhs )
{
    *this = rhs;
}
P2Pevent::P2Pevent ( const P3PmsgItem& rhs )
        : P3PmsgItem ( rhs )
{
}
P2Pevent::~P2Pevent ( )
{
    // Last events maintenance
    if ( GetP2Pevent() == this )
      IsolateP2Pevent ( );
}

//
//  Cancels the contained P2Pevent
//  NOTES: Object self destructs.  P2Pevent's are intended to 
//         accumulate information over their life cycle.  As such
//         notifications are performed immediately before cancellation
//
//  Parameters:  bool bNotify = true
//               Notifications flag
//                 true... Perform notifications
//                 false.. Skip all notifications
//
//  Returns:     P2Pevent* = 0
//               Null pointer
P2Pevent*
P2Pevent::Cancel ( bool bNotify )
{
    // Confirm
    if (    bNotify    &&
         !m_bDisplayed    )
      Display();

    // Perform notifications
    P2PeventPost_HWND ( *this );
    // Simply
    delete this;
    return 0;
}

//
//  Registers for external P2Pevent notifications
//  NOTES: Registered notifications are subsequently performed for
//         each matching type.  
//
//  Parameters:  HWND hWnd
//               Handle for window to which notification is posted.
//
//               UINT uiWM_USER
//               Users assigned windows message identification
// 
//               DWORD dwP2PeventFilter
//               P2Pevent's filter mask to be applied
// 
//               DWORD_PTR dwUserKey
//               User defined message key.
//
static HWND      g_HWnd = 0;
static UINT      s_uiWM_USER = 0;
static DWORD     s_dwEventFilter = 0;
static DWORD_PTR s_dwUserKey = 0;
void
P2Pevent::Register4P2Pevents ( HWND hWnd, UINT uiWM_USER
                             , DWORD dwEventFilter, DWORD_PTR dwUserKey )
{
    g_HWnd = hWnd;
    s_uiWM_USER = uiWM_USER;
    s_dwEventFilter = dwEventFilter;
    s_dwUserKey = dwUserKey;
}
static P2PeventCBFnc  s_pfnP2PeventCB = nullptr;
static P2PeventSinkID s_nP2PeventSinkID = 0;
static DWORD          s_dwSinkP2PeventFilter = 0;
static DWORD_PTR      s_dwSinkUserKey = 0;
void
P2Pevent::Register4P2Pevents ( P2PeventSinkID nP2PeventSinkID, P2PeventCBFnc pfnP2PeventCB
                             , DWORD dwP2PeventFilter, DWORD_PTR dwSinkUserKey )
{
    s_pfnP2PeventCB        = pfnP2PeventCB;
    s_nP2PeventSinkID      = nP2PeventSinkID;
    s_dwSinkP2PeventFilter = dwP2PeventFilter;
    s_dwSinkUserKey        = dwSinkUserKey;
}

///////////////////////////////////////////////////////////////////////
//  Factories

//
//  Makes empty P2Pevent image suitable for chaining
//  NOTES: Problematic exercise on the edge of application stability
//
//  Parameters: P2Pevent_e eClass
//              Instance class
//
//  Returns:    P2Pevent*
//              Pointer to P2Pevent object suitable P2Pevent chaining
P2Pevent*
P2Pevent::MakeEvent( P2Pevent_e eClass )
{
    // Implementation
    P2Pevent *pP2Pevent = 0;
    try
    {
      pP2Pevent = new P2Pevent ( );
     ((P2PeventNode *)pP2Pevent->c_vBlob()) ->  eClass = static_cast<BYTE>(eClass);
     ((P2PeventNode *)pP2Pevent->c_vBlob()) -> dwEpoch = time(0);
      if ( s_szServiceName[0] )
        (*pP2Pevent).DESC += P3PmsgField ( TEvent__Svc, DataWSTR08(s_szServiceName) );

      // Origin of event details
      // NOTES: We may or may not be P2PmsgHub context
      //P2PmsgHubID nP2PmsgHubID = GetP2PmsgHubContext();
      //if ( nP2PmsgHubID > 0 )
      //{
      //  LPCTSTR lpszHubName  = GetP2PmsgHubName ( nP2PmsgHubID );
      //  (*pP2Pevent) += P3PmsgField ( TEvent__Hub, DataBSTR08(lpszHubName) );
      //  LPCTSTR lpszPumpName = GetP2PmsgPumpName( );
      //  (*pP2Pevent) += P3PmsgField ( TEvent__Pmp, DataBSTR08(lpszPumpName) );
      //}
    }
    catch ( ... )
    {
      delete pP2Pevent;
             pP2Pevent = 0;
      throw;                           // Propagate original exception
    }

    // Never a NULL pointer
    return pP2Pevent;
}
//
//  Initialise P2Pevent object
//  NOTES: Problematic exercise on the edge of application stability
//
//  Parameters: P2Pevent_e eClass
//              Instance class
//
//  Returns:    P2Pevent*
//              Pointer to P2Pevent object suitable P2Pevent chaining
P2Pevent*
P2Pevent::InitEvent( P2Pevent_e eClass )
{
    // Implementation
    ((P2PeventNode *)c_vBlob()) ->  eClass = static_cast<BYTE>(eClass);
    ((P2PeventNode *)c_vBlob()) -> dwEpoch = time(0);
    if ( s_szServiceName[0] )
      (*this).DESC += P3PmsgField ( TEvent__Svc, DataWSTR08(s_szServiceName) );

    // Origin of event details
    // NOTES: We may or may not be P2PmsgHub context
    //P2PmsgHubID nP2PmsgHubID = GetP2PmsgHubContext();
    //if ( nP2PmsgHubID > 0 )
    //{
    //  LPCTSTR lpszHubName  = GetP2PmsgHubName ( nP2PmsgHubID );
    //  (*this) += P3PmsgField ( TEvent__Hub, DataBSTR08(lpszHubName) );
    //  LPCTSTR lpszPumpName = GetP2PmsgPumpName( );
    //  (*this) += P3PmsgField ( TEvent__Pmp, DataBSTR08(lpszPumpName) );
    //}

    // Never a NULL pointer
    return this;
}

///////////////////////////////////////////////////////////////////////
//  Overloaded Operators

//
//  operator = () overload
//
//  Parameters:  const P2Pevent& rhs
//               Right hand side
//
//  Returns:     P2Pevent&
//               Self reference
//
P2Pevent&
P2Pevent::operator = ( const P2Pevent& rhs )
{
    // Optimise
    if ( this == &rhs )
      return *this;
    P3PmsgItem::operator = ( rhs );

    // Tidy up and
    return *this;
}

///////////////////////////////////////////////////////////////////////
//  Operations

//
//  Sets formatted modulename
//  NOTES: Displaces previous field contents
//
//  Parameters:  const char *pszModuleName
//               Module name to be formatted and set
//
//               ...
//               Variable argument list associated with above format
//               string.
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
#ifdef _UNICODE
P2Pevent*
P2Pevent::Module ( LPCWSTR lpszModuleFormat, ... )
{
    // Locals
    if ( lpszModuleFormat == nullptr )
      return this;
    va_list ap;
    TCHAR   szModule[512] = {0};
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszModuleFormat );
    if ( lpszModuleFormat )
      cSize += _vstprintf_s ( &szModule[cSize], ARRAYSIZE(szModule)-cSize, lpszModuleFormat, ap );
    szModule[cSize++] = 0;
    VERIFY(cSize<255);

    // Persist and
    Module_ ( &szModule[0] );

    // Tidy up, and
    va_end ( ap );
    return this;
}
#endif
P2Pevent*
P2Pevent::Module ( LPCSTR lpszModuleFormat, ... )
{
    // Locals
    if ( lpszModuleFormat == 0 )
      return this;
    va_list ap;
    char    szModule[512] = {0};
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszModuleFormat );
    if ( lpszModuleFormat )
      cSize += vsprintf_s ( &szModule[cSize], ARRAYSIZE(szModule)-cSize, lpszModuleFormat, ap );
    szModule[cSize++] = 0;
    va_end ( ap );

    // Persist and
#ifdef _UNICODE
    CString strModule = ATL::CA2WEX(&szModule[0]).m_szBuffer;
    Module_ ( strModule );
#else
    Module_ ( szModule );
#endif

    // Tidy up, and
    return this;
}
P2Pevent*
P2Pevent::Module_ ( LPCTSTR lpszModule )
{
#ifdef _UNICODE
    P3PmsgData oData = DataWSTR08(lpszModule);
#else
    P3PmsgData oData = DataBSTR08(lpszModule);
#endif

    // Persist and
    if ( P3PmsgItem::Exists(TEvent__Fnc) )
      (*this)[TEvent__Fnc] = oData;
    else
      (*this).DESC += P3PmsgItem ( TEvent__Fnc, oData );

    // Tidy up, and
    return this;
}

//
//  Allocates P2Pevent message
//  NOTES: Build a list of successive message strings
//
//  Parameters:  LPCTSTR lpszMessageFormat
//               Message format
//
//               ...
//               Variable argument list associated with above format
//               string.
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
#ifdef _UNICODE
P2Pevent*
P2Pevent::Message ( LPCWSTR lpszMessageFormat, ... )
{
    // Locals
    va_list ap;
    TCHAR   szModule[4096] = {0};
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszMessageFormat );
    if ( lpszMessageFormat )
      cSize += _vstprintf_s ( &szModule[cSize], ARRAYSIZE(szModule)-cSize, lpszMessageFormat, ap );
    szModule[cSize++] = 0;
    VERIFY(cSize<255);

    // Persist and
    if ( cSize > 1 )
      Message_ ( szModule );

    // Tidy up, and
    va_end ( ap );
    return this;
}
#endif
P2Pevent*
P2Pevent::Message ( LPCSTR lpszMessageFormat, ... )
{
    // Locals
    va_list ap;
    char    szMessage[2048] = {0};
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszMessageFormat );
    if ( lpszMessageFormat )
      cSize += vsprintf_s ( &szMessage[cSize], ARRAYSIZE(szMessage)-cSize, lpszMessageFormat, ap );
    szMessage[cSize++] = 0;
    va_end ( ap );

    // Persist and
    if ( cSize > 1 ) {
      CString strMessage = szMessage;
      Message_ ( strMessage );
    }

    // Tidy up, and
    return this;
}
P2Pevent*
P2Pevent::Message_ ( LPCTSTR lpszMessage )
{
    // Persist and
    if ( !P3PmsgItem::Exists(TEvent__Dsc) )
      (*this).DESC += P3PmsgList ( P3PmsgField(TEvent__Dsc) );
    P3PmsgList& oList = dynamic_cast<P3PmsgList&>((*this)[TEvent__Dsc]);
    if ( _tcslen(lpszMessage) < 127 )
      oList += DataWSTR08(lpszMessage);
    else
      oList += DataWSTR16(lpszMessage);

    // Tidy up, and
    m_csDescription.Empty ( );         // Activates reformat
    return this;
}

//
//  Allocates P2Pevent advice
//  NOTES: Build a list of successive advice strings
//
//  Parameters:  LPCTSTR lpszFormat
//               Advice format
//
//               ...
//               Variable argument list associated with above format
//               string.
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
#ifdef _UNICODE
P2Pevent*
P2Pevent::Advice ( LPCWSTR lpszAdviceFormat, ... )
{
    // Locals
    va_list ap;
    TCHAR   szAdvice[512] = {0};
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszAdviceFormat );
    if ( lpszAdviceFormat )
      cSize += _vstprintf_s ( &szAdvice[cSize], ARRAYSIZE(szAdvice)-cSize, lpszAdviceFormat, ap );
    szAdvice[cSize++] = 0;
    VERIFY(cSize<255);

    // Persist and
    Advice_ ( szAdvice );

    // Tidy up, and
    va_end ( ap );
    return this;
}
#endif
P2Pevent*
P2Pevent::Advice ( LPCSTR lpszAdviceFormat, ... )
{
    // Locals
    va_list ap;
    char    szAdvice[512] = {0};
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszAdviceFormat );
    if ( lpszAdviceFormat )
      cSize += vsprintf_s ( &szAdvice[cSize], ARRAYSIZE(szAdvice)-cSize, lpszAdviceFormat, ap );
    szAdvice[cSize++] = 0;
    va_end ( ap );

    // Persist and
#ifdef _UNICODE
    CString strAdviceFormat = ATL::CA2WEX(lpszAdviceFormat).m_szBuffer;
    Advice_ ( strAdviceFormat );
#else
    Advice_ ( lpszAdviceFormat );
#endif

    // Tidy up, and
    return this;
}
P2Pevent*
P2Pevent::Advice_ ( LPCTSTR lpszAdvice )
{
    // Persist and
    if ( !P3PmsgItem::Exists(TEvent__Adv) )
      (*this).DESC += P3PmsgList ( P3PmsgField(TEvent__Adv) );
    P3PmsgList& oList = dynamic_cast<P3PmsgList&>((*this)[TEvent__Adv]);
    if ( _tcslen(lpszAdvice) < 127 )
      oList += DataWSTR08(lpszAdvice);
    else
      oList += DataWSTR16(lpszAdvice);

    // Tidy up, and
    m_csDescription.Empty ( );         // Activates reformat
    return this;
}

//
//  Sets error group
//  NOTES: Usually a short description
//           WIN.. Windows
//           WSA.. Windows Socket Architecture
//           P2P.. Peer2Peer
//       : Displaces previous field contents
//
//
//  Parameters:  LPCTSTR lpszGroup
//
//               ...
//               Variable argument list associated with above
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
#ifdef _UNICODE
P2Pevent*
P2Pevent::Group ( LPCWSTR lpszGroupFormat, ... )
{
    // Locals
    va_list ap;
    TCHAR   szGroup[512];
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start(ap, lpszGroupFormat);
    if ( lpszGroupFormat )
      cSize += _vstprintf_s (&szGroup[cSize], ARRAYSIZE(szGroup) - cSize, lpszGroupFormat, ap);
    szGroup[cSize++] = 0;
    VERIFY(cSize<255);

    // Persist and
    Group_ ( szGroup );

    // Tidy up, and
    va_end(ap);
    return this;
}
#endif
P2Pevent*
P2Pevent::Group ( LPCSTR lpszGroupFormat, ... )
{
    // Locals
    va_list ap;
    char    szGroup[512];
    int      cSize = 0;

    // Complete formating from variable argument list.
    va_start ( ap, lpszGroupFormat );
    if (lpszGroupFormat)
      cSize += vsprintf_s ( &szGroup[cSize], ARRAYSIZE(szGroup) - cSize, lpszGroupFormat, ap );
    szGroup[cSize++] = 0;
    va_end(ap);

    // Persist and
#ifdef _UNICODE
    CString strGroupFormat = ATL::CA2WEX(lpszGroupFormat).m_szBuffer;
    Group_(strGroupFormat);
#else
    Group_(lpszGroupFormat);
#endif

    // Tidy up, and
    return this;
}
P2Pevent*
P2Pevent::Group_ ( LPCTSTR lpszGroup )
{
    P3PmsgData oData = DataWSTR08(lpszGroup);

    // Persist and
    if ( P3PmsgItem::Exists(TEvent__Grp) )
      (*this)[TEvent__Grp] = oData;
    else
      (*this).DESC += P3PmsgField ( TEvent__Grp,oData );

    // Tidy up, and
    return this;
}

//
//  Sets HRESULT code for object
//  NOTES: Over writes existing HRESULT for this level
//
//
//  Parameters:  HRESULT hr
//               System error to be set
//
//               LPCTSTR lpszhr = 0
//               Text description associated with above error
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::HResult ( HRESULT hr, LPCTSTR lpszHR )
{
    if ( hr == 0 )
      hr = GetLastError();
    if ( hr == 0 )
      hr = errno;
    if ( hr == 0 )
      hr = _doserrno;
    //if ( hr == 0 )
    //  hr = std::stderror;

    // Persist
    TCHAR szHR[256] = {0};
    if ( lpszHR == 0 )
    {
      DWORD dwSize = FormatMessage ( FORMAT_MESSAGE_FROM_SYSTEM
                                   , 0
                                   , hr
                                   , 0
                                   , szHR, ARRAYSIZE(szHR)
                                   , 0 );
               szHR[dwSize] = 0;
      lpszHR = szHR;
    }

    // Simply
    ((P2PeventNode *)c_vBlob()) -> hr = hr;
    if ( P3PmsgItem::Exists(TEvent__Sys) )
      (*this)[TEvent__Sys] = DataWSTR16(lpszHR);
    else
      (*this).DESC += P3PmsgField ( TEvent__Sys, DataWSTR16(lpszHR) );
    return this;
}
P2Pevent*
P2Pevent::HResultHMODULE ( HRESULT hr, LPCTSTR lpszHMODULEname, LPCTSTR lpszHR )
{
    if ( hr == 0 )
      hr = GetLastError();
    if ( hr == 0 )
      hr = errno;
    if ( hr == 0 )
      hr = _doserrno;

    // Persist
    TCHAR szHR[256] = {0};
    DWORD dwSize = 0;
    if ( lpszHR == nullptr )
    {
      dwSize = FormatMessage ( /*FORMAT_MESSAGE_ALLOCATE_BUFFER |*/ FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS
                             , reinterpret_cast<LPCVOID>(GetModuleHandle(lpszHMODULEname))
                             , hr
                             , MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
                             , &szHR[0] //reinterpret_cast<LPTSTR>(&szHR)
                             , ARRAYSIZE(szHR), NULL);
      szHR[dwSize] = 0;
    }

    // Delgate
    if ( dwSize > 0 )
      return HResult ( hr, &szHR[0] );
    return HResult ( hr, 0 );
}

//
//  Sets this P2Pevent as our last thread P2Pevent
//  NOTES: Delegates directly through to SetP2Pevent()
//
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::SetLast ( )
{
    // Simply
    SetP2Pevent ( this );
    return this;
}

//  Throws pointer to this P2Pevent
//  NOTES: Allocated memory must be released upon capture
//       : Usually only EVERR type events are thrown
//
//  Parameters:  bool bSetLastEvent = true
//                 true... Set this event as last event for 
//                         thread
//                 false.. Skip last thread event sequence
void
P2Pevent::Throw ( bool bSetLastEvent )
{
#if !defined(_WIN32)
    if ( getenv("P2P_ERR_TRACE") )
      fprintf(stderr, "[THROW] module='%ls' msg='%ls' advice='%ls'\n",
              GetModule() ? GetModule() : L"",
              (const wchar_t*)GetMessage().GetString(),
              (const wchar_t*)GetAdvice().GetString());
#endif
    // Thrown P2Pevent's become the last
    if ( bSetLastEvent )
      SetLast ( );

    // Simply
    throw this;
}

///////////////////////////////////////////////////////////////////////
//  Deployment configuration
//  NOTES: One plain text file, two settings, read once.  Refer Msgexception.h
//         for the format and for the two rules that make it safe.
//
//  A note on what is NOT here.  This deliberately does not use the registry,
//  an XML/JSON parser, or MFC's CWinApp profile helpers.  Everything below
//  runs on the diagnostic path of a library that is loaded into hosts as
//  different as an MFC application, a bare JVM and a Windows service, and
//  compiles for Linux as well.  A dependency taken here is a dependency taken
//  everywhere, to read two lines.
//
//  It also cannot report its own troubles the normal way: raising a P2Pevent
//  about a malformed configuration file would re-enter Display(), which is the
//  code asking for the configuration.  Complaints are therefore ACCUMULATED
//  into a note and emitted with the first diagnostic that goes to the log,
//  which is the one place they are certain to be read.

static const size_t s_cchP2PcfgPATH = 1024;

//  Declared HERE rather than beside P2PeventTextOnly() below, because
//  LoadConfigFile() drives it and is defined first.  CONSTANT-initialised on
//  purpose: refer P2PeventTextOnly() for why a dynamic initialiser on this
//  particular variable is an ordering hazard.
static int      s_nP2PeventTextOnly = -1;    // -1 unresolved, 0 dialog may be used, 1 text only

static bool     s_bP2PcfgLoaded   = false;   // search performed (found or not)
static bool     s_bP2PcfgTextOnly = false;   // "ErrToMessageBox: 0" was read
static bool     s_bP2PcfgNoted    = false;   // note already written to the log
static wchar_t  s_szP2PcfgPath [ s_cchP2PcfgPATH ] = { 0 };   // file that was read
static wchar_t  s_szP2PlogPath [ s_cchP2PcfgPATH ] = { 0 };   // resolved log, "" = stderr
static wchar_t  s_szP2PcfgNote [ 1024 ]            = { 0 };   // accumulated complaints

//
//  The one lock, guarding the configuration statics and the log write
//  NOTES: Function-local so the CRITICAL_SECTION is initialised exactly once
//         by the compiler's thread-safe static, without a dynamic initialiser
//         at namespace scope - the ordering hazard the text-only latch above
//         is constant-initialised to avoid.
//       : The section is never copied.  A CRITICAL_SECTION is documented
//         opaque, and its DebugInfo refers back to the object's own address,
//         so returning one by value from an initialising lambda would produce
//         a section pointing at a temporary that has gone.
//       : Held across the fopen/fwrite/fclose of a log write, so it must be
//         RECURSIVE - and it is, both as a Win32 section and as the Linux
//         shim's std::recursive_mutex - because the write path may still have
//         to complete the lazy load underneath itself.
static CRITICAL_SECTION s_csP2Pcfg;

static CRITICAL_SECTION*
P2PcfgLock ( )
{
    static const int s_nOnce = [] () -> int
    {
        ::InitializeCriticalSection ( &s_csP2Pcfg );
        return 1;
    } ();
    (void)s_nOnce;
    return &s_csP2Pcfg;
}

//
//  Records a complaint about the configuration, for later emission
//
//  Parameters:  LPCWSTR lpszWhat
//               One clause, no trailing punctuation.
static void
P2PcfgNote ( LPCWSTR lpszWhat )
{
    if ( !lpszWhat || !*lpszWhat )
      return;

    size_t nUsed = ::wcslen ( s_szP2PcfgNote );
    size_t nRoom = ( sizeof(s_szP2PcfgNote) / sizeof(wchar_t) ) - 1;
    if ( nUsed && nUsed + 2 < nRoom )
    {
      s_szP2PcfgNote [ nUsed++ ] = L';';
      s_szP2PcfgNote [ nUsed++ ] = L' ';
    }
    for ( size_t i = 0; lpszWhat[i] && nUsed < nRoom; i++ )
      s_szP2PcfgNote [ nUsed++ ] = lpszWhat[i];
    s_szP2PcfgNote [ nUsed ] = 0;
}

//
//  Copies a wide string into a fixed buffer, truncating rather than failing
//  NOTES: Not wcsncpy_s.  The Linux shim carries wcscpy_s and not the counted
//         form, and a diagnostic path is the last place to want a build that
//         only compiles on one of the two platforms.
static void
P2PcfgCopy ( wchar_t *pszTo, size_t cchTo, LPCWSTR lpszFrom )
{
    if ( !pszTo || !cchTo )
      return;
    size_t i = 0;
    if ( lpszFrom )
      for ( ; lpszFrom[i] && i + 1 < cchTo; i++ )
        pszTo[i] = lpszFrom[i];
    pszTo[i] = 0;
}

//
//  Opens a file by wide path, on either platform
//  NOTES: There is no _wfopen on Linux and the shim does not invent one, so
//         the path is converted to UTF-8 there.  On Windows the wide call is
//         kept, so a path outside the active code page still opens.
static FILE*
P2PcfgOpen ( LPCWSTR lpszPath, const char *pszMode )
{
    if ( !lpszPath || !*lpszPath || !pszMode )
      return 0;

#if defined(_WIN32)
    wchar_t wszMode [ 8 ] = { 0 };
    for ( int i = 0; pszMode[i] && i < 7; i++ )
      wszMode[i] = (wchar_t)(unsigned char)pszMode[i];

    FILE *pf = 0;
    if ( ::_wfopen_s ( &pf, lpszPath, wszMode ) != 0 )
      return 0;
    return pf;
#else
    char szPath [ s_cchP2PcfgPATH * 4 ] = { 0 };
    if ( ::WideCharToMultiByte ( CP_UTF8, 0, lpszPath, -1
                               , szPath, (int)sizeof(szPath), 0, 0 ) <= 0 )
      return 0;
    return ::fopen ( szPath, pszMode );
#endif
}

//
//  Directory part of a path, WITHOUT the trailing separator
//  NOTES: Hand-rolled rather than _wsplitpath_s, which would need four
//         buffers to discard three of them, and accepts either separator
//         because a configuration path may arrive from anywhere.
static void
P2PcfgDirOf ( LPCWSTR lpszPath, wchar_t *pszDir, size_t cchDir )
{
    P2PcfgCopy ( pszDir, cchDir, lpszPath );
    for ( size_t i = ::wcslen ( pszDir ); i > 0; i-- )
      if ( pszDir[i-1] == L'\\' || pszDir[i-1] == L'/' )
      {
        pszDir[i-1] = 0;
        return;
      }
    pszDir[0] = 0;                     // no separator: no directory part
}

//
//  Is this path already absolute?
static bool
P2PcfgIsAbsolute ( LPCWSTR lpszPath )
{
    if ( !lpszPath || !*lpszPath )
      return false;
    if ( lpszPath[0] == L'\\' || lpszPath[0] == L'/' )
      return true;                     // rooted, or a UNC share
    return ( lpszPath[1] == L':' );    // drive-qualified
}

//
//  Resolves a possibly relative name against a directory
//  NOTES: THE DIRECTORY IS THE CONFIGURATION FILE'S, never the working
//         directory.  A service started by the SCM inherits
//         %SystemRoot%\System32 as its working directory, so a bare
//         "errorLog.txt" resolved against the CWD would try to write into a
//         system folder - which fails unelevated, and is vandalism when it
//         succeeds.  Resolving against the file that named it puts the log
//         where whoever wrote the configuration was looking.
static void
P2PcfgResolve ( LPCWSTR lpszDir, LPCWSTR lpszName
              , wchar_t *pszOut, size_t cchOut )
{
    if ( P2PcfgIsAbsolute ( lpszName ) || !lpszDir || !*lpszDir )
    {
      P2PcfgCopy ( pszOut, cchOut, lpszName );
      return;
    }
    P2PcfgCopy ( pszOut, cchOut, lpszDir );
    size_t n = ::wcslen ( pszOut );
    if ( n + 1 < cchOut )
      pszOut[n++] = L'\\';
    pszOut[n] = 0;
    for ( size_t i = 0; lpszName[i] && n + 1 < cchOut; i++ )
      pszOut[n++] = lpszName[i];
    pszOut[n] = 0;
}

//
//  Reads a boolean setting
//  NOTES: The three spellings the setting documents, plus true/false and
//         enable/disable, because somebody will write them and refusing a
//         word whose meaning is unambiguous serves nobody.
//       : An UNRECOGNISED value leaves the setting alone and complains.  It
//         must not fall to either default silently: reading "ErrToMessageBox:
//         maybe" as OFF would suppress a windowed application's dialogs on
//         the strength of a typo, and reading it as ON would suppress the
//         evidence that the line was never understood.
//
//  Returns:     bool
//               true  ... *pbValue was set
static bool
P2PcfgParseBool ( LPCWSTR lpszValue, bool *pbValue )
{
    static LPCWSTR s_apszTrue [] = { L"1", L"on", L"yes", L"true"
                                   , L"enable", L"enabled" };
    static LPCWSTR s_apszFalse[] = { L"0", L"off", L"no", L"false"
                                   , L"disable", L"disabled" };

    for ( size_t i = 0; i < sizeof(s_apszTrue)/sizeof(s_apszTrue[0]); i++ )
      if ( ::_wcsicmp ( lpszValue, s_apszTrue[i] ) == 0 )
        { *pbValue = true;  return true; }
    for ( size_t i = 0; i < sizeof(s_apszFalse)/sizeof(s_apszFalse[0]); i++ )
      if ( ::_wcsicmp ( lpszValue, s_apszFalse[i] ) == 0 )
        { *pbValue = false; return true; }
    return false;
}

//
//  Trims ASCII blanks from both ends of a wide buffer, in place
static wchar_t*
P2PcfgTrim ( wchar_t *psz )
{
    while ( *psz == L' ' || *psz == L'\t' )
      psz++;
    size_t n = ::wcslen ( psz );
    while ( n && ( psz[n-1] == L' '  || psz[n-1] == L'\t'
                || psz[n-1] == L'\r' || psz[n-1] == L'\n' ) )
      psz[--n] = 0;
    return psz;
}

//
//  Applies one "Key: Value" line
//  NOTES: '=' is accepted as well as ':' because a caller who has written one
//         .ini file in their life will type it, and the intent is not in
//         doubt.
static void
P2PcfgApplyLine ( wchar_t *pszLine, LPCWSTR lpszDir
                , bool *pbLogNamed )
{
    wchar_t *pszKey = P2PcfgTrim ( pszLine );
    if ( !*pszKey || *pszKey == L'#' || *pszKey == L';' )
      return;                          // blank, or a comment
    if ( pszKey[0] == L'/' && pszKey[1] == L'/' )
      return;

    wchar_t *pszSep = pszKey;
    while ( *pszSep && *pszSep != L':' && *pszSep != L'=' )
      pszSep++;
    if ( !*pszSep )
    {
      P2PcfgNote ( L"line without a ':' ignored" );
      return;
    }
    *pszSep = 0;
    wchar_t *pszValue = P2PcfgTrim ( pszSep + 1 );
    pszKey = P2PcfgTrim ( pszKey );

    if ( ::_wcsicmp ( pszKey, P2PMSG_CFG_DIALOG ) == 0 )
    {
      bool bDialog = true;
      if ( !P2PcfgParseBool ( pszValue, &bDialog ) )
        P2PcfgNote ( L"ErrToMessageBox value not understood, setting ignored" );
      else if ( !bDialog )
        s_bP2PcfgTextOnly = true;      // one way only: refer LoadConfigFile
    }
    else if ( ::_wcsicmp ( pszKey, P2PMSG_CFG_LOG ) == 0 )
    {
      if ( !*pszValue )
        P2PcfgNote ( L"LogFile named nothing, setting ignored" );
      else
      {
        P2PcfgResolve ( lpszDir, pszValue, s_szP2PlogPath, s_cchP2PcfgPATH );
        *pbLogNamed = true;
      }
    }
    else
      P2PcfgNote ( L"unknown setting ignored" );
}

//
//  Reads and applies one configuration file
//  NOTES: Read as BYTES and converted, rather than with a wide stream, so
//         that the encoding is decided here instead of by whatever the host
//         has done to the C locale.  UTF-8 with or without a BOM is the
//         expected form; a UTF-16 file - what "Save as Unicode" used to
//         produce - is detected and refused by name rather than parsed into
//         nonsense, because every second byte of it is NUL and the failure
//         would otherwise present as "my file is ignored".
//
//  Returns:     bool
//               true  ... the file existed and was read
static bool
P2PcfgReadFile ( LPCWSTR lpszPath )
{
    FILE *pf = P2PcfgOpen ( lpszPath, "rb" );
    if ( !pf )
      return false;

    char  szRaw [ 8192 ] = { 0 };
    size_t nRaw = ::fread ( szRaw, 1, sizeof(szRaw) - 1, pf );
    ::fclose ( pf );
    szRaw [ nRaw ] = 0;

    if ( nRaw >= 2 && ( ( (unsigned char)szRaw[0] == 0xFF && (unsigned char)szRaw[1] == 0xFE )
                     || ( (unsigned char)szRaw[0] == 0xFE && (unsigned char)szRaw[1] == 0xFF ) ) )
    {
      P2PcfgNote ( L"configuration file is UTF-16 and was not read; save it as UTF-8" );
      return true;                     // it EXISTED - do not keep searching
    }

    const char *pszFrom = szRaw;
    if ( nRaw >= 3 && (unsigned char)szRaw[0] == 0xEF
                   && (unsigned char)szRaw[1] == 0xBB
                   && (unsigned char)szRaw[2] == 0xBF )
      pszFrom += 3;                    // UTF-8 BOM

    static wchar_t s_wszText [ 8192 ] = { 0 };
    if ( ::MultiByteToWideChar ( CP_UTF8, 0, pszFrom, -1
                               , s_wszText, (int)(sizeof(s_wszText)/sizeof(wchar_t)) ) <= 0 )
    {
      P2PcfgNote ( L"configuration file is not valid UTF-8 and was not read" );
      return true;
    }

    wchar_t wszDir [ s_cchP2PcfgPATH ] = { 0 };
    P2PcfgDirOf ( lpszPath, wszDir, s_cchP2PcfgPATH );

    bool bLogNamed = false;
    wchar_t *pszLine = s_wszText;
    while ( *pszLine )
    {
      wchar_t *pszEnd = pszLine;
      while ( *pszEnd && *pszEnd != L'\n' )
        pszEnd++;
      const bool bMore = ( *pszEnd != 0 );
      *pszEnd = 0;
      P2PcfgApplyLine ( pszLine, wszDir, &bLogNamed );
      if ( !bMore )
        break;
      pszLine = pszEnd + 1;
    }

    // "ErrToMessageBox: 0" on its own still has to land somewhere, and the
    // documented somewhere is errorLog.txt beside this file.  Without this a
    // configuration that turned the dialog off would silently fall through to
    // a stderr that a windowed host does not have.
    if ( s_bP2PcfgTextOnly && !bLogNamed && !*s_szP2PlogPath )
      P2PcfgResolve ( wszDir, P2PMSG_LOG_FILE, s_szP2PlogPath, s_cchP2PcfgPATH );

    P2PcfgCopy ( s_szP2PcfgPath, s_cchP2PcfgPATH, lpszPath );
    return true;
}

//
//  Full path of the running executable, and of this library's own module
//  NOTES: TWO candidates, and they differ in the case that matters.  Loaded
//         into a foreign host - a bare JVM under the Panama bindings, or an
//         installer stub - the executable's folder belongs to somebody else
//         entirely, and the file the deployer placed beside the MSCS DLLs
//         would never be found by looking only there.
//       : On Linux the shim's GetModuleFileNameW ignores its HMODULE and
//         answers for the executable, so the two candidates collapse into
//         one.  Harmless: the second search simply repeats the first.
static void
P2PcfgHostDir ( void *pModuleAddr, wchar_t *pszDir, size_t cchDir )
{
    pszDir[0] = 0;
    wchar_t wszPath [ s_cchP2PcfgPATH ] = { 0 };

#if defined(_WIN32)
    HMODULE hMod = 0;
    if ( pModuleAddr )
    {
      if ( !::GetModuleHandleExW ( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                 | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
                                 , (LPCWSTR)pModuleAddr, &hMod ) )
        return;
    }
    if ( !::GetModuleFileNameW ( hMod, wszPath, (DWORD)s_cchP2PcfgPATH ) )
      return;
#else
    (void)pModuleAddr;
    if ( !::GetModuleFileNameW ( 0, wszPath, (DWORD)s_cchP2PcfgPATH ) )
      return;
#endif

    P2PcfgDirOf ( wszPath, pszDir, cchDir );
}

//
//  Reads the deployment configuration
//  NOTES: Search order: P2PMSG_CONFIG, then P2Pmsg.cfg beside the running
//         executable, then beside this library's own module.  The first file
//         that EXISTS wins, whether or not it parsed cleanly - a malformed
//         file is a thing to complain about, not a reason to fall through to
//         a different one and behave as though the deployer had not spoken.
//       : ErrToMessageBox can only ever move the latch TOWARDS text.  There
//         is no assignment to 0 anywhere below, deliberately: a configuration
//         file that said the dialog was wanted must not be able to cancel
//         P2PMSG_NO_UI or an earlier ForceTextOutput(true), and it must never
//         look like the reason a service came back to the dialog branch.
//         The safe direction is the only direction this file drives.
//
//  Parameters:  LPCWSTR lpszPath
//               Explicit file to read, or 0 to perform the search.
//
//  Returns:     bool
//               true  ... a configuration file was found and read
bool
P2Pevent::LoadConfigFile ( LPCWSTR lpszPath )
{
    ::EnterCriticalSection ( P2PcfgLock ( ) );

    s_bP2PcfgLoaded    = true;         // set FIRST: a failed search must not
    s_szP2PcfgPath[0]  = 0;            // be repeated on every event raised
    s_szP2PlogPath[0]  = 0;
    s_bP2PcfgTextOnly  = false;

    bool bRead = false;
    if ( lpszPath && *lpszPath )
    {
      bRead = P2PcfgReadFile ( lpszPath );
      if ( !bRead )
        P2PcfgNote ( L"named configuration file could not be opened" );
    }
    else
    {
      wchar_t wszEnv [ s_cchP2PcfgPATH ] = { 0 };
#if defined(_WIN32)
      if ( ::GetEnvironmentVariableW ( P2PMSG_CFG_ENV, wszEnv
                                     , (DWORD)s_cchP2PcfgPATH ) )
      {
        bRead = P2PcfgReadFile ( wszEnv );
        if ( !bRead )
          P2PcfgNote ( L"P2PMSG_CONFIG names a file that could not be opened" );
      }
#endif
      // Beside the executable, then beside this library's own module.  The
      // address of a function in THIS file identifies the module without
      // naming the DLL, which would be wrong for a static link.
      for ( int i = 0; !bRead && i < 2; i++ )
      {
        wchar_t wszDir [ s_cchP2PcfgPATH ] = { 0 };
        wchar_t wszTry [ s_cchP2PcfgPATH ] = { 0 };
        P2PcfgHostDir ( i == 0 ? 0 : (void*)&P2PcfgReadFile, wszDir, s_cchP2PcfgPATH );
        if ( !*wszDir )
          continue;
        P2PcfgResolve ( wszDir, P2PMSG_CFG_FILE, wszTry, s_cchP2PcfgPATH );
        bRead = P2PcfgReadFile ( wszTry );
      }
    }

    if ( s_bP2PcfgTextOnly )
      s_nP2PeventTextOnly = 1;         // towards text, never away from it

    ::LeaveCriticalSection ( P2PcfgLock ( ) );
    return bRead;
}

//
//  Performs the search once, on the first event that needs an answer
//  NOTES: Lazy rather than at load time.  An MFC extension DLL's entry point
//         runs under the loader lock, and opening a file there is how a
//         deadlock gets built.
static void
P2PcfgEnsure ( )
{
    if ( s_bP2PcfgLoaded )
      return;
    P2Pevent::LoadConfigFile ( nullptr );
}

//
//  Which configuration file was read
//
//  Returns:     LPCWSTR
//               Full path, or L"" if none was found.
LPCWSTR
P2Pevent::ConfigFilePath ( )
{
    P2PcfgEnsure ( );
    return s_szP2PcfgPath;
}

//
//  Where text is being written
//
//  Returns:     LPCWSTR
//               Full path of the log, or L"" for stderr.
LPCWSTR
P2Pevent::LogFilePath ( )
{
    P2PcfgEnsure ( );
    return s_szP2PlogPath;
}

//
//  Names the log destination from code
//  NOTES: The code-side twin of the LogFile setting, and what a test uses:
//         several test executables share one output folder, so a test that
//         proved this by dropping a file beside the runner would reconfigure
//         its neighbours.
//       : A relative path here resolves against the WORKING directory, not a
//         configuration file's - there is no configuration file in play, and
//         a caller passing a relative path from code has one in mind.
//
//  Parameters:  LPCWSTR lpszPath
//               File to write text to, or 0 to restore stderr.
//
//  Returns:     LPCWSTR
//               The destination this call replaced.  Points at library
//               storage that the NEXT call overwrites, so copy it to keep it.
LPCWSTR
P2Pevent::SetLogFile ( LPCWSTR lpszPath )
{
    static wchar_t s_wszWas [ s_cchP2PcfgPATH ] = { 0 };

    ::EnterCriticalSection ( P2PcfgLock ( ) );
    P2PcfgEnsure ( );                  // or the lazy load would undo this
    P2PcfgCopy ( s_wszWas,      s_cchP2PcfgPATH, s_szP2PlogPath );
    P2PcfgCopy ( s_szP2PlogPath, s_cchP2PcfgPATH, lpszPath );
    ::LeaveCriticalSection ( P2PcfgLock ( ) );

    return s_wszWas;
}

//
//  What the configuration file was unhappy about
//  NOTES: Empty when the file was understood, or when there was none.  Not an
//         error channel - every complaint below names a setting that was
//         IGNORED, so the process is running on documented defaults.
//
//  Returns:     LPCWSTR
//               Complaints, separated by "; ", or L"".
LPCWSTR
P2Pevent::ConfigDiagnostic ( )
{
    P2PcfgEnsure ( );
    return s_szP2PcfgNote;
}

//
//  Name of an event class, for a log line
//  NOTES: The full set, unlike the stderr line in Display(), which collapses
//         everything that is not DEBUG or TRACE to "ERROR".  That line is
//         long-standing output which other things read, so it is left as it
//         stands; a log file being written for the first time here has no
//         such obligation and may as well be accurate.
static LPCWSTR
P2PeventClassName ( P2Pevent_e eClass )
{
    switch ( eClass )
    {
      case P2Pevent_ERROR   : return L"ERROR";
      case P2Pevent_WARNING : return L"WARNING";
      case P2Pevent_INFO    : return L"INFO";
      case P2Pevent_DEBUG   : return L"DEBUG";
      case P2Pevent_TRACE   : return L"TRACE";
      case P2Pevent_LOG     : return L"LOG";
      case P2Pevent_REPORT  : return L"REPORT";
      default               : return L"EVENT";
    }
}

//
//  Writes one event to the configured log file
//  NOTES: Opened, appended and CLOSED per event.  Slower than holding the
//         handle, and correct for the same reason a diagnostic exists at all:
//         nothing is buffered into a handle that a crash would take with it,
//         the file can be rotated or deleted underneath a running process,
//         and no descriptor is held across the lifetime of a host that may
//         never shut down cleanly.  This is an error path - the cost of an
//         fopen on it is not a consideration.
//       : Serialised, because events arrive on interior worker threads and
//         two hub pumps raising at once would otherwise interleave halves of
//         two messages into one line.
//       : Reports failure rather than handling it, so Display() can fall back
//         to stderr.  It MUST NOT raise a P2Pevent - it is called from inside
//         the display of one.
//       : THE FILE IS UTF-8, with no byte order mark, on both platforms - refer
//         the note in the body for why that had to be decided here rather than
//         left to fwprintf.  No BOM because the file is appended to and a mark
//         belongs at the start of a file, not once per entry.
//       : One entry is bounded at 8191 wide characters and truncated beyond
//         that.  A diagnostic that long has already said what it had to say,
//         and an unbounded assembly buffer on the error path is how a reporting
//         path becomes the fault it is reporting.
//
//  Returns:     bool
//               true  ... written
//               false ... no log configured, or it could not be written
static bool
P2PeventWriteLog ( P2Pevent_e eClass, LPCWSTR lpszOrigin, LPCWSTR lpszText )
{
    P2PcfgEnsure ( );
    if ( !*s_szP2PlogPath )
      return false;                    // nothing configured: stderr, as before

    ::EnterCriticalSection ( P2PcfgLock ( ) );

    // Assembled wide, converted ONCE to UTF-8, and written as BYTES.
    //
    // Not fwprintf straight to the stream, which is what this did first and
    // which produced a log file whose ENCODING DEPENDED ON THE TOOLCHAIN: MSVC
    // put UTF-16 code units into the byte stream, glibc converted to the
    // locale's multibyte encoding, and the same source produced two files that
    // no single reader could open.  Measured, not assumed - the Windows log
    // decoded as UTF-16LE at two bytes a character while the Linux one came
    // back as narrow text, and a test that searched for a wide substring passed
    // on one platform and failed on the other.
    //
    // A log file is read by people and by tools, so its encoding is part of the
    // feature rather than an implementation detail.  UTF-8 is the answer that
    // needs no argument on either platform, so the conversion is done here and
    // the file is opened in binary mode to keep the bytes exactly as produced.
    static wchar_t s_wszEntry [ 8192 ];
    static char    s_szUtf8   [ sizeof(s_wszEntry) * 2 ];
    const size_t cchEntry = sizeof(s_wszEntry)/sizeof(wchar_t);

    // Local time, formatted portably.  GetLocalTime is Windows-only and the
    // shim does not carry it; localtime is in both dialects, under two
    // different thread-safe spellings.
    wchar_t wszWhen [ 32 ] = L"";
    std::time_t tNow = std::time ( 0 );
    struct tm oTm = { 0 };
#if defined(_WIN32)
    const bool bTime = ( ::localtime_s ( &oTm, &tNow ) == 0 );
#else
    const bool bTime = ( ::localtime_r ( &tNow, &oTm ) != 0 );
#endif
    if ( bTime )
      ::swprintf ( wszWhen, sizeof(wszWhen)/sizeof(wchar_t)
                 , L"%04d-%02d-%02d %02d:%02d:%02d"
                 , oTm.tm_year + 1900, oTm.tm_mon + 1, oTm.tm_mday
                 , oTm.tm_hour, oTm.tm_min, oTm.tm_sec );

    size_t nAt = 0;
    s_wszEntry[0] = 0;

    // The configuration's own complaints, once, at the head of the first
    // entry.  This is the only destination certain to be read by whoever
    // wrote the file being complained about.
    if ( !s_bP2PcfgNoted )
    {
      s_bP2PcfgNoted = true;
      if ( *s_szP2PcfgNote )
      {
        int r = ::swprintf ( s_wszEntry, cchEntry
                           , L"%ls [CONFIG] %ls\n    %ls\n"
                           , wszWhen, s_szP2PcfgPath, s_szP2PcfgNote );
        if ( r > 0 )
          nAt = (size_t)r;
      }
    }

    // %ls, not %s - refer the note on the stderr line in Display().  The body
    // is multi-line, and is indented so that one entry reads as one entry in a
    // file that many threads append to.
    int r = ::swprintf ( s_wszEntry + nAt, cchEntry - nAt
                       , L"%ls [%ls] %ls\n"
                       , wszWhen, P2PeventClassName ( eClass )
                       , lpszOrigin ? lpszOrigin : L"" );
    if ( r > 0 )
      nAt += (size_t)r;

    for ( LPCWSTR p = lpszText ? lpszText : L""; *p && nAt + 8 < cchEntry; )
    {
      LPCWSTR q = p;
      while ( *q && *q != L'\n' )
        q++;
      r = ::swprintf ( s_wszEntry + nAt, cchEntry - nAt, L"    %.*ls\n"
                     , (int)(q - p), p );
      if ( r <= 0 )
        break;                         // truncated: a partial entry beats none
      nAt += (size_t)r;
      p = *q ? q + 1 : q;
    }

    bool bWrote = false;
    const int nBytes = ::WideCharToMultiByte ( CP_UTF8, 0, s_wszEntry, (int)nAt
                                             , s_szUtf8, (int)sizeof(s_szUtf8)
                                             , 0, 0 );
    if ( nBytes > 0 )
    {
      FILE *pf = P2PcfgOpen ( s_szP2PlogPath, "ab" );
      if ( pf )
      {
        bWrote = ( ::fwrite ( s_szUtf8, 1, (size_t)nBytes, pf ) == (size_t)nBytes );
        if ( ::fflush ( pf ) != 0 )
          bWrote = false;
        ::fclose ( pf );
      }
    }

    ::LeaveCriticalSection ( P2PcfgLock ( ) );
    return bWrote;
}

//
//  Has the dialog been taken off the table for this process?
//  NOTES: Seeded once from P2PMSG_NO_UI and the configuration file, then
//         latched, so ForceTextOutput() can move it either way afterwards.
//       : s_nP2PeventTextOnly is CONSTANT-initialised on purpose (declared up
//         with the configuration statics, which drive it).  A dynamic
//         initialiser here would be an ordering hazard: an event displayed
//         during another translation unit's dynamic initialisation would read
//         it before it had been set.
//       : TWO INPUTS, AND ONLY ONE DIRECTION.  Either the environment or the
//         configuration file may latch this to text; NEITHER can latch it back
//         to the dialog.  So P2PMSG_NO_UI=1 with "ErrToMessageBox: 1" is text,
//         and the file cannot be the reason a host that had asked for no
//         dialogs gets one - which would be exactly the regression this whole
//         area exists to prevent.  Only ForceTextOutput(), which is the host's
//         own code and not a deployment artefact, can move it the other way.
//
//  Returns:     bool
//               true  ... never raise a dialog from this process
static bool
P2PeventTextOnly ( )
{
    if ( s_nP2PeventTextOnly < 0 )
    {
#if defined(_MSC_VER)
#  pragma warning( push )
#  pragma warning( disable : 4996 )    // getenv: read-only lookup, resolved once
#endif
      const char *p = std::getenv ( "P2PMSG_NO_UI" );
#if defined(_MSC_VER)
#  pragma warning( pop )
#endif
      s_nP2PeventTextOnly = ( p && *p && *p != '0' ) ? 1 : 0;

      P2PcfgEnsure ( );                // may latch to 1; cannot clear it
      if ( s_bP2PcfgTextOnly )
        s_nP2PeventTextOnly = 1;
    }
    return s_nP2PeventTextOnly > 0;
}

//
//  Could a modal dialog raised by THIS process be seen and dismissed?
//  NOTES: Resolved once - neither answer can change for the life of a process.
//       : Two tests, and they are NOT redundant.  A service registered
//         SERVICE_INTERACTIVE_PROCESS is given the visible WinSta0 and so
//         passes the window-station test, but it still sits in session 0 and
//         so fails the first.  A service under the SCM normally fails both.
//       : Failure to obtain an answer is treated as VIEWABLE.  The fallback
//         has to be the behaviour that stood before this test existed;
//         concluding "nobody can see it" from a failed query would silence a
//         windowed application's diagnostics on the strength of an API error.
//
//  Returns:     bool
//               true  ... a dialog would reach somebody who can dismiss it
//               false ... a dialog here is unviewable, therefore a deadlock
static bool
P2PeventDialogViewable ( )
{
#if defined(_WIN32)
    static const int s_nViewable = [] () -> int
    {
        // Session 0 has been the isolated services session since Vista, and
        // the Interactive Services Detection service that used to offer to
        // show its desktop is gone from Windows 10 onwards.  Nothing raised
        // there reaches a person.
        DWORD dwSession = 0;
        if ( ::ProcessIdToSessionId ( ::GetCurrentProcessId ( ), &dwSession ) &&
             dwSession == 0 )
          return 0;

        // A service under the SCM runs on the "Service-0x0-3e7$" window
        // station, which does not carry WSF_VISIBLE.  An interactive
        // application runs on WinSta0, which does.
        HWINSTA hWinSta = ::GetProcessWindowStation ( );
        USEROBJECTFLAGS oFlags = { 0 };
        DWORD dwReturned = 0;
        if ( hWinSta &&
             ::GetUserObjectInformationW ( hWinSta, UOI_FLAGS, &oFlags
                                         , sizeof(oFlags), &dwReturned ) )
          return ( oFlags.dwFlags & WSF_VISIBLE ) ? 1 : 0;

        return 1;
    } ();
    return s_nViewable != 0;
#else
    return false;                      // off Windows MessageBoxEx is a stub
#endif
}

//
//  Should this event be emitted as TEXT rather than as a modal dialog?
//  NOTES: THE ORDER OF THESE QUESTIONS IS THE ARGUMENT.  The first two ask
//         whether anybody COULD SEE a dialog; only if somebody could does it
//         matter whether there is anywhere to WRITE instead.  A dialog nobody
//         can dismiss is not a worse diagnostic, it is a hung hub, so it is
//         refused before the write questions are reached at all.
//       : The write questions, and their history.  This test used to be
//         GetConsoleWindow() alone, which answers a much
//         narrower question than it appears to.  It returns NULL whenever the
//         process's output is REDIRECTED - a pipe, a file, ctest, any CI
//         harness - even though stderr is perfectly writable, and NULL for a
//         ConPTY-hosted console.  Those runs fell through to the MB_TASKMODAL
//         branch in Display(): a modal dialog raised on whichever thread
//         reached Cancel(), which for P2PeerCon::OnClose is the hub's OWN
//         PUMP.  With nobody to click OK the pump never returns from its
//         dispatch, never observes the CLOSE signal queued for it, and
//         CloseHub() spins on it forever (P2PeerHub.cpp:290).
//       : Not theoretical, and not new - the note that branch already carried
//         records the same deadlock in TwoConTest.  Measured on the four
//         p2p_auth tests under a redirected launch: 55 hangs in 84 runs, every
//         one of them parked in USER32!MessageBoxExW on the pump thread, with
//         the main thread in YieldForP2PmsgPump.
//       : That fix asked "is there anywhere to write this?", which left one
//         shape still landing on the dialog - a Windows service, which has no
//         console, no standard error AND nowhere to put a window.  The
//         viewability questions close it WITHOUT reversing the earlier call: a
//         windowed application is in session >0 on a visible station, so it is
//         untouched and keeps its dialogs.
//       : A service therefore needs no configuration to be safe.  P2PMSG_NO_UI
//         and ForceTextOutput() remain, for a windowed host that wants no
//         dialogs from the library at all.
//
//  Parameters:  bool bTextOnly
//               Host or environment has taken the dialog off the table.
//
//               bool bViewable
//               A dialog raised here could be seen and dismissed.
//
//               bool bStdErr
//               STD_ERROR_HANDLE is a handle that can be written.
//
//               bool bConsole
//               Process owns a console window.
//
//  Returns:     bool
//               true  ... emit as text
//               false ... a dialog is the only option, and it can be dismissed
bool
P2Pevent::TextOutputPolicy ( bool bTextOnly, bool bViewable
                           , bool bStdErr,   bool bConsole )
{
    if ( bTextOnly )
      return true;                     // asked for, explicitly
    if ( !bViewable )
      return true;                     // nobody could dismiss it: NEVER a dialog
    if ( bStdErr )
      return true;                     // redirected or console - either is fine
    if ( bConsole )
      return true;
    return false;                      // a windowed host with nowhere to write
}

//
//  Resolves the policy against this process, as it stands now
//
//  Returns:     bool
//               true  ... emit as text
//               false ... no stream to write; a dialog is the only option
static bool
P2PeventUseTextOutput ( )
{
#if defined(_WIN32)
    HANDLE hErr = ::GetStdHandle ( STD_ERROR_HANDLE );
    const bool bStdErr  = hErr && hErr != INVALID_HANDLE_VALUE;
    const bool bConsole = ::GetConsoleWindow ( ) != 0;
#else
    const bool bStdErr  = true;        // off Windows there is always a stderr
    const bool bConsole = false;
#endif
    return P2Pevent::TextOutputPolicy ( P2PeventTextOnly      ( )
                                      , P2PeventDialogViewable ( )
                                      , bStdErr
                                      , bConsole );
}

//
//  Takes the dialog off the table for this process, or puts it back
//  NOTES: What a service host calls on the way up.  It is belt and braces
//         rather than the fix - P2PeventDialogViewable() already refuses the
//         dialog for a service - and it states the intent at the deployment
//         boundary, where somebody reading the host can see it.
//       : It also covers the one service shape viewability cannot detect:
//         SERVICE_INTERACTIVE_PROCESS on a visible station is caught by the
//         session test, but a host that has moved itself out of session 0 for
//         its own reasons is not.
//
//  Parameters:  bool bTextOnly
//               true to emit every event as text and never raise a dialog.
//
//  Returns:     bool
//               The setting this call replaced, for restoration.
bool
P2Pevent::ForceTextOutput ( bool bTextOnly )
{
    bool bWas = P2PeventTextOnly ( );  // resolves the environment first
    s_nP2PeventTextOnly = bTextOnly ? 1 : 0;
    return bWas;
}

//
//  How would an event raised right now be emitted?
//
//  Returns:     bool
//               true  ... as text
//               false ... as a modal dialog
bool
P2Pevent::UsesTextOutput ( )
{
    return P2PeventUseTextOutput ( );
}

//
//  Installs the destination for text, replacing stderr
//  NOTES: A host with no stderr - a Windows service - installs a sink so the
//         diagnostic is not written to a handle that is not there.  Without
//         one, refusing the dialog would trade a hang for a silent loss.
//       : Refer P2PeventTextFnc for the thread contract, and for why this is
//         not the P2PeventCBFnc notification sink.
//
//  Parameters:  P2PeventTextFnc pfnTextSink
//               Sink to install, or 0 to restore stderr.
//
//  Returns:     P2PeventTextFnc
//               The sink this call replaced, for restoration or chaining.
static P2PeventTextFnc s_pfnP2PeventTextSink = nullptr;

P2PeventTextFnc
P2Pevent::SetTextSink ( P2PeventTextFnc pfnTextSink )
{
    P2PeventTextFnc pfnWas = s_pfnP2PeventTextSink;
    s_pfnP2PeventTextSink = pfnTextSink;
    return pfnWas;
}

//
//  Displays P2Pevent message
//  NOTES: Refer Notify() for performing registered P2Pevent sink
//         notifications
//
//
//  Parameters:  HWND hWnd
//               Parent window for which error displayed
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::Display ( const HWND hWnd )
{
    // Display the message.
    P2Pevent_e eClass = GetClass ( );
    //if ( IsP2PeventReg(1<<eClass) ||
    //     eClass == P2Pevent_ERROR )
    // The notification mask alone decides.  There used to be an unconditional
    // `|| eClass == P2Pevent_ERROR` here, which meant Configure(REMMASK,
    // P2Pevotn_ERROR) - the documented way to turn reporting off - was ignored
    // for the one class that matters, so an application could not opt out of
    // error reporting at all.  ERROR is set in the default mask, so the
    // out-of-the-box behaviour is unchanged.
    if ( s_dwP2Pevotn_MASK & (1<<eClass) )
    {
      // Some locals
      CString csMessage;

      // Address component
      CString csOrigin;
      if ( GetService() )
        csOrigin += GetService();
      if ( P3PmsgItem::Exists(TEvent__Hub) )
      {
        csOrigin += L"[Hub=";
        csOrigin += (*this)[TEvent__Hub].c_wstr();
        csOrigin += _T("]");
      }
      if ( P3PmsgItem::Exists(TEvent__Pmp) )
      {
        csOrigin += L"[Pump=";
        csOrigin += (*this)[TEvent__Pmp].c_wstr();
        csOrigin += L"]";
      }
      if ( !csOrigin.IsEmpty() )
      {
        if ( !csMessage.IsEmpty() )
          csMessage += _T("\n");
        csMessage += csOrigin;
      }

      // Function component
      CString strFunction;
      strFunction.Format(_T("[%s]%s"), GetService(), GetModule() );
      if ( P3PmsgItem::Exists(TEvent__Fnc) )
      {
        P3PmsgItem& oItemFunc = dynamic_cast<P3PmsgItem&>((*this)[TEvent__Fnc]);
        P3PmsgCurs& oCursFunc = oItemFunc.DESC.r_Curs ( );
        strFunction += _T("(");
        for ( int i = 0; oCursFunc.Goto(i); i++ )
        {
          if ( i > 0 )
            strFunction += _T(", ");
          strFunction += oCursFunc.r_name().c_name();
          if ( oCursFunc.IsItem() )
          {
            strFunction += _T("=");
            strFunction += oCursFunc.r_item().ToString();
          }
        }
        strFunction += _T(")");
      }

      // HRESULT component
      if ( GetHRESULT() )
      {
        CString csHResult = GetHRESULText();
        if ( !csHResult.IsEmpty() )
          csMessage += csHResult;
        csHResult.Format (_T("[HRESULT=%ld]\n"), GetHRESULT() );
        csMessage += csHResult;
      }

      // Message component
      if ( !GetMessage().IsEmpty() )
      {
        if ( !csMessage.IsEmpty() )
          csMessage += L"\n";
        csMessage += GetMessage();
      }

      // Advice component
      if ( !GetAdvice().IsEmpty() )
      {
        if ( !csMessage.IsEmpty() )
          csMessage += L"\n";
        csMessage += GetAdvice();
      }

      // Module component
      // NOTES: Window with focus will loose that focus
      //      : MessageBoxEx(), for some reason, performs better
      //        in a muti-threaded environment
      HWND hWndFocus = ::GetFocus ( );

      // A dialog is raised ONLY where somebody could dismiss one, and only
      // then when there is nowhere to write instead.
      // MB_TASKMODAL blocks the CALLING thread until somebody dismisses it,
      // and Display() is reached from Cancel() on interior worker threads -
      // P2PeerCon::OnClose runs on the hub's own pump.  Where nobody can click
      // OK the pump never returns from its dispatch, never observes the CLOSE
      // signal queued for it, and CloseHub() waits on it forever.  See
      // P2PeventUseTextOutput() above for the policy, and for why neither
      // GetConsoleWindow() nor a writable stderr was enough on its own.
      // Wide output, so this is correct whether or not the application has
      // put the stream into _O_U16TEXT.
      if ( P2PeventUseTextOutput ( ) )
      {
        // A host that installed a text sink owns the text.  This is not a
        // preference: a service under the SCM has NO standard error, so the
        // fwprintf below writes to a handle that is not there and the
        // diagnostic is lost outright.  Refusing the dialog must not trade a
        // hang for silence, and this is where the alternative destination -
        // the Windows event log, for P2PeerService - is reached.
        // Destination precedence, and the reason for it: an installed sink
        // beats a configured log file, which beats stderr.
        //   The sink is the HOST'S OWN CODE - P2PeerService installs the event
        // log sink on the way up - whereas the log file is a deployment
        // artefact that may have been dropped beside the executable by
        // somebody who never saw this process.  If the file won, a stray
        // P2Pmsg.cfg would silently divert a service's diagnostics out of the
        // event log and into a path under %SystemRoot%\System32, which is what
        // a service's working directory points at.
        //   And a configured log beats stderr unconditionally - not only when
        // stderr is missing - because naming a LogFile is an explicit request
        // for a destination, which a default is not.
        if ( s_pfnP2PeventTextSink )
          (*s_pfnP2PeventTextSink) ( eClass
                                   , (LPCWSTR)strFunction
                                   , (LPCWSTR)csMessage );
        else if ( !P2PeventWriteLog ( eClass
                                    , (LPCWSTR)strFunction
                                    , (LPCWSTR)csMessage ) )
        {
          // No log configured, or the configured one could not be written.
          // Either way stderr is the fallback, so an unwritable log costs the
          // formatting and not the diagnostic.
          // %ls, NOT %s. In a WIDE printf, "%s" means a NARROW string on glibc
          // and a WIDE one on MSVC - so %s here printed each UTF-16 argument as
          // char*, stopping at the first embedded NUL, i.e. after exactly ONE
          // character on Linux. Every diagnostic this library emitted there was
          // destroyed down to its first letter, and "[ERROR]" arrived as "[E]" -
          // which reads like a severity code rather than a bug, which is why it
          // survived so long. Recorded in an internal session log at the time;
          // it went on to obstruct two separate diagnoses before it was tracked
          // down. %ls is wide in both dialects.
          fwprintf ( stderr, L"\n[%ls] %ls\n%ls\n"
                   , eClass == P2Pevent_DEBUG ? L"DEBUG" :
                     eClass == P2Pevent_TRACE ? L"TRACE" : L"ERROR"
                   , (LPCWSTR)strFunction
                   , (LPCWSTR)csMessage );
          fflush ( stderr );
        }
      }
      else if ( eClass == P2Pevent_DEBUG )
        MessageBoxEx ( 0, csMessage, strFunction
                     , MB_OK | MB_ICONINFORMATION
                     | MB_TASKMODAL, 0 );
      else if ( eClass == P2Pevent_TRACE )
        MessageBoxEx ( 0, csMessage, strFunction
                     , MB_OK | MB_USERICON
                     | MB_TASKMODAL, 0 );
      else
        MessageBoxEx ( 0, csMessage, strFunction
                     , MB_OK | MB_ICONERROR
                     | MB_TASKMODAL, 0 );

      // Restore focus of original window
      // NOTES: MessageBoxEx() will NOT restore focus upon
      //        completion.
      //      : This action is only performed when user has
      //        provided us with a parent window
      if ( hWnd      &&
           hWndFocus    )
        ::SetFocus ( hWndFocus );

    }

    // Tidy up and
    m_bDisplayed = true;
    return this;
}

//
//  Prints P2Pevent message
//  NOTES: Refer Notify() for performing registered P2Pevent sink
//         notifications
//
//
//  Parameters:  FILE *fd = stdout
//               TTY output device
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::Print ( FILE *fd )
{
    // Delegate, essentually intercepted for chaining
    P3PmsgItem::Print ( fd, 0, 1 );
    return this;
}

//
//  Isolates this P2Pevent instance
//  NOTES: Isolated P2Pevent objects are no longer thread attached
//         and as such may be thread swapped
//       : Refer SetP2Pevent() and GetP2Pevent() for further deatils
//
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::Isolate ( )
{
    // Object is floating free
    if ( GetP2Pevent() == this )
      IsolateP2Pevent();
    return this;
}

//
//  Perform notifications for P2Pevent
//  NOTES: Notifications are normally performed upon event
//         cancellation, refer Cancel() for further details
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::Notify ( )
{
    // Perform notifications
    P2PeventPost_HWND ( *this );
    return this;
}

///////////////////////////////////////////////////////////////////////
//  Environmental snapshots
//  NOTES: Load P2Pevent object up with both a variable set of 
//         environment parameters associated with P2Pevent
//       : Intent is to form a snapshot of the state associated with
//         the event

//
//  Sets formatted function parameters
//  NOTES: Appends to existing parameter list
//
//  Parameters:  LPCTSTR lpszFParamName
//               Name of the function parameter
//
//               const P3PmsgData& oData
//               Data associated with above parameter
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::SetFParam  ( LPCTNAM lpszFParamName, const P3PmsgData& oData )
{
    // Simply append to module parameter list
    if ( !P3PmsgItem::Exists(TEvent__Fnc) )
      (*this).DESC += P3PmsgField ( TEvent__Fnc, DataWSTR08(L"") );
    P3PmsgItem& oItem = dynamic_cast<P3PmsgItem&>((*this)[TEvent__Fnc]);
    oItem.DESC += P3PmsgField ( lpszFParamName, oData );

    // Tidy up, and
    return this;
}

//
//  Placeholder for formatting 3rd Party function parameters
//
//  Parameters:  P2Pevent *pEvent
//               Re-chaining object
//
//  Returns:     P2Pevent*
//               Pointer to this object suitable for P2Pevent chaining
P2Pevent*
P2Pevent::SetFParam ( P2Pevent *pEvent )
{
    UNREFERENCED_PARAMETER(pEvent);
    // Tidy up, and
    ASSERT(this!=pEvent);
    return this;
}

///////////////////////////////////////////////////////////////////////
//  Configuration 

//
//  Sets a formatted service name
//  NOTES: Service name is global and usually constant for a process
//         and as such is set as part of the initialisation sequences
//       : "Svc" field is added to all P2Pevents generated in the
//         process domain for which it's defined
//
//
//  Parameters:  LPCTSTR lpszServiceName
//               Service name to be formatted and loaded.
//
//               ...
//               Variable argument list associated with above format
//               string.
//
//  Returns:     LPCTSTR
//               Formatted service name
LPCTSTR
P2Pevent::Service ( LPCTSTR lpszServiceFormat, ... )
{
    // Introduce the locals.
    va_list       ap;                  // Variable argument list
    TCHAR         szService[512]={0};
    int           cSize = 0;
    va_start ( ap, lpszServiceFormat );

    // Complete formating from variable argument list.
    if ( lpszServiceFormat )
      cSize = _vstprintf_s ( szService, ARRAYSIZE(szService), lpszServiceFormat, ap );
    ASSERT(cSize<ARRAYSIZE(s_szServiceName));
    if ( cSize < ARRAYSIZE(s_szServiceName) )
      wcscpy_s ( s_szServiceName, ARRAYSIZE(s_szServiceName), szService );
    va_end ( ap );

    // Persist, and
    return s_szServiceName;
}

//
//  Configures P2Pevent reporting 
//  NOTES: Reporting is only performed for configured events and is
//         independant of the registered P2Pevent notification sinks
//
//
//  Parameters:  ReportCmd_e eCmd
//               Configuration command
//
//               DWORD dwCmdArg
//               Argument associated with above command
//
//  Returns:     DWORD
//               Configuration command dependent result
//
DWORD
P2Pevent::Configure ( ReportCmd_e eCmd, DWORD dwCmdArg )
{
    // Command application
    if ( eCmd == ADDMASK )
      return s_dwP2Pevotn_MASK |=  dwCmdArg;
    if ( eCmd == REMMASK )
      return s_dwP2Pevotn_MASK &= ~dwCmdArg;
    if ( eCmd == SETMASK )
      return s_dwP2Pevotn_MASK  =  dwCmdArg;
    if ( eCmd == GETMASK )
      return s_dwP2Pevotn_MASK;

    // Tidy up and
    ASSERT(0);
    return (DWORD)~0;
}

///////////////////////////////////////////////////////////////////////
//  Troubleshooting

void
P2Pevent::AssertValid ( ) const
{
    // Firstly delegate
  __super::AssertValid ( );
}

//
//  Breaks operation and places operator in the debugger
//  NOTES: Null operation in release mode
//       : Add and remove from P2Pevent chain during debugging etc
//
//  Returns:     P2Pevent*
//               Pointer to this object used for P2Pevent chaining
P2Pevent*
P2Pevent::Break ( ) const
{
  __debugbreak ( );
    return (P2Pevent *)this;
}

///////////////////////////////////////////////////////////////////////
//  Property management

//
//  Fetches the P2Pevent class
//  NOTES: Refer MakeEvent() for setting P2Pevent class
//
//  Returns:     P2Pevent_e
//               Event class
P2Pevent_e
P2Pevent::GetClass ( )
{
    return (P2Pevent_e)((P2PeventNode *)c_vBlob())->eClass;
}

LPCTSTR
P2Pevent::GetClassText ( )
{
    P2Pevent_e eClass = GetClass();
    if ( eClass == P2Pevent_UNDEF )
      return _T("EVUND");
    if ( eClass == P2Pevent_ERROR )
      return _T("EVERR");
    if ( eClass == P2Pevent_WARNING )
      return _T("EVWRN");
    if ( eClass == P2Pevent_DEBUG )
      return _T("EVDBG");
    if ( eClass == P2Pevent_TRACE )
      return _T("EVTRC");
    if ( eClass == P2Pevent_LOG )
      return _T("EVLOG");
    if ( eClass == P2Pevent_INFO )
      return _T("EVINF");
    if ( eClass ==  7 )
      return _T("EV005");
    if ( eClass ==  8 )
      return _T("EV005");
    if ( eClass ==  9 )
      return _T("EV005");
    if ( eClass == 10 )
      return _T("EV005");
    if ( eClass == 11 )
      return _T("EV005");
    if ( eClass == 12 )
      return _T("EV005");
    if ( eClass == 13 )
      return _T("EV005");
    if ( eClass == 14 )
      return _T("EV005");
    if ( eClass == 15 )
      return _T("EV005");
    if ( eClass == 16 )
      return _T("EV005");
    if ( eClass == 17 )
      return _T("EV005");
    if ( eClass == 18 )
      return _T("EV005");
    if ( eClass == 19 )
      return _T("EV005");
    if ( eClass == 20 )
      return _T("EV005");
    if ( eClass == 21 )
      return _T("EV005");
    if ( eClass == 22 )
      return _T("EV005");
    if ( eClass == 23 )
      return _T("EV005");
    if ( eClass == 24 )
      return _T("EV005");
    if ( eClass == 25 )
      return _T("EV005");
    if ( eClass == 26 )
      return _T("EV005");
    if ( eClass == 27 )
      return _T("EV005");
    if ( eClass == 28 )
      return _T("EV005");
    if ( eClass == 29 )
      return _T("EV005");
    if ( eClass == 30 )
      return _T("EV005");
    if ( eClass == 31 )
      return _T("EV005");

    // Hmmm
    return L"EV___";
}

//
//  Fetches P2Pevent service or application name
//  NOTES: Service auto-assigned in MakeEvent()
//
//  Returns:     LPCTSTR
//               Application or service name
//
LPCTSTR
P2Pevent::GetService ( )
{
    if ( P3PmsgItem::Exists(TEvent__Svc) )
      return (*this)[TEvent__Svc].c_wstr();
    return L"";
}

//
//  Fetches P2Pevent group name
//  NOTES: Refer Group() for group name assignment
//
//  Returns:     LPCTSTR
//               Group name
//
LPCTSTR
P2Pevent::GetGroup ( )
{
    if ( P3PmsgItem::Exists(TEvent__Grp) )
      return (*this)[TEvent__Grp].c_wstr();
    return L"";
}

//
//  Fetches composition P2Pevent description
//
//  Returns:     LPCTSTR
//               Message text
//
const CString&
P2Pevent::GetMessage ( )
{
    if ( m_csDescription.IsEmpty()       &&
         P3PmsgItem::Exists(TEvent__Dsc)     )
    {
      P3PmsgList& oList = dynamic_cast<P3PmsgList&>((*this)[TEvent__Dsc]);
      VBLaddr aEntry = oList.GetHeadPos();
      while ( aEntry )
      {
        if ( !m_csDescription.IsEmpty() )
          m_csDescription += L"\n";
        P3PmsgData& oEntry = oList.GetNext ( aEntry );
        m_csDescription += oEntry.c_wstr();
      }
    }
    return m_csDescription;
} 

//
//  Fetches P2Pevent advice
//  NOTES: Refer Advice() for accumulating P2Pevent advice
//
//  Returns:     const CString&
//               Advice text
//
const CString&
P2Pevent::GetAdvice ( )
{
    if ( m_csAdvice.IsEmpty()            &&
         P3PmsgItem::Exists(TEvent__Adv)    )
    {
      P3PmsgList& oList = dynamic_cast<P3PmsgList&>((*this)[TEvent__Adv]);
      VBLaddr aEntry = oList.GetHeadPos();
      while ( aEntry )
      {
        if ( m_csAdvice.IsEmpty() )
          m_csAdvice += _T("ADVICE\t: ");
        else
          m_csAdvice += _T("\n\t: ");
        P3PmsgData& oEntry = oList.GetNext ( aEntry );
        m_csAdvice += oEntry.c_wstr();
      }
    }
    return m_csAdvice;
}

//
//  Fetches P2Pevent module
//  NOTES: Refer Module() for setting P2Pevent module name
//
//  Returns:     LPCTSTR
//               Name of precipitating module
//
LPCTSTR
P2Pevent::GetModule ( )
{
    if ( P3PmsgItem::Exists(TEvent__Fnc) )
      return (*this)[TEvent__Fnc].c_wstr();
    return L"";
}

BOOL
P2Pevent::EmptyModule ( )
{
    if ( P3PmsgItem::Exists(TEvent__Fnc) )
      return TRUE;
    return FALSE;
}

//
//  Fetches P2Pevent HRESULT property
//  NOTES: Synomym for win32 error code
//       : Refer HResult() for setting P2Pevent HRESULT property
//
HRESULT
P2Pevent::GetHRESULT ( )
{
    return ((P2PeventNode *)c_vBlob())->hr;
}
LPCTSTR
P2Pevent::GetHRESULText ( )
{
    if ( P3PmsgItem::Exists(TEvent__Sys) )
      return (*this)[TEvent__Sys].c_wstr();
    return L"";
}

//
//  Fetches P2Pevent object creation time
//  NOTES: Due consideration should be given to the domain in which
//         the event was created.  Unrelated P2Pevents may have the
//         same creation time
//       : Auto-assigned in MakeEvent()
//
//  Returns:     time_t
//               Epoch at which event was created
//
time_t
P2Pevent::GetTime ( )
{
    return ((P2PeventNode *)c_vBlob()) -> dwEpoch;
}

//
//  Fetches P2Pevent event number
//  NOTES: Auto-assigned in MakeEvent()
//
//  Returns:     DWORD
//               Internally allocated event number
//
DWORD
P2Pevent::GetEvent ( )
{
    return ((P2PeventNode *)c_vBlob()) -> nEventNo;
}

///////////////////////////////////////////////////////////////////////
//  P2Pevent sink notification callbacks
//  NOTES: CreateP2PeventSink() compatible

//
//  Posts P2Pevent to nominated window
//  NOTES: Window MUST intercept posted P2Pevent and release
//         associated resources
//
//  Parameters: P2PeventSinkID nSinkID
//              Notification sink
//
//              DWORD dwCBKey
//              Callback key, effectively handle to window
//
//              const P2Pevent& oEvent
//              Reference to notification event
void WINAPI
P2PeventCB_HWND ( P2PeventSinkID nSinkID, DWORD_PTR dwCBKey
                , const P2Pevent& oEvent )
{
    // Window notifications
    // NOTES: Copy asynchonously posted
    if ( IsWindow((HWND)dwCBKey) ) 
      ::PostMessage ( (HWND)dwCBKey, WM_P2PeventNOTN
                    , (WPARAM)new P2Pevent(oEvent), nSinkID );
}

//
//  Posts P2Pevent to nominated thread
//  NOTES: Thread MUST intercept posted P2Pevent and release
//         associated resources
//
//  Parameters: P2PeventSinkID nSinkID
//              Notification sink
//
//              DWORD dwCBKey
//              Callbak key, effectively handle to window
//
//              const P2Pevent& oEvent
//              Reference to notification event
void
P2PeventCB_Thread ( P2PeventSinkID nSinkID, DWORD_PTR dwCBKey
                  , const P2Pevent& oEvent )
{
    // Thread notifications
    // NOTES: Copy asynchonously posted
    if ( IsWindow((HWND)dwCBKey) ) 
      ::PostThreadMessage ( (DWORD)dwCBKey, WM_P2PeventNOTN
                          , (WPARAM)new P2Pevent(oEvent), nSinkID );
}
void WINAPI 
P2PeventPost_HWND ( const P2Pevent& oP2Pevent )
{
//((P2Pevent&)oP2Pevent).Print(stdout);
    if ( g_HWnd && IsWindow(g_HWnd) ) 
      ::PostMessage ( g_HWnd, s_uiWM_USER
                    , (WPARAM)new P2Pevent(oP2Pevent), s_dwUserKey );

    if ( s_pfnP2PeventCB )
      (*s_pfnP2PeventCB) ( s_nP2PeventSinkID, s_dwSinkUserKey, oP2Pevent );
}

///////////////////////////////////////////////////////////////////////
//  Catch block interceptions
//  NOTES: Utilised in catch handlers to translate exception object
//         into P2Pevent object.  Usually extracts Message() and
//         HResult() from message/environment
//
P2Pevent*
P2Pevent_catch ( CException *pEx, bool bDeleteEx )
{
    TCHAR szError[512];
    pEx -> GetErrorMessage ( szError, ARRAYSIZE(szError) );
    if ( bDeleteEx )
      pEx -> Delete ();
    //  _T("%ls"), never szError as the format itself: Message() is
    //  Message(LPCWSTR lpszFormat, ...) and hands the string to _vstprintf_s,
    //  so any '%' in the text CException::GetErrorMessage produced consumed a
    //  variadic argument that was never passed (finding M3 of the internal,
    //  unpublished security review). This is
    //  the generic catch-block translator, so the text is whatever exception
    //  reached it - including file paths and OS-formatted messages carrying
    //  inserts. %ls not %s: wide in both the MSVC and glibc dialects (5aa9b2a).
    P2Pevent *pEVT = EVERR
                  -> Message( _T("%ls"), szError )
                  -> HResult( GetLastError() );
    return pEVT;
}


///////////////////////////////////////////////////////////////////////

__declspec(thread) P2Pevent *tls_pMsgexception = 0;

//
//  THE THREAD'S LAST EVENT DIES WITH THE THREAD
//  NOTES: SetP2Pevent deletes the event it displaces, so a thread that raises
//         a hundred events holds one.  Nothing ever displaced the LAST one.
//         The thread ended, the slot went with it, and the P2Pevent the slot
//         pointed at did not - one event, plus every P3Pmsg block it owns,
//         for every thread that ever raised one.  On a host that cycles pump
//         threads that is unbounded.
//       : Measured on p2p_e2ewaive under LSan: 23,682 bytes in 41 allocations,
//         all of it ONE refused-seal diagnostic that SealAppMsgOutbound parked
//         with ->Display()->SetLast() on a pump thread that then exited.
//       : LSan called every one of those 41 blocks INDIRECT and none of them
//         direct, which reads like an orphan with no parent.  It is not: the
//         graph is CYCLIC - the VB heap's block registry points at the blocks
//         and the blocks carry the heap - and no member of a cycle is the one
//         nothing else points at.  Absence of a direct root was the clue, not
//         a contradiction.
//       : Not a DLL_THREAD_DETACH hook.  Msgcore.cpp keeps DllMain commented
//         out, Msgcore also builds as a static library, and the POSIX build
//         has no such callback at all.  A thread_local destructor is the one
//         mechanism all three configurations share.
//       : Armed from SetP2Pevent rather than declared beside the slot, so a
//         thread that never raises an event never registers a destructor.
//         m_bArmed exists to be written: a store the optimiser cannot drop is
//         what forces the initialisation that performs the registration.
namespace
{
  struct P2PeventLastReaper
  {
    bool m_bArmed = false;
    void Arm ( ) { m_bArmed = true; }
    ~P2PeventLastReaper ( )
    {
      //  Deliberately NOT through SetP2Pevent: that would touch this guard
      //  while the guard is being destroyed.  The slot is cleared BEFORE the
      //  delete because ~P2Pevent reads it back - it isolates itself when it
      //  finds it is the thread's last
      P2Pevent *pEvent  = tls_pMsgexception;
      tls_pMsgexception = 0;
      delete pEvent;
    }
  };

  thread_local P2PeventLastReaper tls_oLastReaper;
}


//
//  Sets last P2Pevent for current thread context
//  NOTES: Last thread events exist until displaced by a subsequent
//         P2Pevent or cancelled
//
//  Parameters:  P2Pevent *pEvent
//               New P2Pevent to be set for thread context
//

void
SetP2Pevent ( P2Pevent *pEvent )
{
    // To be sure, to be sure
    if ( tls_pMsgexception == pEvent )
      return;
    // Recovery
    if ( tls_pMsgexception )
      delete tls_pMsgexception;
    //  Registers this thread's reaper, the first time this thread parks an
    //  event.  Refer P2PeventLastReaper above: it is what deletes this one if
    //  nothing displaces it before the thread ends
    if ( pEvent )
      tls_oLastReaper.Arm ( );
    tls_pMsgexception = pEvent;
}

//
//  Fetch last P2Pevent for current thread context
//  NOTES: Last thread events exist until displaced by a subsequent
//         P2Pevent or cancelled
//
//  Returns:     P2Pevent*
//               Retrieved P2Pevent
//
P2Pevent*
GetP2Pevent ( )
{
    // Simply
    return tls_pMsgexception;
}


P2Pevent*
IsolateP2Pevent()
{
    // Locals
    P2Pevent *pMsgexception = tls_pMsgexception;
    tls_pMsgexception = 0;
    return pMsgexception;
}

//
//  Throw last P2Pevent pointer exception for current thread context
//  NOTES: Thrown event remains the last P2Pevent for thread
//
void
ThrowP2Pevent()
{
    // Throw last event
    if ( tls_pMsgexception )
      throw tls_pMsgexception;
    return;
}

//DWORD
//IsP2PeventReg ( DWORD dwP2PeventMask )
//{
//    return s_dwP2PeventRegMask & dwP2PeventMask;
//}
