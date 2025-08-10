#include "core/spherical_quadtree.hpp"
#include "core/camera.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace core {

// SphericalQuadtreeNode implementation
SphericalQuadtreeNode::SphericalQuadtreeNode(const glm::vec3& center, float size, 
                                             uint32_t level, Face face, 
                                             SphericalQuadtreeNode* parent)
    : parent(parent), level(level), face(face) {
    
    patch.center = center;
    patch.size = size;
    patch.level = level;
    patch.morphFactor = 0.0f;
    patch.screenSpaceError = 0.0f;
    
    // Calculate corner positions
    float halfSize = size * 0.5f;
    glm::vec3 up, right;
    
    // Determine local coordinate system based on face
    switch (face) {
        case FACE_POS_X:
            up = glm::vec3(0, 1, 0);
            right = glm::vec3(0, 0, 1);
            break;
        case FACE_NEG_X:
            up = glm::vec3(0, 1, 0);
            right = glm::vec3(0, 0, -1);
            break;
        case FACE_POS_Y:
            up = glm::vec3(0, 0, 1);
            right = glm::vec3(1, 0, 0);
            break;
        case FACE_NEG_Y:
            up = glm::vec3(0, 0, -1);
            right = glm::vec3(1, 0, 0);
            break;
        case FACE_POS_Z:
            up = glm::vec3(0, 1, 0);
            right = glm::vec3(-1, 0, 0);  // Reversed to match winding of other faces
            break;
        case FACE_NEG_Z:
            up = glm::vec3(0, 1, 0);
            right = glm::vec3(1, 0, 0);   // Reversed to match winding of other faces
            break;
    }
    
    // Set corner positions (in cube space, will be projected to sphere)
    patch.corners[0] = center + (-right - up) * halfSize; // Bottom-left
    patch.corners[1] = center + (right - up) * halfSize;  // Bottom-right
    patch.corners[2] = center + (right + up) * halfSize;  // Top-right
    patch.corners[3] = center + (-right + up) * halfSize; // Top-left
    
    // Clear neighbors
    for (int i = 0; i < 4; i++) {
        neighbors[i] = nullptr;
    }
}

glm::vec3 SphericalQuadtreeNode::cubeToSphere(const glm::vec3& cubePos, float radius) const {
    // Project cube position to sphere using gnomonic projection
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos = cubePos * glm::sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
    spherePos.y *= glm::sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
    spherePos.z *= glm::sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
    
    return glm::normalize(spherePos) * radius;
}

void SphericalQuadtreeNode::subdivide(const DensityField& densityField) {
    if (!isLeaf()) return;
    
    float childSize = patch.size * 0.5f;
    
    for (int i = 0; i < 4; i++) {
        glm::vec3 childCenter = getChildCenter(i);
        children[i] = std::make_unique<SphericalQuadtreeNode>(
            childCenter, childSize, level + 1, face, this
        );
    }
    
    updateNeighborReferences();
}

