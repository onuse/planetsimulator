# Performance Optimization Plan

## Current Performance Issues
- FPS: ~0.2-2 FPS (should be 60+)
- Frame time: 100-500ms (should be <16ms)

## Debug Code Identified

### 1. CRITICAL - Per-Frame Verbose Logging [COMPLETED]
**File: `src/rendering/cpu_vertex_generator.cpp`**
- Lines 626-660: Detailed patch analysis for EVERY patch - **DISABLED**
- Outputs transform matrices, corners, vertex stats - **DISABLED**
- **Impact**: MASSIVE - prints hundreds of lines per frame

**File: `src/rendering/lod_manager.cpp`**
- Multiple cout statements per frame - **DISABLED**
- Vertex generation stats - **DISABLED**
- Camera-relative transform logging - **DISABLED**
- **Impact**: HIGH - multiple outputs per frame

**File: `src/core/spherical_quadtree.cpp`**
- Face processing debug - **DISABLED**
- Subdivision logging - **DISABLED**
- Screen space error calculations - **DISABLED**
- SelectLOD warnings - **DISABLED**
- **Impact**: MEDIUM - many calculations logged

**File: `src/main.cpp`**
- Early frame debug - **DISABLED**
- Screenshot check logging - **DISABLED**
- Frame stats reduced from every 300 to 3000 frames - **REDUCED**
- **Impact**: LOW-MEDIUM - per-frame output

### 2. Disabled Optimizations
**Backface Culling**: DISABLED
- `vulkan_renderer_transvoxel.cpp`: `VK_CULL_MODE_NONE`
- Rendering both sides of all triangles
- **Impact**: 2x overdraw

**Clipping Planes**: Debug Override
- Near plane forced to 1.0 (should be dynamic)
- Far plane forced to 100 million
- **Impact**: Depth precision issues

### 3. Excessive Calculations
- Transform matrices calculated per vertex
- Debug statistics collected even when not used
- Verbose error checking on every operation

## Optimization Steps

### Phase 1: Disable Debug Logging (Quick Win) ✅ COMPLETED
1. ✅ Comment out debug output in `cpu_vertex_generator.cpp`
2. ✅ Disable LODManager verbose logging
3. ✅ Remove frame-by-frame debug output
4. ✅ Disable subdivision logging
5. ✅ Disable SelectLOD warnings
6. ✅ Reduce frame stats frequency

### Phase 2: Re-enable Graphics Optimizations
1. Fix triangle winding for X faces
2. Re-enable backface culling
3. Restore dynamic clipping planes
4. Remove depth bias once culling works

### Phase 3: Algorithmic Optimizations
1. Cache transform matrices
2. Reduce redundant calculations
3. Use instanced rendering where possible
4. Optimize vertex generation

## Expected Performance Gains
- Phase 1: 10-50x speedup (remove I/O bottleneck) - **IN PROGRESS - Testing needed**
- Phase 2: 2x speedup (reduce overdraw)
- Phase 3: 2-5x speedup (algorithmic improvements)

## Completion Status
- ✅ **Phase 1 Complete**: All verbose debug logging disabled
- ⏳ **Next Step**: Test performance improvement
- 📋 **Remaining**: Fix triangle winding, re-enable culling, optimize algorithms

## Files to Modify
Priority order:
1. `cpu_vertex_generator.cpp` - Remove patch analysis
2. `lod_manager.cpp` - Disable verbose logging
3. `spherical_quadtree.cpp` - Remove debug output
4. `main.cpp` - Remove frame debug
5. `vulkan_renderer_transvoxel.cpp` - Re-enable culling (after winding fix)