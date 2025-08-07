// Test the pipeline from prepareRenderData to GPU upload
// This test catches the bug where GPU bypasses renderData and reads raw nodes

#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <functional>
#include <glm/glm.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Simulate what GPUOctree::uploadOctree does WRONG (direct traversal)
void simulateWrongGPUUpload(const OctreePlanet* planet) {
    std::cout << "TEST: Simulating WRONG GPU upload (direct traversal)..." << std::endl;
    
    // This is what the buggy code does - collect ALL leaf nodes
    std::vector<const OctreeNode*> allNodes;
    
    std::function<void(const OctreeNode*)> collectNodes = 
        [&collectNodes, &allNodes](const OctreeNode* node) {
        if (node->isLeaf()) {
            allNodes.push_back(node);
        } else {
            const auto& children = node->getChildren();
            for (const auto& child : children) {
                if (child) {
                    collectNodes(child.get());
                }
            }
        }
    };
    
    if (planet->getRoot()) {
        collectNodes(planet->getRoot());
    }
    
    std::cout << "  Collected " << allNodes.size() << " total leaf nodes" << std::endl;
    
    // Check materials in first few nodes
    int airNodes = 0, rockNodes = 0, waterNodes = 0;
    for (size_t i = 0; i < std::min(size_t(100), allNodes.size()); i++) {
        const auto& voxels = allNodes[i]->getVoxels();
        uint8_t dominant = voxels[0].getDominantMaterial();
        if (dominant == 0) airNodes++;
        else if (dominant == 1) rockNodes++;
        else if (dominant == 2) waterNodes++;
    }
    
    std::cout << "  First 100 nodes: " << airNodes << " air, " 
              << rockNodes << " rock, " << waterNodes << " water" << std::endl;
              
    // This will include ALL nodes, including far-away air nodes!
    float totalNodes = allNodes.size();
    int totalAirNodes = 0;
    for (const auto* node : allNodes) {
        const auto& voxels = node->getVoxels();
        bool allAir = true;
        for (int i = 0; i < 8; i++) {
            if (voxels[i].getDominantMaterial() != 0) {
                allAir = false;
                break;
            }
        }
        if (allAir) totalAirNodes++;
    }
    
    float airPercent = (totalAirNodes / totalNodes) * 100.0f;
    std::cout << "  Total: " << totalAirNodes << "/" << allNodes.size() 
              << " nodes are pure air (" << airPercent << "%)" << std::endl;
              
    if (airPercent > 50.0f) {
        std::cout << "  ❌ PROBLEM: Majority of nodes are air!" << std::endl;
        std::cout << "     GPU will waste time processing empty space!" << std::endl;
    }
}

