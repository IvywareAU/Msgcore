// Copyright © 2000-2010, 2026 Ivyware Pty Ltd, Khrustal & Mann
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
//  P2Pevent prototypes and definitions.
//
#pragma once
#ifndef NO_DEBUG_NEW
#define new DEBUG_NEW
#endif

#include "Msgcore.h"
#include "MsgCollectors.h"
#include "P2Pmsg.h"
#include "MsgAttr.h"
#include "MsgStck.h"
#include "MsgCurs.h"

//
//  P2Pevent classes 
//  NOTES: Form the basis upon which all P2Pevent's are displayed
//         and notifications performed for registered sinks
// 
typedef unsigned int  P2PeventSinkID;    // Event sink identification
typedef DWORD         P2PevotnMask;      // Event notification mask

typedef enum
{
    P2Pevent_UNDEF     =  0,
    P2Pevent_ERROR     =  1,
    P2Pevent_WARNING   =  2,
    P2Pevent_INFO      =  3,
    P2Pevent_DEBUG     =  4,
    P2Pevent_TRACE     =  5,
    P2Pevent_LOG       =  6,
    P2Pevent_REPORT    =  7,
    P2Pevent_RESERVED8 =  8,
    P2Pevent_RESERVED9 =  9,

    P2Pevent_USER0     = 16,
    P2Pevent_USER1     = 17,
    P2Pevent_USER3     = 18,
} P2Pevent_e;

//
//  P2Pevent display and registered sink notification masks
//  NOTES: Refer Display() for presentation of P2Pevent's in the
//         context of application
//       : Refer Notify() for performing P2Pevent notifications in
//         the context of registered P2Pevent sinks
// 
const DWORD P2Pevotn_ERROR =  (1<<P2Pevent_ERROR);
// WARNING had no mask constant, alone among the classes anything raises, and
// that was not a cosmetic gap: the default mask was P2Pevotn_ERROR only, so
// every EVWRN->Display() in these repositories was invisible out of the box,
// and the one place that wanted them had to write (1 << P2Pevent_WARNING) by
// hand (_TargetCore_UseExamples/.../DialogOrLogFile.cpp).  Found while closing
// the TargetCore finding F-S6-3 - recorded in that repository's production
// plan, which is not published here - whose "make the omission loud" half
// turned out to be a warning nobody would have seen.
//
// NAMING THE CLASS DID NOT TURN IT ON; a later decision did.  Since
// 2026-08-20 the default mask is ERROR|WARNING, closing F-S6-4 - see the
// initialiser of s_dwP2Pevotn_MASK in Msgexception.cpp for the argument and
// for the one call that opts back out.  This constant is still what makes
// either direction sayable.
const DWORD P2Pevotn_WARNING= (1<<P2Pevent_WARNING);
const DWORD P2Pevotn_DEBUG =  (1<<P2Pevent_DEBUG);
const DWORD P2Pevotn_TRACE =  (1<<P2Pevent_TRACE);
const DWORD P2Pevotn_LOG   =  (1<<P2Pevent_LOG);
const DWORD P2Pevotn_REPORT=  (1<<P2Pevent_REPORT);
const DWORD P2Pevotn_INFO  =  (1<<P2Pevent_INFO);

const DWORD P2Pevotn_USER0 =  (1<<P2Pevent_USER0);

const DWORD P2Pevotn_MISC  = ~( P2Pevotn_ERROR
                              | P2Pevotn_DEBUG
                              | P2Pevotn_TRACE
                              | P2Pevotn_LOG
                              | P2Pevotn_REPORT )
                                & ~(1<<31);       // Top bit reserved
const DWORD P2Pevotn_FULL  = ~0 & ~(1<<31);  // Top bit reserved

