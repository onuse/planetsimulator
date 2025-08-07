# Octree Ray Marching Shader Specification

## Purpose
Render a voxel octree planet by ray marching through space and traversing the octree structure to find material voxels.

## Input Data Structures

### GPU Octree Node (32 bytes)
```
struct OctreeNode {
    vec4 centerAndSize;      // xyz = world position center, w = half-size of node
    uvec4 childrenAndFlags;  // x = children offset, y = voxel offset, z = flags, w = reserved
}
```

### Flags Encoding (32-bit)
- Bit 0: Is leaf node (1 = leaf, 0 = internal)
- Bits 8-15: Material type (0=Air, 1=Rock, 2=Water, 3=Magma)
- Other bits: Reserved

### Node Buffer Layout
- Index 0: Root node (encompasses entire planet)
- Children of node N start at index `nodes[N].childrenAndFlags.x`
- 8 children per internal node, indexed 0-7 based on octant

## Algorithm Steps

### 1. Ray Setup
```
INPUT: Camera position (rayOrigin), pixel coordinates (uv)
OUTPUT: Ray direction (rayDir)

1. Transform pixel UV from screen space [-1,1] to world space
2. Calculate normalized ray direction from camera to pixel
```

### 2. Planet Intersection Test
```
INPUT: rayOrigin, rayDir, planetCenter (0,0,0), planetRadius
OUTPUT: Hit distance or -1 if miss

1. Solve ray-sphere intersection equation
2. If no intersection, return black (space)
3. Calculate entry point: rayStart = rayOrigin + rayDir * hitDistance
```

### 3. Octree Traversal
```
INPUT: rayStart, rayDir, nodeBuffer
OUTPUT: Material type and hit position

ALGORITHM:
currentPosition = rayStart
stepSize = minStepSize
maxSteps = 200

FOR each step from 0 to maxSteps:
    1. Find current octree node:
       a. Start at root (index 0)
       b. WHILE node is not leaf AND has valid children:
          - Calculate octant from position relative to node center
          - octant = 0
          - if (pos.x > center.x) octant |= 1
          - if (pos.y > center.y) octant |= 2  
          - if (pos.z > center.z) octant |= 4
          - childIndex = node.childrenOffset + octant
          - node = nodeBuffer[childIndex]
       
    2. If node is leaf:
       - Extract material from flags: (flags >> 8) & 0xFF
       - If material != Air (0):
          RETURN material and position
    
    3. Advance ray:
       - currentPosition += rayDir * stepSize
       - stepSize = max(minStep, currentNodeSize * 0.5)
    
    4. Check termination:
       - If distance(currentPosition, planetCenter) > planetRadius: BREAK
       - If total distance > maxDistance: BREAK

RETURN Air (no hit)
```

### 4. Material Shading
```
INPUT: Material type, hit position, light direction
OUTPUT: Final color

1. Map material to base color:
   - Air: transparent
   - Rock: brown (0.5, 0.4, 0.3)
   - Water: blue (0.0, 0.3, 0.7)
   - Magma: orange (1.0, 0.3, 0.0)

2. Calculate lighting:
   - normal = normalize(hitPosition - planetCenter)
   - NdotL = max(0, dot(normal, lightDir))
   - color = baseColor * (0.3 + 0.7 * NdotL)
```

## Critical Invariants

1. **Node Indexing**: Children of node N are at indices [N.childrenOffset, N.childrenOffset+7]
2. **Octant Calculation**: Must be consistent between CPU generation and GPU traversal
3. **Leaf Detection**: Check bit 0 of flags (flags & 1)
4. **Material Extraction**: Bits 8-15 of flags ((flags >> 8) & 0xFF)
5. **Bounds Checking**: Never access nodeBuffer beyond valid indices

## Common Pitfalls

1. **Wrong Planet Radius**: Mismatch between generation and rendering
2. **Invalid Child Offsets**: 0xFFFFFFFF means no children
3. **Octant Miscalculation**: Position must be relative to current node center
4. **Premature Termination**: Stepping too far or checking bounds incorrectly
5. **Material in Wrong Bits**: Ensure bit shifting is correct

## Test Cases

1. **Ray from outside hitting planet**: Should find surface material
2. **Ray from inside planet**: Should immediately find material
3. **Ray missing planet**: Should return space/black
4. **Deep octree traversal**: Should handle multiple levels correctly
5. **Leaf with Air material**: Should continue marching
6. **Leaf with solid material**: Should stop and shade