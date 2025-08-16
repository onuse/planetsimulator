#include "rendering/lod_manager.hpp"
#include "core/global_patch_generator.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace rendering {

LODManager::LODManager(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue)
    : device(device), physicalDevice(physicalDevice),
      commandPool(commandPool), graphicsQueue(graphicsQueue),
      currentMode(QUADTREE_ONLY), transitionBlendFactor(0.0f), currentAltitude(0.0f) {
}

LODManager::~LODManager() {
    // Clean up Vulkan resources
    if (device != VK_NULL_HANDLE) {
        destroyBuffer(quadtreeData.vertexBuffer, quadtreeData.vertexMemory);
        destroyBuffer(quadtreeData.indexBuffer, quadtreeData.indexMemory);
        destroyBuffer(quadtreeData.instanceBuffer, quadtreeData.instanceMemory);
        
        // Clean up octree chunks
        if (transvoxelRenderer) {
            for (auto& chunk : octreeChunks) {
                transvoxelRenderer->destroyChunkBuffers(chunk);
            }
        }
    }
}

void LODManager::initialize(float planetRadius, uint32_t seed) {
    std::cout << "[LODManager] Initializing with planet radius: " << planetRadius << ", seed: " << seed << std::endl;
    
    // Initialize density field (SDF foundation)
    densityField = std::make_shared<core::DensityField>(planetRadius, seed);
    
    // Initialize spherical quadtree for surface rendering
    core::SphericalQuadtree::Config quadConfig;
    quadConfig.planetRadius = planetRadius;
    quadConfig.maxLevel = 20;
    quadConfig.pixelError = 2.0f;
    quadConfig.morphRegion = 0.3f;
    quadConfig.enableMorphing = true;
    quadConfig.enableCrackFixes = true;
    quadConfig.enableFaceCulling = false;  // TEMPORARILY DISABLED FOR DEBUGGING
    quadConfig.enableFrustumCulling = false;  // DISABLED - test without frustum culling
    quadConfig.enableDistanceCulling = false;  // DISABLED - test without distance culling
    
    quadtree = std::make_unique<core::SphericalQuadtree>(quadConfig, densityField);
    
    // Initialize octree for volumetric rendering
    octreePlanet = std::make_unique<octree::OctreePlanet>(planetRadius, 10);
    octreePlanet->generate(seed);
    
    // Initialize Transvoxel renderer
    transvoxelRenderer = std::make_unique<TransvoxelRenderer>(
        device, physicalDevice, commandPool, graphicsQueue
    );
    
    // Initialize CPU vertex generator
    CPUVertexGenerator::Config vertexGenConfig;
    vertexGenConfig.gridResolution = 65;  // 65x65 vertices per patch
    vertexGenConfig.planetRadius = planetRadius;
    vertexGenConfig.enableSkirts = false;  // Disabled for debugging
    vertexGenConfig.skirtDepth = 500.0f;
    vertexGenConfig.enableVertexCaching = true;
    vertexGenConfig.maxCacheSize = 100000;
    vertexGenerator = std::make_unique<CPUVertexGenerator>(vertexGenConfig);
    
    // Create initial buffers for quadtree rendering
    // Base mesh is a subdivided quad with skirt vertices
    const uint32_t baseResolution = 64; // 64x64 grid per patch
    const uint32_t skirtVertexCount = baseResolution * 4; // Skirt vertices around all 4 edges
    const uint32_t vertexCount = baseResolution * baseResolution + skirtVertexCount;
    const uint32_t mainIndexCount = (baseResolution - 1) * (baseResolution - 1) * 6;
    const uint32_t skirtIndexCount = (baseResolution - 1) * 4 * 6; // 4 edges, 2 triangles per quad
    const uint32_t indexCount = mainIndexCount + skirtIndexCount;
    
    // Generate base mesh vertices (unit quad)
    std::vector<glm::vec2> baseVertices;
    baseVertices.reserve(vertexCount);
    
    // Main patch vertices
    for (uint32_t y = 0; y < baseResolution; y++) {
        for (uint32_t x = 0; x < baseResolution; x++) {
            float u = static_cast<float>(x) / (baseResolution - 1);
            float v = static_cast<float>(y) / (baseResolution - 1);
            baseVertices.push_back(glm::vec2(u, v));
        }
    }
    
    // Skirt vertices - push vertices slightly outside the patch
    const float skirtOffset = -0.05f; // Negative to push down/out
    
    // Top edge skirt (v = 0, pushed up)
    for (uint32_t x = 0; x < baseResolution; x++) {
        float u = static_cast<float>(x) / (baseResolution - 1);
        baseVertices.push_back(glm::vec2(u, skirtOffset));
    }
    
    // Bottom edge skirt (v = 1, pushed down)
    for (uint32_t x = 0; x < baseResolution; x++) {
        float u = static_cast<float>(x) / (baseResolution - 1);
        baseVertices.push_back(glm::vec2(u, 1.0f - skirtOffset));
    }
    
    // Left edge skirt (u = 0, pushed left)
    for (uint32_t y = 0; y < baseResolution; y++) {
        float v = static_cast<float>(y) / (baseResolution - 1);
        baseVertices.push_back(glm::vec2(skirtOffset, v));
    }
    
    // Right edge skirt (u = 1, pushed right)
    for (uint32_t y = 0; y < baseResolution; y++) {
        float v = static_cast<float>(y) / (baseResolution - 1);
        baseVertices.push_back(glm::vec2(1.0f - skirtOffset, v));
    }
    
    // Generate indices
    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    
    // Main patch indices
    for (uint32_t y = 0; y < baseResolution - 1; y++) {
        for (uint32_t x = 0; x < baseResolution - 1; x++) {
            uint32_t topLeft = y * baseResolution + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * baseResolution + x;
            uint32_t bottomRight = bottomLeft + 1;
            
            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    // Skirt indices
    uint32_t skirtStart = baseResolution * baseResolution;
    
    // Top edge skirt
    for (uint32_t x = 0; x < baseResolution - 1; x++) {
        uint32_t mainTop = x;
        uint32_t mainTopRight = mainTop + 1;
        uint32_t skirtTop = skirtStart + x;
        uint32_t skirtTopRight = skirtTop + 1;
        
        // First triangle
        indices.push_back(mainTop);
        indices.push_back(skirtTop);
        indices.push_back(mainTopRight);
        
        // Second triangle
        indices.push_back(mainTopRight);
        indices.push_back(skirtTop);
        indices.push_back(skirtTopRight);
    }
    
    // Bottom edge skirt
    uint32_t bottomSkirtStart = skirtStart + baseResolution;
    for (uint32_t x = 0; x < baseResolution - 1; x++) {
        uint32_t mainBottom = (baseResolution - 1) * baseResolution + x;
        uint32_t mainBottomRight = mainBottom + 1;
        uint32_t skirtBottom = bottomSkirtStart + x;
        uint32_t skirtBottomRight = skirtBottom + 1;
        
        // First triangle
        indices.push_back(mainBottom);
        indices.push_back(mainBottomRight);
        indices.push_back(skirtBottom);
        
        // Second triangle
        indices.push_back(mainBottomRight);
        indices.push_back(skirtBottomRight);
        indices.push_back(skirtBottom);
    }
    
    // Left edge skirt
    uint32_t leftSkirtStart = skirtStart + baseResolution * 2;
    for (uint32_t y = 0; y < baseResolution - 1; y++) {
        uint32_t mainLeft = y * baseResolution;
        uint32_t mainLeftBottom = (y + 1) * baseResolution;
        uint32_t skirtLeft = leftSkirtStart + y;
        uint32_t skirtLeftBottom = skirtLeft + 1;
        
        // First triangle
        indices.push_back(mainLeft);
        indices.push_back(mainLeftBottom);
        indices.push_back(skirtLeft);
        
        // Second triangle
        indices.push_back(mainLeftBottom);
        indices.push_back(skirtLeftBottom);
        indices.push_back(skirtLeft);
    }
    
    // Right edge skirt
    uint32_t rightSkirtStart = skirtStart + baseResolution * 3;
    for (uint32_t y = 0; y < baseResolution - 1; y++) {
        uint32_t mainRight = y * baseResolution + (baseResolution - 1);
        uint32_t mainRightBottom = (y + 1) * baseResolution + (baseResolution - 1);
        uint32_t skirtRight = rightSkirtStart + y;
        uint32_t skirtRightBottom = skirtRight + 1;
        
        // First triangle
        indices.push_back(mainRight);
        indices.push_back(skirtRight);
        indices.push_back(mainRightBottom);
        
        // Second triangle
        indices.push_back(mainRightBottom);
        indices.push_back(skirtRight);
        indices.push_back(skirtRightBottom);
    }
    
    // Create vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(glm::vec2) * baseVertices.size();
    createBuffer(vertexBufferSize, 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                quadtreeData.vertexBuffer, quadtreeData.vertexMemory);
    
    // Create index buffer
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    createBuffer(indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                quadtreeData.indexBuffer, quadtreeData.indexMemory);
    
    quadtreeData.indexCount = static_cast<uint32_t>(indices.size());
    
    // Upload vertex data to GPU
    uploadBufferData(quadtreeData.vertexBuffer, baseVertices.data(), vertexBufferSize);
    
    // Upload index data to GPU
    uploadBufferData(quadtreeData.indexBuffer, indices.data(), indexBufferSize);
    
    std::cout << "[LODManager] Initialization complete. Created base mesh with " 
              << baseVertices.size() << " vertices and " << indices.size() << " indices" << std::endl;
}

void LODManager::update(const glm::vec3& cameraPos, const glm::mat4& viewProj, float deltaTime) {
    // Calculate altitude
    currentAltitude = glm::length(cameraPos) - densityField->getPlanetRadius();
    stats.altitude = currentAltitude;
    
    // Select rendering mode based on altitude
    currentMode = selectRenderingMode(currentAltitude);
    stats.mode = currentMode;
    
    // Debug output periodically
    static int updateCount = 0;
    if (updateCount++ % 60 == 0) {
        std::cout << "[LODManager] Altitude: " << currentAltitude << "m, Mode: " 
                  << (currentMode == QUADTREE_ONLY ? "QUADTREE" : 
                      currentMode == TRANSITION_ZONE ? "TRANSITION" : "OCTREE")
                  << std::endl;
    }
    
    // Update quadtree LOD selection
    quadtree->update(cameraPos, viewProj, deltaTime);
    const auto& quadPatches = quadtree->getVisiblePatches();
    stats.quadtreePatches = static_cast<uint32_t>(quadPatches.size());
    
    // DEBUG: Count patches per face
    int faceDebugCounts[6] = {0};
    for (const auto& patch : quadPatches) {
        if (patch.faceId < 6) faceDebugCounts[patch.faceId]++;
    }
    std::cout << "[LODManager] Got " << quadPatches.size() << " patches. Per face: "
              << faceDebugCounts[0] << " " << faceDebugCounts[1] << " " 
              << faceDebugCounts[2] << " " << faceDebugCounts[3] << " "
              << faceDebugCounts[4] << " " << faceDebugCounts[5] << std::endl;
    
    // Update transition blend factor
    if (currentAltitude > config.transitionStartAltitude) {
        transitionBlendFactor = 0.0f;
    } else if (currentAltitude < config.transitionEndAltitude) {
        transitionBlendFactor = 1.0f;
    } else {
        float range = config.transitionStartAltitude - config.transitionEndAltitude;
        transitionBlendFactor = (config.transitionStartAltitude - currentAltitude) / range;
    }
    stats.blendFactor = transitionBlendFactor;
    
    // Update rendering data based on mode
    switch (currentMode) {
        case QUADTREE_ONLY:
            updateQuadtreeBuffersCPU(quadPatches, cameraPos);
            break;
            
        case TRANSITION_ZONE:
            updateQuadtreeBuffersCPU(quadPatches, cameraPos);
            prepareTransitionZone(cameraPos);
            break;
            
        case OCTREE_TRANSVOXEL:
            updateOctreeChunks(cameraPos);
            break;
    }
}

void LODManager::render(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                       const glm::mat4& viewProj) {
    
    static int renderCount = 0;
    if (renderCount++ % 300 == 0 || currentAltitude <= 10000.0f) {
        std::cout << "[LODManager::render] Mode: " << currentMode 
                  << ", Instances: " << quadtreeData.instanceCount
                  << ", Indices: " << quadtreeData.indexCount 
                  << ", Altitude: " << currentAltitude << "m" << std::endl;
    }
    
    switch (currentMode) {
        case QUADTREE_ONLY:
            // Render quadtree patches only
            if (quadtreeData.instanceCount > 0) {
                // NOTE: Pipeline binding should be done by the caller (VulkanRenderer)
                // The caller should bind the appropriate pipeline before calling this render function
                
                // Bind vertex and index buffers
                VkBuffer vertexBuffers[] = {quadtreeData.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, quadtreeData.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                
                // Note: Instance buffer should be bound as a storage buffer via descriptor set,
                // not as a vertex buffer. This is handled by the descriptor set binding.
                
                // Debug output before draw
                static int drawCallCount = 0;
                drawCallCount++;
                
                // Always log when instance count is high or on every 100th call
                if (quadtreeData.instanceCount > 1000 || drawCallCount % 100 == 0) {
                    std::cout << "[LODManager::DrawIndexed] Call #" << drawCallCount 
                              << " - Indices: " << quadtreeData.indexCount
                              << ", Instances: " << quadtreeData.instanceCount
                              << ", Altitude: " << stats.altitude
                              << "m" << std::endl;
                    
                    // Warn about potentially problematic instance counts
                    if (quadtreeData.instanceCount > 5000) {
                        std::cerr << "[WARNING] Very high instance count: " 
                                  << quadtreeData.instanceCount << std::endl;
                    }
                    
                    // Safety check - prevent drawing with excessive instances
                    // Increased limit to handle close-up views with many patches
                    if (quadtreeData.instanceCount > 50000) {
                        std::cerr << "[ERROR] Instance count exceeds safety limit (50000): " 
                                  << quadtreeData.instanceCount << std::endl;
                        std::cerr << "Clamping to prevent potential issues" << std::endl;
                        // Don't skip draw call - just warn
                    }
                }
                
                // Final safety check before draw
                if (quadtreeData.indexCount == 0) {
                    std::cerr << "[ERROR] Attempting to draw with 0 indices!" << std::endl;
                    break;
                }
                
                // Draw instanced
                std::cout << "[DEBUG] Drawing with indexCount=" << quadtreeData.indexCount 
                          << " instanceCount=" << quadtreeData.instanceCount << std::endl;
                vkCmdDrawIndexed(commandBuffer, quadtreeData.indexCount, 
                               quadtreeData.instanceCount, 0, 0, 0);
            }
            break;
            
        case TRANSITION_ZONE:
            // Render both systems with blending
            // First render quadtree with fade-out
            if (quadtreeData.instanceCount > 0) {
                // TODO: Set blend factor uniform
                VkBuffer vertexBuffers[] = {quadtreeData.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, quadtreeData.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandBuffer, quadtreeData.indexCount,
                               quadtreeData.instanceCount, 0, 0, 0);
            }
            
            // Then render octree chunks with fade-in
            if (!octreeChunks.empty()) {
                transvoxelRenderer->render(octreeChunks, commandBuffer, pipelineLayout);
            }
            break;
            
        case OCTREE_TRANSVOXEL:
            // Render octree/transvoxel only
            if (!octreeChunks.empty()) {
                transvoxelRenderer->render(octreeChunks, commandBuffer, pipelineLayout);
            }
            break;
    }
}

LODManager::RenderingMode LODManager::selectRenderingMode(float altitude) {
    if (!config.enableTransitions) {
        // Hard switching
        return altitude > config.transitionEndAltitude ? QUADTREE_ONLY : OCTREE_TRANSVOXEL;
    }
    
    if (altitude > config.transitionStartAltitude) {
        return QUADTREE_ONLY;
    } else if (altitude < config.transitionEndAltitude) {
        return OCTREE_TRANSVOXEL;
    } else {
        return TRANSITION_ZONE;
    }
}

void LODManager::updateQuadtreeBuffers_OLD(const std::vector<core::QuadtreePatch>& patches) {
    // Update instance buffer with patch data
    quadtreeData.instanceCount = static_cast<uint32_t>(patches.size());
    
    if (patches.empty()) return;
    
    // Calculate required instance buffer size
    // Each instance needs: transform matrix, morph params, height texture coords, patch info
    // MUST match the shader struct exactly! Using double precision for transform
    struct InstanceData {
        glm::dmat4 patchTransform;  // Double precision transform matrix
        glm::vec4 morphParams;      // morphFactor, neighborLODs
        glm::vec4 heightTexCoord;
        glm::vec4 patchInfo;        // level, size, unused, unused
    };
    
    VkDeviceSize instanceBufferSize = sizeof(InstanceData) * patches.size();
    
    // Keep track of current buffer size
    static VkDeviceSize currentBufferSize = 0;
    
    // Only recreate instance buffer if it doesn't exist or size changed
    if (quadtreeData.instanceBuffer == VK_NULL_HANDLE || 
        instanceBufferSize > currentBufferSize) {
        
        // Store old buffer to destroy after creating new one
        VkBuffer oldBuffer = quadtreeData.instanceBuffer;
        VkDeviceMemory oldMemory = quadtreeData.instanceMemory;
        
        // Create new buffer with extra space to avoid frequent recreations
        // Use exponential growth strategy: allocate 2x required or at least 1000 instances
        VkDeviceSize minSize = sizeof(InstanceData) * 1000;  // Support at least 1000 patches
        VkDeviceSize allocSize = std::max(instanceBufferSize * 2, minSize);
        
        std::cout << "[LODManager] Reallocating instance buffer: " 
                  << "Required: " << patches.size() << " patches (" << instanceBufferSize << " bytes), "
                  << "Allocating: " << (allocSize / sizeof(InstanceData)) << " patches (" << allocSize << " bytes)" 
                  << std::endl;
        
        // Create new buffer before destroying old one
        quadtreeData.instanceBuffer = VK_NULL_HANDLE;
        quadtreeData.instanceMemory = VK_NULL_HANDLE;
        
        createBuffer(allocSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    quadtreeData.instanceBuffer, quadtreeData.instanceMemory);
        currentBufferSize = allocSize;
        
        // Notify renderer that instance buffer has changed
        // This is critical to update descriptor sets before next draw
        bufferUpdateRequired = true;
        
        // Clean up old buffer after new one is created
        if (oldBuffer != VK_NULL_HANDLE) {
            // Wait for GPU to finish using old buffer
            vkDeviceWaitIdle(device);
            destroyBuffer(oldBuffer, oldMemory);
        }
    }
    
    // Map memory and update instance data
    void* data;
    if (vkMapMemory(device, quadtreeData.instanceMemory, 0, instanceBufferSize, 0, &data) != VK_SUCCESS) {
        std::cerr << "[ERROR] Failed to map instance buffer memory!" << std::endl;
        return;
    }
    
    InstanceData* instanceArray = static_cast<InstanceData*>(data);
    
    // Enhanced debug logging
    static int debugCounter = 0;
    debugCounter++;
    
    // Track valid/invalid patches for summary
    int validPatches = 0;
    int degeneratePatches = 0;
    int invertedPatches = 0;
    int invalidDataPatches = 0;
    
    // Log on high patch counts or periodically
    if (patches.size() > 1000 || debugCounter % 100 == 0) {
        std::cout << "[UpdateQuadtreeBuffers] Frame " << debugCounter 
                  << " - Updating " << patches.size() << " patches" << std::endl;
        
        if (patches.size() > 5000) {
            std::cerr << "[WARNING] Excessive patch count in buffer update: " 
                      << patches.size() << std::endl;
        }
    }
    
    // Detailed logging for debugging - log at specific patch counts that are problematic
    static bool logged2000 = false;
    if (patches.size() == 6 || (patches.size() > 2000 && !logged2000)) {
        if (patches.size() > 2000) logged2000 = true;
        std::cout << "[UpdateQuadtreeBuffers] DETAILED - " << patches.size() << " patches:" << std::endl;
        for (size_t j = 0; j < std::min(size_t(3), patches.size()); j++) {
            std::cout << "  Patch " << j << ":" << std::endl;
            std::cout << "    Center: " << patches[j].center.x << "," << patches[j].center.y << "," << patches[j].center.z << std::endl;
            std::cout << "    Corners: [0](" << patches[j].corners[0].x << "," << patches[j].corners[0].y << "," << patches[j].corners[0].z << ")"
                      << " [1](" << patches[j].corners[1].x << "," << patches[j].corners[1].y << "," << patches[j].corners[1].z << ")"
                      << " [2](" << patches[j].corners[2].x << "," << patches[j].corners[2].y << "," << patches[j].corners[2].z << ")"
                      << " [3](" << patches[j].corners[3].x << "," << patches[j].corners[3].y << "," << patches[j].corners[3].z << ")" << std::endl;
            std::cout << "    Level: " << patches[j].level << ", Size: " << patches[j].size << std::endl;
        }
    }
    
    for (size_t i = 0; i < patches.size(); i++) {
        const auto& patch = patches[i];
        
        // Validate patch data thoroughly
        bool isValid = true;
        
        // Check corners for NaN/Inf
        for (int c = 0; c < 4; c++) {
            if (!std::isfinite(patch.corners[c].x) || !std::isfinite(patch.corners[c].y) || !std::isfinite(patch.corners[c].z)) {
                std::cerr << "[ERROR] Patch " << i << " corner " << c << " is NaN/Inf!" << std::endl;
                isValid = false;
            }
            // Check for extreme values that might cause issues
            // Check for extreme values - cube space should be normalized (-1 to 1)
            float magnitude = glm::length(patch.corners[c]);
            if (magnitude > 2.0f) {  // sqrt(3) ≈ 1.73 is max for unit cube corner
                std::cerr << "[ERROR] Patch " << i << " corner " << c << " has extreme magnitude: " << magnitude 
                          << " (expected <= 1.73 for unit cube)" << std::endl;
                std::cerr << "  Corner value: (" << patch.corners[c].x << ", " << patch.corners[c].y << ", " << patch.corners[c].z << ")" << std::endl;
                isValid = false;
            }
        }
        
        // Check center
        if (!std::isfinite(patch.center.x) || !std::isfinite(patch.center.y) || !std::isfinite(patch.center.z)) {
            std::cerr << "[ERROR] Patch " << i << " center is NaN/Inf!" << std::endl;
            isValid = false;
        }
        
        // Check level
        if (patch.level > 20 || patch.level < 0) {
            std::cerr << "[ERROR] Patch " << i << " level invalid: " << patch.level << std::endl;
            isValid = false;
        }
        
        // Check neighbor levels
        for (int n = 0; n < 4; n++) {
            if (patch.neighborLevels[n] > 20 || patch.neighborLevels[n] < 0) {
                std::cerr << "[ERROR] Patch " << i << " neighbor " << n << " level invalid: " << patch.neighborLevels[n] << std::endl;
                isValid = false;
            }
        }
        
        if (!isValid) {
            // Skip this patch by setting zero transform
            instanceArray[i].patchTransform = glm::dmat4(0.0);
            instanceArray[i].morphParams = glm::vec4(0.0f);
            instanceArray[i].heightTexCoord = glm::vec4(0.0f);
            instanceArray[i].patchInfo = glm::vec4(0.0f);
            invalidDataPatches++;
            continue;
        }
        
        // Create a GlobalPatch from the patch data to get the correct transform
        core::GlobalPatchGenerator::GlobalPatch globalPatch;
        
        // FIXED: For face-aligned patches, we need to preserve the semantic ordering of corners
        // The corners are ordered as: bottom-left, bottom-right, top-right, top-left
        // We need to identify which dimension is fixed and set bounds accordingly
        glm::vec3 minBounds, maxBounds;
        
        // Check which dimension has no variation (that's the fixed face dimension)
        float xRange = std::abs(patch.corners[2].x - patch.corners[0].x);
        float yRange = std::abs(patch.corners[2].y - patch.corners[0].y);
        float zRange = std::abs(patch.corners[2].z - patch.corners[0].z);
        const float eps = 0.001f;
        
        if (xRange < eps) {
            // X is fixed - patch is on +X or -X face
            // Corners are ordered: (x, minY, minZ), (x, minY, maxZ), (x, maxY, maxZ), (x, maxY, minZ)
            double x = patch.corners[0].x;
            minBounds = glm::dvec3(x, 
                                  std::min(patch.corners[0].y, patch.corners[2].y),
                                  std::min(patch.corners[0].z, patch.corners[2].z));
            maxBounds = glm::dvec3(x,
                                  std::max(patch.corners[0].y, patch.corners[2].y),
                                  std::max(patch.corners[0].z, patch.corners[2].z));
        }
        else if (yRange < eps) {
            // Y is fixed - patch is on +Y or -Y face
            // Corners are ordered: (minX, y, minZ), (maxX, y, minZ), (maxX, y, maxZ), (minX, y, maxZ)
            double y = patch.corners[0].y;
            minBounds = glm::dvec3(patch.corners[0].x, y, patch.corners[0].z);
            maxBounds = glm::dvec3(patch.corners[2].x, y, patch.corners[2].z);
        }
        else if (zRange < eps) {
            // Z is fixed - patch is on +Z or -Z face
            // Corners are ordered: (minX, minY, z), (maxX, minY, z), (maxX, maxY, z), (minX, maxY, z)
            double z = patch.corners[0].z;
            minBounds = glm::dvec3(patch.corners[0].x, patch.corners[0].y, z);
            maxBounds = glm::dvec3(patch.corners[2].x, patch.corners[2].y, z);
        }
        else {
            // Not a face patch - fall back to calculating from all corners
            minBounds = glm::dvec3(1e9);
            maxBounds = glm::dvec3(-1e9);
            for (int j = 0; j < 4; j++) {
                minBounds.x = glm::min<double>(minBounds.x, patch.corners[j].x);
                minBounds.y = glm::min<double>(minBounds.y, patch.corners[j].y);
                minBounds.z = glm::min<double>(minBounds.z, patch.corners[j].z);
                maxBounds.x = glm::max<double>(maxBounds.x, patch.corners[j].x);
                maxBounds.y = glm::max<double>(maxBounds.y, patch.corners[j].y);
                maxBounds.z = glm::max<double>(maxBounds.z, patch.corners[j].z);
            }
        }
        
        globalPatch.minBounds = minBounds;
        globalPatch.maxBounds = maxBounds;
        globalPatch.center = patch.center;
        globalPatch.level = patch.level;
        globalPatch.faceId = patch.faceId;
        
        // Get the globally consistent transform
        glm::dmat4 transform = globalPatch.createTransform();
        
        // Validate transform matrix before setting
        bool transformValid = true;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                if (!std::isfinite(transform[col][row])) {
                    std::cerr << "[ERROR] Patch " << i << " transform[" << col << "][" << row << "] is NaN/Inf!" << std::endl;
                    transformValid = false;
                }
            }
        }
        
        // Check determinant to detect degenerate transforms
        // Note: Negative determinants are OK - they just mean left-handed coordinate system
        double det = glm::determinant(transform);
        if (std::abs(det) < 1e-10) {
            std::cerr << "[ERROR] Patch " << i << " has DEGENERATE transform (det=" << det << ")!" << std::endl;
            std::cerr << "  Face " << patch.faceId << ", Level " << patch.level << std::endl;
            std::cerr << "  Corners: [0](" << patch.corners[0].x << "," << patch.corners[0].y << "," << patch.corners[0].z << ")" << std::endl;
            std::cerr << "          [1](" << patch.corners[1].x << "," << patch.corners[1].y << "," << patch.corners[1].z << ")" << std::endl;
            std::cerr << "          [2](" << patch.corners[2].x << "," << patch.corners[2].y << "," << patch.corners[2].z << ")" << std::endl;
            std::cerr << "          [3](" << patch.corners[3].x << "," << patch.corners[3].y << "," << patch.corners[3].z << ")" << std::endl;
            transformValid = false;
            degeneratePatches++;
        }
        // Removed check for negative determinant - it's not an error!
        
        if (!transformValid) {
            instanceArray[i].patchTransform = glm::dmat4(1.0); // Identity matrix as fallback
            std::cerr << "[WARNING] Using identity transform for patch " << i << std::endl;
        } else {
            instanceArray[i].patchTransform = transform;
            validPatches++;
        }
        
        // Set morph parameters with neighbor LOD levels for T-junction fixing
        // Top, Right, Bottom, Left edges
        instanceArray[i].morphParams = glm::vec4(patch.morphFactor, 
                                                 static_cast<float>(patch.neighborLevels[0]),
                                                 static_cast<float>(patch.neighborLevels[1]), 
                                                 static_cast<float>(patch.neighborLevels[2]));
        
        // Use heightTexCoord to pass the 4th neighbor level and patch level
        instanceArray[i].heightTexCoord = glm::vec4(static_cast<float>(patch.neighborLevels[3]), 
                                                    static_cast<float>(patch.level), 
                                                    0.0f, 0.0f);
        
        // Set patch info - include face ID
        instanceArray[i].patchInfo = glm::vec4(patch.level, patch.size, static_cast<float>(patch.faceId), 0.0f);
    }
    
    // Log summary if we found any problems
    if (degeneratePatches > 0 || invertedPatches > 0 || invalidDataPatches > 0) {
        std::cerr << "[PATCH VALIDATION SUMMARY] Total: " << patches.size() 
                  << ", Valid: " << validPatches 
                  << ", Invalid data: " << invalidDataPatches
                  << ", Degenerate: " << degeneratePatches 
                  << ", Inverted: " << invertedPatches << std::endl;
    } else if (debugCounter % 100 == 0 || patches.size() > 1000) {
        std::cout << "[PATCH VALIDATION] All " << validPatches << " patches valid!" << std::endl;
    }
    
    // Debug: Count patches per face and level
    if (debugCounter % 20 == 0 || debugCounter == 1) {
        int faceCounts[6] = {0};
        int levelCounts[10] = {0};
        
        // Also check for suspicious patches (potential holes)
        int suspiciousPatches = 0;
        
        for (const auto& patch : patches) {
            if (patch.faceId < 6) faceCounts[patch.faceId]++;
            if (patch.level < 10) levelCounts[patch.level]++;
            
            // Check for patches at face boundaries
            bool atBoundary = false;
            for (int c = 0; c < 4; c++) {
                float x = std::abs(patch.corners[c].x);
                float y = std::abs(patch.corners[c].y);
                float z = std::abs(patch.corners[c].z);
                
                // Check if corner is at cube face edge (value close to 1)
                if ((x > 0.98f && x < 1.02f) || 
                    (y > 0.98f && y < 1.02f) || 
                    (z > 0.98f && z < 1.02f)) {
                    atBoundary = true;
                }
            }
            
            if (atBoundary) {
                suspiciousPatches++;
                if (debugCounter == 1 && suspiciousPatches <= 5) {
                    std::cout << "[BOUNDARY PATCH] Face " << patch.faceId 
                              << " Level " << patch.level
                              << " Center: " << patch.center.x << "," 
                              << patch.center.y << "," << patch.center.z << std::endl;
                }
            }
        }
        
        std::cout << "[PATCH DISTRIBUTION] Faces: ";
        for (int i = 0; i < 6; i++) std::cout << faceCounts[i] << " ";
        std::cout << "| Levels: ";
        for (int i = 0; i < 5; i++) if (levelCounts[i] > 0) 
            std::cout << "L" << i << ":" << levelCounts[i] << " ";
        std::cout << " | Boundary patches: " << suspiciousPatches;
        std::cout << std::endl;
    }
    
    vkUnmapMemory(device, quadtreeData.instanceMemory);
}

void LODManager::updateOctreeChunks(const glm::vec3& viewPos) {
    // Update octree LOD
    octreePlanet->updateLOD(viewPos);
    
    // Generate/update Transvoxel chunks
    // This is simplified - real implementation would be more complex
    if (octreeChunks.empty()) {
        // Create initial chunks around viewer
        const float chunkSize = 100.0f;
        const int chunksPerSide = 5;
        
        for (int x = -chunksPerSide; x <= chunksPerSide; x++) {
            for (int y = -chunksPerSide; y <= chunksPerSide; y++) {
                for (int z = -chunksPerSide; z <= chunksPerSide; z++) {
                    TransvoxelChunk chunk;
                    chunk.position = viewPos + glm::vec3(x, y, z) * chunkSize;
                    chunk.voxelSize = 1.0f;
                    chunk.lodLevel = 0;
                    
                    transvoxelRenderer->generateMesh(chunk, *octreePlanet);
                    if (chunk.hasValidMesh) {
                        octreeChunks.push_back(chunk);
                    }
                }
            }
        }
    }
    
    stats.octreeChunks = static_cast<uint32_t>(octreeChunks.size());
}

void LODManager::prepareTransitionZone(const glm::vec3& viewPos) {
    // In transition zone, prepare octree chunks while still showing quadtree
    // Start loading octree data in background
    updateOctreeChunks(viewPos);
}

void LODManager::updateQuadtreeBuffersCPU(const std::vector<core::QuadtreePatch>& patches, const glm::vec3& viewPosition) {
    if (patches.empty()) {
        quadtreeData.instanceCount = 0;
        return;
    }
    
    std::cout << "[LODManager] Generating CPU vertices for " << patches.size() << " patches" << std::endl;
    
    // Calculate total vertex and index requirements
    size_t totalVertices = 0;
    size_t totalIndices = 0;
    std::vector<CPUVertexGenerator::PatchMesh> meshes;
    meshes.reserve(patches.size());
    
    // Generate meshes for all patches
    for (const auto& patch : patches) {
        // Create transform matrix for patch
        core::GlobalPatchGenerator::GlobalPatch globalPatch;
        
        // CRITICAL: Use the EXACT double-precision bounds stored in the patch
        // These were computed once with full precision and must be preserved!
        globalPatch.minBounds = patch.minBounds;
        globalPatch.maxBounds = patch.maxBounds;
        globalPatch.center = patch.center;
        globalPatch.level = patch.level;
        globalPatch.faceId = patch.faceId;
        
        glm::dmat4 transform = globalPatch.createTransform();
        
        // Generate mesh using CPU vertex generator
        auto mesh = vertexGenerator->generatePatchMesh(patch, transform);
        totalVertices += mesh.vertexCount;
        totalIndices += mesh.indexCount;
        meshes.push_back(std::move(mesh));
    }
    
    std::cout << "[LODManager] Generated " << totalVertices << " vertices and " 
              << totalIndices << " indices" << std::endl;
    
    // Debug: Print first vertex to verify data
    if (!meshes.empty() && !meshes[0].vertices.empty()) {
        const auto& v = meshes[0].vertices[0];
        std::cout << "[DEBUG] First vertex: pos(" << v.position.x << "," << v.position.y << "," << v.position.z 
                  << ") normal(" << v.normal.x << "," << v.normal.y << "," << v.normal.z 
                  << ") height=" << v.height << std::endl;
        
        // Also print camera distance for debugging
        glm::vec3 cameraPos(1.115e+07, 4.770e+06, 9.556e+06);  // From debug output
        float dist = glm::length(v.position - cameraPos);
        std::cout << "[DEBUG] Distance from camera to first vertex: " << dist << " meters" << std::endl;
    }
    
    // Allocate or reallocate vertex buffer if needed
    VkDeviceSize vertexBufferSize = totalVertices * sizeof(PatchVertex);
    VkDeviceSize indexBufferSize = totalIndices * sizeof(uint32_t);
    
    static VkDeviceSize currentVertexBufferSize = 0;
    static VkDeviceSize currentIndexBufferSize = 0;
    
    // Reallocate vertex buffer if needed
    if (quadtreeData.vertexBuffer == VK_NULL_HANDLE || 
        vertexBufferSize > currentVertexBufferSize) {
        
        VkBuffer oldBuffer = quadtreeData.vertexBuffer;
        VkDeviceMemory oldMemory = quadtreeData.vertexMemory;
        
        // Allocate with extra space
        VkDeviceSize allocSize = std::max(vertexBufferSize * 2, 
                                         VkDeviceSize(sizeof(PatchVertex) * 100000));
        
        createBuffer(allocSize,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    quadtreeData.vertexBuffer, quadtreeData.vertexMemory);
        
        currentVertexBufferSize = allocSize;
        bufferUpdateRequired = true;
        
        if (oldBuffer != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            destroyBuffer(oldBuffer, oldMemory);
        }
    }
    
    // Reallocate index buffer if needed
    if (quadtreeData.indexBuffer == VK_NULL_HANDLE || 
        indexBufferSize > currentIndexBufferSize) {
        
        VkBuffer oldBuffer = quadtreeData.indexBuffer;
        VkDeviceMemory oldMemory = quadtreeData.indexMemory;
        
        VkDeviceSize allocSize = std::max(indexBufferSize * 2, 
                                         VkDeviceSize(sizeof(uint32_t) * 500000));
        
        createBuffer(allocSize,
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    quadtreeData.indexBuffer, quadtreeData.indexMemory);
        
        currentIndexBufferSize = allocSize;
        
        if (oldBuffer != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            destroyBuffer(oldBuffer, oldMemory);
        }
    }
    
    // Upload vertex data with camera-relative transform
    // This is THE critical step for numerical precision at planet scale
    void* vertexData;
    if (vkMapMemory(device, quadtreeData.vertexMemory, 0, vertexBufferSize, 0, &vertexData) == VK_SUCCESS) {
        // Use the camera position passed from update() for relative transform
        glm::vec3 cameraPos = viewPosition;
        
        // Debug: Print camera position once
        static int uploadCount = 0;
        if (uploadCount++ == 0) {
            std::cout << "[Camera-Relative] Transforming vertices relative to camera at: (" 
                      << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z 
                      << "), distance: " << glm::length(cameraPos) << " meters" << std::endl;
        }
        
        size_t vertexOffset = 0;
        for (size_t m = 0; m < meshes.size(); m++) {
            const auto& mesh = meshes[m];
            // Apply camera-relative transform to each vertex
            PatchVertex* destVertices = reinterpret_cast<PatchVertex*>(
                static_cast<char*>(vertexData) + vertexOffset);
            
            for (size_t v = 0; v < mesh.vertexCount; v++) {
                PatchVertex vertex = mesh.vertices[v];
                
                // Check for escaping vertices BEFORE transform
                float worldDist = glm::length(vertex.position);
                if (worldDist > 6.371e6 * 1.5f) { // More than 1.5x planet radius
                    static int escapeCount = 0;
                    if (escapeCount++ < 10) {
                        std::cout << "  ESCAPING VERTEX DETECTED: worldDist=" << worldDist 
                                  << " at (" << vertex.position.x << ", " << vertex.position.y 
                                  << ", " << vertex.position.z << ")" << std::endl;
                    }
                }
                
                // Apply camera-relative transform to avoid precision issues
                vertex.position = vertex.position - cameraPos;
                destVertices[v] = vertex;
                
                // Debug: Check for any extreme vertices after transform
                if (uploadCount == 1 && m == 0 && v == 0) {
                    float relDist = glm::length(vertex.position);
                    std::cout << "  First vertex after transform: distance from camera = " 
                              << relDist << " meters (should be ~planet radius)" << std::endl;
                }
            }
            
            vertexOffset += mesh.vertexCount * sizeof(PatchVertex);
        }
        vkUnmapMemory(device, quadtreeData.vertexMemory);
    }
    
    // Upload index data  
    void* indexData;
    if (vkMapMemory(device, quadtreeData.indexMemory, 0, indexBufferSize, 0, &indexData) == VK_SUCCESS) {
        size_t indexOffset = 0;
        size_t vertexBase = 0;
        
        for (const auto& mesh : meshes) {
            // Adjust indices to account for vertex offset
            for (uint32_t idx : mesh.indices) {
                uint32_t adjustedIdx = idx + static_cast<uint32_t>(vertexBase);
                std::memcpy(static_cast<char*>(indexData) + indexOffset, 
                           &adjustedIdx, sizeof(uint32_t));
                indexOffset += sizeof(uint32_t);
            }
            vertexBase += mesh.vertexCount;
        }
        vkUnmapMemory(device, quadtreeData.indexMemory);
    }
    
    // Update instance buffer with simplified data for CPU vertices
    struct InstanceData {
        glm::mat4 mvpMatrix;
        glm::vec4 morphParams;
        glm::vec4 patchInfo;
    };
    
    VkDeviceSize instanceBufferSize = sizeof(InstanceData) * patches.size();
    static VkDeviceSize currentInstanceBufferSize = 0;
    
    if (quadtreeData.instanceBuffer == VK_NULL_HANDLE || 
        instanceBufferSize > currentInstanceBufferSize) {
        
        VkBuffer oldBuffer = quadtreeData.instanceBuffer;
        VkDeviceMemory oldMemory = quadtreeData.instanceMemory;
        
        VkDeviceSize allocSize = std::max(instanceBufferSize * 2, 
                                         VkDeviceSize(sizeof(InstanceData) * 1000));
        
        createBuffer(allocSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    quadtreeData.instanceBuffer, quadtreeData.instanceMemory);
        
        currentInstanceBufferSize = allocSize;
        bufferUpdateRequired = true;
        
        if (oldBuffer != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            destroyBuffer(oldBuffer, oldMemory);
        }
    }
    
    // Fill instance data
    void* instanceData;
    if (vkMapMemory(device, quadtreeData.instanceMemory, 0, instanceBufferSize, 0, &instanceData) == VK_SUCCESS) {
        InstanceData* instances = static_cast<InstanceData*>(instanceData);
        
        for (size_t i = 0; i < patches.size(); i++) {
            const auto& patch = patches[i];
            
            // For now, just use identity MVP (will be multiplied by view-proj in shader)
            instances[i].mvpMatrix = glm::mat4(1.0f);
            instances[i].morphParams = glm::vec4(patch.morphFactor, 
                                                patch.neighborLevels[0],
                                                patch.neighborLevels[1], 
                                                patch.neighborLevels[2]);
            instances[i].patchInfo = glm::vec4(patch.level, patch.size, patch.faceId, 0.0f);
        }
        
        vkUnmapMemory(device, quadtreeData.instanceMemory);
    }
    
    quadtreeData.indexCount = static_cast<uint32_t>(totalIndices);
    quadtreeData.instanceCount = 1;  // Not using instancing - all vertices in one draw call
    
    // Print stats
    auto stats = vertexGenerator->getStats();
    std::cout << "[LODManager] Vertex cache stats - Hits: " << stats.cacheHits 
              << ", Misses: " << stats.cacheMisses 
              << ", Cache size: " << stats.currentCacheSize << std::endl;
}

void LODManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VkBuffer& buffer,
                             VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }
    
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void LODManager::destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

uint32_t LODManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type");
}

void LODManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void LODManager::uploadBufferData(VkBuffer buffer, const void* data, VkDeviceSize size) {
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    // Copy data to staging buffer
    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Copy from staging to device buffer
    copyBuffer(stagingBuffer, buffer, size);
    
    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

} // namespace rendering