# Material Mixing & LOD Design Document

## Core Requirements
1. **Deterministic**: Same input → same output (for reproducible evolution)
2. **Scale-invariant**: Looks correct from 1m to 10,000km
3. **Performance**: Real-time on modern GPUs
4. **Memory-efficient**: Planet-scale data
5. **Evolution-compatible**: Can change over time

## Data Structure Design

### Current (Problematic)
```cpp
struct Voxel {
    MaterialType material;  // Single material - too limiting!
    float density;
    float temperature;
};
```

### Proposed: Compact Mixed Material
```cpp
// 16 bytes per voxel - reasonable for GPU
struct MixedVoxel {
    // Material composition (4 bytes)
    uint8_t rock;      // 0-255 normalized
    uint8_t water;     // 0-255 normalized
    uint8_t air;       // 0-255 normalized
    uint8_t sediment;  // 0-255 normalized
    // Sum should ≈ 255 (allowing rounding errors)
    
    // Physical properties (4 bytes)
    uint8_t temperature;  // 0-255 mapped to real range
    uint8_t pressure;     // 0-255 mapped to real range
    uint8_t age;         // Geological age (0-255 epochs)
    uint8_t flags;       // Special properties/state
    
    // Visual hints (4 bytes) - for rendering
    uint8_t roughness;   // Surface texture
    uint8_t vegetation;  // Future: plant coverage
    uint8_t moisture;    // Wetness/snow
    uint8_t erosion;     // Erosion amount
    
    // Dynamics (4 bytes) - for simulation
    uint8_t plate_id;    // Which tectonic plate
    uint8_t stress_x;    // Directional stress
    uint8_t stress_y;    
    uint8_t velocity_z;  // Vertical movement
};

// Alternative: If 16 bytes is too much, use 8 bytes
struct CompactMixedVoxel {
    // 2 dominant materials + blend (4 bytes)
    uint8_t material1;    // Primary material type
    uint8_t material2;    // Secondary material type
    uint8_t blend;        // 0=all mat1, 255=all mat2
    uint8_t properties;   // Packed temperature/pressure
    
    // Visual/dynamics (4 bytes)
    uint8_t age;
    uint8_t plate_id;
    uint8_t stress;       // Magnitude only
    uint8_t flags;
};
```

## Hierarchical Averaging Algorithm

### Problem
When zooming out, we need to average 8 child voxels into 1 parent. Simple averaging creates artifacts (mountains become ocean).

### Solution: Feature-Aware Averaging
```cpp
MixedVoxel averageVoxels(const MixedVoxel children[8]) {
    MixedVoxel parent;
    
    // Step 1: Identify dominant feature
    FeatureType feature = identifyFeature(children);
    
    // Step 2: Average based on feature type
    switch(feature) {
        case MOUNTAIN:
            // Preserve peak - use MAX for rock, MIN for water
            parent.rock = maxOf(children.rock);
            parent.water = minOf(children.water);
            break;
            
        case OCEAN:
            // Preserve depth - use MAX for water
            parent.water = maxOf(children.water);
            parent.rock = minOf(children.rock);
            break;
            
        case COAST:
            // Blend naturally
            parent.rock = averageOf(children.rock);
            parent.water = averageOf(children.water);
            break;
            
        case MIXED:
            // Standard average
            parent = standardAverage(children);
            break;
    }
    
    // Step 3: Preserve important properties
    parent.age = maxOf(children.age);  // Keep oldest rock
    parent.temperature = averageOf(children.temperature);
    
    return parent;
}

FeatureType identifyFeature(const MixedVoxel children[8]) {
    int highRock = 0, highWater = 0;
    
    for(int i = 0; i < 8; i++) {
        if(children[i].rock > 200) highRock++;
        if(children[i].water > 200) highWater++;
    }
    
    if(highRock >= 6) return MOUNTAIN;  // Mostly rock
    if(highWater >= 6) return OCEAN;     // Mostly water
    if(highRock > 0 && highWater > 0) return COAST;  // Mixed
    return MIXED;
}
```

## LOD Selection Algorithm

### Current Problem
Fixed LOD based on distance causes popping and doesn't account for feature importance.

### Solution: Adaptive LOD
```cpp
int selectLOD(
    const OctreeNode& node,
    const Camera& camera,
    const RenderContext& context
) {
    float distance = length(camera.position - node.center);
    float baseDetail = log2(distance / node.size);
    
    // Adjust for feature importance
    float importance = 0.0f;
    
    // High-frequency areas need more detail
    if(node.hasHighVariance()) importance += 2.0f;
    
    // Boundaries need more detail
    if(node.isMaterialBoundary()) importance += 1.0f;
    
    // Player focus areas
    if(node.inPlayerFocus()) importance += 3.0f;
    
    // Combine
    int targetLOD = (int)(baseDetail - importance);
    
    // Clamp to valid range
    return clamp(targetLOD, 0, node.maxDepth);
}
```

## Rendering Material Mixtures

