# Claude Development Guide for Planet Simulator

This file provides guidance to Claude Code (claude.ai/code) when working with this C++ planet simulator project.

## Project Overview
C++ Vulkan-based planet simulator using hierarchical octree voxel rendering to simulate Earth-scale geological processes in real-time.

## Current State (December 2024)
- ✅ Basic planet rendering with octree voxels
- ✅ Material system (Rock, Water, Air - air is invisible)
- ✅ GPU instanced rendering (~40K nodes at 40+ FPS at depth 7)
- ✅ ImGui debug interface with performance metrics
- ✅ Screenshot system with auto-cleanup
- 🚧 Performance limited - need GPU octree for higher detail

## Critical Information

### Rendering Pipeline
⚠️ **IMPORTANT**: We use ONLY instance-based rendering. The ray marching pipeline has been disabled to avoid confusion. Do not enable dual pipelines until we have a proper LOD system.

### Performance Targets
- Depth 7: ~40K nodes @ 40-80 FPS ✅ (current default)
- Depth 8: ~166K nodes @ 15 FPS
- Depth 9: ~662K nodes @ 4 FPS (too slow)
- Target: 1M+ nodes @ 60 FPS (requires GPU octree - Phase 3.6 in ROADMAP)

### Known Issues
1. **Shader Caching**: Shaders are loaded from `shaders/` relative to working directory, not build directory
2. **Build Warnings**: CMake sometimes tries to copy old shaders - use compile.bat instead
3. **Performance**: CPU-based instance rendering hits limits at ~100K nodes

## Build Instructions

### Prerequisites
- Visual Studio 2022 with C++ development tools
- Vulkan SDK 1.4.321.1+ installed at `C:\VulkanSDK\`
- CMake 3.20+
- Git

### Building
```batch
# Clean build (recommended when shaders change)
rd /s /q build
build.bat

# Quick rebuild (C++ only)
cmake --build build --config Release --target OctreePlanet

# Shader compilation only (IMPORTANT: do this when changing shaders)
cd shaders
compile.bat
```

### Running
```batch
# Default settings (depth 7, 40+ FPS)
build\bin\Release\OctreePlanet.exe

# Higher detail (slower)
build\bin\Release\OctreePlanet.exe -max-depth 8

# With auto-terminate and screenshots
build\bin\Release\OctreePlanet.exe -auto-terminate 10 -screenshot-interval 2

