@echo off
setlocal

set BUILD_DIR=build_windows

if not exist "%BUILD_DIR%\bin\Release\PlanetSimulator.exe" (
    echo Executable not found! Building first...
    call rebuild_windows.bat
)

echo Running PlanetSimulator (Windows build)...
%BUILD_DIR%\bin\Release\PlanetSimulator.exe %*

endlocal