# Debug Render Tracking Points

## Places to Add Logging

### 1. SphericalQuadtree::update()
```cpp
// In src/core/spherical_quadtree.cpp around line 481
for (int i = 0; i < 6; i++) {
    auto& root = roots[i];
    
    // ADD THIS:
    RenderLogger::getInstance().logMessage("Processing face " + std::to_string(i));
    
    if (!root) {
        RenderLogger::getInstance().logMessage("  Face " + std::to_string(i) + " root is NULL!");
        continue;
    }
    
    root->selectLOD(...);
    
    // ADD THIS:
    RenderLogger::getInstance().logMessage("  Face " + std::to_string(i) + 
        " generated " + std::to_string(patchCount) + " patches");
}
```

### 2. LODManager::render()
```cpp
// In src/rendering/lod_manager.cpp
void LODManager::render(...) {
    // ADD THIS:
    auto& logger = RenderLogger::getInstance();
    logger.startFrame(frameNumber++);
    
    // After getting patches:
    auto patches = quadtree->getVisiblePatches();
    for (const auto& patch : patches) {
        logger.logPatchGenerated(patch.faceId, patch.center, patch.level);
    }
    
    // Before draw call:
    logger.logDrawCall(vertexCount, indexCount, instanceCount, "Quadtree patches");
}
```

### 3. VulkanRenderer command recording
```cpp
// In src/rendering/vulkan_renderer.cpp
vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
// ADD THIS:
RenderLogger::getInstance().logDrawCall(0, indexCount, instanceCount, "Vulkan Draw");
```

## What We Can Track

1. **Per-Face Statistics**
   - How many patches generated per face
   - How many vertices per face
   - Which faces have 0 patches (missing!)

2. **Draw Call Analysis**
   - Number of draw calls
   - Total vertices/indices submitted
   - Instance counts

3. **Pipeline Stage Tracking**
   - Where patches are created
   - Where they're filtered out
   - Where they reach GPU

## RenderDoc Alternative

If you have RenderDoc working:
1. Launch the game through RenderDoc
2. Press F12 to capture a frame
3. In RenderDoc you can see:
   - Event Browser: Every draw call
   - Mesh Output: The actual triangles
   - Pipeline State: What's bound
   - Textures/Buffers: Raw data

This shows EXACTLY what the GPU sees.

## Quick Test: Count Draw Calls

Add this simple counter:
```cpp
// In vulkan_renderer.cpp
static int drawCallCount = 0;
vkCmdDrawIndexed(...);
drawCallCount++;
if (drawCallCount % 60 == 0) {
    std::cout << "Draw calls this frame: " << drawCallCount/60 << std::endl;
    drawCallCount = 0;
}
```

## The Key Question

**Are we making 6 draw calls (one per face) or fewer?**

If fewer than 6, faces are being filtered out before reaching the GPU.
If 6, then the issue is in the vertex data or GPU state.