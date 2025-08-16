#!/usr/bin/env pwsh

# Diagnostic Test Runner for Planet Simulator
# Systematically runs priority tests to identify missing faces issue

Write-Host "====================================" -ForegroundColor Cyan
Write-Host "Planet Simulator Diagnostic Runner" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""

# Ensure we're in the correct directory
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot

# Create results directory
$resultsDir = "$projectRoot/diagnostic_results"
if (!(Test-Path $resultsDir)) {
    New-Item -ItemType Directory -Path $resultsDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultsFile = "$resultsDir/test_results_$timestamp.txt"
$summaryFile = "$resultsDir/summary_$timestamp.txt"

# Helper function to run a test and capture output
function Run-Test {
    param(
        [string]$TestName,
        [string]$TestPath,
        [string]$Phase,
        [string]$Purpose
    )
    
    Write-Host "Running: $TestName" -ForegroundColor Yellow
    Write-Host "  Purpose: $Purpose" -ForegroundColor Gray
    
    # Write to results file
    Add-Content -Path $resultsFile -Value "========================================="
    Add-Content -Path $resultsFile -Value "TEST: $TestName"
    Add-Content -Path $resultsFile -Value "PHASE: $Phase"
    Add-Content -Path $resultsFile -Value "PURPOSE: $Purpose"
    Add-Content -Path $resultsFile -Value "TIME: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    Add-Content -Path $resultsFile -Value "-----------------------------------------"
    
    # Check if test executable exists
    if (!(Test-Path $TestPath)) {
        Write-Host "  ❌ Test not found: $TestPath" -ForegroundColor Red
        Add-Content -Path $resultsFile -Value "ERROR: Test executable not found"
        Add-Content -Path $resultsFile -Value ""
        return $false
    }
    
    # Run the test
    $output = & $TestPath 2>&1
    $exitCode = $LASTEXITCODE
    
    # Write output to results file
    Add-Content -Path $resultsFile -Value $output
    Add-Content -Path $resultsFile -Value "-----------------------------------------"
    Add-Content -Path $resultsFile -Value "EXIT CODE: $exitCode"
    Add-Content -Path $resultsFile -Value ""
    
    # Display result
    if ($exitCode -eq 0) {
        Write-Host "  ✓ Passed" -ForegroundColor Green
        
        # Check for specific patterns in output
        if ($output -match "Missing faces: (\d+)") {
            Write-Host "  ! Missing faces detected: $($Matches[1])" -ForegroundColor Magenta
        }
        if ($output -match "Gap detected: ([\d.]+) meters") {
            Write-Host "  ! Gap detected: $($Matches[1]) meters" -ForegroundColor Magenta
        }
        return $true
    } else {
        Write-Host "  ✗ Failed (exit code: $exitCode)" -ForegroundColor Red
        
        # Extract key error info
        $errorLines = $output | Select-String -Pattern "error|fail|assert|missing" -SimpleMatch
        if ($errorLines) {
            Write-Host "  Error details:" -ForegroundColor Red
            $errorLines | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkRed }
        }
        return $false
    }
}

# Build paths
$buildDir = "$projectRoot/build"
$binDir = "$buildDir/bin/Release"
$testDir = "$buildDir/tests/Release"

Write-Host "Test directories:" -ForegroundColor Cyan
Write-Host "  Build: $buildDir"
Write-Host "  Bin: $binDir"
Write-Host "  Tests: $testDir"
Write-Host ""

# Check if build exists
if (!(Test-Path $buildDir)) {
    Write-Host "Build directory not found! Please build the project first." -ForegroundColor Red
    Write-Host "Run: ./rebuild.bat" -ForegroundColor Yellow
    exit 1
}

# Initialize counters
$totalTests = 0
$passedTests = 0
$failedTests = 0
$missingTests = 0

Write-Host "Starting diagnostic test suite..." -ForegroundColor Cyan
Write-Host ""

# PHASE 1: Problem Confirmation
Write-Host "=== PHASE 1: Problem Confirmation ===" -ForegroundColor Cyan
Write-Host ""

