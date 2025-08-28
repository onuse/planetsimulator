# Adaptive LOD System Design for Planet Simulator

## Executive Summary
Design a view-dependent, adaptive LOD system that provides extreme zoom capability (from space to millimeter-scale terrain) while maintaining 60+ FPS through efficient resource utilization.

## Current System Problems
1. **Uniform global LOD**: Entire planet rendered at same detail level
2. **Wasted triangles**: Back hemisphere gets same detail as visible area
3. **Limited zoom**: Blurry at close range due to triangle budget limits
4. **Memory inefficient**: Regenerates entire mesh on LOD change
5. **No streaming**: Everything loaded at once

## Proposed Solution: Hierarchical Adaptive Chunked LOD (HACLOD)

### Core Architecture

#### 1. Cube-Sphere Patch System
- Divide planet into 6 cube faces (already partially implemented)
- Each face divided into patch hierarchy (quadtree)
- Patches are the atomic unit of LOD selection
- Patch size: 33x33 vertices (32x32 quads) for optimal GPU cache usage

#### 2. LOD Metrics
```cpp
struct LODMetrics {
    float screenSpaceError;     // Pixels of error if using this LOD
    float geometricError;        // World-space error in meters
    float distanceToCamera;      // Direct distance
    float dotProductToView;      // Facing ratio (backface culling)
    bool inFrustum;             // Frustum culling result
    float priority;             // Combined priority score
};
```

#### 3. Patch Hierarchy
```cpp
class TerrainPatch {
    // Position on cube face
    int faceID;           // 0-5 for cube faces
    int level;            // 0=entire face, increases with subdivision
    vec2 faceUV;          // Position within face [0,1]
    vec2 size;            // Size on face [0,1]
    
    // Mesh data
    vector<Vertex> vertices;      // 33x33 vertex grid
    Buffer* vertexBuffer;         // GPU buffer
    Buffer* indexBuffer;          // Shared between same-LOD patches
    
    // Hierarchy
    TerrainPatch* parent;
    TerrainPatch* children[4];    // Quadtree subdivision
    
    // LOD state
    LODMetrics metrics;
    bool isActive;                // Currently being rendered
    bool hasGeometry;             // Geometry generated
    float lastUsedTime;           // For cache management
};
```

### Key Algorithms

#### 1. Screen-Space Error Calculation
```cpp
float calculateScreenSpaceError(TerrainPatch& patch, Camera& cam) {
    // Get patch bounding sphere
    vec3 center = patch.getWorldCenter();
    float radius = patch.getWorldRadius();
    
    // Calculate geometric error for this LOD level
    float geometricError = radius * 0.1f; // 10% of patch size
    
    // Project to screen space
    float distance = length(cam.position - center);
    float screenRadius = (radius / distance) * cam.screenHeight;
    float screenError = (geometricError / distance) * cam.screenHeight;
    
    // Account for view angle (reduce priority for oblique views)
    vec3 viewDir = normalize(center - cam.position);
    vec3 patchNormal = normalize(center); // Sphere normal
    float viewDot = max(0.0f, dot(viewDir, patchNormal));
    
    return screenError / (viewDot + 0.1f); // Avoid division by zero
}
```

#### 2. Adaptive Refinement
```cpp
void updateLOD(TerrainPatch& patch, Camera& cam, float errorThreshold) {
    // Calculate metrics
    patch.metrics = calculateMetrics(patch, cam);
    
    // Skip if outside frustum or backfacing
    if (!patch.metrics.inFrustum || patch.metrics.dotProductToView < -0.1f) {
        patch.deactivate();
        return;
    }
    
    // Check if we need to subdivide
    if (patch.metrics.screenSpaceError > errorThreshold && patch.level < MAX_LEVEL) {
        // Subdivide - create/activate children
        if (!patch.hasChildren()) {
            patch.subdivide();
        }
        patch.deactivate();
        for (auto& child : patch.children) {
            updateLOD(*child, cam, errorThreshold);
        }
    } 
    // Check if we should merge (simplify)
    else if (patch.metrics.screenSpaceError < errorThreshold * 0.5f && patch.parent) {
        // Low error - can use parent instead
        patch.deactivate();
    }
    // This patch is at appropriate LOD
    else {
        patch.activate();
        if (!patch.hasGeometry) {
            patch.generateGeometry();
        }
    }
}
```

#### 3. Seamless Patch Boundaries (Critical!)
To avoid cracks between different LOD levels:

