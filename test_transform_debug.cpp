#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== Debug Transform Matrices ===\n\n";
    std::cout << std::fixed << std::setprecision(6);
    
    // Create patches at the +Z/+X boundary
    core::GlobalPatchGenerator::GlobalPatch zPatch;
    zPatch.minBounds = glm::vec3(0.5f, -0.5f, 1.0f);
    zPatch.maxBounds = glm::vec3(1.0f, 0.5f, 1.0f);
    zPatch.center = (zPatch.minBounds + zPatch.maxBounds) * 0.5f;
    zPatch.level = 1;
    zPatch.faceId = 4; // +Z
    
    core::GlobalPatchGenerator::GlobalPatch xPatch;
    xPatch.minBounds = glm::vec3(1.0f, -0.5f, 0.5f);
    xPatch.maxBounds = glm::vec3(1.0f, 0.5f, 1.0f);
    xPatch.center = (xPatch.minBounds + xPatch.maxBounds) * 0.5f;
    xPatch.level = 1;
    xPatch.faceId = 0; // +X
    
    auto zTransform = zPatch.createTransform();
    auto xTransform = xPatch.createTransform();
    
    std::cout << "+Z Patch:\n";
    std::cout << "  Bounds: (" << zPatch.minBounds.x << "," << zPatch.minBounds.y << "," << zPatch.minBounds.z 
              << ") to (" << zPatch.maxBounds.x << "," << zPatch.maxBounds.y << "," << zPatch.maxBounds.z << ")\n";
    std::cout << "  Transform matrix:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "    [" << zTransform[i][0] << ", " << zTransform[i][1] 
                  << ", " << zTransform[i][2] << ", " << zTransform[i][3] << "]\n";
    }
    
    std::cout << "\n+X Patch:\n";
    std::cout << "  Bounds: (" << xPatch.minBounds.x << "," << xPatch.minBounds.y << "," << xPatch.minBounds.z 
              << ") to (" << xPatch.maxBounds.x << "," << xPatch.maxBounds.y << "," << xPatch.maxBounds.z << ")\n";
    std::cout << "  Transform matrix:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "    [" << xTransform[i][0] << ", " << xTransform[i][1] 
                  << ", " << xTransform[i][2] << ", " << xTransform[i][3] << "]\n";
    }
    
    std::cout << "\nShared edge analysis:\n";
    std::cout << "+Z patch right edge (x=1): from y=" << zPatch.minBounds.y << " to y=" << zPatch.maxBounds.y << "\n";
    std::cout << "+X patch top edge (z=1): from y=" << xPatch.minBounds.y << " to y=" << xPatch.maxBounds.y << "\n";
    std::cout << "These should be the same edge!\n\n";
    
    // Test specific UV mappings
    std::cout << "UV Mapping Test:\n";
    
    // +Z patch at UV(1, 0.5) should be at cube position (1, 0, 1)
    glm::dvec4 zUV(1.0, 0.5, 0.0, 1.0);
    glm::dvec3 zCube = glm::dvec3(zTransform * zUV);
    std::cout << "+Z UV(1.0, 0.5) -> cube(" << zCube.x << ", " << zCube.y << ", " << zCube.z << ")\n";
    std::cout << "  Expected: (1.0, 0.0, 1.0)\n";
    
    // +X patch at UV(0.5, 1.0) should also be at cube position (1, 0, 1)
    glm::dvec4 xUV(0.5, 1.0, 0.0, 1.0);
    glm::dvec3 xCube = glm::dvec3(xTransform * xUV);
    std::cout << "+X UV(0.5, 1.0) -> cube(" << xCube.x << ", " << xCube.y << ", " << xCube.z << ")\n";
    std::cout << "  Expected: (1.0, 0.0, 1.0)\n";
    
    glm::dvec3 diff = zCube - xCube;
    std::cout << "\nDifference: (" << diff.x << ", " << diff.y << ", " << diff.z << ")\n";
    double gap = glm::length(diff);
    std::cout << "Gap in cube space: " << gap << "\n";
    
    if (gap < 1e-6) {
        std::cout << "✓ Patches map to the same point!\n";
    } else {
        std::cout << "✗ PROBLEM: Patches don't map to the same point!\n";
        std::cout << "\nThe issue is that the patches are NOT parameterized correctly.\n";
        std::cout << "The +Z patch thinks its right edge goes from (1, -0.5, 1) to (1, 0.5, 1)\n";
        std::cout << "The +X patch thinks its top edge goes from (1, -0.5, 1) to (1, 0.5, 1)\n";
        std::cout << "But they're using different UV mappings!\n";
    }
    
    return 0;
}