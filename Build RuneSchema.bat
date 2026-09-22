@echo off
setlocal EnableExtensions EnableDelayedExpansion
title RuneSchema 0.7.0 Builder - Visual Studio 2026

REM ============================================================
REM RuneSchema 0.7.0 - normalized runtime contract
REM
REM Source:
REM   raw beside this BAT (the existing project, built in place)
REM
REM Build cache:
REM   build-cache beside the labeled source and clean folders
REM
REM Clean staged output:
REM   ..\0.7.0-clean\RuneSchema\dlls\main.dll
REM   (UPX level 9/best + LZMA, then optionally signed)
REM ============================================================

REM Use the folder containing this BAT; do not assemble or copy another tree.
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "SOURCE=%ROOT%\raw"
for %%I in ("%ROOT%\..\build-cache") do set "BUILD=%%~fI"
for %%I in ("%ROOT%\..\0.7.0-clean\RuneSchema") do set "RELEASE=%%~fI"
set "OUTPUT=%RELEASE%.staging-%RANDOM%-%RANDOM%"
set "DLLDIR=%OUTPUT%\dlls"
set "FINALDLL=%DLLDIR%\main.dll"
set "HELPYDIR=%OUTPUT%\plugins\RuneSchema.Helpy\dll"
set "HELPYDLL=%HELPYDIR%\RuneSchema.Helpy.dll"
set "UPXEXE="
REM Private signing configuration is intentionally kept outside source control.
REM Copy build-signing.local.cmd.example to build-signing.local.cmd and edit it.
if exist "%ROOT%\build-signing.local.cmd" call "%ROOT%\build-signing.local.cmd"
if not defined RUNESCHEMA_SIGN_TIMESTAMP set "RUNESCHEMA_SIGN_TIMESTAMP=https://timestamp.digicert.com"

set "GENERATOR=Visual Studio 18 2026"
set "CONFIG=Game__Shipping__Win64"
set "TARGET=RuneSchema"

REM Initialize the compiler environment when the builder is launched by
REM double-click instead of from a Visual Studio developer prompt.
set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
if exist "%VSDEVCMD%" call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul

echo.
echo ============================================================
echo RuneSchema Builder
echo ============================================================
echo Source : %SOURCE%
echo Build  : %BUILD%
echo Stage  : %FINALDLL%
echo Helpy  : %HELPYDLL%
echo Output : %RELEASE%\dlls\main.dll
echo Config : %CONFIG%
echo IDE    : Visual Studio 2026
echo ============================================================
echo.

REM ------------------------------------------------------------
REM Validate source tree
REM ------------------------------------------------------------
if not exist "%SOURCE%\CMakeLists.txt" (
    echo [ERROR] Could not find:
    echo         %SOURCE%\CMakeLists.txt
    echo.
    echo Expected project folder:
    echo   %SOURCE%
    goto :fail
)

REM ------------------------------------------------------------
REM Validate required command-line tools
REM ------------------------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake was not found in PATH.
    echo Install CMake and enable "Add CMake to PATH".
    goto :fail
)

where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git was not found in PATH.
    echo Git is required for RuneSchema's fetched dependencies.
    goto :fail
)

where cl.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] The Visual Studio C++ compiler environment is unavailable.
    echo Install the Desktop development with C++ workload for Visual Studio 2026.
    goto :fail
)

REM UE4SS pins public submodules with git@github.com URLs. Apply a process-local
REM transport rewrite so clean builds use HTTPS without changing the developer's
REM global Git configuration or requiring GitHub SSH keys.
set "GIT_CONFIG_COUNT=1"
set "GIT_CONFIG_KEY_0=url.https://github.com/.insteadOf"
set "GIT_CONFIG_VALUE_0=git@github.com:"

echo [Tools]
cmake --version | findstr /B /C:"cmake version"
git --version
echo.

REM Prefer the bundled, pinned UPX dependency. Sibling folders and PATH remain
REM supported for developer workspaces that intentionally manage tools centrally.
if exist "%ROOT%\tools\upx\upx.exe" set "UPXEXE=%ROOT%\tools\upx\upx.exe"
for %%U in (
    "%ROOT%\..\upx-5.2.1-win64\upx.exe"
    "%ROOT%\..\..\upx-5.2.1-win64\upx.exe"
) do if not defined UPXEXE if exist "%%~fU" set "UPXEXE=%%~fU"
if not defined UPXEXE for /f "delims=" %%U in ('where upx.exe 2^>nul') do if not defined UPXEXE set "UPXEXE=%%U"
if not defined UPXEXE (
    echo [ERROR] Required UPX 5.2.1 dependency was not found.
    echo Expected bundled tool: %ROOT%\tools\upx\upx.exe
    goto :fail
)
echo upx: %UPXEXE%
"%UPXEXE%" --version | findstr /B /C:"upx"
echo.

