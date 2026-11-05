@echo off
setlocal
call "%~dp0native-adapter\build_addon.cmd"
if errorlevel 1 exit /b 1
call "%~dp0native-client\build.cmd"
if errorlevel 1 exit /b 1
echo Server: native-adapter\build-addon\BF4SeatTakeover.dll
echo Client: native-client\build\BF4DeployClient.dll - install as BF4DeployAll.dll
exit /b 0
