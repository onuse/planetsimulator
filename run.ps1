Write-Host "Running Octree Planet Simulator..." -ForegroundColor Green
Push-Location $PSScriptRoot
try {
    & ".\build\bin\Release\OctreePlanet.exe" $args
} finally {
    Pop-Location
}