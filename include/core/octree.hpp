#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include <functional>
#include "mixed_voxel.hpp"

namespace octree {

// Legacy material types for backward compatibility during migration
enum class MaterialType : uint8_t {
    Air = 0,
    Rock = 1,
    Water = 2,
    Magma = 3,
    Ice = 4,
    Sediment = 5
};

// Use MixedVoxel as our primary voxel type
using Voxel = MixedVoxel;

// Octree node for sparse voxel octree
class OctreeNode {
public:
    // Node can be either a leaf with voxel data or internal with children
    static constexpr int OCTREE_CHILDREN = 8;
    static constexpr int LEAF_VOXELS = 8; // 2x2x2 voxel block at leaf level
    
    OctreeNode(const glm::vec3& center, float halfSize, int level);
    ~OctreeNode() = default;
    
    // Subdivision
    void subdivide();
    bool isLeaf() const { return children[0] == nullptr; }
    
    // Voxel access
    Voxel* getVoxel(const glm::vec3& position);
    void setVoxel(const glm::vec3& position, const Voxel& voxel);
    
    // LOD operations
    void simplify(); // Merge children if similar enough
    bool shouldSubdivide(const glm::vec3& viewPos, float qualityFactor = 1.0f) const;
    
    // Traversal
    void traverse(const std::function<void(OctreeNode*)>& visitor);
    
    // Serialization for GPU
    struct GPUNode {
        glm::vec3 center;
        float halfSize;
        uint32_t childrenIndex; // Index to first child in GPU buffer
        uint32_t voxelIndex;    // Index to voxel data if leaf
        uint32_t level;
        uint32_t flags;         // Various flags (is_leaf, material_type for simplified nodes, etc.)
    };
    
    GPUNode toGPUNode(uint32_t& nodeIndex, uint32_t& voxelIndex) const;
    
    // Getters for GPU octree access
    const glm::vec3& getCenter() const { return center; }
    float getHalfSize() const { return halfSize; }
    const std::array<Voxel, LEAF_VOXELS>& getVoxels() const { return voxels; }
    const std::array<std::unique_ptr<OctreeNode>, OCTREE_CHILDREN>& getChildren() const { return children; }
    
    // Friend class for access
    friend class OctreePlanet;
    
private:
    glm::vec3 center;
    float halfSize;
    int level;
    
    // Either children or voxel data, not both
    std::array<std::unique_ptr<OctreeNode>, OCTREE_CHILDREN> children;
    std::array<Voxel, LEAF_VOXELS> voxels; // Only used in leaf nodes
    
    // Helper to get child index from position
    int getChildIndex(const glm::vec3& position) const;
    glm::vec3 getChildCenter(int index) const;
};

// Main planet class using octree
class OctreePlanet {
public:
    OctreePlanet(float radius, int maxDepth);
    ~OctreePlanet() = default;
    
    // Generation
    void generate(uint32_t seed);
    // Removed: generateTerrain(), generatePlates() - not currently used
    
    // Simulation
    void update(float deltaTime);  // Currently disabled for performance
    // Removed: simulatePhysics(), simulatePlates() - need GPU implementation
    
    // Rendering preparation
    struct RenderData {
        std::vector<OctreeNode::GPUNode> nodes;
        std::vector<Voxel> voxels;
        std::vector<uint32_t> visibleNodes; // Indices of nodes to render
    };
    
    RenderData prepareRenderData(const glm::vec3& viewPos, const glm::mat4& viewProj);
    
    // Access
    Voxel* getVoxel(const glm::vec3& position);
    void setVoxel(const glm::vec3& position, const Voxel& voxel);
    
    // LOD management
    void updateLOD(const glm::vec3& viewPos);
    
    float getRadius() const { return radius; }
    int getMaxDepth() const { return maxDepth; }
    const OctreeNode* getRoot() const { return root.get(); }
    
private:
    float radius;
    int maxDepth;
    std::unique_ptr<OctreeNode> root;
    
    // Plate tectonics data
    struct Plate {
        uint32_t id;
        glm::vec3 velocity;
        float density;
        bool oceanic;
    };
    std::vector<Plate> plates;
    
    // Helper functions  
    void generateTestSphere(OctreeNode* node, int depth);
    // Removed: generateSphere() - functionality in setMaterials()
    bool isInsidePlanet(const glm::vec3& position) const;
    float getDistanceFromSurface(const glm::vec3& position) const;
};

} // namespace octree