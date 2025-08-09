# TODO - Planet Simulator

## Current Status (2024-08-09)

### What's Working ✅
- **Transvoxel algorithm** - Generates valid meshes from voxel data (tests pass)
- **Vertex deduplication** - Fixed 5x vertex overhead, now shares vertices properly (57% reduction)
- **Octree initialization** - Proper error handling instead of silent failures
- **Mesh generation** - 352,295 triangles being generated across 81 chunks
- **Draw calls** - Being issued correctly (no Vulkan validation errors)
- **Test coverage** - Comprehensive tests for mesh connectivity and algorithms

### The Problem 🔴
- **Planet renders as scattered white dots** instead of solid globe
- Despite valid mesh generation and draw calls, triangles appear disconnected on screen
- UI stats now show correct triangle count (was showing 0 due to wrong variable)

## Immediate Next Steps (Priority Order)

### 1. RenderDoc Investigation 🔍
**Why:** Visual debugging will show exactly what's happening on GPU
**What to check:**
- [ ] Capture frame with F12
- [ ] **Mesh Output tab** - Are triangles actually connected or separate?
- [ ] **Pipeline State** - Verify `VK_POLYGON_MODE_FILL` not `POINT` or `LINE`
- [ ] **Vertex Input** - Check if vertex positions are correct
- [ ] **VS Output** - Verify transforms aren't placing all vertices at same position
- [ ] **Draw calls** - Confirm `vkCmdDrawIndexed` with correct index counts

**Likely issues:**
- Pipeline state wrong (polygon mode, primitive topology)
- Vertex buffer format mismatch
- Index buffer type wrong (uint16 vs uint32)
- Transform matrices producing degenerate triangles

### 2. After RenderDoc Findings
Based on what RenderDoc reveals, fix the specific GPU pipeline issue:
- [ ] Fix pipeline state if needed
- [ ] Fix vertex/index buffer formats
- [ ] Fix shader transforms
- [ ] Verify depth testing and culling settings

### 3. Visual Improvements (Once Rendering Works)
- [ ] Add material colors from MixedVoxel system
- [ ] Implement basic lighting/shading
- [ ] Add atmospheric rendering
- [ ] Implement LOD transition cells (prevent cracks between detail levels)

## Known Technical Debt

### Architecture
- ✅ Removed parallel rendering paths (SimpleSurfaceExtractor, DebugRenderer)
- ✅ Single implementation path: TransvoxelRenderer only
- ⚠️ LOD transition cells not implemented (will cause cracks at boundaries)

### Performance
- Vertex deduplication working but could be optimized further
- No GPU-based mesh generation yet (all CPU)
- No mesh caching system

## Test Results Summary
```
test_transvoxel_algorithm: PASS (algorithm correct)
test_mesh_connectivity: PASS (vertices properly shared)
test_chunk_boundaries: PASS (chunks align correctly)
Vertex reduction: 1395 -> 599 vertices (57% improvement)
Triangle generation: 352,295 triangles from 81 chunks
```

## Files Recently Modified
- `src/algorithms/mesh_generation.cpp` - Added vertex deduplication
- `src/rendering/transvoxel_renderer.cpp` - Fixed octree validation
- `src/rendering/imgui_manager.cpp` - Fixed stats display
- `include/rendering/vulkan_renderer.hpp` - Added getTriangleCount()

## Build & Run Commands
```batch
# Build
cmd.exe /c "build.bat"

# Quick rebuild
cmd.exe /c "cmake --build build --config Release --target OctreePlanet"

# Run with screenshots
build\bin\Release\OctreePlanet.exe -auto-terminate 5 -screenshot-interval 2

# Run with Vulkan validation
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
build\bin\Release\OctreePlanet.exe
```

## RenderDoc Setup
1. Download from https://renderdoc.org/
2. File → Launch Application → OctreePlanet.exe
3. Press F12 to capture frame
4. Check Mesh Output, Pipeline State, VS Input/Output

## Notes from Today's Session
- Parallel rendering paths were causing confusion (now cleaned)
- Test-driven development successfully identified vertex sharing issues
- The core Transvoxel algorithm is working correctly
- Issue is definitely in GPU pipeline, not mesh generation
- Stats display was showing wrong variables (fixed)

## Success Criteria
- [ ] Planet renders as solid globe, not scattered dots
- [ ] Can see clear spherical shape
- [ ] Triangles form continuous surface
- [ ] Performance stays above 30 FPS