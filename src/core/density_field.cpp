#include "core/density_field.hpp"
#include <random>
#include <numeric>
#include <algorithm>
#include <vector>

namespace core {

// The 12 edge-midpoint gradients of a cube, the standard simplex gradient set.
static const glm::vec3 grad3[12] = {
    glm::vec3( 1,  1,  0), glm::vec3(-1,  1,  0), glm::vec3( 1, -1,  0), glm::vec3(-1, -1,  0),
    glm::vec3( 1,  0,  1), glm::vec3(-1,  0,  1), glm::vec3( 1,  0, -1), glm::vec3(-1,  0, -1),
    glm::vec3( 0,  1,  1), glm::vec3( 0, -1,  1), glm::vec3( 0,  1, -1), glm::vec3( 0, -1, -1)
};

static inline int fastFloor(float x) {
    const int xi = static_cast<int>(x);
    return x < static_cast<float>(xi) ? xi - 1 : xi;
}

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

// 3D simplex noise, following Gustavson's reference formulation.
//
// The previous implementation picked simplex corners with branches that
// returned the same offset for different orderings, and synthesised the third
// corner from a 0.5 threshold rather than the traversal order. It was not
// simplex noise and produced visible axis-aligned structure.
float DensityField::simplexNoise3D(const glm::vec3& pos) const {
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;

    // Skew the input space to decide which simplex cell we are in
    const float s = (pos.x + pos.y + pos.z) * F3;
    const int i = fastFloor(pos.x + s);
    const int j = fastFloor(pos.y + s);
    const int k = fastFloor(pos.z + s);

    // Unskew the cell origin back to (x, y, z) space
    const float t = static_cast<float>(i + j + k) * G3;
    const float x0 = pos.x - (static_cast<float>(i) - t);
    const float y0 = pos.y - (static_cast<float>(j) - t);
    const float z0 = pos.z - (static_cast<float>(k) - t);

    // Determine which of the six tetrahedra we are in, i.e. the traversal
    // order of the coordinates from largest to smallest.
    int i1, j1, k1;  // offsets for the second corner
    int i2, j2, k2;  // offsets for the third corner
    if (x0 >= y0) {
        if (y0 >= z0)      { i1 = 1; j1 = 0; k1 = 0;  i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0;  i2 = 1; j2 = 0; k2 = 1; }
        else               { i1 = 0; j1 = 0; k1 = 1;  i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0)       { i1 = 0; j1 = 0; k1 = 1;  i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0)  { i1 = 0; j1 = 1; k1 = 0;  i2 = 0; j2 = 1; k2 = 1; }
        else               { i1 = 0; j1 = 1; k1 = 0;  i2 = 1; j2 = 1; k2 = 0; }
    }

    const glm::vec3 c0(x0, y0, z0);
    const glm::vec3 c1(x0 - static_cast<float>(i1) + G3,
                       y0 - static_cast<float>(j1) + G3,
                       z0 - static_cast<float>(k1) + G3);
    const glm::vec3 c2(x0 - static_cast<float>(i2) + 2.0f * G3,
                       y0 - static_cast<float>(j2) + 2.0f * G3,
                       z0 - static_cast<float>(k2) + 2.0f * G3);
    const glm::vec3 c3(x0 - 1.0f + 3.0f * G3,
                       y0 - 1.0f + 3.0f * G3,
                       z0 - 1.0f + 3.0f * G3);

    const int ii = i & 255;
    const int jj = j & 255;
    const int kk = k & 255;

    const int gi0 = perm[ii +      perm[jj +      perm[kk     ]]] % 12;
    const int gi1 = perm[ii + i1 + perm[jj + j1 + perm[kk + k1]]] % 12;
    const int gi2 = perm[ii + i2 + perm[jj + j2 + perm[kk + k2]]] % 12;
    const int gi3 = perm[ii + 1  + perm[jj + 1  + perm[kk + 1 ]]] % 12;

    // Radially symmetric attenuation around each corner
    auto contribution = [](const glm::vec3& c, const glm::vec3& g) -> float {
        float t = 0.6f - glm::dot(c, c);
        if (t < 0.0f) return 0.0f;
        t *= t;
        return t * t * glm::dot(g, c);
    };

    const float n = contribution(c0, grad3[gi0])
                  + contribution(c1, grad3[gi1])
                  + contribution(c2, grad3[gi2])
                  + contribution(c3, grad3[gi3]);

    // Scale to approximately [-1, 1]
    return 32.0f * n;
}

float DensityField::fbmNoise(const glm::vec3& pos, int octaves, float frequency,
                             float amplitude, float lacunarity, float gain) const {
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

    return maxValue > 0.0f ? value / maxValue : 0.0f;
}

float DensityField::getMaxRelief() const {
    return getMaxElevation() + getMaxOceanDepth();
}

float DensityField::getMaxElevation() const {
    const TerrainParams& tp = terrainParams;
    const float fraction = std::abs(tp.continentAmplitude)
                         + std::abs(tp.mountainAmplitude)
                         + std::abs(tp.detailAmplitude);
    return fraction * planetRadius * tp.reliefExaggeration;
}

float DensityField::getMaxOceanDepth() const {
    const TerrainParams& tp = terrainParams;
    const float fraction = std::abs(tp.oceanDepth) + std::abs(tp.detailAmplitude);
    return fraction * planetRadius * tp.reliefExaggeration;
}

float DensityField::getSeaLevelHeight() const {
    return terrainParams.seaLevel * planetRadius * terrainParams.reliefExaggeration;
}

float DensityField::getSnowLineElevation(const glm::vec3& sphereNormal) const {
    const TerrainParams& tp = terrainParams;
    const glm::vec3 n = glm::normalize(sphereNormal);

    // |y| is |sin(latitude)|. Squaring keeps the snow line high across the
    // tropics and drops it sharply only at high latitude.
    const float lat = std::abs(n.y);
    const float polar = lat * lat;

    const float fraction = glm::mix(tp.snowLineEquator, tp.snowLinePolar, polar);
    return getMaxElevation() * fraction;
}

float DensityField::getSeaIceCoverage(const glm::vec3& sphereNormal) const {
    const TerrainParams& tp = terrainParams;
    const glm::vec3 n = glm::normalize(sphereNormal);

    // Perturbing the latitude before thresholding gives a ragged ice margin
    // instead of a perfect circle of latitude.
    const float wobble = simplexNoise3D(n * 5.0f + glm::vec3(11.3f, 4.7f, 88.1f)) * 0.04f;
    const float lat = std::abs(n.y) + wobble;

    return smoothstep(tp.seaIceLatitude - tp.seaIceMargin,
                      tp.seaIceLatitude + tp.seaIceMargin, lat);
}

bool DensityField::isSeaIce(const glm::vec3& sphereNormal) const {
    return getSeaIceCoverage(sphereNormal) > 0.5f;
}

float DensityField::getTerrainHeight(const glm::vec3& sphereNormal) const {
    const TerrainParams& tp = terrainParams;

    // Sample on the unit sphere. Frequencies are cycles per radius, so the
    // feature scale is tied to the planet rather than to absolute metres.
    const glm::vec3 n = glm::normalize(sphereNormal);

    // Offsets decorrelate the layers; without them every octave lines up.
    // Few octaves and a low gain keep continents coherent - piling on octaves
    // here is what turns landmasses into speckle.
    const float continent = fbmNoise(n * tp.continentFrequency, 4, 1.0f, 1.0f, 2.0f, 0.42f);
    const float mountainNoise =
        fbmNoise(n * tp.mountainFrequency + glm::vec3(31.416f, 12.72f, 57.31f), 4, 1.0f, 1.0f);
    const float detail =
        fbmNoise(n * tp.detailFrequency + glm::vec3(71.7f, 93.2f, 19.4f), 3, 1.0f, 1.0f);

    // Land mask. Thresholding the continent noise instead of using it directly
    // as a height is what makes the planet bimodal - shelf or abyss with a
    // coastline between - rather than one continuous slope from deep to high.
    const float land = smoothstep(tp.coastLow, tp.coastHigh, continent);

    // Ocean basins sit well below the continental plateau.
    float height = glm::mix(tp.oceanDepth, tp.continentAmplitude, land);

    // Ridged multifractal for mountains: folding at zero turns smooth noise
    // into ridge lines. FBM output clusters near zero, so 1-|n| sits near 1
    // almost everywhere - without the threshold below, every piece of land
    // ends up at mountain height. Thresholding keeps only the ridge crests.
    // Squaring the land mask too keeps ranges inland rather than fringing
    // every coast.
    float ridges = 1.0f - std::abs(mountainNoise);
    ridges = smoothstep(tp.ridgeThreshold, 1.0f, ridges);
    ridges *= ridges;
    height += ridges * land * land * tp.mountainAmplitude;

    // Fine detail, damped over water so ocean surfaces stay smooth.
    height += detail * tp.detailAmplitude * (0.2f + 0.8f * land);

    return (tp.seaLevel + height) * planetRadius * tp.reliefExaggeration;
}

float DensityField::getDensity(const glm::vec3& worldPos) const {
    const float radius = glm::length(worldPos);
    if (radius < 1e-6f) {
        return -planetRadius;  // centre of the planet, deep inside
    }

    // Far from the shell that terrain can occupy, the distance to the terrain
    // surface is within max relief of the distance to the reference sphere,
    // so skip the noise entirely.
    const float band = getMaxRelief() * 1.5f + 1.0f;
    if (std::abs(radius - planetRadius) > band) {
        return radius - planetRadius;
    }

    const glm::vec3 sphereNormal = worldPos / radius;
    const float targetRadius = planetRadius + getTerrainHeight(sphereNormal);

    // Negative inside the planet, positive outside.
    return radius - targetRadius;
}

glm::vec3 DensityField::getGradient(const glm::vec3& worldPos, float epsilon) const {
    const float dx = getDensity(worldPos + glm::vec3(epsilon, 0, 0)) -
                     getDensity(worldPos - glm::vec3(epsilon, 0, 0));
    const float dy = getDensity(worldPos + glm::vec3(0, epsilon, 0)) -
                     getDensity(worldPos - glm::vec3(0, epsilon, 0));
    const float dz = getDensity(worldPos + glm::vec3(0, 0, epsilon)) -
                     getDensity(worldPos - glm::vec3(0, 0, epsilon));

    const glm::vec3 g(dx, dy, dz);
    const float len = glm::length(g);
    if (len < 1e-12f) {
        // Degenerate sample, fall back to the radial direction.
        const float r = glm::length(worldPos);
        return r > 1e-6f ? worldPos / r : glm::vec3(0, 1, 0);
    }
    return g / len;
}

uint8_t DensityField::getMaterialAt(const glm::vec3& worldPos) const {
    if (getDensity(worldPos) > 0.0f) {
        return 0; // Air
    }

    const float radius = glm::length(worldPos);
    if (radius < 1e-6f) {
        return 1; // Rock at the core
    }

    const glm::vec3 sphereNormal = worldPos / radius;
    const float terrainHeight = getTerrainHeight(sphereNormal);
    const float seaLevel = getSeaLevelHeight();
    const float elevation = terrainHeight - seaLevel;

    // Normalise against how high land actually goes, not against total relief;
    // ocean depth would otherwise swamp the land bands.
    const float maxElevation = std::max(getMaxElevation(), 1e-6f);
    const float e = elevation / maxElevation;

    if (e < -0.02f) {
        return 5; // Sediment (ocean floor)
    } else if (e < 0.0f) {
        return isSeaIce(sphereNormal) ? 4 : 2; // Sea ice or water
    } else if (elevation > getSnowLineElevation(sphereNormal)) {
        return 4; // Ice and snow
    }
    return 1;     // Rock
}

float DensityField::getMaterialWeight(const glm::vec3& worldPos, uint8_t material) const {
    return (getMaterialAt(worldPos) == material) ? 1.0f : 0.0f;
}

} // namespace core
