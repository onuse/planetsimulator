# Deprecated Files to Remove

## Summary
With the adaptive sphere implementation now working for both CPU and GPU pipelines, the old octree-based GPU compute pipeline and related files are deprecated and can be removed.

## Deprecated Compute Shaders
These old GPU compute shaders are no longer needed since we use `adaptive_sphere.comp`:

- `shaders/src/compute/sphere_generator.comp` - Old octree-based sphere generator
- `shaders/src/compute/mesh_generator.comp` - Old octree mesh generation  
- `shaders/src/compute/mesh_generator_simple.comp` - Simplified version, unused
- `shaders/src/compute/mesh_generator_simple_sphere.comp` - Another variant, unused
- `shaders/src/compute/mesh_generator_proper.comp` - Another variant, unused
- `shaders/src/compute/mesh_generator_fixed.comp` - Another variant, unused
- `shaders/src/compute/terrain_sphere.comp` - Old terrain implementation
- `shaders/src/compute/simple_sphere_single.comp` - Test shader, unused
- `shaders/src/compute/surface_points.comp` - Point cloud generation, unused
- `shaders/src/compute/octree_verify.comp` - Debug shader, likely unused
- `shaders/src/compute/test_sphere.comp` - Test shader
- `shaders/src/compute/test_simple.comp` - Test shader  
- `shaders/src/compute/test_triangle.comp` - Test shader

## Deprecated CPU Code
- `src/rendering/debug_cpu_mesh_reference_fast.cpp` - Old CPU reference implementation
- `src/rendering/vulkan_renderer_cpu_upload.cpp` - Old CPU upload path
- `src/rendering/gpu_octree_old.cpp.backup` - Backup file

## Deprecated Headers (from gitignore)
Based on the deleted files in git status:
- `include/rendering/cpu_vertex_generator.hpp` - Already deleted
- `include/rendering/hierarchical_gpu_octree.hpp` - Already deleted

## Code to Refactor
- `src/rendering/vulkan_renderer_mesh_compute.cpp` - Lines 30-358 contain the old octree-based GPU path that's no longer used

## Templates That May Be Deprecated
- `shaders/src/templates/mesh_generator_compute_template.c` - Old template for mesh_generator.comp
- `shaders/src/templates/mesh_generator_compute_simple_sphere.c` - Template for simple sphere
- `shaders/src/templates/surface_points_compute_template.c` - Template for surface points
- `shaders/src/templates/octree_verify_compute_template.c` - Template for octree verify

## KEEP These Files
- `adaptive_sphere.comp` - Current GPU implementation
- `generate_adaptive_sphere.cpp` - Current CPU implementation  
- Files related to the three active pipelines: CPU_ADAPTIVE, GPU_COMPUTE, GPU_WITH_CPU_VERIFY

## Recommendation
1. Remove all deprecated compute shaders listed above
2. Remove the old octree GPU path from `vulkan_renderer_mesh_compute.cpp` (lines 30-358)
3. Clean up unused shader templates
4. Remove test/debug shaders that aren't actively used
5. Update CMakeLists.txt to remove references to deleted files