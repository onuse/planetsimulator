#include "core/spherical_quadtree.hpp"
#include "core/camera.hpp"
#include "core/global_patch_generator.hpp"
#include "math/planet_math.hpp"
#include "math/patch_alignment.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace core {

// SphericalQuadtreeNode implementation
SphericalQuadtreeNode::SphericalQuadtreeNode(const glm::dvec3& center, double size, 
                                             uint32_t level, Face face, 
                                             SphericalQuadtreeNode* parent)
    : parent(parent), level(level), face(face) {
    
    // Use the new GlobalPatchGenerator for truly global coordinates from the start
    // Calculate the bounds in cube space based on face (using double precision)
    glm::dvec3 minBounds, maxBounds;
    double halfSize = size * 0.5;
    
    // For face-aligned patches, only vary the non-face dimensions
    switch (face) {
    case FACE_POS_X:
        minBounds = glm::dvec3(1.0, center.y - halfSize, center.z - halfSize);
        maxBounds = glm::dvec3(1.0, center.y + halfSize, center.z + halfSize);
        break;
    case FACE_NEG_X:
        minBounds = glm::dvec3(-1.0, center.y - halfSize, center.z - halfSize);
        maxBounds = glm::dvec3(-1.0, center.y + halfSize, center.z + halfSize);
        break;
    case FACE_POS_Y:
        minBounds = glm::dvec3(center.x - halfSize, 1.0, center.z - halfSize);
        maxBounds = glm::dvec3(center.x + halfSize, 1.0, center.z + halfSize);
        break;
    case FACE_NEG_Y:
        minBounds = glm::dvec3(center.x - halfSize, -1.0, center.z - halfSize);
        maxBounds = glm::dvec3(center.x + halfSize, -1.0, center.z + halfSize);
        break;
    case FACE_POS_Z:
        minBounds = glm::dvec3(center.x - halfSize, center.y - halfSize, 1.0);
        maxBounds = glm::dvec3(center.x + halfSize, center.y + halfSize, 1.0);
        break;
    case FACE_NEG_Z:
        minBounds = glm::dvec3(center.x - halfSize, center.y - halfSize, -1.0);
        maxBounds = glm::dvec3(center.x + halfSize, center.y + halfSize, -1.0);
        break;
    }
    
    // Clamp to cube boundaries
    minBounds = glm::max(minBounds, glm::dvec3(-1.0));
    maxBounds = glm::min(maxBounds, glm::dvec3(1.0));
    
    // Create patch using global coordinate system
    GlobalPatchGenerator::GlobalPatch globalPatch;
    globalPatch.minBounds = glm::vec3(minBounds);  // GlobalPatch still uses vec3
    globalPatch.maxBounds = glm::vec3(maxBounds);
    globalPatch.center = glm::vec3((minBounds + maxBounds) * 0.5);
    globalPatch.level = level;
    globalPatch.faceId = static_cast<int>(face);
    
    // Transfer data to our QuadtreePatch structure (keeping double precision)
    patch.center = (minBounds + maxBounds) * 0.5;  // Use double precision directly
    patch.minBounds = minBounds;
    patch.maxBounds = maxBounds;
    patch.size = static_cast<float>(size);  // Size can stay float for now
    patch.level = level;
    patch.faceId = static_cast<uint32_t>(face);
    patch.morphFactor = 0.0f;
    patch.screenSpaceError = 0.0f;
    
    // Get corners in canonical global order
    glm::vec3 corners[4];
    globalPatch.getCorners(corners);
    for (int i = 0; i < 4; i++) {
        patch.corners[i] = glm::dvec3(corners[i]);  // Convert to double precision
        patch.neighborLevels[i] = level;
    }
    
    // Create the patch transform using GlobalPatch (convert result to double)
    patch.patchTransform = glm::dmat4(globalPatch.createTransform());
    
    // Clear neighbors
    for (int i = 0; i < 4; i++) {
        neighbors[i] = nullptr;
    }
}

glm::dvec3 SphericalQuadtreeNode::cubeToSphere(const glm::dvec3& cubePos, double radius) const {
    // Use the tested math function
    glm::dvec3 spherePos = math::cubeToSphere(cubePos);
    return spherePos * radius;
}

void SphericalQuadtreeNode::subdivide(const DensityField& densityField) {
    if (!isLeaf()) return;
    
    // Create GlobalPatch from current patch
    GlobalPatchGenerator::GlobalPatch globalPatch;
    
    // Calculate bounds from corners (use double precision)
    glm::dvec3 minBounds(1e9), maxBounds(-1e9);
    for (int i = 0; i < 4; i++) {
        minBounds = glm::min(minBounds, patch.corners[i]);
        maxBounds = glm::max(maxBounds, patch.corners[i]);
    }
    
    globalPatch.minBounds = glm::vec3(minBounds);
    globalPatch.maxBounds = glm::vec3(maxBounds);
    globalPatch.center = glm::vec3(patch.center);
    globalPatch.level = patch.level;
    globalPatch.faceId = static_cast<int>(face);
    
    // Subdivide using global coordinates
    std::vector<GlobalPatchGenerator::GlobalPatch> childPatches = 
        GlobalPatchGenerator::subdivide(globalPatch);
    
    double childSize = patch.size * 0.5;
    
    for (int i = 0; i < 4; i++) {
        children[i] = std::make_unique<SphericalQuadtreeNode>(
            glm::dvec3(childPatches[i].center), childSize, level + 1, face, this
        );
        
        // Sample heights for the child patch
        // Use higher resolution for closer LODs
        uint32_t resolution = 32; // Could vary by level
        children[i]->sampleHeights(densityField, resolution);
    }
    
    // Set up internal neighbor connections between children
    // Child layout:
    // 3---2
    // |   |
    // 0---1
    children[0]->setNeighbor(EDGE_RIGHT, children[1].get());
    children[1]->setNeighbor(EDGE_LEFT, children[0].get());
    children[1]->setNeighbor(EDGE_TOP, children[2].get());
    children[2]->setNeighbor(EDGE_BOTTOM, children[1].get());
    children[2]->setNeighbor(EDGE_LEFT, children[3].get());
    children[3]->setNeighbor(EDGE_RIGHT, children[2].get());
    children[3]->setNeighbor(EDGE_BOTTOM, children[0].get());
    children[0]->setNeighbor(EDGE_TOP, children[3].get());
    
    updateNeighborReferences();
}

