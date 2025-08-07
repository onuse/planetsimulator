// Test to trace the exact data flow from voxel creation to GPU upload
// This should catch where voxels are losing their material data

#include <iostream>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Test the exact flow we see in the debug output
void test_octree_node_voxel_access() {
    std::cout << "TEST: OctreeNode voxel access pattern..." << std::endl;
    
    // Simulate what happens during planet generation
    OctreeNode* node = new OctreeNode(glm::vec3(0, 0, 0), 1000.0f, 0);
    
    // Step 1: Voxels are initialized in constructor (should be air)
    std::cout << "  After construction:" << std::endl;
    for (int i = 0; i < 8; i++) {
        uint8_t mat = node->voxels[i].getDominantMaterial();
        std::cout << "    Voxel " << i << ": rock=" << (int)node->voxels[i].rock 
                  << " water=" << (int)node->voxels[i].water 
                  << " air=" << (int)node->voxels[i].air 
                  << " dominant=" << (int)mat << std::endl;
    }
    
    // Verify initial state is air
    assert(node->voxels[0].air == 255 && "Should start as air");
    assert(node->voxels[0].getDominantMaterial() == 0 && "Should be material type 0 (air)");
    
    // Step 2: Materials are set (like in generateTestSphere)
    std::cout << "\n  Setting materials..." << std::endl;
    for (int i = 0; i < 8; i++) {
        if (i < 4) {
            // Set to rock
            node->voxels[i] = MixedVoxel::createPure(255, 0, 0);
            std::cout << "    Set voxel " << i << " to ROCK" << std::endl;
        } else {
            // Set to water
            node->voxels[i] = MixedVoxel::createPure(0, 255, 0);
            std::cout << "    Set voxel " << i << " to WATER" << std::endl;
        }
    }
    
    // Step 3: Verify materials are set correctly
    std::cout << "\n  After setting materials:" << std::endl;
    for (int i = 0; i < 8; i++) {
        uint8_t mat = node->voxels[i].getDominantMaterial();
        std::cout << "    Voxel " << i << ": rock=" << (int)node->voxels[i].rock 
                  << " water=" << (int)node->voxels[i].water 
                  << " air=" << (int)node->voxels[i].air 
                  << " dominant=" << (int)mat << std::endl;
        
        if (i < 4) {
            assert(mat == 1 && "Should be rock (material 1)");
        } else {
            assert(mat == 2 && "Should be water (material 2)");
        }
    }
    
    // Step 4: Access through getVoxels() (like GPU upload does)
    std::cout << "\n  Accessing through getVoxels():" << std::endl;
    const auto& voxels = node->getVoxels();
    for (int i = 0; i < 8; i++) {
        uint8_t mat = voxels[i].getDominantMaterial();
        std::cout << "    Voxel " << i << " via getVoxels(): rock=" << (int)voxels[i].rock 
                  << " water=" << (int)voxels[i].water 
                  << " air=" << (int)voxels[i].air 
                  << " dominant=" << (int)mat << std::endl;
        
        // This is what GPU upload code does - verify it works
        if (i < 4) {
            assert(voxels[i].rock == 255 && "Rock should be preserved");
            assert(mat == 1 && "Should still be rock");
        } else {
            assert(voxels[i].water == 255 && "Water should be preserved");
            assert(mat == 2 && "Should still be water");
        }
    }
    
    delete node;
    std::cout << "  ✓ Voxel data preserved through all access patterns" << std::endl;
}

// Test what happens after subdivision
void test_voxel_access_after_subdivision() {
    std::cout << "\nTEST: Voxel access after subdivision..." << std::endl;
    
    OctreeNode* node = new OctreeNode(glm::vec3(0, 0, 0), 1000.0f, 0);
    
    // Set materials before subdivision
    for (int i = 0; i < 8; i++) {
        node->voxels[i] = MixedVoxel::createPure(200, 50, 5); // Mostly rock
    }
    
    std::cout << "  Before subdivision - is leaf: " << node->isLeaf() << std::endl;
    const auto& voxelsBefore = node->getVoxels();
    assert(voxelsBefore[0].rock == 200 && "Should have rock before subdivision");
    
    // Subdivide
    node->subdivide();
    
    std::cout << "  After subdivision - is leaf: " << node->isLeaf() << std::endl;
    
    // After subdivision, parent is NOT a leaf
    assert(!node->isLeaf() && "Should not be leaf after subdivision");
    
    // What happens if we call getVoxels() on non-leaf?
    // This is the potential bug - non-leaf nodes still have voxels array
    const auto& voxelsAfter = node->getVoxels();
    std::cout << "  Parent voxels after subdivision (should not be used):" << std::endl;
    for (int i = 0; i < 8; i++) {
        std::cout << "    Voxel " << i << ": rock=" << (int)voxelsAfter[i].rock 
                  << " water=" << (int)voxelsAfter[i].water 
                  << " air=" << (int)voxelsAfter[i].air << std::endl;
    }
    
    // Check children have proper voxels
    std::cout << "  Child voxels after subdivision:" << std::endl;
    for (int c = 0; c < 8; c++) {
        if (node->children[c] && node->children[c]->isLeaf()) {
            const auto& childVoxels = node->children[c]->getVoxels();
            std::cout << "    Child " << c << " voxel 0: rock=" << (int)childVoxels[0].rock 
                      << " water=" << (int)childVoxels[0].water 
                      << " air=" << (int)childVoxels[0].air << std::endl;
        }
    }
    
    delete node;
    std::cout << "  ✓ Subdivision behavior verified" << std::endl;
}

