# Terrain Discontinuity Investigation

## Problem Analysis

### Issue 1: Terrain Discontinuities
**Symptoms:**
- Terrain appears as "jammed puzzle pieces"
- Sharp boundaries between patches
- Discontinuous continents

**Root Cause Hypothesis:**
The terrain sampling uses `normalize(cubePos)` which should be consistent, but:
1. Different patches might have slightly different cube positions due to floating-point errors
2. The noise function might be sensitive to tiny differences
3. Face boundaries might introduce coordinate system discontinuities

**Current Code (quadtree_patch.vert:286):**
```glsl
dvec3 consistentSamplePos = normalize(cubePos);
float height = getTerrainHeight(vec3(consistentSamplePos));
```

**Potential Fixes:**
1. Quantize/snap the sample position to a grid
2. Use a hash-based terrain cache
3. Ensure exact vertex positions at boundaries

### Issue 2: Black Triangular Gaps
**Symptoms:**
- Black triangular areas where cube faces meet
- Visible at cube edges/corners
- Gaps not covered by skirt geometry

**Root Cause Hypothesis:**
1. Missing patches at face boundaries
2. Skirt geometry not extending far enough
3. Face culling removing necessary patches

**Investigation Steps:**
1. Check if all 6 faces are being processed
2. Verify patch generation at face boundaries
3. Check skirt geometry coverage

## Test Plan

### Test 1: Verify Continuous Terrain
- Sample terrain at exact same position from different patches
- Should get identical height values

### Test 2: Check Face Coverage  
- Count patches per face
- Verify no gaps in coverage
- Check boundary patch generation

### Test 3: Skirt Geometry
- Verify skirt extends below surface
- Check skirt at face boundaries
- Ensure no gaps between skirts