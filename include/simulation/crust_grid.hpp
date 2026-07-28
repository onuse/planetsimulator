#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace simulation {

// Plate tectonics on a geodesic grid.
//
// This is the simulation state of the planet's crust. Large-scale elevation is
// an OUTPUT of it, not an input: crust thickness and density float on the
// mantle, and Airy isostasy decides what is continent and what is ocean floor.
// Noise appears only in the initial condition, as the primordial thickness
// heterogeneity that decides where the first cratons sit.
//
// Why a surface grid and not the voxel octree: crustal thickness runs from
// 7 km (oceanic) to 70 km (orogens), so resolving it in voxels would need
// kilometre-scale cells over the whole planet - of order 10^7 columns stepped
// every tick. The physics is columnar anyway. Voxels remain the detail and
// local-edit representation, derived from this.
class CrustGrid {
public:
    // Physical constants. These are measured quantities, not tuning knobs.
    struct Constants {
        float mantleDensity      = 3300.0f;   // kg/m^3, asthenosphere
        float continentalDensity = 2750.0f;   // kg/m^3
        float oceanicDensity     = 2950.0f;   // kg/m^3

        float continentalThickness = 35000.0f; // m, typical craton
        float oceanicThickness     =  7000.0f; // m, typical mature seafloor
        float maxCrustThickness    = 70000.0f; // m, Himalayan-scale orogen
        float minCrustThickness    =  5000.0f; // m, hyper-extended margin

        // Half-space cooling: oceanic lithosphere subsides as sqrt(age).
        // Earth's seafloor drops ~3.1 km between the ridge and 80 Ma.
        float thermalSubsidenceRate = 347.0f;  // m per sqrt(My)
        float thermalSubsidenceMaxAge = 80.0f; // My, subsidence plateaus

        // Total ocean water expressed as a global equivalent layer - the depth
        // it would form if spread over a smooth planet. Earth's is ~2.7 km.
        // Sea level is solved from this, so growing continents raise it.
        float oceanWaterGEL = 2700.0f;         // m

        // How much of the surface carries continental crust at t = 0, i.e. how
        // much felsic crust differentiated early. Earth's continental crust
        // covers ~40% of the surface, much of it submerged shelf. This is an
        // initial condition, not a target for the finished planet - tectonics
        // moves it from here.
        float initialContinentalFraction = 0.38f;
    };

    struct Plate {
        glm::vec3 eulerPole{0.0f, 1.0f, 0.0f}; // unit rotation axis
        float angularVelocity = 0.0f;          // radians per million years
        bool oceanic = true;

        // Rotation banked but not yet applied to the grid. A plate creeping a
        // few kilometres per step would otherwise round to the same cell every
        // time and never move at all; instead the motion accumulates until it
        // is worth a cell, then is spent in one go.
        float pendingRotation = 0.0f;          // radians
    };

    struct Cell {
        glm::vec3 position{0.0f};  // unit vector, fixed - the grid is Eulerian
        uint16_t plateId = 0;
        float thickness = 7000.0f; // m
        float density = 2950.0f;   // kg/m^3
        float age = 0.0f;          // My since this crust formed
        float elevation = 0.0f;    // m above the isostatic datum, derived
    };

    // subdivisions: icosphere level. 6 gives 40,962 cells, ~17 km apart on a
    // 1000 km planet. plateCount: number of rigid plates.
    CrustGrid(float planetRadius, uint32_t seed, int subdivisions = 6, int plateCount = 12);

    // Advance the simulation. Deliberately takes a fixed geological timestep;
    // callers accumulate wall-clock time and call this when enough has passed.
    void step(float millionYears);

    // Large-scale elevation at a direction on the sphere, in metres relative
    // to sea level. Resolves down to cell spacing; finer detail is the
    // renderer's business.
    float sampleElevation(const glm::vec3& sphereNormal) const;

    // Nearest cell to a direction, via the spatial accelerator.
    int findNearestCell(const glm::vec3& sphereNormal) const;

    const std::vector<Cell>& getCells() const { return cells; }
    const std::vector<Plate>& getPlates() const { return plates; }

    // Adjacency, as an allocation-free range. Geodesic cells have 5 or 6
    // neighbours - the twelve pentagons are the icosahedron's original corners.
    int neighbourCount(int cell) const {
        return neighbourStart[cell + 1] - neighbourStart[cell];
    }
    int neighbourAt(int cell, int k) const {
        return neighbourIndices[neighbourStart[cell] + k];
    }

    float getPlanetRadius() const { return planetRadius; }
    float getSeaLevel() const { return seaLevel; }
    float getSimulationTime() const { return simulationTime; }

    // Typical distance between neighbouring cells, in metres. This is the
    // finest scale the simulation resolves; anything below it is the
    // renderer's roughness, not simulated relief.
    float cellSpacing() const;

    // Current elevation extremes relative to sea level, cached each step so
    // per-vertex sampling does not have to scan the whole grid.
    float getMinElevation() const { return minElevation; }
    float getMaxElevation() const { return maxElevation; }

    // Bumped on every step, so renderers can tell when to rebuild.
    uint64_t getVersion() const { return version; }

    Constants& getConstants() { return constants; }
    const Constants& getConstants() const { return constants; }

    // Surface velocity of the plate owning this point, in metres per My.
    glm::vec3 plateVelocityAt(const glm::vec3& sphereNormal, uint16_t plateId) const;

    // Mean cell area in m^2, used to turn per-cell sums into volumes.
    float getCellArea() const;

    // Diagnostics
    struct Stats {
        float landFraction = 0.0f;
        float meanElevation = 0.0f;
        float minElevation = 0.0f;
        float maxElevation = 0.0f;
        float meanOceanicAge = 0.0f;
        float crustVolume = 0.0f;   // m^3, should be near-conserved
    };
    Stats computeStats() const;

private:
    float planetRadius;
    uint32_t seed;
    Constants constants;

    std::vector<Cell> cells;
    std::vector<Plate> plates;

    // Flattened adjacency: neighbours of cell i are
    // neighbourIndices[neighbourStart[i] .. neighbourStart[i+1])
    std::vector<int> neighbourIndices;
    std::vector<int> neighbourStart;

    // Latitude/longitude bin accelerator over the fixed cell positions.
    static constexpr int BIN_LAT = 64;
    static constexpr int BIN_LON = 128;
    std::vector<std::vector<int>> bins;

    float seaLevel = 0.0f;
    float simulationTime = 0.0f;
    float minElevation = 0.0f;
    float maxElevation = 0.0f;
    uint64_t version = 0;

    void buildGeodesicGrid(int subdivisions);
    void buildAccelerator();
    void assignPlates(int plateCount);
    void seedInitialCrust();

    void advect(float dt);
    void applyStrain(float dt);
    void diffuseThermalAge(float dt);
    void updateIsostasy();
    void solveSeaLevel();

    // Isostatic elevation of one column above the compensation datum.
    float isostaticHeight(const Cell& cell) const;

    int binIndex(const glm::vec3& n) const;
};

} // namespace simulation
