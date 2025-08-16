# Planet Simulator Rendering Pipeline
## From Mathematical Planet to GPU Pixels

### Table of Contents
1. [Overview](#overview)
2. [Stage 1: Planet Definition](#stage-1-planet-definition)
3. [Stage 2: Spatial Organization](#stage-2-spatial-organization)
4. [Stage 3: LOD Selection](#stage-3-lod-selection)
5. [Stage 4: Mesh Generation](#stage-4-mesh-generation)
6. [Stage 5: GPU Upload](#stage-5-gpu-upload)
7. [Stage 6: Vertex Transformation](#stage-6-vertex-transformation)
8. [Stage 7: Fragment Shading](#stage-7-fragment-shading)
9. [Known Issues & Suspicions](#known-issues--suspicions)

---

## Overview

The planet renderer uses a **cube-sphere** approach where a cube is projected onto a sphere. The cube has 6 faces, each subdivided into patches using a quadtree structure.

```
Mathematical Planet → Cube Mapping → Quadtree Patches → Mesh Generation → GPU Rendering
```

---

## Stage 1: Planet Definition

### Location: `core/density_field.cpp`, `main.cpp`

The planet starts as:
- **Radius**: 6,371,000 meters (Earth-sized)
- **Density Field**: Determines terrain height at any 3D point
- **Material System**: Different materials (rock, water, etc.) based on density

### Cube-Sphere Mapping
The planet uses a cube projected to a sphere. Each point on the cube is projected using:
```cpp
// From cube position to sphere
vec3 pos2 = cubePos * cubePos;
spherePos.x = cubePos.x * sqrt(1.0 - pos2.y*0.5 - pos2.z*0.5 + pos2.y*pos2.z/3.0);
spherePos.y = cubePos.y * sqrt(1.0 - pos2.x*0.5 - pos2.z*0.5 + pos2.x*pos2.z/3.0);
spherePos.z = cubePos.z * sqrt(1.0 - pos2.x*0.5 - pos2.y*0.5 + pos2.x*pos2.y/3.0);
```

**The 6 Cube Faces:**
- Face 0: +X (x = 1, y and z vary from -1 to 1)
- Face 1: -X (x = -1, y and z vary)
- Face 2: +Y (y = 1, x and z vary)
- Face 3: -Y (y = -1, x and z vary)
- Face 4: +Z (z = 1, x and y vary)
- Face 5: -Z (z = -1, x and y vary)

---

## Stage 2: Spatial Organization

### Location: `core/spherical_quadtree.cpp`, `rendering/lod_manager.cpp`

The planet uses two parallel systems:

### SphericalQuadtree (Far View)
- 6 root nodes (one per cube face)
- Each node can subdivide into 4 children
- Used for distant viewing (space view)
- Handles face culling

### Octree (Near View) 
- Single root covering entire planet
- Each node subdivides into 8 children
- Used for surface-level detail
- Contains actual voxel data and materials

### LODManager
Switches between systems based on altitude:
- **Altitude > 100km**: Quadtree only
- **Altitude 50-100km**: Transition zone
- **Altitude < 50km**: Octree with Transvoxel

---

## Stage 3: LOD Selection

### Location: `core/spherical_quadtree.cpp::update()`

Each frame:
1. Calculate camera position and view frustum
2. For each quadtree node:
   - Check if in frustum
   - Calculate screen-space error
   - Subdivide if error too large
3. Collect visible patches

### Patch Structure (`QuadtreePatch`)
```cpp
struct QuadtreePatch {
    vec3 corners[4];       // World-space corners
    vec3 center;           // World-space center
    dmat4 patchTransform;  // UV [0,1]² to world transform
    int faceId;            // Which cube face (0-5)
    int level;             // Subdivision level
}
```

---

## Stage 4: Mesh Generation

### Two Paths:

### Path A: CPU Vertex Generation (Quadtree)
**Location:** `rendering/cpu_vertex_generator.cpp`

1. Takes a `QuadtreePatch`
2. Creates a regular grid (e.g., 33x33 vertices)
3. For each grid point (u,v):
   - Transform UV to cube position using `patchTransform`
   - Project cube position to sphere
   - Sample density field for height
   - Generate vertex with position, normal, color

### Path B: Transvoxel (Octree)
**Location:** `algorithms/mesh_generation.cpp`

1. Takes octree nodes with voxel data
2. Uses marching cubes variant
3. Handles LOD transitions with special cases
4. Generates triangle mesh directly from voxels

---

## Stage 5: GPU Upload

### Location: `rendering/vulkan_renderer.cpp`, `rendering/lod_manager.cpp`

### Vertex Data Flow:
```
CPU Mesh → Staging Buffer → GPU Vertex Buffer → Draw Call
```

### Vertex Format:
```cpp
struct Vertex {
    vec3 position;  // Camera-relative position!
    vec3 color;     
    vec3 normal;
    vec2 texCoord;
}
```

**CRITICAL:** Positions are **camera-relative** (world position - camera position) to avoid floating-point precision issues at planet scale.

---

## Stage 6: Vertex Transformation

### Location: `shaders/src/vertex/triangle.vert`

The vertex shader:
1. Receives camera-relative position
2. Transforms to clip space: `gl_Position = viewProj * vec4(relativePos, 1)`
3. Reconstructs world position for altitude: `worldPos = relativePos + cameraPos`
4. Calculates altitude for terrain coloring

### Transform Matrices:
- **View Matrix**: Has translation removed (camera at origin)
- **Projection Matrix**: Standard perspective projection
- **ViewProj**: Combined matrix

---

## Stage 7: Fragment Shading

### Location: `shaders/src/fragment/triangle.frag`

1. Receives interpolated data from vertex shader
2. Applies altitude-based coloring (ocean, beach, grass, rock, snow)
3. Calculates lighting (sun direction, ambient)
4. Applies atmospheric scattering
5. Outputs final pixel color

---

## Known Issues & Suspicions

### 1. **Black Gaps at Face Boundaries (CURRENT ISSUE)**
- **Symptom**: Black lines where cube faces meet
- **Our Fix**: GlobalPatchGenerator transform corrected
- **Test Result**: Vertices align perfectly (0m gap)
- **But**: Visual gaps still appear

**Possible Causes:**
- Patches might not actually be generated at face boundaries?
- Transform might be applied differently in actual rendering vs tests?
- Floating-point precision loss during GPU transformation?
- Z-fighting between overlapping patches?

### 2. **Transform Pipeline Complexity**
Multiple transform stages might compound errors:
```
UV [0,1] → Patch Transform → Cube [-1,1] → Sphere projection → 
Camera-relative → GPU upload → Vertex shader → Clip space
```

### 3. **Two Parallel Systems**
- Quadtree and Octree systems might not align properly
- Transition zone (50-100km altitude) might have issues
- Different mesh generation methods (CPU vs Transvoxel)

### 4. **Camera-Relative Rendering**
- Positions are pre-transformed on CPU: `vertex.pos = worldPos - cameraPos`
- Shader reconstructs world position: `worldPos = vertex.pos + cameraPos`
- Any mismatch in camera position could cause issues

### 5. **Patch Generation**
**Question**: Are patches actually generated to cover face boundaries?
- Do we create patches that span across faces?
- Or do we only create patches within each face?
- If patches stop at face boundaries, there WILL be gaps!

---

## Critical Questions to Investigate

1. **Patch Coverage**: 
   - When we're near a face edge (e.g., x=0.99 on +X face), do we generate a patch that extends to x=1.0?
   - Or do patches stop short of face boundaries?

2. **Transform Consistency**:
   - Is `GlobalPatchGenerator::createTransform()` actually used by all rendering paths?
   - Is the transform applied the same way in tests vs actual rendering?

3. **Vertex Deduplication**:
   - Are we accidentally removing shared vertices thinking they're duplicates?
   - Does the vertex buffer contain the actual boundary vertices?

4. **Debug Visualization**:
   - The planet shows colored patches - is this debug coloring by face?
   - Are we rendering face boundaries with a debug color/wireframe?

5. **Subdivision Logic**:
   - Does the quadtree subdivision stop at face boundaries?
   - Should patches be allowed to cross face boundaries?

---

## Next Steps

1. **Verify Patch Coverage**: Check if patches actually reach face boundaries
2. **Trace Single Vertex**: Follow one boundary vertex from generation to screen
3. **Add Debug Output**: Log patch bounds when near face boundaries
4. **Check GPU Data**: Dump actual vertex buffer contents to verify positions
5. **Simplify Test Case**: Render just 2 adjacent patches from different faces

The black gaps suggest something fundamental is wrong in how we handle face boundaries. The math says vertices align, but the GPU says otherwise!