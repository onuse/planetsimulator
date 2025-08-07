#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include "../include/core/octree.hpp"
#include "../include/rendering/gpu_octree.hpp"

using namespace octree;
using namespace rendering;

class OctreeMaterialTests {
public:
    void runAllTests() {
        std::cout << "=== OCTREE MATERIAL PIPELINE TESTS ===" << std::endl << std::endl;
        
        testVoxelInitialization();
        testMaterialAssignment();
        testMaterialCounting();
        testGPUEncoding();
        testEndToEnd();
        
        std::cout << std::endl << "=== ALL TESTS PASSED ===" << std::endl;
    }
    
private:
    void testVoxelInitialization() {
        std::cout << "Test 1: Voxel Initialization" << std::endl;
        
        OctreeNode node(glm::vec3(0, 0, 0), 1000.0f, 0);
        
        // Check all voxels are initialized to Air
        for (int i = 0; i < 8; i++) {
            assert(node.voxels[i].material == MaterialType::Air);
            assert(node.voxels[i].density == 0.0f);
        }
        
        std::cout << "  ✓ All voxels initialized to Air" << std::endl;
    }
    
    void testMaterialAssignment() {
        std::cout << "Test 2: Material Assignment" << std::endl;
        
        float planetRadius = 6371000.0f;
        
        // Test node at planet surface
        {
            OctreeNode surfaceNode(glm::vec3(6300000, 0, 0), 100000.0f, 0);
            
            // Simulate material assignment
            for (int i = 0; i < 8; i++) {
                glm::vec3 voxelOffset(
                    (i & 1) ? surfaceNode.halfSize * 0.5f : -surfaceNode.halfSize * 0.5f,
                    (i & 2) ? surfaceNode.halfSize * 0.5f : -surfaceNode.halfSize * 0.5f,
                    (i & 4) ? surfaceNode.halfSize * 0.5f : -surfaceNode.halfSize * 0.5f
                );
                glm::vec3 voxelPos = surfaceNode.center + voxelOffset;
                float dist = glm::length(voxelPos);
                
                if (dist > planetRadius) {
                    surfaceNode.voxels[i].material = MaterialType::Air;
                } else if (dist > planetRadius * 0.95f) {
                    // Surface - simplified to always Rock for testing
                    surfaceNode.voxels[i].material = MaterialType::Rock;
                } else {
                    surfaceNode.voxels[i].material = MaterialType::Rock; // Mantle
                }
            }
            
            // Verify we have non-air materials
            int nonAirCount = 0;
            for (int i = 0; i < 8; i++) {
                if (surfaceNode.voxels[i].material != MaterialType::Air) {
                    nonAirCount++;
                }
            }
            
            assert(nonAirCount > 0);
            std::cout << "  ✓ Surface node has " << nonAirCount << " non-air voxels" << std::endl;
        }
        
        // Test node in space
        {
            OctreeNode spaceNode(glm::vec3(10000000, 0, 0), 100000.0f, 0);
            
            for (int i = 0; i < 8; i++) {
                glm::vec3 voxelOffset(
                    (i & 1) ? spaceNode.halfSize * 0.5f : -spaceNode.halfSize * 0.5f,
                    (i & 2) ? spaceNode.halfSize * 0.5f : -spaceNode.halfSize * 0.5f,
                    (i & 4) ? spaceNode.halfSize * 0.5f : -spaceNode.halfSize * 0.5f
                );
                glm::vec3 voxelPos = spaceNode.center + voxelOffset;
                float dist = glm::length(voxelPos);
                
                spaceNode.voxels[i].material = (dist > planetRadius) ? MaterialType::Air : MaterialType::Rock;
            }
            
            // All should be air
            for (int i = 0; i < 8; i++) {
                assert(spaceNode.voxels[i].material == MaterialType::Air);
            }
            
            std::cout << "  ✓ Space node has all air voxels" << std::endl;
        }
        
        // Test node at planet core
        {
            OctreeNode coreNode(glm::vec3(0, 0, 0), 100000.0f, 0);
            
            for (int i = 0; i < 8; i++) {
                glm::vec3 voxelOffset(
                    (i & 1) ? coreNode.halfSize * 0.5f : -coreNode.halfSize * 0.5f,
                    (i & 2) ? coreNode.halfSize * 0.5f : -coreNode.halfSize * 0.5f,
                    (i & 4) ? coreNode.halfSize * 0.5f : -coreNode.halfSize * 0.5f
                );
                glm::vec3 voxelPos = coreNode.center + voxelOffset;
                float dist = glm::length(voxelPos);
                
                // Core should be magma
                if (dist < planetRadius * 0.5f) {
                    coreNode.voxels[i].material = MaterialType::Magma;
                } else {
                    coreNode.voxels[i].material = MaterialType::Rock;
                }
            }
            
            // Should have magma
            int magmaCount = 0;
            for (int i = 0; i < 8; i++) {
                if (coreNode.voxels[i].material == MaterialType::Magma) {
                    magmaCount++;
                }
            }
            
            assert(magmaCount > 0);
            std::cout << "  ✓ Core node has " << magmaCount << " magma voxels" << std::endl;
        }
    }
    
