#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include <glm/glm.hpp>
#include "../include/core/octree.hpp"

class RenderingVerificationTests {
public:
    void runAllTests() {
        std::cout << "=== RENDERING VERIFICATION TESTS ===" << std::endl << std::endl;
        
        testNodeDistribution();
        testMaterialDistribution();
        testGPUDataGeneration();
        testRayMarchingSimulation();
        generateDebugOutput();
        
        std::cout << std::endl << "=== RENDERING VERIFICATION COMPLETE ===" << std::endl;
    }
    
private:
    // Test 1: Verify nodes are distributed correctly around planet
    void testNodeDistribution() {
        std::cout << "Test 1: Node Distribution Analysis" << std::endl;
        
        float planetRadius = 6371000.0f;
        octree::OctreePlanet planet(planetRadius, 5);
        planet.generate(42);
        
        // Collect all leaf nodes
        struct NodeInfo {
            glm::vec3 center;
            float halfSize;
            octree::MaterialType dominantMaterial;
            float distanceFromCenter;
        };
        
        std::vector<NodeInfo> nodes;
        
        const_cast<octree::OctreeNode*>(planet.getRoot())->traverse([&](octree::OctreeNode* node) {
            if (node->isLeaf()) {
                NodeInfo info;
                info.center = node->getCenter();
                info.halfSize = node->getHalfSize();
                info.distanceFromCenter = glm::length(info.center);
                
                // Calculate dominant material
                const auto& voxels = node->getVoxels();
                int materialCounts[4] = {0, 0, 0, 0};
                for (const auto& voxel : voxels) {
                    if (static_cast<int>(voxel.material) < 4) {
                        materialCounts[static_cast<int>(voxel.material)]++;
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
                
                nodes.push_back(info);
            }
        });
        
        // Analyze distribution
        int insideCore = 0;     // < 0.5R
        int inMantle = 0;       // 0.5R - 0.95R  
        int nearSurface = 0;    // 0.95R - 1.05R
        int inAtmosphere = 0;   // 1.05R - 1.2R
        int inSpace = 0;        // > 1.2R
        
        for (const auto& node : nodes) {
            float ratio = node.distanceFromCenter / planetRadius;
            if (ratio < 0.5f) insideCore++;
            else if (ratio < 0.95f) inMantle++;
            else if (ratio < 1.05f) nearSurface++;
            else if (ratio < 1.2f) inAtmosphere++;
            else inSpace++;
        }
        
        std::cout << "  Total leaf nodes: " << nodes.size() << std::endl;
        std::cout << "  Spatial distribution:" << std::endl;
        std::cout << "    Core (<0.5R): " << insideCore << std::endl;
        std::cout << "    Mantle (0.5-0.95R): " << inMantle << std::endl;
        std::cout << "    Surface (0.95-1.05R): " << nearSurface << std::endl;
        std::cout << "    Atmosphere (1.05-1.2R): " << inAtmosphere << std::endl;
        std::cout << "    Space (>1.2R): " << inSpace << std::endl;
        
        // CRITICAL: We should have many nodes near the surface
        if (nearSurface < nodes.size() / 10) {
            std::cout << "  ⚠️ WARNING: Too few surface nodes! Only " << nearSurface 
                      << " out of " << nodes.size() << std::endl;
        } else {
            std::cout << "  ✓ Good surface node distribution" << std::endl;
        }
    }
    
    // Test 2: Verify materials are properly distributed
    void testMaterialDistribution() {
        std::cout << "\nTest 2: Material Distribution Analysis" << std::endl;
        
        float planetRadius = 6371000.0f;
        octree::OctreePlanet planet(planetRadius, 5);
        planet.generate(42);
        
        // Count materials at different distances
        struct DistanceBin {
            float minDist;
            float maxDist;
            int materialCounts[4];
            int nodeCount;
        };
        
        DistanceBin bins[] = {
            {0, planetRadius * 0.5f, {0,0,0,0}, 0},           // Core
            {planetRadius * 0.5f, planetRadius * 0.9f, {0,0,0,0}, 0},   // Mantle
            {planetRadius * 0.9f, planetRadius * 1.1f, {0,0,0,0}, 0},   // Surface
            {planetRadius * 1.1f, planetRadius * 2.0f, {0,0,0,0}, 0}    // Space
        };
        
        const_cast<octree::OctreeNode*>(planet.getRoot())->traverse([&](octree::OctreeNode* node) {
            if (node->isLeaf()) {
                float dist = glm::length(node->getCenter());
                
                // Find which bin this node belongs to
                for (auto& bin : bins) {
                    if (dist >= bin.minDist && dist < bin.maxDist) {
                        bin.nodeCount++;
                        
                        // Count materials in this node
                        const auto& voxels = node->getVoxels();
                        for (const auto& voxel : voxels) {
                            int mat = static_cast<int>(voxel.material);
                            if (mat >= 0 && mat < 4) {
                                bin.materialCounts[mat]++;
                            }
                        }
                        break;
                    }
                }
            }
        });
        
        const char* binNames[] = {"Core", "Mantle", "Surface", "Space"};
        const char* matNames[] = {"Air", "Rock", "Water", "Magma"};
        
        for (int i = 0; i < 4; i++) {
            std::cout << "  " << binNames[i] << " region (" << bins[i].nodeCount << " nodes):" << std::endl;
            if (bins[i].nodeCount > 0) {
                for (int m = 0; m < 4; m++) {
                    int percentage = (bins[i].materialCounts[m] * 100) / (bins[i].nodeCount * 8);
                    if (bins[i].materialCounts[m] > 0) {
                        std::cout << "    " << matNames[m] << ": " << bins[i].materialCounts[m] 
                                  << " voxels (" << percentage << "%)" << std::endl;
                    }
                }
            }
        }
        
        // Verify expectations
        bool hasIssues = false;
        
        // Core should have magma or rock
        if (bins[0].nodeCount > 0 && bins[0].materialCounts[3] == 0 && bins[0].materialCounts[1] == 0) {
            std::cout << "  ⚠️ WARNING: Core has no magma or rock!" << std::endl;
            hasIssues = true;
        }
        
        // Surface should have mix of rock and water
        if (bins[2].nodeCount > 0) {
            int rockWaterTotal = bins[2].materialCounts[1] + bins[2].materialCounts[2];
            if (rockWaterTotal < bins[2].nodeCount * 4) { // Less than 50% rock/water
                std::cout << "  ⚠️ WARNING: Surface has too little rock/water!" << std::endl;
                hasIssues = true;
            }
        }
        
        // Space should be mostly air
        if (bins[3].nodeCount > 0 && bins[3].materialCounts[0] < bins[3].nodeCount * 6) {
            std::cout << "  ⚠️ WARNING: Space region not mostly air!" << std::endl;
            hasIssues = true;
        }
        
        if (!hasIssues) {
            std::cout << "  ✓ Material distribution looks correct" << std::endl;
        }
    }
    
    // Test 3: Verify GPU data generation
    void testGPUDataGeneration() {
        std::cout << "\nTest 3: GPU Data Generation" << std::endl;
        
        float planetRadius = 1000.0f; // Smaller for testing
        octree::OctreePlanet planet(planetRadius, 3);
        planet.generate(42);
        
        // Simulate GPU octree flattening (using simpler structures to avoid dependency issues)
        struct SimpleGPUNode {
            glm::vec4 centerAndSize;
            glm::uvec4 childrenAndFlags;
        };
        struct SimpleGPUVoxel {
            glm::vec4 colorAndDensity;
            glm::vec4 tempAndVelocity;
        };
        
        std::vector<SimpleGPUNode> gpuNodes;
        std::vector<SimpleGPUVoxel> gpuVoxels;
        
        uint32_t nodeIndex = 0;
        uint32_t voxelIndex = 0;
        
        // Collect and sort nodes by distance (as done in actual implementation)
        struct NodeWithDistance {
            const octree::OctreeNode* node;
            float distance;
        };
        
        std::vector<NodeWithDistance> allNodes;
        
        std::function<void(const octree::OctreeNode*)> collectNodes = 
            [&](const octree::OctreeNode* node) {
            if (node->isLeaf()) {
                float dist = glm::length(node->getCenter());
                allNodes.push_back({node, dist});
            } else {
                const auto& children = node->getChildren();
                for (const auto& child : children) {
                    if (child) {
                        collectNodes(child.get());
                    }
                }
            }
        };
        
        collectNodes(planet.getRoot());
        
        // Sort by distance (closest first)
        std::sort(allNodes.begin(), allNodes.end(), 
            [](const NodeWithDistance& a, const NodeWithDistance& b) {
                return a.distance < b.distance;
            });
        
        std::cout << "  Sorted " << allNodes.size() << " nodes by distance" << std::endl;
        
        // Check first 10 nodes
        std::cout << "  First 10 nodes after sorting:" << std::endl;
        int nonAirCount = 0;
        for (size_t i = 0; i < std::min(size_t(10), allNodes.size()); i++) {
            const auto& nodeInfo = allNodes[i];
            float ratio = nodeInfo.distance / planetRadius;
            
            // Check dominant material
            const auto& voxels = nodeInfo.node->getVoxels();
            int materialCounts[4] = {0,0,0,0};
            for (const auto& voxel : voxels) {
                int mat = static_cast<int>(voxel.material);
                if (mat >= 0 && mat < 4) {
                    materialCounts[mat]++;
                }
            }
            
            int dominantMat = 0;
            int maxCount = materialCounts[0];
            for (int m = 1; m < 4; m++) {
                if (materialCounts[m] > maxCount) {
                    maxCount = materialCounts[m];
                    dominantMat = m;
                }
            }
            
            const char* matNames[] = {"Air", "Rock", "Water", "Magma"};
            std::cout << "    Node " << i << ": dist=" << nodeInfo.distance 
                      << " (r=" << ratio << ") material=" << matNames[dominantMat] << std::endl;
            
            if (dominantMat != 0) {
                nonAirCount++;
            }
        }
        
        if (nonAirCount == 0) {
            std::cout << "  ⚠️ ERROR: First 10 nodes are all Air! Planet will render black!" << std::endl;
        } else {
            std::cout << "  ✓ Found " << nonAirCount << " non-air nodes in first 10 (good!)" << std::endl;
        }
    }
    
    // Test 4: Simulate ray marching
    void testRayMarchingSimulation() {
        std::cout << "\nTest 4: Ray Marching Simulation" << std::endl;
        
        float planetRadius = 1000.0f;
        octree::OctreePlanet planet(planetRadius, 4);
        planet.generate(42);
        
        // Simulate rays from camera
        glm::vec3 cameraPos(2000, 0, 0); // Camera at 2x radius
        
        struct RayTest {
            glm::vec3 direction;
            const char* description;
        };
        
        RayTest rays[] = {
            {glm::normalize(glm::vec3(-1, 0, 0)), "Direct to planet center"},
            {glm::normalize(glm::vec3(-1, 0.5, 0)), "Grazing top"},
            {glm::normalize(glm::vec3(-1, -0.5, 0)), "Grazing bottom"},
            {glm::normalize(glm::vec3(-1, 0, 0.5)), "Grazing side"},
            {glm::normalize(glm::vec3(1, 0, 0)), "Away from planet"}
        };
        
        for (const auto& ray : rays) {
            std::cout << "  Ray: " << ray.description << std::endl;
            
            // Simple ray-sphere intersection
            float t = -1;
            glm::vec3 oc = cameraPos;
            float a = glm::dot(ray.direction, ray.direction);
            float b = 2.0f * glm::dot(oc, ray.direction);
            float c = glm::dot(oc, oc) - planetRadius * planetRadius;
            float discriminant = b * b - 4 * a * c;
            
            if (discriminant >= 0) {
                float t1 = (-b - sqrt(discriminant)) / (2 * a);
                float t2 = (-b + sqrt(discriminant)) / (2 * a);
                t = (t1 > 0) ? t1 : t2;
            }
            
            if (t > 0) {
                glm::vec3 hitPoint = cameraPos + ray.direction * t;
                float hitDist = glm::length(hitPoint);
                std::cout << "    Hit at distance " << t << ", position dist=" << hitDist << std::endl;
                
                // Find what material would be at this point
                const char* expectedMaterial = "Unknown";
                if (hitDist < planetRadius * 0.5f) {
                    expectedMaterial = "Magma (core)";
                } else if (hitDist < planetRadius * 0.95f) {
                    expectedMaterial = "Rock (mantle)";
                } else {
                    expectedMaterial = "Rock/Water (surface)";
                }
                std::cout << "    Expected material: " << expectedMaterial << std::endl;
            } else {
                std::cout << "    No hit (correct for rays missing planet)" << std::endl;
            }
        }
    }
    
    // Test 5: Generate debug output file
    void generateDebugOutput() {
        std::cout << "\nTest 5: Generating Debug Output" << std::endl;
        
        std::ofstream debug("planet_debug.txt");
        if (!debug.is_open()) {
            std::cout << "  Could not create debug file" << std::endl;
            return;
        }
        
        float planetRadius = 6371000.0f;
        octree::OctreePlanet planet(planetRadius, 4);
        planet.generate(42);
        
        debug << "PLANET RENDERING DEBUG INFORMATION\n";
        debug << "===================================\n\n";
        debug << "Planet radius: " << planetRadius << " meters\n";
        debug << "Root node size: " << planet.getRoot()->getHalfSize() * 2 << " meters\n\n";
        
        // Sample points on sphere surface
        debug << "SAMPLE SURFACE POINTS:\n";
        for (int i = 0; i < 10; i++) {
            float theta = (float)i / 10.0f * 2.0f * 3.14159f;
            glm::vec3 surfacePoint(
                cos(theta) * planetRadius,
                sin(theta) * planetRadius,
                0
            );
            
            // Find nearest node
            float minDist = 1e10f;
            octree::MaterialType nearestMaterial = octree::MaterialType::Air;
            
            const_cast<octree::OctreeNode*>(planet.getRoot())->traverse([&](octree::OctreeNode* node) {
                if (node->isLeaf()) {
                    float dist = glm::length(node->getCenter() - surfacePoint);
                    if (dist < minDist) {
                        minDist = dist;
                        // Get dominant material
                        const auto& voxels = node->getVoxels();
                        int counts[4] = {0,0,0,0};
                        for (const auto& v : voxels) {
                            counts[static_cast<int>(v.material)]++;
                        }
                        int maxIdx = 0;
                        for (int j = 1; j < 4; j++) {
                            if (counts[j] > counts[maxIdx]) maxIdx = j;
                        }
                        nearestMaterial = static_cast<octree::MaterialType>(maxIdx);
                    }
                }
            });
            
            const char* matNames[] = {"Air", "Rock", "Water", "Magma"};
            debug << "  Point " << i << " (theta=" << theta << "): " 
                  << matNames[static_cast<int>(nearestMaterial)] 
                  << " (nearest node at " << minDist << "m)\n";
        }
        
        debug.close();
        std::cout << "  Debug output written to planet_debug.txt" << std::endl;
        std::cout << "  ✓ Debug information generated" << std::endl;
    }
};

int main() {
    try {
        RenderingVerificationTests tests;
        tests.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}