REM ------------------------------------------------------------
REM Verify THIS CMake actually knows the VS 2026 generator.
REM No Visual Studio pre-detection is performed here: CMake itself
REM is the authoritative detector and gives much better diagnostics.
REM ------------------------------------------------------------
cmake --help | findstr /C:"Visual Studio 18 2026" >nul
if errorlevel 1 (
    echo [ERROR] This CMake installation does not advertise:
    echo         "%GENERATOR%"
    echo.
    echo Update CMake to 4.2 or newer and try again.
    goto :fail
)

REM ------------------------------------------------------------
REM Optional full clean:
REM   Build RuneSchema.bat clean
REM ------------------------------------------------------------
if /I "%~1"=="clean" (
    echo [Clean] Removing entire build cache...
    if exist "%BUILD%" rmdir /S /Q "%BUILD%"
    echo.
)

REM ------------------------------------------------------------
REM Automatically discard a stale generator cache.
REM This handles the earlier VS 2022 build directory.
REM ------------------------------------------------------------
if exist "%BUILD%\CMakeCache.txt" (
    findstr /C:"CMAKE_GENERATOR:INTERNAL=%GENERATOR%" "%BUILD%\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 (
        echo [Clean] Existing CMake cache belongs to another generator.
        echo         Removing stale build directory...
        rmdir /S /Q "%BUILD%"
        echo.
    )
)

REM ------------------------------------------------------------
REM A project move changes CMake's source path even with the same generator.
REM Reuse the cache only when it belongs to the raw folder beside this BAT.
REM ------------------------------------------------------------
set "CMAKE_SOURCE=%SOURCE:\=/%"
if exist "%BUILD%\CMakeCache.txt" (
    findstr /I /L /X /C:"CMAKE_HOME_DIRECTORY:INTERNAL=%CMAKE_SOURCE%" "%BUILD%\CMakeCache.txt" >nul 2>&1
    if errorlevel 1 (
        echo [Clean] Existing CMake cache belongs to another source folder.
        echo         Removing stale build directory...
        rmdir /S /Q "%BUILD%"
        if exist "%BUILD%" (
            echo [ERROR] Could not remove the old build cache. Close tools using it and retry.
            goto :fail
        )
        echo.
    )
)

REM ------------------------------------------------------------
REM Build into a unique staging directory. The published package is replaced
REM only after compilation, compression, signing and verification all succeed.
REM ------------------------------------------------------------
mkdir "%DLLDIR%" >nul 2>&1
mkdir "%OUTPUT%\mods" >nul 2>&1
mkdir "%OUTPUT%\plugins" >nul 2>&1
mkdir "%HELPYDIR%" >nul 2>&1
mkdir "%OUTPUT%\settings" >nul 2>&1
mkdir "%OUTPUT%\runtime\live\saved\references" >nul 2>&1
if exist "%RELEASE%\mods" xcopy "%RELEASE%\mods" "%OUTPUT%\mods\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [ERROR] Could not preserve the consolidated mods from the existing clean release.
    goto :fail
)
copy /Y "%ROOT%\settings\settings.jsonc" "%OUTPUT%\settings\settings.jsonc" >nul
if errorlevel 1 (
    echo [ERROR] Could not stage settings\settings.jsonc.
    goto :fail
)
copy /Y "%ROOT%\README.md" "%OUTPUT%\settings\README.md" >nul
if exist "%ROOT%\docs" xcopy "%ROOT%\docs" "%OUTPUT%\settings\docs\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [ERROR] Could not stage current RuneSchema documentation.
    goto :fail
)
if exist "%OUTPUT%\settings\docs\archive" rmdir /S /Q "%OUTPUT%\settings\docs\archive"
if exist "%ROOT%\plugins" xcopy "%ROOT%\plugins" "%OUTPUT%\plugins\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [ERROR] Could not stage RuneSchema plugins.
    goto :fail
)
REM Plugin subfolders are capability-based. Do not manufacture empty dll, paks,
REM scripts, settings, or docs directories for plugins that do not use them.

