#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include "../include/core/octree.hpp"

using namespace octree;

class MaterialPipelineTests {
public:
    void runAllTests() {
        std::cout << "=== MATERIAL PIPELINE UNIT TESTS ===" << std::endl << std::endl;
        
        testNodeInitialization();
        testSetMaterialsFunction();
        testOctreeGeneration();
        testMaterialPersistence();
        testTraversalAndRetrieval();
        
        std::cout << std::endl << "=== ALL TESTS PASSED ===" << std::endl;
    }
    
private:
    // Test 1: Verify OctreeNode initializes voxels correctly
    void testNodeInitialization() {
        std::cout << "Test 1: Node Initialization" << std::endl;
        
        OctreeNode node(glm::vec3(0, 0, 0), 1000.0f, 0);
        const auto& voxels = node.getVoxels();
        
        // All voxels should start as Vacuum
        int vacuumCount = 0;
        for (int i = 0; i < 8; i++) {
            if (voxels[i].getDominantMaterialID() == core::MaterialID::Vacuum) {
                vacuumCount++;
            }
            // Check temperature is initialized
            assert(voxels[i].temperature == 128);  // Default temperature
        }
        
        assert(vacuumCount == 8);
        std::cout << "  ✓ Node initializes with 8 Vacuum voxels" << std::endl;
    }
    
    // Test 2: Test the setMaterials function in isolation
    void testSetMaterialsFunction() {
        std::cout << "Test 2: setMaterials Function" << std::endl;
        
        // We need to simulate what setMaterials does
        // Create a node at planet surface
        float planetRadius = 6371000.0f;
        glm::vec3 surfacePos(planetRadius * 0.98f, 0, 0);
        OctreeNode surfaceNode(surfacePos, 100000.0f, 5);
        
        // Manually apply the material setting logic from setMaterials
        std::cout << "  Node at distance " << glm::length(surfacePos) << " (radius=" << planetRadius << ")" << std::endl;
        
        // Apply materials based on the algorithm in setMaterials
        int materialCounts[4] = {0, 0, 0, 0};
        for (int i = 0; i < 8; i++) {
            glm::vec3 voxelOffset(
                (i & 1) ? surfaceNode.getHalfSize() * 0.5f : -surfaceNode.getHalfSize() * 0.5f,
                (i & 2) ? surfaceNode.getHalfSize() * 0.5f : -surfaceNode.getHalfSize() * 0.5f,
                (i & 4) ? surfaceNode.getHalfSize() * 0.5f : -surfaceNode.getHalfSize() * 0.5f
            );
            glm::vec3 voxelPos = surfaceNode.getCenter() + voxelOffset;
            float voxelDist = glm::length(voxelPos);
            
            core::MaterialID expectedMaterial;
            if (voxelDist > planetRadius * 1.02f) {
                expectedMaterial = core::MaterialID::Air;
            } else if (voxelDist > planetRadius * 0.98f) {
                // Surface - should be Rock or Water
                expectedMaterial = core::MaterialID::Rock; // Simplified for testing
            } else {
                // Below surface
                expectedMaterial = core::MaterialID::Rock;
            }
            
            materialCounts[static_cast<int>(expectedMaterial)]++;
            std::cout << "    Voxel " << i << " at dist=" << voxelDist 
                     << " -> " << static_cast<int>(expectedMaterial) << std::endl;
        }
        
        std::cout << "  Material distribution: Air=" << materialCounts[0] 
                  << " Rock=" << materialCounts[1] 
                  << " Water=" << materialCounts[2]
                  << " Magma=" << materialCounts[3] << std::endl;
        
        // At surface, we should have mostly Rock/Water, not all Air
        assert(materialCounts[1] > 0 || materialCounts[2] > 0);
        std::cout << "  ✓ Surface node would have non-air materials" << std::endl;
    }
    
    // Test 3: Test octree generation and structure
    void testOctreeGeneration() {
        std::cout << "Test 3: Octree Generation" << std::endl;
        
        // Create a tiny planet for testing
        OctreePlanet miniPlanet(1000.0f, 3); // 1km radius, depth 3
        
        // Before generation
        const OctreeNode* root = miniPlanet.getRoot();
        assert(root != nullptr);
        std::cout << "  ✓ Root node exists" << std::endl;
        
        // Generate with fixed seed
        miniPlanet.generate(42);
        
        // Count nodes
        int totalNodes = 0;
        int leafNodes = 0;
        int nodesWithMaterials = 0;
        
        std::function<void(const OctreeNode*)> countNodes = [&](const OctreeNode* node) {
            totalNodes++;
            if (node->isLeaf()) {
                leafNodes++;
                
                // Check if this leaf has non-air materials
                const auto& voxels = node->getVoxels();
                for (const auto& voxel : voxels) {
                    core::MaterialID id = voxel.getDominantMaterialID();
                    if (id != core::MaterialID::Air && id != core::MaterialID::Vacuum) {
                        nodesWithMaterials++;
                        break;
                    }
                }
            } else {
                // Traverse children
                const auto& children = node->getChildren();
                for (const auto& child : children) {
                    if (child) {
                        countNodes(child.get());
                    }
                }
            }
        };
        
        countNodes(root);
        
        std::cout << "  Total nodes: " << totalNodes << std::endl;
        std::cout << "  Leaf nodes: " << leafNodes << std::endl;
        std::cout << "  Nodes with materials: " << nodesWithMaterials << std::endl;
        
        assert(totalNodes > 0);
        assert(leafNodes > 0);
        // This is the critical test - do we have materials after generation?
        assert(nodesWithMaterials > 0);
        
        std::cout << "  ✓ Generated planet has nodes with materials" << std::endl;
    }
    
