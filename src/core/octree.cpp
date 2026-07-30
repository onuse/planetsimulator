#include "core/octree.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <functional>
#include <iostream>
#include "utils/log.hpp"
#include <queue>
#include <unordered_map>

namespace octree {

// ============================================================================
// OctreeNode Implementation
// ============================================================================

OctreeNode::OctreeNode(const glm::vec3& center, float halfSize, int level)
    : center(center), halfSize(halfSize), level(level) {
    // Initialize as leaf node with air voxels
    for (auto& voxel : voxels) {
        voxel = MixedVoxel::createPure(core::MaterialID::Air);
        voxel.temperature = 10;  // Cold space
        voxel.pressure = 0;      // No pressure
    }
}

void OctreeNode::subdivide() {
    if (!isLeaf()) return; // Already subdivided
    
    float childHalfSize = halfSize * 0.5f;
    
    // Create 8 children
    // Order: -x-y-z, +x-y-z, -x+y-z, +x+y-z, -x-y+z, +x-y+z, -x+y+z, +x+y+z
    for (int i = 0; i < OCTREE_CHILDREN; i++) {
        glm::vec3 childCenter = getChildCenter(i);
        children[i] = std::make_unique<OctreeNode>(childCenter, childHalfSize, level + 1);
        
        // Copy voxel data to appropriate child
        // Each child inherits the voxel data from its corresponding position in parent
        int voxelIndex = i % LEAF_VOXELS;
        if (voxelIndex < LEAF_VOXELS) {
            for (auto& voxel : children[i]->voxels) {
                voxel = voxels[voxelIndex];
            }
        }
    }
}

int OctreeNode::getChildIndex(const glm::vec3& position) const {
    int index = 0;
    if (position.x > center.x) index |= 1;
    if (position.y > center.y) index |= 2;
    if (position.z > center.z) index |= 4;
    return index;
}

glm::vec3 OctreeNode::getChildCenter(int index) const {
    float offset = halfSize * 0.5f;
    return glm::vec3(
        center.x + ((index & 1) ? offset : -offset),
        center.y + ((index & 2) ? offset : -offset),
        center.z + ((index & 4) ? offset : -offset)
    );
}

Voxel* OctreeNode::getVoxel(const glm::vec3& position) {
    if (isLeaf()) {
        // For leaf nodes, determine which voxel in the 2x2x2 block
        glm::vec3 localPos = position - center;
        int voxelIndex = 0;
        if (localPos.x > 0) voxelIndex |= 1;
        if (localPos.y > 0) voxelIndex |= 2;
        if (localPos.z > 0) voxelIndex |= 4;
        
        if (voxelIndex < LEAF_VOXELS) {
            return &voxels[voxelIndex];
        }
        return nullptr;
    } else {
        // Recurse to appropriate child
        int childIndex = getChildIndex(position);
        if (children[childIndex]) {
            return children[childIndex]->getVoxel(position);
        }
        return nullptr;
    }
}

const Voxel* OctreeNode::getVoxel(const glm::vec3& position) const {
    if (isLeaf()) {
        // For leaf nodes, determine which voxel in the 2x2x2 block
        glm::vec3 localPos = position - center;
        int voxelIndex = 0;
        if (localPos.x > 0) voxelIndex |= 1;
        if (localPos.y > 0) voxelIndex |= 2;
        if (localPos.z > 0) voxelIndex |= 4;
        
        if (voxelIndex < LEAF_VOXELS) {
            return &voxels[voxelIndex];
        }
        return nullptr;
    } else {
        // Recurse to appropriate child
        int childIndex = getChildIndex(position);
        if (children[childIndex]) {
            return children[childIndex]->getVoxel(position);
        }
        return nullptr;
    }
}

void OctreeNode::setVoxel(const glm::vec3& position, const Voxel& voxel) {
    if (isLeaf()) {
        // For leaf nodes, determine which voxel in the 2x2x2 block
        glm::vec3 localPos = position - center;
        int voxelIndex = 0;
        if (localPos.x > 0) voxelIndex |= 1;
        if (localPos.y > 0) voxelIndex |= 2;
        if (localPos.z > 0) voxelIndex |= 4;
        
        if (voxelIndex < LEAF_VOXELS) {
            voxels[voxelIndex] = voxel;
        }
    } else {
        // Recurse to appropriate child
        int childIndex = getChildIndex(position);
        if (children[childIndex]) {
            children[childIndex]->setVoxel(position, voxel);
        }
    }
}

void OctreeNode::simplify() {
    if (isLeaf()) return;
    
    // Check if all children are leaves and similar enough to merge
    bool canSimplify = true;
    int validChildren = 0;
    
    // Collect voxels from all children for averaging
    std::vector<MixedVoxel> childVoxels;
    
    for (const auto& child : children) {
        if (!child || !child->isLeaf()) {
            canSimplify = false;
            break;
        }
        
        // Collect all voxels from child
        for (const auto& voxel : child->voxels) {
            childVoxels.push_back(voxel);
        }
        
        validChildren++;
    }
    
    if (canSimplify && validChildren > 0 && childVoxels.size() >= 8) {
        // Use VoxelAverager to merge children intelligently
        // Take first 8 voxels for averaging
        MixedVoxel avgVoxels[8];
        for (int i = 0; i < 8 && i < childVoxels.size(); i++) {
            avgVoxels[i] = childVoxels[i];
        }
        
        MixedVoxel averaged = VoxelAverager::average(avgVoxels);
        
        // Clear children
        for (auto& child : children) {
            child.reset();
        }
        
        // Set averaged voxel data
        for (auto& voxel : voxels) {
            voxel = averaged;
        }
    }
}

bool OctreeNode::shouldSubdivide(const glm::vec3& viewPos, float qualityFactor) const {
    // Calculate distance from viewer to node
    float distance = glm::length(viewPos - center);
    
    // Larger nodes should subdivide at greater distances
    // Smaller nodes only subdivide when very close
    float threshold = halfSize * 100.0f * qualityFactor;
    
    // Also consider level - don't subdivide too deep
    const int MAX_LEVEL = 15;
    if (level >= MAX_LEVEL) return false;
    
    return distance < threshold && isLeaf();
}

void OctreeNode::traverse(const std::function<void(OctreeNode*)>& visitor) {
    visitor(this);
    
    if (!isLeaf()) {
        for (auto& child : children) {
            if (child) {
                child->traverse(visitor);
            }
        }
    }
}

OctreeNode::GPUNode OctreeNode::toGPUNode(uint32_t& nodeIndex, uint32_t& voxelIndex) const {
    GPUNode gpuNode;
    gpuNode.center = center;
    gpuNode.halfSize = halfSize;
    gpuNode.level = level;
    gpuNode.flags = 0;
    
    if (isLeaf()) {
        gpuNode.flags |= 1; // Set leaf flag
        gpuNode.childrenIndex = 0xFFFFFFFF; // Invalid index for leaves
        gpuNode.voxelIndex = voxelIndex;
        voxelIndex += LEAF_VOXELS;
        
        // Find the dominant material type using mixed voxel averaging
        MixedVoxel avgVoxels[LEAF_VOXELS];
        for (int i = 0; i < LEAF_VOXELS; i++) {
            avgVoxels[i] = voxels[i];
        }
        MixedVoxel averaged = VoxelAverager::average(avgVoxels);
        
        // Get dominant material from the averaged voxel
        core::MaterialID dominantMatID = averaged.getDominantMaterialID();
        
        // Store MaterialID directly in flags (no mapping needed!)
        gpuNode.flags |= (static_cast<uint32_t>(dominantMatID) << 8);
    } else {
        gpuNode.childrenIndex = nodeIndex;
        gpuNode.voxelIndex = 0xFFFFFFFF; // Invalid index for internal nodes
        nodeIndex += OCTREE_CHILDREN;
    }
    
    return gpuNode;
}

// ============================================================================
// OctreePlanet Implementation
// ============================================================================

OctreePlanet::OctreePlanet(float radius, int maxDepth)
    : radius(radius), maxDepth(std::min(maxDepth, 12)),  // Increased cap to depth 12 for higher fidelity
      densityField(radius, seed) {
    // Create root node that encompasses entire planet
    // Root size should be large enough to contain the sphere
    float rootHalfSize = radius * 1.5f; // Some padding around the planet
    root = std::make_unique<OctreeNode>(glm::vec3(0.0f), rootHalfSize, 0);
}

void OctreePlanet::generateTestSphere(OctreeNode* node, int depth) {
    // Simple test: subdivide nodes that are near the planet surface
    float distToCenter = glm::length(node->center);
    float nodeRadius = node->halfSize * 1.732f; // sqrt(3) for diagonal
    
    // Check if node intersects the planet at all
    bool intersectsPlanet = (distToCenter - nodeRadius) < radius;
    
    // Only subdivide if we intersect the planet
    if (intersectsPlanet && depth < maxDepth) {
        // For coarse levels, always subdivide; for finer levels, only near surface
        bool nearSurface = std::abs(distToCenter - radius) < nodeRadius;
        if (depth < 3 || nearSurface) {
            node->subdivide();
            
            // Recursively subdivide children
            for (int i = 0; i < OctreeNode::OCTREE_CHILDREN; i++) {
                if (node->children[i]) {
                    generateTestSphere(node->children[i].get(), depth + 1);
                }
            }
        }
    }
    
    // Set materials for leaf nodes (after subdivision check)
    if (!node->isLeaf()) {
        return;
    }

    // Voxels are filled from the density field, the same field the render mesh
    // is displaced by, so materials line up with the terrain they sit on.
    const float seaLevelHeight = densityField.getSeaLevelHeight();
    const float maxElevation = std::max(densityField.getMaxElevation(), 1e-6f);
    const float crustDepth = densityField.getMaxRelief() * 0.5f;
    const float seaRadius = radius + seaLevelHeight;

    for (int i = 0; i < OctreeNode::LEAF_VOXELS; i++) {
        auto& voxel = node->voxels[i];

        // Voxel centres sit at the corners of the leaf's 2x2x2 block
        const glm::vec3 voxelOffset(
            (i & 1) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
            (i & 2) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
            (i & 4) ? node->halfSize * 0.5f : -node->halfSize * 0.5f
        );
        const glm::vec3 voxelPos = node->center + voxelOffset;
        const float voxelDist = glm::length(voxelPos);

        if (voxelDist < 1e-3f) {
            // Planet centre
            voxel = MixedVoxel::createPure(core::MaterialID::Rock);
            voxel.temperature = 255;
            voxel.pressure = 255;
            continue;
        }

        const glm::vec3 sphereNormal = voxelPos / voxelDist;
        const float terrainHeight = densityField.getTerrainHeight(sphereNormal);
        const float solidRadius = radius + terrainHeight;

        if (voxelDist > solidRadius) {
            // Above the solid surface: ocean if below sea level, else air
            if (voxelDist <= seaRadius) {
                voxel = MixedVoxel::createPure(core::MaterialID::Water);
                voxel.temperature = 125;
                voxel.pressure = 140;
            } else {
                voxel = MixedVoxel::createPure(core::MaterialID::Air);
                voxel.temperature = 10;
                voxel.pressure = 0;
            }
            continue;
        }

        // Inside the solid planet. Depth drives temperature and pressure;
        // surface elevation drives which material is exposed.
        const float depthBelowSurface = solidRadius - voxelDist;
        const float depthFraction = glm::clamp(depthBelowSurface / std::max(radius, 1.0f), 0.0f, 1.0f);
        const uint8_t temp = static_cast<uint8_t>(glm::clamp(128.0f + depthFraction * 500.0f, 0.0f, 255.0f));
        const uint8_t press = static_cast<uint8_t>(glm::clamp(128.0f + depthFraction * 500.0f, 0.0f, 255.0f));

        // Anything more than a thin crust down is just rock
        if (depthBelowSurface > crustDepth) {
            voxel = MixedVoxel::createPure(core::MaterialID::Rock);
            voxel.temperature = temp;
            voxel.pressure = press;
            continue;
        }

        // Elevation as a fraction of how high land goes, matching the bands
        // the render mesh colours with.
        const float e = (terrainHeight - seaLevelHeight) / maxElevation;

        const float elevation = terrainHeight - seaLevelHeight;
        if (elevation > densityField.getSnowLineElevation(sphereNormal)) {
            // Above the snow line - shared with the render mesh so voxel
            // materials and surface colour agree about where snow lies
            voxel = MixedVoxel::createMix(core::MaterialID::Snow, 200,
                                          core::MaterialID::Rock, 55);
        } else if (e < -0.02f) {
            // Ocean floor - sediment over rock
            voxel = MixedVoxel::createMix(core::MaterialID::Sand, 140,
                                          core::MaterialID::Rock, 115);
        } else if (e < 0.012f) {
            // Beach
            voxel = MixedVoxel::createPure(core::MaterialID::Sand);
        } else if (e < 0.10f) {
            // Lowlands
            voxel = MixedVoxel::createMix(core::MaterialID::Grass, 170,
                                          core::MaterialID::Rock, 85);
        } else if (e < 0.30f) {
            // Highlands
            voxel = MixedVoxel::createMix(core::MaterialID::Rock, 190,
                                          core::MaterialID::Grass, 65);
        } else {
            // Bare mountain rock
            voxel = MixedVoxel::createPure(core::MaterialID::Rock);
        }

        voxel.temperature = temp;
        voxel.pressure = press;
    }
}

void OctreePlanet::generate(uint32_t seed) {
    // Store seed for terrain generation
    this->seed = seed;

    // Build the tectonic simulation first: it is what decides where continents
    // and ocean basins are. The density field reads from it, and the voxel
    // octree is then filled from the density field, so all three agree.
    crust = std::make_unique<simulation::CrustGrid>(radius, seed);
    densityField.setPlanetRadius(radius);
    densityField.setSeed(seed);
    densityField.setCrustGrid(crust.get());

    // Initialize random number generator with seed
    srand(seed);

    std::cout << "Generating sphere structure from density field..." << std::endl;
    std::cout << "Planet radius: " << radius << " meters" << std::endl;
    std::cout << "Max relief: " << densityField.getMaxRelief() << " meters" << std::endl;
    std::cout << "Root node half-size: " << root->halfSize << " meters" << std::endl;
    std::cout << "Max depth: " << maxDepth << std::endl;
    
    // Timer for performance measurement
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Create a more detailed test sphere
    if (root) {
        // Subdivide nodes near the surface for better detail
        generateTestSphere(root.get(), 0);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(endTime - startTime).count();
        std::cout << "Octree generation took: " << elapsed << " seconds" << std::endl;
        
        // Count nodes for debugging
        int nodeCount = 0;
        int surfaceNodes = 0;
        root->traverse([&nodeCount, &surfaceNodes, this](OctreeNode* node) {
            if (node->isLeaf()) {
                nodeCount++;
                // Check if any voxel is non-air
                for (const auto& voxel : node->voxels) {
                    // Check if voxel is not pure air/vacuum
                    core::MaterialID mat = voxel.getDominantMaterialID();
                    if (mat != core::MaterialID::Air && mat != core::MaterialID::Vacuum) {
                        surfaceNodes++;
                        break;
                    }
                }
            }
        });
        std::cout << "Generated " << nodeCount << " leaf nodes (" << surfaceNodes << " with surface material)" << std::endl;
    }
    
    // Publish a first picture before anyone asks for one, then hand the crust
    // over to its own thread.
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        publishedSnapshot = crust->publishSnapshot();
        renderSnapshot = publishedSnapshot;
    }
    densityField.setCrustSnapshot(renderSnapshot.get());
    startSimulationThread();

