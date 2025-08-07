@echo off
REM Simple C to GLSL transpiler for shader development
REM Usage: c2glsl.bat input.c output.frag

setlocal

set INPUT=%1
set OUTPUT=%2

if "%INPUT%"=="" (
    echo Usage: c2glsl.bat input.c output.frag
    exit /b 1
)

if "%OUTPUT%"=="" (
    echo Usage: c2glsl.bat input.c output.frag
    exit /b 1
)

echo Transpiling %INPUT% to %OUTPUT%...

REM Use PowerShell to extract GLSL sections and remove C-only sections
powershell -Command ^
    "$content = Get-Content '%INPUT%' -Raw;" ^
    "$glsl = '#version 450' + [Environment]::NewLine + [Environment]::NewLine;" ^
    "if ($content -match '// GLSL_HEADER_START(.*)// GLSL_HEADER_END') {" ^
    "    $header = $matches[0] -replace '// GLSL_HEADER_START', '' -replace '// GLSL_HEADER_END', '';" ^
    "    $header = $header -replace '#ifdef GLSL[\s\S]*?#endif', '';" ^
    "    $glsl += $header + [Environment]::NewLine;" ^
    "}" ^
    "$shared = $content -replace '// C_HEADER_START[\s\S]*?// C_HEADER_END', '';" ^
    "$shared = $shared -replace '// C_TEST_START[\s\S]*?// C_TEST_END', '';" ^
    "$shared = $shared -replace '// GLSL_HEADER_START[\s\S]*?// GLSL_HEADER_END', '';" ^
    "$shared = $shared -replace '// GLSL_MAIN_START[\s\S]*?// GLSL_MAIN_END', '';" ^
    "$shared = $shared -replace '#ifndef GLSL[\s\S]*?#endif', '';" ^
    "$shared = $shared -replace '/\*\*[\s\S]*?\*/', '';" ^
    "$glsl += $shared;" ^
    "if ($content -match '// GLSL_MAIN_START(.*)// GLSL_MAIN_END') {" ^
    "    $main = $matches[0] -replace '// GLSL_MAIN_START', '' -replace '// GLSL_MAIN_END', '';" ^
    "    $main = $main -replace '#ifdef GLSL[\s\S]*?#endif', '';" ^
    "    $glsl += [Environment]::NewLine + $main;" ^
    "}" ^
    "$glsl | Out-File -FilePath '%OUTPUT%' -Encoding ASCII"

if %ERRORLEVEL% EQU 0 (
    echo Successfully created %OUTPUT%
) else (
    echo Failed to transpile %INPUT%
    exit /b 1
)

endlocal