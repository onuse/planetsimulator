# Performance Analysis: 4,000 Patches

## Current Performance Baseline
- At ~500 patches: Getting 8.4 FPS (from screenshots)
- At ~150 patches: Getting 3.6 FPS (from earlier screenshots)

## 4,000 Patches Analysis

### Geometry Load
- Vertices per patch: 49×49 = 2,401
- Total vertices: 4,000 × 2,401 = **9,604,000 vertices**
- Triangles per patch: 48×48×2 = 4,608
- Total triangles: 4,000 × 4,608 = **18,432,000 triangles**

### Memory Requirements
- Vertex data: 4,000 × 77 KB = 308 MB
- Index data: 4,000 × 56 KB = 224 MB
- Total GPU memory: ~532 MB

### GPU Performance Expectations

#### Modern GPU Capabilities:
- **RTX 3060**: ~13 billion triangles/sec
- **RTX 2060**: ~8 billion triangles/sec  
- **GTX 1060**: ~4 billion triangles/sec
- **Integrated GPU**: ~1-2 billion triangles/sec

#### At 60 FPS target:
- Need to render 18.4M triangles in 16.67ms
- That's 1.1 billion triangles/second
- **Verdict**: Should work on most discrete GPUs

#### At 30 FPS target:
- Need to render 18.4M triangles in 33.33ms
- That's 550 million triangles/second
- **Verdict**: Should work even on integrated graphics

### Current Bottleneck Analysis

We're getting only 8.4 FPS with 500 patches (2.3M triangles), which suggests:
- Expected: 60+ FPS with 2.3M triangles
- Actual: 8.4 FPS
- **Something else is the bottleneck!**

Possible bottlenecks:
1. **CPU vertex generation** - generating vertices every frame?
2. **Buffer uploads** - uploading all data every frame?
3. **Shader complexity** - expensive per-pixel calculations?
4. **Memory bandwidth** - too much data transfer?
5. **Draw call overhead** - though we use single draw call

## Recommendations

### Option 1: Keep 4,000 but optimize first
Before reducing the limit, we should fix the performance issues:
- Cache generated vertices (don't regenerate every frame)
- Use persistent mapped buffers
- Profile the actual bottleneck

### Option 2: Conservative approach
Reduce maximum to 2,000 patches:
```cpp
} else {                              // < 10 km - surface level
    patchBudget = 2000;  // More conservative
}
```
This gives us:
- 9.2M triangles (well within GPU capability)
- 266 MB memory (very reasonable)
- Should achieve 30-60 FPS once other issues fixed

### Option 3: Dynamic quality settings
Add a quality setting that users can adjust:
```cpp
enum class QualityLevel {
    LOW,     // Max 1,000 patches
    MEDIUM,  // Max 2,000 patches  
    HIGH,    // Max 3,000 patches
    ULTRA    // Max 4,000 patches
};
```

## Conclusion

4,000 patches (18M triangles) is **theoretically reasonable** for modern hardware, but:

1. **Current performance suggests other bottlenecks** that need fixing first
2. **2,000 patches might be more practical** for consistent 60 FPS
3. **The real issue isn't triangle count** - we need to profile and fix the actual bottleneck

The fact that we get 8.4 FPS with only 500 patches suggests the problem isn't the triangle count but something else in the rendering pipeline.