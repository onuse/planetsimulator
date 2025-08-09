# Renderer Architecture - Transvoxel Mesh Generation

## Overview
High-performance planet rendering using **Transvoxel mesh generation** from octree voxel data. 
NO instance rendering. NO raw voxel rendering. Just efficient triangle meshes.

## Core Principle
**Octree stores data → Transvoxel generates meshes → GPU renders triangles**

## Why Transvoxel?
- **Seamless LOD transitions** - No cracks between detail levels
- **Sharp features preserved** - Cliffs, rivers, ridges stay sharp
- **Proven at scale** - Used by Astroneer, Osiris: New Dawn, many others
- **Fast generation** - Can be done on GPU compute shaders
- **Works with MixedVoxel** - Material blending preserved

## Rendering Pipeline

### 1. Transvoxel Mesh Generation (Primary Renderer)
**Purpose**: Convert voxel data to optimized triangle meshes
**Location**: `src/rendering/transvoxel_renderer.cpp`

**LOD Strategy**:
- **LOD 0** (< 1km): Full resolution, all voxels sampled
- **LOD 1** (1-10km): Every 2nd voxel, simplified mesh
- **LOD 2** (10-100km): Every 4th voxel, reduced detail
- **LOD 3** (100km+): Every 8th voxel, minimal detail
- **Horizon**: Pre-computed sphere with height displacement

**Mesh Generation**:
```cpp
struct TransvoxelChunk {
    glm::vec3 position;
    float voxelSize;      // Changes with LOD
    uint32_t lodLevel;
    
    // Generated mesh data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Material from MixedVoxel
    std::vector<glm::vec3> vertexColors;  // From MixedVoxel::getColor()
};
```

**Performance**: 
- 100-500k triangles visible (60+ FPS)
- NOT 17 million cube instances (0.3 FPS)

### 2. Mesh Caching System
**Purpose**: Avoid regenerating unchanged meshes
**Location**: `src/rendering/mesh_cache.cpp`

```cpp
class MeshCache {
    // LRU cache of generated meshes
    std::unordered_map<ChunkID, MeshData> cache;
    
    // Dirty tracking for updates
    std::set<ChunkID> dirtyChunks;
    
    // Generate on demand, cache results
    MeshData& getMesh(ChunkID id) {
        if (dirty or not in cache) {
            generateTransvoxelMesh(id);
        }
        return cache[id];
    }
};
```

### 3. Atmospheric Rendering (Post-process)
**Purpose**: Atmosphere, fog, and distant haze
**Location**: `src/rendering/atmosphere_renderer.cpp`

- Screen-space ray marching for atmosphere ONLY
- NOT for planet surface (that's what meshes are for)
- Adds realism without performance cost

## Data Flow

```
1. OCTREE TRAVERSAL (CPU/GPU)
   ├─ Frustum culling
   ├─ LOD selection based on distance
   └─ Identify visible chunks

2. MESH GENERATION (GPU Compute)
   ├─ Sample voxels at LOD resolution
   ├─ Run Transvoxel algorithm
   ├─ Generate vertices with MixedVoxel colors
   └─ Output to vertex/index buffers

3. RENDERING (GPU)
   ├─ Single draw call per chunk
   ├─ Standard triangle rasterization
   └─ ~100k triangles total (fast!)

4. POST-PROCESS
   └─ Atmospheric scattering
```

## Memory Management

### Buffer Layout
```cpp
struct GPUBuffers {
    // Data buffers (small, hierarchical)
    VkBuffer octreeNodes;        // Octree structure
    VkBuffer voxelData;          // MixedVoxel data
    
    // Generated mesh buffers (what we actually render)
    VkBuffer vertexBuffer;       // Transvoxel output vertices
    VkBuffer indexBuffer;        // Transvoxel output indices
    
    // Compute buffers
    VkBuffer densityField;       // Voxel samples for meshing
    VkBuffer edgeTable;          // Transvoxel lookup tables
};
```

### Memory Budget
- Octree Data: 256 MB (compressed, hierarchical)
- Mesh Cache: 512 MB (generated meshes, LRU eviction)
- Compute Buffers: 128 MB (working space)
- Total VRAM: < 2 GB (achievable!)

## Implementation Phases

### Phase 1: Basic Transvoxel (MVP)
- CPU implementation first (prove it works)
- Single LOD level
- No caching
- Target: 30 FPS
- Consider: Async generation with placeholder meshes for new chunks

### Phase 2: Multi-LOD
- Implement LOD levels 0-3
- Seamless transitions (Transvoxel's strength)
- Basic caching
- Target: 60 FPS

### Phase 3: GPU Compute
- Move mesh generation to GPU
- Parallel chunk processing
- Advanced caching
- Target: 120+ FPS

### Phase 4: Optimizations
- Temporal caching
- Predictive pre-generation
- Mesh simplification
- Texture atlasing for materials

## Performance Targets (Realistic)

| Quality | Resolution | Triangle Budget | Target FPS |
|---------|------------|-----------------|------------|
| Low     | 1080p      | 50k triangles   | 120 FPS    |
| Medium  | 1080p      | 200k triangles  | 60 FPS     |
| High    | 1440p      | 500k triangles  | 60 FPS     |
| Ultra   | 4K         | 1M triangles    | 30 FPS     |

## Why This Will Actually Work

1. **Industry Proven**: Every successful voxel game uses mesh generation
2. **Scalable**: Triangle count independent of world size
3. **Cacheable**: Unchanged chunks reuse meshes
4. **GPU Friendly**: Triangles are what GPUs are built for
5. **MixedVoxel Compatible**: Material blending becomes vertex colors

## What We're Deleting

- ❌ Instance-based rendering (VulkanRenderer::updateInstanceBuffer)
- ❌ Per-voxel draw calls
- ❌ Hierarchical rasterization of cubes
- ❌ Any mention of "millions of visible nodes"

## Key Algorithms

### Transvoxel Regular Cell
- 8 voxel corners → 0-4 triangles per cell
- 256 possible configurations (lookup table)
- Vertex placement on edges between different materials

### Transvoxel Transition Cell
- Special cells at LOD boundaries
- 13 vertices instead of 8
- Prevents cracks between LOD levels
- Critical for seamless planet rendering
- **Implementation warning**: Most complex part of Transvoxel - budget extra debugging time

## References
- [Transvoxel Algorithm](https://transvoxel.org/)
- [GPU Gems 3: Marching Cubes](https://developer.nvidia.com/gpugems/gpugems3/part-i-geometry/chapter-1-generating-complex-procedural-terrains-using-gpu)
- [Dual Contouring Paper](https://www.cs.rice.edu/~jwarren/papers/dualcontour.pdf)

## Success Metrics
- **Frame time**: < 16ms (60 FPS) with medium quality
- **Memory usage**: < 2GB VRAM
- **Mesh generation**: < 5ms per chunk
- **Visual quality**: Smooth surfaces, no voxel artifacts
- **Draw calls**: < 1000 per frame (not 17 million!)