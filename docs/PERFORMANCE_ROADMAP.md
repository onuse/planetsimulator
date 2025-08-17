# Performance Optimization Roadmap

## Current State
- **FPS**: <1 FPS when zooming in, 10-30 FPS at distance
- **Problem**: Generating 21 MILLION vertices (1.3GB) every frame on CPU
- **Root Causes**: 
  - 65×65 vertex grid per patch (4,225 vertices)
  - Up to 5,000 patches when close
  - All vertices regenerated every frame
  - Everything done on CPU

## Phase 1: Reduce Vertex Count (IMMEDIATE RELIEF)
### Step 1.1: Reduce Grid Resolution ✅ COMPLETED
- [x] Change grid resolution from 65×65 to 33×33 (1,089 vertices per patch)
- **Impact**: 4x reduction in vertices (21M → 5.4M)
- **Result**: Build successful, tests passing
- **Next**: Test visual quality and performance improvement

### Step 1.2: Reduce Grid Further if Needed
- [ ] If still slow, try 17×17 (289 vertices per patch)
- **Impact**: 15x reduction from original
- **Verification**: Visual quality check, ensure no gaps between patches

## Phase 2: Optimize Vertex Generation
### Step 2.1: Skip Unchanged Patches
- [ ] Only regenerate vertices for patches that changed LOD level
- [ ] Keep persistent vertex buffers for stable patches
- **Impact**: 90% reduction in vertex generation after initial frame
- **Verification**: Add counter for patches regenerated vs reused

### Step 2.2: Optimize Buffer Updates
- [ ] Only upload changed portions of vertex buffer
- [ ] Use persistent mapped buffers to avoid CPU→GPU copies
- **Impact**: Eliminate 1.3GB/frame memory transfer
- **Verification**: Monitor GPU memory usage, ensure no corruption

## Phase 3: Smarter LOD
### Step 3.1: Distance-Based Vertex Density
- [ ] Use 17×17 for far patches, 33×33 for near
- [ ] Adaptive grid based on screen space error
- **Impact**: Reduce vertex count for distant patches
- **Verification**: Check LOD transitions are smooth

### Step 3.2: More Aggressive LOD Limits
- [ ] Reduce max patches from 5,000 to 1,000
- [ ] Increase minimum patch size
- [ ] Earlier LOD cutoff based on distance
- **Impact**: Hard cap on worst-case performance
- **Verification**: Ensure no "popping" at boundaries

## Phase 4: GPU-Side Generation (MAJOR CHANGE)
### Step 4.1: Move Terrain Height to GPU
- [ ] Use compute shader for height calculation
- [ ] Generate vertices in vertex shader
- **Impact**: Eliminate CPU vertex generation entirely
- **Verification**: Compare GPU vs CPU generated terrain

### Step 4.2: GPU Tessellation
- [ ] Use tessellation shaders for adaptive detail
- [ ] Control tessellation factor based on distance
- **Impact**: Dynamic vertex density without CPU work
- **Verification**: Test on different GPU architectures

## Phase 5: Culling and Optimization
### Step 5.1: Frustum Culling
- [ ] Don't generate patches outside view frustum
- [ ] Early-exit quadtree traversal for off-screen branches
- **Impact**: 50% reduction for typical view
- **Verification**: Rotate camera 360°, ensure no missing patches

### Step 5.2: Occlusion Culling
- [ ] Skip patches on far side of planet
- [ ] Use hierarchical Z-buffer for occlusion
- **Impact**: Another 40-50% reduction
- **Verification**: Orbit planet, check all sides render

### Step 5.3: Re-enable Optimizations
- [ ] Fix triangle winding order
- [ ] Re-enable backface culling
- [ ] Optimize depth buffer precision
- **Impact**: 2x reduction in fragment processing
- **Verification**: Check all 6 cube faces visible

## Testing Protocol
After each step:
1. Run with `-auto-terminate 5 -screenshot-interval 2`
2. Check FPS counter
3. Verify no visual artifacts
4. Test zoom from space to surface
5. Rotate camera 360° at multiple altitudes
6. Check memory usage doesn't grow

## Success Metrics
- **Target**: 60 FPS at all zoom levels
- **Vertex Budget**: <1M vertices per frame
- **Memory Transfer**: <100MB per frame
- **CPU Time**: <5ms for LOD updates

## Current Focus
**Start with Step 1.1: Reduce grid resolution to 33×33**
This alone should give us 4x performance improvement with minimal code changes.