@echo off
echo Compiling shaders...

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

REM Compile octree ray marching shaders
%GLSLC% shaders/octree_raymarch.vert -o shaders/octree_raymarch.vert.spv
if %errorlevel% neq 0 (
    echo Failed to compile octree_raymarch.vert
    exit /b 1
)

%GLSLC% shaders/octree_raymarch.frag -o shaders/octree_raymarch.frag.spv
if %errorlevel% neq 0 (
    echo Failed to compile octree_raymarch.frag
    exit /b 1
)

echo Shaders compiled successfully!