    std::cout << "Planet generation complete, tectonics running on its own thread"
              << std::endl;
    // Materials are already set during simplified generation via setMaterials()
}

// REMOVED FUNCTIONS:
// - generateSphere(): Duplicate functionality, materials set in setMaterials()
// - generateTerrain(): Not used, would add continent generation  
// - generatePlates(): Not used, would add tectonic plates
// - simulatePhysics(): Too expensive on CPU, needs GPU implementation
// - simulatePlates(): Not used, plate tectonics simulation

OctreePlanet::~OctreePlanet() {
    stopSimulationThread();
}

uint64_t OctreePlanet::getCrustVersion() const {
    // The version of what the renderer is actually looking at, not what the
    // simulation thread has reached.
    return renderSnapshot ? renderSnapshot->version : 0;
}

void OctreePlanet::startSimulationThread() {
    if (simulationRunning.load() || !crust) {
        return;
    }
    simulationRunning.store(true);

    simulationThread = std::thread([this]() {
        // The simulation owns the crust while this runs. It advances at
        // whatever pace the physics allows, publishes a picture of the surface
        // after each step, and sleeps if it is running ahead of the requested
        // pace. The renderer never waits for it and never touches the grid.
        while (simulationRunning.load()) {
            const float rate = atomicSimulationRate.load();

            // Paused means paused.
            //
            // The rate used to control only how long to sleep after a step,
            // and the step itself was always a full stable timestep taken
            // before the rate was even read - so asking for zero still
            // advanced the planet a couple of million years every fiftieth of
            // a second, and asking for a tenth of that advanced it in the same
            // jumps with longer gaps between. The slider appeared to do
            // nothing because nearly nothing was what it did.
            if (rate <= 0.0f) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            const auto stepStart = std::chrono::steady_clock::now();

            // Step size follows the rate, so slowing time makes the planet
            // move slowly rather than move in the same lurches further apart.
            // Bounded above by what is numerically stable and below by what is
            // worth the fixed cost of a step - isostasy, sea level and erosion
            // all run once per call whatever the slice is, so slices far below
            // this buy smoothness at a price that is all overhead.
            const float stable = crust->maxStableTimestep();
            const float perStep = rate * 0.05f;   // aim for a step every 50 ms
            const float slice = glm::clamp(perStep, stable * 0.02f, stable);

            crust->step(slice);

            auto snapshot = crust->publishSnapshot();
            {
                std::lock_guard<std::mutex> lock(snapshotMutex);
                publishedSnapshot = std::move(snapshot);
            }

            const float spent = std::chrono::duration<float>(
                std::chrono::steady_clock::now() - stepStart).count();
            atomicAchievedRate.store(spent > 0.0f ? slice / spent : 0.0f);

            // Sleep only if we are ahead of the requested pace; otherwise run
            // flat out and let the achieved rate report the shortfall.
            const float wanted = slice / rate;
            if (wanted > spent) {
                std::this_thread::sleep_for(
                    std::chrono::duration<float>(std::min(wanted - spent, 0.25f)));
            }
        }
    });
}

