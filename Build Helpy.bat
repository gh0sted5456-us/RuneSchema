@echo off
call "%~dp0build\build.bat" -PluginOnly %*
exit /b %ERRORLEVEL%