```cpp
void stitchPatchBoundaries(TerrainPatch& patch) {
    // Check each edge for neighbor LOD
    for (int edge = 0; edge < 4; edge++) {
        TerrainPatch* neighbor = patch.getNeighbor(edge);
        
        if (!neighbor || neighbor->level == patch.level) {
            // Same LOD or no neighbor - use full resolution
            patch.setEdgeResolution(edge, FULL_RES);
        }
        else if (neighbor->level < patch.level) {
            // Neighbor is coarser - match their resolution
            patch.setEdgeResolution(edge, HALF_RES);
            // Skip every other vertex on this edge
        }
        else {
            // Neighbor is finer - they'll match us
            patch.setEdgeResolution(edge, FULL_RES);
        }
    }
    
    // Regenerate index buffer with proper triangulation
    patch.generateAdaptiveIndices();
}
```

### Implementation Phases

#### Phase 1: Basic Patch System
1. Implement cube-sphere patch division
2. Create patch class with basic quadtree
3. Simple distance-based LOD (no screen-space error yet)
4. Basic frustum culling per patch

#### Phase 2: Adaptive Refinement
1. Implement screen-space error metrics
2. Add dynamic subdivision/simplification
3. Patch geometry generation on demand
4. Basic memory pool for patches

#### Phase 3: Seamless Boundaries
1. Implement edge resolution matching
2. Adaptive index buffer generation
3. T-junction elimination
4. Cross-face boundary handling

#### Phase 4: Optimization
1. Patch geometry caching
2. Async geometry generation (background thread)
3. Predictive LOD (pre-generate likely needed patches)
4. GPU-driven culling and LOD selection

#### Phase 5: Advanced Features
1. Geomorphing (smooth LOD transitions)
2. Displacement mapping for ultra-high detail
3. Texture streaming per patch
4. Atmospheric scattering integration

### Memory Management

#### Patch Pool
- Pre-allocate pool of ~10,000 patches
- LRU cache for geometry
- Priority queue for generation
- Background streaming thread

#### Budget Targets
- 500MB geometry (vertices + indices)
- 200MB patch metadata
- 2GB textures (with virtual texturing)
- Target: 2-4 million triangles on screen

### Performance Targets

#### Distance Ranges & Triangle Budgets
- **Orbital** (>100km): 50k triangles visible hemisphere
- **High altitude** (10-100km): 200k triangles for visible area
- **Low altitude** (1-10km): 500k triangles for viewport
- **Near surface** (10m-1km): 1M triangles for immediate area
- **Surface detail** (<10m): 2M triangles for hero terrain
- **Extreme close-up** (<1m): 4M triangles with displacement

#### Frame Time Budget (16.6ms @ 60fps)
- Culling & LOD selection: 1ms
- Patch generation: 2ms (amortized)
- GPU upload: 1ms
- Rendering: 10ms
- Other systems: 2.6ms

### Advantages Over Current System

1. **Efficient triangle usage**: Only visible areas get detail
2. **Unlimited zoom**: Can go from orbit to blade of grass
3. **Stable performance**: Constant triangle budget
4. **Memory efficient**: Stream patches as needed
5. **Future-proof**: Ready for ray tracing, virtual textures

### Integration with Existing Systems

#### Octree Voxel Data
- Patches sample from octree for height/material
- Octree provides coarse collision
- Can share octree LOD system for far view

#### Terrain Generation
- `sampleImprovedTerrain()` called per vertex
- Cache terrain samples per patch
- Can add detail octaves at close range

#### Transvoxel Rendering
- Keep for underground/cave systems
- Patches transition to voxels when going underground
- Hybrid rendering based on context

### Challenges & Solutions

#### Challenge: Floating-point precision at planet scale
**Solution**: Camera-relative rendering with double precision transforms

#### Challenge: Patch boundaries across cube faces  
**Solution**: Special corner vertices shared between 3 faces

#### Challenge: Terrain paging hitches
**Solution**: Predictive loading and incremental generation

#### Challenge: LOD popping
**Solution**: Geomorphing with vertex shader blend

## Conclusion

This HACLOD system will provide:
- **10-100x more detail** where it matters (viewport)
- **Smooth zoom** from space to microscopic detail
- **Stable 60+ FPS** through efficient resource use
- **Foundation** for advanced planet features (cities, vegetation, etc.)

The phased implementation allows incremental progress while maintaining a working system throughout development.