#define TEvent__Adv L"Adv"
#define TEvent__Dsc L"Dsc"
#define TEvent__Fnc L"Fnc"             // Serialisation of function parameters
#define TEvent__Grp L"Grp" 
#define TEvent__Hub L"Hub"
#define TEvent__Pmp L"Pmp"
#define TEvent__Sys L"Sys"
#define TEvent__Svc L"Svc"
#define LAdvice     L"Advice"          // Purely for Multi-byte to UNICODE swapover
#define LDesc       L"Desc"            // Purely for Multi-byte to UNICODE swapover

//
//  P2Pevent callback definitions (default set)
//  NOTES: Used to manage registered P2PeventSink notifications
//       : Call backs will be performed for each filtered event, 
//         other options include thread and window notifications
//       : Remember to cancel the notification prior to allowing
//         the implementation code to go out of scope
class P2Pevent;
typedef void (WINAPI *P2PeventCBFnc)
         ( P2PeventSinkID  nSinkID,    // Sink identification code
           DWORD_PTR      dwCBKey,     // User definied callback key
           const P2Pevent& oP2Pevent );// Notification event

//
//  Destination for an event Display() has decided to emit as TEXT
//  NOTES: Default destination is stderr.  A host that HAS no stderr - a
//         Windows service under the SCM - installs one of these so the
//         diagnostic still lands somewhere it can be read, rather than being
//         written to a handle that is not there.
//       : This is deliberately NOT the P2PeventCBFnc notification sink.  That
//         is a single global slot (refer P2PeventPost_HWND), so a service
//         claiming it would silence whatever its host had registered; the two
//         answer different questions and must not compete for one pointer.
//       : Called on the thread that RAISED the event, which is routinely an
//         interior worker thread - a hub's own pump among them.  It must not
//         block, and it must not raise UI.
typedef void (WINAPI *P2PeventTextFnc)
         ( P2Pevent_e     eClass,      // Event class
           LPCWSTR        lpszOrigin,  // "[service]module(parameters)"
           LPCWSTR        lpszText );  // Assembled message body

//
//  Deployment configuration file, and the log it can name
//  NOTES: Named here rather than left as literals in the implementation
//         because a host, an installer and a test all have to agree on them,
//         and three copies of a file name is two too many.
//       : P2PMSG_CONFIG in the environment overrides the search with an
//         explicit path.  It exists for tests: several test executables share
//         one output folder, so a configuration file dropped beside them would
//         reconfigure every other test in that folder.
#define P2PMSG_CFG_FILE   L"P2Pmsg.cfg"        // beside the host executable
#define P2PMSG_CFG_ENV    L"P2PMSG_CONFIG"     // explicit path, overrides search
#define P2PMSG_LOG_FILE   L"errorLog.txt"      // LogFile default
#define P2PMSG_CFG_DIALOG L"ErrToMessageBox"   // 1/0, on/off, yes/no, true/false
#define P2PMSG_CFG_LOG    L"LogFile"           // destination for text

//
//  P2Pevent implementation
//  NOTES: P2Peer events container
//
class Msgcore_EXT P2Pevent : public P3PmsgItem
{
    // Constructors and destructor
    public:
        P2Pevent ( );
        P2Pevent ( const P2Pevent& rhs );
        P2Pevent ( const P3PmsgItem& rhs );
      virtual
       ~P2Pevent ( );
      P2Pevent*
        Cancel  ( bool bNotify = true );
      static void
        Register4P2Pevents ( HWND hWnd, UINT uiWM_USER
                           , DWORD dwEventFilter, DWORD_PTR dwUserKey );
      static void
        Register4P2Pevents ( P2PeventSinkID nEventSinkID, P2PeventCBFnc pftP2PeventCB
                           , DWORD dwEventFilter, DWORD_PTR dwUserKey );

    typedef enum
    {
      ADDMASK,
      REMMASK,
      SETMASK,
      GETMASK,
    } ReportCmd_e;

    // Factories
    public:
      static P2Pevent*
        MakeEvent( P2Pevent_e eClass = P2Pevent_ERROR );
      P2Pevent*
        InitEvent( P2Pevent_e eClass );

    // Overloaded operators
    public:
      P2Pevent&
        operator = ( const P2Pevent& rhs );

