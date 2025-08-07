#include "rendering/gpu_octree.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <functional>

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

void GPUOctree::uploadOctree(octree::OctreePlanet* planet, 
                            const glm::vec3& viewPos,
                            const glm::mat4& viewProj,
                            VkCommandPool commandPool, 
                            VkQueue queue) {
    std::cout << "Uploading octree to GPU using prepareRenderData..." << std::endl;
    
    // Store planet radius for material determination
    this->planetRadius = planet->getRadius();
    
    // Get properly filtered render data from the planet
    // This gives us ONLY the visible nodes with proper materials
    auto renderData = planet->prepareRenderData(viewPos, viewProj);
    
    std::cout << "  prepareRenderData returned:" << std::endl;
    std::cout << "    - " << renderData.nodes.size() << " visible nodes" << std::endl;
    std::cout << "    - " << renderData.voxels.size() << " voxels" << std::endl;
    std::cout << "    - " << renderData.visibleNodes.size() << " visible indices" << std::endl;
    
    // Convert render data to GPU format
    std::vector<GPUOctreeNode> gpuNodes;
    std::vector<GPUVoxelData> gpuVoxels;
    
    // Reserve exact space needed
    gpuNodes.reserve(renderData.nodes.size());
    gpuVoxels.reserve(renderData.voxels.size());
    
    // Convert nodes from OctreeNode::GPUNode to GPUOctreeNode
    for (const auto& node : renderData.nodes) {
        GPUOctreeNode gpuNode;
        gpuNode.centerAndSize = glm::vec4(node.center, node.halfSize);
        gpuNode.childrenAndFlags = glm::uvec4(
            node.childrenIndex,  // x = children offset
            node.voxelIndex,     // y = voxel offset
            node.flags,          // z = flags (isLeaf, material)
            0                    // w = padding
        );
        gpuNodes.push_back(gpuNode);
    }
    
    // Convert voxels to GPU format
    for (const auto& voxel : renderData.voxels) {
        GPUVoxelData gpuVoxel;
        
        // Get blended color from mixed voxel
        glm::vec3 color = voxel.getColor();
        
        // Pack temperature and other properties
        float temp = voxel.temperature / 255.0f;  // Normalize from uint8_t
        float pressure = voxel.pressure / 255.0f;
        
        gpuVoxel.colorAndDensity = glm::vec4(color, pressure);
        gpuVoxel.tempAndVelocity = glm::vec4(temp, 0.0f, 0.0f, 0.0f);
        
        gpuVoxels.push_back(gpuVoxel);
    }
    
    nodeCount = static_cast<uint32_t>(gpuNodes.size());
    voxelCount = static_cast<uint32_t>(gpuVoxels.size());
    
    std::cout << "Converted to GPU format: " << nodeCount << " nodes, " << voxelCount << " voxels" << std::endl;
    
    // DEBUG: Analyze materials in the voxels we're uploading
    int airCount = 0, rockCount = 0, waterCount = 0, magmaCount = 0;
    for (const auto& voxel : renderData.voxels) {
        core::MaterialID mat = voxel.getDominantMaterialID();
        if (mat == core::MaterialID::Air || mat == core::MaterialID::Vacuum) airCount++;
        else if (mat == core::MaterialID::Rock || mat == core::MaterialID::Granite || 
                 mat == core::MaterialID::Basalt) rockCount++;
        else if (mat == core::MaterialID::Water) waterCount++;
        else if (mat == core::MaterialID::Lava) magmaCount++;
    }
    std::cout << "  Material distribution in upload:" << std::endl;
    std::cout << "    Air: " << airCount << " (" << (airCount * 100.0f / voxelCount) << "%)" << std::endl;
    std::cout << "    Rock: " << rockCount << " (" << (rockCount * 100.0f / voxelCount) << "%)" << std::endl;
    std::cout << "    Water: " << waterCount << " (" << (waterCount * 100.0f / voxelCount) << "%)" << std::endl;
    std::cout << "    Magma: " << magmaCount << " (" << (magmaCount * 100.0f / voxelCount) << "%)" << std::endl;
    
    // DEBUG: Print first few nodes to verify data
    for (uint32_t i = 0; i < std::min(5u, nodeCount); i++) {
        const auto& node = gpuNodes[i];
        bool isLeaf = (node.childrenAndFlags.z & 1) != 0;
        uint32_t material = (node.childrenAndFlags.z >> 8) & 0xFF;
        std::cout << "  Node " << i << ": center=(" << node.centerAndSize.x 
                  << "," << node.centerAndSize.y << "," << node.centerAndSize.z 
                  << ") size=" << node.centerAndSize.w
                  << " isLeaf=" << isLeaf << " material=" << material << std::endl;
    }
    
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
        
        // Determine dominant material from all 8 voxels
        const auto& voxels = node->getVoxels();
        
        // DEBUG: Check what we're actually getting
        static int debugCount = 0;
        if (debugCount++ < 5) {
            float dist = glm::length(node->getCenter());
            std::cout << "GPU Reading " << (node->isLeaf() ? "LEAF" : "NON-LEAF") 
                      << " Node " << currentNodeIndex 
                      << " at dist=" << dist << std::endl;
            std::cout << "  Voxel ptr: " << &voxels[0] << std::endl;
            std::cout << "  First voxel dominant material: " << static_cast<int>(voxels[0].getDominantMaterialID()) << std::endl;
            
            // If it's a non-leaf, that's the problem!
            if (!node->isLeaf()) {
                std::cout << "  WARNING: Reading voxels from non-leaf node!" << std::endl;
            }
        }
        
        // Count materials in the voxels
        uint32_t materialCounts[4] = {0, 0, 0, 0}; // Air, Rock, Water, Magma
        
        // DEBUG: Print actual voxel materials for nodes near planet surface
        static int leafDebugCount = 0;
        float distFromCenter = glm::length(node->getCenter());
        // Only debug nodes near the surface (0.9 to 1.1 times radius)
        bool shouldDebug = (distFromCenter > planetRadius * 0.9f && 
                           distFromCenter < planetRadius * 1.1f && 
                           leafDebugCount++ < 10);
        
        if (shouldDebug) {
            std::cout << "DEBUG Surface Leaf " << currentNodeIndex 
                      << " at dist=" << distFromCenter 
                      << " (planet radius=" << planetRadius << ")" 
                      << "\n  Voxel materials: ";
        }
        
        for (int i = 0; i < 8; i++) {
            // DEBUG: Check voxel data
            core::MaterialID matID = voxels[i].getDominantMaterialID();
            uint32_t mat = 0;  // Map to old material numbers for compatibility
            if (matID == core::MaterialID::Air || matID == core::MaterialID::Vacuum) mat = 0;
            else if (matID == core::MaterialID::Rock || matID == core::MaterialID::Granite || matID == core::MaterialID::Basalt) mat = 1;
            else if (matID == core::MaterialID::Water) mat = 2;
            else if (matID == core::MaterialID::Lava) mat = 3;
            if (shouldDebug) {
                std::cout << mat << " ";
            }
            if (mat < 4) {
                materialCounts[mat]++;
            }
        }
        
        if (shouldDebug) {
            std::cout << "\n  Material counts: Air=" << materialCounts[0] 
                      << " Rock=" << materialCounts[1]
                      << " Water=" << materialCounts[2]
                      << " Magma=" << materialCounts[3] << std::endl;
        }
        
        // Find dominant material
        uint32_t maxCount = 0;
        octree::MaterialType dominantMaterial = octree::MaterialType::Air;
        for (int m = 0; m < 4; m++) {
            if (materialCounts[m] > maxCount) {
                maxCount = materialCounts[m];
                dominantMaterial = static_cast<octree::MaterialType>(m);
            }
        }
        
        // If no clear dominant material, use distance-based heuristic
        if (maxCount == 0) {
            float distFromCenter = glm::length(node->getCenter());
            if (distFromCenter < planetRadius * 0.9f) {
                dominantMaterial = octree::MaterialType::Rock; // Core/mantle
            } else if (distFromCenter < planetRadius * 1.1f) {
                // Surface - use simple noise to vary between rock and water
                float noise = sin(node->getCenter().x * 0.00001f) * cos(node->getCenter().z * 0.00001f);
                dominantMaterial = (noise > 0.0f) ? octree::MaterialType::Rock : octree::MaterialType::Water;
            } else {
                dominantMaterial = octree::MaterialType::Air; // Space
            }
        }
        
        gpuNode.childrenAndFlags.z |= (static_cast<uint32_t>(dominantMaterial) << 8);
        
        // DEBUG: Show final dominant material decision
        if (shouldDebug) {
            std::cout << "  -> Dominant material: " << static_cast<int>(dominantMaterial);
            if (maxCount == 0) {
                std::cout << " (FALLBACK - no voxels had valid materials!)";
            }
            std::cout << " (encoded in flags=0x" << std::hex << gpuNode.childrenAndFlags.z << std::dec << ")" << std::endl;
        }
        
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
            
            // Get blended color from mixed voxel
            glm::vec3 color = voxel.getColor();
            
            // Pack temperature and other properties
            float temp = voxel.temperature / 255.0f;  // Normalize from uint8_t
            float pressure = voxel.pressure / 255.0f;
            
            gpuVoxel.colorAndDensity = glm::vec4(color, pressure);
            gpuVoxel.tempAndVelocity = glm::vec4(temp, 0.0f, 0.0f, 0.0f);
            
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