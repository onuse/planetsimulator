# Renderer Architecture

## Overview
Hybrid rendering system combining multiple techniques for optimal quality and performance.

## C++ Standard: C++17
- Modern features without compatibility issues
- std::optional, std::filesystem, structured bindings
- Excellent compiler support

## Rendering Pipeline

### 1. Hierarchical Rasterization (Primary Renderer)
**Purpose**: Main renderer for octree visualization
**Location**: `src/rendering/hierarchical_renderer.cpp`

- Instanced cube rendering for octree nodes
- GPU-driven culling and LOD selection
- Material-based coloring
- Supports millions of nodes at 60+ FPS

**When to use**:
- All visible octree nodes beyond detail distance
- Fast overview rendering
- Debug visualization

### 2. Marching Cubes (Detail Renderer)
**Purpose**: Smooth surface extraction for close-up views
**Location**: `src/rendering/marching_cubes_renderer.cpp`

- GPU compute shader implementation
- Only for nodes within detail radius (< 10km from camera)
- Generates smooth mesh from voxel data
- Cached mesh generation

**When to use**:
- Close-up surface views
- When visual quality matters more than speed
- Surface analysis modes

### 3. Ray Marching (Effects Renderer)
**Purpose**: Atmospheric and volumetric effects
**Location**: `src/rendering/atmosphere_renderer.cpp`

- Post-process screen-space pass
- Atmospheric scattering
- Clouds and weather effects
- Volumetric lighting

**When to use**:
- Always active as final post-process
- Adds realism to distant views
- Atmospheric effects

## Folder Structure

```
cpp/
├── src/
│   ├── core/               # Core data structures
│   │   ├── octree.cpp
│   │   ├── voxel.cpp
│   │   └── camera.cpp
│   ├── rendering/          # All rendering code
│   │   ├── vulkan_context.cpp
│   │   ├── hierarchical_renderer.cpp
│   │   ├── marching_cubes_renderer.cpp
│   │   ├── atmosphere_renderer.cpp
│   │   ├── render_pipeline.cpp
│   │   └── screenshot.cpp
│   ├── physics/           # Physics simulation
│   │   ├── thermal_simulation.cpp
│   │   ├── plate_tectonics.cpp
│   │   └── mantle_convection.cpp
│   ├── generation/        # World generation
│   │   ├── terrain_generator.cpp
│   │   ├── plate_generator.cpp
│   │   └── noise_functions.cpp
│   └── utils/            # Utilities
│       ├── thread_pool.cpp
│       ├── profiler.cpp
│       └── file_io.cpp
├── include/              # Headers (mirrors src structure)
│   ├── core/
│   ├── rendering/
│   ├── physics/
│   ├── generation/
│   └── utils/
├── shaders/
│   ├── compute/         # Compute shaders
│   │   ├── marching_cubes.comp
│   │   ├── physics_update.comp
│   │   └── octree_update.comp
│   ├── vertex/         # Vertex shaders
│   │   ├── hierarchical.vert
│   │   └── mesh.vert
│   ├── fragment/       # Fragment shaders
│   │   ├── hierarchical.frag
│   │   ├── mesh.frag
│   │   └── atmosphere.frag
│   └── include/        # Shared shader code
│       ├── common.glsl
│       └── noise.glsl
├── tests/              # Unit tests
├── docs/               # Documentation
├── assets/             # Resources (textures, configs)
└── build/             # Build output (gitignored)
```

## Rendering Modes

### 1. Performance Mode
- Hierarchical rasterization only
- Lowest quality, highest FPS
- For simulation focus

### 2. Balanced Mode (Default)
- Hierarchical + Marching cubes for nearby
- Good quality/performance trade-off
- Recommended for most use

### 3. Quality Mode
- Full marching cubes up to medium distance
- Ray-marched atmosphere with volumetrics
- Screenshot/video recording mode

## GPU Memory Management

### Buffer Layout
```cpp
// Shared GPU buffers between renderers
struct GPUBuffers {
    VkBuffer octreeNodes;        // All octree nodes
    VkBuffer voxelData;          // Voxel properties
    VkBuffer visibilityBuffer;   // Visible node indices
    VkBuffer instanceBuffer;     // For hierarchical rendering
    VkBuffer vertexBuffer;       // For marching cubes output
    VkBuffer indirectDrawBuffer; // GPU-driven rendering
};
```

### Memory Pools
- Octree Pool: 512 MB (dynamically grown)
- Mesh Cache Pool: 256 MB (LRU eviction)
- Compute Pool: 128 MB (physics simulation)
- Staging Pool: 64 MB (CPU-GPU transfers)

## Synchronization Strategy

### Frame Pipeline
1. **Update Phase** (CPU)
   - Update camera
   - Update LOD
   - Prepare visible nodes

2. **Compute Phase** (GPU)
   - Physics simulation
   - Marching cubes generation
   - Octree updates

3. **Render Phase** (GPU)
   - Hierarchical rasterization
   - Mesh rendering
   - Atmosphere pass
   - UI overlay

4. **Present Phase**
   - Screenshot capture (if requested)
   - Frame presentation

## Performance Targets

- **1080p**: 60+ FPS with balanced mode
- **4K**: 30+ FPS with balanced mode
- **Node capacity**: 10+ million visible nodes
- **Update rate**: 120+ physics updates/second
- **Memory usage**: < 4 GB VRAM, < 2 GB RAM

## Future Optimizations

1. **Mesh Instancing**: Instance repeated surface patterns
2. **Temporal Caching**: Reuse previous frame data
3. **Variable Rate Shading**: Lower shading rate for distant objects
4. **GPU Octree Generation**: Move more generation to GPU
5. **Multi-GPU Support**: Split rendering across GPUs