glm::dvec3 SphericalQuadtreeNode::getChildCenter(int childIndex) const {
    double quarterSize = patch.size * 0.25;
    glm::dvec3 offset;
    
    switch (childIndex) {
        case 0: offset = glm::dvec3(-quarterSize, -quarterSize, 0); break; // Bottom-left
        case 1: offset = glm::dvec3(quarterSize, -quarterSize, 0); break;  // Bottom-right
        case 2: offset = glm::dvec3(quarterSize, quarterSize, 0); break;   // Top-right
        case 3: offset = glm::dvec3(-quarterSize, quarterSize, 0); break;  // Top-left
    }
    
    // FIXED: Use consistent coordinate transformations matching the face orientations
    glm::dvec3 right, up;
    switch (face) {
        case FACE_POS_X:
            right = glm::dvec3(0, 0, 1);
            up = glm::dvec3(0, 1, 0);
            return patch.center + right * offset.x + up * offset.y;
        case FACE_NEG_X:
            right = glm::dvec3(0, 0, -1);
            up = glm::dvec3(0, 1, 0);
            return patch.center + right * offset.x + up * offset.y;
        case FACE_POS_Y:
            right = glm::dvec3(1, 0, 0);
            up = glm::dvec3(0, 0, 1);
            return patch.center + right * offset.x + up * offset.y;
        case FACE_NEG_Y:
            right = glm::dvec3(1, 0, 0);
            up = glm::dvec3(0, 0, -1);
            return patch.center + right * offset.x + up * offset.y;
        case FACE_POS_Z:
            right = glm::dvec3(-1, 0, 0);
            up = glm::dvec3(0, 1, 0);
            return patch.center + right * offset.x + up * offset.y;
        case FACE_NEG_Z:
            right = glm::dvec3(1, 0, 0);
            up = glm::dvec3(0, 1, 0);
            return patch.center + right * offset.x + up * offset.y;
    }
    
    return patch.center;
}

void SphericalQuadtreeNode::merge() {
    if (isLeaf()) return;
    
    for (auto& child : children) {
        child.reset();
    }
    
    updateNeighborReferences();
}

float SphericalQuadtreeNode::calculateScreenSpaceError(const glm::vec3& viewPos, 
                                                       const glm::mat4& viewProj) const {
    // Use the tested math function with comprehensive logging
    const double planetRadius = 6371000.0;
    glm::dvec3 sphereCenter = cubeToSphere(patch.center, planetRadius);
    
    // Convert to double precision for calculation
    float error = math::calculateScreenSpaceError(
        glm::dvec3(sphereCenter),
        static_cast<double>(patch.size),
        glm::dvec3(viewPos),
        glm::dmat4(viewProj),
        static_cast<double>(planetRadius),
        720  // Screen height
    );
    
    // DEBUG: Log screen space error for debugging patch sizes
    static int errorLogCount = 0;
    static int frameCounter = 0;
    frameCounter++;
    
    // Log first few patches and periodically
    if ((errorLogCount++ < 20) || (frameCounter % 600 == 0 && errorLogCount % 50 == 0)) {
        double distanceFromViewer = glm::length(sphereCenter - glm::dvec3(viewPos));
        double patchWorldSize = patch.size * planetRadius; // Convert from cube space to world space
        // MUTED: Too spammy
        // std::cout << "[ScreenError] Face:" << static_cast<int>(face) 
        //           << " Level:" << level 
        //           << " Error:" << error
        //           << " WorldSize:" << (patchWorldSize/1000.0) << "km"
        //           << " Distance:" << (distanceFromViewer/1000.0) << "km" << std::endl;
    }
    
    return error;
}

