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
    
    // Log patch details for debugging
    static int errorLogCount = 0;
    if (errorLogCount++ % 500 == 0 || error > 1000.0f) {
        std::cout << "[Node::ScreenError] Level:" << level 
                  << " Center:" << math::toString(glm::dvec3(patch.center))
                  << " Size:" << patch.size
                  << " Error:" << error << std::endl;
    }
    
    return error;
}

void SphericalQuadtreeNode::selectLOD(const glm::vec3& viewPos, const glm::mat4& viewProj,
                                      float errorThreshold, uint32_t maxLevel,
                                      std::vector<QuadtreePatch>& visiblePatches) {
    
    // CRITICAL FIX 1: Hard limit on recursion depth
    const uint32_t ABSOLUTE_MAX_LEVEL = 15;  // Proven sufficient by our tests
    if (level >= ABSOLUTE_MAX_LEVEL) {
        if (isLeaf()) {
            patch.isVisible = true;
            visiblePatches.push_back(patch);
        }
        return;  // Stop recursion immediately
    }
    
    // Safety check - prevent buffer overflow (increased limit for close-up views)
    if (visiblePatches.size() >= 20000) {  // Higher limit to support detailed close views
        // Too many patches, just render current level without subdividing
        if (isLeaf()) {
            patch.isVisible = true;
            visiblePatches.push_back(patch);
        }
        std::cout << "[SelectLOD] WARNING: Hit patch limit at 20000 patches" << std::endl;
        return;
    }
    
    // Basic frustum culling - could be improved
    // For now, assume all nodes are potentially visible
    
    float error = calculateScreenSpaceError(viewPos, viewProj);
    patch.screenSpaceError = error;
    
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
                    child->selectLOD(viewPos, viewProj, errorThreshold, maxLevel, visiblePatches);
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
    std::cout << "[SphericalQuadtree] Creating with planet radius: " << config.planetRadius << std::endl;
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
    
    // Set up neighbor relationships between root faces
    // This is complex - each face has specific neighbors at edges
    // Simplified for now
}

void SphericalQuadtree::update(const glm::vec3& viewPos, const glm::mat4& viewProj, float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    static int updateCount = 0;
    if (updateCount++ % 60 == 0) {
        std::cout << "[SphericalQuadtree::update] ViewPos: " << glm::length(viewPos) 
                  << ", Roots: " << roots.size() << std::endl;
    }
    
    // Clear visible patches from last frame
    visiblePatches.clear();
    stats.visibleNodes = 0;
    stats.subdivisions = 0;
    stats.merges = 0;
    
    // Safety limit - prevent excessive patches
    const size_t MAX_PATCHES = 50000;  // Increased limit for close-up detail
    
    // Calculate error threshold based on view distance
    float errorThreshold = calculateErrorThreshold(viewPos);
    
    // Dynamic adjustment - if we're getting too many patches, increase threshold
    static int highPatchFrames = 0;
    if (visiblePatches.size() > 5000) {
        highPatchFrames++;
        if (highPatchFrames > 10) {
            // Sustained high patch count, increase threshold
            errorThreshold *= 1.5f;
            std::cout << "[SphericalQuadtree] Adjusting error threshold to " << errorThreshold 
                      << " due to high patch count (" << visiblePatches.size() << ")" << std::endl;
        }
    } else {
        highPatchFrames = 0;
    }
    
    // Select LOD for each root face - but only if the face is visible
    static int frameCount = 0;
    bool shouldLog = (frameCount++ % 120 == 0);  // Log every 2 seconds at 60fps
    
    if (shouldLog) {
        std::cout << "[DEBUG] Processing cube faces. enableFaceCulling=" 
                  << config.enableFaceCulling << std::endl;
    }
    
    for (int i = 0; i < 6; i++) {
        auto& root = roots[i];
        
        if (!root) {
            if (shouldLog) std::cout << "[DEBUG] Face " << i << " - root is null!" << std::endl;
            continue;
        }
        
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
            if (shouldLog) {
                std::cout << "[DEBUG] Face " << i << " - CULLED" << std::endl;
            }
            continue; // Face is on the far side of the planet
        }
        
        if (shouldLog) {
            std::cout << "[DEBUG] Face " << i << " - PROCESSING" << std::endl;
        }
        root->selectLOD(viewPos, viewProj, errorThreshold, config.maxLevel, visiblePatches);
    }
    
    stats.visibleNodes = static_cast<uint32_t>(visiblePatches.size());
    
    // Perform actual subdivision/merge operations
    // We need to do this in a second pass to avoid modifying the tree while traversing
    // IMPORTANT: Give each face a fair share of subdivisions to prevent imbalance
    // Negative faces need more aggressive subdivision to match positive faces
    const int MAX_SUBDIVISIONS_PER_FACE = 200;  // Each face gets its own budget
    
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
        
        root->selectLOD(viewPos, viewProj, errorThreshold, config.maxLevel, visiblePatches);
    }
    
    stats.visibleNodes = static_cast<uint32_t>(visiblePatches.size());
    
    // Warn if approaching patch limit
    if (visiblePatches.size() > 8000) {
        std::cout << "[WARNING] High patch count: " << visiblePatches.size() 
                  << " patches at altitude " << (glm::length(viewPos) - config.planetRadius) 
                  << "m" << std::endl;
    }
    
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
    
    // Additional logging for debugging
    static float lastLoggedAltitude = -1.0f;
    if (std::abs(altitude - lastLoggedAltitude) > altitude * 0.1f) {  // Log when altitude changes by 10%
        std::cout << "[Quadtree::ErrorThreshold] Altitude: " << altitude/1000.0f 
                  << "km -> Threshold: " << threshold << std::endl;
        lastLoggedAltitude = altitude;
    }
    
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
    
    // Log how many subdivisions this face got
    static int faceLogCount = 0;
    if (faceLogCount++ < 12) {  // Log first 2 frames (6 faces each)
        std::cout << "[FACE SUBDIVISIONS] Face " << node->getFace() 
                  << " performed " << subdivisionsThisFace 
                  << " subdivisions (max: " << maxSubdivisions << ")" << std::endl;
    }
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
        // Debug: log which face is subdividing
        static int debugSubdivCount = 0;
        if (debugSubdivCount++ < 50) {
            std::cout << "[SUBDIVIDE] Face " << node->getFace() 
                      << " Level " << node->getLevel() 
                      << " Center: " << node->getPatch().center.x << "," 
                      << node->getPatch().center.y << "," 
                      << node->getPatch().center.z << std::endl;
        }
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