    // Chained operations
    public:
      P2Pevent*
        Module  ( LPCSTR lpszModuleName, ... );
#ifdef _UNICODE
      P2Pevent*
        Module  ( LPCWSTR lpszModuleName, ... );
#endif
      P2Pevent*
        Module_ ( LPCTSTR lpszModuleName );
      P2Pevent*
        HResult ( HRESULT hr, LPCTSTR lpszhr = 0 );
      P2Pevent*
        HResultHMODULE ( HRESULT hr, LPCTSTR lpszHMODULE, LPCTSTR lpszhr = 0 );
#ifdef _UNICODE
      P2Pevent*
        Message ( LPCWSTR lpszFormat, ... );
#endif
      P2Pevent*
        Message ( LPCSTR lpszFormat, ... );
      P2Pevent*
        Message_( LPCTSTR lpszMessage );
#ifdef _UNICODE
      P2Pevent*
        Advice  ( LPCWSTR lpszFormat, ... );
#endif
      P2Pevent*
        Advice  ( LPCSTR lpszFormat, ... );
      P2Pevent*
        Advice_ ( LPCTSTR lpszFormat );
#ifdef _UNICODE
      P2Pevent*
        Group  ( LPCWSTR lpszGroupFormat, ... );
#endif
      P2Pevent*
        Group  ( LPCSTR lpszGroupFormat, ... );
      P2Pevent*
        Group_ ( LPCTSTR lpszGroupFormat );
      P2Pevent*
        Time    ( DWORD dwEpochTime );
      P2Pevent*
        Display ( const HWND hWnd = 0 );
      P2Pevent*
        Print ( FILE *fd = stdout );
      P2Pevent*
        Notify  ( );
      P2Pevent*
        SetLast ( );
      P2Pevent*
        Isolate ( );
      void
        Throw   ( bool bSetLastEvent = true );

    // Associated P2Pevent function parameters
    // NOTES: Variable parameter set associated with P2Pevent
    public:
      P2Pevent*
        SetFParam  ( LPCTNAM lpszFParamName, const P3PmsgData& oData );
      P2Pevent* SetFParam(LPCTNAM lpszFParamName, const P3PmsgItem & oItem)
      {
        // Function parameter list housekeeping
        // NOTES: Creation upon demand etc
        if (!P3PmsgItem::Exists(TEvent__Fnc))
          (*this).DESC += P3PmsgItem (TEvent__Fnc, DataWSTR08(L""));
        P3PmsgItem& oItemModule = dynamic_cast<P3PmsgItem&>((*this)[TEvent__Fnc]);

        // Create placeholder and append passed contents
        // NOTES: Parameter order is defined by order of creation
        oItemModule.DESC += P3PmsgItem (lpszFParamName, DataWSTR08(L""));
        P3PmsgItem& oItemVar = dynamic_cast<P3PmsgItem&>(oItemModule[lpszFParamName]);
        oItemVar.DESC += oItem;

        // Tidy up, and
        return this;
      }
      P2Pevent*
        SetFParam  ( P2Pevent *pEvent );
      //P2Pevent*
      //  SetFParam  ( LPCTNAM lpszFParamName, const P3PmsgItem& oItem );

    // Configuration
    // NOTES: Manage P2Pevent default and reporting
    public:
      static LPCTSTR
        Service ( LPCTSTR lpszServiceFormat, ... );
      static DWORD
        Configure ( ReportCmd_e eCmd, DWORD dwCmdArg );

