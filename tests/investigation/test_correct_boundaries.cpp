#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== Finding Correct Shared Boundaries ===\n\n";
    std::cout << std::fixed << std::setprecision(6);
    
    // The cube has edges where faces meet
    // +Z face (z=1) meets +X face (x=1) along the edge where x=1, z=1
    // This edge runs from (1, -1, 1) to (1, 1, 1)
    
    std::cout << "Cube edge where +Z and +X meet:\n";
    std::cout << "  Runs from (1, -1, 1) to (1, 1, 1)\n\n";
    
    // Now let's see what patches actually touch this edge
    
    // +Z face patch that includes the x=1 edge
    core::GlobalPatchGenerator::GlobalPatch zPatch;
    zPatch.minBounds = glm::vec3(0.5f, -0.5f, 1.0f);  // Goes from x=0.5 to x=1
    zPatch.maxBounds = glm::vec3(1.0f, 0.5f, 1.0f);
    zPatch.center = glm::vec3((zPatch.minBounds + zPatch.maxBounds) * 0.5f);
    zPatch.level = 1;
    zPatch.faceId = 4; // +Z
    
    std::cout << "+Z Patch that touches the edge:\n";
    std::cout << "  Bounds: (" << zPatch.minBounds.x << ", " << zPatch.minBounds.y << ", " << zPatch.minBounds.z 
              << ") to (" << zPatch.maxBounds.x << ", " << zPatch.maxBounds.y << ", " << zPatch.maxBounds.z << ")\n";
    std::cout << "  Its right edge (x=1) runs from (1, -0.5, 1) to (1, 0.5, 1)\n\n";
    
    // +X face patch that SHOULD include the z=1 edge
    // But wait - the +X face has x=1 fixed, and varies in y and z
    // So a patch on +X face that reaches z=1 would be:
    core::GlobalPatchGenerator::GlobalPatch xPatch;
    xPatch.minBounds = glm::vec3(1.0f, -0.5f, 0.5f);  // Goes from z=0.5 to z=1
    xPatch.maxBounds = glm::vec3(1.0f, 0.5f, 1.0f);
    xPatch.center = glm::vec3((xPatch.minBounds + xPatch.maxBounds) * 0.5f);
    xPatch.level = 1;
    xPatch.faceId = 0; // +X
    
    std::cout << "+X Patch that touches the edge:\n";
    std::cout << "  Bounds: (" << xPatch.minBounds.x << ", " << xPatch.minBounds.y << ", " << xPatch.minBounds.z 
              << ") to (" << xPatch.maxBounds.x << ", " << xPatch.maxBounds.y << ", " << xPatch.maxBounds.z << ")\n";
    std::cout << "  Its top edge (z=1) runs from (1, -0.5, 1) to (1, 0.5, 1)\n\n";
    
    std::cout << "GOOD NEWS: Both patches share the exact same edge in 3D space!\n";
    std::cout << "  Shared edge: from (1, -0.5, 1) to (1, 0.5, 1)\n\n";
    
    std::cout << "The problem is in the UV mapping:\n\n";
    
    auto zTransform = zPatch.createTransform();
    auto xTransform = xPatch.createTransform();
    
    // For +Z patch, the right edge is at u=1, v varies from 0 to 1
    // At v=0.25, we expect to be at y=-0.25 (25% of the way from -0.5 to 0.5)
    glm::dvec4 zUV(1.0, 0.25, 0.0, 1.0);
    glm::dvec3 zPoint = glm::dvec3(zTransform * zUV);
    std::cout << "+Z patch UV(1.0, 0.25) -> (" << zPoint.x << ", " << zPoint.y << ", " << zPoint.z << ")\n";
    std::cout << "  Expected: (1.0, -0.25, 1.0)\n";
    
    // For +X patch, the top edge (z=1) requires finding the right UV
    // The patch uses U->Z mapping, so z=1 requires u=1
    // But wait, if U maps to Z with range 0.5, and origin at 0.5:
    // z = u * 0.5 + 0.5, so z=1 requires u=1
    // And V maps to Y with range 1.0 and origin at -0.5:
    // y = v * 1.0 - 0.5, so y=-0.25 requires v=0.25
    glm::dvec4 xUV(1.0, 0.25, 0.0, 1.0);
    glm::dvec3 xPoint = glm::dvec3(xTransform * xUV);
    std::cout << "+X patch UV(1.0, 0.25) -> (" << xPoint.x << ", " << xPoint.y << ", " << xPoint.z << ")\n";
    std::cout << "  Expected: (1.0, -0.25, 1.0)\n";
    
    double gap = glm::length(zPoint - xPoint);
    std::cout << "\nGap between points: " << gap << "\n";
    
    if (gap < 1e-6) {
        std::cout << "✓ SUCCESS: The patches map to the same point!\n";
    } else {
        std::cout << "✗ FAILURE: The patches don't align properly.\n";
    }
    
    return 0;
}