#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "core/vertex_patch_system.hpp"
#include "rendering/transvoxel_renderer.hpp"

namespace rendering {

// ============================================================================
// Transvoxel renderer that uses the vertex sharing system
// This eliminates gaps at face boundaries!
// ============================================================================
class VertexSharedTransvoxel {
public:
    VertexSharedTransvoxel(VkDevice device, VkPhysicalDevice physicalDevice, 
                          VkCommandPool commandPool, VkQueue graphicsQueue);
    ~VertexSharedTransvoxel();
    
    // Generate mesh using vertex sharing for a chunk
    void generateSharedMesh(TransvoxelChunk& chunk, int faceId, 
                           glm::dvec2 center, double size);
    
    // Batch generation for multiple chunks
    void generateBatchedMeshes(std::vector<TransvoxelChunk>& chunks);
    
    // Create GPU buffers from shared vertex data
    void uploadToGPU(TransvoxelChunk& chunk, 
                     const std::vector<PlanetRenderer::CachedVertex>& vertices,
                     const std::vector<uint32_t>& indices);
    
    // Get statistics
    struct Stats {
        uint32_t totalChunks = 0;
        uint32_t totalVertices = 0;
        uint32_t sharedVertices = 0;
        float sharingRatio = 0.0f;
        float averageGapSize = 0.0f;
    };
    
    Stats getStats() const { return stats_; }
    void resetStats() { stats_ = Stats(); patchSystem_.resetStats(); }
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkCommandPool commandPool_;
    VkQueue graphicsQueue_;
    
    // Our vertex sharing system
    PlanetRenderer::VertexPatchSystem patchSystem_;
    
    // Statistics
    Stats stats_;
    
    // Helper to create Vulkan buffers
    void createVertexBuffer(TransvoxelChunk& chunk, 
                           const std::vector<Vertex>& vertices);
    void createIndexBuffer(TransvoxelChunk& chunk,
                          const std::vector<uint32_t>& indices);
    
    // Convert CachedVertex to rendering Vertex
    Vertex convertVertex(const PlanetRenderer::CachedVertex& cached);
};

} // namespace rendering