@echo off
setlocal
call "%~dp0..\scripts\init-msvc.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
cl /nologo /O2 /std:c++17 /EHsc /W4 /WX deployment_proof.cpp deployment_proof_tests.cpp /Fe:deployment_proof_tests.exe
if errorlevel 1 (popd & exit /b 1)
deployment_proof_tests.exe
set result=%errorlevel%
popd
exit /b %result%
