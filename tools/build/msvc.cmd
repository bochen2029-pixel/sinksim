@echo off
setlocal
rem sinksim build wrapper for Windows: enters the Visual Studio x64 developer environment, then runs CMake
rem presets from the repository root.
rem   msvc.cmd [configure|build|test|all|clean] [preset]      defaults: all msvc-release
rem (The status variable is deliberately not called RC: CMake reads an environment variable of that name as
rem the resource compiler.)
set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=all"
set "PRESET=%~2"
if "%PRESET%"=="" set "PRESET=msvc-release"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo msvc.cmd: vswhere.exe not found; install Visual Studio 2022 with the "Desktop development with C++" workload.
  exit /b 1
)
set "VSDIR="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if "%VSDIR%"=="" (
  echo msvc.cmd: no Visual Studio installation with the C++ toolset was found.
  exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
  echo msvc.cmd: vcvars64.bat failed.
  exit /b 1
)
cd /d "%~dp0..\.."

if /i not "%ACTION%"=="configure" if /i not "%ACTION%"=="build" if /i not "%ACTION%"=="test" if /i not "%ACTION%"=="clean" if /i not "%ACTION%"=="all" (
  echo msvc.cmd: unknown action "%ACTION%" ^(use configure, build, test, all or clean^)
  exit /b 2
)

set "STATUS=0"
if /i "%ACTION%"=="configure" ( cmake --preset %PRESET% || set "STATUS=1" )
if /i "%ACTION%"=="build" ( cmake --build --preset %PRESET% || set "STATUS=1" )
if /i "%ACTION%"=="test" ( ctest --preset %PRESET% --output-on-failure || set "STATUS=1" )
if /i "%ACTION%"=="clean" ( if exist "build\%PRESET%" rmdir /s /q "build\%PRESET%" )
if /i "%ACTION%"=="all" (
  cmake --preset %PRESET% && cmake --build --preset %PRESET% && ctest --preset %PRESET% --output-on-failure || set "STATUS=1"
)
exit /b %STATUS%