glm::vec3 SphericalQuadtreeNode::getChildCenter(int childIndex) const {
    float quarterSize = patch.size * 0.25f;
    glm::vec3 offset;
    
    switch (childIndex) {
        case 0: offset = glm::vec3(-quarterSize, -quarterSize, 0); break; // Bottom-left
        case 1: offset = glm::vec3(quarterSize, -quarterSize, 0); break;  // Bottom-right
        case 2: offset = glm::vec3(quarterSize, quarterSize, 0); break;   // Top-right
        case 3: offset = glm::vec3(-quarterSize, quarterSize, 0); break;  // Top-left
    }
    
    // Transform offset based on face orientation
    glm::vec3 up, right;
    switch (face) {
        case FACE_POS_X:
            return patch.center + glm::vec3(0, offset.y, offset.x);
        case FACE_NEG_X:
            return patch.center + glm::vec3(0, offset.y, -offset.x);
        case FACE_POS_Y:
            return patch.center + glm::vec3(offset.x, 0, offset.y);
        case FACE_NEG_Y:
            return patch.center + glm::vec3(offset.x, 0, -offset.y);
        case FACE_POS_Z:
            return patch.center + glm::vec3(-offset.x, offset.y, 0);
        case FACE_NEG_Z:
            return patch.center + glm::vec3(offset.x, offset.y, 0);
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
    // Calculate distance to viewer
    float distance = glm::length(viewPos - patch.center);
    if (distance < 1.0f) distance = 1.0f;
    
    // Geometric error (size of features at this LOD level)
    float geometricError = patch.size * 0.1f; // Approximate feature size
    
    // Project to screen space
    glm::vec4 centerProj = viewProj * glm::vec4(patch.center, 1.0f);
    if (centerProj.w <= 0.0f) return 0.0f;
    
    glm::vec4 edgeProj = viewProj * glm::vec4(patch.center + glm::vec3(geometricError, 0, 0), 1.0f);
    if (edgeProj.w <= 0.0f) return 0.0f;
    
    // Calculate pixel error
    glm::vec2 centerScreen = glm::vec2(centerProj) / centerProj.w;
    glm::vec2 edgeScreen = glm::vec2(edgeProj) / edgeProj.w;
    
    float screenError = glm::length(edgeScreen - centerScreen) * 720.0f; // Assuming 720p height
    
    return screenError;
}

void SphericalQuadtreeNode::selectLOD(const glm::vec3& viewPos, const glm::mat4& viewProj,
                                      float errorThreshold, uint32_t maxLevel,
                                      std::vector<QuadtreePatch>& visiblePatches) {
    
    // Frustum culling would go here
    // For now, assume all nodes are potentially visible
    
    float error = calculateScreenSpaceError(viewPos, viewProj);
    patch.screenSpaceError = error;
    
    bool shouldSubdivide = error > errorThreshold && level < maxLevel;
    
    if (shouldSubdivide) {
        if (isLeaf()) {
            // Need to subdivide but haven't yet - mark for subdivision
            patch.needsUpdate = true;
            visiblePatches.push_back(patch);
        } else {
            // Recurse to children
            for (auto& child : children) {
                if (child) {
                    child->selectLOD(viewPos, viewProj, errorThreshold, maxLevel, visiblePatches);
                }
            }
        }
    } else {
        // This node is at appropriate detail level
        patch.isVisible = true;
        visiblePatches.push_back(patch);
        
        // If we have children but don't need them, could merge
        if (!isLeaf() && error < errorThreshold * 0.5f) {
            patch.needsUpdate = true; // Mark for potential merge
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
            
            // Interpolate position on patch
            glm::vec3 pos = patch.corners[0] * (1.0f - u) * (1.0f - v) +
                           patch.corners[1] * u * (1.0f - v) +
                           patch.corners[2] * u * v +
                           patch.corners[3] * (1.0f - u) * v;
            
            // Project to sphere and get height
            glm::vec3 spherePos = cubeToSphere(pos, densityField.getPlanetRadius());
            float height = densityField.getTerrainHeight(glm::normalize(spherePos));
            
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
    // Create six root nodes for cube faces
    roots[0] = std::make_unique<SphericalQuadtreeNode>(
        glm::vec3(1, 0, 0), 2.0f, 0, SphericalQuadtreeNode::FACE_POS_X);
    roots[1] = std::make_unique<SphericalQuadtreeNode>(
        glm::vec3(-1, 0, 0), 2.0f, 0, SphericalQuadtreeNode::FACE_NEG_X);
    roots[2] = std::make_unique<SphericalQuadtreeNode>(
        glm::vec3(0, 1, 0), 2.0f, 0, SphericalQuadtreeNode::FACE_POS_Y);
    roots[3] = std::make_unique<SphericalQuadtreeNode>(
        glm::vec3(0, -1, 0), 2.0f, 0, SphericalQuadtreeNode::FACE_NEG_Y);
    roots[4] = std::make_unique<SphericalQuadtreeNode>(
        glm::vec3(0, 0, 1), 2.0f, 0, SphericalQuadtreeNode::FACE_POS_Z);
    roots[5] = std::make_unique<SphericalQuadtreeNode>(
        glm::vec3(0, 0, -1), 2.0f, 0, SphericalQuadtreeNode::FACE_NEG_Z);
    
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
    
    // Calculate error threshold based on view distance
    float errorThreshold = calculateErrorThreshold(viewPos);
    
    // Select LOD for each root face - but only if the face is visible
    for (int i = 0; i < 6; i++) {
        auto& root = roots[i];
        
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
        
        // Check if face points toward camera
        glm::vec3 toCamera = glm::normalize(viewPos);
        float dot = glm::dot(faceNormal, toCamera);
        
        // If the face is pointing away from camera, skip it
        // Use a small threshold to avoid popping at edges
        if (dot < -0.1f) {
            continue; // Face is on the far side of the planet
        }
        
        root->selectLOD(viewPos, viewProj, errorThreshold, config.maxLevel, visiblePatches);
    }
    
    stats.visibleNodes = static_cast<uint32_t>(visiblePatches.size());
    
    // Perform actual subdivision/merge operations
    for (auto& patch : visiblePatches) {
        if (patch.needsUpdate) {
            // This would trigger actual subdivision/merge
            // For now just counting
            stats.subdivisions++;
        }
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
    
    // Adaptive error threshold based on altitude
    if (altitude > 100000.0f) {
        return config.pixelError * 4.0f; // Less detail from space
    } else if (altitude > 10000.0f) {
        return config.pixelError * 2.0f; // Medium detail
    } else {
        return config.pixelError; // Full detail near surface
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