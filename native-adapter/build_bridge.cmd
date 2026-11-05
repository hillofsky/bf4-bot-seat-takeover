@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I"..\core" r63_bindings.cpp native_adapter.cpp r63_vehicle_actions.cpp r63_seat_reader.cpp server_image_guard.cpp bridge_tests.cpp bcrypt.lib /Fe:bridge_tests.exe
if errorlevel 1 (popd & exit /b 1)
bridge_tests.exe
set bridge_result=%errorlevel%
popd
exit /b %bridge_result%
