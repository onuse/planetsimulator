# Vulkan + Octree: Design Document

## Vision

A GPU-native octree implementation that leverages modern Vulkan features for planet-scale voxel simulation and rendering. The octree lives entirely on the GPU, with the CPU acting only as a high-level coordinator.

## Core Principles

1. **GPU-First Design**: The octree is designed for GPU traversal and manipulation
2. **Unified Representation**: Same structure serves physics, rendering, and collision
3. **Dynamic Detail**: Automatic subdivision/merging based on activity and view distance
4. **Zero-Copy Updates**: Physics modifies the same tree that rendering uses
5. **Async Everything**: Leverage Vulkan's multi-queue architecture

## Modern Vulkan Features to Leverage

### 1. **Buffer Device Address (BDA)**
```glsl
// Natural tree pointers in shaders!
struct OctreeNode {
    vec3 center;
    float size;
    uint64_t childrenAddress;  // Direct GPU pointer
    uint64_t dataAddress;      // Points to voxel data
};
```

### 2. **Mesh Shaders** (if available)
- Generate geometry directly from octree
- No vertex buffer needed
- Perfect for adaptive LOD

### 3. **Ray Tracing Extensions** (future)
- Octree as acceleration structure
- Hardware-accelerated traversal
- Perfect quality at any distance

### 4. **Multi-Queue Architecture**
```
Compute Queue 0: Physics simulation
Compute Queue 1: Octree maintenance (split/merge)
Compute Queue 2: Surface extraction
Graphics Queue:  Rendering
Transfer Queue:  Background streaming
```

### 5. **Indirect Execution**
```glsl
// GPU decides what to render
octreeTraverse();
writeDrawCommand();
// CPU never knows how many triangles
```

## Octree Structure

### Node Layout (64 bytes, cache-line aligned)
```c
struct GpuOctreeNode {
    // Spatial info (16 bytes)
    vec3 center;
    float halfSize;
    
    // Tree structure (16 bytes)
    uint childOffset;      // Offset to first child (0 = leaf)
    uint parentOffset;     // For bottom-up traversal
    uint level;           // 0 = root, N = finest detail
    uint childMask;       // Bit i set = child i exists
    
    // Material data (16 bytes) - averaged for interior nodes
    uint materialType;
    float density;
    float temperature;
    float pressure;
    
    // Dynamics (16 bytes)
    vec3 velocity;
    int plateId;
};
```

### Memory Layout
```
[Root Node]
[Level 1 Nodes...]
[Level 2 Nodes...]
...
[Leaf Nodes...]
[Free List...]
```

## Key Algorithms

### 1. **GPU Octree Traversal**
```glsl
// Stackless traversal using parent pointers
uint traverseToPoint(vec3 worldPos) {
    uint nodeIdx = 0; // root
    while (!isLeaf(nodeIdx)) {
        uint childIdx = selectChild(worldPos, nodes[nodeIdx]);
        nodeIdx = nodes[nodeIdx].childOffset + childIdx;
    }
    return nodeIdx;
}
```

### 2. **Parallel Subdivision**
```glsl
// Each thread handles one node
if (needsSubdivision(nodeIdx)) {
    uint newChildren = atomicAdd(nodeCounter, 8);
    nodes[nodeIdx].childOffset = newChildren;
    // Initialize 8 children in parallel
}
```

### 3. **Surface Extraction**
```glsl
// Dual marching cubes at each level
if (crossesSurface(node)) {
    if (node.level < targetLOD) {
        traverseChildren();
    } else {
        generateTriangles(node);
    }
}
```

## Physics Integration

### Adaptive Simulation
- Coarse nodes: Bulk flow equations
- Medium nodes: Averaged convection
- Fine nodes: Detailed voxel physics
- Boundary nodes: Auto-subdivide

### Update Pattern
```glsl
// Bottom-up update
for (level = maxLevel; level >= 0; level--) {
    parallel_for_each_node_at_level(level) {
        if (isLeaf(node)) {
            updatePhysics(node);
        } else {
            averageFromChildren(node);
        }
    }
}
```

## Rendering Pipeline

### LOD Selection
```glsl
float screenSize = projectToScreen(node.halfSize, node.center);
if (screenSize < pixelThreshold) {
    renderNode(node);
} else {
    traverseChildren(node);
}
```

### Multiple Render Modes
1. **Debug**: Render octree structure
2. **Surface**: Extract isosurface at current LOD
3. **Volume**: Ray march through octree
4. **Hybrid**: Surface + volumetric atmosphere

## Memory Management

### Dynamic Allocation
- Free list in GPU memory
- Atomic allocation/deallocation
- Compaction during idle time

### Streaming Strategy
- Keep frequently accessed nodes in VRAM
- Stream distant/deep nodes from system memory
- Predictive loading based on camera movement

## Implementation Phases

### Phase 1: Static Octree
- Build octree from existing voxel data
- Basic traversal and rendering
- Verify structure is correct

### Phase 2: Dynamic Updates
- Subdivision/merging
- Simple physics updates
- Memory management

### Phase 3: Full Integration
- Replace current voxel system
- All physics on octree
- Streaming and paging

### Phase 4: Advanced Features
- Mesh shaders (if available)
- Hardware ray tracing (if available)
- Procedural detail injection

## Performance Targets

- **1 Billion voxels**: Representable with ~10M octree nodes
- **60+ FPS**: From space to surface
- **Real-time physics**: Adaptive detail
- **Memory**: <4GB for Earth-scale planet

## File Structure

```
octree/
  VULKAN_OCTREE.md        // This file
  types.go               // Go-side octree structures
  builder.go             // CPU octree construction
  gpu_octree.go          // GPU buffer management
  shaders/
    octree_common.glsl   // Shared structures
    octree_traverse.comp // Traversal algorithms
    octree_update.comp   // Subdivision/merging
    octree_physics.comp  // Physics updates
    octree_surface.comp  // Surface extraction
    octree_debug.vert/frag // Debug visualization
  vulkan_octree_renderer.go // Renderer implementation
```

## Next Steps

1. Define exact GPU buffer layouts
2. Implement basic CPU octree builder
3. Create simple octree visualizer
4. Port one physics system as proof of concept

The synergy between Vulkan's compute capabilities and octree's hierarchical nature will enable planet-scale simulation and rendering that adapts seamlessly from space to surface level detail.