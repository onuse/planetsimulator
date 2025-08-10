# Planetary LOD System Design Document

## Executive Summary

This document outlines the design for a proper planetary Level-of-Detail (LOD) system that provides seamless rendering from space to ground level without hitching, popping, or blur.

## Current Problems

1. **Static sphere patches** - 384 fixed patches regardless of viewing distance
2. **No height displacement** - Smooth sphere with no terrain detail
3. **Binary switching** - Abrupt change between sphere and Transvoxel
4. **Poor coverage** - Blurry from 50km to infinity, detailed only below 50km
5. **No progressive refinement** - Missing continuous LOD
6. **Missing terrain data** - Voxels only contain materials at fixed planet radius, no actual elevation
7. **Octree/rendering mismatch** - Current octree structure not optimized for surface rendering

## Requirements

### Functional Requirements

1. **Seamless zoom** - Smooth transition from space view to ground level
2. **No visual popping** - Continuous LOD with morphing
3. **Consistent frame rate** - 60+ FPS on RTX 3070
4. **Realistic terrain** - Height displacement at all viewing distances
5. **Memory efficient** - Stream data as needed, don't keep everything in memory

### Non-Functional Requirements

1. **Maintainable** - Clean architecture, well-documented
2. **Extensible** - Easy to add features (atmosphere, water, vegetation)
3. **Debuggable** - Visual debug modes, performance metrics
4. **Configurable** - Adjustable quality settings

## Design Principles

### 1. Continuous LOD (CLOD)
- **NO discrete LOD levels** that cause popping
- Vertex positions morph smoothly between detail levels
- Screen-space error metric drives subdivision

### 2. Unified Data Structure
- Single quadtree covers entire planet
- Same rendering pipeline for all distances
- Progressive enhancement, not system switching

### 3. GPU-Driven Rendering
- Frustum culling on GPU
- Indirect draw calls
- Minimal CPU-GPU synchronization

### 4. Predictive Streaming
- Load data before it's needed
- Priority queue based on predicted camera movement
- Asynchronous loading with double buffering

## Prerequisites: Signed Distance Field Foundation

Before implementing the LOD system, we MUST have proper terrain data. Currently, voxels only store materials at a fixed radius with no elevation information. This needs to be addressed first:

### Required: DensityField Implementation
```cpp
class DensityField {
    // Provides actual 3D terrain data via signed distance
    float getDensity(const glm::vec3& worldPos) {
        float radius = glm::length(worldPos);
        float baseRadius = planetRadius;
        
        // Get terrain displacement (can be negative for valleys)
        float terrainHeight = getTerrainHeight(glm::normalize(worldPos));
        float targetRadius = baseRadius + terrainHeight;
        
        // Signed distance: negative inside planet, positive outside
        return radius - targetRadius;
    }
    
    float getTerrainHeight(const glm::vec3& sphereNormal) {
        // Multi-octave noise for realistic terrain
        float continent = simplexNoise(sphereNormal * 0.5f) * 1000.0f;
        float mountains = simplexNoise(sphereNormal * 2.0f) * 500.0f;
        float details = simplexNoise(sphereNormal * 8.0f) * 50.0f;
        
        return continent + mountains * 0.5f + details * 0.1f;
    }
};
```

This SDF will provide:
- Continuous terrain heights at any point
- Smooth gradients for normal calculation
- Foundation for both LOD patches and Transvoxel mesh generation

## Technical Architecture

### Hybrid Architecture: Quadtree + Octree

Given the existing octree implementation, we propose a hybrid approach:

1. **Quadtree for Surface** (Far to medium range: infinity to ~1km altitude)
   - Efficient for planet surface rendering
   - Natural mapping to sphere faces (6 root nodes)
   - Optimized for view-dependent LOD

2. **Octree for Volume** (Close range: <1km altitude)
   - Handles caves, overhangs, complex geometry
   - Integrates with existing Transvoxel implementation
   - Provides volumetric features

### Core Data Structure: Spherical Quadtree

```
PlanetQuadtree
├── Root (6 cube faces projected to sphere)
├── Nodes (recursive subdivision)
│   ├── Spatial: center, size, level
│   ├── Topology: parent, children[4], neighbors[4]
│   ├── Rendering: vertices, indices, instance data
│   └── Content: heights, normals, materials
└── Acceleration
    ├── Morton codes for spatial queries
    ├── Bounding spheres for culling
    └── Error metrics for LOD selection
```

