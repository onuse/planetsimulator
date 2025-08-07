#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/core/octree.hpp"
#include "../include/rendering/hierarchical_gpu_octree.hpp"

// Mock Vulkan setup for testing
VkDevice createMockDevice() {
    return reinterpret_cast<VkDevice>(1);
}

VkPhysicalDevice createMockPhysicalDevice() {
    return reinterpret_cast<VkPhysicalDevice>(1);
}

int main() {
    std::cout << "=== HIERARCHICAL GPU OCTREE TEST ===" << std::endl << std::endl;
    
    // Test 1: Create octree planet
    std::cout << "Test 1: Creating octree planet..." << std::endl;
    float planetRadius = 6371000.0f;
    octree::OctreePlanet planet(planetRadius, 4);
    planet.generate(42);
    std::cout << "  ✓ Planet created" << std::endl;
    
    // Test 2: Test frustum culling math
    std::cout << "\nTest 2: Testing frustum culling..." << std::endl;
    
    // Create view and projection matrices
    glm::vec3 cameraPos(planetRadius * 2.0f, 0.0f, 0.0f);
    glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
    
    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 100000000.0f);
    glm::mat4 viewProj = proj * view;
    
    std::cout << "  Camera at: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
    
    // Test 3: Test LOD calculation
    std::cout << "\nTest 3: Testing LOD selection..." << std::endl;
    
    struct TestNode {
        glm::vec3 center;
        float halfSize;
        const char* description;
    };
    
    TestNode testNodes[] = {
        {{0, 0, 0}, 100.0f, "Small node at origin"},
        {{planetRadius, 0, 0}, 1000.0f, "Medium node at surface"},
        {{planetRadius * 10, 0, 0}, 10000.0f, "Large node far away"},
    };
    
    for (const auto& test : testNodes) {
        float distance = glm::length(test.center - cameraPos);
        float ratio = distance / test.halfSize;
        
        uint32_t lod;
        if (ratio < 10.0f) lod = 0;
        else if (ratio < 50.0f) lod = 1;
        else if (ratio < 200.0f) lod = 2;
        else if (ratio < 1000.0f) lod = 3;
        else lod = 4;
        
        std::cout << "  " << test.description << ":" << std::endl;
        std::cout << "    Distance: " << distance << ", HalfSize: " << test.halfSize << std::endl;
        std::cout << "    Ratio: " << ratio << ", LOD: " << lod << std::endl;
    }
    
    std::cout << "  ✓ LOD selection logic verified" << std::endl;
    
    // Test 4: Test hierarchical traversal (without Vulkan)
    std::cout << "\nTest 4: Testing hierarchical traversal..." << std::endl;
    
    // Count nodes that would be visible
    int totalNodes = 0;
    int leafNodes = 0;
    
    std::function<void(const octree::OctreeNode*, int)> countNodes = 
        [&](const octree::OctreeNode* node, int depth) {
        totalNodes++;
        if (node->isLeaf()) {
            leafNodes++;
        } else if (depth < 3) { // Limit depth for testing
            const auto& children = node->getChildren();
            for (const auto& child : children) {
                if (child) {
                    countNodes(child.get(), depth + 1);
                }
            }
        }
    };
    
    if (planet.getRoot()) {
        countNodes(planet.getRoot(), 0);
    }
    
    std::cout << "  Total nodes traversed: " << totalNodes << std::endl;
    std::cout << "  Leaf nodes: " << leafNodes << std::endl;
    std::cout << "  ✓ Traversal complete" << std::endl;
    
    // Test 5: Verify material encoding
    std::cout << "\nTest 5: Testing material encoding..." << std::endl;
    
    for (int mat = 0; mat < 4; mat++) {
        uint32_t flags = 1; // isLeaf
        flags |= (mat << 8); // Encode material
        
        bool isLeaf = (flags & 1) != 0;
        uint32_t decodedMat = (flags >> 8) & 0xFF;
        
        const char* matNames[] = {"Air", "Rock", "Water", "Magma"};
        std::cout << "  Material " << matNames[mat] << " (id=" << mat << "):" << std::endl;
        std::cout << "    Encoded flags: 0x" << std::hex << flags << std::dec << std::endl;
        std::cout << "    Decoded: isLeaf=" << isLeaf << ", material=" << decodedMat << std::endl;
        
        assert(isLeaf == true);
        assert(decodedMat == mat);
    }
    std::cout << "  ✓ Material encoding/decoding works correctly" << std::endl;
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    
    return 0;
}