void OctreePlanet::stopSimulationThread() {
    simulationRunning.store(false);
    if (simulationThread.joinable()) {
        simulationThread.join();
    }
}

void OctreePlanet::update(float deltaTime) {
    (void)deltaTime;
    if (!crust) {
        return;
    }

    // The simulation runs on its own thread now, so all this does is pick up
    // whatever picture of the surface is newest. Cheap, never blocks, and the
    // snapshot it hands to the density field stays alive and unmodified for as
    // long as the renderer holds it.
    atomicSimulationRate.store(simulationRate);
    achievedRate = atomicAchievedRate.load();

    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        if (publishedSnapshot && publishedSnapshot != renderSnapshot) {
            renderSnapshot = publishedSnapshot;
        }
    }
    densityField.setCrustSnapshot(renderSnapshot.get());
}

void OctreePlanet::updateLegacyInline(float deltaTime) {
    if (!crust || deltaTime <= 0.0f) {
        return;
    }

    // Bank wall-clock time as geological time. Stepping by the frame time
    // directly would make the simulation's behaviour depend on frame rate.
    pendingMillionYears += deltaTime * simulationRate;

    // Spend at most a slice of each frame on the simulation, and carry the
    // rest over. The step size the physics needs is set by how fast the plates
    // are moving, not by us, so on a fast planet a single frame's worth of
    // geological time can be dozens of sub-steps - running them all inline
    // freezes the window for seconds at a time even though the frame counter
    // in between still reads in the thousands.
    // A step cannot be interrupted once started, and one costs tens of
    // milliseconds, so checking a time budget after running it is useless -
    // that is a frame already lost. Instead bank the budget across frames and
    // only start a step when there is enough banked to pay for one.
    //
    // This amortises rather than eliminates the cost: the step still lands
    // inside a single frame and stalls it. The real fix is to run the
    // simulation off the render thread entirely and have the renderer read a
    // published snapshot; until then this at least keeps the average frame
    // cheap instead of every frame carrying a whole step.
    budgetBankSeconds += deltaTime * (simulationBudgetMs * 0.001f / 0.016f);
    budgetBankSeconds = std::min(budgetBankSeconds, 0.5f);

    float simulated = 0.0f;
    float spentTotal = 0.0f;
    while (pendingMillionYears > 0.0f && budgetBankSeconds >= lastStepCostSeconds) {
        const auto stepStart = std::chrono::steady_clock::now();

        const float slice = std::min(pendingMillionYears, crust->maxStableTimestep());
        crust->step(slice);

        const float cost = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - stepStart).count();

        // Remember what a step costs so the next one is only started when it
        // can be afforded.
        lastStepCostSeconds = lastStepCostSeconds * 0.5f + cost * 0.5f;
        budgetBankSeconds -= cost;
        pendingMillionYears -= slice;
        simulated += slice;
        spentTotal += cost;
    }

    if (spentTotal > 0.0f) {
        achievedRate = simulated / spentTotal;
    }

    // If the machine simply cannot keep up, cap the backlog rather than
    // accumulating a debt that can never be paid off.
    pendingMillionYears = std::min(pendingMillionYears, simulationRate * 2.0f);
}

