# System Interaction Map - Planet Simulator

## The Core Problem
**ENTIRE CUBE FACES ARE MISSING** from the rendered output, not just small gaps.

## System Components & Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                          MAIN LOOP                               │
│                         (main.cpp)                               │
└─────────────────┬───────────────────────────────────────────────┘
                  │
                  ├── Camera Update
                  ├── Input Handling  
                  └── Render Frame
                      │
    ┌─────────────────▼────────────────────────────────┐
    │            VulkanRenderer::render()              │
    │         (vulkan_renderer.cpp)                    │
    │  - Begins command buffer                         │
    │  - Calls lodManager->render()                    │
    │  - Submits to GPU                                │
    └─────────────────┬────────────────────────────────┘
                      │
    ┌─────────────────▼────────────────────────────────┐
    │           LODManager::render()                   │
    │         (lod_manager.cpp)                        │
    │  - Decides render mode based on altitude         │
    │  - Updates SphericalQuadtree                     │
    │  - Gets visible patches                          │
    │  - Generates/uploads vertex data                 │
    └────┬──────────────────────────────────┬──────────┘
         │                                  │
         │ QUADTREE PATH                    │ OCTREE PATH
         │ (Far view)                       │ (Near view)
         │                                  │
    ┌────▼─────────────────┐          ┌────▼─────────────────┐
    │  SphericalQuadtree    │          │     Octree           │
    │ (spherical_quadtree.  │          │   (octree.cpp)       │
    │        cpp)           │          │                      │
    │                       │          │ - Voxel data         │
    │ - 6 root nodes        │          │ - Material info      │
    │ - Subdivides faces    │          │ - Used < 50km alt    │
    │ - LOD selection       │          └──────────────────────┘
    │ - Returns patches     │
    └────┬──────────────────┘
         │
    ┌────▼─────────────────────────────────────────┐
    │          QuadtreePatch Structure             │
    │                                               │
    │  - corners[4]: World space positions         │
    │  - center: World space center                │
    │  - patchTransform: UV→World matrix            │
    │  - faceId: Which cube face (0-5)             │
    │  - level: Subdivision level                  │
    └────┬──────────────────────────────────────────┘
         │
    ┌────▼─────────────────────────────────────────┐
    │         CPUVertexGenerator                   │
    │    (cpu_vertex_generator.cpp)                │
    │                                               │
    │  - Takes QuadtreePatch                       │
    │  - Generates grid (e.g., 33x33)              │
    │  - For each vertex:                           │
    │    • UV → Cube (via patchTransform)          │
    │    • Cube → Sphere projection                │
    │    • Sample density field                    │
    │    • Create Vertex struct                    │
    └────┬──────────────────────────────────────────┘
         │
    ┌────▼─────────────────────────────────────────┐
    │           Vertex Buffer Upload                │
    │                                               │
    │  struct Vertex {                             │
    │    vec3 position;  // CAMERA-RELATIVE!      │
    │    vec3 color;     // Debug face color       │
    │    vec3 normal;                              │
    │    vec2 texCoord;                            │
    │  }                                            │
    └────┬──────────────────────────────────────────┘
         │
    ┌────▼─────────────────────────────────────────┐
    │            GPU Vertex Shader                  │
    │         (triangle.vert)                      │
    │                                               │
    │  - Receives camera-relative position         │
    │  - gl_Position = viewProj * position         │
    │  - Reconstructs world pos for altitude       │
    └────┬──────────────────────────────────────────┘
         │
    ┌────▼─────────────────────────────────────────┐
    │           GPU Fragment Shader                 │
    │         (triangle.frag)                       │
    │                                               │
    │  - Receives interpolated color               │
    │  - Currently showing debug face colors       │
    │  - Should show terrain colors                │
    └──────────────────────────────────────────────┘