### LOD Selection Algorithm

```
function selectLOD(node, camera):
    error = calculateScreenSpaceError(node, camera)
    
    if error > threshold AND node.level < maxLevel:
        if not node.hasChildren:
            node.subdivide()
        for child in node.children:
            selectLOD(child, camera)
    else:
        renderList.add(node)
        node.updateMorphFactor(camera)
```

### Morphing Strategy

To eliminate popping, vertices morph between positions:

```glsl
// Vertex Shader
uniform float morphFactor; // 0 = full detail, 1 = parent level

vec3 morphVertex(vec3 detailPos, vec3 parentPos) {
    // Smooth interpolation
    return mix(detailPos, parentPos, smoothstep(0.0, 1.0, morphFactor));
}
```

**Morph Regions:**
- 0-30% of transition distance: Full detail
- 30-70% of transition distance: Morphing
- 70-100% of transition distance: Parent level

### Height Data Management

#### Virtual Texturing System
- Planet divided into tiles (256x256 samples each)
- Tiles loaded on demand
- LRU cache with configurable memory limit
- Compressed format (16-bit heights, 8-bit normals)

#### Height Sampling
```
Height Pyramid:
Level 0: 1 sample (planet average)
Level 5: 1,536 samples (continents)
Level 10: 1,572,864 samples (mountains)
Level 15: 1.6B samples (hills)
Level 20: 1.7T samples (rocks)
```

Only levels needed for current view are resident in memory.

### Crack Prevention

T-junctions between different LOD levels cause cracks. Solutions:

1. **Vertex Skipping** - Higher detail level skips vertices to match neighbor
2. **Edge Morphing** - Special handling for patch boundaries
3. **Stitching Strips** - Degenerate triangles fill gaps
4. **Transvoxel Transition Cells** - Special cells at LOD boundaries (for close-range octree)

We'll use a combination approach:
- **Edge Morphing** for quadtree patches (GPU-friendly)
- **Transvoxel transitions** for octree boundaries (when switching to volumetric)

```glsl
// For quadtree patches
if (isEdgeVertex && neighborLOD < currentLOD) {
    position = snapToNeighborGrid(position);
}

// Transition flag indicates switch to octree/Transvoxel
if (transitionToVolumetric) {
    position = blendWithTransvoxelBoundary(position);
}
```

### Memory Budget

#### System Configuration
```cpp
struct MemoryConfig {
    size_t maxGPUMemory = 2GB;      // Height/normal/material data
    size_t maxCPUCache = 4GB;       // Cached tiles
    size_t maxNodesActive = 10000;  // Visible patches
    size_t tileSize = 256;          // Samples per tile
};
```

#### Memory Allocation
- **Vertex Buffer**: 100MB (shared, different resolutions)
- **Index Buffer**: 50MB (multiple tessellation levels)
- **Instance Buffer**: 10MB (per-patch data)
- **Height Cache**: 1GB (GPU resident tiles)
- **Streaming Buffer**: 100MB (double buffered upload)

## Transvoxel Integration Strategy

### Transition Zones
The system seamlessly transitions between rendering approaches based on altitude:

```cpp
enum RenderingMode {
    QUADTREE_ONLY,      // > 1km altitude - pure surface patches
    TRANSITION_ZONE,    // 500m - 1km - blend both systems
    OCTREE_TRANSVOXEL   // < 500m - full volumetric with caves
};

RenderingMode selectMode(float altitude) {
    if (altitude > 1000.0f) return QUADTREE_ONLY;
    if (altitude > 500.0f) return TRANSITION_ZONE;
    return OCTREE_TRANSVOXEL;
}
```

### Transition Zone Handling
In the transition zone, both systems work together:

```cpp
void renderTransitionZone(Camera* camera) {
    float blendFactor = (altitude - 500.0f) / 500.0f; // 0 to 1
    
    // Render quadtree patches with fade-out
    renderQuadtreePatches(visibleNodes, blendFactor);
    
    // Start loading octree chunks
    if (blendFactor < 0.5f) {
        prepareOctreeChunks(camera.position);
    }
    
    // Render Transvoxel meshes with fade-in
    if (blendFactor < 0.8f) {
        float octreeAlpha = 1.0f - blendFactor;
        renderTransvoxelChunks(octreeChunks, octreeAlpha);
    }
}
```

### Transvoxel LOD Boundaries
When using Transvoxel at close range, proper transition cells are critical:

