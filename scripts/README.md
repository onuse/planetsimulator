# Scripts Directory

This directory contains utility and build scripts for the Planet Simulator project.

## Python Scripts

- `analyze_tests.py` - Analyzes test results and generates reports
- `clean_cmake.py` - Cleans CMake build artifacts
- `clean_cmake_complete.py` - Complete CMake cleanup including caches

## Batch Scripts

- `compile_shaders.bat` - Compiles GLSL shaders to SPIR-V
- `compile_test.bat` - Compiles and runs tests
- `run_tests.bat` - Runs the test suite
- `test_hierarchical.bat` - Tests hierarchical rendering

## Note

The main build scripts (`build.bat`, `rebuild.bat`, `run.bat`) remain in the project root for easy access.
Shader compilation is primarily handled through `shaders/compile.bat`.