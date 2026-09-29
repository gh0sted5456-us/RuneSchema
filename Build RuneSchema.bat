@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "RUNESCHEMA_ROOT_LAUNCHER=1"

echo.
echo ============================================================
echo                    RuneSchema Builder
echo ============================================================
echo.

call "%~dp0build\build.bat" %*
set "BUILD_EXIT=%ERRORLEVEL%"

if not "%BUILD_EXIT%"=="0" (
    echo.
    echo ============================================================
    echo RuneSchema build FAILED with exit code %BUILD_EXIT%.
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
    echo RuneSchema build completed successfully.
)

exit /b %BUILD_EXIT%
