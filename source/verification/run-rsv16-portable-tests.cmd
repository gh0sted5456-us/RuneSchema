@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "RS_INCLUDE=C:\RuneSchema\Build\_deps\nlohmann_json-src\include"
set "RS_OUT=%TEMP%\runeschema-rsv16-tests"
if not exist "%RS_OUT%" mkdir "%RS_OUT%"
for %%T in (dialogue-definition quest-definition event-definition loader-schemas npc-catalog spawn-radius example-mods) do (
  cl /nologo /EHsc /std:c++20 /W4 /WX /Iraw\include /I"%RS_INCLUDE%" raw\tests\%%T.cpp /Fe:"%RS_OUT%\%%T.exe" /Fo:"%RS_OUT%\%%T.obj"
  if errorlevel 1 goto :failed
  "%RS_OUT%\%%T.exe"
  if errorlevel 1 goto :failed
)
cl /nologo /EHsc /std:c++20 /W4 /WX /Iraw\include /I"%RS_INCLUDE%" verification\F2BrowserTests.cpp /Fe:"%RS_OUT%\F2BrowserTests.exe" /Fo:"%RS_OUT%\F2BrowserTests.obj"
if errorlevel 1 goto :failed
pushd "%RS_OUT%"
F2BrowserTests.exe
if errorlevel 1 goto :failed_popd
popd
echo RSv16 portable tests: PASS
exit /b 0
:failed_popd
popd
:failed
echo RSv16 portable tests: FAIL
exit /b 1
