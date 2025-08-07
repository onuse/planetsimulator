#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "core/octree.hpp"

namespace rendering {

// GPU-friendly octree node structure (must match shader layout)
struct GPUOctreeNode {
    glm::vec4 centerAndSize;     // xyz = center, w = halfSize
    glm::uvec4 childrenAndFlags;  // x = children offset (or 0xFFFFFFFF for leaf)
                                  // y = voxel data offset (for leaves)
                                  // z = flags (bit 0 = isLeaf, bits 8-15 = material)
                                  // w = padding/reserved
};

// Voxel data for GPU
struct GPUVoxelData {
    glm::vec4 colorAndDensity;    // rgb = color, a = density
    glm::vec4 tempAndVelocity;    // x = temperature, yzw = velocity
};

class GPUOctree {
public:
    GPUOctree(VkDevice device, VkPhysicalDevice physicalDevice);
    ~GPUOctree();
    
    // Upload octree from CPU to GPU
    // Now takes viewPos and viewProj to use prepareRenderData for proper filtering
    void uploadOctree(octree::OctreePlanet* planet, 
                     const glm::vec3& viewPos,
                     const glm::mat4& viewProj,
                     VkCommandPool commandPool, 
                     VkQueue queue);
    
    // Get buffer for binding to shaders
    VkBuffer getNodeBuffer() const { return nodeBuffer; }
    VkBuffer getVoxelBuffer() const { return voxelBuffer; }
    uint32_t getNodeCount() const { return nodeCount; }
    uint32_t getRootNodeIndex() const { return 0; } // Root is always at index 0
    
    // Create descriptor set layout for octree buffers
    VkDescriptorSetLayout createDescriptorSetLayout() const;
    
    // Update descriptor sets with octree buffers
    void updateDescriptorSet(VkDescriptorSet descriptorSet) const;
    
private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    
    // GPU buffers
    VkBuffer nodeBuffer = VK_NULL_HANDLE;
    VkDeviceMemory nodeBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelBufferMemory = VK_NULL_HANDLE;
    
    // Staging buffers for upload
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    
    uint32_t nodeCount = 0;
    uint32_t voxelCount = 0;
    float planetRadius = 6371000.0f; // Default Earth radius
    
    // Helper functions
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
                   VkCommandPool commandPool, VkQueue queue);
    
    // Flatten octree for GPU
    void flattenOctree(const octree::OctreeNode* node,
                       std::vector<GPUOctreeNode>& gpuNodes,
                       std::vector<GPUVoxelData>& gpuVoxels,
                       uint32_t& nodeIndex,
                       uint32_t& voxelIndex);
};

} // namespace rendering