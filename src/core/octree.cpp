#include "core/octree.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

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
    : radius(radius), maxDepth(std::min(maxDepth, 12)) {  // Increased cap to depth 12 for higher fidelity
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
    if (node->isLeaf()) {
        // DEBUG: Track material setting, focus on surface nodes
        static int leafCount = 0;
        leafCount++;
        float nodeDist = glm::length(node->center);
        bool nearSurface = (nodeDist > radius * 0.8f && nodeDist < radius * 1.2f);
        static int surfaceDebugCount = 0;
        if (nearSurface && surfaceDebugCount++ < 10) {
            std::cout << "Setting materials for leaf at depth " << depth 
                      << " center=(" << node->center.x << "," << node->center.y 
                      << "," << node->center.z << ") size=" << node->halfSize << std::endl;
        }
        
        // Set material based on distance from center
        for (int i = 0; i < OctreeNode::LEAF_VOXELS; i++) {
            auto& voxel = node->voxels[i];
            
            // Get actual voxel position for more accurate material assignment
            glm::vec3 voxelOffset(
                (i & 1) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                (i & 2) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                (i & 4) ? node->halfSize * 0.5f : -node->halfSize * 0.5f
            );
            glm::vec3 voxelPos = node->center + voxelOffset;
            float voxelDist = glm::length(voxelPos);
            
            // DEBUG: First voxel of surface leaves
            if (nearSurface && surfaceDebugCount <= 3 && i == 0) {
                std::cout << "    Node center=(" << node->center.x << "," << node->center.y 
                          << "," << node->center.z << ") halfSize=" << node->halfSize << std::endl;
                std::cout << "    Voxel offset=(" << voxelOffset.x << "," << voxelOffset.y 
                          << "," << voxelOffset.z << ")" << std::endl;
                std::cout << "    Voxel pos=(" << voxelPos.x << "," << voxelPos.y 
                          << "," << voxelPos.z << ") dist=" << voxelDist << std::endl;
                std::cout << "    Planet radius=" << radius << std::endl;
            }
            
            // Mixed material assignment
            if (voxelDist < radius * 0.9f) {
                // Core/mantle - pure rock
                voxel = MixedVoxel::createPure(core::MaterialID::Rock);
                voxel.temperature = 255;  // Hot core (normalized)
                voxel.pressure = 255;     // High pressure
                if (surfaceDebugCount++ < 5) {
                    std::cout << "    Set ROCK for voxel at dist=" << voxelDist 
                              << " (inside planet core)\n";
                }
            } else if (voxelDist < radius) {
                // Use simple height-based distribution for testing
                // This ensures we get both water and land at all scales
                glm::vec3 sphereNormal = glm::normalize(voxelPos);
                
                // Simple latitude-based variation
                float latitude = asin(sphereNormal.y);
                
                // Add some longitude variation
                float longitude = atan2(sphereNormal.x, sphereNormal.z);
                
                // Create continent pattern using trigonometric functions
                // This works at all scales since it's based on angles, not absolute position
                float pattern1 = sin(longitude * 3.0f) * cos(latitude * 2.0f);
                float pattern2 = sin(longitude * 5.0f + 1.0f) * cos(latitude * 4.0f);
                
                // Combine patterns
                float continentValue = pattern1 * 0.6f + pattern2 * 0.4f;
                
                // Normalize to 0-1 range
                continentValue = (continentValue + 1.0f) * 0.5f;
                
                // DEBUG: Track continent value distribution
                static int debugCount = 0;
                static float minContinent = 999.0f;
                static float maxContinent = -999.0f;
                if (continentValue < minContinent) minContinent = continentValue;
                if (continentValue > maxContinent) maxContinent = continentValue;
                if (debugCount++ < 100) {
                    if (debugCount == 100) {
                        std::cout << "  Continent value range: [" << minContinent << ", " << maxContinent << "]" << std::endl;
                    }
                }
                
                // 30% land, 70% ocean (Earth-like ratio)
                // Use mixed materials for more realistic boundaries
                if (continentValue > 0.8f) {
                    // Mountain peaks - pure rock
                    voxel = MixedVoxel::createPure(core::MaterialID::Rock);
                    voxel.temperature = 128;  // Temperate
                } else if (continentValue > 0.7f) {
                    // Coastline - mixed rock and water
                    float blend = (continentValue - 0.7f) * 10.0f;  // 0 to 1
                    uint8_t rockAmt = static_cast<uint8_t>(128 + 127 * blend);
                    uint8_t waterAmt = static_cast<uint8_t>(127 - 127 * blend);
                    voxel = MixedVoxel::createMix(
                        core::MaterialID::Rock, rockAmt,
                        core::MaterialID::Water, waterAmt
                    );
                    voxel.temperature = 128;
                } else if (continentValue > 0.1f) {
                    // Ocean - pure water
                    voxel = MixedVoxel::createPure(core::MaterialID::Water);
                    voxel.temperature = 128;
                    if (surfaceDebugCount++ < 5) {
                        std::cout << "    Set WATER for voxel at dist=" << voxelDist 
                                  << " (ocean, continent=" << continentValue << ")\n";
                    }
                } else {
                    // Deep ocean trenches - water with some sediment
                    voxel = MixedVoxel::createMix(
                        core::MaterialID::Water, 200,
                        core::MaterialID::Sand, 55  // Sand as sediment
                    );
                    voxel.temperature = 120;  // Slightly cooler
                    voxel.pressure = 150;  // Higher pressure at depth
                }
            } else {
                // Space/atmosphere - pure air
                voxel = MixedVoxel::createPure(core::MaterialID::Air);
                voxel.temperature = 10;   // Cold space
                voxel.pressure = 0;       // No pressure
            }
        }
        
    }
}

