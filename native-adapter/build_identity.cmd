@echo off
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl.exe /nologo /std:c++17 /EHsc /W4 /WX /D_CRT_SECURE_NO_WARNINGS bot_identity_tests.cpp /Fe:bot_identity_tests.exe
if errorlevel 1 (popd & exit /b 1)
bot_identity_tests.exe
set identity_result=%errorlevel%
popd
exit /b %identity_result%
