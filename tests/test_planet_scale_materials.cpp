#include <iostream>
#include <vector>
#include <map>
#include "../include/core/octree.hpp"

// Test material generation at real planet scale
int main() {
    std::cout << "=== PLANET SCALE MATERIAL GENERATION TEST ===" << std::endl;
    
    // Test 1: Small scale (known working)
    {
        std::cout << "\nTest 1: Small planet (radius=1000m)" << std::endl;
        octree::OctreePlanet smallPlanet(1000.0f, 3);
        smallPlanet.generate(42);  // seed
        
        auto renderData = smallPlanet.prepareRenderData(
            glm::vec3(2000, 2000, 2000),  // camera pos
            glm::mat4(1.0f)                // view-proj
        );
        
        // Count materials
        std::map<int, int> materialCounts;
        for (const auto& voxel : renderData.voxels) {
            materialCounts[static_cast<int>(voxel.material)]++;
        }
        
        std::cout << "  Material distribution:" << std::endl;
        std::cout << "    Air: " << materialCounts[0] << std::endl;
        std::cout << "    Rock: " << materialCounts[1] << std::endl;
        std::cout << "    Water: " << materialCounts[2] << std::endl;
        std::cout << "    Magma: " << materialCounts[3] << std::endl;
        
        if (materialCounts[2] == 0) {
            std::cout << "  ERROR: No water generated at small scale!" << std::endl;
        } else {
            std::cout << "  OK: Water generated at small scale" << std::endl;
        }
    }
    
    // Test 2: Real planet scale
    {
        std::cout << "\nTest 2: Real planet (radius=6.371e6m)" << std::endl;
        octree::OctreePlanet realPlanet(6.371e6f, 3);  // Reduced depth for faster test
        realPlanet.generate(42);  // seed
        
        auto renderData = realPlanet.prepareRenderData(
            glm::vec3(1e7f, 1e7f, 1e7f),   // camera pos
            glm::mat4(1.0f)                 // view-proj
        );
        
        // Count materials
        std::map<int, int> materialCounts;
        for (const auto& voxel : renderData.voxels) {
            materialCounts[static_cast<int>(voxel.material)]++;
        }
        
        std::cout << "  Material distribution:" << std::endl;
        std::cout << "    Air: " << materialCounts[0] << std::endl;
        std::cout << "    Rock: " << materialCounts[1] << std::endl;
        std::cout << "    Water: " << materialCounts[2] << std::endl;
        std::cout << "    Magma: " << materialCounts[3] << std::endl;
        
        int totalSurface = materialCounts[1] + materialCounts[2];
        if (totalSurface > 0) {
            float waterRatio = static_cast<float>(materialCounts[2]) / totalSurface;
            std::cout << "  Water ratio: " << (waterRatio * 100) << "% (expected ~70%)" << std::endl;
        }
        
        if (materialCounts[2] == 0) {
            std::cout << "  ERROR: No water generated at planet scale!" << std::endl;
            std::cout << "  This is the bug we're seeing in the main app!" << std::endl;
        } else {
            std::cout << "  OK: Water generated at planet scale" << std::endl;
        }
    }
    
    // Test 3: Check noise function at different scales
    {
        std::cout << "\nTest 3: Noise function behavior" << std::endl;
        
        // Test positions at planet surface
        float radius = 6.371e6f;
        std::vector<glm::vec3> testPoints = {
            glm::vec3(radius, 0, 0),
            glm::vec3(0, radius, 0),
            glm::vec3(0, 0, radius),
            glm::vec3(radius * 0.7f, radius * 0.7f, 0)
        };
        
        std::cout << "  Testing noise at surface points:" << std::endl;
        for (const auto& pos : testPoints) {
            // Simulate the noise calculation from octree.cpp
            float scale1 = 0.0001f;
            float scale2 = 0.0003f;
            float scale3 = 0.001f;
            
            // These would need the actual noise generator, but we can test the math
            std::cout << "    Position (" << pos.x/1e6 << ", " << pos.y/1e6 << ", " << pos.z/1e6 << ")Mm:" << std::endl;
            std::cout << "      Scaled: (" << pos.x * scale1 << ", " << pos.y * scale1 << ", " << pos.z * scale1 << ")" << std::endl;
            
            // The issue might be that at large scales, the noise input values become huge
            if (std::abs(pos.x * scale1) > 1000.0f) {
                std::cout << "      WARNING: Noise input exceeds reasonable range!" << std::endl;
            }
        }
    }
    
    return 0;
}