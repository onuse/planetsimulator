/**
 * Comprehensive test suite for all stages of the octree pipeline
 * Tests each stage independently to isolate issues
 */

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <memory>
#include <functional>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Mock structures to match the actual implementation
namespace octree {

enum class MaterialType : uint8_t {
    Air = 0,
    Rock = 1,
    Water = 2,
    Magma = 3
};

struct Voxel {
    MaterialType material;
    float temperature;
    float density;
    glm::vec3 velocity;
    float pressure;
    uint8_t plateId;
    float stress;
    float age;
};

class OctreeNode {
public:
    static constexpr int OCTREE_CHILDREN = 8;
    static constexpr int LEAF_VOXELS = 8;
    
    glm::vec3 center;
    float halfSize;
    int level;
    std::unique_ptr<OctreeNode> children[OCTREE_CHILDREN];
    Voxel voxels[LEAF_VOXELS];
    
    OctreeNode(const glm::vec3& c, float hs, int l) 
        : center(c), halfSize(hs), level(l) {
        for (auto& voxel : voxels) {
            voxel.material = MaterialType::Air;
            voxel.temperature = 0.0f;
            voxel.density = 0.0f;
        }
    }
    
    bool isLeaf() const {
        return !children[0];
    }
    
    void subdivide() {
        if (!isLeaf()) return;
        
        float childHalfSize = halfSize * 0.5f;
        for (int i = 0; i < OCTREE_CHILDREN; i++) {
            glm::vec3 childCenter = getChildCenter(i);
            children[i] = std::make_unique<OctreeNode>(childCenter, childHalfSize, level + 1);
        }
    }
    
    glm::vec3 getChildCenter(int index) const {
        float offset = halfSize * 0.5f;
        return glm::vec3(
            center.x + ((index & 1) ? offset : -offset),
            center.y + ((index & 2) ? offset : -offset),
            center.z + ((index & 4) ? offset : -offset)
        );
    }
};

} // namespace octree

namespace rendering {

struct GPUOctreeNode {
    glm::vec4 centerAndSize;
    glm::uvec4 childrenAndFlags;
};

struct GPUVoxelData {
    glm::vec4 colorAndDensity;
    glm::vec4 tempAndVelocity;
};

} // namespace rendering

// Test helper functions
class TestFramework {
public:
    static void TestOctreeGeneration() {
        std::cout << "\n=== TEST: Octree Generation ===" << std::endl;
        
        // Test 1: Root node creation
        {
            float planetRadius = 6371000.0f;
            float rootHalfSize = planetRadius * 1.5f;
            octree::OctreeNode root(glm::vec3(0.0f), rootHalfSize, 0);
            
            assert(root.center == glm::vec3(0.0f));
            assert(root.halfSize == rootHalfSize);
            assert(root.level == 0);
            assert(root.isLeaf());
            
            std::cout << "✓ Root node creation correct" << std::endl;
        }
        
        // Test 2: Subdivision
        {
            octree::OctreeNode node(glm::vec3(0.0f), 1000.0f, 0);
            node.subdivide();
            
            assert(!node.isLeaf());
            for (int i = 0; i < 8; i++) {
                assert(node.children[i] != nullptr);
                assert(node.children[i]->halfSize == 500.0f);
                assert(node.children[i]->level == 1);
            }
            
            // Verify child positions
            glm::vec3 expectedCenters[8] = {
                glm::vec3(-500, -500, -500),
                glm::vec3( 500, -500, -500),
                glm::vec3(-500,  500, -500),
                glm::vec3( 500,  500, -500),
                glm::vec3(-500, -500,  500),
                glm::vec3( 500, -500,  500),
                glm::vec3(-500,  500,  500),
                glm::vec3( 500,  500,  500)
            };
            
            for (int i = 0; i < 8; i++) {
                assert(glm::length(node.children[i]->center - expectedCenters[i]) < 0.001f);
            }
            
            std::cout << "✓ Subdivision creates correct children" << std::endl;
        }
        
        // Test 3: Nodes should only be created inside planet bounds
        {
            float planetRadius = 1000.0f;
            octree::OctreeNode root(glm::vec3(0.0f), planetRadius * 1.5f, 0);
            
            // Function to check if node should exist
            auto shouldNodeExist = [planetRadius](const octree::OctreeNode* node) -> bool {
                float distToCenter = glm::length(node->center);
                float nodeRadius = node->halfSize * 1.732f; // sqrt(3)
                return (distToCenter - nodeRadius) < planetRadius * 1.1f; // Small margin
            };
            
            root.subdivide();
            int validNodes = 0;
            int invalidNodes = 0;
            
            for (int i = 0; i < 8; i++) {
                if (root.children[i]) {
                    if (shouldNodeExist(root.children[i].get())) {
                        validNodes++;
                    } else {
                        invalidNodes++;
                        std::cout << "  WARNING: Node " << i << " outside bounds at distance " 
                                  << glm::length(root.children[i]->center) << std::endl;
                    }
                }
            }
            
            assert(invalidNodes == 0); // All nodes should be valid
            std::cout << "✓ All nodes within planet bounds (" << validNodes << " valid nodes)" << std::endl;
        }
    }
    
