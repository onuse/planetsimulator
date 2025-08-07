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

REM Transpile C template to GLSL if template exists
if exist octree_raymarch_template.c (
    echo Transpiling octree_raymarch_template.c to octree_raymarch_generated.frag...
    python extract_glsl.py octree_raymarch_template.c octree_raymarch_generated.frag
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile template - continuing with existing shaders
    )
)

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

REM Compile fixed octree raymarch shader if it exists
if exist octree_raymarch_fixed.frag (
    echo Compiling octree_raymarch_fixed.frag...
    "%GLSLC%" octree_raymarch_fixed.frag -o octree_raymarch_fixed.frag.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile octree_raymarch_fixed.frag
        exit /b 1
    )
)

REM Compile generated octree raymarch shader if it exists
if exist octree_raymarch_generated.frag (
    echo Compiling octree_raymarch_generated.frag...
    "%GLSLC%" octree_raymarch_generated.frag -o octree_raymarch_generated.frag.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile octree_raymarch_generated.frag
        exit /b 1
    ) else (
        echo Copying generated shader as main octree_raymarch.frag.spv...
        copy /Y octree_raymarch_generated.frag.spv octree_raymarch.frag.spv
    )
)

echo Shaders compiled successfully!
echo.
echo Output files:
dir *.spv /b