### Shader Approach
```glsl
// Fragment shader
vec3 getMixedColor(MixedVoxel voxel) {
    vec3 color = vec3(0.0);
    
    // Blend base colors
    float totalMat = float(voxel.rock + voxel.water + 
                           voxel.air + voxel.sediment);
    
    color += ROCK_COLOR * (float(voxel.rock) / totalMat);
    color += WATER_COLOR * (float(voxel.water) / totalMat);
    color += SEDIMENT_COLOR * (float(voxel.sediment) / totalMat);
    
    // Apply physical properties
    color *= temperatureToColor(voxel.temperature);
    
    // Add moisture effects
    if(voxel.moisture > 128) {
        color = mix(color, WATER_COLOR, 
                   float(voxel.moisture - 128) / 127.0);
    }
    
    return color;
}
```

## Memory Layout for GPU

### Octree Node Storage
```cpp
struct GPUOctreeNode {
    // Navigation (16 bytes)
    uint32_t childOffset;      // Offset to first child
    uint32_t voxelOffset;      // Offset to voxel data
    float3   center;           // World position
    float    halfSize;         // Node size
    
    // Averaged properties (16 bytes)
    uint8_t  avgMaterials[4];  // Average composition
    uint8_t  featureType;      // Mountain/ocean/etc
    uint8_t  importance;       // For LOD selection
    uint8_t  flags;
    uint8_t  padding;
    
    float    minElevation;     // Don't lose valleys
    float    maxElevation;     // Don't lose peaks
    
    // Total: 32 bytes (GPU cache friendly)
};
```

### Data Streaming Strategy
```cpp
class VoxelStreamer {
    // Keep frequently accessed data in GPU memory
    GPUBuffer activeNodes;      // Currently visible
    GPUBuffer neighborCache;    // Likely to be needed
    
    // Stream from CPU/disk as needed
    CPUBuffer distantNodes;     // Far away
    DiskCache archivedNodes;   // Very far/old
    
    void update(Camera camera) {
        // Predict what will be needed
        PredictedView next = predictMovement(camera);
        
        // Start async transfers
        streamToGPU(next.required);
        
        // Free unused
        evictFromGPU(next.unused);
    }
};
```

## Determinism Guarantees

### Floating Point Consistency
```cpp
// Use fixed-point for critical calculations
struct FixedPoint {
    int32_t value;  // 16.16 fixed point
    
    FixedPoint operator+(FixedPoint other) {
        return {value + other.value};
    }
    
    float toFloat() const {
        return value / 65536.0f;
    }
};

// Use for plate movement, erosion, etc.
FixedPoint plateVelocity;
```

### Order-Independent Operations
```cpp
// BAD: Order-dependent
for(auto& plate : plates) {
    plate.move();  // Each affects others
}

// GOOD: Order-independent
auto newPositions = computeNewPositions(plates);
applyPositions(plates, newPositions);
```

## Performance Optimizations

### 1. Spatial Locality
- Store octree nodes in Morton order
- Group materials by type for cache efficiency

### 2. Temporal Coherence
- Cache LOD decisions frame-to-frame
- Reuse averaged values when possible

### 3. GPU Parallelism
- Process each node independently
- Use compute shaders for averaging

### 4. Compression
- Store materials as palette indices
- Use BC compression for colors

## Testing Strategy

### Unit Tests
```cpp
TEST(MaterialMixing, PreservesMountainPeak) {
    MixedVoxel children[8];
    // 7 rock, 1 air (mountain peak)
    for(int i = 0; i < 7; i++) {
        children[i].rock = 255;
        children[i].water = 0;
    }
    children[7].air = 255;
    
    auto parent = averageVoxels(children);
    
    // Should still be mostly rock
    EXPECT_GT(parent.rock, 200);
    EXPECT_LT(parent.water, 50);
}

TEST(LOD, ConsistentAcrossFrames) {
    // Same input should give same LOD
    auto lod1 = selectLOD(node, camera, context);
    auto lod2 = selectLOD(node, camera, context);
    EXPECT_EQ(lod1, lod2);
}
```

### Integration Tests
```cpp
TEST(Rendering, MountainVisibleAtAllDistances) {
    createMountain(world);
    
    for(float dist = 1.0f; dist < 1000000.0f; dist *= 10) {
        camera.position = vec3(dist, dist, dist);
        render(world, camera);
        
        // Mountain should never appear as water
        EXPECT_FALSE(appearsAsWater(screenshot));
    }
}
```

## Implementation Phases

### Phase 1: Data Structures (1 week)
- [ ] Implement MixedVoxel structure
- [ ] Update octree to use new voxel type
- [ ] Add material mixing utilities

### Phase 2: Averaging Algorithm (1 week)
- [ ] Implement feature detection
- [ ] Create feature-aware averaging
- [ ] Test with various terrain types

### Phase 3: Rendering (2 weeks)
- [ ] Update shaders for mixed materials
- [ ] Implement color blending
- [ ] Add material transition smoothing

### Phase 4: LOD System (2 weeks)
- [ ] Implement adaptive LOD selection
- [ ] Add temporal coherence
- [ ] Optimize transitions

### Phase 5: Testing & Optimization (1 week)
- [ ] Comprehensive test suite
- [ ] Performance profiling
- [ ] Memory optimization

## Success Criteria

1. **Visual**: Mountains stay mountains at all distances
2. **Performance**: 60 FPS with mixed materials
3. **Memory**: < 4GB for Earth-scale planet
4. **Determinism**: Identical results across runs
5. **Evolution**: Supports geological time simulation

This design provides a robust foundation for planet evolution with proper material mixing and LOD!