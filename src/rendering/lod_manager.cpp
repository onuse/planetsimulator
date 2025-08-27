#include "rendering/lod_manager.hpp"
#include "core/global_patch_generator.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <fstream>

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
        
        // Clean up GPU mesh generation pipeline
        destroyGPUMeshGenerationPipeline();
        
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
    // PERFORMANCE FIX: DRASTICALLY reduced to prevent patch explosion
    // Balanced LOD settings now that scaling is fixed
    quadConfig.maxLevel = 8;  // Good detail without overwhelming
    // Moderate pixel error for balanced subdivision
    quadConfig.pixelError = 4.0f;  // Reasonable detail
    quadConfig.morphRegion = 0.3f;
    quadConfig.enableMorphing = true;
    quadConfig.enableCrackFixes = true;
    quadConfig.enableFaceCulling = false;  // TEMP: Disable to debug black wedge
    quadConfig.enableFrustumCulling = false;  // DISABLED - test without frustum culling
    quadConfig.enableDistanceCulling = false;  // DISABLED - test without distance culling
    
    quadtree = std::make_unique<core::SphericalQuadtree>(quadConfig, densityField);
    
    // Initialize octree for volumetric rendering
    octreePlanet = std::make_unique<octree::OctreePlanet>(planetRadius, 10);
    octreePlanet->generate(seed);
    
    // Note: GPU octree will be set by VulkanRenderer after initialization
    std::cout << "[LODManager] Octree planet generated (GPU upload will be done by VulkanRenderer)" << std::endl;
    
    // Create GPU mesh generation pipeline
    createGPUMeshGenerationPipeline();
    std::cout << "[LODManager] GPU mesh generation pipeline created" << std::endl;
    
    // Initialize Transvoxel renderer
    transvoxelRenderer = std::make_unique<TransvoxelRenderer>(
        device, physicalDevice, commandPool, graphicsQueue
    );
    
    
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
    // Store camera position for GPU mesh generation
    lastCameraPos = cameraPos;
    
    // Calculate altitude
    currentAltitude = glm::length(cameraPos) - densityField->getPlanetRadius();
    stats.altitude = currentAltitude;
    
    // Select rendering mode based on altitude
    currentMode = selectRenderingMode(currentAltitude);
    stats.mode = currentMode;
    
    // PERFORMANCE: Disabled periodic debug output
    // static int updateCount = 0;
    // if (updateCount++ % 60 == 0) {
    //     std::cout << "[LODManager] Altitude: " << currentAltitude << "m, Mode: " 
    //               << (currentMode == QUADTREE_ONLY ? "QUADTREE" : 
    //                   currentMode == TRANSITION_ZONE ? "TRANSITION" : "OCTREE")
    //               << std::endl;
    // }
    
    // Update quadtree LOD selection
    quadtree->update(cameraPos, viewProj, deltaTime);
    const auto& quadPatches = quadtree->getVisiblePatches();
    stats.quadtreePatches = static_cast<uint32_t>(quadPatches.size());
    
    // DEBUG: Enable patch counting to understand what we're getting
    static int updateCount = 0;
    updateCount++;
    if (updateCount % 60 == 0) { // Every second at 60fps
        int faceDebugCounts[6] = {0};
        for (const auto& patch : quadPatches) {
            if (patch.faceId < 6) faceDebugCounts[patch.faceId]++;
        }
        std::cout << "[LODManager] Got " << quadPatches.size() << " patches. Per face: "
                  << faceDebugCounts[0] << " " << faceDebugCounts[1] << " " 
                  << faceDebugCounts[2] << " " << faceDebugCounts[3] << " "
                  << faceDebugCounts[4] << " " << faceDebugCounts[5] << std::endl;
        
        // Show total vertices that will be generated
        size_t totalVertices = quadPatches.size() * 33 * 33; // 33x33 grid per patch
        std::cout << "[LODManager] Will generate approximately " << totalVertices 
                  << " vertices (" << (totalVertices/1000) << "k)" << std::endl;
    }
    
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
            // GPU mesh generation is required - no fallback
            break;
            
        case TRANSITION_ZONE:
            // GPU mesh generation is required - no fallback
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
    
    // Use GPU mesh generation when available
    if (gpuMeshGen.computePipeline != VK_NULL_HANDLE) {
        const auto& patches = quadtree->getVisiblePatches();
        if (!patches.empty()) {
            static int frameCount = 0;
            if (frameCount++ % 60 == 0) {
                std::cout << "[GPU MESH] Pipeline active! Generating mesh for " << patches.size() << " patches" << std::endl;
                std::cout << "[GPU MESH] GPU vertex buffer: " << gpuMeshGen.gpuVertexBuffer 
                          << ", capacity: " << gpuMeshGen.gpuVertexCapacity << " vertices" << std::endl;
            }
            
            // Generate mesh for all visible patches on GPU
            generateFullPlanetOnGPU(commandBuffer);
            
            // The GPU generates vertices and indices directly
            quadtreeData.instanceCount = 1;
            
            if (frameCount % 60 == 0) {
                std::cout << "[GPU MESH] Generated mesh for " << patches.size() 
                          << " patches with " << quadtreeData.indexCount << " indices" << std::endl;
            }
        }
    } else {
        static bool warned = false;
        if (!warned) {
            std::cout << "[GPU MESH] WARNING: Compute pipeline is NULL!" << std::endl;
            warned = true;
        }
    }
    
    switch (currentMode) {
        case QUADTREE_ONLY:
            // Render quadtree patches only
            if (quadtreeData.instanceCount > 0) {
                // NOTE: Pipeline binding should be done by the caller (VulkanRenderer)
                // The caller should bind the appropriate pipeline before calling this render function
                
                // Bind vertex and index buffers
                // Try GPU buffers again with vertices at known visible position
                bool useGPUBuffers = (gpuMeshGen.gpuVertexBuffer != VK_NULL_HANDLE && 
                                     gpuMeshGen.gpuIndexBuffer != VK_NULL_HANDLE);
                
                VkBuffer vertexBuffer = useGPUBuffers ? gpuMeshGen.gpuVertexBuffer : quadtreeData.vertexBuffer;
                VkBuffer indexBuffer = useGPUBuffers ? gpuMeshGen.gpuIndexBuffer : quadtreeData.indexBuffer;
                
                static int bindCount = 0;
                if (bindCount++ % 60 == 0) {
                    std::cout << "[RENDER] Using " << (useGPUBuffers ? "GPU" : "CPU") << " buffers for rendering" << std::endl;
                }
                
                VkBuffer vertexBuffers[] = {vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                
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
                // PERFORMANCE: Disabled verbose drawing debug
                // std::cout << "[DEBUG] Drawing with indexCount=" << quadtreeData.indexCount 
                //           << " instanceCount=" << quadtreeData.instanceCount << std::endl;
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
            
            // GPU-ONLY: Removed CPU octree chunk rendering
            // if (!octreeChunks.empty()) {
            //     transvoxelRenderer->render(octreeChunks, commandBuffer, pipelineLayout);
            // }
            break;
            
        case OCTREE_TRANSVOXEL:
            // GPU-ONLY: Removed CPU transvoxel rendering
            // if (!octreeChunks.empty()) {
            //     transvoxelRenderer->render(octreeChunks, commandBuffer, pipelineLayout);
            // }
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

void LODManager::createGPUMeshGenerationPipeline() {
    // Load compute shader
    std::ifstream shaderFile("shaders/compiled/mesh_generator.comp.spv", std::ios::binary);
    if (!shaderFile) {
        std::cout << "[LODManager] Warning: Failed to open compute shader file - GPU mesh generation disabled" << std::endl;
        return;  // Don't throw, just disable GPU mesh generation for now
    }
    
    std::vector<char> shaderCode((std::istreambuf_iterator<char>(shaderFile)),
                                  std::istreambuf_iterator<char>());
    
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = shaderCode.size();
    shaderInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    
    if (vkCreateShaderModule(device, &shaderInfo, nullptr, &gpuMeshGen.computeShader) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute shader module");
    }
    
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[5] = {};
    
    // Binding 0: Octree nodes (storage buffer)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: Voxel data (storage buffer)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 2: Output vertex buffer (storage buffer)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 3: Output index buffer (storage buffer)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 4: Output index count (storage buffer)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &gpuMeshGen.descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    
    // Create pipeline layout with push constants for patch parameters
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4) + sizeof(glm::vec4) * 2;  // transform + 2 vec4s
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &gpuMeshGen.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &gpuMeshGen.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
    
    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = gpuMeshGen.computeShader;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = gpuMeshGen.pipelineLayout;
    
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gpuMeshGen.computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }
    
    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 5;  // Now we have 5 bindings
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &gpuMeshGen.descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = gpuMeshGen.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &gpuMeshGen.descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(device, &allocInfo, &gpuMeshGen.descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }
    
    // Create GPU vertex buffer for output (49x49 grid = 2401 vertices per patch)
    gpuMeshGen.gpuVertexCapacity = 49 * 49 * 100;  // Space for 100 patches
    // PatchVertex size: vec3 pos + vec3 normal + vec2 uv + float height + uint faceId = 36 bytes
    VkDeviceSize vertexBufferSize = gpuMeshGen.gpuVertexCapacity * 36;
    
    createBuffer(vertexBufferSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 gpuMeshGen.gpuVertexBuffer, gpuMeshGen.gpuVertexMemory);
    
    // Create GPU index buffer for output (48x48x2 triangles x 3 indices per triangle)
    gpuMeshGen.gpuIndexCapacity = 48 * 48 * 2 * 3 * 100;  // Space for 100 patches
    VkDeviceSize indexBufferSize = gpuMeshGen.gpuIndexCapacity * sizeof(uint32_t);
    
    createBuffer(indexBufferSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 gpuMeshGen.gpuIndexBuffer, gpuMeshGen.gpuIndexMemory);
    
    // Create index count buffer (just one uint32_t)
    VkDeviceSize countBufferSize = sizeof(uint32_t);
    
    createBuffer(countBufferSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 gpuMeshGen.gpuIndexCountBuffer, gpuMeshGen.gpuIndexCountMemory);
    
    // Update descriptor set with buffers (only if gpuOctree is available)
    if (!gpuOctree) {
        std::cout << "[LODManager] GPU octree not yet set, skipping descriptor update" << std::endl;
        return;
    }
    
    VkDescriptorBufferInfo bufferInfos[5] = {};
    
    // Octree nodes buffer
    bufferInfos[0].buffer = gpuOctree->getNodeBuffer();
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = VK_WHOLE_SIZE;
    
    // Voxel data buffer
    bufferInfos[1].buffer = gpuOctree->getVoxelBuffer();
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = VK_WHOLE_SIZE;
    
    // Output vertex buffer
    bufferInfos[2].buffer = gpuMeshGen.gpuVertexBuffer;
    bufferInfos[2].offset = 0;
    bufferInfos[2].range = VK_WHOLE_SIZE;
    
    // Output index buffer
    bufferInfos[3].buffer = gpuMeshGen.gpuIndexBuffer;
    bufferInfos[3].offset = 0;
    bufferInfos[3].range = VK_WHOLE_SIZE;
    
    // Output index count buffer
    bufferInfos[4].buffer = gpuMeshGen.gpuIndexCountBuffer;
    bufferInfos[4].offset = 0;
    bufferInfos[4].range = sizeof(uint32_t);
    
    VkWriteDescriptorSet descriptorWrites[5] = {};
    for (int i = 0; i < 5; i++) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = gpuMeshGen.descriptorSet;
        descriptorWrites[i].dstBinding = i;
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pBufferInfo = &bufferInfos[i];
    }
    
    vkUpdateDescriptorSets(device, 5, descriptorWrites, 0, nullptr);
}

