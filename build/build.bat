@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
set "BUILD_SCRIPT=%SCRIPT_DIR%build.ps1"
set "LOG_DIR=%SCRIPT_DIR%logs"
set "LAUNCH_LOG=%LOG_DIR%\launcher-latest.log"

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1

> "%LAUNCH_LOG%" (
    echo ============================================================
    echo RuneSchema launcher diagnostic log
    echo Started: %DATE% %TIME%
    echo Script:  %BUILD_SCRIPT%
    echo Args:    %*
    echo ============================================================
    echo.
)

where powershell.exe >nul 2>&1
if errorlevel 1 (
    >> "%LAUNCH_LOG%" echo ERROR: powershell.exe was not found on PATH.
    echo ERROR: powershell.exe was not found on PATH.
    set "BUILD_EXIT=9009"
    goto :failed
)

if not exist "%BUILD_SCRIPT%" (
    >> "%LAUNCH_LOG%" echo ERROR: build.ps1 was not found: %BUILD_SCRIPT%
    echo ERROR: build.ps1 was not found:
    echo   %BUILD_SCRIPT%
    set "BUILD_EXIT=2"
    goto :failed
)

set "RUNESCHEMA_BUILD_SCRIPT=%BUILD_SCRIPT%"
set "RUNESCHEMA_LAUNCH_LOG=%LAUNCH_LOG%"
set "RUNESCHEMA_OUTER_TRANSCRIPT=1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$log=$env:RUNESCHEMA_LAUNCH_LOG;" ^
  "try {" ^
  "  Start-Transcript -LiteralPath $log -Append | Out-Null;" ^
  "  Write-Host ('Launching: ' + $env:RUNESCHEMA_BUILD_SCRIPT);" ^
  "  & $env:RUNESCHEMA_BUILD_SCRIPT %*;" ^
  "  if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) { exit $LASTEXITCODE }" ^
  "} catch {" ^
  "  Write-Host '';" ^
  "  Write-Host 'FATAL BUILDER ERROR' -ForegroundColor Red;" ^
  "  Write-Host $_.Exception.ToString() -ForegroundColor Red;" ^
  "  try { Add-Content -LiteralPath $log -Value ('FATAL BUILDER ERROR: ' + $_.Exception.ToString()) } catch {};" ^
  "  exit 1" ^
  "} finally {" ^
  "  try { Stop-Transcript | Out-Null } catch {}" ^
  "}"

set "BUILD_EXIT=%ERRORLEVEL%"

if not "%BUILD_EXIT%"=="0" goto :failed

>> "%LAUNCH_LOG%" echo.
>> "%LAUNCH_LOG%" echo Builder completed successfully with exit code 0.
exit /b 0

:failed
>> "%LAUNCH_LOG%" echo.
>> "%LAUNCH_LOG%" echo Builder failed with exit code %BUILD_EXIT%.
echo.
echo RuneSchema builder failed with exit code %BUILD_EXIT%.
echo Diagnostic log:
echo   %LAUNCH_LOG%
echo.
if not "%RUNESCHEMA_ROOT_LAUNCHER%"=="1" pause
exit /b %BUILD_EXIT%