if not exist "%BUILD%" mkdir "%BUILD%" >nul 2>&1

REM ------------------------------------------------------------
REM Configure
REM ------------------------------------------------------------
echo [1/5] Configuring RuneSchema...
echo Generator : %GENERATOR%
echo Platform  : x64
echo.

cmake -S "%SOURCE%" -B "%BUILD%" ^
    -G "%GENERATOR%" ^
    -A x64

if errorlevel 1 (
    echo.
    echo ============================================================
    echo [ERROR] CMake configuration failed
    echo ============================================================
    echo.
    echo The Visual Studio detection above is now CMake's own result.
    echo If it reports missing compiler/toolset components, open:
    echo.
    echo   Visual Studio Installer ^> Visual Studio 2026 ^> Modify
    echo.
    echo and enable:
    echo   Desktop development with C++
    echo.
    echo Recommended individual components:
    echo   - MSVC x64/x86 C++ build tools
    echo   - Windows 11 SDK ^(or current Windows SDK^)
    echo   - C++ CMake tools for Windows
    echo.
    goto :fail
)

REM ------------------------------------------------------------
REM Build
REM ------------------------------------------------------------
echo.
echo [2/5] Building %TARGET%...
echo Configuration: %CONFIG%
echo.

cmake --build "%BUILD%" ^
    --config "%CONFIG%" ^
    --target "%TARGET%" RuneSchemaHelpyPlugin ^
    --parallel

if errorlevel 1 (
    echo.
    echo ============================================================
    echo [ERROR] RuneSchema compilation failed
    echo ============================================================
    echo.
    echo Configuration succeeded, so the error above is now an
    echo actual source/compiler/linker problem. Send that section
    echo and it can be fixed directly.
    goto :fail
)

REM ------------------------------------------------------------
REM Find RuneSchema.dll
REM ------------------------------------------------------------
echo.
echo [3/5] Staging standard output...
echo.

set "BUILTDLL="

for %%P in (
    "%BUILD%\%CONFIG%\RuneSchema.dll"
    "%BUILD%\RuneSchema\%CONFIG%\RuneSchema.dll"
    "%BUILD%\bin\%CONFIG%\RuneSchema.dll"
    "%BUILD%\RuneSchema.dll"
) do (
    if exist "%%~P" (
        set "BUILTDLL=%%~P"
        goto :founddll
    )
)

for /f "delims=" %%F in ('dir /S /B "%BUILD%\RuneSchema.dll" 2^>nul') do (
    set "BUILTDLL=%%F"
    goto :founddll
)

:founddll
if not defined BUILTDLL (
    echo [ERROR] Build returned success, but RuneSchema.dll was not found.
    echo Search root:
    echo   %BUILD%
    goto :fail
)

copy /Y "%BUILTDLL%" "%FINALDLL%" >nul
if errorlevel 1 (
    echo [ERROR] Could not stage:
    echo   %BUILTDLL%
    echo.
    echo to:
    echo   %FINALDLL%
    goto :fail
)

set "BUILTHELPY=%BUILD%\%CONFIG%\RuneSchema.Helpy.dll"
if not exist "%BUILTHELPY%" (
    echo [ERROR] Build returned success, but RuneSchema.Helpy.dll was not found.
    goto :fail
)
copy /Y "%BUILTHELPY%" "%HELPYDLL%" >nul
if errorlevel 1 (
    echo [ERROR] Could not stage Helpy plugin DLL: %BUILTHELPY%
    goto :fail
)
REM ------------------------------------------------------------
REM Attempt maximum compression for every produced DLL. Compression always
REM precedes signing because changing a signed PE image invalidates its signature.
REM A DLL that UPX cannot safely pack retains its original linker output and the
REM build continues; one incompatible plugin must never discard the whole release.
REM ------------------------------------------------------------
echo.
echo [4/5] Attempting UPX maximum compression for every release DLL...
set "MAINCOMPRESSION=Uncompressed"
set "HELPYCOMPRESSION=Uncompressed"
set "DLLCOUNT=0"
set "DLLCOMPRESSED=0"
for /R "%OUTPUT%" %%D in (*.dll) do (
    set /A DLLCOUNT+=1
    set "DLLSTATE=Uncompressed"
    call :compress_optional "%%~fD" DLLSTATE
    if /I "!DLLSTATE:~0,3!"=="UPX" set /A DLLCOMPRESSED+=1
    if /I "%%~fD"=="%FINALDLL%" set "MAINCOMPRESSION=!DLLSTATE!"
    if /I "%%~fD"=="%HELPYDLL%" set "HELPYCOMPRESSION=!DLLSTATE!"
)
if "!DLLCOUNT!"=="0" (
    echo [ERROR] No release DLLs were available for compression.
    goto :fail
)
echo Compression attempts: !DLLCOUNT! DLL^(s^); UPX verified: !DLLCOMPRESSED!.

