// Simple test to understand voxel color patterns
#include <iostream>
#include <glm/glm.hpp>

int main() {
    // Simulate a leaf node center
    glm::vec3 nodeCenter(100.0f, 200.0f, 300.0f);
    float halfSize = 10.0f;
    
    // Corner positions (marching cubes convention)
    glm::vec3 minCorner = nodeCenter - glm::vec3(halfSize);
    glm::vec3 corners[8] = {
        minCorner + glm::vec3(0, 0, 0),               // Corner 0
        minCorner + glm::vec3(halfSize*2, 0, 0),      // Corner 1
        minCorner + glm::vec3(halfSize*2, halfSize*2, 0),    // Corner 2
        minCorner + glm::vec3(0, halfSize*2, 0),             // Corner 3
        minCorner + glm::vec3(0, 0, halfSize*2),             // Corner 4
        minCorner + glm::vec3(halfSize*2, 0, halfSize*2),    // Corner 5
        minCorner + glm::vec3(halfSize*2, halfSize*2, halfSize*2), // Corner 6
        minCorner + glm::vec3(0, halfSize*2, halfSize*2)     // Corner 7
    };
    
    std::cout << "Node center: (" << nodeCenter.x << ", " << nodeCenter.y << ", " << nodeCenter.z << ")" << std::endl;
    std::cout << "\nCorner positions and voxel indices:" << std::endl;
    
    // Method 1: Current code (possibly buggy)
    std::cout << "\nMethod 1 (current code - checking if > center):" << std::endl;
    for (int i = 0; i < 8; i++) {
        glm::vec3 localPos = corners[i] - nodeCenter;
        int voxelIndex = 0;
        if (localPos.x > 0) voxelIndex |= 1;
        if (localPos.y > 0) voxelIndex |= 2;
        if (localPos.z > 0) voxelIndex |= 4;
        
        std::cout << "  Corner " << i << " at (" 
                  << corners[i].x << ", " << corners[i].y << ", " << corners[i].z << ")"
                  << " -> local (" << localPos.x << ", " << localPos.y << ", " << localPos.z << ")"
                  << " -> voxel " << voxelIndex << std::endl;
    }
    
    // Method 2: Direct mapping
    std::cout << "\nMethod 2 (direct corner->voxel mapping):" << std::endl;
    for (int i = 0; i < 8; i++) {
        std::cout << "  Corner " << i << " -> voxel " << i << std::endl;
    }
    
    // Method 3: Corrected octant calculation
    std::cout << "\nMethod 3 (corrected - based on which half):" << std::endl;
    for (int i = 0; i < 8; i++) {
        glm::vec3 localPos = corners[i] - minCorner;  // Relative to min corner
        int voxelIndex = 0;
        if (localPos.x >= halfSize) voxelIndex |= 1;
        if (localPos.y >= halfSize) voxelIndex |= 2; 
        if (localPos.z >= halfSize) voxelIndex |= 4;
        
        std::cout << "  Corner " << i << " at (" 
                  << corners[i].x << ", " << corners[i].y << ", " << corners[i].z << ")"
                  << " -> local (" << localPos.x << ", " << localPos.y << ", " << localPos.z << ")"
                  << " -> voxel " << voxelIndex << std::endl;
    }
    
    return 0;
}