$phase1Tests = @(
    @{
        Name = "test_face_culling"
        Purpose = "Check if face culling is causing missing faces"
    },
    @{
        Name = "test_patch_coverage"
        Purpose = "Verify which faces have patch coverage"
    },
    @{
        Name = "test_capture_real_data"
        Purpose = "Dump actual rendering data"
    },
    @{
        Name = "test_check_coverage"
        Purpose = "Check for missing coverage areas"
    }
)

foreach ($test in $phase1Tests) {
    $testPath = "$testDir/$($test.Name).exe"
    if (!(Test-Path $testPath)) {
        $testPath = "$binDir/$($test.Name).exe"
    }
    
    $result = Run-Test -TestName $test.Name -TestPath $testPath -Phase "1" -Purpose $test.Purpose
    $totalTests++
    if ($result -eq $null) { $missingTests++ }
    elseif ($result) { $passedTests++ }
    else { $failedTests++ }
}

Write-Host ""

# PHASE 2: Pipeline Isolation
Write-Host "=== PHASE 2: Pipeline Isolation ===" -ForegroundColor Cyan
Write-Host ""

$phase2Tests = @(
    @{
        Name = "test_pipeline_isolation"
        Purpose = "Identify where in pipeline faces disappear"
    },
    @{
        Name = "test_pipeline_diagnostic"
        Purpose = "Full pipeline health check"
    },
    @{
        Name = "test_single_patch_render"
        Purpose = "Test if individual patches can render"
    },
    @{
        Name = "test_systematic_verification"
        Purpose = "Systematic verification of all components"
    }
)

foreach ($test in $phase2Tests) {
    $testPath = "$testDir/$($test.Name).exe"
    if (!(Test-Path $testPath)) {
        $testPath = "$binDir/$($test.Name).exe"
    }
    
    $result = Run-Test -TestName $test.Name -TestPath $testPath -Phase "2" -Purpose $test.Purpose
    $totalTests++
    if ($result -eq $null) { $missingTests++ }
    elseif ($result) { $passedTests++ }
    else { $failedTests++ }
}

Write-Host ""

# PHASE 3: Face-Specific Tests
Write-Host "=== PHASE 3: Face-Specific Tests ===" -ForegroundColor Cyan
Write-Host ""

$phase3Tests = @(
    @{
        Name = "test_xface_rendering"
        Purpose = "Test cross-face rendering"
    },
    @{
        Name = "test_cross_face_fix"
        Purpose = "Check cross-face issues"
    },
    @{
        Name = "test_face_boundary_solution"
        Purpose = "Test face boundary handling"
    },
    @{
        Name = "test_edge_mapping_fix"
        Purpose = "Verify edge mapping between faces"
    }
)

foreach ($test in $phase3Tests) {
    $testPath = "$testDir/$($test.Name).exe"
    if (!(Test-Path $testPath)) {
        $testPath = "$binDir/$($test.Name).exe"
    }
    
    $result = Run-Test -TestName $test.Name -TestPath $testPath -Phase "3" -Purpose $test.Purpose
    $totalTests++
    if ($result -eq $null) { $missingTests++ }
    elseif ($result) { $passedTests++ }
    else { $failedTests++ }
}

Write-Host ""

# PHASE 4: Visual/Debug Tests
Write-Host "=== PHASE 4: Visual/Debug Tests ===" -ForegroundColor Cyan
Write-Host ""

$phase4Tests = @(
    @{
        Name = "test_visual_output"
        Purpose = "Visual verification of output"
    },
    @{
        Name = "test_ascii_planet"
        Purpose = "ASCII visualization for debugging"
    },
    @{
        Name = "test_dot_diagnosis"
        Purpose = "Analyze dot patterns"
    },
    @{
        Name = "test_gap_analysis"
        Purpose = "Analyze gaps between faces"
    }
)

foreach ($test in $phase4Tests) {
    $testPath = "$testDir/$($test.Name).exe"
    if (!(Test-Path $testPath)) {
        $testPath = "$binDir/$($test.Name).exe"
    }
    
    $result = Run-Test -TestName $test.Name -TestPath $testPath -Phase "4" -Purpose $test.Purpose
    $totalTests++
    if ($result -eq $null) { $missingTests++ }
    elseif ($result) { $passedTests++ }
    else { $failedTests++ }
}

