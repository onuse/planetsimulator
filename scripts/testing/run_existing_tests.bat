@echo off
setlocal enabledelayedexpansion

echo ====================================
echo Running Existing Tests for Diagnosis
echo ====================================
echo.

:: Create results directory
set RESULTS_DIR=diagnostic_results
if not exist %RESULTS_DIR% mkdir %RESULTS_DIR%

:: Generate timestamp
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set TIMESTAMP=%datetime:~0,8%_%datetime:~8,6%
set RESULTS_FILE=%RESULTS_DIR%\existing_tests_%TIMESTAMP%.txt

:: Build paths
set BIN_DIR=build\bin\Release

echo Test Results > %RESULTS_FILE%
echo ============ >> %RESULTS_FILE%
echo. >> %RESULTS_FILE%

:: Test face boundary alignment (the one we fixed)
echo Running: test_face_boundary_alignment
echo Purpose: Check face boundary alignment (6 million meter gap issue)
%BIN_DIR%\test_face_boundary_alignment.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] Face boundaries align correctly
) else (
    echo   [FAIL] Face boundary issues detected
)
echo ---------------------------------------- >> %RESULTS_FILE%

:: Test frustum culling
echo Running: test_frustum_culling
echo Purpose: Check if frustum culling is hiding faces
%BIN_DIR%\test_frustum_culling.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] Frustum culling working
) else (
    echo   [FAIL] Frustum culling issues
)
echo ---------------------------------------- >> %RESULTS_FILE%

:: Test full pipeline
echo Running: test_full_pipeline
echo Purpose: End-to-end pipeline test
%BIN_DIR%\test_full_pipeline.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] Full pipeline working
) else (
    echo   [FAIL] Pipeline issues detected
)
echo ---------------------------------------- >> %RESULTS_FILE%

:: Test GPU rendering
echo Running: test_gpu_rendering
echo Purpose: Test GPU rendering pipeline
%BIN_DIR%\test_gpu_rendering.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] GPU rendering working
) else (
    echo   [FAIL] GPU rendering issues
)
echo ---------------------------------------- >> %RESULTS_FILE%

:: Test LOD selection
echo Running: test_lod_selection
echo Purpose: Check LOD selection logic
%BIN_DIR%\test_lod_selection.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] LOD selection working
) else (
    echo   [FAIL] LOD selection issues
)
echo ---------------------------------------- >> %RESULTS_FILE%

:: Test instance count
echo Running: test_instance_count
echo Purpose: Verify instance buffer counts
%BIN_DIR%\test_instance_count.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] Instance counts correct
) else (
    echo   [FAIL] Instance count issues
)
echo ---------------------------------------- >> %RESULTS_FILE%

:: Test draw calls
echo Running: test_draw_calls
echo Purpose: Verify draw call generation
%BIN_DIR%\test_draw_calls.exe >> %RESULTS_FILE% 2>&1
if !errorlevel! == 0 (
    echo   [PASS] Draw calls working
) else (
    echo   [FAIL] Draw call issues
)
echo ---------------------------------------- >> %RESULTS_FILE%

echo.
echo Test execution complete!
echo Results saved to: %RESULTS_FILE%
echo.

:: Analyze the log from main app run
echo Analyzing previous main app output...
echo.
echo Key findings from render log:
echo - ALL 6 faces are generating patches!
echo - Face 0: 16 patches
echo - Face 1: 55 patches  
echo - Face 2: 34 patches
echo - Face 3: 19 patches
echo - Face 4: 16 patches
echo - Face 5: 49 patches
echo - Total: 189 patches
echo.
echo This means faces are NOT missing at patch generation level!
echo The issue must be in vertex generation or GPU rendering.

endlocal