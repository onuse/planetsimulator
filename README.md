# Planet Simulator

An ambitious real-time planet renderer using C++, Vulkan, and advanced LOD techniques.

## Features

- **Planetary-scale rendering** - Seamless rendering from space to surface
- **Cube-to-sphere mapping** - 6 cube faces projected to sphere for uniform sampling
- **Hierarchical LOD system** - SphericalQuadtree for far views, Octree for near surface
- **Transvoxel algorithm** - Smooth voxel-to-mesh conversion
- **Real-time performance** - 60+ FPS on modern GPUs

## Building

### Prerequisites
- Visual Studio 2022 or later
- Vulkan SDK 1.3+
- CMake 3.20+
- Windows 10/11

### Quick Build
```bash
# Full rebuild with tests
./rebuild.bat

# Simple build
./build.bat

# Build without tests
./build_simple.bat
```

## Running

```bash
# Basic run
./run.ps1

# With parameters
./run.ps1 -radius 6371000 -max-depth 10 -screenshot-interval 5

# Available options:
#   -radius <meters>          Planet radius (default: 6371000)
#   -max-depth <depth>        Octree max depth (default: 10)
#   -seed <seed>             Random seed (default: 42)
#   -width <pixels>          Window width (default: 1280)
#   -height <pixels>         Window height (default: 720)
#   -auto-terminate <sec>    Exit after N seconds
#   -screenshot-interval <s>  Screenshot interval
#   -vertex-dump            Dump mesh data on exit
#   -quiet                  Suppress verbose output
```

## Controls

- **WASD** - Move camera
- **Q/E** - Move up/down
- **Mouse** - Look around (right-click + drag)
- **Scroll** - Zoom in/out
- **R** - Reset camera
- **P** - Take screenshot
- **TAB** - Toggle orbital/free-fly camera
- **ESC** - Exit

## Project Structure

```
planetsimulator/
├── build/              # Build output
├── docs/               # Documentation
│   ├── architecture/   # System design
│   ├── debugging/      # Debug guides
│   └── fixes/         # Solution docs
├── include/           # Header files
├── scripts/           # Build & utility scripts
│   ├── analysis/      # Analysis tools
│   └── testing/       # Test runners
├── shaders/           # GLSL shaders
├── src/               # Source code
├── tests/             # Test suite
└── debug_output/      # Debug files (gitignored)
```

## Key Components

### Rendering Pipeline
- **SphericalQuadtree** - Manages planet-scale LOD
- **CPUVertexGenerator** - Generates mesh vertices on CPU
- **VulkanRenderer** - Vulkan rendering backend
- **LODManager** - Switches between quadtree/octree based on altitude

### Core Systems
- **Octree** - Spatial data structure for voxels
- **DensityField** - Procedural terrain generation
- **MaterialTable** - Material properties system
- **Camera** - Orbital and free-fly camera modes

## Recent Fixes

1. **Missing Cube Faces** - Fixed backface culling issue with X faces
2. **Depth Precision** - Implemented depth bias to prevent z-fighting at planetary scale
3. **Transform Precision** - Fixed MIN_RANGE bug causing microscopic patches

## Documentation

See the `docs/` folder for detailed documentation:
- `docs/architecture/` - System design and architecture
- `docs/debugging/` - Debugging guides and diagnostics
- `docs/fixes/` - Solutions to specific issues

## Testing

The project includes a comprehensive test suite:
```bash
# Run all tests
./scripts/testing/run_diagnostic_tests.ps1

# Tests are automatically run during rebuild
./rebuild.bat
```

## License

This project is a personal learning project. See CLAUDE.md for AI assistant instructions.

## Contact

For issues or questions, please update the documentation as needed.