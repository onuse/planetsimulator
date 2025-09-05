#include "rendering/gpu_octree.hpp"
#include <iostream>
#include <cstring>

namespace rendering {

// Minimal implementation - just upload octree data
GPUOctree::GPUOctree(VkDevice device, VkPhysicalDevice physicalDevice) 
    : device(device), physicalDevice(physicalDevice) {
    std::cout << "[SimpleGPUOctree] Constructor called at " << this << " with device=" << device << "\n";
}

GPUOctree::GPUOctree(GPUOctree&& other) noexcept
    : device(other.device), physicalDevice(other.physicalDevice),
      nodeBuffer(other.nodeBuffer), nodeBufferMemory(other.nodeBufferMemory),
      voxelBuffer(other.voxelBuffer), voxelBufferMemory(other.voxelBufferMemory),
      nodeCount(other.nodeCount) {
    std::cout << "[SimpleGPUOctree] Move constructor called: " << &other << " -> " << this << "\n";
    std::cout << "  Moving nodeBuffer=" << other.nodeBuffer << " with " << other.nodeCount << " nodes\n";
    // Clear the moved-from object
    other.nodeBuffer = VK_NULL_HANDLE;
    other.nodeBufferMemory = VK_NULL_HANDLE;
    other.voxelBuffer = VK_NULL_HANDLE;
    other.voxelBufferMemory = VK_NULL_HANDLE;
    other.nodeCount = 0;
    other.device = VK_NULL_HANDLE;  // Also clear device to prevent destructor from running
}

GPUOctree& GPUOctree::operator=(GPUOctree&& other) noexcept {
    std::cout << "[SimpleGPUOctree] Move assignment called: " << &other << " -> " << this << "\n";
    if (this != &other) {
        // Clean up existing resources manually (don't call destructor)
        if (nodeBuffer != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, nodeBuffer, nullptr);
        }
        if (nodeBufferMemory != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkFreeMemory(device, nodeBufferMemory, nullptr);
        }
        if (voxelBuffer != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, voxelBuffer, nullptr);
        }
        if (voxelBufferMemory != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkFreeMemory(device, voxelBufferMemory, nullptr);
        }
        
        // Move from other
        device = other.device;
        physicalDevice = other.physicalDevice;
        nodeBuffer = other.nodeBuffer;
        nodeBufferMemory = other.nodeBufferMemory;
        voxelBuffer = other.voxelBuffer;
        voxelBufferMemory = other.voxelBufferMemory;
        nodeCount = other.nodeCount;
        
        std::cout << "  Moved nodeBuffer=" << nodeBuffer << " with " << nodeCount << " nodes\n";
        
        // Clear the moved-from object
        other.nodeBuffer = VK_NULL_HANDLE;
        other.nodeBufferMemory = VK_NULL_HANDLE;
        other.voxelBuffer = VK_NULL_HANDLE;
        other.voxelBufferMemory = VK_NULL_HANDLE;
        other.nodeCount = 0;
        other.device = VK_NULL_HANDLE;  // Also clear device to prevent destructor from running
    }
    return *this;
}

GPUOctree::~GPUOctree() {
    std::cout << "\n[SimpleGPUOctree] *** DESTRUCTOR CALLED ***\n";
    std::cout << "  Object address: " << this << "\n";
    std::cout << "  Device: " << device << "\n";
    std::cout << "  nodeBuffer: " << nodeBuffer << "\n";
    std::cout << "  nodeCount: " << nodeCount << "\n" << std::flush;
    
    // Try to get some context about why we're being destroyed
    #ifdef _WIN32
    std::cout << "  Stack trace would be helpful here to understand why destructor is called\n";
    #endif
    
    // Safety check - if device looks invalid or null, don't try to destroy resources
    if (device == VK_NULL_HANDLE) {
        std::cout << "[SimpleGPUOctree] Device is VK_NULL_HANDLE, skipping cleanup (likely moved-from object)\n";
        return;
    }
    
    if ((uint64_t)device > 0xFFFFFFFF00000000ULL) {
        std::cerr << "[SimpleGPUOctree] WARNING: Device pointer looks corrupted (" << std::hex << (uint64_t)device << std::dec << "), skipping cleanup!\n";
        return;
    }
    
    // Additional validation - check if device is still valid
    // Just try to query properties - if it crashes, we know device is bad
    // vkGetPhysicalDeviceProperties doesn't return error codes
    // VkPhysicalDeviceProperties props;
    // vkGetPhysicalDeviceProperties(physicalDevice, &props);
    // If we get here, physical device is probably still valid
    
    // Clean up node buffer
    if (nodeBuffer != VK_NULL_HANDLE) {
        std::cout << "[SimpleGPUOctree] Destroying nodeBuffer=" << nodeBuffer << "\n" << std::flush;
        vkDestroyBuffer(device, nodeBuffer, nullptr);
        std::cout << "[SimpleGPUOctree] nodeBuffer destroyed\n" << std::flush;
    }
    if (nodeBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, nodeBufferMemory, nullptr);
    }
    
    // Clean up voxel buffer  
    if (voxelBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
    }
    if (voxelBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, voxelBufferMemory, nullptr);
    }
    
    std::cout << "[SimpleGPUOctree] Cleanup complete\n";
}

