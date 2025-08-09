#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include "core/octree.hpp"

namespace algorithms {

// Core mesh vertex structure
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    
    MeshVertex() = default;
    MeshVertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec3& col)
        : position(pos), normal(norm), color(col) {}
};

// Mesh data structure
struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    
    void clear() {
        vertices.clear();
        indices.clear();
    }
    
    bool isEmpty() const {
        return vertices.empty() || indices.empty();
    }
    
    uint32_t getTriangleCount() const {
        return static_cast<uint32_t>(indices.size() / 3);
    }
};

// Parameters for mesh generation
struct MeshGenParams {
    glm::vec3 worldPos;      // World position of region
    float voxelSize;         // Size of each voxel at this LOD
    glm::ivec3 dimensions;   // Number of voxels in each dimension
    uint32_t lodLevel;       // Level of detail (0 = highest)
    
    MeshGenParams(const glm::vec3& pos, float size, const glm::ivec3& dims, uint32_t lod = 0)
        : worldPos(pos), voxelSize(size), dimensions(dims), lodLevel(lod) {}
};

// Simple cube-based mesh generation (for testing)
MeshData generateSimpleCubeMesh(const MeshGenParams& params, const octree::OctreePlanet& planet);

// Transvoxel mesh generation (main algorithm)
MeshData generateTransvoxelMesh(const MeshGenParams& params, const octree::OctreePlanet& planet);

} // namespace algorithms