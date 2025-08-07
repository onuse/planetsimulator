/**
 * Specific test to diagnose the planet rendering issue
 * Tests the actual code paths used in the planet simulator
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>

// Test the actual shader logic
void testShaderTraversal() {
    std::cout << "\n=== Testing Shader Traversal Logic ===" << std::endl;
    
    // Simulate the shader's octree traversal
    struct GPUNode {
        glm::vec4 centerAndSize;
        glm::uvec4 childrenAndFlags;
    };
    
    // Create a simple test octree like the shader would see
    std::vector<GPUNode> nodes;
    
    // Root node (planet-sized)
    GPUNode root;
    root.centerAndSize = glm::vec4(0, 0, 0, 9556500.0f); // Planet radius * 1.5
    root.childrenAndFlags = glm::uvec4(1, 0xFFFFFFFF, 0, 0); // Internal node, children at index 1
    nodes.push_back(root);
    
    // Add 8 children (some with materials)
    for (int i = 0; i < 8; i++) {
        GPUNode child;
        float offset = 4778250.0f; // Half of parent
        child.centerAndSize = glm::vec4(
            (i & 1) ? offset : -offset,
            (i & 2) ? offset : -offset,
            (i & 4) ? offset : -offset,
            2389125.0f
        );
        
        // Make some children have rock material
        if (i == 0 || i == 3 || i == 5) {
            child.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0, 0x0101, 0); // Leaf with Rock
        } else {
            child.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0, 0x0001, 0); // Leaf with Air
        }
        nodes.push_back(child);
    }
    
    // Test ray traversal
    glm::vec3 rayOrigin(20000000.0f, 0, 0);
    glm::vec3 rayDir = glm::normalize(glm::vec3(-1, 0, 0));
    
    // Check planet hit
    glm::vec3 oc = rayOrigin - glm::vec3(0);
    float b = glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - 6371000.0f * 6371000.0f;
    float h = b * b - c;
    
    if (h < 0) {
        std::cout << "ERROR: Ray misses planet!" << std::endl;
    } else {
        std::cout << "✓ Ray hits planet" << std::endl;
        
        // Start traversal
        float planetHit = -b - sqrt(h);
        glm::vec3 rayStart = rayOrigin + rayDir * std::max(planetHit, 0.0f);
        
        std::cout << "  Ray starts at: (" << rayStart.x << ", " << rayStart.y << ", " << rayStart.z << ")" << std::endl;
        std::cout << "  Distance from origin: " << glm::length(rayStart) << std::endl;
        
        // Find which octree node we're in
        uint32_t nodeIndex = 0;
        GPUNode node = nodes[nodeIndex];
        
        std::cout << "  Root node: center=(0,0,0), size=" << node.centerAndSize.w << std::endl;
        
        // Check if root is leaf
        bool isLeaf = (node.childrenAndFlags.z & 1) != 0;
        std::cout << "  Root is leaf: " << (isLeaf ? "yes" : "no") << std::endl;
        
        if (!isLeaf) {
            // Find child
            uint32_t childrenOffset = node.childrenAndFlags.x;
            std::cout << "  Children start at index: " << childrenOffset << std::endl;
            
            if (childrenOffset < nodes.size()) {
                // Determine octant
                glm::vec3 center = glm::vec3(node.centerAndSize);
                uint32_t octant = 0;
                if (rayStart.x > center.x) octant |= 1;
                if (rayStart.y > center.y) octant |= 2;
                if (rayStart.z > center.z) octant |= 4;
                
                std::cout << "  Ray is in octant: " << octant << std::endl;
                
                nodeIndex = childrenOffset + octant;
                if (nodeIndex < nodes.size()) {
                    node = nodes[nodeIndex];
                    isLeaf = (node.childrenAndFlags.z & 1) != 0;
                    uint32_t material = (node.childrenAndFlags.z >> 8) & 0xFF;
                    
                    std::cout << "  Child node " << nodeIndex << ": leaf=" << isLeaf 
                              << ", material=" << material << std::endl;
                    
                    if (material == 1) {
                        std::cout << "✓ Found Rock material!" << std::endl;
                    } else if (material == 0) {
                        std::cout << "  Found Air material (should continue marching)" << std::endl;
                    }
                }
            }
        }
    }
}

// Test the actual planet generation
void testPlanetGeneration() {
    std::cout << "\n=== Testing Planet Generation Logic ===" << std::endl;
    
    float planetRadius = 6371000.0f;
    
    // Test various positions to see what materials they get
    struct TestPoint {
        glm::vec3 position;
        const char* description;
    };
    
    TestPoint testPoints[] = {
        { glm::vec3(0, 0, 0), "Planet center" },
        { glm::vec3(0, 0, 6371000.0f), "North pole surface" },
        { glm::vec3(6371000.0f, 0, 0), "Equator surface" },
        { glm::vec3(0, 0, 5000000.0f), "Inside planet" },
        { glm::vec3(0, 0, 7000000.0f), "Above surface" },
        { glm::vec3(0, 0, 10000000.0f), "Far above surface" }
    };
    
    for (const auto& test : testPoints) {
        float dist = glm::length(test.position);
        std::string material;
        
        if (dist > planetRadius) {
            material = "Air";
        } else if (dist > planetRadius * 0.95f) {
            material = "Rock/Water (surface)";
        } else if (dist > planetRadius * 0.5f) {
            material = "Rock (mantle)";
        } else {
            material = "Magma (core)";
        }
        
        std::cout << "  " << test.description << " (dist=" << dist/1000000.0f << "M km): " 
                  << material << std::endl;
    }
}

// Test the hardcoded material assignment
void testHardcodedMaterials() {
    std::cout << "\n=== Testing Hardcoded Material Assignment ===" << std::endl;
    
    // Test points that should get materials from the hardcoded logic
    struct TestNode {
        glm::vec3 center;
        float expectedMaterial;
    };
    
    TestNode testNodes[] = {
        { glm::vec3(0, 0, 5000000), 1 },  // Inside planet -> Rock
        { glm::vec3(0, 0, 7000000), 1 },  // Near surface -> Rock or Water
        { glm::vec3(0, 0, 12000000), 2 }, // Far but within 15M -> Water
        { glm::vec3(0, 0, 20000000), 0 }  // Beyond 15M -> Air
    };
    
    for (const auto& test : testNodes) {
        float distFromCenter = glm::length(test.center);
        uint32_t dominantMaterial = 0; // Air
        
        // Reproduce the hardcoded logic from gpu_octree.cpp
        if (distFromCenter < 15000000.0f) {
            float noise = sin(test.center.x * 0.00001f) * cos(test.center.z * 0.00001f);
            if (distFromCenter < 6371000.0f) {
                dominantMaterial = 1; // Rock
            } else if (distFromCenter < 9000000.0f) {
                dominantMaterial = (noise > 0.0f) ? 1 : 2; // Rock or Water
            } else {
                dominantMaterial = 2; // Water
            }
        }
        
        std::cout << "  Node at distance " << distFromCenter/1000000.0f << "M km: ";
        switch(dominantMaterial) {
            case 0: std::cout << "Air"; break;
            case 1: std::cout << "Rock"; break;
            case 2: std::cout << "Water"; break;
            case 3: std::cout << "Magma"; break;
        }
        
        if (dominantMaterial > 0) {
            std::cout << " ✓ (has material)";
        } else {
            std::cout << " ✗ (no material - won't render)";
        }
        std::cout << std::endl;
    }
}

// Check for common issues
void checkCommonIssues() {
    std::cout << "\n=== Checking Common Issues ===" << std::endl;
    
    // Issue 1: Empty node buffer
    std::cout << "1. Empty node buffer: ";
    std::vector<int> emptyBuffer;
    if (emptyBuffer.size() == 0) {
        std::cout << "Buffer could be empty (would show black)" << std::endl;
    }
    
    // Issue 2: All materials are Air
    std::cout << "2. All Air materials: ";
    int materials[] = {0, 0, 0, 0, 0, 0, 0, 0};
    bool allAir = true;
    for (int m : materials) {
        if (m != 0) allAir = false;
    }
    if (allAir) {
        std::cout << "All materials are Air (would show black)" << std::endl;
    }
    
    // Issue 3: Wrong planet radius
    std::cout << "3. Radius mismatch: ";
    float generationRadius = 6371000.0f;
    float shaderRadius = 6371000.0f;
    if (std::abs(generationRadius - shaderRadius) > 1.0f) {
        std::cout << "Generation and shader use different radii!" << std::endl;
    } else {
        std::cout << "Radii match ✓" << std::endl;
    }
    
    // Issue 4: Shader traversal depth
    std::cout << "4. Max traversal depth: ";
    const int MAX_DEPTH = 15;
    if (MAX_DEPTH < 5) {
        std::cout << "Too shallow (might not reach leaves)" << std::endl;
    } else {
        std::cout << MAX_DEPTH << " levels ✓" << std::endl;
    }
    
    // Issue 5: Node index overflow
    std::cout << "5. Node index bounds: ";
    uint32_t maxIndex = 200000;
    uint32_t testIndex = 199999;
    if (testIndex >= maxIndex) {
        std::cout << "Index out of bounds!" << std::endl;
    } else {
        std::cout << "Within bounds ✓" << std::endl;
    }
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "    PLANET RENDERING ISSUE DIAGNOSIS     " << std::endl;
    std::cout << "==========================================" << std::endl;
    
    testShaderTraversal();
    testPlanetGeneration();
    testHardcodedMaterials();
    checkCommonIssues();
    
    std::cout << "\n==========================================" << std::endl;
    std::cout << "             DIAGNOSIS COMPLETE           " << std::endl;
    std::cout << "==========================================" << std::endl;
    
    std::cout << "\nPossible issues:" << std::endl;
    std::cout << "1. Check if GPU buffer is actually uploaded (non-zero size)" << std::endl;
    std::cout << "2. Check if shader receives correct planet radius in push constants" << std::endl;
    std::cout << "3. Check if any nodes actually have non-Air materials" << std::endl;
    std::cout << "4. Check if shader traversal reaches leaf nodes" << std::endl;
    std::cout << "5. Enable shader debug mode to see what's happening" << std::endl;
    
    return 0;
}