@echo off
REM This script redirects to the Windows-specific build script
REM For WSL/Linux builds, use rebuild_wsl.sh instead

echo Redirecting to Windows build script...
call rebuild_windows.bat %*