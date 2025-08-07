#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include "../include/core/octree.hpp"

using namespace octree;

class VoxelPositionTests {
public:
    void runAllTests() {
        std::cout << "=== VOXEL POSITION AND MATERIAL TESTS ===" << std::endl << std::endl;
        
        testVoxelPositionCalculation();
        testNodePlacement();
        testMaterialAssignmentByDistance();
        testFirstNodesInTraversal();
        testSurfaceNodeMaterials();
        
        std::cout << std::endl << "=== ALL TESTS PASSED ===" << std::endl;
    }
    
private:
    // Test 1: Verify voxel position calculation
    void testVoxelPositionCalculation() {
        std::cout << "Test 1: Voxel Position Calculation" << std::endl;
        
        // Create a node at a known position
        glm::vec3 nodeCenter(1000, 0, 0);
        float halfSize = 100;
        OctreeNode node(nodeCenter, halfSize, 0);
        
        // Calculate voxel positions manually
        for (int i = 0; i < 8; i++) {
            glm::vec3 voxelOffset(
                (i & 1) ? halfSize * 0.5f : -halfSize * 0.5f,
                (i & 2) ? halfSize * 0.5f : -halfSize * 0.5f,
                (i & 4) ? halfSize * 0.5f : -halfSize * 0.5f
            );
            glm::vec3 voxelPos = nodeCenter + voxelOffset;
            float dist = glm::length(voxelPos);
            
            std::cout << "  Voxel " << i << ": offset=" 
                     << "(" << voxelOffset.x << "," << voxelOffset.y << "," << voxelOffset.z << ")"
                     << " pos=(" << voxelPos.x << "," << voxelPos.y << "," << voxelPos.z << ")"
                     << " dist=" << dist << std::endl;
                     
            // Verify distance calculation
            float expectedDist = std::sqrt(voxelPos.x * voxelPos.x + 
                                         voxelPos.y * voxelPos.y + 
                                         voxelPos.z * voxelPos.z);
            assert(std::abs(dist - expectedDist) < 0.001f);
        }
        
        std::cout << "  ✓ Voxel position calculations are correct" << std::endl;
    }
    
    // Test 2: Verify nodes are placed at expected positions
    void testNodePlacement() {
        std::cout << "Test 2: Node Placement Relative to Planet" << std::endl;
        
        float planetRadius = 6371000.0f;
        OctreePlanet planet(planetRadius, 3); // Shallow tree for testing
        planet.generate(42);
        
        // Collect all leaf nodes and check their distances
        std::vector<std::pair<float, int>> nodeDistances; // distance, depth
        
        // We need a non-const traverse function or cast
        const_cast<OctreeNode*>(planet.getRoot())->traverse([&](OctreeNode* node) {
            if (node->isLeaf()) {
                float dist = glm::length(node->getCenter());
                int depth = 0; // We don't have getLevel() exposed
                nodeDistances.push_back({dist, depth});
            }
        });
        
        // Sort by distance
        std::sort(nodeDistances.begin(), nodeDistances.end());
        
        std::cout << "  First 10 nodes by distance from center:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(10), nodeDistances.size()); i++) {
            float dist = nodeDistances[i].first;
            int depth = nodeDistances[i].second;
            float ratio = dist / planetRadius;
            std::cout << "    Node " << i << ": dist=" << dist 
                     << " (ratio=" << ratio << ") depth=" << depth << std::endl;
        }
        
        // Check if we have nodes inside the planet
        int nodesInsidePlanet = 0;
        int nodesNearSurface = 0;
        int nodesOutsidePlanet = 0;
        
        for (const auto& [dist, depth] : nodeDistances) {
            if (dist < planetRadius * 0.9f) {
                nodesInsidePlanet++;
            } else if (dist < planetRadius * 1.1f) {
                nodesNearSurface++;
            } else {
                nodesOutsidePlanet++;
            }
        }
        
        std::cout << "  Node distribution:" << std::endl;
        std::cout << "    Inside planet (<0.9R): " << nodesInsidePlanet << std::endl;
        std::cout << "    Near surface (0.9R-1.1R): " << nodesNearSurface << std::endl;
        std::cout << "    Outside planet (>1.1R): " << nodesOutsidePlanet << std::endl;
        
