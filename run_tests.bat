@echo off
setlocal

echo Building tests...

REM Create test build directory
if not exist build_tests mkdir build_tests
cd build_tests

REM Configure with CMake
cmake ../tests -G "Visual Studio 17 2022" -A x64

REM Build tests
cmake --build . --config Release

REM Run tests
echo.
echo Running Material Pipeline Tests...
Release\test_material_pipeline.exe

echo.
echo Done!
pause