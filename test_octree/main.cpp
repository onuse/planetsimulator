// Minimal GPU Octree Test Project
// Tests the exact same node structure and traversal as the main project
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

// EXACT same structure as main project
struct GPUOctreeNode {
    glm::vec4 centerAndSize;     // xyz = center, w = halfSize
    glm::uvec4 childrenAndFlags; // x = children offset, y = voxel offset, z = flags, w = reserved
};

enum MaterialType : uint32_t {
    Air = 0,
    Rock = 1,
    Water = 2,
    Magma = 3
};

class OctreeTest {
public:
    std::vector<GPUOctreeNode> nodes;
    
    // Test Case 1: Single root node with water material
    void createSingleNode() {
        nodes.clear();
        GPUOctreeNode root;
        root.centerAndSize = glm::vec4(0, 0, 0, 1000.0f); // 1km cube at origin
        root.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0, 1 | (Water << 8), 0); // Leaf with water
        nodes.push_back(root);
        std::cout << "Test 1: Single water node at origin, size=" << root.centerAndSize.w << std::endl;
    }
    
    // Test Case 2: Root with 8 children (one level deep)
    void createTwoLevelTree() {
        nodes.clear();
        
        // Root node (internal)
        GPUOctreeNode root;
        root.centerAndSize = glm::vec4(0, 0, 0, 1000.0f);
        root.childrenAndFlags = glm::uvec4(1, 0xFFFFFFFF, 0, 0); // Children start at index 1
        nodes.push_back(root);
        
        // Add 8 children
        for (int i = 0; i < 8; i++) {
            GPUOctreeNode child;
            float halfSize = 500.0f;
            
            // Calculate child position based on octant
            float x = (i & 1) ? halfSize : -halfSize;
            float y = (i & 2) ? halfSize : -halfSize;
            float z = (i & 4) ? halfSize : -halfSize;
            
            child.centerAndSize = glm::vec4(x, y, z, halfSize);
            
            // Make some water, some rock
            MaterialType mat = (i % 3 == 0) ? Water : Rock;
            child.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0, 1 | (mat << 8), 0);
            nodes.push_back(child);
        }
        
        std::cout << "Test 2: Root + 8 children, " << nodes.size() << " nodes total" << std::endl;
    }
    
    // Test Case 3: Reproduce exact structure from main project
    void createRealisticTree() {
        nodes.clear();
        
        // Mimic the main project's root node
        GPUOctreeNode root;
        root.centerAndSize = glm::vec4(0, 0, 0, 9556500.0f); // Same as planet radius
        root.childrenAndFlags = glm::uvec4(1, 0xFFFFFFFF, 0, 0);
        nodes.push_back(root);
        
        // Add first level children (8)
        float childSize = 4778250.0f; // Half of root
        for (int i = 0; i < 8; i++) {
            GPUOctreeNode child;
            float x = (i & 1) ? childSize : -childSize;
            float y = (i & 2) ? childSize : -childSize;
            float z = (i & 4) ? childSize : -childSize;
            child.centerAndSize = glm::vec4(x, y, z, childSize);
            
            // Make first child internal, others leaves
            if (i == 0) {
                child.childrenAndFlags = glm::uvec4(9, 0xFFFFFFFF, 0, 0); // Has children at index 9
            } else {
                // Leaf with material - this is what we should find!
                MaterialType mat = (i == 1) ? Water : Air;
                child.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0, 1 | (mat << 8), 0);
            }
            nodes.push_back(child);
        }
        
        // Add some grandchildren for the first child
        float grandchildSize = 2389125.0f;
        for (int i = 0; i < 8; i++) {
            GPUOctreeNode grandchild;
            float x = -childSize + ((i & 1) ? grandchildSize : -grandchildSize);
            float y = -childSize + ((i & 2) ? grandchildSize : -grandchildSize);
            float z = -childSize + ((i & 4) ? grandchildSize : -grandchildSize);
            grandchild.centerAndSize = glm::vec4(x, y, z, grandchildSize);
            
            // All grandchildren are water leaves
            grandchild.childrenAndFlags = glm::uvec4(0xFFFFFFFF, 0, 1 | (Water << 8), 0);
            nodes.push_back(grandchild);
        }
        
        std::cout << "Test 3: Realistic tree with " << nodes.size() << " nodes" << std::endl;
        std::cout << "  Root at (0,0,0) size=" << nodes[0].centerAndSize.w << std::endl;
        std::cout << "  First child at index 1, children at index " << nodes[1].childrenAndFlags.x << std::endl;
    }
    
    // CPU reference traversal - what GPU should produce
    bool cpuTraverse(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::vec3& hitPoint, MaterialType& material) {
        // Simple ray-sphere test against planet
        float planetRadius = nodes[0].centerAndSize.w;
        glm::vec3 oc = rayOrigin;
        float b = glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - planetRadius * planetRadius;
        float h = b * b - c;
        
        if (h < 0.0f) return false; // Miss planet
        
        h = sqrt(h);
        float t = -b - h; // Near intersection
        if (t < 0) t = -b + h; // Use far if we're inside
        
        hitPoint = rayOrigin + rayDir * t;
        
        // Now traverse octree at hit point
        uint32_t nodeIndex = 0;
        const int MAX_DEPTH = 20;
        
        for (int depth = 0; depth < MAX_DEPTH; depth++) {
            if (nodeIndex >= nodes.size()) {
                std::cout << "ERROR: Invalid node index " << nodeIndex << std::endl;
                return false;
            }
            
            GPUOctreeNode& node = nodes[nodeIndex];
            bool isLeaf = (node.childrenAndFlags.z & 1) != 0;
            
            if (isLeaf) {
                material = (MaterialType)((node.childrenAndFlags.z >> 8) & 0xFF);
                std::cout << "CPU: Found leaf at depth " << depth 
                          << ", material=" << material 
                          << " at index " << nodeIndex << std::endl;
                return material != Air;
            }
            
            // Find which child contains hit point
            uint32_t childrenOffset = node.childrenAndFlags.x;
            if (childrenOffset == 0xFFFFFFFF || childrenOffset >= nodes.size()) {
                std::cout << "ERROR: Invalid children offset " << childrenOffset << std::endl;
                return false;
            }
            
            // Determine octant
            glm::vec3 center = glm::vec3(node.centerAndSize);
            uint32_t octant = 0;
            if (hitPoint.x > center.x) octant |= 1;
            if (hitPoint.y > center.y) octant |= 2;
            if (hitPoint.z > center.z) octant |= 4;
            
            nodeIndex = childrenOffset + octant;
            std::cout << "CPU: Depth " << depth << ", moving to child " << octant 
                      << " (index " << nodeIndex << ")" << std::endl;
        }
        
        std::cout << "CPU: Max depth reached!" << std::endl;
        return false;
    }
    
    void runTests() {
        std::cout << "\n=== GPU Octree Test Suite ===\n" << std::endl;
        
        // Test 1: Single node
        createSingleNode();
        MaterialType mat;
        glm::vec3 hit;
        glm::vec3 origin(0, 0, -2000);
        glm::vec3 dir(0, 0, 1);
        
        if (cpuTraverse(origin, dir, hit, mat)) {
            std::cout << "✓ Test 1 passed: Found material " << mat << std::endl;
        } else {
            std::cout << "✗ Test 1 failed: No hit" << std::endl;
        }
        
        // Test 2: Two levels
        std::cout << "\n";
        createTwoLevelTree();
        origin = glm::vec3(100, 100, -2000);
        if (cpuTraverse(origin, dir, hit, mat)) {
            std::cout << "✓ Test 2 passed: Found material " << mat << std::endl;
        } else {
            std::cout << "✗ Test 2 failed: No hit" << std::endl;
        }
        
        // Test 3: Realistic
        std::cout << "\n";
        createRealisticTree();
        origin = glm::vec3(0, 0, -15000000); // Outside planet
        if (cpuTraverse(origin, dir, hit, mat)) {
            std::cout << "✓ Test 3 passed: Found material " << mat << std::endl;
        } else {
            std::cout << "✗ Test 3 failed: No hit" << std::endl;
        }
    }
    
    void saveToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        uint32_t nodeCount = nodes.size();
        file.write((char*)&nodeCount, sizeof(nodeCount));
        file.write((char*)nodes.data(), nodes.size() * sizeof(GPUOctreeNode));
        file.close();
        std::cout << "Saved " << nodeCount << " nodes to " << filename << std::endl;
    }
};

int main() {
    OctreeTest test;
    test.runTests();
    
    // Save test data for GPU testing
    test.createSingleNode();
    test.saveToFile("test1.octree");
    
    test.createTwoLevelTree();
    test.saveToFile("test2.octree");
    
    test.createRealisticTree();
    test.saveToFile("test3.octree");
    
    std::cout << "\nTest octrees saved. Next step: Create GPU shader to load and traverse these." << std::endl;
    
    return 0;
}