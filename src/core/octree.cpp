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
    // Initialize as leaf node with empty voxels
    for (auto& voxel : voxels) {
        voxel.material = MaterialType::Air;
        voxel.temperature = 0.0f;
        voxel.density = 0.0f;
        voxel.velocity = glm::vec3(0.0f);
        voxel.pressure = 0.0f;
        voxel.plateId = 0;
        voxel.stress = 0.0f;
        voxel.age = 0.0f;
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
    MaterialType commonMaterial = MaterialType::Air;
    float avgTemperature = 0.0f;
    float avgDensity = 0.0f;
    int validChildren = 0;
    
    for (const auto& child : children) {
        if (!child || !child->isLeaf()) {
            canSimplify = false;
            break;
        }
        
        // Check if all voxels in child are similar
        MaterialType childMaterial = child->voxels[0].material;
        for (const auto& voxel : child->voxels) {
            if (voxel.material != childMaterial) {
                canSimplify = false;
                break;
            }
            avgTemperature += voxel.temperature;
            avgDensity += voxel.density;
        }
        
        if (validChildren == 0) {
            commonMaterial = childMaterial;
        } else if (commonMaterial != childMaterial) {
            canSimplify = false;
            break;
        }
        
        validChildren++;
    }
    
    if (canSimplify && validChildren > 0) {
        // Merge children into this node
        avgTemperature /= (validChildren * LEAF_VOXELS);
        avgDensity /= (validChildren * LEAF_VOXELS);
        
        // Clear children
        for (auto& child : children) {
            child.reset();
        }
        
        // Set averaged voxel data
        for (auto& voxel : voxels) {
            voxel.material = commonMaterial;
            voxel.temperature = avgTemperature;
            voxel.density = avgDensity;
            // Keep other properties as defaults for now
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
        
        // Find the dominant material type by counting
        int materialCounts[4] = {0, 0, 0, 0}; // Air, Rock, Water, Magma
        for (const auto& voxel : voxels) {
            if (static_cast<int>(voxel.material) < 4) {
                materialCounts[static_cast<int>(voxel.material)]++;
            }
        }
        
        // Find most common material
        MaterialType dominantMaterial = MaterialType::Air;
        int maxCount = materialCounts[0];
        for (int i = 1; i < 4; i++) {
            if (materialCounts[i] > maxCount) {
                maxCount = materialCounts[i];
                dominantMaterial = static_cast<MaterialType>(i);
            }
        }
        
        gpuNode.flags |= (static_cast<uint32_t>(dominantMaterial) << 8);
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
    
    // Check if node intersects the sphere surface
    bool nearSurface = std::abs(distToCenter - radius) < nodeRadius;
    
    // Only subdivide near the surface for better performance
    // Use the actual maxDepth parameter for higher fidelity
    if (depth < 3 || (depth < maxDepth && nearSurface)) {
        node->subdivide();
        
        // Recursively subdivide children
        for (int i = 0; i < OctreeNode::OCTREE_CHILDREN; i++) {
            if (node->children[i]) {
                generateTestSphere(node->children[i].get(), depth + 1);
            }
        }
    } else if (node->isLeaf()) {
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
            
            // Simple material assignment
            if (voxelDist < radius * 0.9f) {
                voxel.material = MaterialType::Rock;  // Core/mantle
                voxel.temperature = 1500.0f;
                voxel.density = 3300.0f;
            } else if (voxelDist < radius) {
                // Generate continents using simple noise pattern
                glm::vec3 spherePos = glm::normalize(voxelPos);
                
                // Create continent patterns using trigonometric functions
                float lon = atan2(spherePos.x, spherePos.z);
                float lat = asin(spherePos.y);
                
                // Multiple frequency components for more interesting continents
                float continent1 = sin(lon * 2.0f) * cos(lat * 3.0f);
                float continent2 = sin(lon * 3.5f + 1.0f) * cos(lat * 2.5f + 0.5f);
                float continent3 = sin(lon * 1.5f - 0.7f) * cos(lat * 4.0f + 1.2f);
                
                // Combine different frequencies
                float continentValue = continent1 * 0.5f + continent2 * 0.3f + continent3 * 0.2f;
                
                // Add some randomness based on position
                float pseudoRandom = fmod(voxelPos.x * 12.9898f + voxelPos.y * 78.233f + voxelPos.z * 37.719f, 1.0f);
                continentValue += (pseudoRandom - 0.5f) * 0.3f;
                
                // 30% land, 70% ocean (Earth-like ratio)
                if (continentValue > 0.2f) {
                    voxel.material = MaterialType::Rock;  // Land/continent
                    voxel.temperature = 288.0f;
                    voxel.density = 2700.0f;
                } else {
                    voxel.material = MaterialType::Water;  // Ocean
                    voxel.temperature = 288.0f;
                    voxel.density = 1000.0f;
                }
            } else {
                voxel.material = MaterialType::Air;   // Space (no atmosphere yet)
                voxel.temperature = 3.0f;  // Near absolute zero in space
                voxel.density = 0.0f;
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
                    if (voxel.material != MaterialType::Air) {
                        surfaceNodes++;
                        break;
                    }
                }
            }
        });
        std::cout << "Generated " << nodeCount << " leaf nodes (" << surfaceNodes << " with surface material)" << std::endl;
    }
    
    std::cout << "Planet generation complete (simplified)" << std::endl;
    
    // Skip terrain and plates for now
    // generateSphere(root.get());
    // generateTerrain();
    // generatePlates();
}

