#include "rendering/hierarchical_gpu_octree.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace rendering {

HierarchicalGPUOctree::HierarchicalGPUOctree(VkDevice device, VkPhysicalDevice physicalDevice)
    : device(device), physicalDevice(physicalDevice) {
}

HierarchicalGPUOctree::~HierarchicalGPUOctree() {
    if (nodeBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, nodeBuffer, nullptr);
        vkFreeMemory(device, nodeBufferMemory, nullptr);
    }
    if (voxelBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelBufferMemory, nullptr);
    }
    if (visibilityBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, visibilityBuffer, nullptr);
        vkFreeMemory(device, visibilityBufferMemory, nullptr);
    }
}

void HierarchicalGPUOctree::Frustum::extractFromMatrix(const glm::mat4& viewProj) {
    // Left plane
    planes[0] = glm::vec4(
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]
    );
    
    // Right plane
    planes[1] = glm::vec4(
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]
    );
    
    // Bottom plane
    planes[2] = glm::vec4(
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]
    );
    
    // Top plane
    planes[3] = glm::vec4(
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]
    );
    
    // Near plane
    planes[4] = glm::vec4(
        viewProj[0][3] + viewProj[0][2],
        viewProj[1][3] + viewProj[1][2],
        viewProj[2][3] + viewProj[2][2],
        viewProj[3][3] + viewProj[3][2]
    );
    
    // Far plane
    planes[5] = glm::vec4(
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]
    );
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }
}

bool HierarchicalGPUOctree::Frustum::intersectsSphere(const glm::vec3& center, float radius) const {
    for (int i = 0; i < 6; i++) {
        float distance = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (distance < -radius) {
            return false; // Sphere is completely outside this plane
        }
    }
    return true;
}

bool HierarchicalGPUOctree::Frustum::intersectsAABB(const glm::vec3& min, const glm::vec3& max) const {
    for (int i = 0; i < 6; i++) {
        glm::vec3 positive(
            planes[i].x > 0 ? max.x : min.x,
            planes[i].y > 0 ? max.y : min.y,
            planes[i].z > 0 ? max.z : min.z
        );
        
        float distance = glm::dot(glm::vec3(planes[i]), positive) + planes[i].w;
        if (distance < 0) {
            return false; // AABB is completely outside this plane
        }
    }
    return true;
}

float HierarchicalGPUOctree::calculateScreenSpaceError(const octree::OctreeNode* node,
                                                       const glm::mat4& viewProj,
                                                       const glm::vec3& viewPos) const {
    // Calculate distance from viewer
    float distance = glm::length(node->getCenter() - viewPos);
    
    // Project node size to screen space
    glm::vec4 centerProj = viewProj * glm::vec4(node->getCenter(), 1.0f);
    glm::vec4 edgeProj = viewProj * glm::vec4(node->getCenter() + glm::vec3(node->getHalfSize(), 0, 0), 1.0f);
    
    if (centerProj.w > 0 && edgeProj.w > 0) {
        glm::vec2 centerScreen = glm::vec2(centerProj) / centerProj.w;
        glm::vec2 edgeScreen = glm::vec2(edgeProj) / edgeProj.w;
        float screenSize = glm::length(edgeScreen - centerScreen);
        
        // Error metric: ratio of node size to screen size
        return node->getHalfSize() / (distance * screenSize + 0.001f);
    }
    
    // Default: use distance-based metric
    return node->getHalfSize() / (distance + 0.001f);
}

uint32_t HierarchicalGPUOctree::selectLOD(const octree::OctreeNode* node,
                                          const glm::vec3& viewPos,
                                          float qualityFactor) const {
    float distance = glm::length(node->getCenter() - viewPos);
    
    // LOD levels based on distance/size ratio
    // Adjusted for smoother transitions and less aggressive reduction
    float ratio = distance / (node->getHalfSize() * qualityFactor);
    
    if (ratio < 50.0f) return 0;       // Maximum detail (was 10)
    if (ratio < 200.0f) return 1;      // High detail (was 50)
    if (ratio < 500.0f) return 2;      // Medium detail (was 200)
    if (ratio < 2000.0f) return 3;     // Low detail (was 1000)
    return 4;                           // Minimum detail
}