    // Diagnostic output policy
    // NOTES: Display() emits an event either as TEXT or as a modal dialog.
    //       : A dialog blocks the thread that raised the event until somebody
    //         dismisses it, and events are raised on interior worker threads -
    //         P2PeerCon::OnClose runs on the hub's own PUMP.  A dialog nobody
    //         can see is therefore a hub that never stops.  Refer
    //         P2PeventUseTextOutput() in the implementation for the policy and
    //         for why each of its inputs is asked for.
    //       : ForceTextOutput() takes the dialog off the table for the process,
    //         as the P2PMSG_NO_UI environment variable does, but from code and
    //         reversibly.  It is what a service host should call on the way up.
    //       : TextOutputPolicy() is the pure decision, exposed for test: a test
    //         process cannot move itself into session 0, so the policy is
    //         driven directly and the live inputs are asserted separately.
    public:
      static bool
        ForceTextOutput ( bool bTextOnly );
      static bool
        UsesTextOutput ( );
      static bool
        TextOutputPolicy ( bool bTextOnly, bool bViewable
                         , bool bStdErr,   bool bConsole );
      static P2PeventTextFnc
        SetTextSink ( P2PeventTextFnc pfnTextSink );

    // Deployment configuration
    // NOTES: A plain text file beside the host executable, read ONCE and
    //         lazily, so that a deployment can send diagnostics to a log file
    //         instead of a modal dialog without rebuilding anything.  Two
    //         settings, one per line, "Key: Value", '#' or ';' to comment:
    //
    //             ErrToMessageBox: 1        # 1/0, on/off, yes/no, true/false
    //             LogFile: errorLog.txt     # relative to THIS file's folder
    //
    //       : THE DEFAULT IS UNCHANGED BEHAVIOUR.  ErrToMessageBox defaults to
    //         ON, and with no configuration file present nothing about this
    //         library's output differs from before the file was understood.
    //       : ErrToMessageBox CANNOT PUT A DIALOG BACK where one would hang the
    //         process.  Setting it to 0 is a third way of saying what
    //         P2PMSG_NO_UI=1 and ForceTextOutput(true) already say; setting it
    //         to 1 merely declines to say it.  The viewability test in
    //         P2PeventUseTextOutput() still refuses the dialog for a service,
    //         because that refusal is not a preference - refer TextOutputPolicy.
    //       : LogFile names the destination for text once the library has
    //         decided on text, and loses to an installed SetTextSink().  It has
    //         to: P2PeerService installs the event log sink, and a stray
    //         configuration file must not silently divert a service's
    //         diagnostics into a file under %SystemRoot%\System32 - which is
    //         where a service's working directory points.
    //       : A RELATIVE LogFile resolves against the folder holding the
    //         configuration file, never the working directory, for the same
    //         reason.
    //       : LoadConfigFile(0) performs the search; a path loads that file and
    //         replaces whatever was loaded before, which is how a test drives
    //         this without depending on what happens to sit beside the runner.
    public:
      static bool
        LoadConfigFile ( LPCWSTR lpszPath = nullptr );
      static LPCWSTR
        ConfigFilePath ( );
      static LPCWSTR
        LogFilePath ( );
      static LPCWSTR
        SetLogFile ( LPCWSTR lpszPath );
      static LPCWSTR
        ConfigDiagnostic ( );

    // Troubleshooting
    public:
      virtual P2Pevent*
        Assert ( ) { ASSERT(0); return this; }
      virtual void
        AssertValid ( ) const;
      P2Pevent*
        Break ( ) const;
      P2Pevent*
        SetTInst ( ) volatile;
      static P2Pevent*
        GetTInst ( );

    // Properties
    public:
      P2Pevent_e
        GetClass ( );
      LPCTSTR
        GetClassText ( );
      LPCTSTR
        GetService ( );
      LPCTSTR
        GetModule ( );
      BOOL
        EmptyModule ( );
      const CString&
        GetMessage ( );
      const CString&
        GetAdvice ( );
      LPCTSTR
        GetGroup ( );
      HRESULT
        GetHRESULT ( );
      LPCTSTR
        GetHRESULText ( );
      time_t
        GetTime ( );
      DWORD
        GetEvent ( );

    // Attributes
    private:
        bool            m_bDisplayed{false};

        CString         m_csDescription;
        CString         m_csAdvice;
};
typedef P2PSafePtr<P2Pevent> P2PeventSP;

