// STEP 2: Investigate the precision issue we found
// This could be a REAL problem causing gaps!

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>

void testPrecisionProblem() {
    std::cout << "=== INVESTIGATING PRECISION ISSUE ===\n\n";
    
    // Planet scale
    const float planetRadius = 6371000.0f;
    const int gridSize = 65; // Actual resolution used
    
    std::cout << "Planet radius: " << std::fixed << std::setprecision(1) << planetRadius << " meters\n";
    std::cout << "Grid resolution: " << gridSize << "x" << gridSize << "\n\n";
    
    // Method 1: Accumulating steps (what a loop might do)
    std::cout << "Method 1: Accumulating in a loop\n";
    float step = 1.0f / (gridSize - 1);
    float accumulated = 0.0f;
    
    for (int i = 0; i < gridSize - 1; i++) {
        accumulated += step;
    }
    
    std::cout << "  Final value: " << std::setprecision(10) << accumulated << "\n";
    std::cout << "  Expected: 1.0\n";
    std::cout << "  Error: " << std::abs(accumulated - 1.0f) << "\n\n";
    
    // Method 2: Direct calculation (what we should do)
    std::cout << "Method 2: Direct calculation\n";
    float direct = static_cast<float>(gridSize - 1) / (gridSize - 1);
    std::cout << "  Final value: " << direct << "\n";
    std::cout << "  Error: " << std::abs(direct - 1.0f) << "\n\n";
    
    // Method 3: Using integer math first
    std::cout << "Method 3: Integer math then divide\n";
    for (int i = 0; i <= 4; i++) {
        float value = static_cast<float>(i * 16) / 64.0f;
        std::cout << "  i=" << i << ": " << value << "\n";
    }
    
    // Now at planet scale
    std::cout << "\n=== AT PLANET SCALE ===\n\n";
    
    // Transform to cube position then to world
    std::cout << "Edge vertex positions (should all have same X):\n";
    for (int i = 0; i < 5; i++) {
        // UV coordinate
        float u = static_cast<float>(i) / 4.0f;
        
        // Transform to cube position (on +X face)
        glm::vec3 cubePos(1.0f, -1.0f + 2.0f * u, 0.0f);
        
        // To sphere (simplified)
        glm::vec3 spherePos = glm::normalize(cubePos) * planetRadius;
        
        std::cout << "  i=" << i << " u=" << u;
        std::cout << " cube=(" << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")";
        std::cout << " world=(" << std::fixed << std::setprecision(2) 
                  << spherePos.x << "," << spherePos.y << "," << spherePos.z << ")\n";
    }
    
    std::cout << "\n=== THE REAL PROBLEM ===\n\n";
    
    // Simulate what happens with float vs double
    std::cout << "Float precision:\n";
    float f1 = 6371000.0f;
    float f2 = 6371000.0f + 1.0f; // Add 1 meter
    std::cout << "  " << f1 << " + 1.0 = " << f2 << "\n";
    std::cout << "  Difference: " << (f2 - f1) << " (should be 1.0)\n\n";
    
    std::cout << "Double precision:\n";
    double d1 = 6371000.0;
    double d2 = 6371000.0 + 1.0;
    std::cout << "  " << d1 << " + 1.0 = " << d2 << "\n";
    std::cout << "  Difference: " << (d2 - d1) << " (should be 1.0)\n\n";
    
    // Key insight
    std::cout << "=== KEY INSIGHT ===\n";
    std::cout << "At planet scale (6.37 million meters), float precision is ~0.5 meters!\n";
    std::cout << "This means vertices can be off by meters just from rounding.\n";
    std::cout << "This could explain gaps at face boundaries!\n\n";
    
    std::cout << "SOLUTION: Use double precision for vertex generation,\n";
    std::cout << "only convert to float at the very end.\n";
}

int main() {
    testPrecisionProblem();
    return 0;
}