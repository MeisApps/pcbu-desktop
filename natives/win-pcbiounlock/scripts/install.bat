@echo off
robocopy "%~dp0\..\cmake-build-debug" "%windir%\system32" "win-pcbiounlock.dll"
pause
