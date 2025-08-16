#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "vertex_id_system.hpp"
#include "vertex_generator.hpp"

// ============================================================================
// Phase 3: Integration Layer
// Connects the vertex identity/generation system to the rendering pipeline
// ============================================================================

namespace PlanetRenderer {

// ============================================================================
// Patch that uses vertex IDs instead of storing vertices directly
// ============================================================================
struct VertexIDPatch {
    // Patch identification
    int faceId;                  // Which cube face (0-5)
    glm::dvec2 center;          // Center UV on face
    double size;                // Size in UV space
    uint32_t level;             // LOD level
    
    // Vertex data as IDs (not positions!)
    std::vector<VertexID> vertexIDs;    // Grid of vertex IDs
    std::vector<uint32_t> indices;      // Triangle indices (local to vertexIDs array)
    
    // Grid dimensions
    int resolution;             // Grid resolution (vertices per edge - 1)
    
    // Rendering state
    bool isDirty = true;
    bool isVisible = false;
    
    // GPU buffer handles (will be filled during actual GPU upload)
    // For now, just use placeholders
    void* vertexBuffer = nullptr;
    void* vertexBufferMemory = nullptr;
    void* indexBuffer = nullptr;
    void* indexBufferMemory = nullptr;
    
    // Global vertex buffer indices (after resolution)
    std::vector<uint32_t> globalIndices;  // Indices into global vertex buffer
};

// ============================================================================
// System for generating patches with shared vertices
// ============================================================================
class VertexPatchSystem {
public:
    VertexPatchSystem();
    ~VertexPatchSystem() = default;
    
    // Generate a patch using the vertex ID system
    VertexIDPatch generatePatch(int faceId, glm::dvec2 center, double size, 
                                int resolution = 32);
    
    // Convert patch to renderable data
    struct RenderData {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<uint32_t> indices;
    };
    
    RenderData convertToRenderData(const VertexIDPatch& patch);
    
    // Batch conversion for multiple patches
    void convertPatchesToGlobalBuffer(const std::vector<VertexIDPatch>& patches,
                                      std::vector<CachedVertex>& globalVertexBuffer,
                                      std::vector<uint32_t>& globalIndexBuffer);
    
    // Statistics
    struct Stats {
        uint32_t totalPatches = 0;
        uint32_t totalVertices = 0;
        uint32_t sharedVertices = 0;
        float sharingRatio = 0.0f;
    };
    
    Stats getStats() const { return stats_; }
    void resetStats() { stats_ = Stats(); }
    
    // Access to underlying systems
    IVertexGenerator& getGenerator() { return generator_; }
    VertexBufferManager& getBufferManager() { return bufferManager_; }
    
private:
    SimpleVertexGenerator generator_;
    VertexBufferManager bufferManager_;
    Stats stats_;
    
    // Helper to create vertex grid for a patch
    void generateVertexGrid(VertexIDPatch& patch);
    
    // Helper to create triangle indices
    void generateTriangleIndices(VertexIDPatch& patch);
};

// ============================================================================
// Adapter to use with existing spherical quadtree
// ============================================================================

// Simple patch representation for testing (avoiding circular dependency)
struct SimplePatch {
    int faceId;
    uint32_t level;
    bool isVisible;
    glm::vec3 center;
};

class QuadtreePatchAdapter {
public:
    QuadtreePatchAdapter();
    ~QuadtreePatchAdapter() = default;
    
    // Convert a simple patch to use vertex IDs
    VertexIDPatch convertFromSimplePatch(const SimplePatch& oldPatch);
    
    // Generate test patches (simplified for integration testing)
    std::vector<VertexIDPatch> generateTestPatches(int numPatches = 6);
    
    // Get the patch system for direct access
    VertexPatchSystem& getPatchSystem() { return patchSystem_; }
    
private:
    VertexPatchSystem patchSystem_;
    
    // Convert face/UV to cube position
    glm::dvec3 faceUVToCube(int face, double u, double v);
};

} // namespace PlanetRenderer