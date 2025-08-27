@echo off
REM This script redirects to the Windows-specific rebuild script
REM For WSL/Linux builds, use rebuild_wsl.sh instead

call rebuild_windows.bat %*