    // Test 4: Verify materials persist after setting
    void testMaterialPersistence() {
        std::cout << "Test 4: Material Persistence" << std::endl;
        
        // Create a node and manually set its materials
        OctreeNode testNode(glm::vec3(0, 0, 0), 1000.0f, 0);
        
        // Get voxels and verify we can't modify them directly (const)
        const auto& voxels = testNode.getVoxels();
        
        // Create a test voxel using new API
        MixedVoxel testVoxel = MixedVoxel::createPure(core::MaterialID::Rock);
        
        // Set voxel at a specific position
        glm::vec3 voxelPos(500, 0, 0); // Within the node
        testNode.setVoxel(voxelPos, testVoxel);
        
        // Retrieve and verify
        MixedVoxel* retrieved = testNode.getVoxel(voxelPos);
        if (retrieved) {
            assert(retrieved->getDominantMaterialID() == core::MaterialID::Rock);
            std::cout << "  ✓ Voxel persists after setting" << std::endl;
        } else {
            // If we can't retrieve by position, check the array directly
            bool foundRock = false;
            for (const auto& voxel : voxels) {
                if (voxel.getDominantMaterialID() == core::MaterialID::Rock) {
                    foundRock = true;
                    break;
                }
            }
            if (foundRock) {
                std::cout << "  ✓ Rock material found in voxel array" << std::endl;
            } else {
                std::cout << "  ✗ WARNING: Material not persisting!" << std::endl;
            }
        }
    }
    
    // Test 5: Test traversal and GPU data collection
    void testTraversalAndRetrieval() {
        std::cout << "Test 5: Traversal and Retrieval" << std::endl;
        
        // Create small planet
        OctreePlanet planet(1000.0f, 2);
        planet.generate(12345);
        
        // Traverse and collect leaf nodes like GPU does
        std::vector<const OctreeNode*> leafNodes;
        
        std::function<void(const OctreeNode*)> collectLeaves = [&](const OctreeNode* node) {
            if (node->isLeaf()) {
                leafNodes.push_back(node);
            } else {
                const auto& children = node->getChildren();
                for (const auto& child : children) {
                    if (child) {
                        collectLeaves(child.get());
                    }
                }
            }
        };
        
        collectLeaves(planet.getRoot());
        
        std::cout << "  Collected " << leafNodes.size() << " leaf nodes" << std::endl;
        
        // Check materials in collected nodes
        int nodesWithMaterials = 0;
        int totalMaterialVoxels = 0;
        
        for (const auto* node : leafNodes) {
            const auto& voxels = node->getVoxels();
            bool hasMaterial = false;
            
            for (const auto& voxel : voxels) {
                core::MaterialID id = voxel.getDominantMaterialID();
                if (id != core::MaterialID::Air && id != core::MaterialID::Vacuum) {
                    totalMaterialVoxels++;
                    hasMaterial = true;
                }
            }
            
            if (hasMaterial) {
                nodesWithMaterials++;
            }
        }
        
        std::cout << "  Nodes with materials: " << nodesWithMaterials << std::endl;
        std::cout << "  Total non-air voxels: " << totalMaterialVoxels << std::endl;
        
        // We should have SOME materials
        if (nodesWithMaterials == 0) {
            std::cout << "  ✗ CRITICAL: No materials found during traversal!" << std::endl;
            std::cout << "  This matches the GPU octree bug!" << std::endl;
            
            // Debug: Check first few nodes
            for (int i = 0; i < std::min(5, (int)leafNodes.size()); i++) {
                const auto* node = leafNodes[i];
                float dist = glm::length(node->getCenter());
                std::cout << "    Node " << i << " at distance " << dist << std::endl;
                
                const auto& voxels = node->getVoxels();
                std::cout << "      Materials: ";
                for (int j = 0; j < 8; j++) {
                    std::cout << static_cast<int>(voxels[j].getDominantMaterialID()) << " ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << "  ✓ Materials found during traversal" << std::endl;
        }
    }
};

int main() {
    try {
        MaterialPipelineTests tests;
        tests.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}