```cpp
class TransvoxelBoundaryHandler {
    // Generate transition cells between LOD levels
    void generateTransitionCells(
        OctreeNode* highDetail,
        OctreeNode* lowDetail,
        TransitionDirection dir
    ) {
        // Use Transvoxel transition tables (0x200 to 0x3FF)
        // These special cells stitch different resolutions
        auto transitionCase = getTransitionCase(highDetail, lowDetail, dir);
        auto mesh = transvoxelTables.getTransitionMesh(transitionCase);
        
        // Ensure watertight connection
        stitchBoundaryVertices(mesh, highDetail, lowDetail);
    }
};
```

## Rendering Pipeline

### 1. Update Phase (CPU)
```cpp
void update(camera, deltaTime) {
    // Predict camera movement
    predictedPos = camera.pos + camera.velocity * lookahead;
    
    // Select LOD nodes
    visibleNodes.clear();
    for (root : quadtreeRoots) {
        selectLOD(root, camera, predictedPos);
    }
    
    // Fix T-junctions
    preventCracks(visibleNodes);
    
    // Update morph factors
    updateMorphing(visibleNodes, deltaTime);
    
    // Stream height data
    priorityQueue = sortByDistance(visibleNodes);
    streamHeightData(priorityQueue);
}
```

### 2. Render Phase (GPU)
```cpp
void render() {
    // Update instance buffer with visible nodes
    instanceBuffer.update(visibleNodes);
    
    // Single draw call for all patches
    vkCmdDrawIndexedIndirect(
        commandBuffer,
        indirectBuffer,
        0,
        visibleNodes.size(),
        sizeof(VkDrawIndexedIndirectCommand)
    );
}
```

### 3. Vertex Processing (GPU)
```glsl
// Vertex Shader
layout(location = 0) in vec2 patchUV;        // 0-1 within patch
layout(location = 1) in uint instanceID;     // Which patch

// Instance data (from buffer)
layout(binding = 1) buffer InstanceData {
    mat4 patchTransform;
    vec4 morphParams;    // factor, neighborLODs
    vec4 heightTexCoord; // UV in virtual texture
} instances[];

void main() {
    // Get instance data
    Instance inst = instances[instanceID];
    
    // Sample height from virtual texture
    float height = textureVirtual(heightTex, inst.heightTexCoord.xy + patchUV);
    
    // Calculate position on sphere
    vec3 spherePos = patchToSphere(patchUV, inst.patchTransform);
    
    // Apply height displacement
    vec3 normal = normalize(spherePos);
    vec3 worldPos = spherePos + normal * height;
    
    // Morphing for smooth LOD
    if (inst.morphParams.x > 0.0) {
        vec3 parentPos = calculateParentPosition(patchUV);
        worldPos = mix(worldPos, parentPos, inst.morphParams.x);
    }
    
    // Edge morphing for crack prevention
    worldPos = fixTJunctions(worldPos, patchUV, inst.morphParams.yzw);
    
    gl_Position = viewProjMatrix * vec4(worldPos, 1.0);
}
```

## Implementation Phases

### Phase 0: SDF Foundation (Week 1) - PREREQUISITE
- [ ] Implement DensityField class with signed distance functions
- [ ] Add simplex noise for terrain generation
- [ ] Verify continuous terrain heights across planet
- [ ] Test SDF gradients for normal calculation
- [ ] Validate C¹ continuity at chunk boundaries

### Phase 1: Core Quadtree Infrastructure (Week 2)
- [ ] Spherical quadtree data structure for surface
- [ ] Basic subdivision logic
- [ ] Integration with existing octree for volumetric data
- [ ] Frustum culling
- [ ] Debug visualization showing quadtree/octree transition

### Phase 2: Height Integration (Week 3)
- [ ] Sample heights from DensityField
- [ ] Virtual texture system for height caching
- [ ] GPU height sampling in vertex shader
- [ ] Normal calculation from SDF gradients
- [ ] Height data streaming infrastructure

### Phase 3: Continuous LOD (Week 4)
- [ ] Screen-space error calculation
- [ ] Vertex morphing in shader
- [ ] Smooth transitions between LOD levels
- [ ] Temporal blending for stability
- [ ] No popping verification

