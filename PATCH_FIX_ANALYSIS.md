# Patch Discontinuity Root Cause Analysis and Fix

## Problem Identification

The terrain appears as "jammed puzzle pieces" with visible discontinuities between patches. Testing reveals that adjacent patches are sampling terrain at completely different 3D positions.

## Root Cause

There's a **coordinate mapping inconsistency** in `GlobalPatchGenerator::createTransform()`:

### Current Broken Logic:
For the +X face (X=1 fixed):
- UV mapping: U->Y, V->Z
- Child layout assumes: Children 0,1 share a vertical edge (varying Y, constant Z)
- But U=1 on child 0 should equal U=0 on child 1
- With U->Y mapping: child 0's U=1 -> maxY, child 1's U=0 -> minY 
- These map to DIFFERENT positions even though they should be the SAME edge!

### Test Results:
```
Testing shared edge between child patches 0 and 1:
V=0.0: Child0 cube(1,0,-1) | Child1 cube(1,-1,0) <- Completely different positions!
```

## The Fix

We need to ensure the UV mapping is consistent with the subdivision layout:

### Option 1: Fix the UV Mapping (RECOMMENDED)
Change the transform creation to match the logical child layout:
- For +X/-X faces: U->Z, V->Y (not U->Y, V->Z)
- For +Y/-Y faces: Keep U->X, V->Z  
- For +Z/-Z faces: Keep U->X, V->Y

This ensures:
- Children 0,1 (left/right) differ in U, which maps to the first varying dimension
- Children 0,3 (bottom/top) differ in V, which maps to the second varying dimension

### Option 2: Fix the Child Layout
Alternatively, reorder children in subdivide() to match current UV mapping.
This is more complex as it affects neighbor connections.

## Implementation

The fix should be applied in `global_patch_generator.hpp` line 89-118:

```cpp
if (range.x < eps) {
    // X is fixed - patch is on +X or -X face
    double x = (minBounds.x + maxBounds.x) * 0.5;
    
    // FIX: U maps to Z (horizontal), V maps to Y (vertical)
    transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
    transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
    transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
}
```

## Verification Plan

After applying the fix:
1. Run `test_terrain_consistency` - should show matching heights at shared edges
2. Visual inspection - planet should appear continuous without "puzzle piece" artifacts
3. Check face boundaries - corners where 3 faces meet should be seamless

## Additional Issues to Address

1. **Floating-point precision**: Even with correct mapping, slight numerical errors can cause discontinuities
   - Solution: Add epsilon snapping at face boundaries (already partially implemented)

2. **T-junction prevention**: LOD differences can create gaps
   - Solution: Vertex snapping in shader (already implemented but may need tuning)

3. **Skirt geometry**: May not extend far enough at face boundaries
   - Solution: Increase skirt depth or adjust skirt generation