//
//  P2Pevent object instanciation shortcuts
#define EVERR  P2Pevent::MakeEvent(P2Pevent_ERROR) 
// P2Pevent_WARNING, not P2Pevent_WARN - the enum above has always spelled it
// in full. The macro had no users, so the typo never reached a compiler until
// P2PeerCon's login authentication diagnostic became the first one.
#define EVWRN (P2Pevent::MakeEvent(P2Pevent_WARNING))
#define EVINF (P2Pevent::MakeEvent(P2Pevent_INFO))
#define EVDBG (P2Pevent::MakeEvent(P2Pevent_DEBUG))
#define EVTRC (P2Pevent::MakeEvent(P2Pevent_TRACE))
#define EVLOG (P2Pevent::MakeEvent(P2Pevent_LOG))
#define EVRPT (P2Pevent::MakeEvent(P2Pevent_REPORT))

#define EVAPP (P2Pevent::MakeEvent(P2Pevent_USER0))

#define T__FUNCTION__ _T(__FUNCTION__)
#define MODULE  Module_(T__FUNCTION__)
#define Message_T(str) Message(_T(str))
#define Advice_T(str)  Advice(_T(str))

//
//  Diagnostic, DiagnosticA and TargetCorelogA were removed here (item 17).
//  All three formatted into a CString and then passed the RESULT as a format
//  string, so any '%' in the formatted text was re-interpreted -- the same
//  format-string sink M3 fixed in the live diagnostics. They had no call site
//  anywhere in this repository, and TargetCorelogA was unusable in Release
//  besides: its #else branch defined DiagnosticA rather than itself, so the
//  name simply did not exist outside a Debug build. Use EVLOG / the P2Pevent
//  chain (Module/Message/Advice above) instead; it takes an already-formatted
//  string and never re-scans it.
//

//
//  Conditional diagnostic
//  NOTES: Dynamic diagnostic that only presents in DEBUG mode
#ifdef _DEBUG
#define Diagnostic(strFormat, ...) \
{ CString strVargs; strVargs.Format(strFormat,__VA_ARGS__); \
COleDateTime _dt_ = COleDateTime::GetCurrentTime(); \
TCHAR szDiagnostic[4096]; \
int cSize = swprintf_s ( &szDiagnostic[0], ARRAYSIZE(szDiagnostic)-1 \
,L"[%s] %s\n",(LPCTSTR)_dt_.Format(L"%H-%M-%S"),(LPCTSTR)strVargs); \
szDiagnostic[cSize]=0; \
CString strDiagnostic = szDiagnostic; \
fwprintf ( stdout, (LPCTSTR)CString(strDiagnostic) ); }
#else
#define Diagnostic(...) ((void*)0)
#endif
//
//  Conditional diagnostic
//  NOTES: Dynamic diagnostic that only presents in DEBUG mode
#ifdef _DEBUG
#define DiagnosticA(strFormatA, ...) \
{ CStringA strVargsA; strVargsA.Format(strFormatA,__VA_ARGS__); \
CString strVargs(strVargsA); \
COleDateTime _dt_ = COleDateTime::GetCurrentTime(); \
TCHAR szDiagnostic[4096]; \
int cSize = swprintf_s ( &szDiagnostic[0], ARRAYSIZE(szDiagnostic)-1 \
,L"[%s] %s\n",(LPCTSTR)_dt_.Format(L"%H-%M-%S"),(LPCTSTR)strVargs); \
szDiagnostic[cSize]=0; \
CString strDiagnostic = szDiagnostic; \
CStringA strDiagnosticA(strDiagnostic); \
fprintf ( stdout, (LPCSTR)strDiagnosticA ); }
#else
#define DiagnosticA(...) ((void*)0)
#endif
//
//  MsgexceptionLog diagnostics
//  NOTES: Dynamic diagnostic that only presents in DEBUG mode
#ifdef _DEBUG
#define P2PmsgcorelogA(lpszFormatA, ...) \
{ CString strVargsA; \
CString strFormatA=lpszFormatA;if(strFormatA.GetLength()>0)strVargsA.Format(lpszFormatA,__VA_ARGS__); \
EVLOG->Module(__FUNCTION__)->Message(strVargsA)->Cancel(); }
#else
#define P2PmsgcorelogA(...) ((void*)0)
#endif