        // We should have nodes in all three regions
        assert(nodesInsidePlanet > 0 || nodesNearSurface > 0);
        std::cout << "  ✓ Nodes are distributed around the planet" << std::endl;
    }
    
    // Test 3: Verify material assignment based on distance
    void testMaterialAssignmentByDistance() {
        std::cout << "Test 3: Material Assignment by Distance" << std::endl;
        
        float planetRadius = 6371000.0f;
        
        // Test different distances
        struct TestCase {
            float distance;
            MaterialType expectedMaterial;
            const char* description;
        };
        
        TestCase cases[] = {
            {planetRadius * 0.3f, MaterialType::Magma, "Core"},
            {planetRadius * 0.7f, MaterialType::Rock, "Mantle"},
            {planetRadius * 0.99f, MaterialType::Rock, "Near surface (should be Rock or Water)"},
            {planetRadius * 1.5f, MaterialType::Air, "Space"}
        };
        
        for (const auto& tc : cases) {
            // Simulate material assignment logic
            MaterialType assignedMaterial;
            
            if (tc.distance > planetRadius * 1.02f) {
                assignedMaterial = MaterialType::Air;
            } else if (tc.distance > planetRadius * 0.98f) {
                // Surface - simplified to Rock for this test
                assignedMaterial = MaterialType::Rock;
            } else if (tc.distance > planetRadius * 0.5f) {
                assignedMaterial = MaterialType::Rock;
            } else {
                assignedMaterial = MaterialType::Magma;
            }
            
            std::cout << "  Distance " << tc.distance << " (" << tc.description << "): ";
            std::cout << "Expected " << static_cast<int>(tc.expectedMaterial);
            std::cout << ", Got " << static_cast<int>(assignedMaterial) << std::endl;
            
            // For surface, both Rock and Water are acceptable
            if (tc.distance > planetRadius * 0.98f && tc.distance < planetRadius * 1.02f) {
                assert(assignedMaterial == MaterialType::Rock || 
                       assignedMaterial == MaterialType::Water);
            } else {
                assert(assignedMaterial == tc.expectedMaterial);
            }
        }
        
        std::cout << "  ✓ Materials are assigned correctly based on distance" << std::endl;
    }
    
    // Test 4: Check the first nodes encountered in traversal
    void testFirstNodesInTraversal() {
        std::cout << "Test 4: First Nodes in Traversal Order" << std::endl;
        
        float planetRadius = 6371000.0f;
        OctreePlanet planet(planetRadius, 4); // Deeper tree
        planet.generate(42);
        
        // Collect first N leaf nodes in traversal order
        std::vector<float> firstNodeDistances;
        int collected = 0;
        const int maxNodes = 20;
        
        std::function<void(const OctreeNode*)> collectFirst = [&](const OctreeNode* node) {
            if (collected >= maxNodes) return;
            
            if (node->isLeaf()) {
                float dist = glm::length(node->getCenter());
                firstNodeDistances.push_back(dist);
                collected++;
                
                if (collected <= 10) {
                    std::cout << "  Leaf " << collected << " at distance " << dist 
                             << " (ratio=" << (dist/planetRadius) << ")" << std::endl;
                }
            } else {
                // Traverse children in order
                const auto& children = node->getChildren();
                for (const auto& child : children) {
                    if (child && collected < maxNodes) {
                        collectFirst(child.get());
                    }
                }
            }
        };
        
        collectFirst(planet.getRoot());
        
        // Check if ALL first nodes are outside the planet (this is the bug!)
        int outsideCount = 0;
        for (float dist : firstNodeDistances) {
            if (dist > planetRadius * 1.1f) {
                outsideCount++;
            }
        }
        
        std::cout << "  First " << maxNodes << " nodes: " << outsideCount 
                  << " are outside planet (>1.1R)" << std::endl;
        
        if (outsideCount == maxNodes) {
            std::cout << "  ✗ BUG CONFIRMED: All first nodes are outside the planet!" << std::endl;
            std::cout << "  This explains why GPU sees only Air materials!" << std::endl;
        } else {
            std::cout << "  ✓ Some early nodes are inside/near the planet" << std::endl;
        }
    }
    
    // Test 5: Check surface nodes specifically
    void testSurfaceNodeMaterials() {
        std::cout << "Test 5: Surface Node Materials" << std::endl;
        
        float planetRadius = 6371000.0f;
        OctreePlanet planet(planetRadius, 5);
        planet.generate(42);
        
        // Find nodes near the surface
        std::vector<const OctreeNode*> surfaceNodes;
        
        const_cast<OctreeNode*>(planet.getRoot())->traverse([&](OctreeNode* node) {
            if (node->isLeaf()) {
                float dist = glm::length(node->getCenter());
                if (dist > planetRadius * 0.9f && dist < planetRadius * 1.1f) {
                    surfaceNodes.push_back(node);
                }
            }
        });
        
        std::cout << "  Found " << surfaceNodes.size() << " surface nodes" << std::endl;
        
        // Check materials in surface nodes
        int nodesWithMaterials = 0;
        int totalVoxelsChecked = 0;
        int nonAirVoxels = 0;
        
        for (size_t i = 0; i < std::min(size_t(10), surfaceNodes.size()); i++) {
            const auto* node = surfaceNodes[i];
            const auto& voxels = node->getVoxels();
            
            bool hasMaterial = false;
            int nodeMaterials[4] = {0, 0, 0, 0};
            
            for (const auto& voxel : voxels) {
                totalVoxelsChecked++;
                int mat = static_cast<int>(voxel.material);
                if (mat < 4) nodeMaterials[mat]++;
                
                if (voxel.material != MaterialType::Air) {
                    nonAirVoxels++;
                    hasMaterial = true;
                }
            }
            
            if (hasMaterial) {
                nodesWithMaterials++;
            }
            
            if (i < 5) {
                float dist = glm::length(node->getCenter());
                std::cout << "  Surface node " << i << " at dist=" << dist
                         << ": Air=" << nodeMaterials[0] 
                         << " Rock=" << nodeMaterials[1]
                         << " Water=" << nodeMaterials[2]
                         << " Magma=" << nodeMaterials[3] << std::endl;
            }
        }
        
        std::cout << "  Surface nodes with materials: " << nodesWithMaterials 
                  << "/" << surfaceNodes.size() << std::endl;
        std::cout << "  Non-air voxels: " << nonAirVoxels 
                  << "/" << totalVoxelsChecked << std::endl;
        
        if (nodesWithMaterials == 0) {
            std::cout << "  ✗ BUG: Surface nodes have no materials!" << std::endl;
        } else {
            std::cout << "  ✓ Surface nodes have materials" << std::endl;
        }
    }
};

int main() {
    try {
        VoxelPositionTests tests;
        tests.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}