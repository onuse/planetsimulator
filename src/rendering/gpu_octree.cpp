#include "rendering/gpu_octree.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace rendering {

GPUOctree::GPUOctree(VkDevice device, VkPhysicalDevice physicalDevice) 
    : device(device), physicalDevice(physicalDevice) {
}

GPUOctree::~GPUOctree() {
    if (nodeBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, nodeBuffer, nullptr);
        vkFreeMemory(device, nodeBufferMemory, nullptr);
    }
    if (voxelBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelBufferMemory, nullptr);
    }
    if (stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
}

void GPUOctree::uploadOctree(const octree::OctreePlanet* planet, VkCommandPool commandPool, VkQueue queue) {
    std::cout << "Uploading octree to GPU..." << std::endl;
    
    // Flatten the octree into GPU-friendly arrays
    std::vector<GPUOctreeNode> gpuNodes;
    std::vector<GPUVoxelData> gpuVoxels;
    
    // Reserve space (start conservatively)
    gpuNodes.reserve(100000);
    gpuVoxels.reserve(100000 * 8);
    
    uint32_t nodeIndex = 0;
    uint32_t voxelIndex = 0;
    
    // Start flattening from root
    if (planet->getRoot()) {
        flattenOctree(planet->getRoot(), gpuNodes, gpuVoxels, nodeIndex, voxelIndex);
    }
    
    nodeCount = static_cast<uint32_t>(gpuNodes.size());
    voxelCount = static_cast<uint32_t>(gpuVoxels.size());
    
    std::cout << "Flattened octree: " << nodeCount << " nodes, " << voxelCount << " voxels" << std::endl;
    
    // Create GPU buffers
    VkDeviceSize nodeBufferSize = sizeof(GPUOctreeNode) * nodeCount;
    VkDeviceSize voxelBufferSize = sizeof(GPUVoxelData) * voxelCount;
    
    // Clean up old buffers if they exist
    if (nodeBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, nodeBuffer, nullptr);
        vkFreeMemory(device, nodeBufferMemory, nullptr);
    }
    if (voxelBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelBufferMemory, nullptr);
    }
    
    // Create node buffer
    createBuffer(nodeBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                nodeBuffer, nodeBufferMemory);
    
    // Create voxel buffer
    createBuffer(voxelBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                voxelBuffer, voxelBufferMemory);
    
    // Upload data via staging buffer
    VkDeviceSize totalSize = nodeBufferSize + voxelBufferSize;
    
    // Create staging buffer
    VkBuffer localStagingBuffer;
    VkDeviceMemory localStagingMemory;
    createBuffer(totalSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                localStagingBuffer, localStagingMemory);
    
    // Copy data to staging buffer
    void* data;
    vkMapMemory(device, localStagingMemory, 0, totalSize, 0, &data);
    
    // Copy nodes
    memcpy(data, gpuNodes.data(), nodeBufferSize);
    // Copy voxels after nodes
    memcpy(static_cast<char*>(data) + nodeBufferSize, gpuVoxels.data(), voxelBufferSize);
    
    vkUnmapMemory(device, localStagingMemory);
    
    // Copy from staging to device buffers
    // First copy nodes to node buffer
    copyBuffer(localStagingBuffer, nodeBuffer, nodeBufferSize, commandPool, queue);
    
    // Then copy voxels to voxel buffer (from the offset in staging buffer)
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
    copyRegion.srcOffset = nodeBufferSize;  // Voxels start after nodes in staging buffer
    copyRegion.dstOffset = 0;
    copyRegion.size = voxelBufferSize;
    vkCmdCopyBuffer(commandBuffer, localStagingBuffer, voxelBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Clean up staging buffer
    vkDestroyBuffer(device, localStagingBuffer, nullptr);
    vkFreeMemory(device, localStagingMemory, nullptr);
    
    std::cout << "GPU octree buffers created and copied" << std::endl;
}

