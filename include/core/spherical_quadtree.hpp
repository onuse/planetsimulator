#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <atomic>
#include "density_field.hpp"

namespace core {

// Forward declarations
class SphericalQuadtreeNode;
class Camera;

// Represents a single patch on the sphere surface
struct QuadtreePatch {
    glm::dvec3 center;          // Center position on sphere (double precision)
    glm::dvec3 corners[4];      // Corner positions (double precision)
    glm::dvec3 minBounds;       // Exact min bounds in cube space (double precision)
    glm::dvec3 maxBounds;       // Exact max bounds in cube space (double precision)
    float size;                 // Angular size (radians)
    uint32_t level;            // LOD level (0 = root)
    uint32_t faceId;           // Which cube face (0-5)
    float morphFactor;         // For smooth LOD transitions (0-1)
    float screenSpaceError;    // Current screen space error
    
    // Neighbor information for crack prevention
    SphericalQuadtreeNode* neighbors[4] = {nullptr, nullptr, nullptr, nullptr};
    uint32_t neighborLevels[4] = {0, 0, 0, 0};
    
    // Rendering data
    bool isVisible = false;
    bool needsUpdate = true;
    
    // CPU-generated vertex data (for vertex buffer upload)
    struct VertexBufferData {
        bool isGenerated = false;
        size_t vertexCount = 0;
        size_t indexCount = 0;
        size_t vertexBufferOffset = 0;  // Offset into global vertex buffer
        size_t indexBufferOffset = 0;   // Offset into global index buffer
        uint32_t meshGeneration = 0;    // Generation counter for cache invalidation
    } vertexData;
    
    // Transform matrix for this patch
    glm::dmat4 patchTransform;
};

// Node in the spherical quadtree
class SphericalQuadtreeNode {
public:
    enum Face {
        FACE_POS_X = 0,
        FACE_NEG_X = 1,
        FACE_POS_Y = 2,
        FACE_NEG_Y = 3,
        FACE_POS_Z = 4,
        FACE_NEG_Z = 5
    };
    
    enum Edge {
        EDGE_TOP = 0,
        EDGE_RIGHT = 1,
        EDGE_BOTTOM = 2,
        EDGE_LEFT = 3
    };
    
    SphericalQuadtreeNode(const glm::dvec3& center, double size, uint32_t level, 
                         Face face, SphericalQuadtreeNode* parent = nullptr);
    ~SphericalQuadtreeNode() = default;
    
    // Subdivision
    void subdivide(const DensityField& densityField);
    void merge();
    bool isLeaf() const { return children[0] == nullptr; }
    bool hasChildren() const { return !isLeaf(); }
    
    // LOD selection
    float calculateScreenSpaceError(const glm::vec3& viewPos, const glm::mat4& viewProj) const;
    void selectLOD(const glm::vec3& viewPos, const glm::mat4& viewProj, 
                  float errorThreshold, uint32_t maxLevel,
                  std::vector<QuadtreePatch>& visiblePatches,
                  bool enableBackfaceCulling = true);
    
    // Morphing
    void updateMorphFactor(float targetError, float morphRegion);
    
    // Neighbor management
    void setNeighbor(Edge edge, SphericalQuadtreeNode* neighbor);
    SphericalQuadtreeNode* getNeighbor(Edge edge) const { return neighbors[edge]; }
    void updateNeighborReferences();
    
    // Data access
    const QuadtreePatch& getPatch() const { return patch; }
    QuadtreePatch& getPatch() { return patch; }
    uint32_t getLevel() const { return level; }
    Face getFace() const { return face; }
    
    // Height sampling
    void sampleHeights(const DensityField& densityField, uint32_t resolution);
    const std::vector<float>& getHeights() const { return heights; }
    
    // Friend class for tree traversal
    friend class SphericalQuadtree;
    
    // Children access for traversal
    std::array<std::unique_ptr<SphericalQuadtreeNode>, 4> children;
    
private:
    // Tree structure
    SphericalQuadtreeNode* parent;
    std::array<SphericalQuadtreeNode*, 4> neighbors;
    
