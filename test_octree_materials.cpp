#include <iostream>
#include <vector>
#include <cmath>

// Test the material detection logic
int main() {
    // Test the dominant material calculation
    std::cout << "Testing material calculation logic:\n\n";
    
    // Simulate what happens in gpu_octree.cpp
    std::cout << "Test 1: All air voxels (should give air)\n";
    uint32_t materialCounts[4] = {8, 0, 0, 0}; // All air
    
    // Find dominant material
    uint32_t maxCount = 0;
    uint32_t dominantMaterial = 0;
    for (int m = 0; m < 4; m++) {
        if (materialCounts[m] > maxCount) {
            maxCount = materialCounts[m];
            dominantMaterial = m;
        }
    }
    std::cout << "  Result: material=" << dominantMaterial << " (0=Air, 1=Rock, 2=Water, 3=Magma)\n\n";
    
    // Test 2: Mixed materials
    std::cout << "Test 2: Mixed voxels (4 rock, 3 water, 1 air)\n";
    materialCounts[0] = 1; // Air
    materialCounts[1] = 4; // Rock  
    materialCounts[2] = 3; // Water
    materialCounts[3] = 0; // Magma
    
    maxCount = 0;
    dominantMaterial = 0;
    for (int m = 0; m < 4; m++) {
        if (materialCounts[m] > maxCount) {
            maxCount = materialCounts[m];
            dominantMaterial = m;
        }
    }
    std::cout << "  Result: material=" << dominantMaterial << " (should be 1=Rock)\n\n";
    
    // Test 3: The hardcoded override
    std::cout << "Test 3: Hardcoded override when all materials are 0\n";
    materialCounts[0] = 0;
    materialCounts[1] = 0;
    materialCounts[2] = 0;
    materialCounts[3] = 0;
    
    maxCount = 0;
    dominantMaterial = 0;
    for (int m = 0; m < 4; m++) {
        if (materialCounts[m] > maxCount) {
            maxCount = materialCounts[m];
            dominantMaterial = m;
        }
    }
    
    // This is what happens in the actual code:
    std::cout << "  Before override: material=" << dominantMaterial << "\n";
    
    // Check for the distance-based override
    float nodeDistance = 1.2e7f; // Example distance
    float planetRadius = 6.371e6f;
    
    if (materialCounts[0] == 0 && materialCounts[1] == 0 && 
        materialCounts[2] == 0 && materialCounts[3] == 0) {
        // No materials counted - use distance heuristic
        if (nodeDistance > planetRadius * 1.5f) {
            dominantMaterial = 0; // Air
        } else {
            dominantMaterial = 2; // Default to water (THIS IS THE PROBLEM!)
        }
        std::cout << "  After override: material=" << dominantMaterial 
                  << " (distance=" << nodeDistance << ", radius=" << planetRadius << ")\n";
    }
    
    std::cout << "\nPROBLEM IDENTIFIED: When material counts are all 0,\n";
    std::cout << "the code defaults to WATER for nodes inside 1.5*radius!\n";
    std::cout << "This happens when voxels aren't being properly counted.\n";
    
    return 0;
}