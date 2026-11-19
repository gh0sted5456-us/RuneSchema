@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "LAUNCHER_NAME=%~nx0"

set "RUNESCHEMA_ROOT_LAUNCHER=1"

echo.
echo ============================================================
echo                      Helpy Builder
echo   Builds Helpy only; RuneSchema.dll is left unchanged.
echo ============================================================
echo.

call "%~dp0build\build.bat" -PluginOnly %*
set "BUILD_EXIT=%ERRORLEVEL%"

if not "%BUILD_EXIT%"=="0" (
    echo.
    echo ============================================================
    echo Helpy build FAILED with exit code %BUILD_EXIT%.
    echo.
    echo Diagnostic log:
    echo   %~dp0build\logs\launcher-latest.log
    echo.
    echo The window will remain open so the error can be reviewed.
    echo ============================================================
    echo.
    pause
) else (
    echo.
    echo Helpy build completed successfully.
    echo Packages: %~dp0dist
    call :hold
)

exit /b %BUILD_EXIT%

:hold
rem Keep a double-clicked window open; return at once when run from a console or script.
echo %CMDCMDLINE% | find /i "%LAUNCHER_NAME%" >nul && pause
exit /b 0
