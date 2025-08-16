@echo off
setlocal enabledelayedexpansion

echo ====================================
echo Planet Simulator Diagnostic Runner
echo ====================================
echo.

:: Create results directory
set RESULTS_DIR=diagnostic_results
if not exist %RESULTS_DIR% mkdir %RESULTS_DIR%

:: Generate timestamp
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set TIMESTAMP=%datetime:~0,8%_%datetime:~8,6%
set RESULTS_FILE=%RESULTS_DIR%\test_results_%TIMESTAMP%.txt
set SUMMARY_FILE=%RESULTS_DIR%\summary_%TIMESTAMP%.txt

:: Build paths
set BUILD_DIR=build
set BIN_DIR=%BUILD_DIR%\bin\Release
set TEST_DIR=%BUILD_DIR%\tests\Release

echo Test directories:
echo   Build: %BUILD_DIR%
echo   Bin: %BIN_DIR%
echo   Tests: %TEST_DIR%
echo.

:: Check if build exists
if not exist %BUILD_DIR% (
    echo Build directory not found! Please build the project first.
    echo Run: rebuild.bat
    exit /b 1
)

:: Initialize results file
echo Diagnostic Test Results > %RESULTS_FILE%
echo ======================== >> %RESULTS_FILE%
echo Timestamp: %TIMESTAMP% >> %RESULTS_FILE%
echo. >> %RESULTS_FILE%

:: Counter variables
set /a TOTAL_TESTS=0
set /a PASSED_TESTS=0
set /a FAILED_TESTS=0

echo Starting diagnostic test suite...
echo.

:: PHASE 1: Problem Confirmation
echo === PHASE 1: Problem Confirmation ===
echo.
echo PHASE 1: Problem Confirmation >> %RESULTS_FILE%
echo. >> %RESULTS_FILE%

:: Test: Face Culling
echo Running: test_face_culling
echo   Purpose: Check if face culling is causing missing faces
set TEST_PATH=%TEST_DIR%\test_face_culling.exe
if not exist !TEST_PATH! set TEST_PATH=%BIN_DIR%\test_face_culling.exe
if exist !TEST_PATH! (
    echo TEST: test_face_culling >> %RESULTS_FILE%
    !TEST_PATH! >> %RESULTS_FILE% 2>&1
    if !errorlevel! == 0 (
        echo   [PASS]
        set /a PASSED_TESTS+=1
    ) else (
        echo   [FAIL] Exit code: !errorlevel!
        set /a FAILED_TESTS+=1
    )
    echo ---------------------------------------- >> %RESULTS_FILE%
) else (
    echo   [SKIP] Test not found
)
set /a TOTAL_TESTS+=1

:: Test: Patch Coverage
echo Running: test_patch_coverage
echo   Purpose: Verify which faces have patch coverage
set TEST_PATH=%TEST_DIR%\test_patch_coverage.exe
if not exist !TEST_PATH! set TEST_PATH=%BIN_DIR%\test_patch_coverage.exe
if exist !TEST_PATH! (
    echo TEST: test_patch_coverage >> %RESULTS_FILE%
    !TEST_PATH! >> %RESULTS_FILE% 2>&1
    if !errorlevel! == 0 (
        echo   [PASS]
        set /a PASSED_TESTS+=1
    ) else (
        echo   [FAIL] Exit code: !errorlevel!
        set /a FAILED_TESTS+=1
    )
    echo ---------------------------------------- >> %RESULTS_FILE%
) else (
    echo   [SKIP] Test not found
)
set /a TOTAL_TESTS+=1

echo.

:: PHASE 2: Pipeline Isolation
echo === PHASE 2: Pipeline Isolation ===
echo.
echo PHASE 2: Pipeline Isolation >> %RESULTS_FILE%
echo. >> %RESULTS_FILE%

:: Test: Visual Output
echo Running: test_visual_output
echo   Purpose: Visual verification of output
set TEST_PATH=%TEST_DIR%\test_visual_output.exe
if not exist !TEST_PATH! set TEST_PATH=%BIN_DIR%\test_visual_output.exe
if exist !TEST_PATH! (
    echo TEST: test_visual_output >> %RESULTS_FILE%
    !TEST_PATH! >> %RESULTS_FILE% 2>&1
    if !errorlevel! == 0 (
        echo   [PASS]
        set /a PASSED_TESTS+=1
    ) else (
        echo   [FAIL] Exit code: !errorlevel!
        set /a FAILED_TESTS+=1
    )
    echo ---------------------------------------- >> %RESULTS_FILE%
) else (
    echo   [SKIP] Test not found
)
set /a TOTAL_TESTS+=1

echo.

:: PHASE 3: Run main app with vertex dump
echo === PHASE 3: Main Application Test ===
echo.
echo Running main application with vertex dump...
%BIN_DIR%\OctreePlanet.exe -vertex-dump -auto-terminate 2 -quiet

:: Analyze mesh dump if it exists
if exist mesh_debug_dump.txt (
    echo.
    echo Analyzing vertex dump...
    echo.
    echo VERTEX DUMP ANALYSIS: >> %RESULTS_FILE%
    findstr /C:"Face" mesh_debug_dump.txt >> %RESULTS_FILE%
    findstr /C:"Total vertices" mesh_debug_dump.txt >> %RESULTS_FILE%
    findstr /C:"Degenerate" mesh_debug_dump.txt >> %RESULTS_FILE%
    echo ---------------------------------------- >> %RESULTS_FILE%
)

echo.
echo ====================================
echo Test Execution Complete
echo ====================================
echo.

:: Generate summary
echo DIAGNOSTIC TEST SUMMARY > %SUMMARY_FILE%
echo ======================== >> %SUMMARY_FILE%
echo Timestamp: %TIMESTAMP% >> %SUMMARY_FILE%
echo Total Tests Run: %TOTAL_TESTS% >> %SUMMARY_FILE%
echo Passed: %PASSED_TESTS% >> %SUMMARY_FILE%
echo Failed: %FAILED_TESTS% >> %SUMMARY_FILE%
echo. >> %SUMMARY_FILE%

echo Summary:
echo   Total Tests: %TOTAL_TESTS%
echo   Passed: %PASSED_TESTS%
echo   Failed: %FAILED_TESTS%
echo.
echo Results saved to:
echo   Full results: %RESULTS_FILE%
echo   Summary: %SUMMARY_FILE%
echo.

:: Check mesh dump for missing faces
if exist mesh_debug_dump.txt (
    echo Face Vertex Counts: >> %SUMMARY_FILE%
    findstr /C:"Face" mesh_debug_dump.txt | findstr /C:"vertices:" >> %SUMMARY_FILE%
    echo. >> %SUMMARY_FILE%
    
    echo Checking for missing faces in vertex dump...
    findstr /C:"vertices: 0" mesh_debug_dump.txt > nul
    if !errorlevel! == 0 (
        echo   WARNING: Some faces have 0 vertices!
        echo WARNING: Some faces have 0 vertices! >> %SUMMARY_FILE%
    ) else (
        echo   All faces have vertices.
    )
)

echo.
echo Next Steps:
echo 1. Review full results in: %RESULTS_FILE%
echo 2. Check summary in: %SUMMARY_FILE%
echo 3. If faces are missing, check mesh_debug_dump.txt
echo 4. Run with RenderDoc for GPU-level debugging

endlocal