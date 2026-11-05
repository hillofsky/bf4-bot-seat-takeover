@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl.exe /nologo /std:c++17 /EHsc /W4 /WX capacity_tests.cpp /Fe:capacity_tests.exe
if errorlevel 1 (popd & exit /b 1)
capacity_tests.exe
set capacity_result=%errorlevel%
popd
exit /b %capacity_result%