// REMOVED: simulatePhysics() - Too expensive on CPU, needs GPU implementation
// REMOVED: simulatePlates() - Not used, plate tectonics simulation

OctreePlanet::RenderData OctreePlanet::prepareRenderData(const glm::vec3& viewPos, const glm::mat4& viewProj) {
    RenderData data;
    
    // Reserve space for nodes and voxels (start smaller, will grow as needed)
    data.nodes.reserve(100000);  // Start with 100k nodes
    data.voxels.reserve(100000 * OctreeNode::LEAF_VOXELS);
    data.visibleNodes.clear();
    
    uint32_t nodeIndex = 0;
    uint32_t voxelIndex = 0;
    
    // Frustum culling planes extraction from view-projection matrix
    glm::vec4 frustumPlanes[6];
    // Left plane
    frustumPlanes[0] = glm::vec4(
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]
    );
    // Right plane
    frustumPlanes[1] = glm::vec4(
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]
    );
    // Bottom plane
    frustumPlanes[2] = glm::vec4(
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]
    );
    // Top plane
    frustumPlanes[3] = glm::vec4(
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]
    );
    // Near plane
    frustumPlanes[4] = glm::vec4(
        viewProj[0][3] + viewProj[0][2],
        viewProj[1][3] + viewProj[1][2],
        viewProj[2][3] + viewProj[2][2],
        viewProj[3][3] + viewProj[3][2]
    );
    // Far plane
    frustumPlanes[5] = glm::vec4(
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]
    );
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(frustumPlanes[i]));
        frustumPlanes[i] /= length;
    }
    
    // Debug: Check if near plane normal points in the right direction
    static int debugCallCount = 0;
    if (radius >= 10000.0f && debugCallCount++ < 10) {
        std::cout << "  Frustum debug call #" << debugCallCount << ":" << std::endl;
        std::cout << "    Near plane: normal=(" 
                  << frustumPlanes[4].x << "," << frustumPlanes[4].y << "," << frustumPlanes[4].z 
                  << ") d=" << frustumPlanes[4].w << std::endl;
        
        // Test planet center against all planes
        for (int i = 0; i < 6; i++) {
            float dist = glm::dot(glm::vec3(frustumPlanes[i]), glm::vec3(0,0,0)) + frustumPlanes[i].w;
            const char* names[] = {"Left", "Right", "Bottom", "Top", "Near", "Far"};
            std::cout << "    " << names[i] << " plane: planet distance = " << dist << std::endl;
        }
    }
    
    // Debug counters for small planets
    int nodesChecked = 0;
    int nodesSkippedFrustum = 0;
    int nodesSkippedLOD = 0;
    int nodesSkippedAir = 0;
    int nodesAdded = 0;
    
    // Add a limit to prevent hanging with millions of nodes
    const int MAX_NODES_TO_PROCESS = 50000;  // Process at most 50k nodes
    
    // Traverse octree and collect visible nodes with hierarchical frustum culling
    std::function<bool(OctreeNode*, uint32_t)> collectNodes = [&](OctreeNode* node, uint32_t parentIndex) -> bool {
        nodesChecked++;
        
        // Safety limit to prevent hanging
        if (nodesChecked > MAX_NODES_TO_PROCESS) {
            return false;  // Stop processing
        }
        
        // HIERARCHICAL FRUSTUM CULLING
        // Test node's bounding sphere against all frustum planes
        // This works for both internal and leaf nodes
        bool inFrustum = true;
        
        // Always perform frustum culling for better performance
        // The bounding sphere radius is halfSize * sqrt(3) for a cube
        float boundingSphereRadius = node->halfSize * 1.732f; // sqrt(3) for diagonal
        
        for (int i = 0; i < 6; i++) {
            // Calculate signed distance from sphere center to plane
            float distance = glm::dot(glm::vec3(frustumPlanes[i]), node->center) + frustumPlanes[i].w;
            
            // If sphere is completely behind the plane, it's outside frustum
            if (distance < -boundingSphereRadius) {
                inFrustum = false;
                nodesSkippedFrustum++;
                
                // For internal nodes, skip entire subtree
                // For leaf nodes, just skip this node
                return false;  // Don't process this node or its children
            }
        }
        
        // Node is at least partially in frustum, continue processing
        
        // Only process leaf nodes for rendering
        if (node->isLeaf()) {
            // Simple LOD: skip small distant nodes
            float distanceToCamera = glm::length(viewPos - node->center);
            float nodeScreenSize = node->halfSize / distanceToCamera;
            
            // Skip nodes that would be smaller than 0.5 pixels
            // For small test planets, be more lenient with LOD
            float lodThreshold = (radius < 10000.0f) ? 0.00001f : 0.0005f;
            if (nodeScreenSize < lodThreshold) {
                nodesSkippedLOD++;
                return true; // Skip but continue traversal
            }
            
            // Check if this node contains non-air material
            bool hasNonAir = false;
            
            for (const auto& voxel : node->voxels) {
                // Check if voxel is not pure air/vacuum
                core::MaterialID mat = voxel.getDominantMaterialID();
                if (mat != core::MaterialID::Air && mat != core::MaterialID::Vacuum) {
                    hasNonAir = true;
                    break;
                }
            }
            
            // Only render nodes with solid material
            if (hasNonAir) {
                // Add node to render data
                uint32_t currentNodeIndex = data.nodes.size();
                data.nodes.push_back(node->toGPUNode(nodeIndex, voxelIndex));
                
                // Add voxels
                for (const auto& voxel : node->voxels) {
                    data.voxels.push_back(voxel);
                }
                data.visibleNodes.push_back(currentNodeIndex);
                nodesAdded++;
            } else {
                nodesSkippedAir++;
            }
        } else {
            // Recurse to children (don't render parent nodes)
            for (const auto& child : node->children) {
                if (child) {
                    collectNodes(child.get(), 0); // parent index not needed since we're only rendering leaves
                }
            }
        }
        return true;
    };
    
    collectNodes(root.get(), 0);
    
    // Debug output for small planets or when debugging
    if (radius < 10000.0f || nodesSkippedFrustum > 0) {
        std::cout << "  PrepareRenderData debug: checked=" << nodesChecked 
                  << ", frustum_skip=" << nodesSkippedFrustum
                  << ", lod_skip=" << nodesSkippedLOD
                  << ", air_skip=" << nodesSkippedAir
                  << ", added=" << nodesAdded << std::endl;
    }
    
    return data;
}