void GPUOctree::uploadOctree(octree::OctreePlanet* planet, 
                            const glm::vec3& viewPos,
                            const glm::mat4& viewProj,
                            VkCommandPool commandPool, 
                            VkQueue queue) {
    
    // Define GPU data structures first
    struct SimpleGPUNode {
        glm::vec4 centerAndSize;
        glm::uvec4 childrenAndFlags;
    };
    
    struct SimpleGPUVoxel {
        glm::vec4 colorAndDensity;
        glm::vec4 extra;
    };
    
    std::cout << "[SimpleGPUOctree] Starting upload...\n" << std::flush;
    
    // Get FULL octree data from planet (not just visible nodes)
    std::cout << "[SimpleGPUOctree] Calling prepareFullOctreeData for complete hierarchy...\n" << std::flush;
    auto renderData = planet->prepareFullOctreeData();
    std::cout << "[SimpleGPUOctree] prepareFullOctreeData returned\n" << std::flush;
    std::cout << "  Got " << renderData.nodes.size() << " nodes, " 
              << renderData.voxels.size() << " voxels\n" << std::flush;
    
    // Debug: Print first few nodes
    if (renderData.nodes.size() > 0) {
        const auto& root = renderData.nodes[0];
        std::cout << "  Root node: center=(" << root.center.x << ", " << root.center.y << ", " << root.center.z 
                  << "), halfSize=" << root.halfSize 
                  << ", childrenIndex=" << root.childrenIndex
                  << ", voxelIndex=" << root.voxelIndex
                  << ", flags=" << root.flags << "\n";
    }
    
    // Debug: Print first few voxels to see what materials we have
    if (renderData.voxels.size() > 8) {
        std::cout << "  First 8 voxels:\n";
        for (size_t i = 0; i < 8 && i < renderData.voxels.size(); i++) {
            const auto& voxel = renderData.voxels[i];
            auto materialID = voxel.getDominantMaterialID();
            std::cout << "    Voxel[" << i << "]: material=" << static_cast<int>(materialID)
                      << " (";
            switch(materialID) {
                case core::MaterialID::Vacuum: std::cout << "Vacuum"; break;
                case core::MaterialID::Air: std::cout << "Air"; break;
                case core::MaterialID::Rock: std::cout << "Rock"; break;
                case core::MaterialID::Water: std::cout << "Water"; break;
                case core::MaterialID::Sand: std::cout << "Sand"; break;
                case core::MaterialID::Grass: std::cout << "Grass"; break;
                case core::MaterialID::Snow: std::cout << "Snow"; break;
                default: std::cout << "Unknown"; break;
            }
            std::cout << ")\n";
        }
    }
    
    // Store counts
    nodeCount = static_cast<uint32_t>(renderData.nodes.size());
    uint32_t voxelDataCount = static_cast<uint32_t>(renderData.voxels.size());
    
    std::vector<SimpleGPUNode> gpuNodes(nodeCount);
    std::vector<SimpleGPUVoxel> gpuVoxels(voxelDataCount);
    
    // Convert nodes
    for (size_t i = 0; i < nodeCount; i++) {
        const auto& node = renderData.nodes[i];
        gpuNodes[i].centerAndSize = glm::vec4(node.center, node.halfSize);
        
        // Use the actual flags from the node
        uint32_t flags = node.flags;
        
        gpuNodes[i].childrenAndFlags = glm::uvec4(
            node.childrenIndex,  // children offset 
            node.voxelIndex,     // voxel offset  
            flags,               // flags
            0                    // padding
        );
    }
    
    // Convert voxels - pack material data for GPU
    for (size_t i = 0; i < voxelDataCount; i++) {
        const auto& voxel = renderData.voxels[i];
        
        // Get dominant material and its properties
        auto materialID = voxel.getDominantMaterialID();
        // Get density as normalized maximum material amount
        uint8_t maxAmount = 0;
        for (int j = 0; j < 4; j++) {
            maxAmount = std::max(maxAmount, voxel.getMaterialAmount(j));
        }
        float density = maxAmount / 255.0f;
        
        // Map material ID to color (matching CPU implementation)
        glm::vec3 color;
        switch(materialID) {
            case core::MaterialID::Water:
                color = glm::vec3(0.1f, 0.3f, 0.6f);
                break;
            case core::MaterialID::Sand:
                color = glm::vec3(0.9f, 0.85f, 0.65f);
                break;
            case core::MaterialID::Grass:
                color = glm::vec3(0.2f, 0.6f, 0.2f);
                break;
            case core::MaterialID::Rock:
                color = glm::vec3(0.4f, 0.3f, 0.2f);
                break;
            case core::MaterialID::Snow:
                color = glm::vec3(0.95f, 0.95f, 0.98f);
                break;
            default:
                color = glm::vec3(0.5f, 0.5f, 0.5f);
                break;
        }
        
        // Pack: xyz = color, w = density
        gpuVoxels[i].colorAndDensity = glm::vec4(color, density);
        // Pack: x = material ID, y = temperature, z = pressure, w = reserved
        gpuVoxels[i].extra = glm::vec4(
            static_cast<float>(materialID),
            static_cast<float>(voxel.temperature) / 255.0f,
            static_cast<float>(voxel.pressure) / 255.0f,
            0.0f
        );
    }
    
    // Calculate buffer sizes
    VkDeviceSize nodeBufferSize = sizeof(SimpleGPUNode) * nodeCount;
    VkDeviceSize voxelBufferSize = sizeof(SimpleGPUVoxel) * voxelDataCount;
    VkDeviceSize totalSize = nodeBufferSize + voxelBufferSize;
    
    std::cout << "  Node buffer size: " << (nodeBufferSize / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "  Voxel buffer size: " << (voxelBufferSize / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "  Total GPU memory needed: " << (totalSize / (1024.0 * 1024.0)) << " MB\n";
    
    // Check if allocation is reasonable
    if (totalSize > 2ULL * 1024 * 1024 * 1024) {  // > 2GB
        std::cerr << "ERROR: Octree too large for GPU! (" 
                  << (totalSize / (1024.0 * 1024.0 * 1024.0)) << " GB)\n";
        std::cerr << "Consider reducing octree depth or using LOD.\n";
        return;
    }
    
    // Create device buffers
    createBuffer(nodeBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                nodeBuffer, nodeBufferMemory);
    
    if (nodeBuffer == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Failed to create node buffer!\n";
        return;
    }
    
    createBuffer(voxelBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                voxelBuffer, voxelBufferMemory);
    
    if (voxelBuffer == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Failed to create voxel buffer!\n";
        // Clean up node buffer
        if (nodeBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, nodeBuffer, nullptr);
            nodeBuffer = VK_NULL_HANDLE;
        }
        if (nodeBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, nodeBufferMemory, nullptr);
            nodeBufferMemory = VK_NULL_HANDLE;
        }
        return;
    }
    
    std::cout << "  Device buffers created successfully\n";
    
    // Create staging buffer for both
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    // totalSize already defined above
    
    createBuffer(totalSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);
    
    // Map and copy data
    void* data;
    vkMapMemory(device, stagingMemory, 0, totalSize, 0, &data);
    
    // Copy nodes first
    memcpy(data, gpuNodes.data(), nodeBufferSize);
    
    // Copy voxels after nodes
    memcpy(static_cast<char*>(data) + nodeBufferSize, gpuVoxels.data(), voxelBufferSize);
    
    vkUnmapMemory(device, stagingMemory);
    
    std::cout << "  Data copied to staging buffer\n";
    
    // Copy from staging to device buffers using command buffer
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    // Copy nodes
    VkBufferCopy copyRegion{};
    copyRegion.size = nodeBufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, nodeBuffer, 1, &copyRegion);
    
    // Copy voxels
    copyRegion.srcOffset = nodeBufferSize;
    copyRegion.dstOffset = 0;
    copyRegion.size = voxelBufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, voxelBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    
    // Clean up
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    
    std::cout << "[SimpleGPUOctree] Upload complete!\n";
}

// Helper function implementations
void GPUOctree::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VkBuffer& buffer,
                             VkDeviceMemory& bufferMemory) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer!\n";
        return;
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory!\n";
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return;
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
    
    std::cerr << "Failed to find suitable memory type!\n";
    return 0;
}

void GPUOctree::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
                           VkCommandPool commandPool, VkQueue queue) {
    // Not needed for minimal implementation - we handle copying inline
}

// Stub implementations for unused functions
void GPUOctree::flattenOctree(const octree::OctreeNode* node,
                              std::vector<GPUOctreeNode>& gpuNodes,
                              std::vector<GPUVoxelData>& gpuVoxels,
                              uint32_t& nodeIndex,
                              uint32_t& voxelIndex) {
    // Not used in minimal implementation
}

VkDescriptorSetLayout GPUOctree::createDescriptorSetLayout() const {
    return VK_NULL_HANDLE;
}

void GPUOctree::updateDescriptorSet(VkDescriptorSet descriptorSet) const {
    // Not implemented
}

} // namespace rendering