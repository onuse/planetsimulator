#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "core/spherical_quadtree.hpp"
#include "math/cube_sphere_mapping.hpp"

namespace rendering {

// Vertex structure for CPU-generated vertices
struct PatchVertex {
    glm::vec3 position;      // World space position
    glm::vec3 normal;        // Surface normal
    glm::vec2 texCoord;      // UV coordinates
    float height;            // Terrain height
    uint32_t faceId;         // Face ID for debug coloring
};

// CPU-side vertex generator for quadtree patches
class CPUVertexGenerator {
public:
    struct Config {
        uint32_t gridResolution = 65;  // 65x65 grid per patch (power of 2 + 1)
        float planetRadius = 6371000.0f;
        bool enableSkirts = true;
        float skirtDepth = 500.0f;  // How far down skirts extend
        bool enableVertexCaching = true;
        size_t maxCacheSize = 100000;  // Maximum cached vertices
    };

    explicit CPUVertexGenerator(const Config& config);
    ~CPUVertexGenerator() = default;

    // Generate vertices for a patch
    struct PatchMesh {
        std::vector<PatchVertex> vertices;
        std::vector<uint32_t> indices;
        size_t vertexCount;
        size_t indexCount;
        
        // Vertex offsets for different parts
        size_t mainVertexStart = 0;
        size_t mainVertexCount = 0;
        size_t skirtVertexStart = 0;
        size_t skirtVertexCount = 0;
    };

    // Generate mesh for a quadtree patch
    PatchMesh generatePatchMesh(const core::QuadtreePatch& patch, 
                                const glm::dmat4& patchTransform);

    // Clear vertex cache (call when camera moves significantly)
    void clearCache();

    // Get statistics
    struct Stats {
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t currentCacheSize = 0;
        size_t totalVerticesGenerated = 0;
    };
    
    const Stats& getStats() const { return stats; }

private:
    Config config;
    Stats stats;

    // Vertex cache for shared vertices at boundaries
    struct VertexKey {
        int32_t qx, qy, qz;  // Quantized position
        uint32_t faceId;     // Face ID to prevent cross-face sharing
        
        bool operator==(const VertexKey& other) const {
            return qx == other.qx && qy == other.qy && qz == other.qz && faceId == other.faceId;
        }
    };

    struct VertexKeyHash {
        size_t operator()(const VertexKey& key) const {
            // Pack into 64-bit for hashing, include faceId
            uint64_t packed = ((uint64_t)key.qx & 0xFFFFF) |
                            (((uint64_t)key.qy & 0xFFFFF) << 20) |
                            (((uint64_t)key.qz & 0xFFFFF) << 40) |
                            (((uint64_t)key.faceId & 0x7) << 60);  // 3 bits for faceId (0-5)
            return std::hash<uint64_t>()(packed);
        }
    };

    std::unordered_map<VertexKey, PatchVertex, VertexKeyHash> vertexCache;

    // Helper functions
    PatchVertex generateVertex(const glm::dvec3& cubePos, bool isSkirt, uint32_t faceId);
    VertexKey createVertexKey(const glm::dvec3& cubePos, uint32_t faceId) const;
    float getTerrainHeight(const glm::vec3& sphereNormal) const;
    glm::vec3 calculateNormal(const glm::vec3& sphereNormal) const;
    
    // Noise functions for terrain generation
    float hash(const glm::vec3& p) const;
    float smoothNoise(const glm::vec3& p) const;
    float terrainNoise(const glm::vec3& p, int octaves) const;
};

} // namespace rendering