@echo off
call "%~dp0build\build.bat" %*
exit /b %ERRORLEVEL%
