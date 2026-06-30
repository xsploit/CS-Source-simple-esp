@echo off
echo START > "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp\_marker.txt"
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "installPath="
for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools -property installationPath`) do set "installPath=%%i"
call "%installPath%\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul 2>&1
cd /d "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp"
if exist build rmdir /s /q build
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 .. > "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp\_buildlog.txt" 2>&1
echo CONFIGURE_EXIT_%errorlevel% >> "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp\_buildlog.txt"
cmake --build . --config Release >> "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp\_buildlog.txt" 2>&1
echo BUILD_EXIT_%errorlevel% >> "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp\_buildlog.txt"
echo DONE >> "C:\Users\SUBSECT\Documents\Codex\2026-06-26\623efc15055e4fb08416037b89f10554-34kiaukr9tphy3at\_fresp\_marker.txt"
