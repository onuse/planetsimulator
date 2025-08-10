#include "rendering/lod_manager.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

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
    
    quadtree = std::make_unique<core::SphericalQuadtree>(quadConfig, densityField);
    
    // Initialize octree for volumetric rendering
    octreePlanet = std::make_unique<octree::OctreePlanet>(planetRadius, 10);
    octreePlanet->generate(seed);
    
    // Initialize Transvoxel renderer
    transvoxelRenderer = std::make_unique<TransvoxelRenderer>(
        device, physicalDevice, commandPool, graphicsQueue
    );
    
    // Create initial buffers for quadtree rendering
    // Base mesh is a subdivided quad
    const uint32_t baseResolution = 64; // 64x64 grid per patch
    const uint32_t vertexCount = baseResolution * baseResolution;
    const uint32_t indexCount = (baseResolution - 1) * (baseResolution - 1) * 6;
    
    // Generate base mesh vertices (unit quad)
    std::vector<glm::vec2> baseVertices;
    baseVertices.reserve(vertexCount);
    
    for (uint32_t y = 0; y < baseResolution; y++) {
        for (uint32_t x = 0; x < baseResolution; x++) {
            float u = static_cast<float>(x) / (baseResolution - 1);
            float v = static_cast<float>(y) / (baseResolution - 1);
            baseVertices.push_back(glm::vec2(u, v));
        }
    }
    
    // Generate indices
    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    
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
            updateQuadtreeBuffers(quadPatches);
            break;
            
        case TRANSITION_ZONE:
            updateQuadtreeBuffers(quadPatches);
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
    if (renderCount++ % 300 == 0) {
        std::cout << "[LODManager::render] Mode: " << currentMode 
                  << ", Instances: " << quadtreeData.instanceCount
                  << ", Indices: " << quadtreeData.indexCount << std::endl;
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
                
                // Draw instanced
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

void LODManager::updateQuadtreeBuffers(const std::vector<core::QuadtreePatch>& patches) {
    // Update instance buffer with patch data
    quadtreeData.instanceCount = static_cast<uint32_t>(patches.size());
    
    if (patches.empty()) return;
    
    // Calculate required instance buffer size
    // Each instance needs: transform matrix, morph params, height texture coords, patch info
    struct InstanceData {
        glm::mat4 transform;
        glm::vec4 morphParams; // morphFactor, neighborLODs
        glm::vec4 heightTexCoord;
        glm::vec4 patchInfo; // level, size, unused, unused
    };
    
    VkDeviceSize instanceBufferSize = sizeof(InstanceData) * patches.size();
    
    // Keep track of current buffer size
    static VkDeviceSize currentBufferSize = 0;
    
    // Only recreate instance buffer if it doesn't exist or size changed
    if (quadtreeData.instanceBuffer == VK_NULL_HANDLE || 
        instanceBufferSize > currentBufferSize) {
        
        // Clean up old buffer if it exists
        if (quadtreeData.instanceBuffer != VK_NULL_HANDLE) {
            destroyBuffer(quadtreeData.instanceBuffer, quadtreeData.instanceMemory);
        }
        
        // Create new buffer with some extra space to avoid frequent recreations
        VkDeviceSize allocSize = instanceBufferSize + sizeof(InstanceData) * 10; // Add space for 10 extra instances
        createBuffer(allocSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    quadtreeData.instanceBuffer, quadtreeData.instanceMemory);
        currentBufferSize = allocSize;
    }
    
    // Map memory and update instance data
    void* data;
    vkMapMemory(device, quadtreeData.instanceMemory, 0, instanceBufferSize, 0, &data);
    
    InstanceData* instanceArray = static_cast<InstanceData*>(data);
    
    // Debug: Log patches with transform details
    static int debugCounter = 0;
    if (debugCounter++ % 100 == 0 && patches.size() > 0) {
        std::cout << "[UpdateQuadtreeBuffers] Updating " << patches.size() << " patches:" << std::endl;
        for (size_t j = 0; j < patches.size(); j++) {
            std::cout << "  Patch " << j << ":" << std::endl;
            std::cout << "    Center: " << patches[j].center.x << "," << patches[j].center.y << "," << patches[j].center.z << std::endl;
            std::cout << "    Corners: [0](" << patches[j].corners[0].x << "," << patches[j].corners[0].y << "," << patches[j].corners[0].z << ")"
                      << " [1](" << patches[j].corners[1].x << "," << patches[j].corners[1].y << "," << patches[j].corners[1].z << ")"
                      << " [2](" << patches[j].corners[2].x << "," << patches[j].corners[2].y << "," << patches[j].corners[2].z << ")"
                      << " [3](" << patches[j].corners[3].x << "," << patches[j].corners[3].y << "," << patches[j].corners[3].z << ")" << std::endl;
        }
    }
    
    for (size_t i = 0; i < patches.size(); i++) {
        const auto& patch = patches[i];
        
        // Compute transform matrix for this patch
        // The patch corners are in cube space (-1 to 1), need to create a transform
        // that maps UV coordinates (0-1) to cube face positions
        glm::mat4 transform = glm::mat4(1.0f);
        
        // Patch corners are in cube space, build transform from them
        glm::vec3 bottomLeft = patch.corners[0];
        glm::vec3 bottomRight = patch.corners[1];
        glm::vec3 topRight = patch.corners[2];
        glm::vec3 topLeft = patch.corners[3];
        
        // Build basis vectors - these define how the UV quad maps to cube face
        glm::vec3 right = bottomRight - bottomLeft;
        glm::vec3 up = topLeft - bottomLeft; // Correct up vector
        
        // Transform maps from UV space (0,0)-(1,1) to cube face positions
        // UV (0,0) should map to bottomLeft, UV(1,1) to topRight
        transform[0] = glm::vec4(right, 0.0f);
        transform[1] = glm::vec4(up, 0.0f);
        transform[2] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f); // Not used for 2D patches
        
        // Translation is the bottom-left corner (UV origin)
        transform[3] = glm::vec4(bottomLeft, 1.0f);
        
        instanceArray[i].transform = transform;
        
        // Set morph parameters
        instanceArray[i].morphParams = glm::vec4(patch.morphFactor, 
                                                 patch.neighborLevels[0],
                                                 patch.neighborLevels[1], 
                                                 patch.neighborLevels[2]);
        
        // Set height texture coordinates (not used yet)
        instanceArray[i].heightTexCoord = glm::vec4(0, 0, 1, 1);
        
        // Set patch info
        instanceArray[i].patchInfo = glm::vec4(patch.level, patch.size, 0.0f, 0.0f);
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