# Quiet mode (less console output)
build\bin\Release\OctreePlanet.exe -quiet
```

## Code Structure

### Core Systems
- `src/core/octree.cpp` - Octree data structure and planet generation
- `src/core/camera.cpp` - Camera controls and matrices
- `src/rendering/vulkan_renderer.cpp` - Main renderer initialization
- `src/rendering/vulkan_renderer_resources.cpp` - Instance buffer management (CRITICAL)
- `src/rendering/vulkan_renderer_commands.cpp` - Command buffer recording
- `src/rendering/vulkan_renderer_pipeline.cpp` - Pipeline creation

### Shaders
- `shaders/hierarchical.vert/frag` - Main rendering shaders (ACTIVE)
- `shaders/octree_raymarch.vert/frag` - Ray marching shaders (DISABLED)
- `shaders/compile.bat` - Compiles all shaders to SPIR-V

### Key Functions
- `OctreePlanet::prepareRenderData()` - Collects visible nodes with frustum culling
- `VulkanRenderer::updateInstanceBuffer()` - Uploads node data to GPU (line 255)
- `OctreePlanet::generateTestSphere()` - Creates planet geometry with materials
- `OctreeNode::toGPUNode()` - Converts node to GPU format with material encoding

## Development Guidelines

### When Making Changes

1. **Always clean shaders when modifying them**:
   ```batch
   del shaders\*.spv
   del build\bin\Release\shaders\*.spv
   cd shaders
   compile.bat
   ```

2. **Test at different depths**:
   - Depth 7: Quick iteration (40+ FPS)
   - Depth 8: Quality/performance balance (15 FPS)
   - Depth 9+: Only for optimization testing

3. **Check material distribution**:
   ```
   Material distribution in 41708 visible nodes:
     Air: 7752 (skipped - invisible)
     Rock: 5051
     Water: 28905
     Magma: 0
   ```

4. **Monitor performance**:
   - FPS counter in ImGui window
   - Frame time graph shows spikes
   - Visible nodes count indicates culling efficiency

### Common Tasks

#### Adding New Materials
1. Add to `MaterialType` enum in `include/core/octree.hpp`
2. Update color mapping in `src/rendering/vulkan_renderer_resources.cpp` line 280
3. Modify generation in `generateTestSphere()` in `src/core/octree.cpp`
4. Update material filtering if needed (currently skips Air)

#### Improving Performance
1. **Immediate**: Adjust LOD threshold in `prepareRenderData()` line 670
2. **Short-term**: Improve frustum culling (line 637)
3. **Long-term**: Implement GPU octree (see ROADMAP Phase 3.6)

#### Debugging Rendering Issues
1. Check shader compilation succeeded: `compile.bat`
2. Verify instance buffer size: Look for "Creating/resizing instance buffer"
3. Check Vulkan validation: `set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`
4. Use RenderDoc for GPU debugging

## Next Priority Tasks (from ROADMAP.md)

### Immediate
1. ✅ Fix rendering pipeline (DONE - using instance-based only)
2. 🚧 Camera controls (WASD + mouse) - Phase 5
3. 📋 GPU octree implementation - Phase 3.6 (CRITICAL)

### Phase 3.6: GPU Octree (Next Major Task)
- Store octree in GPU buffer/texture
- Fragment shader traversal
- Target: 1M+ voxels at 60 FPS

### Phase 4: Terrain Generation
- Integrate FastNoise2
- Continental shelves
- Mountain ranges
- Biomes

## Performance Optimization

### Current Bottlenecks
- CPU preparing instance data for each node
- One instance per octree node (inefficient)
- No hierarchical LOD (all or nothing)
- Memory bandwidth: 32 bytes per instance

### Optimization Strategies
1. **Frustum culling**: Already implemented, tune aggressiveness
2. **Distance LOD**: Skip nodes < 0.5 pixels (line 670)
3. **Material filtering**: Skip air nodes (implemented)
4. **Batch similar nodes**: Merge adjacent same-material nodes
5. **GPU octree**: Ultimate solution (Phase 3.6)

## Testing Checklist
- [ ] Build completes without errors
- [ ] Shaders compile: `cd shaders && compile.bat`
- [ ] Planet renders with correct colors (blue ocean, brown land)
- [ ] FPS > 30 at default settings (depth 7)
- [ ] No red planet (ray marching disabled)
- [ ] Screenshots save to screenshot_dev/
- [ ] ImGui interface shows stats

## Common Errors and Solutions

### "Red Planet" Issue
**Symptom**: Planet renders solid red
**Cause**: Ray marching pipeline enabled with debug mode
**Solution**: Ensure `useGPUOctree = false` in vulkan_renderer.cpp line 69

### Low FPS
**Symptom**: < 30 FPS at depth 7
**Solution**: 
1. Check LOD threshold in octree.cpp line 670
2. Verify air nodes are skipped (line 276 vulkan_renderer_resources.cpp)
3. Reduce max depth with `-max-depth 6`

### Shader Not Updating
**Symptom**: Changes to shaders don't appear
**Solution**: 
```batch
del shaders\*.spv
del build\bin\Release\shaders\*.spv
cd shaders
compile.bat
cd ..
build\bin\Release\OctreePlanet.exe
```

### Build Errors
**Symptom**: CMake or MSBuild errors
**Solution**:
```batch
rd /s /q build
build.bat
```

## Vulkan Debugging

### Enable Validation Layers
```batch
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
set VK_LAYER_PATH=C:\VulkanSDK\1.4.321.1\Bin
build\bin\Release\OctreePlanet.exe
```

### Common Validation Errors
- `vkDestroyBuffer: Invalid device`: Buffer cleanup issue (non-critical)
- Semaphore warnings: Swapchain synchronization (can ignore)

## Project Philosophy
- **Single rendering path**: Don't add complexity until needed
- **Profile first**: Measure before optimizing
- **Incremental progress**: Small working changes over big broken ones
- **Visual debugging**: Use ImGui for runtime inspection

## Remember
- Always test at multiple octree depths
- Clean shaders when they don't update
- Keep instance rendering until GPU octree ready
- Document any new systems in this file
- Check ROADMAP.md for long-term direction