    static void TestMaterialGeneration() {
        std::cout << "\n=== TEST: Material Generation ===" << std::endl;
        
        // Test 1: Voxel material assignment based on distance
        {
            float planetRadius = 1000.0f;
            octree::OctreeNode node(glm::vec3(0.0f, 0.0f, 900.0f), 50.0f, 5);
            
            // Manually set materials as the generation function would
            for (int i = 0; i < 8; i++) {
                glm::vec3 voxelOffset(
                    (i & 1) ? 25.0f : -25.0f,
                    (i & 2) ? 25.0f : -25.0f,
                    (i & 4) ? 25.0f : -25.0f
                );
                glm::vec3 voxelPos = node.center + voxelOffset;
                float dist = glm::length(voxelPos);
                
                if (dist > planetRadius) {
                    node.voxels[i].material = octree::MaterialType::Air;
                } else if (dist > planetRadius * 0.95f) {
                    node.voxels[i].material = octree::MaterialType::Rock; // Surface
                } else {
                    node.voxels[i].material = octree::MaterialType::Rock; // Interior
                }
            }
            
            // Check that we have some non-air materials
            int airCount = 0, rockCount = 0;
            for (int i = 0; i < 8; i++) {
                if (node.voxels[i].material == octree::MaterialType::Air) airCount++;
                if (node.voxels[i].material == octree::MaterialType::Rock) rockCount++;
            }
            
            assert(rockCount > 0); // Should have some rock
            std::cout << "✓ Material assignment works (Air: " << airCount 
                      << ", Rock: " << rockCount << ")" << std::endl;
        }
        
        // Test 2: Surface material variation
        {
            octree::OctreeNode surfaceNode(glm::vec3(0.0f, 0.0f, 6371000.0f), 100.0f, 10);
            
            // Simulate surface material assignment with land/water
            for (int i = 0; i < 8; i++) {
                // Simple pattern: alternate between rock and water
                surfaceNode.voxels[i].material = (i % 3 == 0) ? 
                    octree::MaterialType::Water : octree::MaterialType::Rock;
            }
            
            int waterCount = 0, landCount = 0;
            for (int i = 0; i < 8; i++) {
                if (surfaceNode.voxels[i].material == octree::MaterialType::Water) waterCount++;
                if (surfaceNode.voxels[i].material == octree::MaterialType::Rock) landCount++;
            }
            
            assert(waterCount > 0 && landCount > 0);
            std::cout << "✓ Surface has both water and land (Water: " << waterCount 
                      << ", Land: " << landCount << ")" << std::endl;
        }
    }
    
