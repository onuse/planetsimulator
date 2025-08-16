// STEP 7: Use EXACT same bounds as step4 but with production transforms directly
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
    std::cout << "=== TESTING WITH EXACT STEP4 BOUNDS ===\n\n";
    
    const double radius = 6371000.0;
    
    // Use EXACT same bounds as test_methodology_step4.cpp
    
    // Patch 1: +X face, top edge
    core::GlobalPatchGenerator::GlobalPatch patch1;
    patch1.minBounds = glm::dvec3(1.0, 0.5, -0.5);
    patch1.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    patch1.center = glm::dvec3(1.0, 0.75, 0.0);
    patch1.level = 2;
    patch1.faceId = 0; // +X face
    
    // Patch 2: +Y face, right edge  
    core::GlobalPatchGenerator::GlobalPatch patch2;
    patch2.minBounds = glm::dvec3(0.5, 1.0, -0.5);
    patch2.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    patch2.center = glm::dvec3(0.75, 1.0, 0.0);
    patch2.level = 2;
    patch2.faceId = 2; // +Y face
    
    std::cout << "Patch 1 (+X face): bounds (" 
              << patch1.minBounds.x << "," << patch1.minBounds.y << "," << patch1.minBounds.z
              << ") to ("
              << patch1.maxBounds.x << "," << patch1.maxBounds.y << "," << patch1.maxBounds.z << ")\n";
    
    std::cout << "Patch 2 (+Y face): bounds (" 
              << patch2.minBounds.x << "," << patch2.minBounds.y << "," << patch2.minBounds.z
              << ") to ("
              << patch2.maxBounds.x << "," << patch2.maxBounds.y << "," << patch2.maxBounds.z << ")\n\n";
    
    // They share the edge from (1,1,-0.5) to (1,1,0.5)
    std::cout << "Shared edge should be from (1,1,-0.5) to (1,1,0.5)\n\n";
    
    // Generate transforms using production code
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    // Print the transforms
    std::cout << "=== TRANSFORMS ===\n";
    std::cout << "Patch 1 transform:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: (" 
                  << transform1[i][0] << ", " << transform1[i][1] << ", " 
                  << transform1[i][2] << ", " << transform1[i][3] << ")\n";
    }
    
    std::cout << "\nPatch 2 transform:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: (" 
                  << transform2[i][0] << ", " << transform2[i][1] << ", " 
                  << transform2[i][2] << ", " << transform2[i][3] << ")\n";
    }
    
    // Test specific points along the shared edge
    std::cout << "\n=== TESTING SHARED EDGE VERTICES ===\n";
    
    for (int i = 0; i <= 4; i++) {
        float t = i / 4.0f;
        
        // Map t to the actual Z range (-0.5 to 0.5)
        float z = -0.5f + t;
        
        // For +X face: Edge at Y=1 
        // The UV for this edge: U maps to Z, V maps to Y
        // To get Y=1, we need V=1
        // To get Z=z, we need U that maps to z
        // Since patch goes from Z=-0.5 to 0.5, U=t should map correctly
        glm::dvec3 xCubePos = glm::dvec3(transform1 * glm::dvec4(t, 1, 0, 1));
        
        // For +Y face: Edge at X=1
        // The UV for this edge: U maps to X, V maps to Z  
        // To get X=1, we need U=1
        // To get Z=z, we need V=t 
        glm::dvec3 yCubePos = glm::dvec3(transform2 * glm::dvec4(1, t, 0, 1));
        
        std::cout << "\nPoint " << i << " (t=" << t << ", expected Z=" << z << "):\n";
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
            std::cout << "  Difference: (" << diff.x << ", " << diff.y << ", " << diff.z << ")\n";
            
            // Transform to sphere to see the gap
            glm::dvec3 xSphere = cubeToSphere(xCubePos, radius);
            glm::dvec3 ySphere = cubeToSphere(yCubePos, radius);
            double sphereDist = glm::length(xSphere - ySphere);
            std::cout << "  Sphere space distance: " << sphereDist << " meters\n";
        }
    }
    
    return 0;
}