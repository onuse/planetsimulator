#!/bin/bash
# Run script for WSL/Linux builds

BUILD_DIR="build_wsl"

if [ ! -f "$BUILD_DIR/bin/PlanetSimulator" ]; then
    echo "Executable not found! Building first..."
    ./rebuild_wsl.sh
fi

echo "Running PlanetSimulator (WSL build)..."
$BUILD_DIR/bin/PlanetSimulator "$@"