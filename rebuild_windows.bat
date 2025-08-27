@echo off
setlocal enabledelayedexpansion

echo Building Octree Planet Simulator (Windows)...
echo.

:: Use a separate build directory for Windows
set BUILD_DIR=build_windows

:: Always clean and rebuild shaders
echo Cleaning and rebuilding shaders...
if exist shaders (
    cd shaders
    
    :: Clean all generated files
    echo Deleting all .frag and .spv files...
    del /F /Q *.spv 2>nul
    del /F /Q src\fragment\*.frag 2>nul
    del /F /Q ..\%BUILD_DIR%\bin\Release\shaders\*.spv 2>nul
    
    :: Compile shaders
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

:: Clean build directory if requested
if "%1"=="clean" (
    echo Cleaning Windows build directory...
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
)

:: Create build directory
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

:: Force CMake reconfiguration to detect new files (GLOB_RECURSE workaround)
if exist CMakeCache.txt (
    echo Touching CMakeCache.txt to ensure file changes are detected...
    copy /b CMakeCache.txt +,, CMakeCache.txt >nul
)

:: Configure with CMake (Visual Studio generator)
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

:: Build
echo.
echo Building Release build...
cmake --build . --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo Build complete!

:: Return to root directory to copy shaders
cd ..

:: Copy shaders to build directory
echo Copying shaders to build directory...
if not exist %BUILD_DIR%\bin\Release\shaders mkdir %BUILD_DIR%\bin\Release\shaders
copy /Y shaders\*.spv %BUILD_DIR%\bin\Release\shaders\ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Failed to copy some shaders
)

:: Go back to build directory for tests
cd %BUILD_DIR%

:: Run tests automatically
echo.
echo Running tests...
ctest -C Release --output-on-failure
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo WARNING: Some tests failed
    :: Don't exit on test failure, just warn
)

echo.
echo Executable: %BUILD_DIR%\bin\Release\PlanetSimulator.exe
echo.
echo Run with: %BUILD_DIR%\bin\Release\PlanetSimulator.exe
echo Or with: run_windows.bat -auto-terminate 60 -screenshot-interval 10
cd ..

endlocal