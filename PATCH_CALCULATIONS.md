# Planet Patch Requirements Analysis

## Planet Specifications
- Planet radius: 6,371 km (Earth-sized)
- 6 cube faces (cube-to-sphere mapping)
- Quadtree subdivision (each patch divides into 4)

## LOD Level Analysis

### Level 0 (Root)
- 6 patches (1 per face)
- Patch size: ~10,000 km per edge
- Total area coverage: entire planet

### Level 1
- 24 patches (4 per face)
- Patch size: ~5,000 km per edge

### Level 2
- 96 patches (16 per face)
- Patch size: ~2,500 km per edge

### Level 3
- 384 patches (64 per face)
- Patch size: ~1,250 km per edge

### Level 4
- 1,536 patches (256 per face)
- Patch size: ~625 km per edge

### Level 5
- 6,144 patches (1,024 per face)
- Patch size: ~312 km per edge

### Level 6
- 24,576 patches (4,096 per face)
- Patch size: ~156 km per edge

### Level 7
- 98,304 patches (16,384 per face)
- Patch size: ~78 km per edge

### Level 8
- 393,216 patches (65,536 per face)
- Patch size: ~39 km per edge

### Level 9
- 1,572,864 patches (262,144 per face)
- Patch size: ~20 km per edge

### Level 10
- 6,291,456 patches (1,048,576 per face)
- Patch size: ~10 km per edge

## Viewing Scenarios

### 1. Space View (altitude > 10,000 km)
- Can see ~3 faces fully
- Need low detail (Level 0-3)
- Estimated patches: 6-384 total

### 2. High Altitude (1,000-10,000 km)
- Can see 1-2 faces
- Need moderate detail (Level 3-5)
- Estimated patches: 100-1,000 for visible area

### 3. Medium Altitude (100-1,000 km)
- See partial face (~1/4 to 1/16 of face)
- Need good detail (Level 5-7)
- Estimated patches: 500-2,000 for visible area

### 4. Low Altitude (10-100 km)
- See small region (~100-1000 km²)
- Need high detail (Level 7-9)
- Estimated patches: 1,000-5,000 for visible area

### 5. Near Surface (<10 km)
- See very small region (~10-100 km²)
- Need maximum detail (Level 9-10)
- Estimated patches: 2,000-10,000 for visible area

## Memory Constraints

### Per Patch:
- Vertex grid: 49x49 = 2,401 vertices
- Per vertex: position (3 floats) + normal (3 floats) + UV (2 floats) = 32 bytes
- Vertex data per patch: 2,401 × 32 = 76,832 bytes (~77 KB)
- Index data: ~14,000 indices × 4 bytes = 56 KB
- Total per patch: ~133 KB

### Total Memory:
- 1,000 patches = ~133 MB
- 2,000 patches = ~266 MB
- 5,000 patches = ~665 MB
- 10,000 patches = ~1.33 GB

## Performance Constraints

### GPU Limits:
- Modern GPUs handle 10-50 million triangles at 60 FPS
- Each patch: ~4,800 triangles (48×48 quads × 2 triangles)
- 2,000 patches = 9.6 million triangles (good for 60 FPS)
- 5,000 patches = 24 million triangles (30-60 FPS range)
- 10,000 patches = 48 million triangles (may struggle)

### Draw Calls:
- Currently using single draw call with concatenated buffers
- Limit is vertex buffer size, not draw call count

## Recommended Design

### Dynamic Patch Budget:
```cpp
struct PatchBudget {
    // Base limits
    static constexpr size_t MIN_PATCHES = 100;          // Minimum for any view
    static constexpr size_t MAX_PATCHES_OPTIMAL = 2000; // Target for 60 FPS
    static constexpr size_t MAX_PATCHES_QUALITY = 5000; // Maximum for quality
    
    // Per-face fairness
    static constexpr size_t MIN_PATCHES_PER_FACE = 20;  // Ensure all faces get some
    
    // Dynamic calculation based on altitude
    static size_t calculatePatchBudget(float altitude) {
        if (altitude > 10000000.0f) {        // > 10,000 km
            return 500;   // See whole planet, low detail
        } else if (altitude > 1000000.0f) {  // > 1,000 km  
            return 1000;  // See large area
        } else if (altitude > 100000.0f) {   // > 100 km
            return 2000;  // Standard view
        } else if (altitude > 10000.0f) {    // > 10 km
            return 3000;  // Close view
        } else {
            return 5000;  // Maximum detail at surface
        }
    }
    
    // Per-face limit based on visibility
    static size_t calculatePerFaceLimit(size_t totalBudget, int visibleFaces) {
        // Reserve minimum for each face, distribute rest based on screen coverage
        size_t reserved = MIN_PATCHES_PER_FACE * 6;
        size_t distributable = totalBudget - reserved;
        return MIN_PATCHES_PER_FACE + (distributable / visibleFaces);
    }
};
```

### Adaptive LOD Thresholds:
- Screen space error threshold should also vary with altitude
- Closer views need finer error thresholds
- Balance between quality and performance

## Conclusion

Instead of arbitrary limits like "500" or "2000", we should:

1. **Base limits on viewing distance**: 500-5000 patches depending on altitude
2. **Ensure fair distribution**: No face should take more than 50% of budget
3. **Reserve minimums**: Each visible face gets at least 20 patches
4. **Consider memory**: Stay under 500 MB for vertex data
5. **Target performance**: Aim for 2000 patches (10M triangles) for 60 FPS

The current issue where Face 0 takes all 500 patches is clearly wrong - at any viewing distance, we need patches distributed across all visible faces.