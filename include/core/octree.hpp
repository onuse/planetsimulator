#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include "mixed_voxel.hpp"
#include "density_field.hpp"
#include "simulation/crust_grid.hpp"

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
    const Voxel* getVoxel(const glm::vec3& position) const;
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
    ~OctreePlanet();   // stops the simulation thread
    
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
    
    // Prepare FULL octree data for GPU (includes entire hierarchy for traversal)
    RenderData prepareFullOctreeData();
    
    // Access
    Voxel* getVoxel(const glm::vec3& position);
    const Voxel* getVoxel(const glm::vec3& position) const;
    void setVoxel(const glm::vec3& position, const Voxel& voxel);
    
    // LOD management
    void updateLOD(const glm::vec3& viewPos);
    
    float getRadius() const { return radius; }
    int getMaxDepth() const { return maxDepth; }
    const OctreeNode* getRoot() const { return root.get(); }

    // The terrain field this planet was built from. Renderers sample the same
    // field so mesh geometry and voxel materials cannot drift apart.
    core::DensityField& getDensityField() { return densityField; }
    const core::DensityField& getDensityField() const { return densityField; }

    // The tectonic simulation driving that field.
    simulation::CrustGrid* getCrustGrid() { return crust.get(); }
    const simulation::CrustGrid* getCrustGrid() const { return crust.get(); }

    // How finely the crust is resolved, as an icosphere subdivision level.
    //
    // Level 6 is 40,962 cells, 17.5 km apart. Level 7 is 163,842 at 8.8 km, and
    // costs about eight times as much per million years - four times the cells,
    // and half the stable timestep, because a plate may not cross more than
    // half a cell in a step. The two compound.
    //
    // Worth it for landscapes and not for tectonics, which is a real choice
    // rather than a better setting: a divide cannot migrate less than one cell,
    // so drainage capture needs the finer grid, while plate motion comes out the
    // same either way. Set before generate().
    void setCrustResolution(int subdivisions) { crustSubdivisions = subdivisions; }
    int getCrustResolution() const { return crustSubdivisions; }

    // Bumped whenever tectonics changes the surface, so renderers know to
    // rebuild their meshes.
    uint64_t getCrustVersion() const;

    // The surface picture the renderer is currently sampling, as shared
    // ownership rather than the bare pointer the density field holds.
    //
    // Anything reading terrain off the render thread needs this. The bare
    // pointer is swapped once a frame when a newer snapshot arrives, and the
    // one it replaces is freed as soon as nothing else holds it - which is
    // fine for a reader that runs between frames and fatal for one that does
    // not. Holding this for the length of a read keeps it alive.
    std::shared_ptr<const simulation::CrustGrid::Snapshot> getRenderSnapshot() const {
        return renderSnapshot;
    }

    // How much geological time we would like per second of wall clock.
    void setSimulationRate(float millionYearsPerSecond) { simulationRate = millionYearsPerSecond; }
    float getSimulationRate() const { return simulationRate; }

    // How much we are actually managing, in My per second of wall clock. The
    // simulation takes whatever timestep plate speed allows, so on a fast
    // planet it can fall behind what was asked for; better to report that than
    // to quietly drop geological time.
    float getAchievedSimulationRate() const { return achievedRate; }

    // Wall-clock time per frame the simulation may spend. Everything left over
    // is carried to the next frame rather than stalling the renderer.
    void setSimulationBudgetMs(float ms) { simulationBudgetMs = ms; }

    // Run exactly this much geological time, as fast as the machine allows,
    // regardless of the requested rate.
    //
    // The rate is a wall-clock pace, which is the right control for watching a
    // planet and the wrong one for measuring it: how much geology happened
    // between two observations then depends on frame rate and on how busy the
    // machine was. Anything of the form "what does this look like two million
    // years from now" needs the amount of time to be the thing that is
    // specified and the wall clock to be whatever it turns out to be.
    //
    // Does not block. The caller keeps rendering and watches advanceComplete(),
    // so the window stays alive through a long run and can still be
    // photographed part way through.
    void requestAdvance(float millionYears);
    bool advanceComplete() const { return advanceRemaining.load() <= 0.0f; }
    float advanceOutstanding() const { return advanceRemaining.load(); }
    
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
    // Removed: sampleImprovedTerrain() - replaced by DensityField, which is
    // 3D noise on the sphere rather than sin/cos in lat/long
    // Removed: generateSphere() - functionality in setMaterials()
    bool isInsidePlanet(const glm::vec3& position) const;
    float getDistanceFromSurface(const glm::vec3& position) const;

    uint32_t seed = 42;
    core::DensityField densityField;

    // Tectonic simulation. Created in generate(), because it needs the seed.
    // Once running it is owned by the simulation thread; the main thread must
    // only touch it through published snapshots.
    std::unique_ptr<simulation::CrustGrid> crust;

    std::thread simulationThread;
    std::atomic<bool> simulationRunning{false};
    std::mutex snapshotMutex;
    std::shared_ptr<const simulation::CrustGrid::Snapshot> publishedSnapshot;
    std::shared_ptr<const simulation::CrustGrid::Snapshot> renderSnapshot;
    std::atomic<float> atomicSimulationRate{1.0f};
    std::atomic<float> atomicAchievedRate{0.0f};

    // Geological time still owed to an explicit request, in My.
    std::atomic<float> advanceRemaining{0.0f};

    void startSimulationThread();
    void stopSimulationThread();

    // Kept for headless use, where stepping inline is simpler than running a
    // thread nobody is watching.
    void updateLegacyInline(float deltaTime);

    // Geological time is stepped in fixed increments; wall-clock time is
    // banked here until a whole step is due.
    int crustSubdivisions = 6;          // icosphere level for the crust grid
    float simulationRate = 1.0f;        // My per second of wall clock, wanted
    float achievedRate = 0.0f;          // My per second of wall clock, actual
    float simulationBudgetMs = 4.0f;    // per frame
    float pendingMillionYears = 0.0f;
    float budgetBankSeconds = 0.0f;
    float lastStepCostSeconds = 0.02f;  // seeded with a guess, measured after
};

} // namespace octree