void OctreePlanet::generateSphere(OctreeNode* node) {
    // Recursively subdivide nodes that intersect the planet surface
    float distToCenter = glm::length(node->center);
    float nodeRadius = node->halfSize * 1.732f; // sqrt(3) for diagonal
    
    // Check if node intersects planet
    bool intersectsSurface = std::abs(distToCenter - radius) < nodeRadius;
    bool insidePlanet = distToCenter + nodeRadius < radius;
    bool outsidePlanet = distToCenter - nodeRadius > radius;
    
    if (outsidePlanet) {
        // Node is completely outside planet - leave as air
        return;
    }
    
    if (node->level < maxDepth && (intersectsSurface || insidePlanet)) {
        // Subdivide if we haven't reached max depth
        node->subdivide();
        
        // Recursively generate children
        node->traverse([this](OctreeNode* child) {
            if (child->level > 0) { // Skip the root node itself
                generateSphere(child);
            }
        });
    } else if (node->isLeaf()) {
        // Set voxel materials based on position
        for (int i = 0; i < OctreeNode::LEAF_VOXELS; i++) {
            glm::vec3 voxelOffset(
                (i & 1) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                (i & 2) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                (i & 4) ? node->halfSize * 0.5f : -node->halfSize * 0.5f
            );
            glm::vec3 voxelPos = node->center + voxelOffset;
            
            float dist = glm::length(voxelPos);
            
            if (dist > radius) {
                node->voxels[i].material = MaterialType::Air;
                node->voxels[i].density = 0.0f;
            } else if (dist > radius * 0.95f) {
                // Surface layer - can be water or land
                node->voxels[i].material = (rand() % 100 < 70) ? MaterialType::Water : MaterialType::Rock;
                node->voxels[i].density = (node->voxels[i].material == MaterialType::Water) ? 1000.0f : 2700.0f;
                node->voxels[i].temperature = 288.0f; // ~15°C
            } else if (dist > radius * 0.5f) {
                // Mantle
                node->voxels[i].material = MaterialType::Rock;
                node->voxels[i].density = 3300.0f + (radius - dist) / radius * 2000.0f;
                node->voxels[i].temperature = 1000.0f + (radius - dist) / radius * 3000.0f;
            } else {
                // Core
                node->voxels[i].material = MaterialType::Magma;
                node->voxels[i].density = 10000.0f + (radius - dist) / radius * 3000.0f;
                node->voxels[i].temperature = 4000.0f + (radius - dist) / radius * 2000.0f;
            }
        }
    }
}

void OctreePlanet::generateTerrain() {
    // Add terrain features using noise functions
    // For now, just add some basic elevation variation
    
    root->traverse([this](OctreeNode* node) {
        if (node->isLeaf()) {
            for (int i = 0; i < OctreeNode::LEAF_VOXELS; i++) {
                if (node->voxels[i].material == MaterialType::Rock ||
                    node->voxels[i].material == MaterialType::Water) {
                    
                    glm::vec3 voxelOffset(
                        (i & 1) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                        (i & 2) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                        (i & 4) ? node->halfSize * 0.5f : -node->halfSize * 0.5f
                    );
                    glm::vec3 voxelPos = node->center + voxelOffset;
                    
                    // Simple height variation based on position
                    float longitude = atan2(voxelPos.x, voxelPos.z);
                    float latitude = asin(voxelPos.y / glm::length(voxelPos));
                    
                    // Create some continental masses
                    float continentNoise = sin(longitude * 3.0f) * cos(latitude * 2.0f);
                    
                    if (continentNoise > 0.3f && node->voxels[i].material == MaterialType::Water) {
                        node->voxels[i].material = MaterialType::Rock;
                        node->voxels[i].density = 2700.0f;
                    }
                }
            }
        }
    });
}

