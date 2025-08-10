#include "core/density_field.hpp"
#include <random>
#include <numeric>

namespace core {

// Simplex noise gradients
static const glm::vec3 grad3[] = {
    glm::vec3(1, 1, 0), glm::vec3(-1, 1, 0), glm::vec3(1, -1, 0), glm::vec3(-1, -1, 0),
    glm::vec3(1, 0, 1), glm::vec3(-1, 0, 1), glm::vec3(1, 0, -1), glm::vec3(-1, 0, -1),
    glm::vec3(0, 1, 1), glm::vec3(0, -1, 1), glm::vec3(0, 1, -1), glm::vec3(0, -1, -1)
};

DensityField::DensityField(float planetRadius, uint32_t seed) 
    : planetRadius(planetRadius), seed(seed) {
    initPermutationTable();
}

void DensityField::setSeed(uint32_t newSeed) {
    seed = newSeed;
    initPermutationTable();
}

void DensityField::initPermutationTable() {
    std::vector<uint8_t> p(256);
    std::iota(p.begin(), p.end(), 0);
    
    std::mt19937 gen(seed);
    std::shuffle(p.begin(), p.end(), gen);
    
    for (int i = 0; i < 256; i++) {
        perm[i] = p[i];
        perm[256 + i] = p[i];
    }
}

float DensityField::simplexNoise3D(const glm::vec3& pos) const {
    // Simplified 3D simplex noise implementation
    float n0, n1, n2, n3;
    
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;
    
    float s = (pos.x + pos.y + pos.z) * F3;
    int i = static_cast<int>(std::floor(pos.x + s));
    int j = static_cast<int>(std::floor(pos.y + s));
    int k = static_cast<int>(std::floor(pos.z + s));
    
    float t = (i + j + k) * G3;
    glm::vec3 origin = glm::vec3(i - t, j - t, k - t);
    glm::vec3 offset = pos - origin;
    
    glm::ivec3 order;
    if (offset.x >= offset.y) {
        if (offset.y >= offset.z) {
            order = glm::ivec3(1, 0, 0);
        } else if (offset.x >= offset.z) {
            order = glm::ivec3(1, 0, 0);
        } else {
            order = glm::ivec3(0, 0, 1);
        }
    } else {
        if (offset.y < offset.z) {
            order = glm::ivec3(0, 0, 1);
        } else if (offset.x < offset.z) {
            order = glm::ivec3(0, 1, 0);
        } else {
            order = glm::ivec3(0, 1, 0);
        }
    }
    
    glm::vec3 g1 = offset - glm::vec3(order) + G3;
    glm::vec3 g2 = offset - 0.5f + 2.0f * G3;
    glm::vec3 g3 = offset - 1.0f + 3.0f * G3;
    
    int ii = i & 255;
    int jj = j & 255;
    int kk = k & 255;
    
    int gi0 = perm[ii + perm[jj + perm[kk]]] % 12;
    int gi1 = perm[ii + order.x + perm[jj + order.y + perm[kk + order.z]]] % 12;
    int gi2 = perm[ii + (offset.x >= 0.5f ? 1 : 0) + perm[jj + (offset.y >= 0.5f ? 1 : 0) + perm[kk + (offset.z >= 0.5f ? 1 : 0)]]] % 12;
    int gi3 = perm[ii + 1 + perm[jj + 1 + perm[kk + 1]]] % 12;
    
    float t0 = 0.6f - glm::dot(offset, offset);
    if (t0 < 0) {
        n0 = 0.0f;
    } else {
        t0 *= t0;
        n0 = t0 * t0 * glm::dot(grad3[gi0], offset);
    }
    
    float t1 = 0.6f - glm::dot(g1, g1);
    if (t1 < 0) {
        n1 = 0.0f;
    } else {
        t1 *= t1;
        n1 = t1 * t1 * glm::dot(grad3[gi1], g1);
    }
    
    float t2 = 0.6f - glm::dot(g2, g2);
    if (t2 < 0) {
        n2 = 0.0f;
    } else {
        t2 *= t2;
        n2 = t2 * t2 * glm::dot(grad3[gi2], g2);
    }
    
    float t3 = 0.6f - glm::dot(g3, g3);
    if (t3 < 0) {
        n3 = 0.0f;
    } else {
        t3 *= t3;
        n3 = t3 * t3 * glm::dot(grad3[gi3], g3);
    }
    
    return 32.0f * (n0 + n1 + n2 + n3);
}

float DensityField::fbmNoise(const glm::vec3& pos, int octaves, float frequency, float amplitude, float lacunarity, float gain) const {
    float value = 0.0f;
    float amp = amplitude;
    float freq = frequency;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += simplexNoise3D(pos * freq) * amp;
        maxValue += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    
    return value / maxValue;
}

float DensityField::getTerrainHeight(const glm::vec3& sphereNormal) const {
    // Multi-octave terrain generation
    glm::vec3 samplePos = sphereNormal * planetRadius;
    
    // Continental shelf generation
    float continent = fbmNoise(samplePos, 4, terrainParams.continentScale, 
                               terrainParams.continentAmplitude, 2.1f, 0.45f);
    
    // Mountain ranges
    float mountains = fbmNoise(samplePos, 3, terrainParams.mountainScale,
                              terrainParams.mountainAmplitude, 2.3f, 0.4f);
    
    // Mask mountains based on continental height
    float mountainMask = smoothstep(-200.0f, 500.0f, continent);
    mountains *= mountainMask;
    
    // Small scale details
    float details = fbmNoise(samplePos, 2, terrainParams.detailScale,
                            terrainParams.detailAmplitude, 2.0f, 0.5f);
    
    // Ocean floor modification
    float height = continent + mountains * 0.7f + details * 0.3f;
    if (height < terrainParams.seaLevel) {
        // Deeper ocean floor
        float oceanDepth = (terrainParams.seaLevel - height) / terrainParams.continentAmplitude;
        height = terrainParams.seaLevel + terrainParams.oceanDepth * oceanDepth;
    }
    
    return height;
}

float DensityField::getDensity(const glm::vec3& worldPos) const {
    float radius = glm::length(worldPos);
    
    // Early exit for positions far from surface
    if (std::abs(radius - planetRadius) > 10000.0f) {
        return radius - planetRadius;
    }
    
    glm::vec3 sphereNormal = worldPos / radius;
    float terrainHeight = getTerrainHeight(sphereNormal);
    float targetRadius = planetRadius + terrainHeight;
    
    // Signed distance: negative inside planet, positive outside
    return radius - targetRadius;
}

glm::vec3 DensityField::getGradient(const glm::vec3& worldPos, float epsilon) const {
    // Central difference gradient calculation
    float dx = getDensity(worldPos + glm::vec3(epsilon, 0, 0)) - 
               getDensity(worldPos - glm::vec3(epsilon, 0, 0));
    float dy = getDensity(worldPos + glm::vec3(0, epsilon, 0)) - 
               getDensity(worldPos - glm::vec3(0, epsilon, 0));
    float dz = getDensity(worldPos + glm::vec3(0, 0, epsilon)) - 
               getDensity(worldPos - glm::vec3(0, 0, epsilon));
    
    return glm::normalize(glm::vec3(dx, dy, dz) / (2.0f * epsilon));
}

uint8_t DensityField::getMaterialAt(const glm::vec3& worldPos) const {
    float density = getDensity(worldPos);
    
    if (density > 0) {
        return 0; // Air
    }
    
    float radius = glm::length(worldPos);
    glm::vec3 sphereNormal = worldPos / radius;
    float terrainHeight = getTerrainHeight(sphereNormal);
    
    // Material based on depth and height
    if (terrainHeight < terrainParams.seaLevel - 100.0f) {
        return 5; // Sediment (ocean floor)
    } else if (terrainHeight < terrainParams.seaLevel) {
        return 2; // Water
    } else if (terrainHeight > 2000.0f) {
        return 4; // Ice (mountain peaks)
    } else {
        return 1; // Rock
    }
}

float DensityField::getMaterialWeight(const glm::vec3& worldPos, uint8_t material) const {
    uint8_t actualMaterial = getMaterialAt(worldPos);
    return (actualMaterial == material) ? 1.0f : 0.0f;
}

} // namespace core