void SphericalQuadtreeNode::selectLOD(const glm::vec3& viewPos, const glm::mat4& viewProj,
                                      float errorThreshold, uint32_t maxLevel,
                                      std::vector<QuadtreePatch>& visiblePatches,
                                      bool enableBackfaceCulling) {
    
    // CRITICAL FIX 1: Hard limit on recursion depth
    // BALANCE: Need enough subdivision for detail, but not too much for performance
    // Level 10 = 1024 subdivisions per face edge = reasonable detail
    const uint32_t ABSOLUTE_MAX_LEVEL = 10;  // Was 15, then 8, now 10
    if (level >= ABSOLUTE_MAX_LEVEL) {
        if (isLeaf()) {
            patch.isVisible = true;
            visiblePatches.push_back(patch);
        }
        return;  // Stop recursion immediately
    }
    
    // PERFORMANCE: Dynamic patch budget based on altitude and view
    // Calculate appropriate patch budget for current viewing conditions
    float altitude = glm::length(viewPos) - 6371000.0f;  // Camera altitude in meters
    
    // STRESS TEST: Much more restrictive budgets to force culling issues
    size_t patchBudget;
    if (altitude > 10000000.0f) {        // > 10,000 km - see whole planet
        patchBudget = 100;   // Was 500
    } else if (altitude > 5000000.0f) {  // > 1,000 km - see large area
        patchBudget = 200;   // Was 1000
    } else if (altitude > 1000000.0f) {  // > 1,000 km - see large area
        patchBudget = 400;   // Was 1000
    } else if (altitude > 500000.0f) {   // > 100 km - standard view
        patchBudget = 500;   // Was 2000
    } else if (altitude > 100000.0f) {   // > 100 km - standard view
        patchBudget = 500;   // Was 2000
    } else if (altitude > 50000.0f) {   // > 100 km - standard view
        patchBudget = 600;   // Was 2000
    } else if (altitude > 10000.0f) {    // > 10 km - close view
        patchBudget = 700;   // Was 3000
    } else {                              // < 10 km - surface level
        patchBudget = 800;   // Was 4000
    }
    
    // Log budget changes
    static size_t lastBudget = 0;
    if (patchBudget != lastBudget) {
        std::cout << "[PATCH BUDGET] Altitude: " << altitude/1000.0f << "km -> Budget: " 
                  << patchBudget << " patches" << std::endl;
        lastBudget = patchBudget;
    }
    
    // GLOBAL BUDGET: Check against total patches across ALL faces
    // This will stress the culling system as intended
    size_t totalPatches = visiblePatches.size();
    
    // Check if we've hit the global budget
    if (totalPatches >= patchBudget) {
        // Hit global limit - just render current level without subdividing
        if (isLeaf()) {
            patch.isVisible = true;
            visiblePatches.push_back(patch);
        }
        // Log when we hit the limit (only occasionally to avoid spam)
        static int limitLogCount = 0;
        if (limitLogCount++ % 100 == 0) {
            std::cout << "[GLOBAL PATCH BUDGET] Alt: " << altitude/1000.0f << "km"
                      << ", Budget: " << patchBudget
                      << ", Used: " << totalPatches
                      << ", Face " << face << " (stopping subdivisions)" << std::endl;
        }
        return;
    }
    
    // BACKFACE CULLING: Check if this patch faces away from the camera
    if (enableBackfaceCulling) {
        // First convert cube position to sphere position
        glm::dvec3 spherePos = cubeToSphere(patch.center, 6371000.0);
        glm::dvec3 patchNormal = glm::normalize(spherePos);
        glm::dvec3 toCamera = glm::normalize(glm::dvec3(viewPos) - spherePos);
        
        // Dot product tells us if patch faces camera
        double dotProduct = glm::dot(patchNormal, toCamera);
        
        // Cull patches facing away (with some tolerance for edge cases)
        // Tolerance depends on patch size and level
        // At the horizon, dot product is 0, so we need to be careful
        // Positive dot = facing camera, negative = facing away
        double cullThreshold = 0.0; // Cull if dot < 0 (facing away)
        
        // Add some tolerance based on patch level to account for curvature
        // Higher level patches are smaller and need less tolerance
        // Level 1 patches are our base 24 patches, they need special handling
        if (level == 0) {
            // Never cull level 0 (the 6 root faces) - they're not leaves anyway
            cullThreshold = -1.0;
        } else if (level == 1) {
            // Level 1 are our 24 base patches - be lenient to see horizon
            cullThreshold = -0.4; // Allow patches up to 40% facing away
        } else if (level <= 3) {
            cullThreshold = -0.1; // Some tolerance for mid-level patches
        } else {
            cullThreshold = 0.0; // Strict for deeply subdivided patches
        }
        
        // Track culling statistics for leaf nodes
        static int cullCount = 0;
        static int totalLeafChecks = 0;
        if (isLeaf()) {
            totalLeafChecks++;
        }
        
        if (dotProduct < cullThreshold) {
            // Patch faces away from camera - cull it
            // But if it's not a leaf, we still need to check children
            // as they might be visible due to sphere curvature
            if (isLeaf()) {
                cullCount++;
                if (totalLeafChecks % 1000 == 0) {
                    std::cout << "[BACKFACE CULLING] Culled " << cullCount << " of " << totalLeafChecks 
                              << " leaf patches (" << (100.0f * cullCount / totalLeafChecks) << "%)" << std::endl;
                }
                return; // Skip this patch entirely
            }
            // For non-leaves, still recurse but with tighter culling
            cullThreshold = 0.1; // More aggressive for children
        }
    }
    
    float error = calculateScreenSpaceError(viewPos, viewProj);
    patch.screenSpaceError = error;
    
    // DEBUG: Disabled for performance
    // static int patchDebugCount = 0;
    // if (patchDebugCount++ < 10) {
    //     std::cout << "[DEBUG] selectLOD: level=" << level 
    //               << ", error=" << error 
    //               << ", threshold=" << errorThreshold 
    //               << ", maxLevel=" << maxLevel
    //               << ", shouldSubdivide=" << (error > errorThreshold && level < maxLevel) 
    //               << std::endl;
    // }
    
    // CRITICAL FIX 2: Ensure maxLevel is reasonable
    uint32_t safeMaxLevel = std::min(maxLevel, ABSOLUTE_MAX_LEVEL);
    
    // CRITICAL FIX 3: Minimum patch size check (1 meter at planet surface)
    const float MIN_PATCH_SIZE = 1.0f / (1 << ABSOLUTE_MAX_LEVEL);  // Smallest meaningful patch
    if (patch.size < MIN_PATCH_SIZE) {
        if (isLeaf()) {
            patch.isVisible = true;
            visiblePatches.push_back(patch);
        }
        return;  // Patch too small to subdivide further
    }
    
    bool shouldSubdivide = error > errorThreshold && level < safeMaxLevel;
    
    // Log subdivision decisions for key nodes (reduced frequency to prevent spam)
    // DISABLED: Too verbose, overwhelming the output
    // static int lodLogCount = 0;
    // if (lodLogCount++ % 10000 == 0 || (level < 3 && shouldSubdivide)) {
    //     std::cout << "[SelectLOD] Level:" << level 
    //               << " Error:" << error 
    //               << " Threshold:" << errorThreshold
    //               << " ShouldSubdivide:" << shouldSubdivide
    //               << " IsLeaf:" << isLeaf() << std::endl;
    // }
    
    // DEBUG: Log LOD selection decisions for root level patches
    static int lodLogCount = 0;
    if (level <= 1 && (lodLogCount++ % 20 == 0)) {
    //    glm::dvec3 sphereCenter = cubeToSphere(patch.center, 6371000.0);
    //    double distanceFromViewer = glm::length(sphereCenter - glm::dvec3(viewPos));
    //    std::cout << "[LOD] Face:" << static_cast<int>(face) 
    //              << " Level:" << level 
    //              << " Error:" << error 
    //              << " Threshold:" << errorThreshold
    //              << " Distance:" << (distanceFromViewer/1000.0) << "km"
    //              << " ShouldSubdivide:" << shouldSubdivide
    //              << " IsLeaf:" << isLeaf() << std::endl;
    }

    if (shouldSubdivide) {
        if (isLeaf()) {
            // This is a leaf that needs to subdivide
            // We'll actually subdivide it now (requires access to density field)
            // For now, just render at current level and mark for subdivision
            patch.needsUpdate = true;
            patch.isVisible = true;
            
            // Update neighbor levels even for patches that need subdivision
            for (int i = 0; i < 4; i++) {
                if (neighbors[i]) {
                    patch.neighborLevels[i] = neighbors[i]->level;
                } else {
                    patch.neighborLevels[i] = level;
                }
            }
            
            visiblePatches.push_back(patch);
        } else {
            // Already subdivided, recurse to children
            for (auto& child : children) {
                if (child) {
                    child->selectLOD(viewPos, viewProj, errorThreshold, maxLevel, visiblePatches, enableBackfaceCulling);
                }
            }
        }
    } else {
        // This node is at appropriate detail level
        if (isLeaf()) {
            // Render this leaf node
            patch.isVisible = true;
            
            // Update neighbor levels for T-junction prevention
            for (int i = 0; i < 4; i++) {
                if (neighbors[i]) {
                    patch.neighborLevels[i] = neighbors[i]->level;
                } else {
                    // No neighbor means we're at an edge - use same level
                    patch.neighborLevels[i] = level;
                }
            }
            
            visiblePatches.push_back(patch);
        } else {
            // We have children but don't need that detail - could merge
            // For now, just render this node
            patch.isVisible = true;
            
            // Update neighbor levels
            for (int i = 0; i < 4; i++) {
                if (neighbors[i]) {
                    patch.neighborLevels[i] = neighbors[i]->level;
                } else {
                    patch.neighborLevels[i] = level;
                }
            }
            
            visiblePatches.push_back(patch);
            
            // Mark for potential merge if error is very low
            if (error < errorThreshold * 0.3f) {
                patch.needsUpdate = true; // Mark for potential merge
            }
        }
    }
}

