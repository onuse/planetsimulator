# This script redirects to the Windows-specific executable
# For WSL/Linux builds, use run_wsl.sh instead

Write-Host "Running Planet Simulator (Windows build)..." -ForegroundColor Green
Push-Location $PSScriptRoot
try {
    & ".\build_windows\bin\Release\PlanetSimulator.exe" $args
} finally {
    Pop-Location
}