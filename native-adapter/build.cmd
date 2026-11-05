@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I"..\core" native_adapter.cpp tests.cpp "..\core\takeover.cpp" /Fe:native_adapter_tests.exe
if errorlevel 1 (popd & exit /b 1)
native_adapter_tests.exe
if errorlevel 1 (popd & exit /b 1)
cl.exe /nologo /std:c++17 /EHsc /W4 /WX server_image_guard.cpp image_guard_tests.cpp bcrypt.lib /Fe:image_guard_tests.exe
if errorlevel 1 (popd & exit /b 1)
image_guard_tests.exe
if errorlevel 1 (popd & exit /b 1)
cl.exe /nologo /std:c++17 /EHsc /W4 /WX r63_seat_reader.cpp server_image_guard.cpp seat_reader_tests.cpp bcrypt.lib /Fe:seat_reader_tests.exe
if errorlevel 1 (popd & exit /b 1)
seat_reader_tests.exe
if errorlevel 1 (popd & exit /b 1)
cl.exe /nologo /std:c++17 /EHsc /W4 /WX r63_vehicle_actions.cpp r63_seat_reader.cpp server_image_guard.cpp vehicle_actions_tests.cpp bcrypt.lib /Fe:vehicle_actions_tests.exe
if errorlevel 1 (popd & exit /b 1)
vehicle_actions_tests.exe
set adapter_result=%errorlevel%
popd
exit /b %adapter_result%
