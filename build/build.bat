@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" %*
exit /b %ERRORLEVEL%
