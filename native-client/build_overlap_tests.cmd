@echo off
setlocal
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist build mkdir build
cl.exe /nologo /O2 /std:c++17 /EHsc /W4 /WX overlap_tests.cpp /Fe:build\overlap_tests.exe /Fo:build\overlap_tests.obj
if errorlevel 1 (popd & exit /b 1)
build\overlap_tests.exe
set result=%errorlevel%
popd
exit /b %result%
