@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl.exe /nologo /std:c++17 /EHsc /W4 /WX /LD probe_dll.cpp r63_vehicle_actions.cpp r63_seat_reader.cpp server_image_guard.cpp bcrypt.lib /Fe:bf4_seat_probe.dll
set probe_result=%errorlevel%
popd
exit /b %probe_result%