void SphericalQuadtreeNode::updateMorphFactor(float targetError, float morphRegion) {
    float normalizedError = patch.screenSpaceError / targetError;
    
    // Morph factor calculation
    if (normalizedError < 1.0f - morphRegion) {
        patch.morphFactor = 0.0f; // Full detail
    } else if (normalizedError > 1.0f) {
        patch.morphFactor = 1.0f; // Parent level
    } else {
        // Smooth transition
        float t = (normalizedError - (1.0f - morphRegion)) / morphRegion;
        patch.morphFactor = t * t * (3.0f - 2.0f * t); // Smoothstep
    }
}

void SphericalQuadtreeNode::setNeighbor(Edge edge, SphericalQuadtreeNode* neighbor) {
    neighbors[edge] = neighbor;
    if (neighbor) {
        patch.neighbors[edge] = neighbor;
        patch.neighborLevels[edge] = neighbor->getLevel();
    }
}

void SphericalQuadtreeNode::updateNeighborReferences() {
    // Update neighbor references after subdivision/merge
    // This is complex and would need careful implementation to handle all cases
    // For now, leaving as a stub
}

void SphericalQuadtreeNode::sampleHeights(const DensityField& densityField, uint32_t resolution) {
    if (heightResolution == resolution && !heights.empty()) {
        return; // Already sampled
    }
    
    heightResolution = resolution;
    heights.resize(resolution * resolution);
    
    for (uint32_t y = 0; y < resolution; y++) {
        for (uint32_t x = 0; x < resolution; x++) {
            float u = static_cast<float>(x) / (resolution - 1);
            float v = static_cast<float>(y) / (resolution - 1);
            
            // Interpolate position on patch (use double precision)
            glm::dvec3 pos = patch.corners[0] * (1.0 - u) * (1.0 - v) +
                            patch.corners[1] * double(u) * (1.0 - v) +
                            patch.corners[2] * double(u) * double(v) +
                            patch.corners[3] * (1.0 - u) * double(v);
            
            // Project to sphere and get height
            glm::dvec3 spherePos = cubeToSphere(pos, densityField.getPlanetRadius());
            float height = densityField.getTerrainHeight(glm::vec3(glm::normalize(spherePos)));
            
            heights[y * resolution + x] = height;
        }
    }
}

