@echo off
echo ========================================
echo Rebuilding and Testing GPU Debug Shader
echo ========================================
echo.

echo Step 1: Building project...
call rebuild.bat
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Step 2: Running test...
echo Press 'G' to switch to GPU pipeline
echo Expected: Blue/green split planet instead of beige
echo.

cd /d C:\Users\glimm\Documents\Projects\planetsimulator
build_windows\bin\Release\PlanetSimulator.exe -auto-terminate 5

echo.
echo ========================================
echo Test complete. Check visual output.
echo ========================================
pause