### Phase 4: Crack Prevention & Transvoxel Integration (Week 5)
- [ ] T-junction detection in quadtree
- [ ] Edge morphing implementation
- [ ] Neighbor tracking system
- [ ] Transvoxel transition cells at octree boundaries
- [ ] Seamless quadtree-to-octree handoff at close range
- [ ] Watertight mesh verification

### Phase 5: Optimization (Week 6)
- [ ] GPU-driven culling
- [ ] Indirect rendering
- [ ] Memory management and caching
- [ ] Performance profiling
- [ ] LOD bias and quality settings

### Phase 6: Polish & Production (Week 7)
- [ ] Full integration with existing systems
- [ ] Quality presets (Low/Medium/High/Ultra)
- [ ] Debug visualization modes
- [ ] Performance metrics overlay
- [ ] Documentation and examples

## Performance Targets

### Frame Time Budget (16.67ms for 60 FPS)
- **LOD Selection**: 1ms
- **Streaming**: 1ms (amortized)
- **Instance Update**: 0.5ms
- **GPU Rendering**: 10ms
- **Other Systems**: 4ms

### Metrics to Track
```cpp
struct PerformanceMetrics {
    float lodSelectionTime;
    float streamingTime;
    float gpuRenderTime;
    
    int nodesVisible;
    int nodesSubdivided;
    int nodesMerged;
    
    size_t memoryUsed;
    size_t tilesLoaded;
    int drawCalls;
};
```

## Comparison with Existing Solutions

### Current System
- **Pros**: Simple, working Transvoxel at close range
- **Cons**: Static patches, no LOD, blurry at distance

### Proposed System
- **Pros**: Continuous LOD, works at all distances, no popping
- **Cons**: More complex, requires careful tuning

### Industry References

#### Outerra Engine
- Logarithmic depth buffer
- Adaptive mesh density
- Procedural detail enhancement

#### Google Earth
- Clipmap-style approach
- Hierarchical data streaming
- Predictive caching

#### Star Citizen
- Object container streaming
- Physics grid transitions
- Nested coordinate systems

## Risk Mitigation

### Risk 1: Morphing Artifacts
**Mitigation**: Careful tuning of morph regions, smooth interpolation functions

### Risk 2: Memory Pressure
**Mitigation**: Aggressive culling, virtual texturing, compressed formats

### Risk 3: GPU Bottleneck
**Mitigation**: Instanced rendering, GPU culling, LOD bias settings

### Risk 4: Crack Visibility
**Mitigation**: Multiple crack prevention strategies, runtime validation

### Risk 5: SDF Performance
**Mitigation**: Cache density values, use octaves sparingly, GPU compute for batch evaluation

### Risk 6: Quadtree/Octree Transition
**Mitigation**: Overlap transition zone, temporal blending, predictive loading

### Risk 7: Transvoxel Complexity
**Mitigation**: Implement in phases, maintain fallback to simple marching cubes initially

## Success Criteria

1. **Visual Quality**
   - No visible popping during zoom
   - No cracks between patches
   - Sharp detail at appropriate distances

2. **Performance**
   - Consistent 60+ FPS on RTX 3070
   - < 2GB GPU memory usage
   - < 100ms to zoom from space to ground

3. **Robustness**
   - Handles rapid camera movement
   - Recovers from memory pressure
   - Graceful quality degradation

## Configuration Options

```json
{
    "lod": {
        "maxLevel": 20,
        "pixelError": 2.0,
        "morphRange": 0.3,
        "memoryLimit": "2GB",
        "streamAhead": 3,
        "quality": "high"
    },
    "debug": {
        "showWireframe": false,
        "showLODColors": false,
        "showMorphRegions": false,
        "showMemoryUsage": true
    }
}
```

## Testing Strategy

### Unit Tests
- Quadtree operations
- Morton code generation
- Error metric calculation
- Morph factor interpolation

### Integration Tests
- LOD selection consistency
- Crack prevention
- Memory limits
- Streaming pipeline

### Performance Tests
- Frame time stability
- Memory usage patterns
- Worst-case scenarios
- Stress testing

### Visual Tests
- Screenshot comparisons
- Artifact detection
- Smooth transition verification

## Conclusion

This design provides a robust, scalable planetary LOD system that:
1. Eliminates visual popping through continuous LOD
2. Works efficiently at all viewing distances
3. Integrates smoothly with existing systems
4. Provides room for future enhancements

The key innovation is using a unified quadtree with vertex morphing rather than switching between different rendering systems, ensuring smooth transitions and consistent performance.