void LODManager::updateGPUMeshGenerationDescriptors() {
    if (!gpuOctree) {
        std::cerr << "[LODManager] Cannot update descriptors: GPU octree not set!" << std::endl;
        return;
    }
    
    if (gpuMeshGen.descriptorSet == VK_NULL_HANDLE) {
        std::cerr << "[LODManager] Cannot update descriptors: descriptor set not created!" << std::endl;
        return;
    }
    
    // Update descriptor set with buffers
    VkDescriptorBufferInfo bufferInfos[5] = {};
    
    // Octree nodes buffer
    bufferInfos[0].buffer = gpuOctree->getNodeBuffer();
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = VK_WHOLE_SIZE;
    
    // Voxel data buffer  
    bufferInfos[1].buffer = gpuOctree->getVoxelBuffer();
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = VK_WHOLE_SIZE;
    
    // Output vertex buffer
    bufferInfos[2].buffer = gpuMeshGen.gpuVertexBuffer;
    bufferInfos[2].offset = 0;
    bufferInfos[2].range = VK_WHOLE_SIZE;
    
    // Output index buffer
    bufferInfos[3].buffer = gpuMeshGen.gpuIndexBuffer;
    bufferInfos[3].offset = 0;
    bufferInfos[3].range = VK_WHOLE_SIZE;
    
    // Output index count buffer
    bufferInfos[4].buffer = gpuMeshGen.gpuIndexCountBuffer;
    bufferInfos[4].offset = 0;
    bufferInfos[4].range = sizeof(uint32_t);
    
    VkWriteDescriptorSet descriptorWrites[5] = {};
    for (int i = 0; i < 5; i++) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = gpuMeshGen.descriptorSet;
        descriptorWrites[i].dstBinding = i;
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pBufferInfo = &bufferInfos[i];
    }
    
    vkUpdateDescriptorSets(device, 5, descriptorWrites, 0, nullptr);
    std::cout << "[LODManager] GPU mesh generation descriptors updated with GPU octree buffers" << std::endl;
}

