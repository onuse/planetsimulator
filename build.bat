@echo off
setlocal enabledelayedexpansion

echo Building Octree Planet Simulator (C++)...
echo.

REM Always clean and rebuild shaders
echo Cleaning and rebuilding shaders...
if exist shaders (
    cd shaders
    
    REM Clean all generated files
    echo Deleting all .frag and .spv files...
    del /F /Q *.spv 2>nul
    del /F /Q src\fragment\*.frag 2>nul
    del /F /Q ..\build\bin\Release\shaders\*.spv 2>nul
    
    REM Compile shaders
    echo Compiling shaders...
    call compile.bat
    if !ERRORLEVEL! NEQ 0 (
        echo Shader compilation failed!
        cd ..
        pause
        exit /b 1
    )
    echo Shaders compiled successfully!
    
    cd ..
)

REM Create build directory if it doesn't exist
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
"C:\Program Files\CMake\bin\cmake.exe" .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed\!
    pause
    exit /b 1
)

echo.
echo Building Release build...
"C:\Program Files\CMake\bin\cmake.exe" --build . --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo Build failed\!
    pause
    exit /b 1
)

echo.
echo Build complete!

REM Copy shaders to build directory (single operation)
if exist ..\shaders\*.spv (
    echo Copying shaders to build directory...
    if not exist bin\Release\shaders mkdir bin\Release\shaders
    copy /Y ..\shaders\*.spv bin\Release\shaders\ >nul 2>&1
)

REM Run tests automatically
echo.
echo Running tests...
ctest -C Release --output-on-failure
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Tests failed!
    cd ..
    pause
    exit /b 1
)
echo All tests passed!

echo.
echo Executable: build\bin\Release\OctreePlanet.exe
echo.
echo Run with: bin\Release\OctreePlanet.exe
echo Or with options: bin\Release\OctreePlanet.exe -auto-terminate 60 -screenshot-interval 10
cd ..