// SphericalQuadtree implementation
SphericalQuadtree::SphericalQuadtree(const Config& config, std::shared_ptr<DensityField> densityField)
    : config(config), densityField(densityField) {
    // PERFORMANCE: Disabled initialization logging
    // std::cout << "[SphericalQuadtree] Creating with planet radius: " << config.planetRadius << std::endl;
    initializeRoots();
}

void SphericalQuadtree::initializeRoots() {
    // Create six root nodes for cube faces (use double precision)
    roots[0] = std::make_unique<SphericalQuadtreeNode>(
        glm::dvec3(1, 0, 0), 2.0, 0, SphericalQuadtreeNode::FACE_POS_X);
    roots[1] = std::make_unique<SphericalQuadtreeNode>(
        glm::dvec3(-1, 0, 0), 2.0, 0, SphericalQuadtreeNode::FACE_NEG_X);
    roots[2] = std::make_unique<SphericalQuadtreeNode>(
        glm::dvec3(0, 1, 0), 2.0, 0, SphericalQuadtreeNode::FACE_POS_Y);
    roots[3] = std::make_unique<SphericalQuadtreeNode>(
        glm::dvec3(0, -1, 0), 2.0, 0, SphericalQuadtreeNode::FACE_NEG_Y);
    roots[4] = std::make_unique<SphericalQuadtreeNode>(
        glm::dvec3(0, 0, 1), 2.0, 0, SphericalQuadtreeNode::FACE_POS_Z);
    roots[5] = std::make_unique<SphericalQuadtreeNode>(
        glm::dvec3(0, 0, -1), 2.0, 0, SphericalQuadtreeNode::FACE_NEG_Z);
    
    // MANDATORY: Subdivide all root faces once to create 24 base patches
    // This provides better culling granularity and prevents "bald planet" issues
    std::cout << "[SphericalQuadtree] Creating 24 base patches (mandatory level-1 subdivision)" << std::endl;
    for (int i = 0; i < 6; i++) {
        if (roots[i] && roots[i]->isLeaf()) {
            roots[i]->subdivide(*densityField);
        }
    }
    std::cout << "[SphericalQuadtree] Initialization complete with 24 base patches" << std::endl;
    
    // Update node count to reflect the mandatory subdivisions
    totalNodeCount = 30;  // 6 root nodes + 24 children (4 per root)
    
    // Set up neighbor relationships between root faces
    // This is complex - each face has specific neighbors at edges
    // Simplified for now
}

