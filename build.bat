@echo off
setlocal enabledelayedexpansion

echo Building Octree Planet Simulator (C++)...
echo.

REM First check and compile shaders if needed
echo Checking shader freshness...
set SHADER_STALE=0

REM Check if shader directory exists
if exist shaders (
    cd shaders
    
    REM Check if compiled shaders exist
    set REQUIRED_SHADERS=hierarchical.vert.spv hierarchical.frag.spv octree_raymarch.vert.spv octree_raymarch.frag.spv
    for %%s in (%REQUIRED_SHADERS%) do (
        if not exist "compiled\%%s" (
            echo Shader %%s is missing - must recompile
            set SHADER_STALE=1
        )
    )
    
    REM Compile shaders if any are stale
    if !SHADER_STALE!==1 (
        echo.
        echo Recompiling all shaders...
        call compile.bat
        if !ERRORLEVEL! NEQ 0 (
            echo Shader compilation failed!
            cd ..
            pause
            exit /b 1
        )
        echo Shaders compiled successfully!
        
        REM Copy to build directory immediately
        echo Copying shaders to build directory...
        if not exist ..\build\bin\Release\shaders mkdir ..\build\bin\Release\shaders
        copy /Y *.spv ..\build\bin\Release\shaders\ >nul 2>&1
        if exist compiled\*.spv (
            copy /Y compiled\*.spv ..\build\bin\Release\shaders\ >nul 2>&1
        )
    ) else (
        echo All shaders are fresh
    )
    
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
echo Build complete\!

REM Final shader copy to ensure they're fresh
if exist ..\shaders (
    echo Ensuring shaders are up to date in build directory...
    if not exist bin\Release\shaders mkdir bin\Release\shaders
    REM Copy from root shaders directory if they exist
    if exist ..\shaders\*.spv (
        copy /Y ..\shaders\*.spv bin\Release\shaders\ >nul 2>&1
    )
    REM Also copy from compiled subdirectory if they exist
    if exist ..\shaders\compiled\*.spv (
        copy /Y ..\shaders\compiled\*.spv bin\Release\shaders\ >nul 2>&1
    )
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
