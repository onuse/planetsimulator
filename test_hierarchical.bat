@echo off
echo Running planet simulator with hierarchical GPU octree...
echo.
echo NOTE: Press 'H' in the application to toggle hierarchical octree mode
echo.
cd build\bin\Release
OctreePlanet.exe -auto-terminate 120
cd ..\..\..