    // Node data
    QuadtreePatch patch;
    uint32_t level;
    Face face;
    
    // Height data (cached)
    std::vector<float> heights;
    uint32_t heightResolution = 0;
    
    // Helper functions
    glm::dvec3 cubeToSphere(const glm::dvec3& cubePos, double radius) const;
    glm::dvec3 getChildCenter(int childIndex) const;
    void fixTJunctions();
};

// Main spherical quadtree for planet surface
class SphericalQuadtree {
public:
    struct Config {
        float planetRadius = 6371000.0f;
        uint32_t maxLevel = 10;  // Conservative limit to prevent excessive subdivision
        float pixelError = 2.0f;        // Maximum pixel error
        float morphRegion = 0.3f;        // Size of morph region (0-1)
        size_t maxNodes = 10000;         // Maximum active nodes
        bool enableMorphing = true;      // Enable vertex morphing
        bool enableCrackFixes = true;    // Enable T-junction prevention
        bool enableFaceCulling = false;   // DISABLED for testing - was causing missing faces
        bool enableBackfaceCulling = false; // DISABLED - was culling 92% of patches, causing sparse rendering
        bool enableFrustumCulling = true; // Toggle view frustum culling
        bool enableDistanceCulling = true; // Toggle distance-based culling
    };
    
    SphericalQuadtree(const Config& config, std::shared_ptr<DensityField> densityField);
    ~SphericalQuadtree() = default;
    
    // Main update function
    void update(const glm::vec3& viewPos, const glm::mat4& viewProj, float deltaTime);
    
    // Get visible patches for rendering
    const std::vector<QuadtreePatch>& getVisiblePatches() const { return visiblePatches; }
    
    // Configuration
    Config& getConfig() { return config; }
    const Config& getConfig() const { return config; }
    
    // Statistics
    struct Stats {
        uint32_t visibleNodes = 0;
        uint32_t totalNodes = 0;
        uint32_t subdivisions = 0;
        uint32_t merges = 0;
        float lodSelectionTime = 0.0f;
        float morphUpdateTime = 0.0f;
    };
    
    const Stats& getStats() const { return stats; }
    
    // Integration with octree system
    bool shouldUseOctree(float altitude) const;
    float getTransitionBlendFactor(float altitude) const;
    
private:
    Config config;
    std::shared_ptr<DensityField> densityField;
    
    // Six root nodes (cube faces)
    std::array<std::unique_ptr<SphericalQuadtreeNode>, 6> roots;
    
    // Visible patches for current frame
    std::vector<QuadtreePatch> visiblePatches;
    
    // Statistics
    mutable Stats stats;
    std::atomic<uint32_t> totalNodeCount{6};
    
    // LOD selection helpers
    // Initialization
    void initializeRoots();
    glm::mat4 getFaceTransform(SphericalQuadtreeNode::Face face) const;
    
    // LOD management
    void performSubdivisions(SphericalQuadtreeNode* node, const glm::vec3& viewPos,
                            const glm::mat4& viewProj, float errorThreshold, uint32_t maxLevel);
    void performSubdivisionsForFace(SphericalQuadtreeNode* node, const glm::vec3& viewPos,
                                    const glm::mat4& viewProj, float errorThreshold, 
                                    uint32_t maxLevel, int maxSubdivisions);
    void performSubdivisionsRecursive(SphericalQuadtreeNode* node, const glm::vec3& viewPos,
                                      const glm::mat4& viewProj, float errorThreshold,
                                      uint32_t maxLevel, int& subdivisionCount, int maxSubdivisions);
    void performMerges(const glm::vec3& viewPos, const glm::mat4& viewProj, float errorThreshold);
    void performMergesRecursive(SphericalQuadtreeNode* node, const glm::vec3& viewPos,
                                const glm::mat4& viewProj, float mergeThreshold);
    float calculateErrorThreshold(const glm::vec3& viewPos) const;
    void preventCracks(std::vector<QuadtreePatch>& patches);
    void updateMorphFactors(std::vector<QuadtreePatch>& patches, float deltaTime);
};

} // namespace core