void OctreePlanet::generatePlates() {
    // Generate tectonic plates
    const int numPlates = 7 + rand() % 5; // 7-11 plates
    
    plates.clear();
    plates.reserve(numPlates);
    
    for (int i = 0; i < numPlates; i++) {
        Plate plate;
        plate.id = i + 1;
        
        // Random plate velocity (very slow)
        float speed = (rand() / float(RAND_MAX)) * 0.00001f; // meters per second
        float angle = (rand() / float(RAND_MAX)) * 2.0f * 3.14159f;
        plate.velocity = glm::vec3(
            cos(angle) * speed,
            0.0f,
            sin(angle) * speed
        );
        
        // Random density (oceanic plates are denser)
        plate.oceanic = (rand() % 100) < 40; // 40% oceanic
        plate.density = plate.oceanic ? 3000.0f : 2700.0f;
        
        plates.push_back(plate);
    }
    
    // Assign plates to surface voxels
    root->traverse([this, numPlates](OctreeNode* node) {
        if (node->isLeaf()) {
            for (int i = 0; i < OctreeNode::LEAF_VOXELS; i++) {
                if (node->voxels[i].material == MaterialType::Rock ||
                    node->voxels[i].material == MaterialType::Water) {
                    
                    glm::vec3 voxelOffset(
                        (i & 1) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                        (i & 2) ? node->halfSize * 0.5f : -node->halfSize * 0.5f,
                        (i & 4) ? node->halfSize * 0.5f : -node->halfSize * 0.5f
                    );
                    glm::vec3 voxelPos = node->center + voxelOffset;
                    
                    // Simple plate assignment based on position
                    float angle = atan2(voxelPos.x, voxelPos.z);
                    int plateIndex = int((angle + 3.14159f) / (2.0f * 3.14159f) * numPlates) % numPlates;
                    
                    node->voxels[i].plateId = plateIndex + 1;
                }
            }
        }
    });
}

void OctreePlanet::update(float deltaTime) {
    // TODO: Optimize physics - currently disabled as it's too slow with millions of nodes
    // Traversing 6+ million nodes every frame kills performance
    return;
    
    // Update physics simulation
    simulatePhysics(deltaTime);
    
    // Update plate tectonics
    simulatePlates(deltaTime);
    
    // Update ages
    root->traverse([deltaTime](OctreeNode* node) {
        if (node->isLeaf()) {
            for (auto& voxel : node->voxels) {
                voxel.age += deltaTime;
            }
        }
    });
}

void OctreePlanet::simulatePhysics(float deltaTime) {
    // Simple temperature diffusion for now
    // In a real implementation, this would be done on GPU
    
    root->traverse([deltaTime](OctreeNode* node) {
        if (node->isLeaf()) {
            // Simple heat diffusion between voxels
            float diffusionRate = 0.01f * deltaTime;
            
            float avgTemp = 0.0f;
            int count = 0;
            for (const auto& voxel : node->voxels) {
                if (voxel.material != MaterialType::Air) {
                    avgTemp += voxel.temperature;
                    count++;
                }
            }
            
            if (count > 0) {
                avgTemp /= count;
                
                // Apply diffusion
                for (auto& voxel : node->voxels) {
                    if (voxel.material != MaterialType::Air) {
                        voxel.temperature += (avgTemp - voxel.temperature) * diffusionRate;
                    }
                }
            }
        }
    });
}

void OctreePlanet::simulatePlates(float deltaTime) {
    // Move plates based on their velocities
    // This is a very simplified plate tectonics simulation
    
    for (auto& plate : plates) {
        // Update plate properties over time
        // In reality, this would involve complex mantle convection calculations
        
        // Add some random drift to plate velocities
        float driftAmount = 0.000001f * deltaTime;
        plate.velocity.x += (rand() / float(RAND_MAX) - 0.5f) * driftAmount;
        plate.velocity.z += (rand() / float(RAND_MAX) - 0.5f) * driftAmount;
        
        // Limit velocity
        float speed = glm::length(plate.velocity);
        if (speed > 0.0001f) {
            plate.velocity = glm::normalize(plate.velocity) * 0.0001f;
        }
    }
}

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
    
    // Traverse octree and collect visible nodes with hierarchical frustum culling
    std::function<bool(OctreeNode*, uint32_t)> collectNodes = [&](OctreeNode* node, uint32_t parentIndex) -> bool {
        // Hierarchical frustum culling - if parent is rejected, don't check children
        bool inFrustum = true;
        
        // Only do frustum culling for non-leaf nodes to enable early rejection
        if (!node->isLeaf() || node->halfSize > 1000.0f) {  // Check internal nodes and large leaves
            for (int i = 0; i < 6; i++) {
                float distance = glm::dot(glm::vec3(frustumPlanes[i]), node->center) + frustumPlanes[i].w;
                if (distance < -node->halfSize * 1.732f) { // sqrt(3) for diagonal
                    inFrustum = false;
                    break;
                }
            }
            if (!inFrustum) return false;  // Early rejection - don't check children
        }
        
        // Only process leaf nodes for rendering
        if (node->isLeaf()) {
            // Check if this node contains non-air material
            bool hasNonAir = false;
            for (const auto& voxel : node->voxels) {
                if (voxel.material != MaterialType::Air) {
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
    
    return data;
}

void OctreePlanet::updateLOD(const glm::vec3& viewPos) {
    // TODO: Optimize LOD update - currently disabled as it's too slow with millions of nodes
    return;
    
    // Update LOD by subdividing or simplifying nodes based on distance
    
    std::function<void(OctreeNode*)> updateNode = [&](OctreeNode* node) {
        if (node->shouldSubdivide(viewPos)) {
            node->subdivide();
            
            // Initialize new children with appropriate data
            if (!node->isLeaf()) {
                for (auto& child : node->children) {
                    if (child) {
                        generateSphere(child.get());
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
}

Voxel* OctreePlanet::getVoxel(const glm::vec3& position) {
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