void HierarchicalGPUOctree::traverseWithCulling(const octree::OctreeNode* node,
                                                const Frustum& frustum,
                                                const glm::vec3& viewPos,
                                                std::vector<GPUNode>& gpuNodes,
                                                std::vector<GPUVoxelData>& gpuVoxels,
                                                uint32_t& nodeIndex,
                                                uint32_t& voxelIndex,
                                                uint32_t parentIndex) {
    
    // Calculate node bounds
    glm::vec3 nodeMin = node->getCenter() - glm::vec3(node->getHalfSize());
    glm::vec3 nodeMax = node->getCenter() + glm::vec3(node->getHalfSize());
    
    // Frustum culling - sphere check first (faster)
    if (!frustum.intersectsSphere(node->getCenter(), node->getHalfSize() * 1.732f)) {
        visibilityInfo.culledNodes++;
        return; // Node is outside frustum
    }
    
    // More precise AABB check
    if (!frustum.intersectsAABB(nodeMin, nodeMax)) {
        visibilityInfo.culledNodes++;
        return;
    }
    
    // LOD selection
    uint32_t lodLevel = selectLOD(node, viewPos);
    
    // Check if we should stop at this LOD level
    // Only stop at higher LOD levels to avoid aggressive transitions
    bool stopAtThisLevel = node->isLeaf() || 
                           lodLevel >= 3 || // Only use lower detail for very distant nodes
                           !node->getChildren()[0]; // No children available
    
    uint32_t currentNodeIndex = nodeIndex++;
    gpuNodes.push_back(GPUNode());
    
    GPUNode& gpuNode = gpuNodes[currentNodeIndex];
    gpuNode.centerAndSize = glm::vec4(node->getCenter(), node->getHalfSize());
    gpuNode.boundsMin = glm::vec4(nodeMin, 0);
    gpuNode.boundsMax = glm::vec4(nodeMax, 0);
    gpuNode.childrenAndFlags.w = lodLevel;
    
    // Update parent's child pointer if needed
    if (parentIndex != 0xFFFFFFFF && parentIndex < gpuNodes.size()) {
        if (gpuNodes[parentIndex].childrenAndFlags.x == 0xFFFFFFFF) {
            gpuNodes[parentIndex].childrenAndFlags.x = currentNodeIndex;
        }
    }
    
    if (stopAtThisLevel) {
        // Treat as leaf for rendering
        gpuNode.childrenAndFlags.x = 0xFFFFFFFF; // No children
        gpuNode.childrenAndFlags.y = voxelIndex;  // Voxel data offset
        gpuNode.childrenAndFlags.z = 1;           // isLeaf flag
        
        // Calculate dominant material
        const auto& voxels = node->getVoxels();
        uint32_t materialCounts[4] = {0, 0, 0, 0};
        
        for (const auto& voxel : voxels) {
            // Map material ID to old material numbers for compatibility
            core::MaterialID matID = voxel.getDominantMaterialID();
            uint32_t mat = 0;
            if (matID == core::MaterialID::Rock || matID == core::MaterialID::Granite || matID == core::MaterialID::Basalt) mat = 1;
            else if (matID == core::MaterialID::Water) mat = 2;
            else if (matID == core::MaterialID::Lava) mat = 3;
            
            if (mat < 4) materialCounts[mat]++;
        }
        
        uint32_t maxCount = 0;
        octree::MaterialType dominantMaterial = octree::MaterialType::Air;
        for (int m = 0; m < 4; m++) {
            if (materialCounts[m] > maxCount) {
                maxCount = materialCounts[m];
                dominantMaterial = static_cast<octree::MaterialType>(m);
            }
        }
        
        // Encode material
        gpuNode.childrenAndFlags.z |= (static_cast<uint32_t>(dominantMaterial) << 8);
        
        // Add to visible nodes
        visibilityInfo.visibleNodes.push_back(currentNodeIndex);
        visibilityInfo.lodLevels.push_back(lodLevel);
        
        // Store voxel data
        for (const auto& voxel : voxels) {
            GPUVoxelData gpuVoxel;
            
            // Get blended color from mixed voxel
            glm::vec3 color = voxel.getColor();
            
            float temp = voxel.temperature / 255.0f;
            float pressure = voxel.pressure / 255.0f;
            gpuVoxel.colorAndDensity = glm::vec4(color, pressure);
            gpuVoxel.tempAndVelocity = glm::vec4(temp, 0.0f, 0.0f, 0.0f);
            
            gpuVoxels.push_back(gpuVoxel);
            voxelIndex++;
        }
    } else {
        // Internal node - traverse children
        gpuNode.childrenAndFlags.x = nodeIndex; // Children will start here
        gpuNode.childrenAndFlags.y = 0xFFFFFFFF; // No voxel data
        gpuNode.childrenAndFlags.z = 0;          // Not a leaf
        
        // Traverse children with culling
        const auto& children = node->getChildren();
        for (const auto& child : children) {
            if (child) {
                traverseWithCulling(child.get(), frustum, viewPos,
                                  gpuNodes, gpuVoxels, nodeIndex, voxelIndex,
                                  currentNodeIndex);
            }
        }
    }
}

