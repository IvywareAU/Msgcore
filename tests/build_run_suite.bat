@echo off
REM Copyright © 2026 Khrustal & Mann
REM              MELBOURNE, VICTORIA, AUSTRALIA, 3000
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM     http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
REM implied. See the License for the specific language governing
REM permissions and limitations under the License.
REM
REM  Build and run the Msgcore unit suites (x64, Debug).
REM
REM      build_run_suite.bat [static^|dll]
REM
REM  With no argument it picks whichever link mode the tree can satisfy.
REM
REM  the release-readiness register, item 15 (Stage E). This is build_run_c4.bat's twin and
REM  is deliberately a near-copy of it rather than a shared helper the two both
REM  call: the toolchain probe, the three-candidate import-library search and the
REM  two link modes are the parts that have actually broken before, and having
REM  them in one place did not stop that. What differs is only the source list
REM  and the output name. If a third runner ever appears, factor them then.
REM
REM  NOTES: Everything this script produces goes to tests\out\, as build_run_c4
REM         does, so .gitignore's depth-independent "out/" already covers it.
REM       : BOTH LINK MODES MATTER HERE for the same reason they matter for C4 --
REM         LINKAGE.md's rule is that an offset minted against one copy of the
REM         heap does not resolve against another, and a suite that only ever ran
REM         one way would not notice a regression that breaks the other.

setlocal
set "HERE=%~dp0"
set "REPO=%~dp0.."
set "OUTDIR=%~dp0out"

REM ---- Visual Studio build environment --------------------------------------
if defined VCINSTALLDIR goto :have_vc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :no_vs
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 goto :no_vs
:have_vc

REM ---- Locate an import library ---------------------------------------------
set "DLLLIB="
if defined WDMSCS_LIB if exist "%WDMSCS_LIB%\x64\Debug\Msgcore.lib" set "DLLLIB=%WDMSCS_LIB%\x64\Debug"
if not defined DLLLIB if exist "%REPO%\lib\x64\Debug\Msgcore.lib" set "DLLLIB=%REPO%\lib\x64\Debug"
if not defined DLLLIB if exist "%REPO%\..\lib\x64\Debug\Msgcore.lib" set "DLLLIB=%REPO%\..\lib\x64\Debug"

REM ---- Choose a link mode ---------------------------------------------------
if /i "%~1"=="static" goto :mode_static
if /i "%~1"=="dll"    goto :mode_dll
if defined DLLLIB                                 goto :mode_dll
if exist "%REPO%\out\x64\DebugLib\Msgcore.lib"    goto :mode_static
goto :nothing_built

REM  comsuppwd.lib is in BOTH modes, which is where this script departs from
REM  build_run_c4.bat rather than copying it. It is not a Msgcore dependency and
REM  it is not a consequence of the link mode: the VARIANT cases in
REM  MsgcoreSuite.cpp construct _bstr_t, whose Data_t ctor calls
REM  _com_issue_error, and that lives in the COM support library. C4LoadTest
REM  never touches _bstr_t, so its dll mode links without it and static mode only
REM  appeared to need it because it is in LINKAGE.md's archive list for other
REM  reasons. Getting this wrong fails as LNK2019 at link time, not at runtime.
:mode_dll
set "MODE=dll"
if not defined DLLLIB goto :nothing_built
set "LIBDIR=%DLLLIB%"
set "EXTRADEF="
set "EXTRALIB=comsuppwd.lib"
set "NEEDDLL=1"
goto :build

:mode_static
set "MODE=static"
set "LIBDIR=%REPO%\out\x64\DebugLib"
set "EXTRADEF=/DMsgcore_STATIC"
set "EXTRALIB=MsWsock.lib ws2_32.lib comsuppwd.lib Propsys.lib"
set "NEEDDLL="
goto :build

REM ---- Build ----------------------------------------------------------------
:build
if not exist "%LIBDIR%\Msgcore.lib" goto :nothing_built
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo === BUILD (%MODE%) ===
cl /nologo /EHsc /MDd /std:c++17 /Zc:wchar_t ^
   /D_DEBUG /D_CONSOLE /D_UNICODE /DUNICODE /D_AFXDLL /D_WIN32_WINNT=0x0603 %EXTRADEF% ^
   /I"%REPO%" /Fo"%OUTDIR%\\" /Fd"%OUTDIR%\suite.pdb" ^
   "%HERE%SuiteMain.cpp" "%HERE%TestFramework.cpp" ^
   "%HERE%MsgcoreSuite.cpp" "%HERE%MsgcoreCApiSuite.cpp" ^
   /link /LIBPATH:"%LIBDIR%" Msgcore.lib %EXTRALIB% /OUT:"%OUTDIR%\msgcore_suite.exe"
if errorlevel 1 goto :fail_build

if not defined NEEDDLL goto :run
if not exist "%REPO%\out\x64\Debug\Msgcore.dll" goto :no_dll
copy /Y "%REPO%\out\x64\Debug\Msgcore.dll" "%OUTDIR%" >nul

REM ---- Run ------------------------------------------------------------------
:run
echo.
echo === RUN ===
pushd "%OUTDIR%"
"%OUTDIR%\msgcore_suite.exe"
set "RC=%errorlevel%"
popd
endlocal & exit /b %RC%

REM ---- Diagnostics ----------------------------------------------------------
:no_vs
echo.
echo ERROR: no Visual Studio C++ toolset found.
echo        Run this from a "x64 Native Tools Command Prompt for VS 2022", or
echo        install the "Desktop development with C++" workload.
endlocal & exit /b 1

:nothing_built
echo.
echo ERROR: Msgcore has not been built for x64.
echo        Looked for an import library at
if defined WDMSCS_LIB echo            %WDMSCS_LIB%\x64\Debug\Msgcore.lib
echo            %REPO%\lib\x64\Debug\Msgcore.lib
echo            %REPO%\..\lib\x64\Debug\Msgcore.lib
echo        and a static archive at
echo            %REPO%\out\x64\DebugLib\Msgcore.lib
echo        and found none of them. Build Debug^|x64 (or DebugLib^|x64) first:
echo            msbuild "Msgcore(2022).vcxproj" /p:Configuration=Debug /p:Platform=x64
endlocal & exit /b 1

:no_dll
echo.
echo ERROR: %REPO%\out\x64\Debug\Msgcore.dll is missing.
echo        The import library exists but the DLL does not - build Debug^|x64.
endlocal & exit /b 1

:fail_build
echo.
echo ERROR: the suite failed to compile or link.
endlocal & exit /b 1