//
//  Catch block interceptions
//  NOTES: Utilised in catch handlers to translate exception object
//         into P2Pevent object.  Usually extracts Message() and
//         HResult() from message/environment
//       : Example
//         catch ( CException *pEx )
//         {
//           P2Pevent *pEVT = P2Pevent_catch ( pEx, true )
//                          -> Module ( __FUNCTION__ )
//                          -> etc;
//         }
Msgcore_EXT P2Pevent*
P2Pevent_catch ( CException *pEx, bool bDeleteEx = true );

#define catch_pP2Pevent_Cancel \
    catch ( P2Pevent *pEVT ) \
    { \
      ASSERT(pEVT);if(pEVT)pEVT->Cancel(); \
    }
#define catch_pP2Pevent_SetLast \
    catch ( P2Pevent *pEVT ) \
    { \
      ASSERT(pEVT);if(pEVT)pEVT->SetLast(); \
    } 

#define catch_ALL_Cancel \
    catch ( ... ) \
    { \
      EVERR-> MODULE \
           -> Message ( _T("Unknown exception") ) \
           -> Cancel(); \
    }
#define catch_ALL_SetLast \
    catch ( ... ) \
    { \
      EVERR-> MODULE \
           -> Message ( _T("Unknown exception") ) \
           -> SetLast(); \
    }

#define catch_pCException_Cancel \
    catch ( CException *pEx ) \
    { \
      P2Pevent *pEVT = P2Pevent_catch(pEx); \
      pEVT -> MODULE \
           -> Cancel(); \
    } 
#define catch_pCException_SetLast \
    catch ( CException *pEx ) \
    { \
      P2Pevent *pEVT = P2Pevent_catch(pEx); \
      pEVT -> MODULE \
           -> SetLast(); \
    } \

//
//  P2Pevent content helpers
//  NOTES: Used to summarise content of P2Pevent objects
#define EVT_EmptyGroup(pEVT) (_tcslen(pEVT->GetGroup())>0)
#define EVT_EmptyModule(pEVT) (_tcslen(pEVT->GetModule())>0)

//
//  SetFParam ( ) helpers
//  NOTES: A[ppend] FP[aram's] work for the standard 'C' types
//         supported by the P2PmsgData() constructor set
//       : Add your own variants for 3rd Party structs and objects
//         with P2PeerMsg and P2PeerCon providing a suitable
//         patterns to follow
//       : Usage
//         EVERR->Module(__FUNCTION__)
//              ->AFP(nPumpID)->AFPmsg(pMsg)->AFPyourObj(pObj)
//              ->Message("This is an event associated message")
//       : Such parameters are appended beneath the P2Pevent function
//         node.  It should be assumed that any appended parameters
//         will have global exposure
//       : L#arg IS NOT A WIDE LITERAL, which is why the name arrives through a
//         two-step widen instead. `#` and `##` are separate operations, so
//         writing L immediately before #arg leaves TWO tokens -- the identifier
//         L and a narrow "name" -- which MSVC's preprocessor silently glues back
//         into one wide literal and GCC reports as an undeclared `L` (13 of them,
//         plus two expected-')' cascades, in the CMake/Linux build). AFP__widen
//         takes the ALREADY-STRINGIFIED name, so the paste has a real string
//         literal to work on and both preprocessors agree.
#define AFP__widen(str) L##str
#define AFP(arg) SetFParam(AFP__widen(#arg),P3PmsgData(arg))
#define AFP_f(arg,fmt) SetFParam(AFP__widen(#arg),P3PmsgData(arg).c_printf(fmt))

