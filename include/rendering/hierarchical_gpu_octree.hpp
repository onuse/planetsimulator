#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_set>
#include "core/octree.hpp"

namespace rendering {

// Hierarchical GPU Octree with proper spatial culling and LOD
class HierarchicalGPUOctree {
public:
    // GPU node structure that preserves hierarchy
    struct GPUNode {
        glm::vec4 centerAndSize;      // xyz = center, w = halfSize
        glm::uvec4 childrenAndFlags;  // x = first child index (or 0xFFFFFFFF)
                                      // y = voxel data offset (for leaves)
                                      // z = flags (bit 0 = isLeaf, bits 8-15 = material)
                                      // w = LOD level
        glm::vec4 boundsMin;          // AABB for frustum culling
        glm::vec4 boundsMax;
    };
    
    // Voxel data remains the same
    struct GPUVoxelData {
        glm::vec4 colorAndDensity;
        glm::vec4 tempAndVelocity;
    };
    
    // Visibility info for current frame
    struct VisibilityInfo {
        std::vector<uint32_t> visibleNodes;  // Indices of visible nodes
        std::vector<uint32_t> lodLevels;     // LOD level for each visible node
        uint32_t totalNodes;
        uint32_t culledNodes;
        uint32_t lodTransitions;
    };
    
    HierarchicalGPUOctree(VkDevice device, VkPhysicalDevice physicalDevice);
    ~HierarchicalGPUOctree();
    
    // Upload with frustum culling and LOD
    void uploadOctree(const octree::OctreePlanet* planet, 
                     const glm::mat4& viewProj,
                     const glm::vec3& viewPos,
                     VkCommandPool commandPool, 
                     VkQueue queue);
    
    // Get visibility info for current frame
    const VisibilityInfo& getVisibilityInfo() const { return visibilityInfo; }
    
    // Buffer access
    VkBuffer getNodeBuffer() const { return nodeBuffer; }
    VkBuffer getVoxelBuffer() const { return voxelBuffer; }
    VkBuffer getVisibilityBuffer() const { return visibilityBuffer; }
    uint32_t getVisibleNodeCount() const { return visibilityInfo.visibleNodes.size(); }
    
private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    
    // GPU buffers
    VkBuffer nodeBuffer = VK_NULL_HANDLE;
    VkDeviceMemory nodeBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelBufferMemory = VK_NULL_HANDLE;
    VkBuffer visibilityBuffer = VK_NULL_HANDLE;  // Indices of visible nodes
    VkDeviceMemory visibilityBufferMemory = VK_NULL_HANDLE;
    
    VisibilityInfo visibilityInfo;
    float planetRadius = 6371000.0f;
    
    // Frustum culling
    struct Frustum {
        glm::vec4 planes[6];  // Left, Right, Bottom, Top, Near, Far
        
        void extractFromMatrix(const glm::mat4& viewProj);
        bool intersectsAABB(const glm::vec3& min, const glm::vec3& max) const;
        bool intersectsSphere(const glm::vec3& center, float radius) const;
    };
    
    // LOD selection
    float calculateScreenSpaceError(const octree::OctreeNode* node,
                                   const glm::mat4& viewProj,
                                   const glm::vec3& viewPos) const;
    
    uint32_t selectLOD(const octree::OctreeNode* node,
                      const glm::vec3& viewPos,
                      float qualityFactor = 1.0f) const;
    
    // Hierarchical traversal with culling
    void traverseWithCulling(const octree::OctreeNode* node,
                            const Frustum& frustum,
                            const glm::vec3& viewPos,
                            std::vector<GPUNode>& gpuNodes,
                            std::vector<GPUVoxelData>& gpuVoxels,
                            uint32_t& nodeIndex,
                            uint32_t& voxelIndex,
                            uint32_t parentIndex = 0xFFFFFFFF);
    
    // Helper functions
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer& buffer,
                     VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
                   VkCommandPool commandPool, VkQueue queue);
};

} // namespace rendering