// Simulate what SHOULD happen - use prepareRenderData
void simulateCorrectGPUUpload(OctreePlanet* planet) {
    std::cout << "TEST: Simulating CORRECT GPU upload (using renderData)..." << std::endl;
    
    // Get render data (filtered, visible nodes only)
    glm::vec3 viewPos(0, 0, planet->getRadius() * 1.5f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet->prepareRenderData(viewPos, viewProj);
    
    std::cout << "  RenderData has " << renderData.nodes.size() << " visible nodes" << std::endl;
    std::cout << "  RenderData has " << renderData.voxels.size() << " voxels" << std::endl;
    
    // Check materials in render data
    int airVoxels = 0, rockVoxels = 0, waterVoxels = 0;
    for (const auto& voxel : renderData.voxels) {
        uint8_t dominant = voxel.getDominantMaterial();
        if (dominant == 0) airVoxels++;
        else if (dominant == 1) rockVoxels++;
        else if (dominant == 2) waterVoxels++;
    }
    
    std::cout << "  Voxel materials: " << airVoxels << " air, " 
              << rockVoxels << " rock, " << waterVoxels << " water" << std::endl;
              
    float airPercent = (float)airVoxels / renderData.voxels.size() * 100.0f;
    std::cout << "  Air percentage in visible voxels: " << airPercent << "%" << std::endl;
    
    if (airPercent < 10.0f) {
        std::cout << "  ✓ Good! Visible nodes are mostly solid material" << std::endl;
    }
}

// Test that shows the difference
void test_render_vs_direct_traversal() {
    std::cout << "\nTEST: Compare renderData vs direct traversal..." << std::endl;
    
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 7);
    planet.generate(42);
    
    // First, check what prepareRenderData gives us
    glm::vec3 viewPos(0, 0, earthRadius * 1.5f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    std::cout << "  prepareRenderData results:" << std::endl;
    std::cout << "    - " << renderData.nodes.size() << " nodes" << std::endl;
    std::cout << "    - " << renderData.voxels.size() << " voxels" << std::endl;
    std::cout << "    - " << renderData.visibleNodes.size() << " visible node indices" << std::endl;
    
    // Count materials in renderData voxels
    int renderAir = 0, renderRock = 0, renderWater = 0;
    for (const auto& voxel : renderData.voxels) {
        uint8_t mat = voxel.getDominantMaterial();
        if (mat == 0) renderAir++;
        else if (mat == 1) renderRock++;
        else if (mat == 2) renderWater++;
    }
    
    std::cout << "    - Materials: " << renderAir << " air, " 
              << renderRock << " rock, " << renderWater << " water" << std::endl;
    
    // Now count what direct traversal would give
    int totalLeaves = 0;
    int airLeaves = 0;
    
    std::function<void(const OctreeNode*)> countLeaves = 
        [&](const OctreeNode* node) {
        if (node->isLeaf()) {
            totalLeaves++;
            
            // Check if all voxels are air
            bool allAir = true;
            for (const auto& voxel : node->getVoxels()) {
                if (voxel.getDominantMaterial() != 0) {
                    allAir = false;
                    break;
                }
            }
            if (allAir) airLeaves++;
        } else {
            for (const auto& child : node->getChildren()) {
                if (child) countLeaves(child.get());
            }
        }
    };
    
    countLeaves(planet.getRoot());
    
    std::cout << "\n  Direct traversal results:" << std::endl;
    std::cout << "    - " << totalLeaves << " total leaf nodes" << std::endl;
    std::cout << "    - " << airLeaves << " pure air nodes" << std::endl;
    std::cout << "    - " << (totalLeaves - airLeaves) << " nodes with materials" << std::endl;
    
    // The key insight!
    std::cout << "\n  CRITICAL DIFFERENCE:" << std::endl;
    std::cout << "    prepareRenderData: " << renderData.nodes.size() << " nodes (filtered)" << std::endl;
    std::cout << "    Direct traversal:  " << totalLeaves << " nodes (ALL leaves)" << std::endl;
    
    float reduction = (1.0f - (float)renderData.nodes.size() / totalLeaves) * 100.0f;
    std::cout << "    -> prepareRenderData reduces node count by " << reduction << "%!" << std::endl;
    
    // Test assertion
    assert(renderData.nodes.size() < totalLeaves / 2 && 
           "RenderData should have significantly fewer nodes than total leaves");
    
    std::cout << "\n  ✓ Test shows prepareRenderData properly filters nodes" << std::endl;
}

// Test what the GPU actually processes
void test_gpu_material_extraction() {
    std::cout << "\nTEST: GPU material extraction from nodes..." << std::endl;
    
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 6); // Smaller for faster test
    planet.generate(42);
    
    // Get render data
    glm::vec3 viewPos(0, 0, earthRadius * 1.5f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Simulate GPU material extraction (like in gpu_octree.cpp)
    int fallbackUsed = 0;
    int properMaterials = 0;
    
    for (const auto& node : renderData.nodes) {
        bool isLeaf = (node.flags & 1) != 0;
        
        if (isLeaf) {
            uint32_t voxelIdx = node.voxelIndex;
            
            if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                // Count materials
                uint32_t materialCounts[4] = {0, 0, 0, 0};
                
                for (int i = 0; i < 8; i++) {
                    uint32_t mat = renderData.voxels[voxelIdx + i].getDominantMaterial();
                    if (mat < 4) materialCounts[mat]++;
                }
                
                uint32_t maxCount = 0;
                for (int m = 0; m < 4; m++) {
                    if (materialCounts[m] > maxCount) {
                        maxCount = materialCounts[m];
                    }
                }
                
                if (maxCount == 0) {
                    // Would use fallback!
                    fallbackUsed++;
                    
                    // Debug first few fallbacks
                    if (fallbackUsed <= 3) {
                        std::cout << "  Node would use fallback at pos (" 
                                  << node.center.x << ", " << node.center.y 
                                  << ", " << node.center.z << ")" << std::endl;
                        std::cout << "    Voxel materials:";
                        for (int i = 0; i < 8; i++) {
                            std::cout << " " << (int)renderData.voxels[voxelIdx + i].getDominantMaterial();
                        }
                        std::cout << std::endl;
                    }
                } else {
                    properMaterials++;
                }
            }
        }
    }
    
    std::cout << "  Results: " << properMaterials << " nodes with materials, " 
              << fallbackUsed << " would use fallback" << std::endl;
    
    if (fallbackUsed == 0) {
        std::cout << "  ✓ No fallback needed when using renderData!" << std::endl;
    } else {
        std::cout << "  ❌ Some nodes still trigger fallback!" << std::endl;
    }
    
    assert(fallbackUsed == 0 && "GPU should not need fallback with renderData");
}

int main() {
    std::cout << "=== Render to GPU Pipeline Test ===" << std::endl;
    std::cout << "Testing the data flow from prepareRenderData to GPU\n" << std::endl;
    
    try {
        // Show the problem
        float earthRadius = 6371000.0f;
        OctreePlanet planet(earthRadius, 5); // Smaller for demo
        planet.generate(42);
        
        simulateWrongGPUUpload(&planet);
        std::cout << std::endl;
        simulateCorrectGPUUpload(&planet);
        
        // Run detailed tests
        test_render_vs_direct_traversal();
        test_gpu_material_extraction();
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        std::cout << "Tests confirm: GPU should use renderData, not direct traversal!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}