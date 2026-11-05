@echo off
setlocal
set "PSModulePath=%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy RemoteSigned -File "%~dp0CodexSeatTakeover\Control-AllSeatTakeover.ps1" -StartServer
set "seat_result=%errorlevel%"
if not "%seat_result%"=="0" pause
exit /b %seat_result%
