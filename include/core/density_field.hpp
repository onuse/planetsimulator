#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "simulation/crust_grid.hpp"

namespace core {

// Signed distance field foundation for terrain generation.
//
// This is the single source of truth for planet shape. Both the voxel octree
// and the render mesh sample it, so geometry and materials cannot disagree.
//
// Terrain is 3D noise evaluated on the unit sphere. That matters: a
// latitude/longitude parameterisation pinches at the poles and seams at
// longitude +/-pi, which is what the old sin/cos terrain did.
class DensityField {
public:
    DensityField(float planetRadius = 6371000.0f, uint32_t seed = 42);
    ~DensityField() = default;

    // Signed distance to the terrain surface: negative inside, positive outside.
    float getDensity(const glm::vec3& worldPos) const;

    // Terrain elevation in metres relative to the reference sphere, for a
    // point on the unit sphere. Positive is above sea level.
    float getTerrainHeight(const glm::vec3& sphereNormal) const;

    // Elevation without the sub-grid roughness - the shape the simulation
    // actually resolves. Use this where the fine detail is not observable,
    // notably ocean colour: seafloor roughness is real but you cannot see it
    // through four kilometres of water, and letting it through paints a
    // mottled pattern across every ocean.
    float getLargeScaleElevation(const glm::vec3& sphereNormal) const;

    // Gradient of the density field, for surface normals.
    glm::vec3 getGradient(const glm::vec3& worldPos, float epsilon = 1.0f) const;

    // Material queries
    uint8_t getMaterialAt(const glm::vec3& worldPos) const;
    float getMaterialWeight(const glm::vec3& worldPos, uint8_t material) const;

    // Attach the tectonic simulation. Once set, large-scale elevation comes
    // from simulated crust - thickness, density and age under isostasy - and
    // noise is demoted to sub-grid roughness below the simulation's cell
    // spacing. With no grid attached the field falls back to pure procedural
    // terrain, which is what the standalone tests and tools use.
    void setCrustGrid(const simulation::CrustGrid* grid) { crustGrid = grid; }
    const simulation::CrustGrid* getCrustGrid() const { return crustGrid; }

    // The surface to sample. The simulation runs on another thread, so this is
    // a published snapshot rather than the live grid; whoever owns it keeps it
    // alive for as long as this pointer is set, and swaps it only between
    // frames. Without one, the grid is sampled directly - which is what the
    // single-threaded tests do.
    void setCrustSnapshot(const simulation::CrustGrid::Snapshot* snapshot) {
        crustSnapshot = snapshot;
    }

    // Configuration
    void setSeed(uint32_t seed);
    uint32_t getSeed() const { return seed; }
    void setPlanetRadius(float radius) { planetRadius = radius; }
    float getPlanetRadius() const { return planetRadius; }

    // Terrain parameters.
    //
    // Frequencies are in cycles per planet radius and amplitudes are fractions
    // of planet radius, so the terrain keeps its proportions whatever radius
    // the planet is given. Earth's continental relief is ~1 km on a 6371 km
    // radius (1.6e-4) and its mountains reach ~8.8 km (1.4e-3).
    struct TerrainParams {
        // Low continent frequency is what makes landmasses read as continents
        // rather than as speckle. Roughly, this is how many continent-sized
        // features fit around the planet.
        float continentFrequency = 1.1f;
        float mountainFrequency  = 3.5f;
        float detailFrequency    = 12.0f;

        // Continental plateau height above sea level, and how deep the ocean
        // basins sit below it. Earth is strongly bimodal this way: most of the
        // surface is either shelf or abyssal plain, with little in between.
        // The plateau is deliberately low: most land should be lowland, with
        // mountains as the exception rather than the rule.
        float continentAmplitude = 6.0e-5f;
        float mountainAmplitude  = 1.1e-3f;
        float detailAmplitude    = 5.0e-5f;
        float oceanDepth         = -6.0e-4f;
        float seaLevel           = 0.0f;      // fraction of radius

        // Coastline shaping: the continent noise range that becomes shoreline.
        // A narrow band gives sharp coasts, a wide one gives gentle shelves.
        float coastLow  = 0.02f;
        float coastHigh = 0.20f;

        // Ridged noise sits near its maximum almost everywhere, so mountains
        // have to be cut back to ridge lines or the whole planet becomes
        // alpine. Only the top of the ridge signal survives this threshold.
        float ridgeThreshold = 0.82f;

        // Snow line as a fraction of maximum elevation, at the equator and at
        // the poles. The polar value is what produces ice caps.
        float snowLineEquator = 0.55f;
        float snowLinePolar   = 0.02f;

        // Latitude around which oceans freeze, as |sin(latitude)|, and how
        // wide the transition from open water to pack ice is.
        float seaIceLatitude = 0.88f;
        float seaIceMargin   = 0.07f;

        // Physically accurate relief is almost invisible from orbit - Earth's
        // full range is 0.3% of its radius. 1.0 is real; the default
        // exaggerates so terrain reads at planetary zoom.
        float reliefExaggeration = 8.0f;
    };

    TerrainParams& getTerrainParams() { return terrainParams; }
    const TerrainParams& getTerrainParams() const { return terrainParams; }

    // Largest elevation magnitude the current parameters can produce, in
    // metres. Used to bound searches and early-outs.
    float getMaxRelief() const;

    // Highest land above sea level, and deepest ocean below it, in metres.
    // Shading bands normalise against these rather than against total relief,
    // so land colours do not all collapse into one band.
    float getMaxElevation() const;
    float getMaxOceanDepth() const;

    // Sea level in metres, relative to the reference sphere.
    float getSeaLevelHeight() const;

    // Elevation above sea level, in metres, at which snow lies at this point.
    // Falls towards the poles, which is what forms ice caps. Shared so mesh
    // colour and voxel materials cannot disagree about where snow is.
    float getSnowLineElevation(const glm::vec3& sphereNormal) const;

    // How completely open ocean at this point is frozen over, 0 to 1. The
    // margin is deliberately soft and noise-perturbed; a hard latitude cutoff
    // draws a visible straight line across the pole.
    float getSeaIceCoverage(const glm::vec3& sphereNormal) const;

    // Whether open ocean at this point is frozen over.
    bool isSeaIce(const glm::vec3& sphereNormal) const;

private:
    float planetRadius;
    uint32_t seed;
    TerrainParams terrainParams;
    const simulation::CrustGrid* crustGrid = nullptr;
    const simulation::CrustGrid::Snapshot* crustSnapshot = nullptr;

    // Noise functions
    float simplexNoise3D(const glm::vec3& pos) const;
    float fbmNoise(const glm::vec3& pos, int octaves, float frequency,
                   float amplitude, float lacunarity = 2.0f, float gain = 0.5f) const;

    // Permutation table for noise, duplicated to 512 so index wrapping is free
    uint8_t perm[512];
    void initPermutationTable();

    // Helper functions
    static float smoothstep(float edge0, float edge1, float x) {
        x = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }
};

} // namespace core