Write-Host ""
Write-Host "====================================" -ForegroundColor Cyan
Write-Host "Test Execution Complete" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""

# Generate summary
$summary = @"
========================================
DIAGNOSTIC TEST SUMMARY
========================================
Timestamp: $timestamp
Total Tests Run: $totalTests
Passed: $passedTests
Failed: $failedTests
Missing: $missingTests

Results saved to:
  Full results: $resultsFile
  Summary: $summaryFile

Key Findings:
"@

Write-Host $summary -ForegroundColor White
Add-Content -Path $summaryFile -Value $summary

# Analyze results for key patterns
Write-Host ""
Write-Host "Analyzing results for patterns..." -ForegroundColor Yellow

$results = Get-Content $resultsFile -Raw

# Check for missing faces
if ($results -match "Missing faces:.*?(\d+)") {
    $missingFaceCount = $Matches[1]
    $finding = "- Found $missingFaceCount missing faces"
    Write-Host $finding -ForegroundColor Magenta
    Add-Content -Path $summaryFile -Value $finding
}

# Check which faces are missing
if ($results -match "Face (\d+).*?0 patches") {
    $missingFaces = [regex]::Matches($results, "Face (\d+).*?0 patches")
    $faceList = $missingFaces | ForEach-Object { $_.Groups[1].Value }
    $finding = "- Faces with no patches: $($faceList -join ', ')"
    Write-Host $finding -ForegroundColor Magenta
    Add-Content -Path $summaryFile -Value $finding
}

# Check for gaps
if ($results -match "Gap detected: ([\d.]+) meters") {
    $gaps = [regex]::Matches($results, "Gap detected: ([\d.]+) meters")
    $maxGap = ($gaps | ForEach-Object { [double]$_.Groups[1].Value } | Measure-Object -Maximum).Maximum
    $finding = "- Maximum gap detected: $maxGap meters"
    Write-Host $finding -ForegroundColor Magenta
    Add-Content -Path $summaryFile -Value $finding
}

# Check for vertex issues
if ($results -match "Degenerate triangles: (\d+)") {
    $degenerateCount = $Matches[1]
    $finding = "- Found $degenerateCount degenerate triangles"
    Write-Host $finding -ForegroundColor Magenta
    Add-Content -Path $summaryFile -Value $finding
}

Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Green
Write-Host "1. Review full results in: $resultsFile" -ForegroundColor White
Write-Host "2. Check summary in: $summaryFile" -ForegroundColor White
Write-Host "3. Run the main application with logging:" -ForegroundColor White
Write-Host "   ./run.ps1 -vertex-dump -auto-terminate 5" -ForegroundColor Yellow
Write-Host "4. Check mesh_debug_dump.txt for face-specific data" -ForegroundColor White

# Also run a quick main app test with vertex dump
Write-Host ""
Write-Host "Running main application with vertex dump..." -ForegroundColor Cyan
& "$binDir/OctreePlanet.exe" -vertex-dump -auto-terminate 2 -quiet

if (Test-Path "mesh_debug_dump.txt") {
    Write-Host "Analyzing vertex dump..." -ForegroundColor Yellow
    $meshDump = Get-Content "mesh_debug_dump.txt" -Raw
    
    # Check for face data
    $facePattern = "Face (\d+).*?vertices: (\d+)"
    $faceData = [regex]::Matches($meshDump, $facePattern)
    
    Write-Host ""
    Write-Host "Vertex dump analysis:" -ForegroundColor Cyan
    foreach ($match in $faceData) {
        $faceId = $match.Groups[1].Value
        $vertexCount = $match.Groups[2].Value
        if ([int]$vertexCount -eq 0) {
            Write-Host "  Face $faceId`: NO VERTICES (MISSING!)" -ForegroundColor Red
        } else {
            Write-Host "  Face $faceId`: $vertexCount vertices" -ForegroundColor Green
        }
    }
}

Write-Host ""
Write-Host "Diagnostic run complete!" -ForegroundColor Green