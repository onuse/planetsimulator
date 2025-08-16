# Diagnosis: Double Planet Rendering Issue

## Symptoms
1. **Visual**: Appears to be "rendering two different planets at once"
2. **Black hole/missing region**: Some area is not being rendered
3. **Previously fixed**: std::round() bug in vertex snapping was already fixed
4. **Transforms verified**: Cross-face boundaries align correctly

## Analysis of Code Flow

### 1. Patch Collection (SphericalQuadtree)
- 6 root nodes created, one per cube face
- Each root spans from -1 to 1 in non-fixed dimensions
- `selectLOD` recursively collects visible patches into `visiblePatches` vector
- **Potential Issue**: Patches collected twice in update() - once for subdivision, once for final render

### 2. Instance Buffer Update (LODManager)
- Gets patches from quadtree->getVisiblePatches()
- For each patch, creates a GlobalPatch and transform matrix
- Uploads transform matrices to instance buffer
- **Instance count**: Set to patches.size()

### 3. Vertex Generation (Shader)
- Each instance uses patchTransform from instance buffer
- UV coordinates transformed to cube space
- Cube coordinates projected to sphere
- **Potential Issue**: gl_InstanceIndex might exceed array bounds

### 4. Draw Call
- vkCmdDrawIndexed with instanceCount = number of patches
- **Debug output shows**: Sometimes very high instance counts (>5000)

## Most Likely Causes

### Theory 1: Patch Collection Duplication
- In SphericalQuadtree::update(), visiblePatches.clear() is called TWICE
- First collection for subdivision decisions
- Second collection after subdivisions
- **If clear() fails or is skipped, patches accumulate**

### Theory 2: Transform Matrix Issues
- Mirroring detected between +X and -X faces
- Each face's transform could be creating geometry on opposite side
- **Double planet = same geometry mirrored across origin**

### Theory 3: Instance Buffer Not Cleared
- Instance buffer grows but old data might persist
- Draw call might use stale instance count
- **Would explain increasing complexity over time**

### Theory 4: Face Visibility Culling Disabled
- All 6 faces might be rendered regardless of camera position
- Backface culling disabled for testing
- **Would show opposite side of planet through the front**

## Recommended Fix Priority

1. **Check visiblePatches clearing**:
   - Ensure visiblePatches.clear() is called properly
   - Log size before and after clear()
   - Verify no accumulation between frames

2. **Add patch ID coloring**:
   - Assign unique color to each face ID
   - This will show if opposite faces are both visible

3. **Verify instance count**:
   - Log instanceCount at every stage
   - Ensure it matches patches.size() exactly
   - Check if draw is called multiple times

4. **Fix transform matrices**:
   - Ensure transforms don't create mirrored geometry
   - Verify each face only renders on its side

## Quick Test
Add this logging to LODManager::render():
```cpp
std::cout << "[RENDER] Drawing " << quadtreeData.instanceCount 
          << " instances, patches.size()=" << quadPatches.size() << std::endl;
```

If instanceCount > patches.size(), there's accumulation.
If both are high but equal, check for duplicate collection.