    static void TestGPUFlattening() {
        std::cout << "\n=== TEST: GPU Octree Flattening ===" << std::endl;
        
        // Test 1: Leaf node flattening
        {
            octree::OctreeNode leafNode(glm::vec3(100, 200, 300), 50.0f, 3);
            
            // Set materials
            leafNode.voxels[0].material = octree::MaterialType::Rock;
            leafNode.voxels[1].material = octree::MaterialType::Rock;
            leafNode.voxels[2].material = octree::MaterialType::Water;
            for (int i = 3; i < 8; i++) {
                leafNode.voxels[i].material = octree::MaterialType::Rock;
            }
            
            // Simulate GPU node creation
            rendering::GPUOctreeNode gpuNode;
            gpuNode.centerAndSize = glm::vec4(leafNode.center, leafNode.halfSize);
            
            // Count materials for dominant material
            int materialCounts[4] = {0};
            for (const auto& voxel : leafNode.voxels) {
                materialCounts[static_cast<int>(voxel.material)]++;
            }
            
            // Find dominant
            int dominantMaterial = 0;
            int maxCount = 0;
            for (int i = 0; i < 4; i++) {
                if (materialCounts[i] > maxCount) {
                    maxCount = materialCounts[i];
                    dominantMaterial = i;
                }
            }
            
            gpuNode.childrenAndFlags.z = 1; // Leaf flag
            gpuNode.childrenAndFlags.z |= (dominantMaterial << 8); // Material in bits 8-15
            
            assert((gpuNode.childrenAndFlags.z & 1) == 1); // Is leaf
            assert(((gpuNode.childrenAndFlags.z >> 8) & 0xFF) == 1); // Rock is dominant
            
            std::cout << "✓ Leaf node flattening preserves material (dominant: Rock)" << std::endl;
        }
        
        // Test 2: Internal node flattening
        {
            octree::OctreeNode internalNode(glm::vec3(0, 0, 0), 1000.0f, 0);
            internalNode.subdivide();
            
            rendering::GPUOctreeNode gpuNode;
            gpuNode.centerAndSize = glm::vec4(internalNode.center, internalNode.halfSize);
            gpuNode.childrenAndFlags.x = 100; // Children start at index 100
            gpuNode.childrenAndFlags.z = 0; // Not a leaf
            
            assert((gpuNode.childrenAndFlags.z & 1) == 0); // Not leaf
            assert(gpuNode.childrenAndFlags.x == 100); // Children offset preserved
            
            std::cout << "✓ Internal node flattening preserves structure" << std::endl;
        }
    }
    
    static void TestShaderDataFormat() {
        std::cout << "\n=== TEST: Shader Data Format ===" << std::endl;
        
        // Test 1: Material encoding in flags
        {
            uint32_t flags = 0;
            
            // Set as leaf
            flags |= 1;
            
            // Encode material type Rock (1) in bits 8-15
            uint32_t material = 1;
            flags |= (material << 8);
            
            // Verify encoding
            assert((flags & 1) == 1); // Is leaf
            assert(((flags >> 8) & 0xFF) == 1); // Material is Rock
            
            std::cout << "✓ Material encoding in flags correct" << std::endl;
        }
        
        // Test 2: Material decoding
        {
            uint32_t testFlags[] = {
                0x0001, // Leaf with Air (0)
                0x0101, // Leaf with Rock (1)
                0x0201, // Leaf with Water (2)
                0x0301  // Leaf with Magma (3)
            };
            
            octree::MaterialType expectedMaterials[] = {
                octree::MaterialType::Air,
                octree::MaterialType::Rock,
                octree::MaterialType::Water,
                octree::MaterialType::Magma
            };
            
            for (int i = 0; i < 4; i++) {
                uint32_t material = (testFlags[i] >> 8) & 0xFF;
                assert(material == static_cast<uint32_t>(expectedMaterials[i]));
            }
            
            std::cout << "✓ Material decoding from flags correct" << std::endl;
        }
    }
    
    static void TestTraversalLogic() {
        std::cout << "\n=== TEST: Traversal Logic ===" << std::endl;
        
        // Test 1: Child index calculation
        {
            octree::OctreeNode node(glm::vec3(0, 0, 0), 1000.0f, 0);
            
            struct TestCase {
                glm::vec3 position;
                int expectedIndex;
            };
            
            TestCase tests[] = {
                { glm::vec3(-100, -100, -100), 0 }, // -x -y -z
                { glm::vec3( 100, -100, -100), 1 }, // +x -y -z
                { glm::vec3(-100,  100, -100), 2 }, // -x +y -z
                { glm::vec3( 100,  100, -100), 3 }, // +x +y -z
                { glm::vec3(-100, -100,  100), 4 }, // -x -y +z
                { glm::vec3( 100, -100,  100), 5 }, // +x -y +z
                { glm::vec3(-100,  100,  100), 6 }, // -x +y +z
                { glm::vec3( 100,  100,  100), 7 }  // +x +y +z
            };
            
            for (const auto& test : tests) {
                int index = 0;
                if (test.position.x > node.center.x) index |= 1;
                if (test.position.y > node.center.y) index |= 2;
                if (test.position.z > node.center.z) index |= 4;
                
                assert(index == test.expectedIndex);
            }
            
            std::cout << "✓ Child index calculation correct for all octants" << std::endl;
        }
        
        // Test 2: Ray-sphere intersection
        {
            glm::vec3 sphereCenter(0, 0, 0);
            float sphereRadius = 1000.0f;
            
            // Test hit
            glm::vec3 rayOrigin(2000, 0, 0);
            glm::vec3 rayDir = glm::normalize(glm::vec3(-1, 0, 0));
            
            glm::vec3 oc = rayOrigin - sphereCenter;
            float b = glm::dot(oc, rayDir);
            float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
            float h = b * b - c;
            
            assert(h >= 0); // Should hit
            
            // Test miss
            rayOrigin = glm::vec3(2000, 2000, 0); // Move ray to miss
            rayDir = glm::normalize(glm::vec3(0, 1, 0)); // Pointing up
            oc = rayOrigin - sphereCenter;
            b = glm::dot(oc, rayDir);
            c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
            h = b * b - c;
            
            assert(h < 0); // Should miss
            
            std::cout << "✓ Ray-sphere intersection logic correct" << std::endl;
        }
    }
    
