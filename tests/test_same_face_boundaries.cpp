#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

// ============================================================================
// CORRECT TEST: Same-Face Boundary Alignment
// This test verifies that patches on the SAME cube face share vertices correctly
// This is what our GlobalPatchGenerator fix actually addresses
// ============================================================================

const double PLANET_RADIUS = 6371000.0;
const double MAX_ALLOWED_GAP = 1.0; // 1 meter max gap

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
    std::cout << "=== SAME-FACE BOUNDARY ALIGNMENT TEST ===\n\n";
    std::cout << "This test verifies that adjacent patches on the SAME cube face\n";
    std::cout << "share vertices correctly after the GlobalPatchGenerator fix.\n\n";
    
    bool allTestsPassed = true;
    
    // Test 1: Adjacent patches on +Z face (left and right halves)
    std::cout << "Test 1: Adjacent patches on +Z face\n";
    std::cout << "----------------------------------------\n";
    
    // Left patch on +Z face
    core::GlobalPatchGenerator::GlobalPatch leftPatch;
    leftPatch.minBounds = glm::vec3(-1.0, -0.5, 1.0);
    leftPatch.maxBounds = glm::vec3(0.0, 0.5, 1.0);
    leftPatch.center = (leftPatch.minBounds + leftPatch.maxBounds) * 0.5f;
    leftPatch.level = 0;
    leftPatch.faceId = 4; // +Z
    
    // Right patch on +Z face
    core::GlobalPatchGenerator::GlobalPatch rightPatch;
    rightPatch.minBounds = glm::vec3(0.0, -0.5, 1.0);
    rightPatch.maxBounds = glm::vec3(1.0, 0.5, 1.0);
    rightPatch.center = (rightPatch.minBounds + rightPatch.maxBounds) * 0.5f;
    rightPatch.level = 0;
    rightPatch.faceId = 4; // +Z
    
    glm::dmat4 leftTransform = leftPatch.createTransform();
    glm::dmat4 rightTransform = rightPatch.createTransform();
    
    double maxGap = 0.0;
    int failCount = 0;
    
    // Test points along the shared edge (x=0 on the +Z face)
    for (double t = 0.0; t <= 1.0; t += 0.1) {
        // Left patch: right edge (u=1)
        glm::dvec4 leftUV(1.0, t, 0.0, 1.0);
        glm::dvec3 leftCube = glm::dvec3(leftTransform * leftUV);
        glm::dvec3 leftWorld = cubeToSphere(leftCube) * PLANET_RADIUS;
        
        // Right patch: left edge (u=0)
        glm::dvec4 rightUV(0.0, t, 0.0, 1.0);
        glm::dvec3 rightCube = glm::dvec3(rightTransform * rightUV);
        glm::dvec3 rightWorld = cubeToSphere(rightCube) * PLANET_RADIUS;
        
        double gap = glm::length(leftWorld - rightWorld);
        maxGap = std::max(maxGap, gap);
        
        if (gap > MAX_ALLOWED_GAP) {
            failCount++;
            std::cout << "  FAIL at t=" << t << ": gap = " << gap << " meters\n";
        }
    }
    
    if (failCount == 0) {
        std::cout << "  PASS: All vertices aligned (max gap: " << maxGap << " meters)\n";
    } else {
        std::cout << "  FAIL: " << failCount << " vertices have gaps > " << MAX_ALLOWED_GAP << " meters\n";
        allTestsPassed = false;
    }
    
    // Test 2: Adjacent patches on +X face (top and bottom halves)
    std::cout << "\nTest 2: Adjacent patches on +X face\n";
    std::cout << "----------------------------------------\n";
    
    // Bottom patch on +X face
    core::GlobalPatchGenerator::GlobalPatch bottomPatch;
    bottomPatch.minBounds = glm::vec3(1.0, -1.0, -0.5);
    bottomPatch.maxBounds = glm::vec3(1.0, 0.0, 0.5);
    bottomPatch.center = (bottomPatch.minBounds + bottomPatch.maxBounds) * 0.5f;
    bottomPatch.level = 0;
    bottomPatch.faceId = 0; // +X
    
    // Top patch on +X face
    core::GlobalPatchGenerator::GlobalPatch topPatch;
    topPatch.minBounds = glm::vec3(1.0, 0.0, -0.5);
    topPatch.maxBounds = glm::vec3(1.0, 1.0, 0.5);
    topPatch.center = (topPatch.minBounds + topPatch.maxBounds) * 0.5f;
    topPatch.level = 0;
    topPatch.faceId = 0; // +X
    
    glm::dmat4 bottomTransform = bottomPatch.createTransform();
    glm::dmat4 topTransform = topPatch.createTransform();
    
    maxGap = 0.0;
    failCount = 0;
    
    // Test points along the shared edge (y=0 on the +X face)
    for (double t = 0.0; t <= 1.0; t += 0.1) {
        // Bottom patch: top edge (v=1)
        glm::dvec4 bottomUV(t, 1.0, 0.0, 1.0);
        glm::dvec3 bottomCube = glm::dvec3(bottomTransform * bottomUV);
        glm::dvec3 bottomWorld = cubeToSphere(bottomCube) * PLANET_RADIUS;
        
        // Top patch: bottom edge (v=0)
        glm::dvec4 topUV(t, 0.0, 0.0, 1.0);
        glm::dvec3 topCube = glm::dvec3(topTransform * topUV);
        glm::dvec3 topWorld = cubeToSphere(topCube) * PLANET_RADIUS;
        
        double gap = glm::length(bottomWorld - topWorld);
        maxGap = std::max(maxGap, gap);
        
        if (gap > MAX_ALLOWED_GAP) {
            failCount++;
            std::cout << "  FAIL at t=" << t << ": gap = " << gap << " meters\n";
        }
    }
    
    if (failCount == 0) {
        std::cout << "  PASS: All vertices aligned (max gap: " << maxGap << " meters)\n";
    } else {
        std::cout << "  FAIL: " << failCount << " vertices have gaps > " << MAX_ALLOWED_GAP << " meters\n";
        allTestsPassed = false;
    }
    
    // Summary
    std::cout << "\n========================================\n";
    if (allTestsPassed) {
        std::cout << "ALL TESTS PASSED ✓\n";
        std::cout << "The GlobalPatchGenerator fix is working correctly!\n";
        std::cout << "Adjacent patches on the same face share vertices properly.\n";
        return 0;
    } else {
        std::cout << "TESTS FAILED ✗\n";
        std::cout << "There are still alignment issues with same-face patches.\n";
        return 1;
    }
}