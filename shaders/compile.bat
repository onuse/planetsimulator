@echo off
echo Compiling shaders...

REM IMPORTANT: First delete old .spv files to avoid cache/permission issues
echo Cleaning old shader binaries...
del /F /Q *.spv 2>nul

REM Find glslc - check multiple VulkanSDK versions
where glslc >nul 2>&1
if %errorlevel% equ 0 (
    set GLSLC=glslc
) else if exist "C:\VulkanSDK\1.4.321.1\Bin\glslc.exe" (
    set GLSLC=C:\VulkanSDK\1.4.321.1\Bin\glslc.exe
) else if exist "C:\VulkanSDK\1.3.290.0\Bin\glslc.exe" (
    set GLSLC=C:\VulkanSDK\1.3.290.0\Bin\glslc.exe
) else if exist "C:\VulkanSDK\1.3.283.0\Bin\glslc.exe" (
    set GLSLC=C:\VulkanSDK\1.3.283.0\Bin\glslc.exe
) else if exist "C:\VulkanSDK\1.3.275.0\Bin\glslc.exe" (
    set GLSLC=C:\VulkanSDK\1.3.275.0\Bin\glslc.exe
) else (
    echo ERROR: Could not find glslc.exe
    echo Please install VulkanSDK or update paths in this script
    exit /b 1
)

echo Using glslc: %GLSLC%

REM Compile hierarchical shaders
echo Compiling hierarchical.vert...
"%GLSLC%" hierarchical.vert -o hierarchical.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile hierarchical.vert
    exit /b 1
)

echo Compiling hierarchical.frag...
"%GLSLC%" hierarchical.frag -o hierarchical.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile hierarchical.frag
    exit /b 1
)

REM Compile octree raymarch shaders
echo Compiling octree_raymarch.vert...
"%GLSLC%" octree_raymarch.vert -o octree_raymarch.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile octree_raymarch.vert
    exit /b 1
)

echo Compiling octree_raymarch.frag...
"%GLSLC%" octree_raymarch.frag -o octree_raymarch.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile octree_raymarch.frag
    exit /b 1
)

echo Shaders compiled successfully!
echo.
echo Output files:
dir *.spv /b