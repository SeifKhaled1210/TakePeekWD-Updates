@echo off
REM ============================================================================
REM TakePeek 2.0 WD — Kernel Driver Build Script
REM Requires: Windows Driver Kit (WDK) installed alongside Visual Studio
REM Usage:    Run from VS Developer Command Prompt (x64 Native Tools)
REM ============================================================================

setlocal

REM --- Locate WDK ---
set "WDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
set "WDK_VER="

REM Auto-detect latest WDK version
for /d %%d in ("%WDK_ROOT%\lib\*") do set "WDK_VER=%%~nxd"

if "%WDK_VER%"=="" (
    echo ERROR: WDK not found. Install from https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
    exit /b 1
)

echo Using WDK version: %WDK_VER%

set "WDK_INC=%WDK_ROOT%\Include\%WDK_VER%"
set "WDK_LIB=%WDK_ROOT%\Lib\%WDK_VER%"

REM --- Compiler flags ---
set CL_FLAGS=/kernel /GS- /Gz /D_AMD64_ /DAMD64 /W3 /O2 /c
set CL_FLAGS=%CL_FLAGS% /I"%WDK_INC%\km" /I"%WDK_INC%\shared" /I"%WDK_INC%\um"

REM --- Link flags ---
set LINK_FLAGS=/DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry
set LINK_FLAGS=%LINK_FLAGS% /LIBPATH:"%WDK_LIB%\km\x64"
set LINK_LIBS=ntoskrnl.lib hal.lib wdmsec.lib

REM --- Build ---
if not exist "..\build\driver" mkdir "..\build\driver"

echo Compiling driver.c...
cl.exe %CL_FLAGS% src\driver.c /Fo"..\build\driver\driver.obj"
if errorlevel 1 (
    echo Compile FAILED
    exit /b 1
)

echo Linking TakePeek_WD_Driver.sys...
link.exe %LINK_FLAGS% /OUT:"..\build\driver\TakePeek_WD_Driver.sys" "..\build\driver\driver.obj" %LINK_LIBS%
if errorlevel 1 (
    echo Link FAILED
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL: ..\build\driver\TakePeek_WD_Driver.sys
echo Map with kdmapper: kdmapper.exe TakePeek_WD_Driver.sys
echo.

endlocal