```

## Key Classes and Their Responsibilities

### 1. **SphericalQuadtree**
- **File**: `core/spherical_quadtree.cpp`
- **Creates**: 6 root nodes (one per cube face)
- **Does**: 
  - LOD selection based on screen space error
  - Face visibility culling (currently DISABLED)
  - Patch collection for rendering
- **Outputs**: `std::vector<QuadtreePatch>`

### 2. **SphericalQuadtreeNode**
- **File**: `core/spherical_quadtree.cpp`
- **Represents**: One node in the quadtree
- **Can**: Subdivide into 4 children
- **Stores**: QuadtreePatch data

### 3. **GlobalPatchGenerator**
- **File**: `core/global_patch_generator.hpp`
- **Purpose**: Create consistent transforms for patches
- **Key Method**: `createTransform()` - converts UV [0,1] to world space
- **Our Fix**: Was here (MIN_RANGE issue)

### 4. **LODManager**
- **File**: `rendering/lod_manager.cpp`
- **Decides**: Which rendering system to use (quadtree vs octree)
- **Manages**: Transition between systems based on altitude
- **Calls**: SphericalQuadtree::update() or Octree methods

### 5. **CPUVertexGenerator**
- **File**: `rendering/cpu_vertex_generator.cpp`
- **Input**: QuadtreePatch
- **Output**: Vertex data ready for GPU
- **Process**: UV → Cube → Sphere → Sample terrain → Create vertices

### 6. **VulkanRenderer**
- **File**: `rendering/vulkan_renderer.cpp`
- **Manages**: All Vulkan resources
- **Renders**: Whatever LODManager provides
- **Key Issue**: Are all faces' data actually being uploaded?

## Critical Questions to Answer

### Q1: How many patches are generated?
- Check: `SphericalQuadtree::getVisiblePatches().size()`
- Expected: Patches from all 6 faces
- Reality: ?

### Q2: How many patches reach the GPU?
- Check: LODManager's vertex buffer upload count
- Check: Draw call instance count

### Q3: Is face visibility working correctly?
- `enableFaceCulling = false` but are faces still being skipped?
- Check the actual subdivision logic

### Q4: Is the vertex data correct?
- Are vertices being transformed correctly?
- Is camera-relative positioning working?

### Q5: Is it a rendering state issue?
- Depth testing?
- Blend modes?
- Viewport settings?

## Suspicious Areas

### 1. **Face Generation**
```cpp
// In SphericalQuadtree::update()
for (int i = 0; i < 6; i++) {
    auto& root = roots[i];
    if (!root) continue;  // <- Could roots be null?
    // ...
    root->selectLOD(...);  // <- Does this actually generate patches?
}
```

### 2. **Patch Collection**
Where exactly do patches get added to `visiblePatches`?
Is every face's patches being collected?

### 3. **Vertex Generation**
Is CPUVertexGenerator being called for all patches?
Or only some subset?

### 4. **GPU Upload**
Are we uploading all vertex data?
Or is there a limit/buffer size issue?

## Debug Strategy

### Step 1: Count Everything
Add counters at each stage:
- How many root faces processed
- How many patches generated per face
- How many vertices generated
- How many vertices uploaded
- How many draw calls issued

### Step 2: Isolate One Face
Modify code to ONLY render face 0 (+X)
Does it render completely?
Then add face 4 (+Z)
Do both render?

### Step 3: Bypass Systems
Skip LODManager, directly create 6 simple quads (one per face)
Do they all render?
This tests if Vulkan pipeline is working.

### Step 4: Trace One Face
Pick face 4 (+Z) which seems to render
Pick face 0 (+X) which might be missing
Trace them through the ENTIRE pipeline
Where do they diverge?

## The Real Problem
We keep fixing mathematical correctness (vertices align, transforms are correct)
But the issue is **entire faces are missing from rendering**
This suggests a higher-level logic error, not a math error.

Possibilities:
1. **Faces aren't being processed** - Loop exits early?
2. **Patches aren't being generated** - Subdivision fails?
3. **Vertices aren't being created** - Generator skips them?
4. **Data isn't reaching GPU** - Upload fails?
5. **GPU isn't rendering them** - State issue?

We need systematic logging at EVERY stage to find where faces disappear.