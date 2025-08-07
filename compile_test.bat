@echo off
echo Compiling material pipeline test...

cl.exe /std:c++17 /EHsc /O2 /I include /I build\_deps\glm-src ^
    tests\test_material_pipeline.cpp ^
    src\core\octree.cpp ^
    /Fe:test_material.exe ^
    /link /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% EQU 0 (
    echo Running test...
    test_material.exe
) else (
    echo Compilation failed!
)

pause