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
//  Platform layer — Windows Service Control Manager (WinSvc.h) shim.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §6.2). P2PeerService
//  bridges a P2PeerHub to the Windows SCM. The plan splits it: the protocol-neutral console/
//  daemon path (Init/Run/RunConsoleModeAttachment/PostP2PeerHub) is the Linux entry, while
//  SCM install/uninstall/status is Windows-only.
//
//  Rather than #ifdef the SCM call sites throughout the 1026-line legacy .cpp, this shim
//  supplies the WinSvc TYPES + CONSTANTS (so every declaration parses) and the SCM FUNCTIONS
//  as Linux no-op stubs that report "not a service" (OpenSCManager/CreateService/... fail,
//  StartServiceCtrlDispatcher returns FALSE so control falls through to the console/daemon
//  Run path, SetServiceStatus/CloseServiceHandle succeed harmlessly). The legacy source is
//  thus unmodified; on Linux the Install/UnInstall/dispatcher methods simply become no-ops,
//  and the daemon runs via Run()/RunConsoleModeAttachment(). Optional sd_notify integration
//  can later hook SetServiceStatus's RUNNING transition.
//
//  On _WIN32 this forwards to the real <WinSvc.h>; the file is only ever on the Linux include
//  path (via win-compat/WinSvc.h), so the guard is belt-and-suspenders.
//
#pragma once

#if defined(_WIN32)
  #include <WinSvc.h>