void SphericalQuadtree::update(const glm::vec3& viewPos, const glm::mat4& viewProj, float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Track camera movement to determine if we need to update LOD
    static glm::vec3 lastViewPos = viewPos;
    static float lastErrorThreshold = -1.0f;
    
    float viewMovement = glm::length(viewPos - lastViewPos);
    float currentCamDistance = glm::length(viewPos);
    float altitude = currentCamDistance - config.planetRadius;
    
    // Only update LOD if camera moved significantly or it's the first frame
    bool cameraMovedSignificantly = viewMovement > (altitude * 0.01f); // 1% of altitude
    static bool isFirstFrame = true;  // Track actual first frame properly
    
    // Also check for zoom changes (altitude changes)
    static float lastAltitude = altitude;
    bool altitudeChanged = std::abs(altitude - lastAltitude) > (altitude * 0.01f); // 1% altitude change
    
    bool shouldUpdateLOD = cameraMovedSignificantly || altitudeChanged || isFirstFrame;
    
    // DEBUG: Add frame counter and zoom detection for logging
    static int frameCount = 0;
    static float lastCamDistance = -1.0f;
    frameCount++;
    
    float distanceChange = (lastCamDistance > 0) ? std::abs(currentCamDistance - lastCamDistance) : 0;
    bool isZooming = distanceChange > 50000.0f; // Significant movement (50km)
    bool shouldLogFrame = (frameCount % 20 == 0) || isZooming; // Log every 20 frames OR when zooming
    
    if (shouldUpdateLOD) {
        if (cameraMovedSignificantly || altitudeChanged || isFirstFrame) {
            std::cout << "[LOD UPDATE] " << (isFirstFrame ? "First frame" : 
                         altitudeChanged ? "Altitude changed" : "Camera moved") 
                      << " " << viewMovement << "m, alt change: " << std::abs(altitude - lastAltitude) 
                      << "m, updating subdivisions" << std::endl;
        }
        lastViewPos = viewPos;
        lastAltitude = altitude;
        isFirstFrame = false;  // Clear first frame flag after first update
    }
    
    if (isZooming && lastCamDistance > 0) {
        std::cout << "\n[ZOOM DETECTED] Distance changed by " << distanceChange/1000.0f 
                  << "km (from " << lastCamDistance/1000.0f << "km to " 
                  << currentCamDistance/1000.0f << "km)" << std::endl;
    }
    lastCamDistance = currentCamDistance;
    
    if (shouldLogFrame) {
        std::cout << "\n=== QUADTREE DEBUG FRAME " << frameCount << " ===\n";
        std::cout << "[DEBUG] ViewPos: " << viewPos.x << "," << viewPos.y << "," << viewPos.z 
                  << " (distance: " << glm::length(viewPos) << ")\n";
        std::cout << "[DEBUG] Altitude: " << (glm::length(viewPos) - config.planetRadius) << "m\n";
    }
    
    // Clear visible patches from last frame
    visiblePatches.clear();
    stats.visibleNodes = 0;
    stats.subdivisions = 0;
    stats.merges = 0;
    
    // Debug: Track patch selection
    static int frameDebugCount = 0;
    bool debugFrame = (frameDebugCount++ % 60 == 0);  // Every 60 frames
    
    // Safety limit - prevent excessive patches
    const size_t MAX_PATCHES = 50000;  // Increased limit for close-up detail
    
    // Calculate error threshold based on view distance
    float errorThreshold = calculateErrorThreshold(viewPos);
    
    // Dynamic adjustment - if we're getting too many patches, increase threshold
    static int highPatchFrames = 0;
    if (visiblePatches.size() > 2000) {  // More reasonable limit
        highPatchFrames++;
        if (highPatchFrames > 10) {
            // Sustained high patch count, increase threshold
            errorThreshold *= 1.2f;  // Gentler adjustment
            // PERFORMANCE: Disabled threshold adjustment logging
            // std::cout << "[SphericalQuadtree] Adjusting error threshold to " << errorThreshold 
            //           << " due to high patch count (" << visiblePatches.size() << ")" << std::endl;
        }
    } else {
        highPatchFrames = 0;
    }
    
    // PERFORMANCE: Disabled face processing debug logging
    // static int frameCount = 0;
    // bool shouldLog = (frameCount++ % 120 == 0);  // Log every 2 seconds at 60fps
    bool shouldLog = false;  // PERFORMANCE: Disabled all debug logging
    
    // if (shouldLog) {
    //     std::cout << "[DEBUG] Processing cube faces. enableFaceCulling=" 
    //               << config.enableFaceCulling << std::endl;
    // }
    
    // DEBUG: Track patches per face
    std::vector<size_t> patchesBeforeFace(6, 0);
    std::vector<size_t> patchesAfterFace(6, 0);
    
    // Rotate starting face each frame to ensure fairness
    static int startFace = 0;
    startFace = (startFace + 1) % 6;
    
    for (int j = 0; j < 6; j++) {
        int i = (startFace + j) % 6;  // Process faces in rotating order
        auto& root = roots[i];
        
        if (!root) {
            if (shouldLogFrame) std::cout << "[DEBUG] Face " << i << " - root is null!" << std::endl;
            continue;
        }
        
        patchesBeforeFace[i] = visiblePatches.size();
        
        // Check if this cube face is visible from the camera
        // Get the face normal based on the face index
        glm::vec3 faceNormal;
        switch (i) {
            case 0: faceNormal = glm::vec3(1, 0, 0); break;   // +X
            case 1: faceNormal = glm::vec3(-1, 0, 0); break;  // -X
            case 2: faceNormal = glm::vec3(0, 1, 0); break;   // +Y
            case 3: faceNormal = glm::vec3(0, -1, 0); break;  // -Y
            case 4: faceNormal = glm::vec3(0, 0, 1); break;   // +Z
            case 5: faceNormal = glm::vec3(0, 0, -1); break;  // -Z
        }
        
        // Use math function for culling decision with logging
        bool shouldCull = config.enableFaceCulling ? 
            math::shouldCullFace(i, viewPos, config.planetRadius) : false;
        
        if (shouldCull) {
            if (shouldLogFrame) {
                std::cout << "[DEBUG] Face " << i << " - CULLED" << std::endl;
            }
            continue; // Face is on the far side of the planet
        }
        
        if (shouldLogFrame) {
            std::cout << "[DEBUG] Face " << i << " - PROCESSING..." 
                      << " (isLeaf: " << root->isLeaf() 
                      << ", children: " << (root->children[0] ? "Y" : "N") << ")" << std::endl;
        }
        root->selectLOD(viewPos, viewProj, errorThreshold, config.maxLevel, visiblePatches, config.enableBackfaceCulling);
        
        patchesAfterFace[i] = visiblePatches.size();
        
        if (shouldLogFrame) {
            size_t patchesAdded = patchesAfterFace[i] - patchesBeforeFace[i];
            std::cout << "[DEBUG] Face " << i << " added " << patchesAdded << " patches" << std::endl;
        }
    }
    
    // DEBUG: Show detailed breakdown
    if (shouldLogFrame) {
        std::cout << "[DEBUG] Total patches: " << visiblePatches.size() << std::endl;
        std::cout << "[DEBUG] Per face breakdown:" << std::endl;
        int totalVisible = 0;
        for (int i = 0; i < 6; i++) {
            size_t count = patchesAfterFace[i] - patchesBeforeFace[i];
            std::cout << "  Face " << i << ": " << count << " patches";
            if (count == 0) {
                std::cout << " [WARNING: No patches!]";
            }
            std::cout << std::endl;
            totalVisible += (count > 0) ? 1 : 0;
        }
        if (totalVisible < 3) {
            std::cout << "[WARNING] Only " << totalVisible << " faces have patches! This might cause missing sectors!" << std::endl;
        }
    }
    
    stats.visibleNodes = static_cast<uint32_t>(visiblePatches.size());
    
    // Perform actual subdivision/merge operations
    // ONLY do this when camera has moved or on first frame!
    if (shouldUpdateLOD) {
        // We need to do this in a second pass to avoid modifying the tree while traversing
        // IMPORTANT: Give each face a fair share of subdivisions to prevent imbalance
        // Negative faces need more aggressive subdivision to match positive faces
        // BALANCE: Need enough subdivisions to cover the face properly
        // For level 4: need 4+16+64 = 84 subdivisions minimum
        const int MAX_SUBDIVISIONS_PER_FACE = 100;  // Increased to allow proper coverage
        
        for (int i = 0; i < 6; i++) {
            auto& root = roots[i];
            
            // For negative faces (odd indices), use lower error threshold to force more subdivision
            float faceErrorThreshold = errorThreshold;
            if (i % 2 == 1) {  // Negative faces: 1, 3, 5
                faceErrorThreshold *= 0.5f;  // Half threshold = more aggressive subdivision
                if (shouldLog) {
                    std::cout << "[DEBUG] Face " << i << " using reduced threshold: " 
                              << faceErrorThreshold << std::endl;
                }
            }
            
            // Reset subdivision counter for each face to ensure fairness
            performSubdivisionsForFace(root.get(), viewPos, viewProj, faceErrorThreshold, 
                                       config.maxLevel, MAX_SUBDIVISIONS_PER_FACE);
        }
        
        // Also perform merges when camera moves away
        performMerges(viewPos, viewProj, errorThreshold);
    }
    
    // Clear and re-select after subdivisions
    visiblePatches.clear();
    for (int i = 0; i < 6; i++) {
        auto& root = roots[i];
        
        // Check visibility again
        glm::vec3 faceNormal;
        switch (i) {
            case 0: faceNormal = glm::vec3(1, 0, 0); break;
            case 1: faceNormal = glm::vec3(-1, 0, 0); break;
            case 2: faceNormal = glm::vec3(0, 1, 0); break;
            case 3: faceNormal = glm::vec3(0, -1, 0); break;
            case 4: faceNormal = glm::vec3(0, 0, 1); break;
            case 5: faceNormal = glm::vec3(0, 0, -1); break;
        }
        
        // Use same culling logic as first pass - CHECK CONFIG FLAG!
        bool shouldCull = config.enableFaceCulling ? 
            math::shouldCullFace(i, viewPos, config.planetRadius) : false;
        if (shouldCull) {
            continue;
        }
        
        root->selectLOD(viewPos, viewProj, errorThreshold, config.maxLevel, visiblePatches, config.enableBackfaceCulling);
    }
    
    stats.visibleNodes = static_cast<uint32_t>(visiblePatches.size());
    
    // PERFORMANCE: Disabled high patch count warning
    // if (visiblePatches.size() > 8000) {
    //     std::cout << "[WARNING] High patch count: " << visiblePatches.size() 
    //               << " patches at altitude " << (glm::length(viewPos) - config.planetRadius) 
    //               << "m" << std::endl;
    // }
    
    auto lodTime = std::chrono::high_resolution_clock::now();
    stats.lodSelectionTime = std::chrono::duration<float, std::milli>(lodTime - startTime).count();
    
    // Prevent cracks at T-junctions
    if (config.enableCrackFixes) {
        preventCracks(visiblePatches);
    }
    
    // Update morph factors for smooth transitions
    if (config.enableMorphing) {
        updateMorphFactors(visiblePatches, deltaTime);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    stats.morphUpdateTime = std::chrono::duration<float, std::milli>(endTime - lodTime).count();
    
    stats.totalNodes = totalNodeCount.load();
}

float SphericalQuadtree::calculateErrorThreshold(const glm::vec3& viewPos) const {
    float altitude = glm::length(viewPos) - config.planetRadius;
    
    // Clamp altitude to prevent negative values during transitions
    altitude = std::max(altitude, 100.0f);
    
    // Use the tested math function with logging
    float threshold = math::calculateLODThreshold(
        static_cast<double>(altitude),
        static_cast<double>(config.planetRadius)
    );
    
    // BALANCE: Allow subdivision but prevent over-subdivision at close range
    // This minimum prevents excessive patches when very close to surface
    threshold = std::max(threshold, 1.0f);  // Allow detail down to 1.0 threshold
    
    // PERFORMANCE: Disabled altitude threshold logging
    // static float lastLoggedAltitude = -1.0f;
    // if (std::abs(altitude - lastLoggedAltitude) > altitude * 0.1f) {  // Log when altitude changes by 10%
    //     std::cout << "[Quadtree::ErrorThreshold] Altitude: " << altitude/1000.0f 
    //               << "km -> Threshold: " << threshold << std::endl;
    //     lastLoggedAltitude = altitude;
    // }
    
    return threshold;
}

void SphericalQuadtree::performSubdivisionsForFace(SphericalQuadtreeNode* node,
                                                   const glm::vec3& viewPos,
                                                   const glm::mat4& viewProj,
                                                   float errorThreshold,
                                                   uint32_t maxLevel,
                                                   int maxSubdivisions) {
    if (!node) return;
    
    // Track subdivisions for this face only - NO STATICS!
    int subdivisionsThisFace = 0;
    
    performSubdivisionsRecursive(node, viewPos, viewProj, errorThreshold, 
                                maxLevel, subdivisionsThisFace, maxSubdivisions);
    
    // PERFORMANCE: Disabled face subdivision logging
    // static int faceLogCount = 0;
    // if (faceLogCount++ < 12) {  // Log first 2 frames (6 faces each)
    //     std::cout << "[FACE SUBDIVISIONS] Face " << node->getFace() 
    //               << " performed " << subdivisionsThisFace 
    //               << " subdivisions (max: " << maxSubdivisions << ")" << std::endl;
    // }
}

void SphericalQuadtree::performSubdivisionsRecursive(SphericalQuadtreeNode* node,
                                                     const glm::vec3& viewPos,
                                                     const glm::mat4& viewProj,
                                                     float errorThreshold,
                                                     uint32_t maxLevel,
                                                     int& subdivisionCount,
                                                     int maxSubdivisions) {
    if (!node || subdivisionCount >= maxSubdivisions) return;
    
    float error = node->calculateScreenSpaceError(viewPos, viewProj);
    bool shouldSubdivide = error > errorThreshold && node->getLevel() < maxLevel;
    
    if (shouldSubdivide && node->isLeaf()) {
        // Check if we've hit our subdivision budget
        if (subdivisionCount >= maxSubdivisions) {
            return; // Stop subdividing this face
        }
        
        // Subdivide this node
        // PERFORMANCE: Disabled subdivision debug logging
        // static int debugSubdivCount = 0;
        // if (debugSubdivCount++ < 50) {
        //     std::cout << "[SUBDIVIDE] Face " << node->getFace() 
        //               << " Level " << node->getLevel() 
        //               << " Center: " << node->getPatch().center.x << "," 
        //               << node->getPatch().center.y << "," 
        //               << node->getPatch().center.z << std::endl;
        // }
        node->subdivide(*densityField);
        stats.subdivisions++;
        subdivisionCount++;
    }
    
    // Recurse to children if they exist
    if (!node->isLeaf()) {
        for (int i = 0; i < 4; i++) {
            if (node->children[i]) {
                performSubdivisionsRecursive(node->children[i].get(), viewPos, viewProj, 
                                           errorThreshold, maxLevel, subdivisionCount, maxSubdivisions);
            }
        }
    }
}

void SphericalQuadtree::preventCracks(std::vector<QuadtreePatch>& patches) {
    // T-junction prevention
    // This requires checking each patch's neighbors and adjusting edge vertices
    // Complex implementation - simplified for now
    
    for (auto& patch : patches) {
        for (int edge = 0; edge < 4; edge++) {
            if (patch.neighbors[edge] && patch.neighborLevels[edge] < patch.level) {
                // This edge needs special handling to prevent cracks
                // Mark for edge morphing in shader
            }
        }
    }
}

void SphericalQuadtree::performMerges(const glm::vec3& viewPos, const glm::mat4& viewProj, float errorThreshold) {
    // Merge nodes that no longer need subdivision
    // Use 90% of subdivision threshold for merging to minimize hysteresis
    // (subdivide at error > threshold, merge at error < 0.9 * threshold)
    float mergeThreshold = errorThreshold * 0.9f;
    
    static int mergeCount = 0;
    if (mergeCount++ % 100 == 0) {
        std::cout << "[MERGE] Checking merges with threshold: " << mergeThreshold << std::endl;
    }
    
    for (int i = 0; i < 6; i++) {
        if (roots[i]) {
            performMergesRecursive(roots[i].get(), viewPos, viewProj, mergeThreshold);
        }
    }
}

void SphericalQuadtree::performMergesRecursive(SphericalQuadtreeNode* node, 
                                                const glm::vec3& viewPos,
                                                const glm::mat4& viewProj,
                                                float mergeThreshold) {
    if (!node || node->isLeaf()) return;
    
    // NEVER merge below level 1 - we maintain 24 base patches minimum
    if (node->getLevel() == 0) {
        // Still recurse to children to check their merges
        for (int i = 0; i < 4; i++) {
            if (node->children[i]) {
                performMergesRecursive(node->children[i].get(), viewPos, viewProj, mergeThreshold);
            }
        }
        return;
    }
    
    // Check if all children are leaves and have low error
    bool canMerge = true;
    for (int i = 0; i < 4; i++) {
        if (!node->children[i] || !node->children[i]->isLeaf()) {
            canMerge = false;
            break;
        }
        
        float childError = node->children[i]->calculateScreenSpaceError(viewPos, viewProj);
        if (childError > mergeThreshold) {
            canMerge = false;
            break;
        }
    }
    
    if (canMerge) {
        node->merge();
        stats.merges++;
    } else {
        // Recurse to children
        for (int i = 0; i < 4; i++) {
            if (node->children[i]) {
                performMergesRecursive(node->children[i].get(), viewPos, viewProj, mergeThreshold);
            }
        }
    }
}

void SphericalQuadtree::updateMorphFactors(std::vector<QuadtreePatch>& patches, float deltaTime) {
    float morphSpeed = 2.0f; // Morph transition speed
    
    for (auto& patch : patches) {
        // Smooth morph factor transitions
        float targetMorph = patch.morphFactor;
        float currentMorph = patch.morphFactor;
        
        if (std::abs(targetMorph - currentMorph) > 0.001f) {
            float delta = (targetMorph - currentMorph) * morphSpeed * deltaTime;
            patch.morphFactor = currentMorph + delta;
        }
    }
}

bool SphericalQuadtree::shouldUseOctree(float altitude) const {
    return altitude < 500.0f; // Use octree below 500m altitude
}

float SphericalQuadtree::getTransitionBlendFactor(float altitude) const {
    if (altitude > 1000.0f) return 0.0f; // Pure quadtree
    if (altitude < 500.0f) return 1.0f;  // Pure octree
    
    // Smooth transition between 500m and 1km
    return 1.0f - (altitude - 500.0f) / 500.0f;
}

} // namespace core