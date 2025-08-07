#include <iostream>
#include <map>
#include "../include/core/octree.hpp"

// Test materials at each stage of the pipeline
int main() {
    std::cout << "=== MATERIAL PIPELINE STAGE TEST ===" << std::endl;
    
    // Stage 1: Test immediately after generation
    std::cout << "\nStage 1: Check materials right after generate()" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(42);
        
        // Immediately get render data
        auto renderData = planet.prepareRenderData(
            glm::vec3(2000, 2000, 2000),
            glm::mat4(1.0f)
        );
        
        std::map<int, int> counts;
        for (const auto& voxel : renderData.voxels) {
            counts[static_cast<int>(voxel.material)]++;
        }
        
        std::cout << "  Voxels in renderData: " << renderData.voxels.size() << std::endl;
        std::cout << "  Air: " << counts[0] << std::endl;
        std::cout << "  Rock: " << counts[1] << std::endl;
        std::cout << "  Water: " << counts[2] << std::endl;
        
        if (counts[2] == 0) {
            std::cout << "  ❌ BUG: No water in renderData!" << std::endl;
        } else {
            std::cout << "  ✓ Water found in renderData" << std::endl;
        }
    }
    
    // Stage 2: Test with multiple prepareRenderData calls
    std::cout << "\nStage 2: Multiple prepareRenderData calls" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(42);
        
        // First call
        auto renderData1 = planet.prepareRenderData(
            glm::vec3(2000, 2000, 2000),
            glm::mat4(1.0f)
        );
        
        int water1 = 0;
        for (const auto& voxel : renderData1.voxels) {
            if (voxel.material == octree::MaterialType::Water) water1++;
        }
        
        // Second call
        auto renderData2 = planet.prepareRenderData(
            glm::vec3(2000, 2000, 2000),
            glm::mat4(1.0f)
        );
        
        int water2 = 0;
        for (const auto& voxel : renderData2.voxels) {
            if (voxel.material == octree::MaterialType::Water) water2++;
        }
        
        std::cout << "  First call: " << water1 << " water voxels" << std::endl;
        std::cout << "  Second call: " << water2 << " water voxels" << std::endl;
        
        if (water1 != water2) {
            std::cout << "  ❌ BUG: Material counts change between calls!" << std::endl;
        }
    }
    
    // Stage 3: Test at different scales
    std::cout << "\nStage 3: Different planet scales" << std::endl;
    {
        float scales[] = {100.0f, 1000.0f, 10000.0f, 100000.0f, 1000000.0f, 6371000.0f};
        
        for (float scale : scales) {
            octree::OctreePlanet planet(scale, 2); // Low depth for speed
            planet.generate(42);
            
            auto renderData = planet.prepareRenderData(
                glm::vec3(scale * 2, scale * 2, scale * 2),
                glm::mat4(1.0f)
            );
            
            int water = 0;
            for (const auto& voxel : renderData.voxels) {
                if (voxel.material == octree::MaterialType::Water) water++;
            }
            
            std::cout << "  Scale " << scale << "m: " << water << " water voxels out of " 
                      << renderData.voxels.size() << std::endl;
            
            if (scale >= 100000.0f && water == 0) {
                std::cout << "    ❌ Water disappears at scale " << scale << std::endl;
            }
        }
    }
    
    // Stage 4: Check node flags
    std::cout << "\nStage 4: Check node structure" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(42);
        
        auto renderData = planet.prepareRenderData(
            glm::vec3(2000, 2000, 2000),
            glm::mat4(1.0f)
        );
        
        int leafNodes = 0;
        int nodesWithVoxels = 0;
        
        for (const auto& node : renderData.nodes) {
            if (node.flags & 1) { // Is leaf
                leafNodes++;
                if (node.voxelIndex != 0xFFFFFFFF) {
                    nodesWithVoxels++;
                }
            }
        }
        
        std::cout << "  Total nodes: " << renderData.nodes.size() << std::endl;
        std::cout << "  Leaf nodes: " << leafNodes << std::endl;
        std::cout << "  Nodes with voxels: " << nodesWithVoxels << std::endl;
        std::cout << "  Total voxels: " << renderData.voxels.size() << std::endl;
        std::cout << "  Expected voxels: " << nodesWithVoxels * 8 << std::endl;
        
        if (renderData.voxels.size() != nodesWithVoxels * 8) {
            std::cout << "  ⚠ Voxel count mismatch!" << std::endl;
        }
    }
    
    return 0;
}