void OctreePlanet::generate(uint32_t seed) {
    // Initialize random number generator with seed
    srand(seed);
    
    std::cout << "Generating sphere structure..." << std::endl;
    std::cout << "Planet radius: " << radius << " meters" << std::endl;
    std::cout << "Root node half-size: " << root->halfSize << " meters" << std::endl;
    
    // Create a more detailed test sphere
    if (root) {
        // Subdivide nodes near the surface for better detail
        generateTestSphere(root.get(), 0);
        
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
    
    std::cout << "Planet generation complete (simplified)" << std::endl;
    // Materials are already set during simplified generation via setMaterials()
}

// REMOVED FUNCTIONS:
// - generateSphere(): Duplicate functionality, materials set in setMaterials()
// - generateTerrain(): Not used, would add continent generation  
// - generatePlates(): Not used, would add tectonic plates
// - simulatePhysics(): Too expensive on CPU, needs GPU implementation
// - simulatePlates(): Not used, plate tectonics simulation

void OctreePlanet::update(float deltaTime) {
    // Physics simulation currently disabled for performance
    // With millions of nodes, traversing every frame is too expensive
    // Future: Move physics to GPU compute shaders
    return;
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
    
    // Traverse octree and collect visible nodes with hierarchical frustum culling
    std::function<bool(OctreeNode*, uint32_t)> collectNodes = [&](OctreeNode* node, uint32_t parentIndex) -> bool {
        nodesChecked++;
        
        // Hierarchical frustum culling - if parent is rejected, don't check children
        bool inFrustum = true;
        
        // For small test planets, disable frustum culling entirely
        if (radius >= 10000.0f) {
            // Always do frustum culling for internal nodes to enable hierarchical rejection
            if (!node->isLeaf()) {  // Check all internal nodes
                for (int i = 0; i < 6; i++) {
                    float distance = glm::dot(glm::vec3(frustumPlanes[i]), node->center) + frustumPlanes[i].w;
                    if (distance < -node->halfSize * 1.732f) { // sqrt(3) for diagonal
                        inFrustum = false;
                        nodesSkippedFrustum++;
                        break;
                    }
                }
                if (!inFrustum) return false;  // Early rejection - don't check children
            }
        }
        
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

float OctreePlanet::getDistanceFromSurface(const glm::vec3& position) const {
    return glm::length(position) - radius;
}

} // namespace octree