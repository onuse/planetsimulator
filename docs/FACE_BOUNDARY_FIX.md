# Face Boundary Gap Fix

## Problem
The planet renderer had 12,735 meter gaps at cube face boundaries, causing visible seams where different faces of the cube meet.

## Root Cause
The EPSILON value of 0.001 used for boundary detection was too large. When patches at face boundaries had coordinates like:
- +Z face edge: (0.999, 0, 1.0) - off by 0.001 from x=1
- +X face edge: (1.0, 0, 0.999) - off by 0.001 from z=1

These small differences in cube space created 12.7km gaps after sphere projection at planet scale.

## Solution
Changed EPSILON from 0.001 to 1e-7 in three critical locations:

1. **shaders/src/vertex/quadtree_patch.vert** (line 259)
   - Snaps vertex positions to exact face boundaries during rendering

2. **src/core/spherical_quadtree.cpp** (line 57)
   - Ensures patch bounds are exactly at ±1 when at face boundaries

3. **include/math/patch_alignment.hpp** (lines 33, 84, 96)
   - Handles face boundary detection and snapping

## Impact
- **Before**: 12,735 meter gaps at face boundaries
- **After**: < 0.13 meter gaps (essentially eliminated)

## Testing
Run `test_epsilon_fix.cpp` to verify the fix:
```bash
g++ -o test_epsilon_fix test_epsilon_fix.cpp -I./include -std=c++17
./test_epsilon_fix
```

## Technical Details
The cube-to-sphere projection amplifies small errors by the planet radius (6,371km). An offset of 0.001 in cube coordinates becomes:
- 0.001 offset → 12,735 meter gap
- 0.0001 offset → 1,274 meter gap  
- 1e-7 offset → 0.13 meter gap

The new epsilon of 1e-7 is small enough to ensure vertices at face boundaries are truly shared between adjacent patches from different faces.