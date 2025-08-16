// STEP 6: Use the EXACT production transform generation
// Let's include the actual GlobalPatchGenerator header

#include <iostream>
#include <iomanip>
#include "core/global_patch_generator.hpp"

// The exact cube-to-sphere from production
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos, double radius) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos) * radius;
}

int main() {
    std::cout << "=== USING EXACT PRODUCTION TRANSFORMS ===\n\n";
    
    const double radius = 6371000.0;
    
    // Create two patches at the boundary between +X and +Y faces
    // These SHOULD share an edge at X=1, Y=1
    
    // Patch 1: +X face, top-right quadrant
    core::GlobalPatchGenerator::GlobalPatch patch1;
    patch1.minBounds = glm::dvec3(1.0, 0.0, 0.0);
    patch1.maxBounds = glm::dvec3(1.0, 1.0, 1.0);
    patch1.center = glm::dvec3(1.0, 0.5, 0.5);
    patch1.level = 1;
    patch1.faceId = 0; // +X face
    
    // Patch 2: +Y face, top-right quadrant  
    core::GlobalPatchGenerator::GlobalPatch patch2;
    patch2.minBounds = glm::dvec3(0.0, 1.0, 0.0);
    patch2.maxBounds = glm::dvec3(1.0, 1.0, 1.0);
    patch2.center = glm::dvec3(0.5, 1.0, 0.5);
    patch2.level = 1;
    patch2.faceId = 2; // +Y face
    
    std::cout << "Patch 1 (+X face): bounds (" 
              << patch1.minBounds.x << "," << patch1.minBounds.y << "," << patch1.minBounds.z
              << ") to ("
              << patch1.maxBounds.x << "," << patch1.maxBounds.y << "," << patch1.maxBounds.z << ")\n";
    
    std::cout << "Patch 2 (+Y face): bounds (" 
              << patch2.minBounds.x << "," << patch2.minBounds.y << "," << patch2.minBounds.z
              << ") to ("
              << patch2.maxBounds.x << "," << patch2.maxBounds.y << "," << patch2.maxBounds.z << ")\n\n";
    
    // They share the edge from (1,1,0) to (1,1,1)
    std::cout << "Shared edge should be from (1,1,0) to (1,1,1)\n\n";
    
    // Generate transforms using production code
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    // Test specific points along the shared edge
    std::cout << "=== TESTING SHARED EDGE VERTICES ===\n";
    
    for (int i = 0; i <= 4; i++) {
        float t = i / 4.0f;
        
        // For +X face: The edge at Y=1 is UV (t, 1)
        // U maps to Z (from 0 to 1), V maps to Y (from 0 to 1)
        glm::dvec3 xCubePos = glm::dvec3(transform1 * glm::dvec4(t, 1, 0, 1));
        
        // For +Y face: The edge at X=1 is UV (1, t)
        // U maps to X (from 0 to 1), V maps to Z (from 0 to 1)
        glm::dvec3 yCubePos = glm::dvec3(transform2 * glm::dvec4(1, t, 0, 1));
        
        std::cout << "\nPoint " << i << " (t=" << t << "):\n";
        std::cout << "  +X UV(" << t << ",1) -> cube(" 
                  << xCubePos.x << ", " << xCubePos.y << ", " << xCubePos.z << ")\n";
        std::cout << "  +Y UV(1," << t << ") -> cube(" 
                  << yCubePos.x << ", " << yCubePos.y << ", " << yCubePos.z << ")\n";
        
        glm::dvec3 diff = xCubePos - yCubePos;
        double cubeDist = glm::length(diff);
        std::cout << "  Cube space distance: " << cubeDist;
        
        if (cubeDist < 0.0001) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ MISMATCH!\n";
            
            // Transform to sphere to see the gap
            glm::dvec3 xSphere = cubeToSphere(xCubePos, radius);
            glm::dvec3 ySphere = cubeToSphere(yCubePos, radius);
            double sphereDist = glm::length(xSphere - ySphere);
            std::cout << "  Sphere space distance: " << sphereDist << " meters\n";
        }
    }
    
    std::cout << "\n=== ANALYSIS ===\n";
    std::cout << "If cube positions don't match, the transforms are wrong.\n";
    std::cout << "If they do match but sphere positions differ, cube-to-sphere is wrong.\n";
    
    return 0;
}