void GPUOctree::flattenOctree(const octree::OctreeNode* node,
                              std::vector<GPUOctreeNode>& gpuNodes,
                              std::vector<GPUVoxelData>& gpuVoxels,
                              uint32_t& nodeIndex,
                              uint32_t& voxelIndex) {
    
    uint32_t currentNodeIndex = nodeIndex++;
    
    // Add placeholder node (we'll fill it in after we know children indices)
    gpuNodes.push_back(GPUOctreeNode());
    
    GPUOctreeNode& gpuNode = gpuNodes[currentNodeIndex];
    gpuNode.centerAndSize = glm::vec4(node->getCenter(), node->getHalfSize());
    
    // Debug: Print root node bounds
    if (currentNodeIndex == 0) {
        std::cout << "Root node: center=(" << node->getCenter().x << ", " 
                  << node->getCenter().y << ", " << node->getCenter().z 
                  << "), halfSize=" << node->getHalfSize() << std::endl;
    }
    
    if (node->isLeaf()) {
        // Leaf node - store voxel data
        gpuNode.childrenAndFlags.x = 0xFFFFFFFF; // No children
        gpuNode.childrenAndFlags.y = voxelIndex;  // Voxel data offset
        gpuNode.childrenAndFlags.z = 1;           // isLeaf flag
        
        // Determine dominant material
        const auto& voxels = node->getVoxels();
        octree::MaterialType dominantMaterial = voxels[0].material;
        gpuNode.childrenAndFlags.z |= (static_cast<uint32_t>(dominantMaterial) << 8);
        
        // Debug: Print first few leaf nodes with non-air material
        static int solidLeafCount = 0;
        if (dominantMaterial != octree::MaterialType::Air && solidLeafCount++ < 10) {
            std::cout << "Leaf node " << currentNodeIndex 
                      << " at (" << node->getCenter().x << ", " 
                      << node->getCenter().y << ", " << node->getCenter().z 
                      << "), halfSize=" << node->getHalfSize()
                      << ", material=" << static_cast<int>(dominantMaterial) << std::endl;
        }
        
        // Add voxel data
        for (const auto& voxel : voxels) {
            GPUVoxelData gpuVoxel;
            
            // Map material to color
            glm::vec3 color;
            switch (voxel.material) {
                case octree::MaterialType::Air:
                    color = glm::vec3(0.7f, 0.85f, 1.0f);
                    break;
                case octree::MaterialType::Rock:
                    color = glm::vec3(0.5f, 0.4f, 0.3f);
                    break;
                case octree::MaterialType::Water:
                    color = glm::vec3(0.0f, 0.3f, 0.7f);
                    break;
                case octree::MaterialType::Magma:
                    color = glm::vec3(1.0f, 0.3f, 0.0f);
                    break;
                default:
                    color = glm::vec3(0.5f, 0.5f, 0.5f);
            }
            
            gpuVoxel.colorAndDensity = glm::vec4(color, voxel.density);
            gpuVoxel.tempAndVelocity = glm::vec4(voxel.temperature, voxel.velocity);
            
            gpuVoxels.push_back(gpuVoxel);
            voxelIndex++;
        }
    } else {
        // Internal node - we need to set children offset AFTER processing children
        // For now, mark as internal with placeholder
        gpuNode.childrenAndFlags.x = 0xFFFFFFFE; // Temporary marker
        gpuNode.childrenAndFlags.y = 0xFFFFFFFF; // No voxel data
        gpuNode.childrenAndFlags.z = 0;          // Not a leaf
        
        // Remember where children will start
        uint32_t firstChildIndex = nodeIndex;
        
        // Debug: Track first few internal nodes
        static int debugCount = 0;
        if (debugCount++ < 10) {
            std::cout << "Internal node " << currentNodeIndex 
                      << ": children start at " << firstChildIndex
                      << ", center=(" << node->getCenter().x << "," 
                      << node->getCenter().y << "," << node->getCenter().z 
                      << "), halfSize=" << node->getHalfSize() << std::endl;
        }
        
        // Process all children
        const auto& children = node->getChildren();
        int validChildren = 0;
        for (int i = 0; i < 8; i++) {
            if (children[i]) {
                flattenOctree(children[i].get(), gpuNodes, gpuVoxels, nodeIndex, voxelIndex);
                validChildren++;
            } else {
                // Calculate child position for empty node
                float childHalfSize = node->getHalfSize() * 0.5f;
                glm::vec3 childOffset(
                    (i & 1) ? childHalfSize : -childHalfSize,
                    (i & 2) ? childHalfSize : -childHalfSize,
                    (i & 4) ? childHalfSize : -childHalfSize
                );
                glm::vec3 childCenter = node->getCenter() + childOffset;
                
                // Add empty leaf node for missing child
                GPUOctreeNode emptyNode;
                emptyNode.centerAndSize = glm::vec4(childCenter, childHalfSize);
                emptyNode.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0xFFFFFFFF, 1 | (0u << 8), 0); // Leaf with air
                gpuNodes.push_back(emptyNode);
                nodeIndex++;
            }
        }
        
        // Now update the parent node with the correct children offset
        gpuNodes[currentNodeIndex].childrenAndFlags.x = firstChildIndex;
        
        if (currentNodeIndex == 0) {
            std::cout << "Root has " << validChildren << " valid children" << std::endl;
            std::cout << "Root children start at index " << firstChildIndex << std::endl;
        }
    }
}

void GPUOctree::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
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

uint32_t GPUOctree::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
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

VkDescriptorSetLayout GPUOctree::createDescriptorSetLayout() const {
    // TODO: Implement descriptor set layout creation
    return VK_NULL_HANDLE;
}

void GPUOctree::updateDescriptorSet(VkDescriptorSet descriptorSet) const {
    // TODO: Implement descriptor set update
}

void GPUOctree::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
                          VkCommandPool commandPool, VkQueue queue) {
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