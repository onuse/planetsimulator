@echo off
echo Compiling shaders...
"%VULKAN_SDK%\Bin\glslc.exe" shader.vert -o vert.spv
"%VULKAN_SDK%\Bin\glslc.exe" shader.frag -o frag.spv

echo Building C++ application with cl.exe directly...

REM Set up Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Compile with MSVC
cl.exe /std:c++17 /EHsc /O2 /MD /I"%VULKAN_SDK%\Include" main.cpp /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib /OUT:matrix_test.exe

echo Done! Run with: matrix_test.exe