@echo off
setlocal enabledelayedexpansion

echo [INFO] Searching for Visual Studio...
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!vswhere!" set "vswhere=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "!vswhere!" (
    for /f "usebackq tokens=*" %%i in (`"!vswhere!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools -property installationPath`) do (
        set "installPath=%%i"
    )
)

if exist "!installPath!" (
    echo [INFO] Found: !installPath!
    if exist "!installPath!\Common7\Tools\VsDevCmd.bat" (
        call "!installPath!\Common7\Tools\VsDevCmd.bat" -arch=x64
    )
)

echo.
echo [INFO] Building Project (Educational ESP)...

:: Professional practice: Don't delete the build folder unless explicitly requested.
:: This enables "Incremental Builds" (way faster).
if not exist build mkdir build
cd build

:: Configure
cmake -G "Visual Studio 17 2022" -A x64 ..
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %errorlevel%
)

:: Build
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b %errorlevel%
)

echo.
echo [SUCCESS] Build Complete! 
echo [INFO] Binary: build/Release/CSS_Educational_ESP.exe
pause
