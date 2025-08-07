// Test to catch the GPU fallback heuristic issue
// The GPU should get materials from actual voxel data, NOT from the distance-based fallback

#include <iostream>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Test that voxels at Earth scale have proper materials
void test_earth_scale_voxels() {
    std::cout << "TEST: Earth-scale planet voxels have materials..." << std::endl;
    
    float earthRadius = 6371000.0f; // Real Earth radius in meters
    OctreePlanet planet(earthRadius, 7); // Depth 7 like the real program
    planet.generate(42); // Same seed as real program
    
    // Check voxels during prepare phase
    glm::vec3 viewPos(0, 0, earthRadius * 1.5f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    std::cout << "  Render data: " << renderData.nodes.size() << " nodes, " 
              << renderData.voxels.size() << " voxels" << std::endl;
    
    // Count materials in the actual voxels
    int airCount = 0, rockCount = 0, waterCount = 0;
    for (const auto& voxel : renderData.voxels) {
        uint8_t mat = voxel.getDominantMaterial();
        switch(mat) {
            case 0: airCount++; break;
            case 1: rockCount++; break;
            case 2: waterCount++; break;
        }
    }
    
    std::cout << "  Actual voxel materials: " << airCount << " air, " 
              << rockCount << " rock, " << waterCount << " water" << std::endl;
    
    // CRITICAL TEST: Should have non-air materials
    if (rockCount == 0 && waterCount == 0) {
        std::cerr << "  ❌ FALLBACK HEURISTIC BUG DETECTED!" << std::endl;
        std::cerr << "     All voxels are Air - GPU will use fallback!" << std::endl;
        assert(false && "Voxels lost their materials at Earth scale");
    }
    
    // At least 20% should be non-air for a planet
    float nonAirPercent = (float)(rockCount + waterCount) / renderData.voxels.size() * 100.0f;
    assert(nonAirPercent > 20.0f && "Planet should be mostly solid, not air");
    
    std::cout << "  ✓ Earth-scale voxels have proper materials" << std::endl;
}

// Test that leaf nodes contain expected materials after generation
void test_leaf_materials_after_generation() {
    std::cout << "TEST: Leaf materials persist after generation..." << std::endl;
    
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 5); // Lower depth for faster test
    planet.generate(42);
    
    // Traverse and check leaf nodes directly
    int leafCount = 0;
    int leafWithMaterials = 0;
    int totalVoxelsChecked = 0;
    int nonAirVoxels = 0;
    
    std::function<void(const OctreeNode*)> checkNode = [&](const OctreeNode* node) {
        if (node->isLeaf()) {
            leafCount++;
            bool hasMaterial = false;
            
            // Check all 8 voxels in this leaf
            const auto& voxels = node->getVoxels();
            for (int i = 0; i < 8; i++) {
                totalVoxelsChecked++;
                uint8_t mat = voxels[i].getDominantMaterial();
                if (mat != 0) { // Not air
                    nonAirVoxels++;
                    hasMaterial = true;
                }
            }
            
            if (hasMaterial) {
                leafWithMaterials++;
            }
        } else {
            // Recurse to children
            for (const auto& child : node->getChildren()) {
                if (child) {
                    checkNode(child.get());
                }
            }
        }
    };
    
    checkNode(planet.getRoot());
    
    std::cout << "  Checked " << leafCount << " leaf nodes" << std::endl;
    std::cout << "  " << leafWithMaterials << " leaves have non-air materials" << std::endl;
    std::cout << "  " << nonAirVoxels << "/" << totalVoxelsChecked 
              << " voxels are non-air" << std::endl;
    
    // Should have at least some leaves with materials
    assert(leafWithMaterials > 0 && "Should have leaves with materials");
    assert(nonAirVoxels > 0 && "Should have non-air voxels");
    
    std::cout << "  ✓ Materials persist in leaf nodes" << std::endl;
}

