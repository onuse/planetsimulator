#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

const double PLANET_RADIUS = 6371000.0;

// Cube to sphere projection
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

int main() {
    std::cout << "=== ACTUAL GAP TEST WITH GLOBALATCHGENERATOR ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Create the EXACT patches from test 27
    // Test 1: +Z face meets +X face
    
    // +Z patch (face 4) - Right side of +Z
    core::GlobalPatchGenerator::GlobalPatch zPatch;
    zPatch.minBounds = glm::vec3(0.0, -0.5, 1.0);  // Note: Different from test!
    zPatch.maxBounds = glm::vec3(1.0, 0.5, 1.0);
    zPatch.center = (zPatch.minBounds + zPatch.maxBounds) * 0.5f;
    zPatch.level = 0;
    zPatch.faceId = 4;
    
    // +X patch (face 0) - Top side of +X
    core::GlobalPatchGenerator::GlobalPatch xPatch;
    xPatch.minBounds = glm::vec3(1.0, -0.5, 0.0);  // Note: Different from test!
    xPatch.maxBounds = glm::vec3(1.0, 0.5, 1.0);
    xPatch.center = (xPatch.minBounds + xPatch.maxBounds) * 0.5f;
    xPatch.level = 0;
    xPatch.faceId = 0;
    
    std::cout << "+Z Patch bounds: (" << zPatch.minBounds.x << "," << zPatch.minBounds.y << "," << zPatch.minBounds.z 
              << ") to (" << zPatch.maxBounds.x << "," << zPatch.maxBounds.y << "," << zPatch.maxBounds.z << ")\n";
    std::cout << "+X Patch bounds: (" << xPatch.minBounds.x << "," << xPatch.minBounds.y << "," << xPatch.minBounds.z 
              << ") to (" << xPatch.maxBounds.x << "," << xPatch.maxBounds.y << "," << xPatch.maxBounds.z << ")\n\n";
    
    // Get transforms
    glm::dmat4 zTransform = zPatch.createTransform();
    glm::dmat4 xTransform = xPatch.createTransform();
    
    std::cout << "Testing points along what SHOULD be the shared edge:\n";
    std::cout << "The edge where +Z (x=1) meets +X (z=1) at (1, y, 1)\n\n";
    
    // Test several points
    for (double t = 0.0; t <= 1.0; t += 0.5) {
        // For +Z patch: right edge is at u=1, v varies
        glm::dvec4 zUV(1.0, t, 0.0, 1.0);
        glm::dvec3 zCube = glm::dvec3(zTransform * zUV);
        
        // For +X patch: top edge is at v=1, u varies  
        glm::dvec4 xUV(t, 1.0, 0.0, 1.0);
        glm::dvec3 xCube = glm::dvec3(xTransform * xUV);
        
        std::cout << "t=" << t << ":\n";
        std::cout << "  +Z patch UV(1," << t << ") -> cube(" 
                  << zCube.x << ", " << zCube.y << ", " << zCube.z << ")\n";
        std::cout << "  +X patch UV(" << t << ",1) -> cube(" 
                  << xCube.x << ", " << xCube.y << ", " << xCube.z << ")\n";
        
        // Project to sphere
        glm::dvec3 zSphere = cubeToSphere(zCube) * PLANET_RADIUS;
        glm::dvec3 xSphere = cubeToSphere(xCube) * PLANET_RADIUS;
        
        double gap = glm::length(zSphere - xSphere);
        std::cout << "  Gap after sphere projection: " << gap << " meters\n";
        
        if (gap > 1000000) {
            std::cout << "  >>> HUGE GAP! These points are NOT on the same edge!\n";
        }
        
        std::cout << "\n";
    }
    
    std::cout << "=== DIAGNOSIS ===\n";
    std::cout << "The problem is that the test is comparing PERPENDICULAR edges!\n";
    std::cout << "+Z right edge: varies in Y dimension (x=1, y varies, z=1)\n";
    std::cout << "+X top edge: varies in Z dimension (x=1, y=1, z varies)\n";
    std::cout << "These are DIFFERENT edges that only meet at the corner!\n";
    
    return 0;
}