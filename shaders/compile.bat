@echo off
echo Compiling shaders with new folder structure...

REM Set paths
set SRC_VERTEX=src\vertex
set SRC_FRAGMENT=src\fragment
set SRC_TEMPLATES=src\templates
set TOOLS=tools
set COMPILED=compiled

REM IMPORTANT: First delete old .spv files to avoid cache/permission issues
echo Cleaning old shader binaries...
del /F /Q %COMPILED%\*.spv 2>nul

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

REM Transpile C templates to GLSL
if exist %SRC_TEMPLATES%\octree_raymarch_template.c (
    echo Transpiling octree_raymarch_template.c to octree_raymarch.frag...
    python %TOOLS%\extract_glsl.py %SRC_TEMPLATES%\octree_raymarch_template.c %SRC_FRAGMENT%\octree_raymarch.frag
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile octree_raymarch template - continuing with existing shaders
    )
)

if exist %SRC_TEMPLATES%\hierarchical_template.c (
    echo Transpiling hierarchical_template.c to hierarchical.frag...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\hierarchical_template.c %SRC_FRAGMENT%\hierarchical.frag
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile hierarchical template - continuing with existing shaders
    )
)

REM Compile hierarchical shaders
echo Compiling hierarchical.vert...
"%GLSLC%" %SRC_VERTEX%\hierarchical.vert -o %COMPILED%\hierarchical.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile hierarchical.vert
    exit /b 1
)

echo Compiling hierarchical.frag...
"%GLSLC%" %SRC_FRAGMENT%\hierarchical.frag -o %COMPILED%\hierarchical.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile hierarchical.frag
    exit /b 1
)

REM Compile octree raymarch shaders
echo Compiling octree_raymarch.vert...
"%GLSLC%" %SRC_VERTEX%\octree_raymarch.vert -o %COMPILED%\octree_raymarch.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile octree_raymarch.vert
    exit /b 1
)

echo Compiling octree_raymarch.frag...
"%GLSLC%" %SRC_FRAGMENT%\octree_raymarch.frag -o %COMPILED%\octree_raymarch.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile octree_raymarch.frag
    exit /b 1
)

REM Compile fixed octree raymarch shader if it exists
if exist %SRC_FRAGMENT%\octree_raymarch_fixed.frag (
    echo Compiling octree_raymarch_fixed.frag...
    "%GLSLC%" %SRC_FRAGMENT%\octree_raymarch_fixed.frag -o %COMPILED%\octree_raymarch_fixed.frag.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile octree_raymarch_fixed.frag
        exit /b 1
    )
)


REM Also copy compiled shaders to root for backward compatibility
echo Copying compiled shaders to root for compatibility...
copy /Y %COMPILED%\*.spv . 2>nul

echo Shaders compiled successfully!
echo.
echo Output files:
dir %COMPILED%\*.spv /b