// Test the exact scenario from our debug output
void test_debug_output_scenario() {
    std::cout << "\nTEST: Reproducing debug output scenario..." << std::endl;
    
    // Create a node at the distance we see in debug output
    float dist = 1.24143e+07f; // From "Leaf 2 at dist=1.24143e+07"
    float planetRadius = 6.371e+06f;
    
    glm::vec3 center(-4.77825e+06f, -4.77825e+06f, -4.77825e+06f);
    OctreeNode* node = new OctreeNode(center, 4.77825e+06f, 1);
    
    std::cout << "  Node at distance " << dist << " (planet radius " << planetRadius << ")" << std::endl;
    
    // This is far outside planet, should be air
    for (int i = 0; i < 8; i++) {
        // Voxels start as air (from constructor)
        assert(node->voxels[i].air == 255 && "Should be air");
        assert(node->voxels[i].getDominantMaterial() == 0 && "Should be material 0");
    }
    
    // This matches our debug output: "materials: 0 0 0 0 0 0 0 0"
    std::cout << "  Materials: ";
    for (int i = 0; i < 8; i++) {
        std::cout << (int)node->voxels[i].getDominantMaterial() << " ";
    }
    std::cout << std::endl;
    
    // But the heuristic assigns material based on distance
    octree::MaterialType hardcodedMaterial;
    if (dist < planetRadius * 0.9f) {
        hardcodedMaterial = octree::MaterialType::Rock;
    } else if (dist < planetRadius * 1.1f) {
        // Surface - would use noise, but let's say water
        hardcodedMaterial = octree::MaterialType::Water;
    } else {
        hardcodedMaterial = octree::MaterialType::Air;
    }
    
    std::cout << "  Hardcoded dominant: " << (int)hardcodedMaterial << std::endl;
    assert(hardcodedMaterial == octree::MaterialType::Air && "Far nodes should be air");
    
    delete node;
    std::cout << "  ✓ Debug scenario reproduced correctly" << std::endl;
}

// Test that setting materials on leaf nodes works
void test_leaf_material_setting() {
    std::cout << "\nTEST: Setting materials on leaf nodes..." << std::endl;
    
    // Create a small octree
    OctreeNode* root = new OctreeNode(glm::vec3(0), 1000.0f, 0);
    root->subdivide();
    
    // Set materials on leaf children
    int leafCount = 0;
    for (int i = 0; i < 8; i++) {
        if (root->children[i] && root->children[i]->isLeaf()) {
            leafCount++;
            // Set specific material pattern
            for (int v = 0; v < 8; v++) {
                root->children[i]->voxels[v] = MixedVoxel::createPure(255, 0, 0); // Rock
            }
        }
    }
    
    assert(leafCount == 8 && "Should have 8 leaf children");
    
    // Verify materials are set
    for (int i = 0; i < 8; i++) {
        if (root->children[i] && root->children[i]->isLeaf()) {
            const auto& voxels = root->children[i]->getVoxels();
            assert(voxels[0].rock == 255 && "Leaf should have rock");
            assert(voxels[0].getDominantMaterial() == 1 && "Should be material 1");
        }
    }
    
    delete root;
    std::cout << "  ✓ Materials can be set on leaf nodes" << std::endl;
}

int main() {
    std::cout << "\n=== Voxel Data Flow Test ===" << std::endl;
    std::cout << "Tracing exact data flow to find where materials are lost\n" << std::endl;
    
    try {
        test_octree_node_voxel_access();
        test_voxel_access_after_subdivision();
        test_debug_output_scenario();
        test_leaf_material_setting();
        
        std::cout << "\n✅ ALL DATA FLOW TESTS PASSED!" << std::endl;
        std::cout << "\nConclusions:" << std::endl;
        std::cout << "  1. Voxels ARE initialized to air (0,0,255) correctly" << std::endl;
        std::cout << "  2. getDominantMaterial() returns 0 for air (correct)" << std::endl;
        std::cout << "  3. Materials CAN be set and retrieved correctly" << std::endl;
        std::cout << "  4. The debug output 'materials: 0 0 0 0 0 0 0 0' means ALL AIR" << std::endl;
        std::cout << "\nThe issue is likely:" << std::endl;
        std::cout << "  - Materials are not being set during planet generation, OR" << std::endl;
        std::cout << "  - Materials are being reset/overwritten after setting, OR" << std::endl;
        std::cout << "  - We're reading from the wrong nodes (non-leaf instead of leaf)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}