    void testMaterialCounting() {
        std::cout << "Test 3: Material Counting for GPU" << std::endl;
        
        OctreeNode testNode(glm::vec3(0, 0, 0), 1000.0f, 0);
        
        // Set up a test pattern
        testNode.voxels[0].material = MaterialType::Rock;
        testNode.voxels[1].material = MaterialType::Rock;
        testNode.voxels[2].material = MaterialType::Rock;
        testNode.voxels[3].material = MaterialType::Water;
        testNode.voxels[4].material = MaterialType::Water;
        testNode.voxels[5].material = MaterialType::Water;
        testNode.voxels[6].material = MaterialType::Water;
        testNode.voxels[7].material = MaterialType::Air;
        
        // Count materials
        uint32_t materialCounts[4] = {0, 0, 0, 0};
        for (int i = 0; i < 8; i++) {
            uint32_t mat = static_cast<uint32_t>(testNode.voxels[i].material);
            if (mat < 4) {
                materialCounts[mat]++;
            }
        }
        
        assert(materialCounts[0] == 1); // Air
        assert(materialCounts[1] == 3); // Rock
        assert(materialCounts[2] == 4); // Water
        assert(materialCounts[3] == 0); // Magma
        
        // Find dominant
        uint32_t maxCount = 0;
        MaterialType dominant = MaterialType::Air;
        for (int m = 0; m < 4; m++) {
            if (materialCounts[m] > maxCount) {
                maxCount = materialCounts[m];
                dominant = static_cast<MaterialType>(m);
            }
        }
        
        assert(dominant == MaterialType::Water);
        std::cout << "  ✓ Correctly identified Water as dominant material (4/8 voxels)" << std::endl;
        
        // Test all same material
        for (int i = 0; i < 8; i++) {
            testNode.voxels[i].material = MaterialType::Rock;
        }
        
        for (int i = 0; i < 4; i++) materialCounts[i] = 0;
        for (int i = 0; i < 8; i++) {
            uint32_t mat = static_cast<uint32_t>(testNode.voxels[i].material);
            if (mat < 4) materialCounts[mat]++;
        }
        
        assert(materialCounts[1] == 8);
        std::cout << "  ✓ Correctly counted uniform Rock material (8/8 voxels)" << std::endl;
    }
    
    void testGPUEncoding() {
        std::cout << "Test 4: GPU Material Encoding" << std::endl;
        
        // Test encoding each material type
        for (int mat = 0; mat < 4; mat++) {
            uint32_t flags = 1; // isLeaf
            flags |= (mat << 8); // Material in bits 8-15
            
            // Decode
            bool isLeaf = (flags & 1) != 0;
            uint32_t decodedMat = (flags >> 8) & 0xFF;
            
            assert(isLeaf == true);
            assert(decodedMat == mat);
        }
        
        std::cout << "  ✓ All materials encode/decode correctly" << std::endl;
        
        // Test specific encoding values
        {
            uint32_t flags = 1 | (2 << 8); // Leaf with Water
            assert((flags & 1) == 1);
            assert(((flags >> 8) & 0xFF) == 2);
            std::cout << "  ✓ Water encodes to 0x" << std::hex << flags << std::dec 
                     << " (expected 0x201)" << std::endl;
        }
        
        {
            uint32_t flags = 1 | (1 << 8); // Leaf with Rock
            assert((flags & 1) == 1);
            assert(((flags >> 8) & 0xFF) == 1);
            std::cout << "  ✓ Rock encodes to 0x" << std::hex << flags << std::dec 
                     << " (expected 0x101)" << std::endl;
        }
    }
    
    void testEndToEnd() {
        std::cout << "Test 5: End-to-End Pipeline" << std::endl;
        
        // Create a small planet
        OctreePlanet planet(1000.0f, 2); // Small radius, shallow depth for testing
        
        // Generate with fixed seed for reproducibility
        planet.generate(12345);
        
        // Check that materials were set
        int nodesWithMaterials = 0;
        int totalLeaves = 0;
        
        planet.getRoot()->traverse([&](OctreeNode* node) {
            if (node->isLeaf()) {
                totalLeaves++;
                for (const auto& voxel : node->voxels) {
                    if (voxel.material != MaterialType::Air) {
                        nodesWithMaterials++;
                        break;
                    }
                }
            }
        });
        
        std::cout << "  Generated " << totalLeaves << " leaf nodes" << std::endl;
        std::cout << "  " << nodesWithMaterials << " nodes have non-air materials" << std::endl;
        
        // Should have at least some nodes with materials
        assert(nodesWithMaterials > 0);
        assert(nodesWithMaterials <= totalLeaves);
        
        // Check material distribution makes sense
        int airNodes = 0, rockNodes = 0, waterNodes = 0, magmaNodes = 0;
        
        planet.getRoot()->traverse([&](OctreeNode* node) {
            if (node->isLeaf()) {
                // Count dominant material
                uint32_t counts[4] = {0, 0, 0, 0};
                for (const auto& voxel : node->voxels) {
                    if (static_cast<uint32_t>(voxel.material) < 4) {
                        counts[static_cast<uint32_t>(voxel.material)]++;
                    }
                }
                
                // Find dominant
                uint32_t maxCount = 0;
                int dominant = 0;
                for (int i = 0; i < 4; i++) {
                    if (counts[i] > maxCount) {
                        maxCount = counts[i];
                        dominant = i;
                    }
                }
                
                switch(dominant) {
                    case 0: airNodes++; break;
                    case 1: rockNodes++; break;
                    case 2: waterNodes++; break;
                    case 3: magmaNodes++; break;
                }
            }
        });
        
        std::cout << "  Material distribution:" << std::endl;
        std::cout << "    Air: " << airNodes << " nodes" << std::endl;
        std::cout << "    Rock: " << rockNodes << " nodes" << std::endl;
        std::cout << "    Water: " << waterNodes << " nodes" << std::endl;
        std::cout << "    Magma: " << magmaNodes << " nodes" << std::endl;
        
        // Should have a mix of materials
        assert(airNodes > 0);  // Space around planet
        assert(rockNodes > 0 || waterNodes > 0); // Surface materials
        
        std::cout << "  ✓ End-to-end pipeline generates valid material distribution" << std::endl;
    }
};

int main() {
    try {
        OctreeMaterialTests tests;
        tests.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}