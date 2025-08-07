@echo off
echo Compiling shaders...

REM Clean old compiled shaders first
echo Cleaning old compiled shaders...
del /Q shaders\*.spv 2>nul
del /Q shaders\vertex\*.spv 2>nul
del /Q shaders\fragment\*.spv 2>nul
del /Q build\bin\Release\shaders\*.spv 2>nul
del /Q build\bin\Debug\shaders\*.spv 2>nul

REM Try to find glslc in PATH first
where glslc >nul 2>&1
if %errorlevel% equ 0 (
    set GLSLC=glslc
) else (
    REM Try common Vulkan SDK location
    set GLSLC=C:\glsl\glslc.exe
    if not exist "%GLSLC%" (
        echo Error: glslc.exe not found! Please install Vulkan SDK or add to PATH.
        exit /b 1
    )
)

REM Compile hierarchical octree shaders
%GLSLC% shaders/src/vertex/hierarchical.vert -o shaders/hierarchical.vert.spv
if %errorlevel% neq 0 (
    echo Failed to compile hierarchical.vert
    exit /b 1
)

%GLSLC% shaders/src/fragment/hierarchical.frag -o shaders/hierarchical.frag.spv
if %errorlevel% neq 0 (
    echo Failed to compile hierarchical.frag
    exit /b 1
)

echo Shaders compiled successfully!