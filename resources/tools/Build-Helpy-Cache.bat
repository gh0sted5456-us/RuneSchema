@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-helpy-cache.ps1" -ZipPath "%~1"
set "RSCacheExit=%ERRORLEVEL%"
echo.
pause
exit /b %RSCacheExit%
