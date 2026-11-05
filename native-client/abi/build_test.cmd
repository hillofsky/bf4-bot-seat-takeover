@echo off
setlocal
cd /d "%~dp0"
call "%~dp0..\..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
ml64 /nologo /c /Zi /Fo full_observer.obj full_observer.asm
if errorlevel 1 exit /b 1
ml64 /nologo /c /Zi /Fo probe.obj probe.asm
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /W4 /WX /Zi test.cpp full_observer.obj probe.obj /Fe:abi_test.exe /link /incremental:no
if errorlevel 1 exit /b 1
abi_test.exe
exit /b %errorlevel%
