#include <iostream>
#include "../include/core/mixed_voxel.hpp"

int main() {
    std::cout << "=== COLOR DEBUG TEST ===" << std::endl;
    
    // Test core voxel (what we're seeing as green)
    {
        octree::MixedVoxel core = octree::MixedVoxel::createPure(255, 0, 0);
        core.temperature = 255;
        core.pressure = 255;
        
        glm::vec3 color = core.getColor();
        std::cout << "\nCore voxel (rock=255, water=0, air=0, temp=255):" << std::endl;
        std::cout << "  Color RGB: (" << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
        std::cout << "  Expected: reddish-brown (~0.65, ~0.45, ~0.3)" << std::endl;
        
        if (color.y > color.x && color.y > color.z) {
            std::cout << "  WARNING: Green is dominant! This explains the bright green planet!" << std::endl;
        }
    }
    
    // Test surface water
    {
        octree::MixedVoxel water = octree::MixedVoxel::createPure(0, 255, 0);
        water.temperature = 128;
        
        glm::vec3 color = water.getColor();
        std::cout << "\nWater voxel (rock=0, water=255, air=0, temp=128):" << std::endl;
        std::cout << "  Color RGB: (" << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
        std::cout << "  Expected: blue (~0.0, ~0.3, ~0.7)" << std::endl;
    }
    
    // Test air
    {
        octree::MixedVoxel air = octree::MixedVoxel::createPure(0, 0, 255);
        air.temperature = 10;
        
        glm::vec3 color = air.getColor();
        std::cout << "\nAir voxel (rock=0, water=0, air=255, temp=10):" << std::endl;
        std::cout << "  Color RGB: (" << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
        std::cout << "  Expected: light blue (~0.65, ~0.8, ~1.0)" << std::endl;
    }
    
    // Test coastline mix
    {
        octree::MixedVoxel coast = octree::MixedVoxel::createPure(128, 127, 0);
        coast.temperature = 128;
        
        glm::vec3 color = coast.getColor();
        std::cout << "\nCoast voxel (rock=128, water=127, air=0, temp=128):" << std::endl;
        std::cout << "  Color RGB: (" << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
        std::cout << "  Expected: brownish-blue mix" << std::endl;
    }
    
    return 0;
}