#else
  #include "p2ptypes.h"    // DWORD/BOOL/LPTSTR/LPCTSTR/WINAPI/HANDLE

  // ---- Handle types ---------------------------------------------------------
  typedef void* SC_HANDLE;
  typedef void* SERVICE_STATUS_HANDLE;

  // ---- SERVICE_STATUS -------------------------------------------------------
  typedef struct _SERVICE_STATUS {
      DWORD dwServiceType;
      DWORD dwCurrentState;
      DWORD dwControlsAccepted;
      DWORD dwWin32ExitCode;
      DWORD dwServiceSpecificExitCode;
      DWORD dwCheckPoint;
      DWORD dwWaitHint;
  } SERVICE_STATUS, *LPSERVICE_STATUS;

  // ---- Callback function types ---------------------------------------------
  typedef void (WINAPI *LPSERVICE_MAIN_FUNCTIONW)(DWORD, LPWSTR*);
  typedef void (WINAPI *LPSERVICE_MAIN_FUNCTIONA)(DWORD, LPSTR*);
  typedef void (WINAPI *LPHANDLER_FUNCTION)(DWORD);
  #ifdef _UNICODE
    typedef LPSERVICE_MAIN_FUNCTIONW LPSERVICE_MAIN_FUNCTION;
  #else
    typedef LPSERVICE_MAIN_FUNCTIONA LPSERVICE_MAIN_FUNCTION;
  #endif

  // ---- SERVICE_TABLE_ENTRY --------------------------------------------------
  typedef struct _SERVICE_TABLE_ENTRYW {
      LPWSTR                   lpServiceName;
      LPSERVICE_MAIN_FUNCTIONW lpServiceProc;
  } SERVICE_TABLE_ENTRYW, *LPSERVICE_TABLE_ENTRYW;
  typedef struct _SERVICE_TABLE_ENTRYA {
      LPSTR                    lpServiceName;
      LPSERVICE_MAIN_FUNCTIONA lpServiceProc;
  } SERVICE_TABLE_ENTRYA, *LPSERVICE_TABLE_ENTRYA;
  #ifdef _UNICODE
    typedef SERVICE_TABLE_ENTRYW  SERVICE_TABLE_ENTRY;
    typedef LPSERVICE_TABLE_ENTRYW LPSERVICE_TABLE_ENTRY;
  #else
    typedef SERVICE_TABLE_ENTRYA  SERVICE_TABLE_ENTRY;
    typedef LPSERVICE_TABLE_ENTRYA LPSERVICE_TABLE_ENTRY;
  #endif

  // ---- Constants (real Win32 bit values) ------------------------------------
  #ifndef SERVICE_WIN32
    #define SERVICE_WIN32               0x00000030
    #define SERVICE_WIN32_OWN_PROCESS   0x00000010
    #define SERVICE_WIN32_SHARE_PROCESS 0x00000020
  #endif
  #ifndef SERVICE_STOPPED
    #define SERVICE_STOPPED             0x00000001
    #define SERVICE_START_PENDING       0x00000002
    #define SERVICE_STOP_PENDING        0x00000003
    #define SERVICE_RUNNING             0x00000004
    #define SERVICE_CONTINUE_PENDING    0x00000005
    #define SERVICE_PAUSE_PENDING       0x00000006
    #define SERVICE_PAUSED              0x00000007
  #endif
  #ifndef SERVICE_ACCEPT_STOP
    #define SERVICE_ACCEPT_STOP           0x00000001
    #define SERVICE_ACCEPT_PAUSE_CONTINUE 0x00000002
    #define SERVICE_ACCEPT_SHUTDOWN       0x00000004
  #endif
  #ifndef SERVICE_CONTROL_STOP
    #define SERVICE_CONTROL_STOP          0x00000001
    #define SERVICE_CONTROL_PAUSE         0x00000002
    #define SERVICE_CONTROL_CONTINUE      0x00000003
    #define SERVICE_CONTROL_INTERROGATE   0x00000004
    #define SERVICE_CONTROL_SHUTDOWN      0x00000005
  #endif
  #ifndef SERVICE_DEMAND_START
    #define SERVICE_BOOT_START            0x00000000
    #define SERVICE_SYSTEM_START          0x00000001
    #define SERVICE_AUTO_START            0x00000002
    #define SERVICE_DEMAND_START          0x00000003
    #define SERVICE_DISABLED              0x00000004
  #endif
  #ifndef SERVICE_ERROR_NORMAL
    #define SERVICE_ERROR_IGNORE          0x00000000
    #define SERVICE_ERROR_NORMAL          0x00000001
    #define SERVICE_ERROR_SEVERE          0x00000002
    #define SERVICE_ERROR_CRITICAL        0x00000003
  #endif
  #ifndef SERVICE_ALL_ACCESS
    #define SERVICE_QUERY_CONFIG          0x0001
    #define SERVICE_CHANGE_CONFIG         0x0002
    #define SERVICE_QUERY_STATUS          0x0004
    #define SERVICE_START                 0x0010
    #define SERVICE_STOP                  0x0020
    #define SERVICE_ALL_ACCESS            0xF01FF
  #endif
  #ifndef SC_MANAGER_ALL_ACCESS
    #define SC_MANAGER_CONNECT            0x0001
    #define SC_MANAGER_CREATE_SERVICE     0x0002
    #define SC_MANAGER_ALL_ACCESS         0xF003F
  #endif
  #ifndef SERVICE_NO_CHANGE
    #define SERVICE_NO_CHANGE             0xffffffff
  #endif

  // ---- SCM functions: Linux no-op stubs (SCM is Windows-only, §6.2) ----------
  //  The daemon/console path (Run/RunConsoleModeAttachment) is what actually runs on Linux;
  //  these let the legacy SCM methods compile and degrade to no-ops. StartServiceCtrlDispatcher
  //  returning FALSE (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) is exactly the Windows signal
  //  for "not launched by the SCM — run as a console app", which the caller already handles.
  #ifndef ERROR_FAILED_SERVICE_CONTROLLER_CONNECT
    #define ERROR_FAILED_SERVICE_CONTROLLER_CONNECT 1063
  #endif

  inline SC_HANDLE OpenSCManagerW(LPCWSTR, LPCWSTR, DWORD) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }
  inline SC_HANDLE OpenSCManagerA(LPCSTR,  LPCSTR,  DWORD) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }

  inline SC_HANDLE CreateServiceW(SC_HANDLE, LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD, DWORD,
                                  LPCWSTR, LPCWSTR, LPDWORD, LPCWSTR, LPCWSTR, LPCWSTR)
      { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }
  inline SC_HANDLE CreateServiceA(SC_HANDLE, LPCSTR, LPCSTR, DWORD, DWORD, DWORD, DWORD,
                                  LPCSTR, LPCSTR, LPDWORD, LPCSTR, LPCSTR, LPCSTR)
      { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }

  inline SC_HANDLE OpenServiceW(SC_HANDLE, LPCWSTR, DWORD) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }
  inline SC_HANDLE OpenServiceA(SC_HANDLE, LPCSTR,  DWORD) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }

  inline BOOL StartServiceW(SC_HANDLE, DWORD, LPCWSTR*) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
  inline BOOL StartServiceA(SC_HANDLE, DWORD, LPCSTR*)  { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }

  inline BOOL ControlService(SC_HANDLE, DWORD, LPSERVICE_STATUS) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
  inline BOOL DeleteService(SC_HANDLE)                           { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
  inline BOOL CloseServiceHandle(SC_HANDLE)                      { return TRUE; }
  inline BOOL QueryServiceStatus(SC_HANDLE, LPSERVICE_STATUS)    { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }

  inline BOOL StartServiceCtrlDispatcherW(const SERVICE_TABLE_ENTRYW*) { SetLastError(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT); return FALSE; }
  inline BOOL StartServiceCtrlDispatcherA(const SERVICE_TABLE_ENTRYA*) { SetLastError(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT); return FALSE; }

  inline SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerW(LPCWSTR, LPHANDLER_FUNCTION) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }
  inline SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerA(LPCSTR,  LPHANDLER_FUNCTION) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return nullptr; }

  inline BOOL SetServiceStatus(SERVICE_STATUS_HANDLE, LPSERVICE_STATUS) { return TRUE; }   // no-op success

  inline BOOL ChangeServiceConfigW(SC_HANDLE, DWORD, DWORD, DWORD, LPCWSTR, LPCWSTR, LPDWORD,
                                   LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
  inline BOOL ChangeServiceConfigA(SC_HANDLE, DWORD, DWORD, DWORD, LPCSTR, LPCSTR, LPDWORD,
                                   LPCSTR, LPCSTR, LPCSTR, LPCSTR) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }

  //  TCHAR-generic aliases (the code calls the unadorned names under _UNICODE/ANSI).
  #ifdef _UNICODE
    #define OpenSCManager             OpenSCManagerW
    #define CreateService             CreateServiceW
    #define OpenService               OpenServiceW
    #define StartService              StartServiceW
    #define StartServiceCtrlDispatcher StartServiceCtrlDispatcherW
    #define RegisterServiceCtrlHandler RegisterServiceCtrlHandlerW
    #define ChangeServiceConfig       ChangeServiceConfigW
  #else
    #define OpenSCManager             OpenSCManagerA
    #define CreateService             CreateServiceA
    #define OpenService               OpenServiceA
    #define StartService              StartServiceA
    #define StartServiceCtrlDispatcher StartServiceCtrlDispatcherA
    #define RegisterServiceCtrlHandler RegisterServiceCtrlHandlerA
    #define ChangeServiceConfig       ChangeServiceConfigA
  #endif
#endif  // _WIN32
