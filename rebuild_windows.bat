@echo off
setlocal enabledelayedexpansion

echo Building Planet Simulator...
echo.

REM Work from the project root regardless of where this was invoked from.
pushd "%~dp0"

set BUILD_DIR=build_windows

call shaders\compile.bat
if !ERRORLEVEL! NEQ 0 (
    echo Shader compilation failed!
    popd
    exit /b 1
)

if "%1"=="clean" (
    echo Cleaning build directory...
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
)

REM GLFW 3.3.8 declares a cmake_minimum_required that CMake 4 rejects, hence
REM the policy flag. Drop it when GLFW is bumped to 3.4 or newer.
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    popd
    exit /b 1
)

cmake --build %BUILD_DIR% --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    popd
    exit /b 1
)

REM The executable loads shaders relative to the working directory, and run.ps1
REM runs it from the project root - so this copy is only for running the binary
REM directly out of the build tree.
if not exist %BUILD_DIR%\bin\Release\shaders mkdir %BUILD_DIR%\bin\Release\shaders
copy /Y shaders\*.spv %BUILD_DIR%\bin\Release\shaders\ >nul 2>&1

echo.
echo Running tests...
ctest --test-dir %BUILD_DIR% -C Release --output-on-failure
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo WARNING: some tests failed
)

echo.
echo Build complete: %BUILD_DIR%\bin\Release\PlanetSimulator.exe
echo Run with: run.ps1

popd
endlocal
