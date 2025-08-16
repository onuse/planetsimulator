# Theory: Back Face Bleed-Through (The Dots Problem)

## Visual Evidence
- Green dots appear on blue faces (bottom of sphere)
- The dots match the color of the face directly behind (green = opposite side)
- Dots appear in a regular pattern, suggesting systematic precision issues
- Only visible where front and back faces are very close in depth buffer values

## Coherent Theory

### The Depth Precision Problem
At planet scale (6.371 million meters radius), and viewing from ~15 million meters away, the front and back faces of the sphere are competing for depth buffer precision.

**Key measurements:**
- Camera distance: ~15.4 million meters
- Planet radius: ~6.371 million meters  
- Front face distance: ~9 million meters
- Back face distance: ~22 million meters
- Depth buffer: 24-bit (1 in 16.7 million precision)

### Why Dots Appear (REVISED)

**Critical Observation**: The dots do NOT follow the sphere's outline/silhouette! They appear clustered in the interior of visible faces, not at the edges where front/back would be closest.

1. **NOT Z-Fighting at Silhouette**
   - If it were simple depth precision at grazing angles, dots would form a ring near the sphere edge
   - But dots appear scattered across the interior of the blue face
   - This rules out simple "front and back faces too close" theory

2. **Camera-Relative Transform Precision Loss**
   - We transform vertices to camera-relative space: `vertex.position = worldPos - cameraPos`
   - For back faces, this creates positions ~22 million meters away
   - Small errors in the transform accumulate differently for front vs back faces
   - Some vertices end up slightly closer/farther than they should be

3. **Possible Causes for Interior Dots**
   
   a) **Degenerate or Flipped Triangles**
   - Some triangles in the blue face might be degenerate (zero area)
   - Creates "holes" where background (green face) shows through
   - Would explain clustering rather than edge pattern
   
   b) **Vertex Position Errors**
   - Some vertices might be calculated incorrectly
   - Pushed behind the sphere instead of on surface
   - Creates gaps in the mesh where back face is visible
   
   c) **Index Buffer Corruption**
   - Wrong indices might connect vertices from different faces
   - Would create triangles that span across the sphere
   - These would render behind legitimate triangles
   
   d) **Patch Boundary Stitching Failure**
   - T-junction resolution might be creating gaps
   - Adjacent patches at different LODs might not align perfectly
   - Gaps let back face show through

## Testable Predictions (REVISED)

Based on the observation that dots are NOT at silhouette:

1. **Dots appear in face interior** ✓
   - Confirmed: Dots scattered across blue face, not at edges
   
2. **Dots match opposite face color** ✓
   - Confirmed: Green dots on blue face (green is on opposite side)
   
3. **Critical Test: Stability with camera movement**
   - **If mesh gaps**: Dots would be STABLE - same location as you zoom in/out
     - The holes are fixed in world space
     - You could zoom in on a specific dot
     - Dots would get larger but stay in same place
   
   - **If depth/precision issues**: Dots would FLICKER and MOVE
     - Different camera distances = different precision errors
     - Dots would appear/disappear as you move
     - Pattern would shift with camera position
   
   - **If LOD-related**: Dots would CHANGE at specific distances
     - New dots appear when LOD level changes
     - Pattern would be consistent at each LOD level
     - But would change dramatically at LOD transitions
   
3. **Pattern follows patch boundaries**
   - Need to verify: Do dots align with our 33x33 vertex grids?
   
4. **Depth buffer values nearly identical**
   - Test: Log depth values where dots appear vs surrounding pixels
   
5. **Problem worsens with distance**
   - Test: Move camera farther away, dots should increase
   - Test: Move camera closer, dots should decrease

## Proposed Tests

### Test 1: Depth Buffer Analysis
```cpp
// In fragment shader, output depth gradient
float depthGradient = abs(dFdx(gl_FragCoord.z)) + abs(dFdy(gl_FragCoord.z));
if (depthGradient > threshold) {
    // Highlight areas with steep depth gradients
    outColor = vec4(1, 0, 1, 1); // Magenta for high gradient
}
```

