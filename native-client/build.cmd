@echo off
setlocal
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist build mkdir build
pushd build
cl.exe /nologo /O2 /c /W3 /D MH_STATIC /I"..\..\vendor\minhook\include" ..\..\vendor\minhook\src\buffer.c ..\..\vendor\minhook\src\hook.c ..\..\vendor\minhook\src\trampoline.c ..\..\vendor\minhook\src\hde\hde64.c
if errorlevel 1 (popd & popd & exit /b 1)
ml64.exe /nologo /c /Fo full_observer.obj ..\abi\full_observer.asm
if errorlevel 1 (popd & popd & exit /b 1)
cl.exe /nologo /O2 /std:c++17 /EHsc /W4 /WX /D MH_STATIC /LD ..\client_addon.cpp buffer.obj hook.obj trampoline.obj hde64.obj full_observer.obj bcrypt.lib /Fe:BF4DeployClient.dll
set result=%errorlevel%
popd
popd
exit /b %result%
