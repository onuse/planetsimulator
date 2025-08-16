#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "core/density_field.hpp"
#include "core/spherical_quadtree.hpp"
#include "core/octree.hpp"
#include "transvoxel_renderer.hpp"
#include "cpu_vertex_generator.hpp"

namespace rendering {

// Manages the transition between different LOD systems
class LODManager {
public:
    enum RenderingMode {
        QUADTREE_ONLY,      // > 1km altitude - pure surface patches
        TRANSITION_ZONE,    // 500m - 1km - blend both systems
        OCTREE_TRANSVOXEL   // < 500m - full volumetric with caves
    };
    
    struct Config {
        float quadtreeOnlyAltitude = 1000.0f;    // Above this, use only quadtree
        float transitionStartAltitude = 1000.0f; // Start blending
        float transitionEndAltitude = 500.0f;    // End blending, full octree
        bool enableTransitions = true;            // Enable smooth transitions
        bool debugVisualization = false;          // Show LOD boundaries
    };
    
    LODManager(VkDevice device, VkPhysicalDevice physicalDevice, 
               VkCommandPool commandPool, VkQueue graphicsQueue);
    ~LODManager();
    
    // Initialize with planet data
    void initialize(float planetRadius, uint32_t seed);
    
    // Main update function
    void update(const glm::vec3& cameraPos, const glm::mat4& viewProj, float deltaTime);
    
    // Rendering
    void render(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                const glm::mat4& viewProj);
    
    // Get current rendering mode
    RenderingMode getCurrentMode() const { return currentMode; }
    float getTransitionBlendFactor() const { return transitionBlendFactor; }
    
    // Get quadtree instance buffer for descriptor set binding
    VkBuffer getQuadtreeInstanceBuffer() const { return quadtreeData.instanceBuffer; }
    
    // Check if instance buffer was updated and needs descriptor set update
    bool isBufferUpdateRequired() { 
        bool result = bufferUpdateRequired;
        bufferUpdateRequired = false;  // Reset flag after checking
        return result;
    }
    
    // Access to subsystems
    core::DensityField* getDensityField() { return densityField.get(); }
    core::SphericalQuadtree* getQuadtree() { return quadtree.get(); }
    octree::OctreePlanet* getOctree() { return octreePlanet.get(); }
    
    // Configuration
    Config& getConfig() { return config; }
    const Config& getConfig() const { return config; }
    
    // Statistics
    struct Stats {
        uint32_t quadtreePatches = 0;
        uint32_t octreeChunks = 0;
        float altitude = 0.0f;
        RenderingMode mode = QUADTREE_ONLY;
        float blendFactor = 0.0f;
    };
    
    const Stats& getStats() const { return stats; }
    
private:
    Config config;
    Stats stats;
    
    // Vulkan resources
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    
    // Core systems
    std::shared_ptr<core::DensityField> densityField;
    std::unique_ptr<core::SphericalQuadtree> quadtree;
    std::unique_ptr<octree::OctreePlanet> octreePlanet;
    std::unique_ptr<TransvoxelRenderer> transvoxelRenderer;
    std::unique_ptr<CPUVertexGenerator> vertexGenerator;
    
    // Current state
    RenderingMode currentMode;
    float transitionBlendFactor;
    float currentAltitude;
    
    // Quadtree rendering resources
    struct QuadtreeRenderData {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        uint32_t instanceCount = 0;
    };
    QuadtreeRenderData quadtreeData;
    
    // Octree/Transvoxel rendering data
    std::vector<TransvoxelChunk> octreeChunks;
    
    // Buffer update tracking
    bool bufferUpdateRequired = false;
    
    // Helper functions
    RenderingMode selectRenderingMode(float altitude);
    void updateQuadtreeBuffers_OLD(const std::vector<core::QuadtreePatch>& patches);
    void updateQuadtreeBuffersCPU(const std::vector<core::QuadtreePatch>& patches, const glm::vec3& viewPosition);
    void updateOctreeChunks(const glm::vec3& viewPos);
    void prepareTransitionZone(const glm::vec3& viewPos);
    
    // Buffer management
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer& buffer,
                     VkDeviceMemory& bufferMemory);
    void destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void uploadBufferData(VkBuffer buffer, const void* data, VkDeviceSize size);
};

} // namespace rendering