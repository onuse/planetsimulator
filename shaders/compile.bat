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

REM Compile hierarchical shaders directly to root
echo Compiling hierarchical.vert...
"%GLSLC%" %SRC_VERTEX%\hierarchical.vert -o hierarchical.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile hierarchical.vert
    exit /b 1
)

echo Compiling hierarchical.frag...
"%GLSLC%" %SRC_FRAGMENT%\hierarchical.frag -o hierarchical.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile hierarchical.frag
    exit /b 1
)

REM Compile octree raymarch shaders directly to root
echo Compiling octree_raymarch.vert...
"%GLSLC%" %SRC_VERTEX%\octree_raymarch.vert -o octree_raymarch.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile octree_raymarch.vert
    exit /b 1
)

echo Compiling octree_raymarch.frag...
"%GLSLC%" %SRC_FRAGMENT%\octree_raymarch.frag -o octree_raymarch.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile octree_raymarch.frag
    exit /b 1
)

REM Create triangle.frag manually since it's not generated from template
echo Creating triangle.frag...
(
echo #version 450
echo.
echo // Inputs from vertex shader
echo layout(location = 0^) in vec3 fragColor;
echo layout(location = 1^) in vec3 fragNormal;
echo layout(location = 2^) in vec3 fragWorldPos;
echo.
echo // Uniform buffer
echo layout(binding = 0^) uniform UniformBufferObject {
echo     mat4 view;
echo     mat4 proj;
echo     mat4 viewProj;
echo     vec3 viewPos;
echo     float time;
echo     vec3 lightDir;
echo     float padding;
echo } ubo;
echo.
echo // Output color
echo layout(location = 0^) out vec4 outColor;
echo.
echo void main(^) {
echo     // Simple Lambertian lighting
echo     vec3 lightColor = vec3(1.0, 1.0, 0.95^);
echo     vec3 ambient = 0.3 * fragColor;
echo.    
echo     // Directional lighting
echo     vec3 lightDirNorm = normalize(-ubo.lightDir^);
echo     float diff = max(dot(normalize(fragNormal^), lightDirNorm^), 0.0^);
echo     vec3 diffuse = diff * lightColor * fragColor;
echo.    
echo     // Simple atmospheric scattering for distance
echo     float distance = length(ubo.viewPos - fragWorldPos^);
echo     float fogFactor = 1.0 - exp(-distance / 1000000.0^); // 1000km fog distance
echo     vec3 fogColor = vec3(0.7, 0.85, 1.0^);
echo.    
echo     vec3 finalColor = ambient + diffuse;
echo     finalColor = mix(finalColor, fogColor, fogFactor * 0.2^);
echo.    
echo     outColor = vec4(finalColor, 1.0^);
echo }
) > %SRC_FRAGMENT%\triangle.frag

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

echo.
echo Shaders compiled successfully!
echo Output files:
dir *.spv /b