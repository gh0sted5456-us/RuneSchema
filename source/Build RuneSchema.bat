@echo off
call "%~dp0..\build\build.bat" %*
exit /b %ERRORLEVEL%
