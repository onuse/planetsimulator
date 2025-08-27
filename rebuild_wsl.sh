#!/bin/bash
# Build script for WSL/Linux environments

echo "Building Octree Planet Simulator (WSL/Linux)..."
echo

# Use a separate build directory for WSL
BUILD_DIR="build_wsl"

# Always clean and rebuild shaders first
echo "Cleaning and rebuilding shaders..."
if [ -d "shaders" ]; then
    cd shaders
    
    # Clean all generated files
    echo "Deleting all .frag and .spv files..."
    rm -f *.spv 2>/dev/null
    rm -f src/fragment/*.frag 2>/dev/null
    rm -f ../$BUILD_DIR/bin/shaders/*.spv 2>/dev/null
    
    # Compile shaders
    echo "Compiling shaders..."
    if [ -f "compile.bat" ]; then
        # Convert Windows batch script to bash commands
        python tools/c2glsl.py || { echo "Shader compilation failed!"; cd ..; exit 1; }
        
        # Compile SPIR-V files
        for shader in src/vertex/*.vert src/fragment/*.frag src/compute/*.comp; do
            if [ -f "$shader" ]; then
                output=$(basename "$shader").spv
                glslc "$shader" -o "$output" || { echo "Failed to compile $shader"; cd ..; exit 1; }
            fi
        done
    fi
    echo "Shaders compiled successfully!"
    
    cd ..
fi

# Clean build directory if requested
if [ "$1" == "clean" ]; then
    echo "Cleaning WSL build directory..."
    rm -rf $BUILD_DIR
fi

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Force CMake reconfiguration to detect new files (GLOB_RECURSE workaround)
if [ -f "CMakeCache.txt" ]; then
    echo "Touching CMakeCache.txt to ensure file changes are detected..."
    touch CMakeCache.txt
fi

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    cd ..
    exit 1
fi

# Build
echo
echo "Building..."
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Build failed!"
    cd ..
    exit 1
fi

echo
echo "Build complete!"

# Return to root directory to copy shaders
cd ..

# Copy shaders to build directory
echo "Copying shaders to build directory..."
mkdir -p $BUILD_DIR/bin/shaders
cp shaders/*.spv $BUILD_DIR/bin/shaders/ 2>/dev/null
if [ $? -ne 0 ]; then
    echo "WARNING: Failed to copy some shaders"
fi

# Go back to build directory for tests
cd $BUILD_DIR

# Run tests automatically
echo
echo "Running tests..."
ctest --output-on-failure
if [ $? -ne 0 ]; then
    echo
    echo "WARNING: Some tests failed"
    # Don't exit on test failure, just warn
fi

echo
echo "Executable: $BUILD_DIR/bin/PlanetSimulator"
echo
echo "Run with: $BUILD_DIR/bin/PlanetSimulator"
echo "Or with: ./run_wsl.sh -auto-terminate 60 -screenshot-interval 10"
cd ..

exit 0