//
//  P2Pevent callback definitions (default set)
//  NOTES: Used to manage registered P2PeventSink notifications
//       : Call backs will be performed for each filtered event, 
//         other options include thread and window notifications
//       : Remember to cancel the notification prior to allowing
//         the implementation code to go out of scope
//typedef void (WINAPI *P2PeventCBFnc)
//         ( P2PeventSinkID  nSinkID,    // Sink identification code
//           DWORD_PTR      dwCBKey,     // User definied callback key
//           const P2Pevent& oEvent );   // Notification event

Msgcore_EXT void WINAPI                // Posts P2Pevent to P2PeventSink
P2PeventCB_HWND ( P2PeventSinkID nSinkID, DWORD_PTR dwCBKey
                , const P2Pevent& oEvent ); 
extern Msgcore_EXT
UINT WM_P2PeventNOTN;                  // Notification identification

Msgcore_EXT void WINAPI                // Performs modal display
P2PeventCB_Modal( P2PeventSinkID nSinkID, DWORD_PTR dwCBKey
                , const P2Pevent& oEvent ); 
Msgcore_EXT void WINAPI                // Posts P2Pevent to HWND
P2PeventPost_HWND ( const P2Pevent& oP2Pevent ); 

//
//  P2Pevent Display() helpers
Msgcore_EXT int
P2PeventMBox ( P2Pevent *pP2Pevent, UINT nType = MB_OK );

//
//  P2Pevent sink management 
//  NOTES: Manage the creation, operation and life cycle of
//         P2PeventSink's.  Clients may register for P2Pevent
//         notifications
//       : Thread isolation of P2Pevent's
//Msgcore_EXT BOOL
//StartupP2Pevent ( );
//Msgcore_EXT void
//CleanupP2Pevent ( );
//Msgcore_EXT BOOL
//CloseP2PeventSink ( P2PeventSinkID nSinkID );
//Msgcore_EXT P2PeventSinkID
//CreateP2PeventSink ( DWORD_PTR dwCBKey
//                   , P2PeventCBFnc pCBFnc
//                   , bool bReg4all = true );
//Msgcore_EXT BOOL
//EnumP2PeventSink ( P2PeventSinkID& nSinkID );
Msgcore_EXT P2Pevent*
GetP2Pevent ( );
Msgcore_EXT P2Pevent*
IsolateP2Pevent ( );
//Msgcore_EXT int
//PerformP2PeventNotn ( P2Pevent *pEvent = 0 );

//const DWORD P2Pevent_AddMask    = 1;
//const DWORD P2Pevent_RemMask    = 2;
//const DWORD P2Pevent_GetMask    = 3;
//const DWORD P2Pevent_SetMask    = 4;
//const DWORD P2Pevent_AddContext = 5;
//const DWORD P2Pevent_RemContext = 6;
//const DWORD P2Pevent_RstContext = 7;
//Msgcore_EXT DWORD
//RegP2PeventCmd ( P2PeventSinkID nSinkID
//               , DWORD dwCmd
//               , DWORD dwCmdArg );
//Msgcore_EXT DWORD
//IsP2PeventReg ( DWORD dwP2PeventMask );
//#define IsEVDBG (IsP2PeventReg(1<<P2Pevent_DEBUG)?true:false)
//#define IsEVTRC (IsP2PeventReg(1<<P2Pevent_TRACE)?true:false)
//#define IsEVLOG (IsP2PeventReg(1<<P2Pevent_LOG)?true:false)

Msgcore_EXT void
SetP2Pevent ( P2Pevent *pEvent = 0 );
Msgcore_EXT void
ThrowP2Pevent ( );

///////////////////////////////////////
//  Debug definitions
#ifdef _DEBUG 
#define _bDEBUG FALSE
#else 
#define _bDEBUG FALSE
#endif;
static BOOL s_bDebugExtraAxis = FALSE;
static BOOL s_PYCB_CreateChart = _bDEBUG;
static BOOL s_bDebugSizeMove = FALSE;
static BOOL s_bDebug_PYCBActive = _bDEBUG;
static BOOL s_bPYCB_ModelTrade = _bDEBUG;
static BOOL s_bDebugModelCursor = TRUE;