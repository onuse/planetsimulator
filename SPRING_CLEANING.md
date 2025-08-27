# Spring Cleaning - Removing Dormant Code Paths

## Active Path (KEEP):
- `updateQuadtreeBuffersCPU` in LODManager
- `CPUVertexGenerator::generatePatchMesh`
- QUADTREE_ONLY render mode
- CPU vertex shader: `quadtree_patch_cpu.vert`

## To Remove:

### 1. GPU Compute Mesh Generation
- [ ] Remove `generateFullPlanetOnGPU()` function
- [ ] Remove `GPUMeshGeneration` struct  
- [ ] Remove `createGPUMeshGenerationPipeline()`
- [ ] Remove `destroyGPUMeshGenerationPipeline()`
- [ ] Delete all `mesh_generator_compute_*.c` templates except `simple_sphere`
- [ ] Remove `mesh_generator.comp` shader

### 2. Unused Vertex Generators
- [ ] Remove `SimpleVertexGenerator` class
- [ ] Remove `IVertexGenerator` interface
- [ ] Remove `VertexGeneratorSystem` class

### 3. Transvoxel/Octree Paths (if not using voxel data)
- [ ] Check if OCTREE_TRANSVOXEL mode is ever used
- [ ] Remove TransvoxelRenderer if unused
- [ ] Remove transvoxel shaders if unused

### 4. Unused Shader Templates
```
mesh_generator_compute_altitude.c
mesh_generator_compute_analyze.c
mesh_generator_compute_template.c
mesh_generator_compute_template_debug.c
mesh_generator_compute_template_fixed.c
mesh_generator_compute_template_octree.c
mesh_generator_compute_template_old.c
```

### 5. Test/Debug Code
- [ ] Remove TEST GRID generation
- [ ] Remove debug vertex dumping code
- [ ] Remove unused debug shaders

## After Cleanup:
- Single clear path: CPU vertex generation for quadtree patches
- Simplified shader pipeline
- Easier to debug and optimize