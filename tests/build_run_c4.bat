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
REM  Build and run the C4 IOMAGE-load rejection test (x64, Debug).
REM
REM      build_run_c4.bat [static^|dll]
REM
REM  With no argument it picks whichever link mode the tree can satisfy.
REM
REM  NOTES: Everything this script produces goes to tests\out\. .gitignore's
REM         "out/" pattern carries no leading slash, so it already matches an
REM         out directory at any depth - no new ignore rule is needed. The
REM         earlier version built into tests\ itself and left c4test.exe,
REM         *.obj, a copied Msgcore.dll and the *.dat fixtures behind, none of
REM         them ignored by anything.
REM       : Two link modes, because the import library and the static archive
REM         land in different places:
REM           dll    - links the import library the DLL configurations write to
REM                    %WDMSCS_LIB%\<Platform>\<Configuration>. Copies Msgcore.dll
REM                    next to the test so it loads.
REM           static - links out\x64\DebugLib\Msgcore.lib. Needs Msgcore_STATIC,
REM                    and the four extra libraries that LINKAGE.md lists: an
REM                    archive carries no import records, so the DLL's own
REM                    dependencies become the consumer's.
REM       : dll mode used to be available only inside the MSCS tree, because
REM         WDMSCS_LIB was defaulted there and nowhere else. Directory.Build.props
REM         is now part of this repository and defaults it to the repo's own lib\,
REM         so both modes work in a standalone clone. The probe below therefore
REM         tries three locations, most specific first: an explicit WDMSCS_LIB,
REM         this repository's lib\, then the parent tree's.
REM       : MsgcoreMFC.lib is NOT a dependency and never was. The old command
REM         line linked it and copied MsgcoreMFC.dll; dumpbin confirms
REM         Msgcore.dll imports only PROPSYS, mfc140ud and the CRT/Win32 set,
REM         and the test itself includes nothing from that project.

setlocal
set "HERE=%~dp0"
set "REPO=%~dp0.."
set "OUTDIR=%~dp0out"

REM ---- Visual Studio build environment --------------------------------------
REM  Located rather than hard-coded, so a Professional, Enterprise or Build
REM  Tools install works as well as Community.
if defined VCINSTALLDIR goto :have_vc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :no_vs
REM  Both streams to nul: vcvars64.bat shells out to its own copy of vswhere and
REM  writes a harmless "not recognized" line to stderr on some installs. Its
REM  exit code is still checked, so a real failure is not swallowed.
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 goto :no_vs
:have_vc

REM ---- Locate an import library ---------------------------------------------
REM  Three candidates, most specific first. WDMSCS_LIB names the ROOT; the
REM  per-platform, per-configuration subdirectory is appended by
REM  Directory.Build.props, so probe that and not the root itself.
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

:mode_dll
set "MODE=dll"
if not defined DLLLIB goto :nothing_built
set "LIBDIR=%DLLLIB%"
set "EXTRADEF="
set "EXTRALIB="
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
   /I"%REPO%" /Fo"%OUTDIR%\\" /Fd"%OUTDIR%\c4test.pdb" ^
   "%HERE%C4LoadTest.cpp" ^
   /link /LIBPATH:"%LIBDIR%" Msgcore.lib %EXTRALIB% /OUT:"%OUTDIR%\c4test.exe"
if errorlevel 1 goto :fail_build

if not defined NEEDDLL goto :run
if not exist "%REPO%\out\x64\Debug\Msgcore.dll" goto :no_dll
copy /Y "%REPO%\out\x64\Debug\Msgcore.dll" "%OUTDIR%" >nul

REM ---- Run ------------------------------------------------------------------
REM  From inside tests\out so the *.dat fixtures the test writes land there too.
:run
echo.
echo === RUN ===
pushd "%OUTDIR%"
"%OUTDIR%\c4test.exe"
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
echo ERROR: the test failed to compile or link.
endlocal & exit /b 1
