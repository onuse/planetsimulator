#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace core {

// Signed Distance Field foundation for terrain generation
// Provides continuous terrain heights and material distribution
class DensityField {
public:
    DensityField(float planetRadius = 6371000.0f, uint32_t seed = 42);
    ~DensityField() = default;
    
    // Main SDF interface - returns signed distance (negative inside, positive outside)
    float getDensity(const glm::vec3& worldPos) const;
    
    // Get terrain height at a specific point on the sphere surface
    float getTerrainHeight(const glm::vec3& sphereNormal) const;
    
    // Get gradient for normal calculation
    glm::vec3 getGradient(const glm::vec3& worldPos, float epsilon = 1.0f) const;
    
    // Material queries
    uint8_t getMaterialAt(const glm::vec3& worldPos) const;
    float getMaterialWeight(const glm::vec3& worldPos, uint8_t material) const;
    
    // Configuration
    void setSeed(uint32_t seed);
    void setPlanetRadius(float radius) { planetRadius = radius; }
    float getPlanetRadius() const { return planetRadius; }
    
    // Terrain parameters
    struct TerrainParams {
        float continentScale = 0.00005f;      // Large scale features
        float continentAmplitude = 1000.0f;   // Continental shelves
        float mountainScale = 0.0002f;        // Mountain ranges
        float mountainAmplitude = 500.0f;     // Mountain heights
        float detailScale = 0.0008f;          // Small details
        float detailAmplitude = 50.0f;        // Detail heights
        float oceanDepth = -4000.0f;          // Ocean floor depth
        float seaLevel = 0.0f;                // Sea level offset
    };
    
    TerrainParams& getTerrainParams() { return terrainParams; }
    const TerrainParams& getTerrainParams() const { return terrainParams; }
    
private:
    float planetRadius;
    uint32_t seed;
    TerrainParams terrainParams;
    
    // Noise functions
    float simplexNoise3D(const glm::vec3& pos) const;
    float fbmNoise(const glm::vec3& pos, int octaves, float frequency, float amplitude, float lacunarity = 2.0f, float gain = 0.5f) const;
    
    // Permutation tables for noise
    mutable uint8_t perm[512];
    void initPermutationTable();
    
    // Helper functions
    float smoothstep(float edge0, float edge1, float x) const {
        x = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }
};

} // namespace core