REM ------------------------------------------------------------
REM Authenticode signing. Configure either a certificate-store thumbprint or a
REM PFX path. Missing configuration produces a clearly marked unsigned build.
REM Compression always comes first.
REM ------------------------------------------------------------
echo.
echo [5/5] Finalizing release signature...
set "SIGNSTATE=Unsigned development build"
set "SIGNTOOL="
set "SIGNREQUESTED="
if defined RUNESCHEMA_SIGN_CERT_SHA1 set "SIGNREQUESTED=1"
if defined RUNESCHEMA_SIGN_PFX set "SIGNREQUESTED=1"
if not defined SIGNREQUESTED (
    echo [WARNING] Release signing is not configured; publishing an unsigned build.
    echo Copy build-signing.local.cmd.example to build-signing.local.cmd to enable signing.
) else (
    for /f "delims=" %%S in ('where signtool.exe 2^>nul') do if not defined SIGNTOOL set "SIGNTOOL=%%S"
    if not defined SIGNTOOL for /f "usebackq delims=" %%S in (`powershell -NoProfile -Command "$p=Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue ^| Where-Object FullName -Match '\\x64\\signtool.exe$' ^| Sort-Object FullName -Descending ^| Select-Object -First 1 -ExpandProperty FullName; $p"`) do set "SIGNTOOL=%%S"
    if not defined SIGNTOOL (
        echo [ERROR] Signing is configured, but signtool.exe was not found.
        goto :fail
    )
    if defined RUNESCHEMA_SIGN_PFX (
        if not exist "%RUNESCHEMA_SIGN_PFX%" (
            echo [ERROR] Signing PFX does not exist: %RUNESCHEMA_SIGN_PFX%
            goto :fail
        )
        REM SignTool securely prompts for the PFX password; it is never saved in the BAT.
        "!SIGNTOOL!" sign /f "%RUNESCHEMA_SIGN_PFX%" /fd SHA256 /tr "%RUNESCHEMA_SIGN_TIMESTAMP%" /td SHA256 "%FINALDLL%" "%HELPYDLL%"
    ) else (
        set "STORE_SWITCH="
        if /I "%RUNESCHEMA_SIGN_STORE%"=="LocalMachine" set "STORE_SWITCH=/sm"
        "!SIGNTOOL!" sign !STORE_SWITCH! /sha1 "%RUNESCHEMA_SIGN_CERT_SHA1%" /fd SHA256 /tr "%RUNESCHEMA_SIGN_TIMESTAMP%" /td SHA256 "%FINALDLL%" "%HELPYDLL%"
    )
    if errorlevel 1 (
        echo [ERROR] Authenticode signing failed. Nothing was published.
        goto :fail
    )
    "!SIGNTOOL!" verify /pa /all "%FINALDLL%"
    if errorlevel 1 (
        echo [ERROR] Authenticode verification failed. Nothing was published.
        goto :fail
    )
    "!SIGNTOOL!" verify /pa /all "%HELPYDLL%"
    if errorlevel 1 (
        echo [ERROR] Helpy plugin Authenticode verification failed. Nothing was published.
        goto :fail
    )
    set "SIGNSTATE=Signed and verified"
)

REM ------------------------------------------------------------
REM Build receipt
REM ------------------------------------------------------------
(
    echo RuneSchema build receipt
    echo ========================
    echo Source=%SOURCE%
    echo Generator=%GENERATOR%
    echo Configuration=%CONFIG%
    echo BuiltDLL=%BUILTDLL%
    echo BuiltHelpyDLL=%BUILTHELPY%
    echo ReleaseDLL=%FINALDLL%
    echo HelpyDLL=%HELPYDLL%
    echo MainCompression=!MAINCOMPRESSION!
    echo HelpyCompression=!HELPYCOMPRESSION!
    echo DLLCompressionAttempts=!DLLCOUNT!
    echo DLLsUPXVerified=!DLLCOMPRESSED!
    echo Signature=!SIGNSTATE!
    echo Date=%DATE%
    echo Time=%TIME%
) > "%OUTPUT%\settings\BUILD-INFO.txt"