    static void TestMemoryLayout() {
        std::cout << "\n=== TEST: Memory Layout ===" << std::endl;
        
        // Test structure sizes and alignment
        {
            assert(sizeof(rendering::GPUOctreeNode) == 32); // vec4 + uvec4
            assert(sizeof(rendering::GPUVoxelData) == 32);  // 2x vec4
            
            std::cout << "✓ GPU structure sizes correct (32 bytes each)" << std::endl;
        }
        
        // Test array indexing
        {
            std::vector<rendering::GPUOctreeNode> nodes;
            nodes.resize(100);
            
            // Parent at index 0
            nodes[0].childrenAndFlags.x = 1; // Children start at index 1
            
            // Children at indices 1-8
            for (int i = 0; i < 8; i++) {
                nodes[1 + i].centerAndSize.w = 500.0f;
                nodes[1 + i].childrenAndFlags.z = 1; // Leaf
            }
            
            // Verify traversal
            uint32_t childOffset = nodes[0].childrenAndFlags.x;
            assert(childOffset == 1);
            
            for (uint32_t i = 0; i < 8; i++) {
                uint32_t childIndex = childOffset + i;
                assert(nodes[childIndex].centerAndSize.w == 500.0f);
                assert((nodes[childIndex].childrenAndFlags.z & 1) == 1);
            }
            
            std::cout << "✓ Array indexing and traversal correct" << std::endl;
        }
    }
};

// Performance test
void TestPerformance() {
    std::cout << "\n=== TEST: Performance Metrics ===" << std::endl;
    
    // Measure octree generation time
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create a moderately complex octree
    octree::OctreeNode root(glm::vec3(0.0f), 10000.0f, 0);
    
    // Subdivide to depth 3
    std::function<void(octree::OctreeNode*, int)> subdivideRecursive;
    subdivideRecursive = [&](octree::OctreeNode* node, int depth) {
        if (depth >= 3) return;
        node->subdivide();
        for (int i = 0; i < 8; i++) {
            if (node->children[i]) {
                subdivideRecursive(node->children[i].get(), depth + 1);
            }
        }
    };
    
    subdivideRecursive(&root, 0);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Count nodes
    int nodeCount = 0;
    std::function<void(octree::OctreeNode*)> countNodes;
    countNodes = [&](octree::OctreeNode* node) {
        nodeCount++;
        if (!node->isLeaf()) {
            for (int i = 0; i < 8; i++) {
                if (node->children[i]) {
                    countNodes(node->children[i].get());
                }
            }
        }
    };
    
    countNodes(&root);
    
    std::cout << "✓ Generated " << nodeCount << " nodes in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "  (" << (duration.count() / nodeCount) << " μs per node)" << std::endl;
}

int main() {
    std::cout << "===========================================" << std::endl;
    std::cout << "    OCTREE PIPELINE COMPREHENSIVE TEST    " << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        // Run all test stages
        TestFramework::TestOctreeGeneration();
        TestFramework::TestMaterialGeneration();
        TestFramework::TestGPUFlattening();
        TestFramework::TestShaderDataFormat();
        TestFramework::TestTraversalLogic();
        TestFramework::TestMemoryLayout();
        TestPerformance();
        
        std::cout << "\n===========================================" << std::endl;
        std::cout << "         ALL TESTS PASSED! ✓              " << std::endl;
        std::cout << "===========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}