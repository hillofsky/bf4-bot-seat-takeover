@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist build-addon mkdir build-addon
pushd build-addon
cl.exe /nologo /O2 /c /W3 /D MH_STATIC /I"..\..\vendor\minhook\include" ..\..\vendor\minhook\src\buffer.c ..\..\vendor\minhook\src\hook.c ..\..\vendor\minhook\src\trampoline.c ..\..\vendor\minhook\src\hde\hde64.c
if errorlevel 1 (popd & popd & exit /b 1)
ml64.exe /nologo /c /Fo boat_observer.obj ..\abi\boat_observer.asm
if errorlevel 1 (popd & popd & exit /b 1)
ml64.exe /nologo /c /Fo armor_read.obj ..\abi\armor_read.asm
if errorlevel 1 (popd & popd & exit /b 1)
cl.exe /nologo /O2 /std:c++17 /EHsc /W4 /WX /D MH_STATIC /LD ..\seat_addon.cpp ..\driver_policy.cpp ..\deployment_proof.cpp ..\r63_seat_reader.cpp ..\server_image_guard.cpp buffer.obj hook.obj trampoline.obj hde64.obj boat_observer.obj armor_read.obj bcrypt.lib /Fe:BF4SeatTakeover.dll
set addon_result=%errorlevel%
if "%addon_result%"=="0" if not exist BF4SeatTakeover.ini copy /y "..\BF4SeatTakeover.ini" BF4SeatTakeover.ini >nul
popd
popd
exit /b %addon_result%
