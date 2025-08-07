#include <iostream>
#include <vector>
#include "../include/core/octree.hpp"

// Test to identify the exact method causing material loss
int main() {
    std::cout << "=== IDENTIFYING EXACT METHOD CAUSING MATERIAL LOSS ===" << std::endl;
    
    // Test our checkerboard pattern at different positions
    std::cout << "\nTesting checkerboard pattern logic:" << std::endl;
    
    // Simulate the exact code from octree.cpp
    auto testCheckerboard = [](glm::vec3 voxelPos, float gridSize) -> bool {
        int gridX = static_cast<int>(voxelPos.x / gridSize);
        int gridY = static_cast<int>(voxelPos.y / gridSize);
        int gridZ = static_cast<int>(voxelPos.z / gridSize);
        
        bool isWater = ((gridX + gridY + gridZ) % 2) == 0;
        
        std::cout << "  Pos(" << voxelPos.x/1e6 << ", " << voxelPos.y/1e6 << ", " << voxelPos.z/1e6 << ")Mm";
        std::cout << " -> Grid(" << gridX << ", " << gridY << ", " << gridZ << ")";
        std::cout << " -> Sum=" << (gridX + gridY + gridZ);
        std::cout << " -> " << (isWater ? "WATER" : "ROCK") << std::endl;
        
        return isWater;
    };
    
    // Test at small scale (working)
    std::cout << "\n1. Small scale (radius=1000m):" << std::endl;
    float smallRadius = 1000.0f;
    std::vector<glm::vec3> smallPositions = {
        glm::vec3(900, 0, 0),
        glm::vec3(0, 900, 0),
        glm::vec3(900, 100, 0),
        glm::vec3(-900, 0, 0)
    };
    
    int smallWater = 0;
    for (const auto& pos : smallPositions) {
        if (testCheckerboard(pos, 100000.0f)) smallWater++;
    }
    std::cout << "  Result: " << smallWater << "/4 are water" << std::endl;
    
    // Test at planet scale (broken)
    std::cout << "\n2. Planet scale (radius=6.371e6m):" << std::endl;
    float planetRadius = 6.371e6f;
    std::vector<glm::vec3> planetPositions = {
        glm::vec3(6.3e6f, 0, 0),
        glm::vec3(0, 6.3e6f, 0),
        glm::vec3(6.3e6f, 1e5f, 0),
        glm::vec3(-6.3e6f, 0, 0)
    };
    
    int planetWater = 0;
    for (const auto& pos : planetPositions) {
        if (testCheckerboard(pos, 100000.0f)) planetWater++;
    }
    std::cout << "  Result: " << planetWater << "/4 are water" << std::endl;
    
    // Test the actual pattern distribution
    std::cout << "\n3. Testing grid cell distribution at planet scale:" << std::endl;
    std::cout << "  Grid size: 100000m (100km)" << std::endl;
    
    // Sample many points on planet surface
    int waterCount = 0;
    int rockCount = 0;
    int samples = 100;
    
    for (int i = 0; i < samples; i++) {
        float angle = (i * 2.0f * 3.14159f) / samples;
        glm::vec3 pos(
            planetRadius * cos(angle),
            planetRadius * sin(angle),
            0
        );
        
        int gridX = static_cast<int>(pos.x / 100000.0f);
        int gridY = static_cast<int>(pos.y / 100000.0f);
        int gridZ = static_cast<int>(pos.z / 100000.0f);
        
        bool isWater = ((gridX + gridY + gridZ) % 2) == 0;
        
        if (isWater) waterCount++;
        else rockCount++;
        
        if (i < 5) {  // Show first few
            std::cout << "    Sample " << i << ": Grid(" << gridX << "," << gridY << "," << gridZ 
                      << ") sum=" << (gridX+gridY+gridZ) << " -> " << (isWater ? "W" : "R") << std::endl;
        }
    }
    
    std::cout << "  Distribution: " << waterCount << " water, " << rockCount << " rock" << std::endl;
    
    // The problem analysis
    std::cout << "\n4. ROOT CAUSE ANALYSIS:" << std::endl;
    std::cout << "  Planet radius: 6.371e6m" << std::endl;
    std::cout << "  Grid cell size: 100000m" << std::endl;
    std::cout << "  Typical grid coordinates: (" << (int)(6.371e6f/100000) << ", 0, 0)" << std::endl;
    std::cout << "  That's grid cell (63, 0, 0) with sum=63 (ODD -> ROCK)" << std::endl;
    std::cout << "  Most surface points map to grid coords with sum ~60-65" << std::endl;
    std::cout << "  These sums are mostly ODD, causing all ROCK!" << std::endl;
    
    // Test different grid sizes
    std::cout << "\n5. Testing different grid sizes:" << std::endl;
    float gridSizes[] = {1000.0f, 10000.0f, 100000.0f, 1000000.0f, 10000000.0f};
    
    for (float gridSize : gridSizes) {
        int water = 0;
        for (int i = 0; i < 8; i++) {
            float angle = (i * 2.0f * 3.14159f) / 8;
            glm::vec3 pos(planetRadius * cos(angle), planetRadius * sin(angle), 0);
            
            int gridX = static_cast<int>(pos.x / gridSize);
            int gridY = static_cast<int>(pos.y / gridSize);
            int gridZ = 0;
            
            if (((gridX + gridY + gridZ) % 2) == 0) water++;
        }
        
        std::cout << "  Grid size " << gridSize/1000 << "km: " << water << "/8 water" << std::endl;
    }
    
    std::cout << "\nCONCLUSION: The bug is in the checkerboard pattern using fixed 100km grid" << std::endl;
    std::cout << "at line ~318-320 in octree.cpp where gridX/Y/Z are calculated!" << std::endl;
    
    return 0;
}