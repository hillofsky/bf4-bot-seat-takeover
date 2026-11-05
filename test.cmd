@echo off
setlocal
pushd "%~dp0core"
call build.cmd
if errorlevel 1 (popd & exit /b 1)
popd
call "%~dp0native-adapter\build.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-adapter\build_bridge.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-adapter\build_identity.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-adapter\build_capacity.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-adapter\build_deployment_proof.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-adapter\abi\build_test.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-client\build_tests.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-client\build_overlap_tests.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-client\abi\build_test.cmd"
if errorlevel 1 exit /b 1
echo All regression suites passed.
exit /b 0
