@echo off
setlocal

rem try vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath"`) do set "VSINSTALL=%%i"
)

rem fallback: look in default location
if "%VSINSTALL%"=="" (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community" set "VSINSTALL=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
)

if "%VSINSTALL%"=="" (
  echo ERROR: Cannot find Visual Studio installation. Please install or set VSINSTALL.
  exit /b 1
)

rem Prefer VsDevCmd; pass host/target architecture flags if desired
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -host_arch=x64 -arch=x86
if errorlevel 1 (
  echo ERROR: VsDevCmd failed.
  exit /b 1
)

rem finally invoke real cmake with all forwarded args
"cmake" %*
endlocal
