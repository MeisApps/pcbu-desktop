@echo off
robocopy "%~dp0\..\cmake-build-debug" "%windir%\system32" "win_pulseunlock.dll"
pause
