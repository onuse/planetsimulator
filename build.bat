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
    
    REM Check each shader source file
    for %%f in (*.frag *.vert) do (
        set shader_name=%%~nf
        set shader_spv=!shader_name!.spv
        
        REM Check if .spv exists
        if not exist "!shader_spv!" (
            echo Shader !shader_spv! is missing - must recompile
            set SHADER_STALE=1
        ) else (
            REM Use PowerShell to check file age (more reliable than forfiles)
            for /f %%a in ('powershell -command "(Get-Date) - (Get-Item '!shader_spv!').LastWriteTime | %% TotalMinutes"') do (
                set age=%%a
                REM Check if older than 2 minutes
                for /f %%b in ('powershell -command "if (!age! -gt 2) {echo 1} else {echo 0}"') do (
                    if %%b==1 (
                        echo Shader !shader_spv! is older than 2 minutes - must recompile
                        set SHADER_STALE=1
                    )
                )
            )
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
        if exist ..\build\bin\Release\shaders (
            copy /Y *.spv ..\build\bin\Release\shaders\ >nul 2>&1
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
    copy /Y ..\shaders\*.spv bin\Release\shaders\ >nul 2>&1
)

echo Executable: build\bin\Release\OctreePlanet.exe
echo.
echo Run with: bin\Release\OctreePlanet.exe
echo Or with options: bin\Release\OctreePlanet.exe -auto-terminate 60 -screenshot-interval 10
cd ..