REM Publish only a complete, verified staging tree. Preserve the previous output
REM as RuneSchema.previous rather than recursively deleting it.
set "PREVIOUS=%RELEASE%.previous"
if exist "!PREVIOUS!" set "PREVIOUS=%RELEASE%.previous-%RANDOM%-%RANDOM%"
if exist "%RELEASE%" move "%RELEASE%" "!PREVIOUS!" >nul
move "%OUTPUT%" "%RELEASE%" >nul
if errorlevel 1 (
    echo [ERROR] Could not publish the staged package.
    if exist "!PREVIOUS!" if not exist "%RELEASE%" move "!PREVIOUS!" "%RELEASE%" >nul
    goto :fail
)
set "OUTPUT=%RELEASE%"
set "DLLDIR=%OUTPUT%\dlls"
set "FINALDLL=%DLLDIR%\main.dll"
set "HELPYDIR=%OUTPUT%\plugins\RuneSchema.Helpy\dll"
set "HELPYDLL=%HELPYDIR%\RuneSchema.Helpy.dll"

echo.
echo ============================================================
echo [SUCCESS] RuneSchema built and staged
echo ============================================================
echo.
echo Compressed release DLL:
echo   %FINALDLL%
echo.
for %%F in ("%FINALDLL%") do echo Size: %%~zF bytes
echo.
echo SHA-256:
certutil -hashfile "%FINALDLL%" SHA256 | findstr /V /C:"hash of file" /C:"CertUtil:"
echo.
echo Helpy plugin DLL:
echo   %HELPYDLL%
for %%F in ("%HELPYDLL%") do echo Size: %%~zF bytes
certutil -hashfile "%HELPYDLL%" SHA256 | findstr /V /C:"hash of file" /C:"CertUtil:"
echo.
echo Signature: !SIGNSTATE!
echo.
echo Cached build directory:
echo   %BUILD%
echo.
echo Clean package directory:
echo   %OUTPUT%
echo.
echo Force a completely clean rebuild with:
echo   "%~nx0" clean
echo.
pause
exit /b 0

:compress_optional
set "COMPRESSDLL=%~1"
set "COMPRESSSTATE=%~2"
copy /Y "%COMPRESSDLL%" "%COMPRESSDLL%.uncompressed" >nul
if errorlevel 1 (
    echo [WARNING] Could not create a compression fallback for %COMPRESSDLL%.
    set "%COMPRESSSTATE%=Uncompressed; backup unavailable"
    exit /b 0
)
"%UPXEXE%" -9 --best --lzma "%COMPRESSDLL%"
if errorlevel 1 (
    echo [WARNING] UPX declined %COMPRESSDLL%; retaining the uncompressed DLL.
    copy /Y "%COMPRESSDLL%.uncompressed" "%COMPRESSDLL%" >nul
    del /Q "%COMPRESSDLL%.uncompressed" >nul 2>&1
    set "%COMPRESSSTATE%=Uncompressed; UPX declined"
    exit /b 0
)
"%UPXEXE%" -t "%COMPRESSDLL%"
if errorlevel 1 (
    echo [WARNING] UPX verification failed for %COMPRESSDLL%; restoring the uncompressed DLL.
    copy /Y "%COMPRESSDLL%.uncompressed" "%COMPRESSDLL%" >nul
    del /Q "%COMPRESSDLL%.uncompressed" >nul 2>&1
    set "%COMPRESSSTATE%=Uncompressed; UPX verification failed"
    exit /b 0
)
del /Q "%COMPRESSDLL%.uncompressed" >nul 2>&1
set "%COMPRESSSTATE%=UPX -9 --best --lzma; verified"
exit /b 0

:fail
echo.
echo ============================================================
echo [FAILED] RuneSchema was not staged
echo ============================================================
echo.
echo Do not use an existing DLL as a result of this failed build.
echo Expected successful output:
echo   %FINALDLL%
echo.
pause
exit /b 1
