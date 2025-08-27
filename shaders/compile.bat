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

REM Compile test shaders if they exist (for debugging)
if exist %SRC_VERTEX%\test_simple.vert (
    echo Compiling test_simple.vert...
    "%GLSLC%" %SRC_VERTEX%\test_simple.vert -o test_simple.vert.spv
    if %errorlevel% neq 0 (
        echo WARNING: Failed to compile test_simple.vert
    )
)

if exist %SRC_FRAGMENT%\test_simple.frag (
    echo Compiling test_simple.frag...
    "%GLSLC%" %SRC_FRAGMENT%\test_simple.frag -o test_simple.frag.spv
    if %errorlevel% neq 0 (
        echo WARNING: Failed to compile test_simple.frag
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

REM Transpile point cloud shaders
if exist %SRC_TEMPLATES%\point_cloud_vertex_template.c (
    echo Transpiling point_cloud_vertex_template.c to point_cloud.vert...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\point_cloud_vertex_template.c %SRC_VERTEX%\point_cloud.vert
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile point cloud vertex template
    )
)

if exist %SRC_TEMPLATES%\point_cloud_fragment_template.c (
    echo Transpiling point_cloud_fragment_template.c to point_cloud.frag...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\point_cloud_fragment_template.c %SRC_FRAGMENT%\point_cloud.frag
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile point cloud fragment template
    )
)

REM Transpile compute shader templates
if exist %SRC_TEMPLATES%\octree_verify_compute_template.c (
    echo Transpiling octree_verify_compute_template.c to octree_verify.comp...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\octree_verify_compute_template.c src\compute\octree_verify.comp
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile octree_verify compute template
    )
)

if exist %SRC_TEMPLATES%\surface_points_compute_template.c (
    echo Transpiling surface_points_compute_template.c to surface_points.comp...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\surface_points_compute_template.c src\compute\surface_points.comp
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile surface_points compute template
    )
)

if exist %SRC_TEMPLATES%\mesh_generator_compute_template.c (
    echo Transpiling mesh_generator_compute_template.c to mesh_generator.comp...
    python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\mesh_generator_compute_template.c src\compute\mesh_generator.comp
    if %errorlevel% neq 0 (
        echo WARNING: Failed to transpile mesh_generator compute template
    )
)

REM Compile compute shaders
set SRC_COMPUTE=src\compute
if exist %SRC_COMPUTE%\mesh_generator.comp (
    echo Compiling mesh_generator.comp...
    "%GLSLC%" %SRC_COMPUTE%\mesh_generator.comp -o mesh_generator.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile mesh_generator.comp
        exit /b 1
    )
)

if exist %SRC_COMPUTE%\mesh_generator_simple.comp (
    echo Compiling mesh_generator_simple.comp...
    "%GLSLC%" %SRC_COMPUTE%\mesh_generator_simple.comp -o mesh_generator_simple.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile mesh_generator_simple.comp
        exit /b 1
    )
)

if exist %SRC_COMPUTE%\mesh_generator_simple_sphere.comp (
    echo Compiling mesh_generator_simple_sphere.comp...
    "%GLSLC%" %SRC_COMPUTE%\mesh_generator_simple_sphere.comp -o mesh_generator_simple_sphere.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile mesh_generator_simple_sphere.comp
        exit /b 1
    )
)

if exist %SRC_COMPUTE%\octree_verify.comp (
    echo Compiling octree_verify.comp...
    "%GLSLC%" %SRC_COMPUTE%\octree_verify.comp -o octree_verify.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile octree_verify.comp
        exit /b 1
    )
)

if exist %SRC_COMPUTE%\surface_points.comp (
    echo Compiling surface_points.comp...
    "%GLSLC%" %SRC_COMPUTE%\surface_points.comp -o surface_points.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile surface_points.comp
        exit /b 1
    )
)

if exist %SRC_COMPUTE%\sphere_generator.comp (
    echo Compiling sphere_generator.comp...
    "%GLSLC%" %SRC_COMPUTE%\sphere_generator.comp -o sphere_generator.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile sphere_generator.comp
        exit /b 1
    )
)

if exist %SRC_COMPUTE%\terrain_sphere.comp (
    echo Compiling terrain_sphere.comp...
    "%GLSLC%" %SRC_COMPUTE%\terrain_sphere.comp -o terrain_sphere.comp.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile terrain_sphere.comp
        exit /b 1
    )
)

REM Compile point cloud shaders
if exist %SRC_VERTEX%\point_cloud.vert (
    echo Compiling point_cloud.vert...
    "%GLSLC%" %SRC_VERTEX%\point_cloud.vert -o point_cloud.vert.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile point_cloud.vert
        exit /b 1
    )
)

if exist %SRC_FRAGMENT%\point_cloud.frag (
    echo Compiling point_cloud.frag...
    "%GLSLC%" %SRC_FRAGMENT%\point_cloud.frag -o point_cloud.frag.spv
    if %errorlevel% neq 0 (
        echo ERROR: Failed to compile point_cloud.frag
        exit /b 1
    )
)

echo.
echo Shaders compiled successfully!
echo Output files:
dir *.spv /b