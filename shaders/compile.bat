@echo off
echo Compiling shaders...

REM Work from this script's own directory, so it does not matter where it is
REM invoked from.
pushd "%~dp0"

set SRC_VERTEX=src\vertex
set SRC_FRAGMENT=src\fragment
set SRC_TEMPLATES=src\templates
set TOOLS=tools

REM The transpiled .vert/.frag output directories are generated, not tracked,
REM so they will not exist on a fresh clone.
if not exist %SRC_VERTEX% mkdir %SRC_VERTEX%
if not exist %SRC_FRAGMENT% mkdir %SRC_FRAGMENT%

del /F /Q *.spv 2>nul
del /F /Q %SRC_FRAGMENT%\*.frag 2>nul
del /F /Q %SRC_VERTEX%\*.vert 2>nul

REM Find glslc - prefer PATH, then the installed SDK, then known versions
where glslc >nul 2>&1
if %errorlevel% equ 0 (
    set GLSLC=glslc
) else if exist "%VULKAN_SDK%\Bin\glslc.exe" (
    set GLSLC=%VULKAN_SDK%\Bin\glslc.exe
) else (
    echo ERROR: Could not find glslc.exe
    echo Install the Vulkan SDK, or set VULKAN_SDK
    popd
    exit /b 1
)

echo Using glslc: %GLSLC%

REM The renderer draws with exactly one pipeline, so there are exactly two
REM shaders. Both are transpiled from C templates, which keeps them testable as
REM plain C and - more importantly - keeps them in version control, which is
REM where the vertex shader went missing from once already.

echo Transpiling triangle_vertex_template.c to triangle.vert...
python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\triangle_vertex_template.c %SRC_VERTEX%\triangle.vert
if %errorlevel% neq 0 (
    echo ERROR: Failed to transpile triangle vertex template
    popd
    exit /b 1
)

echo Transpiling triangle_fragment_template.c to triangle.frag...
python %TOOLS%\extract_simple_glsl.py %SRC_TEMPLATES%\triangle_fragment_template.c %SRC_FRAGMENT%\triangle.frag
if %errorlevel% neq 0 (
    echo ERROR: Failed to transpile triangle fragment template
    popd
    exit /b 1
)

echo Compiling triangle.vert...
"%GLSLC%" %SRC_VERTEX%\triangle.vert -o triangle.vert.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile triangle.vert
    popd
    exit /b 1
)

echo Compiling triangle.frag...
"%GLSLC%" %SRC_FRAGMENT%\triangle.frag -o triangle.frag.spv
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile triangle.frag
    popd
    exit /b 1
)

echo.
echo Shaders compiled successfully!
dir *.spv /b
popd
