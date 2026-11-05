@echo off
where cl.exe >nul 2>nul
if not errorlevel 1 exit /b 0
set "BF4_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%BF4_VSWHERE%" (
    echo Install Visual Studio C++ x64 build tools or use an x64 Native Tools Command Prompt.
    exit /b 1
)
set "BF4_VS_PATH="
for /f "usebackq tokens=*" %%i in (`"%BF4_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BF4_VS_PATH=%%i"
if not defined BF4_VS_PATH exit /b 1
call "%BF4_VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
