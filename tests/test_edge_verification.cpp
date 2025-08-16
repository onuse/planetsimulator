#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iomanip>

// This test verifies if our GlobalPatchGenerator fix is actually working

// Include the actual GlobalPatchGenerator
#include "core/global_patch_generator.hpp"

const double PLANET_RADIUS = 6371000.0;

// Cube to sphere (matching the actual implementation)
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

void testActualEdgeAlignment() {
    std::cout << "=== Testing ACTUAL GlobalPatchGenerator Edge Alignment ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Create two patches that should share an edge
    // Patch A: +Z face at right edge (X=0.5 to 1.0)
    // Patch B: +X face at top edge (Z=0.5 to 1.0)
    
    core::GlobalPatchGenerator::GlobalPatch patchA;
    patchA.minBounds = glm::dvec3(0.5, -0.5, 1.0);
    patchA.maxBounds = glm::dvec3(1.0, 0.5, 1.0);
    patchA.center = (patchA.minBounds + patchA.maxBounds) * 0.5;
    patchA.level = 1;
    patchA.faceId = 4; // +Z face
    
    core::GlobalPatchGenerator::GlobalPatch patchB;
    patchB.minBounds = glm::dvec3(1.0, -0.5, 0.5);
    patchB.maxBounds = glm::dvec3(1.0, 0.5, 1.0);
    patchB.center = (patchB.minBounds + patchB.maxBounds) * 0.5;
    patchB.level = 1;
    patchB.faceId = 0; // +X face
    
    std::cout << "Patch A (+Z face): ";
    std::cout << "min=(" << patchA.minBounds.x << "," << patchA.minBounds.y << "," << patchA.minBounds.z << ") ";
    std::cout << "max=(" << patchA.maxBounds.x << "," << patchA.maxBounds.y << "," << patchA.maxBounds.z << ")\n";
    
    std::cout << "Patch B (+X face): ";
    std::cout << "min=(" << patchB.minBounds.x << "," << patchB.minBounds.y << "," << patchB.minBounds.z << ") ";
    std::cout << "max=(" << patchB.maxBounds.x << "," << patchB.maxBounds.y << "," << patchB.maxBounds.z << ")\n\n";
    
    // Get transforms using the ACTUAL createTransform method
    glm::dmat4 transformA = patchA.createTransform();
    glm::dmat4 transformB = patchB.createTransform();
    
    std::cout << "Testing shared edge at X=1, Z=1:\n";
    std::cout << "This edge should produce IDENTICAL vertices from both patches\n\n";
    
    double maxGap = 0;
    int mismatches = 0;
    
    for (int i = 0; i <= 8; i++) {
        double t = i / 8.0;
        double y = -0.5 + t; // Y ranges from -0.5 to 0.5 along the edge
        
        // For patch A (+Z face), the right edge is at U=1
        glm::dvec4 uvA(1.0, t, 0.0, 1.0);
        glm::dvec3 cubePosA = glm::dvec3(transformA * uvA);
        
        // For patch B (+X face), the top edge is at V=1
        glm::dvec4 uvB(t, 1.0, 0.0, 1.0);
        glm::dvec3 cubePosB = glm::dvec3(transformB * uvB);
        
        // Convert to sphere and scale to planet
        glm::dvec3 spherePosA = cubeToSphere(cubePosA) * PLANET_RADIUS;
        glm::dvec3 spherePosB = cubeToSphere(cubePosB) * PLANET_RADIUS;
        
        double gap = glm::length(spherePosA - spherePosB);
        maxGap = std::max(maxGap, gap);
        
        std::cout << "Y=" << y << ": ";
        std::cout << "A_cube=(" << cubePosA.x << "," << cubePosA.y << "," << cubePosA.z << ") ";
        std::cout << "B_cube=(" << cubePosB.x << "," << cubePosB.y << "," << cubePosB.z << ") ";
        std::cout << "Gap=" << gap << "m ";
        
        if (gap > 1.0) {
            std::cout << "✗ MISMATCH!";
            mismatches++;
        } else {
            std::cout << "✓";
        }
        std::cout << "\n";
    }
    
    std::cout << "\n=== RESULTS ===\n";
    std::cout << "Maximum gap: " << maxGap << " meters\n";
    std::cout << "Mismatches: " << mismatches << " out of 9 test points\n";
    
    if (maxGap > 1.0) {
        std::cout << "\n✗ EDGE ALIGNMENT FAILED!\n";
        std::cout << "The patches are NOT producing identical vertices at shared edges.\n";
        std::cout << "This explains the 6 million meter gaps in the planet rendering.\n";
    } else {
        std::cout << "\n✓ EDGE ALIGNMENT SUCCESSFUL!\n";
        std::cout << "The patches ARE producing identical vertices at shared edges.\n";
    }
}

int main() {
    testActualEdgeAlignment();
    return 0;
}