@echo off
echo === SHADER VERIFICATION TEST ===
echo.

echo Checking vertex shader for material type attribute...
findstr /C:"layout(location = 5) in uint instanceMaterialType" ..\..\shaders\vertex\hierarchical.vert
if %errorlevel% neq 0 (
    echo ERROR: Vertex shader missing instanceMaterialType at location 5!
    exit /b 1
)
echo [OK] Vertex shader has material type input

echo.
echo Checking vertex shader passes material to fragment...
findstr /C:"fragMaterialType = instanceMaterialType" ..\..\shaders\vertex\hierarchical.vert
if %errorlevel% neq 0 (
    echo ERROR: Vertex shader not passing material type to fragment shader!
    exit /b 1
)
echo [OK] Vertex shader passes material type

echo.
echo Checking fragment shader receives material type...
findstr /C:"layout(location = 3)" ..\..\shaders\fragment\hierarchical.frag | findstr /C:"flat" | findstr /C:"uint"
if %errorlevel% neq 0 (
    echo ERROR: Fragment shader not receiving uint material type at location 3!
    exit /b 1
)
echo [OK] Fragment shader receives material type

echo.
echo Checking compiled shader dates...
dir ..\..\shaders\*.spv | findstr /C:"hierarchical"
echo.
echo If dates are old, shaders need recompilation!

echo.
echo Checking shader binary sizes...
for %%F in (..\..\shaders\hierarchical.vert.spv) do echo Vertex shader size: %%~zF bytes
for %%F in (..\..\shaders\hierarchical.frag.spv) do echo Fragment shader size: %%~zF bytes

echo.
echo === VERIFICATION COMPLETE ===
echo All shader source files have correct declarations.
echo Now verify the compiled .spv files are up to date!