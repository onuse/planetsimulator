# Test Suite

## Clean, Working Tests

### Core Tests
- **test_octree_core.cpp** - Basic octree operations (creation, generation, rendering)
- **test_materials.cpp** - Material generation, blending, and averaging
- **test_gpu_rendering.cpp** - GPU rendering pipeline and data flow

### Specialized Tests (need verification)
- **test_frustum_culling.cpp** - Frustum culling optimization
- **test_hierarchical_octree.cpp** - Hierarchical octree structure
- **test_lod_selection.cpp** - Level of detail selection
- **test_mixed_voxel.cpp** - MixedVoxel type operations
- **test_mixed_voxel_integration.cpp** - MixedVoxel integration

## Test Results Summary

✅ **Working Tests:**
- test_octree_core: All 5 tests pass
- test_materials: All 5 tests pass (with known sparse voxel issue)
- test_gpu_rendering: All 5 tests pass

⚠️ **Known Issues Found:**
1. Sparse voxels (6 air + 2 material) average to air, causing black planet
2. Many nodes generated outside planet radius contain only air
3. Node material flags don't always match voxel contents

## Running Tests

```bash
# Compile
g++ -std=c++17 -I./include -I./third_party/glm tests/test_octree_core.cpp src/core/octree.cpp src/core/camera.cpp -pthread -o build/test_octree_core
g++ -std=c++17 -I./include -I./third_party/glm tests/test_materials.cpp src/core/octree.cpp src/core/camera.cpp -pthread -o build/test_materials
g++ -std=c++17 -I./include -I./third_party/glm tests/test_gpu_rendering.cpp src/core/octree.cpp src/core/camera.cpp -pthread -o build/test_gpu_rendering

# Run
./build/test_octree_core
./build/test_materials
./build/test_gpu_rendering
```

## Test Philosophy

1. **Simple is better** - Each test should test one thing
2. **Assertions matter** - Every test must have assertions
3. **Use public API** - Don't access private members
4. **Fast execution** - Tests should run quickly
5. **Clear output** - Show what's being tested and the result