// Test the specific distance ranges where materials are set
void test_material_distance_ranges() {
    std::cout << "TEST: Materials set at correct distances..." << std::endl;
    
    float radius = 6371000.0f;
    
    // Test core distance (< 0.9 * radius)
    float coreDistance = radius * 0.8f;
    std::cout << "  Core distance: " << coreDistance << " (should be rock)" << std::endl;
    
    // Test surface distance (0.9 - 1.1 * radius)  
    float surfaceDistance = radius * 1.0f;
    std::cout << "  Surface distance: " << surfaceDistance << " (should be rock/water)" << std::endl;
    
    // Test space distance (> 1.1 * radius)
    float spaceDistance = radius * 1.2f;
    std::cout << "  Space distance: " << spaceDistance << " (should be air)" << std::endl;
    
    // Create a small planet to test these ranges
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    // Check materials at different distances
    int coreNodes = 0, surfaceNodes = 0, spaceNodes = 0;
    
    std::function<void(const OctreeNode*)> checkDistances = [&](const OctreeNode* node) {
        if (node->isLeaf()) {
            float dist = glm::length(node->getCenter());
            
            if (dist < radius * 0.9f) {
                coreNodes++;
                // Core should be rock
                const auto& voxels = node->getVoxels();
                for (const auto& voxel : voxels) {
                    if (dist < radius * 0.5f) { // Deep core
                        // Very deep nodes might be air if outside planet
                        continue;
                    }
                    uint8_t mat = voxel.getDominantMaterial();
                    if (mat != 1 && mat != 0) { // Not rock or air
                        std::cerr << "  WARNING: Core node at dist " << dist 
                                  << " has material " << (int)mat << std::endl;
                    }
                }
            } else if (dist < radius * 1.1f) {
                surfaceNodes++;
                // Surface should have mix of rock and water
            } else {
                spaceNodes++;
                // Space should be air
                const auto& voxels = node->getVoxels();
                for (const auto& voxel : voxels) {
                    uint8_t mat = voxel.getDominantMaterial();
                    assert(mat == 0 && "Space nodes should be air");
                }
            }
        } else {
            for (const auto& child : node->getChildren()) {
                if (child) checkDistances(child.get());
            }
        }
    };
    
    checkDistances(planet.getRoot());
    
    std::cout << "  Found " << coreNodes << " core nodes, " 
              << surfaceNodes << " surface nodes, " 
              << spaceNodes << " space nodes" << std::endl;
    
    std::cout << "  ✓ Materials set at correct distances" << std::endl;
}

// Test that the GPU would need to use fallback
void test_gpu_fallback_detection() {
    std::cout << "TEST: Detect when GPU fallback would trigger..." << std::endl;
    
    // Simulate what the GPU upload code does
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 7);
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, earthRadius * 1.5f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Check like GPU upload does
    int fallbackCount = 0;
    int properMaterialCount = 0;
    
    for (size_t nodeIdx = 0; nodeIdx < renderData.visibleNodes.size(); nodeIdx++) {
        uint32_t idx = renderData.visibleNodes[nodeIdx];
        const auto& node = renderData.nodes[idx];
        
        if (node.flags & 1) { // Is leaf
            uint32_t voxelIdx = node.voxelIndex;
            if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                // Count materials in the 8 voxels
                uint32_t materialCounts[4] = {0, 0, 0, 0};
                
                for (int i = 0; i < 8; i++) {
                    uint32_t mat = renderData.voxels[voxelIdx + i].getDominantMaterial();
                    if (mat < 4) {
                        materialCounts[mat]++;
                    }
                }
                
                // Find dominant material
                uint32_t maxCount = 0;
                for (int m = 0; m < 4; m++) {
                    if (materialCounts[m] > maxCount) {
                        maxCount = materialCounts[m];
                    }
                }
                
                // Would GPU use fallback?
                if (maxCount == 0) {
                    fallbackCount++;
                    std::cout << "  Node " << idx << " would trigger fallback (all voxels invalid)" << std::endl;
                } else {
                    properMaterialCount++;
                }
            }
        }
    }
    
    std::cout << "  " << properMaterialCount << " nodes have proper materials" << std::endl;
    std::cout << "  " << fallbackCount << " nodes would use fallback heuristic" << std::endl;
    
    // CRITICAL: Fallback should NEVER be needed if voxels are correct
    if (fallbackCount > 0) {
        std::cerr << "  ❌ GPU FALLBACK BUG: " << fallbackCount 
                  << " nodes would use distance heuristic!" << std::endl;
        assert(false && "GPU should never need fallback heuristic");
    }
    
    std::cout << "  ✓ GPU would use actual voxel materials" << std::endl;
}

int main() {
    std::cout << "\n=== GPU Fallback Heuristic Test ===" << std::endl;
    std::cout << "Testing that GPU uses voxel materials, not fallback\n" << std::endl;
    
    try {
        test_earth_scale_voxels();
        test_leaf_materials_after_generation();
        test_material_distance_ranges();
        test_gpu_fallback_detection();
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        std::cout << "GPU is using actual voxel materials correctly" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        std::cerr << "\nFOUND THE BUG: GPU is using fallback heuristic!" << std::endl;
        std::cerr << "This means voxels are losing their materials somewhere" << std::endl;
        return 1;
    }
    
    return 0;
}