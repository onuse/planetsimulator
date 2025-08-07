#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <string>
#include <glm/glm.hpp>
#include "../include/core/octree.hpp"

class FullPipelineTest {
public:
    void run() {
        std::cout << "=== FULL RENDERING PIPELINE TEST ===" << std::endl << std::endl;
        
        // Create planet with high detail
        float planetRadius = 6371000.0f;
        std::cout << "Creating planet with radius " << planetRadius/1000 << " km..." << std::endl;
        octree::OctreePlanet planet(planetRadius, 6); // High detail
        planet.generate(42);
        
        // Simulate the exact GPU upload process
        std::cout << "\nSimulating GPU upload process..." << std::endl;
        
        // Step 1: Collect all leaf nodes
        struct NodeWithDistance {
            const octree::OctreeNode* node;
            float distance;
            octree::MaterialType dominantMaterial;
        };
        
        std::vector<NodeWithDistance> allNodes;
        
        std::function<void(const octree::OctreeNode*)> collectNodes = 
            [&](const octree::OctreeNode* node) {
            if (node->isLeaf()) {
                NodeWithDistance info;
                info.node = node;
                info.distance = glm::length(node->getCenter());
                
                // Calculate dominant material
                const auto& voxels = node->getVoxels();
                int materialCounts[4] = {0, 0, 0, 0};
                for (const auto& voxel : voxels) {
                    int mat = static_cast<int>(voxel.material);
                    if (mat >= 0 && mat < 4) {
                        materialCounts[mat]++;
                    }
                }
                
                int maxCount = 0;
                info.dominantMaterial = octree::MaterialType::Air;
                for (int i = 0; i < 4; i++) {
                    if (materialCounts[i] > maxCount) {
                        maxCount = materialCounts[i];
                        info.dominantMaterial = static_cast<octree::MaterialType>(i);
                    }
                }
                
                allNodes.push_back(info);
            } else {
                const auto& children = node->getChildren();
                for (const auto& child : children) {
                    if (child) {
                        collectNodes(child.get());
                    }
                }
            }
        };
        
        if (planet.getRoot()) {
            collectNodes(planet.getRoot());
        }
        
        std::cout << "Collected " << allNodes.size() << " leaf nodes" << std::endl;
        
        // Step 2: Sort by distance (CRITICAL!)
        std::sort(allNodes.begin(), allNodes.end(), 
            [](const NodeWithDistance& a, const NodeWithDistance& b) {
                return a.distance < b.distance;
            });
        
        std::cout << "Sorted nodes by distance from origin" << std::endl;
        
        // Step 3: Analyze what GPU will see
        std::cout << "\n=== GPU WILL SEE (first 50 nodes) ===" << std::endl;
        
        const char* matNames[] = {"Air", "Rock", "Water", "Magma"};
        int materialStats[4] = {0, 0, 0, 0};
        
        for (size_t i = 0; i < std::min(size_t(50), allNodes.size()); i++) {
            const auto& nodeInfo = allNodes[i];
            float ratio = nodeInfo.distance / planetRadius;
            
            materialStats[static_cast<int>(nodeInfo.dominantMaterial)]++;
            
            if (i < 20) {  // Show first 20 in detail
                std::cout << "  Node " << i << ": ";
                std::cout << "dist=" << (nodeInfo.distance/1000) << "km ";
                std::cout << "(r=" << ratio << ") ";
                std::cout << "material=" << matNames[static_cast<int>(nodeInfo.dominantMaterial)];
                
                // Flag potential issues
                if (nodeInfo.dominantMaterial == octree::MaterialType::Air && ratio < 1.0f) {
                    std::cout << " ⚠️ AIR INSIDE PLANET!";
                }
                std::cout << std::endl;
            }
        }
        
        std::cout << "\nFirst 50 nodes material distribution:" << std::endl;
        for (int i = 0; i < 4; i++) {
            std::cout << "  " << matNames[i] << ": " << materialStats[i] << " nodes" << std::endl;
        }
        
        // Step 4: Verify rendering will work
        std::cout << "\n=== RENDERING VERDICT ===" << std::endl;
        
        bool willRenderCorrectly = true;
        std::vector<std::string> issues;
        
        // Check 1: Are first nodes non-air?
        if (materialStats[0] == 50) {  // All air
            willRenderCorrectly = false;
            issues.push_back("ERROR: First 50 nodes are all Air! Planet will be invisible!");
        } else if (materialStats[0] > 40) {  // Mostly air
            issues.push_back("WARNING: First 50 nodes are mostly Air (" + 
                           std::to_string(materialStats[0]) + "/50)");
        }
        
        // Check 2: Do we have solid materials?
        int solidCount = materialStats[1] + materialStats[2] + materialStats[3];
        if (solidCount < 10) {
            willRenderCorrectly = false;
            issues.push_back("ERROR: Too few solid materials in first nodes!");
        }
        
        // Check 3: Distance distribution
        float minDist = allNodes.front().distance / planetRadius;
        float maxDist = allNodes.back().distance / planetRadius;
        std::cout << "Node distance range: " << minDist << "R to " << maxDist << "R" << std::endl;
        
        if (minDist > 1.0f) {
            issues.push_back("WARNING: Closest node is outside planet surface!");
        }
        
        // Report results
        if (willRenderCorrectly && issues.empty()) {
            std::cout << "\n✅ PLANET WILL RENDER CORRECTLY!" << std::endl;
            std::cout << "The planet should appear with:" << std::endl;
            std::cout << "  - Rock (gray/brown) in core and mantle" << std::endl;
            std::cout << "  - Mix of Rock and Water (blue) at surface" << std::endl;
            std::cout << "  - Proper spherical shape" << std::endl;
        } else if (willRenderCorrectly) {
            std::cout << "\n⚠️ PLANET WILL RENDER WITH MINOR ISSUES:" << std::endl;
            for (const auto& issue : issues) {
                std::cout << "  - " << issue << std::endl;
            }
        } else {
            std::cout << "\n❌ PLANET WILL NOT RENDER CORRECTLY!" << std::endl;
            for (const auto& issue : issues) {
                std::cout << "  - " << issue << std::endl;
            }
        }
        
        // Additional statistics
        std::cout << "\n=== OVERALL STATISTICS ===" << std::endl;
        
        int airNodes = 0, rockNodes = 0, waterNodes = 0, magmaNodes = 0;
        float closestNonAir = 1e10f;
        
        for (const auto& nodeInfo : allNodes) {
            switch (nodeInfo.dominantMaterial) {
                case octree::MaterialType::Air: airNodes++; break;
                case octree::MaterialType::Rock: rockNodes++; break;
                case octree::MaterialType::Water: waterNodes++; break;
                case octree::MaterialType::Magma: magmaNodes++; break;
            }
            
            if (nodeInfo.dominantMaterial != octree::MaterialType::Air) {
                closestNonAir = std::min(closestNonAir, nodeInfo.distance);
            }
        }
        
        std::cout << "Total nodes: " << allNodes.size() << std::endl;
        std::cout << "  Air: " << airNodes << " (" << (airNodes*100/allNodes.size()) << "%)" << std::endl;
        std::cout << "  Rock: " << rockNodes << " (" << (rockNodes*100/allNodes.size()) << "%)" << std::endl;
        std::cout << "  Water: " << waterNodes << " (" << (waterNodes*100/allNodes.size()) << "%)" << std::endl;
        std::cout << "  Magma: " << magmaNodes << " (" << (magmaNodes*100/allNodes.size()) << "%)" << std::endl;
        std::cout << "Closest non-air node: " << (closestNonAir/1000) << " km from origin" << std::endl;
        
        std::cout << "\n=== TEST COMPLETE ===" << std::endl;
    }
};

int main() {
    try {
        FullPipelineTest test;
        test.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}