void OctreePlanet::updateLOD(const glm::vec3& viewPos) {
    // LOD update disabled - too expensive with millions of nodes
    // Future: Implement GPU-based LOD or use hierarchical caching
    return;
    
    /* Disabled LOD code - needs optimization
    std::function<void(OctreeNode*)> updateNode = [&](OctreeNode* node) {
        if (node->shouldSubdivide(viewPos)) {
            node->subdivide();
            
            // Would need to set materials for new children here
            // Previously called generateSphere() which is now removed
            if (!node->isLeaf()) {
                for (auto& child : node->children) {
                    if (child) {
                        // TODO: Set materials for new LOD nodes
                        // setMaterials(child.get());
                    }
                }
            }
        } else if (!node->isLeaf()) {
            // Check if we should simplify (merge children)
            float distance = glm::length(viewPos - node->center);
            float threshold = node->halfSize * 200.0f; // Simplify at 2x subdivision distance
            
            if (distance > threshold) {
                node->simplify();
            } else {
                // Recurse to children
                for (auto& child : node->children) {
                    if (child) {
                        updateNode(child.get());
                    }
                }
            }
        }
    };
    
    updateNode(root.get());
    */
}

Voxel* OctreePlanet::getVoxel(const glm::vec3& position) {
    return root->getVoxel(position);
}

