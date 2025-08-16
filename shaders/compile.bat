@echo off
echo Compiling shaders...

REM Set paths
set SRC_VERTEX=src\vertex
set SRC_FRAGMENT=src\fragment
set SRC_TEMPLATES=src\templates
set TOOLS=tools

REM Note: build.bat already cleans files, but clean here too for standalone use
del /F /Q *.spv 2>nul
del /F /Q %SRC_FRAGMENT%\*.frag 2>nul

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

REM Deprecated shaders have been moved to shaders/archive/
REM Only compile shaders that are actively used (triangle shaders for Transvoxel)

REM Transpile triangle fragment shader template
if exist %SRC_TEMPLATES%\triangle_fragment_template.c (
    echo Transpiling triangle_fragment_template.c to triangle.frag...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\triangle_fragment_template.c %SRC_FRAGMENT%\triangle.frag
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile triangle fragment template - continuing with existing shaders
    )
)

REM Transpile quadtree shaders for LOD system
if exist %SRC_TEMPLATES%\quadtree_vertex_template.c (
    echo Transpiling quadtree_vertex_template.c to quadtree_patch.vert...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\quadtree_vertex_template.c %SRC_VERTEX%\quadtree_patch.vert
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile quadtree vertex template
    )
)

if exist %SRC_TEMPLATES%\quadtree_fragment_template.c (
    echo Transpiling quadtree_fragment_template.c to quadtree_patch.frag...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\quadtree_fragment_template.c %SRC_FRAGMENT%\quadtree_patch.frag
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile quadtree fragment template
    )
)

REM Compile triangle shaders for Transvoxel
echo Compiling triangle.vert...
"%GLSLC%" %SRC_VERTEX%\triangle.vert -o triangle.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile triangle.vert
    exit /b 1
)

echo Compiling triangle.frag...
"%GLSLC%" %SRC_FRAGMENT%\triangle.frag -o triangle.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile triangle.frag
    exit /b 1
)

REM Compile quadtree shaders for LOD system
if exist %SRC_VERTEX%\quadtree_patch.vert (
    echo Compiling quadtree_patch.vert...
    "%GLSLC%" %SRC_VERTEX%\quadtree_patch.vert -o quadtree_patch.vert.spv
    if %errorlevel% neq 0 (
        echo WARNING: Failed to compile quadtree_patch.vert
    )
)

REM Compile CPU vertex shader for quadtree
if exist %SRC_VERTEX%\quadtree_patch_cpu.vert (
    echo Compiling quadtree_patch_cpu.vert...
    "%GLSLC%" %SRC_VERTEX%\quadtree_patch_cpu.vert -o quadtree_patch_cpu.vert.spv
    if %errorlevel% neq 0 (
        echo WARNING: Failed to compile quadtree_patch_cpu.vert
    )
)

if exist %SRC_FRAGMENT%\quadtree_patch.frag (
    echo Compiling quadtree_patch.frag...
    "%GLSLC%" %SRC_FRAGMENT%\quadtree_patch.frag -o quadtree_patch.frag.spv
    if %errorlevel% neq 0 (
        echo WARNING: Failed to compile quadtree_patch.frag
    )
)

echo.
echo Shaders compiled successfully!
echo Output files:
dir *.spv /b