void HierarchicalGPUOctree::uploadOctree(const octree::OctreePlanet* planet,
                                         const glm::mat4& viewProj,
                                         const glm::vec3& viewPos,
                                         VkCommandPool commandPool,
                                         VkQueue queue) {
    
    std::cout << "Uploading hierarchical octree with frustum culling..." << std::endl;
    
    // Store planet radius
    this->planetRadius = planet->getRadius();
    
    // Extract frustum from view-projection matrix
    Frustum frustum;
    frustum.extractFromMatrix(viewProj);
    
    // Clear visibility info
    visibilityInfo.visibleNodes.clear();
    visibilityInfo.lodLevels.clear();
    visibilityInfo.culledNodes = 0;
    visibilityInfo.lodTransitions = 0;
    
    // Prepare GPU data
    std::vector<GPUNode> gpuNodes;
    std::vector<GPUVoxelData> gpuVoxels;
    
    gpuNodes.reserve(100000);
    gpuVoxels.reserve(100000 * 8);
    
    uint32_t nodeIndex = 0;
    uint32_t voxelIndex = 0;
    
    // Traverse with frustum culling and LOD
    if (planet->getRoot()) {
        traverseWithCulling(planet->getRoot(), frustum, viewPos,
                          gpuNodes, gpuVoxels, nodeIndex, voxelIndex);
    }
    
    visibilityInfo.totalNodes = static_cast<uint32_t>(gpuNodes.size());
    
    std::cout << "Hierarchical traversal complete:" << std::endl;
    std::cout << "  Total nodes: " << visibilityInfo.totalNodes << std::endl;
    std::cout << "  Visible nodes: " << visibilityInfo.visibleNodes.size() << std::endl;
    std::cout << "  Culled nodes: " << visibilityInfo.culledNodes << std::endl;
    
    // Count LOD distribution
    uint32_t lodCounts[5] = {0, 0, 0, 0, 0};
    for (uint32_t lod : visibilityInfo.lodLevels) {
        if (lod < 5) lodCounts[lod]++;
    }
    std::cout << "  LOD distribution: ";
    for (int i = 0; i < 5; i++) {
        std::cout << "L" << i << "=" << lodCounts[i] << " ";
    }
    std::cout << std::endl;
    
    // Now upload to GPU buffers
    if (gpuNodes.empty()) {
        std::cout << "WARNING: No nodes to upload!" << std::endl;
        return;
    }
    
    VkDeviceSize nodeBufferSize = sizeof(GPUNode) * gpuNodes.size();
    VkDeviceSize voxelBufferSize = sizeof(GPUVoxelData) * gpuVoxels.size();
    VkDeviceSize visibilityBufferSize = sizeof(uint32_t) * visibilityInfo.visibleNodes.size();
    
    // Clean up old buffers if they exist
    if (nodeBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, nodeBuffer, nullptr);
        vkFreeMemory(device, nodeBufferMemory, nullptr);
    }
    if (voxelBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelBufferMemory, nullptr);
    }
    if (visibilityBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, visibilityBuffer, nullptr);
        vkFreeMemory(device, visibilityBufferMemory, nullptr);
    }
    
    // Create new buffers
    createBuffer(nodeBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                nodeBuffer, nodeBufferMemory);
    
    if (voxelBufferSize > 0) {
        createBuffer(voxelBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    voxelBuffer, voxelBufferMemory);
    }
    
    if (visibilityBufferSize > 0) {
        createBuffer(visibilityBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    visibilityBuffer, visibilityBufferMemory);
    }
    
    // Upload via staging buffer
    VkDeviceSize totalSize = nodeBufferSize + voxelBufferSize + visibilityBufferSize;
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(totalSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);
    
    // Copy data to staging buffer
    void* data;
    vkMapMemory(device, stagingMemory, 0, totalSize, 0, &data);
    
    size_t offset = 0;
    memcpy(static_cast<char*>(data) + offset, gpuNodes.data(), nodeBufferSize);
    offset += nodeBufferSize;
    
    if (voxelBufferSize > 0) {
        memcpy(static_cast<char*>(data) + offset, gpuVoxels.data(), voxelBufferSize);
        offset += voxelBufferSize;
    }
    
    if (visibilityBufferSize > 0) {
        memcpy(static_cast<char*>(data) + offset, visibilityInfo.visibleNodes.data(), visibilityBufferSize);
    }
    
    vkUnmapMemory(device, stagingMemory);
    
    // Copy from staging to device buffers
    copyBuffer(stagingBuffer, nodeBuffer, nodeBufferSize, commandPool, queue);
    
    if (voxelBufferSize > 0) {
        // Copy voxels with offset
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
        copyRegion.srcOffset = nodeBufferSize;
        copyRegion.dstOffset = 0;
        copyRegion.size = voxelBufferSize;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, voxelBuffer, 1, &copyRegion);
        
        if (visibilityBufferSize > 0) {
            copyRegion.srcOffset = nodeBufferSize + voxelBufferSize;
            copyRegion.dstOffset = 0;
            copyRegion.size = visibilityBufferSize;
            vkCmdCopyBuffer(commandBuffer, stagingBuffer, visibilityBuffer, 1, &copyRegion);
        }
        
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    
    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    
    std::cout << "Hierarchical GPU octree buffers uploaded successfully" << std::endl;
}

// Buffer management functions would be the same as in GPUOctree
void HierarchicalGPUOctree::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                         VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                         VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }
    
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

uint32_t HierarchicalGPUOctree::findMemoryType(uint32_t typeFilter, 
                                               VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

void HierarchicalGPUOctree::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, 
                                       VkDeviceSize size, VkCommandPool commandPool, 
                                       VkQueue queue) {
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
    
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

} // namespace rendering