### Test 2: Offset Back Faces
```cpp
// In vertex shader, push back faces slightly farther
if (isFrontFacing(worldPos, cameraPos)) {
    gl_Position = viewProj * vec4(cameraRelativePos, 1.0);
} else {
    // Push back face 1000 meters farther
    vec3 pushBack = normalize(cameraRelativePos) * 1000.0;
    gl_Position = viewProj * vec4(cameraRelativePos + pushBack, 1.0);
}
```

### Test 3: Vertex Consistency Check
```cpp
// Log when same world position yields different camera-relative positions
// Compare vertices at patch boundaries
// Should be identical but might have small differences
```

### Test 4: Logarithmic Depth Buffer
- Switch from linear to logarithmic depth buffer
- Should dramatically improve precision at distance
- If dots disappear, confirms depth precision issue

## TEST RESULT: DOTS WORSEN WITH DISTANCE! ✓

**Observation**: 
- Dots are MUCH worse when farther from planet
- Dots gradually disappear as camera approaches surface
- This is classic depth buffer precision loss!

**Confirmed Diagnosis: DEPTH BUFFER PRECISION**
- At 2.15 million meters from origin, the depth buffer can't distinguish front/back faces
- The 24-bit depth buffer has limited precision spread across huge distance range
- Farther away = worse precision = more dots
- Closer = better precision = fewer dots

## What Each Test Result Means

### If Dots Are STABLE (Same Position When Zooming)
**Diagnosis: Actual mesh gaps**
- Vertices are misaligned between patches
- Or triangles are degenerate/missing
- Or T-junction resolution is creating holes

**Solutions:**
1. Fix vertex alignment at patch boundaries
2. Ensure consistent vertex generation 
3. Debug T-junction stitching code
4. Add overlap between adjacent patches

### If Dots FLICKER/MOVE (Change With Camera)
**Diagnosis: Depth precision or floating point issues**
- Not actual holes, just rendering artifacts
- Depth buffer can't distinguish front/back faces
- Or vertex positions calculated differently each frame

**Solutions:**
1. Logarithmic depth buffer
2. Depth bias for back faces
3. Higher precision calculations
4. Separate front/back rendering passes

### If Dots Change at SPECIFIC DISTANCES
**Diagnosis: LOD transition issues**
- Different LOD levels create different gaps
- Patch subdivision changes create/remove holes
- T-junction resolution only works at some LODs

**Solutions:**
1. Fix LOD transition blending
2. Ensure watertight mesh at all LOD levels
3. Better T-junction handling across LOD boundaries
4. Morphing between LOD levels

## Solutions to Consider (After Testing)

1. **Depth Bias for Back Faces**
   - Apply small depth offset to push back faces farther
   - Simple but might cause artifacts at silhouette
   
2. **Improved Depth Buffer Precision**
   - Use reversed Z (1.0 near, 0.0 far) 
   - Or logarithmic depth buffer
   - Better precision distribution
   
3. **Separate Front/Back Rendering**
   - Render front faces first
   - Clear depth buffer
   - Skip back faces entirely (once culling works)
   
4. **Fix Vertex Generation Consistency**
   - Ensure identical vertices get identical transforms
   - Cache transformed positions at patch boundaries
   - Use higher precision for critical calculations

## Next Steps

1. **Verify the theory** - Add diagnostics to confirm depth values are competing
2. **Test predictions** - Implement test shaders to validate our understanding  
3. **Choose solution** - Based on test results, pick most appropriate fix
4. **Implement fix** - Make minimal change that solves the problem
5. **Verify no side effects** - Ensure fix doesn't break other rendering

## Success Criteria

- No dots visible at any viewing angle
- No performance degradation
- No new artifacts introduced
- Works with backface culling re-enabled