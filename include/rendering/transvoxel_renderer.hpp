#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <atomic>
#include <cstddef>

#include "core/octree.hpp"
#include "core/camera.hpp"

namespace rendering {

// Vertex structure compatible with existing hierarchical shaders
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texCoord;
    
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
        
        // Position - location 0
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);
        
        // Color - location 1 (matching fragment shader input)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        // Normal - location 2
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);
        
        // TexCoord - location 3
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, texCoord);
        
        return attributeDescriptions;
    }
};

// Transvoxel chunk representation
struct TransvoxelChunk {
    glm::vec3 position;          // World position of chunk
    float voxelSize;             // Size of each voxel at this LOD level
    uint32_t lodLevel;           // Level of detail (0 = highest detail)
    
    // Generated mesh data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Material data from MixedVoxel system
    std::vector<glm::vec3> vertexColors;  // Per-vertex colors from MixedVoxel::getColor()
    
    // Vulkan resources
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    
    // Chunk state
    bool isDirty = true;         // Needs mesh regeneration
    bool hasValidMesh = false;   // Has generated mesh data
};

class TransvoxelRenderer {
public:
    TransvoxelRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);
    ~TransvoxelRenderer();
    
    // Main interface
    void generateMesh(TransvoxelChunk& chunk, const octree::OctreePlanet& planet);
    void render(const std::vector<TransvoxelChunk>& chunks, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
    
    // Mesh management
    void invalidateChunk(const glm::vec3& position);
    void clearCache();
    
    // Performance stats
    uint32_t getTriangleCount() const { return totalTriangles.load(); }
    uint32_t getChunkCount() const { return activeChunks.load(); }
    
private:
    // Vulkan resources
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    
    // Statistics
    std::atomic<uint32_t> totalTriangles{0};
    std::atomic<uint32_t> activeChunks{0};
    
    // Transvoxel lookup tables
    static const uint8_t regularCellClass[256];
    static const uint8_t regularCellData[16][12];
    // regularVertexData table removed - edge detection done algorithmically
    
    // TODO: Add transition cell tables for LOD boundaries
    // static const uint8_t transitionCellClass[512];
    // static const uint8_t transitionCellData[56][13];
    // static const uint16_t transitionVertexData[512][18];
    
    // TODO: Implement transition cells for LOD boundaries
    // void generateTransitionCell(
    //     TransvoxelChunk& chunk,
    //     const octree::OctreePlanet& planet,
    //     const glm::ivec3& cellPos,
    //     uint8_t transitionFace
    // );
    
public:
    // Buffer management - made public for external mesh generation
    void createVertexBuffer(TransvoxelChunk& chunk);
    void createIndexBuffer(TransvoxelChunk& chunk);
    void destroyChunkBuffers(TransvoxelChunk& chunk);
    
private:
    // Helper functions
    void createBuffer(
        VkDeviceSize size, 
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, 
        VkBuffer& buffer, 
        VkDeviceMemory& bufferMemory
    );
    
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} // namespace rendering