const Voxel* OctreePlanet::getVoxel(const glm::vec3& position) const {
    return root->getVoxel(position);
}

void OctreePlanet::setVoxel(const glm::vec3& position, const Voxel& voxel) {
    root->setVoxel(position, voxel);
}

bool OctreePlanet::isInsidePlanet(const glm::vec3& position) const {
    return glm::length(position) <= radius;
}

// Prepare FULL octree for GPU traversal - includes entire hierarchy
OctreePlanet::RenderData OctreePlanet::prepareFullOctreeData() {
    RenderData data;
    
    // LIMIT DEPTH to avoid GPU memory overflow
    // Full octree at depth 12 can be >1.5GB!
    const int MAX_GPU_DEPTH = 6;  // Conservative limit for testing (depth 6 = ~50-100MB)
    
    // Reserve space for entire octree (estimate based on depth)
    // For depth D, worst case is 8^D nodes, but typically much less
    size_t estimatedNodes = 100000;  // Start conservative
    data.nodes.reserve(estimatedNodes);
    data.voxels.reserve(estimatedNodes * OctreeNode::LEAF_VOXELS);
    
    uint32_t nextNodeIndex = 0;
    uint32_t nextVoxelIndex = 0;
    
    // Map to track node indices for parent-child linking
    std::unordered_map<OctreeNode*, uint32_t> nodeIndexMap;
    
    // BFS traversal to maintain proper indexing
    std::queue<OctreeNode*> nodeQueue;
    nodeQueue.push(root.get());
    
    while (!nodeQueue.empty()) {
        OctreeNode* node = nodeQueue.front();
        nodeQueue.pop();
        
        // Record this node's index
        uint32_t currentNodeIndex = nextNodeIndex++;
        nodeIndexMap[node] = currentNodeIndex;
        
        // Create GPU node
        OctreeNode::GPUNode gpuNode;
        gpuNode.center = node->center;
        gpuNode.halfSize = node->halfSize;
        gpuNode.level = node->level;
        gpuNode.flags = 0;
        
        if (node->isLeaf() || node->level >= MAX_GPU_DEPTH) {
            // Leaf node OR depth limit reached
            gpuNode.flags |= 1;  // Set leaf flag
            gpuNode.childrenIndex = 0xFFFFFFFF;  // No children
            gpuNode.voxelIndex = nextVoxelIndex;
            
            // Add voxels
            for (const auto& voxel : node->voxels) {
                data.voxels.push_back(voxel);
            }
            nextVoxelIndex += OctreeNode::LEAF_VOXELS;
        } else {
            // Internal node - we'll update childrenIndex after processing children
            gpuNode.voxelIndex = 0xFFFFFFFF;  // No voxels
            gpuNode.childrenIndex = nextNodeIndex;  // Children will start here
            
            // Queue all children for processing
            for (int i = 0; i < 8; i++) {
                if (node->children[i]) {
                    nodeQueue.push(node->children[i].get());
                } else {
                    // Create placeholder for missing child
                    OctreeNode::GPUNode emptyNode;
                    emptyNode.center = node->center;  // Use parent center
                    emptyNode.halfSize = node->halfSize * 0.5f;
                    emptyNode.level = node->level + 1;
                    emptyNode.flags = 1;  // Mark as leaf
                    emptyNode.childrenIndex = 0xFFFFFFFF;
                    emptyNode.voxelIndex = 0xFFFFFFFF;  // No voxels
                    data.nodes.push_back(emptyNode);
                    nextNodeIndex++;
                }
            }
        }
        
        // Add the node at its designated index
        if (currentNodeIndex >= data.nodes.size()) {
            data.nodes.resize(currentNodeIndex + 1);
        }
        data.nodes[currentNodeIndex] = gpuNode;
    }
    
    // Calculate memory usage
    size_t nodeMemory = data.nodes.size() * sizeof(OctreeNode::GPUNode);
    size_t voxelMemory = data.voxels.size() * sizeof(Voxel);
    size_t totalMemory = nodeMemory + voxelMemory;
    
    std::cout << "[OctreePlanet] Full octree prepared for GPU (depth limited to " << MAX_GPU_DEPTH << "):\n";
    std::cout << "  Total nodes: " << data.nodes.size() << "\n";
    std::cout << "  Total voxels: " << data.voxels.size() << "\n";
    std::cout << "  Memory usage: " << (totalMemory / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "    Nodes: " << (nodeMemory / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "    Voxels: " << (voxelMemory / (1024.0 * 1024.0)) << " MB\n";
    
    if (data.nodes.size() > 0) {
        std::cout << "  Root at origin: center=(" 
                  << data.nodes[0].center.x << ", " 
                  << data.nodes[0].center.y << ", " 
                  << data.nodes[0].center.z << "), halfSize=" 
                  << data.nodes[0].halfSize << "\n";
    }
    
    // Warn if memory usage is high
    if (totalMemory > 512 * 1024 * 1024) {  // > 512MB
        std::cout << "  WARNING: Large GPU memory usage! Consider reducing octree depth.\n";
    }
    
    return data;
}

float OctreePlanet::getDistanceFromSurface(const glm::vec3& position) const {
    return glm::length(position) - radius;
}

} // namespace octree