void LODManager::destroyGPUMeshGenerationPipeline() {
    if (device == VK_NULL_HANDLE) return;
    
    vkDeviceWaitIdle(device);
    
    if (gpuMeshGen.gpuVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, gpuMeshGen.gpuVertexBuffer, nullptr);
        vkFreeMemory(device, gpuMeshGen.gpuVertexMemory, nullptr);
    }
    
    if (gpuMeshGen.gpuIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, gpuMeshGen.gpuIndexBuffer, nullptr);
        vkFreeMemory(device, gpuMeshGen.gpuIndexMemory, nullptr);
    }
    
    if (gpuMeshGen.gpuIndexCountBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, gpuMeshGen.gpuIndexCountBuffer, nullptr);
        vkFreeMemory(device, gpuMeshGen.gpuIndexCountMemory, nullptr);
    }
    
    if (gpuMeshGen.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, gpuMeshGen.descriptorPool, nullptr);
    }
    
    if (gpuMeshGen.computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, gpuMeshGen.computePipeline, nullptr);
    }
    
    if (gpuMeshGen.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, gpuMeshGen.pipelineLayout, nullptr);
    }
    
    if (gpuMeshGen.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, gpuMeshGen.descriptorSetLayout, nullptr);
    }
    
    if (gpuMeshGen.computeShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, gpuMeshGen.computeShader, nullptr);
    }
}

