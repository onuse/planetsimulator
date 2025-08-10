# Transvoxel Implementation Plan

## Current State
- Using sphere patches that sample colors from voxels
- Creates smooth sphere, not actual terrain
- No representation of mountains, valleys, or surface features
- **CRITICAL: No elevation data exists in voxels - all materials are at planet radius**

## Goal
- Extract actual terrain surface from voxel data
- Show mountains, oceans, cliffs as 3D geometry
- Maintain good performance with LOD system

## Phase 0: Add Terrain Height Data (REQUIRED FIRST!)
Since voxels currently only have materials at fixed radius, we need to add actual terrain:
1. Modify octree generation to vary voxel placement by height
2. OR add elevation field to MixedVoxel struct
3. OR use material density to imply height (rock = high, water = low)

## Phase 1: Basic Marching Cubes (1-2 days)
1. Create new `transvoxel_generator.cpp` alongside current sphere generator
2. Implement basic marching cubes algorithm:
   - Sample 8 corners of voxel cells
   - Determine inside/outside based on voxel materials
   - Generate triangles using lookup table
3. Test with a single chunk near surface

## Phase 2: Density Field (1 day)
1. Create density field from octree:
   ```cpp
   float getDensity(vec3 pos) {
       auto voxel = octree->getVoxel(pos);
       if (voxel->isEmpty()) return 0.0f;  // Air
       if (voxel->isWater()) return 0.3f;  // Water surface
       return 1.0f;  // Solid ground
   }
   ```
2. Use altitude + materials for density
3. Handle water as separate surface level

## Phase 3: Chunk System (2-3 days)
1. Divide planet into chunks (32x32x32 voxels each)
2. Generate mesh per chunk:
   - Only for chunks crossing the isosurface
   - Cache generated meshes
   - Mark dirty when voxels change
3. Frustum cull at chunk level

## Phase 4: Integration (1 day)
1. Switch from sphere patches to transvoxel meshes
2. Update `VulkanRenderer::updateChunks()` to use new generator
3. Test performance and visual quality

## Phase 5: Optimizations
1. Vertex sharing between adjacent cells
2. Mesh simplification for distant chunks
3. Async mesh generation
4. GPU compute shader implementation (future)

## Key Differences from Current Approach

### Current (Sphere Patches):
```
Cube Face -> Project to Sphere -> Sample Colors -> Smooth Surface
```

### Target (Transvoxel):
```
Voxel Grid -> Density Field -> Marching Cubes -> Actual Terrain
```

## Expected Results
- Mountains will appear as actual geometry
- Oceans will be flat surfaces at sea level
- Cliffs and caves will be properly represented
- Performance similar to current (100k-500k triangles)

## Files to Create/Modify
1. `/src/algorithms/transvoxel_generator.cpp` - NEW
2. `/include/algorithms/transvoxel_generator.hpp` - NEW
3. `/src/algorithms/marching_cubes_tables.cpp` - NEW (lookup tables)
4. `/src/rendering/vulkan_renderer_transvoxel.cpp` - MODIFY
5. `/src/rendering/chunk_manager.cpp` - NEW

## Testing Strategy
1. Start with 2D slice visualization (debug mode)
2. Single chunk at planet surface
3. Full planet with LOD
4. Performance profiling

## References
- Eric Lengyel's Transvoxel Algorithm paper
- Paul Bourke's Marching Cubes tables
- Nvidia GPU Gems 3 Chapter 1 (terrain generation)