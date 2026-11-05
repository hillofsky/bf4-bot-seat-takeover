@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
cl.exe /nologo /std:c++17 /EHsc /W4 /WX takeover.cpp tests.cpp /Fe:takeover_tests.exe
if errorlevel 1 exit /b 1
takeover_tests.exe