void LODManager::generateFullPlanetOnGPU(VkCommandBuffer commandBuffer) {
    const auto& patches = quadtree->getVisiblePatches();
    if (patches.empty()) return;
    
    // Calculate the total index count (48*48*6 indices per patch for 49x49 grid)
    // Each patch generates (gridRes-1)*(gridRes-1)*6 indices
    const uint32_t indicesPerPatch = 48 * 48 * 6;  // 13824 indices per patch
    quadtreeData.indexCount = static_cast<uint32_t>(patches.size()) * indicesPerPatch;
    
    // Bind compute pipeline once
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gpuMeshGen.computePipeline);
    
    // Bind descriptor set once
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           gpuMeshGen.pipelineLayout, 0, 1, &gpuMeshGen.descriptorSet,
                           0, nullptr);
    
    struct PushConstants {
        glm::mat4 patchTransform;
        glm::vec4 patchInfo;  // level, size, gridResolution, bufferOffset
        glm::vec4 viewPos;    // Camera position in world space (xyz), planetRadius (w)
    } pushData;
    
    // Generate mesh for each visible patch
    uint32_t patchOffset = 0;
    for (size_t i = 0; i < patches.size(); i++) {
        const auto& patch = patches[i];
        
        pushData.patchTransform = glm::mat4(patch.patchTransform);
        // Store buffer offset in patchInfo.w
        pushData.patchInfo = glm::vec4(patch.level, patch.size, 49.0f, float(patchOffset));
        pushData.viewPos = glm::vec4(lastCameraPos, densityField->getPlanetRadius());
        
        vkCmdPushConstants(commandBuffer, gpuMeshGen.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(pushData), &pushData);
        
        // Dispatch compute shader for this patch (49x49 grid requires 7x7 workgroups with 8x8 threads each)
        vkCmdDispatch(commandBuffer, 7, 7, 1);
        
        // Update offset for next patch (49x49 vertices per patch)
        patchOffset += 49 * 49;
    }
    
    // Memory barrier after all patches
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void LODManager::generateMeshOnGPU(const core::QuadtreePatch& patch, VkCommandBuffer commandBuffer) {
    // Bind compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gpuMeshGen.computePipeline);
    
    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           gpuMeshGen.pipelineLayout, 0, 1, &gpuMeshGen.descriptorSet,
                           0, nullptr);
    
    // Set push constants with patch parameters
    struct PushConstants {
        glm::mat4 patchTransform;
        glm::vec4 patchInfo;  // level, size, gridResolution, planetRadius
        glm::vec4 viewPos;    // Camera position in world space
    } pushData;
    
    pushData.patchTransform = glm::mat4(patch.patchTransform);
    pushData.patchInfo = glm::vec4(patch.level, patch.size, 49.0f, densityField->getPlanetRadius());  // 49x49 grid
    pushData.viewPos = glm::vec4(lastCameraPos, 0.0f);  // Pass actual camera position
    
    vkCmdPushConstants(commandBuffer, gpuMeshGen.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(pushData), &pushData);
    
    // Dispatch compute shader (49x49 grid with 8x8 local workgroups)
    uint32_t gridSize = 49;
    uint32_t workgroupSize = 8;
    uint32_t dispatchX = (gridSize + workgroupSize - 1) / workgroupSize;
    uint32_t dispatchY = (gridSize + workgroupSize - 1) / workgroupSize;
    
    vkCmdDispatch(commandBuffer, dispatchX, dispatchY, 1);
    
    // Memory barrier to ensure compute shader writes are visible to vertex shader
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

} // namespace rendering