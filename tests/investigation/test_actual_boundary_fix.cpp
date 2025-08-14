#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

const double PLANET_RADIUS = 6371000.0;

// Cube to sphere projection (matching shader)
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

// Transform UV to world position using our actual transform
glm::dvec3 transformVertex(glm::dvec2 uv, const glm::dmat4& transform) {
    // UV to local space
    glm::dvec4 localPos(uv.x, uv.y, 0.0, 1.0);
    
    // Transform to cube position
    glm::dvec3 cubePos = glm::dvec3(transform * localPos);
    
    // Snap to boundaries (matching shader)
    const double EPSILON = 1e-5;
    if (std::abs(std::abs(cubePos.x) - 1.0) < EPSILON) {
        cubePos.x = (cubePos.x > 0.0) ? 1.0 : -1.0;
    }
    if (std::abs(std::abs(cubePos.y) - 1.0) < EPSILON) {
        cubePos.y = (cubePos.y > 0.0) ? 1.0 : -1.0;
    }
    if (std::abs(std::abs(cubePos.z) - 1.0) < EPSILON) {
        cubePos.z = (cubePos.z > 0.0) ? 1.0 : -1.0;
    }
    
    // Project to sphere
    glm::dvec3 spherePos = cubeToSphere(cubePos);
    
    return spherePos * PLANET_RADIUS;
}

void testBoundary(const char* name, 
                  const core::GlobalPatchGenerator::GlobalPatch& patch1,
                  const core::GlobalPatchGenerator::GlobalPatch& patch2,
                  glm::dvec2 uv1, glm::dvec2 uv2) {
    
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    glm::dvec3 world1 = transformVertex(uv1, transform1);
    glm::dvec3 world2 = transformVertex(uv2, transform2);
    
    double gap = glm::length(world1 - world2);
    
    std::cout << name << ":\n";
    std::cout << "  Patch 1 UV(" << uv1.x << "," << uv1.y << ") -> world pos\n";
    std::cout << "  Patch 2 UV(" << uv2.x << "," << uv2.y << ") -> world pos\n";
    std::cout << "  Gap: " << gap << " meters";
    
    if (gap < 1.0) {
        std::cout << " ✓ PASS\n";
    } else {
        std::cout << " ✗ FAIL\n";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== Testing Actual GlobalPatchGenerator Fix ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Test 1: +Z face right edge meets +X face top edge  
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
    
    std::cout << "Test 1: +Z/+X Face Boundary\n";
    std::cout << "Testing edge where +Z (x=1) meets +X (z=1)\n\n";
    
    // Test several points along the shared edge
    for (double t = 0.0; t <= 1.0; t += 0.5) {
        char testName[100];
        sprintf(testName, "Point at t=%.1f", t);
        
        // +Z patch: right edge (u=1) varies with v=t
        // +X patch: top edge (v=1) varies with u=t
        testBoundary(testName, zPatch, xPatch, 
                    glm::dvec2(1.0, t), glm::dvec2(t, 1.0));
    }
    
    // Test 2: Same face adjacent patches (should always work)
    core::GlobalPatchGenerator::GlobalPatch leftPatch;
    leftPatch.minBounds = glm::vec3(-0.5f, -0.5f, 1.0f);
    leftPatch.maxBounds = glm::vec3(0.0f, 0.5f, 1.0f);
    leftPatch.center = (leftPatch.minBounds + leftPatch.maxBounds) * 0.5f;
    leftPatch.level = 1;
    leftPatch.faceId = 4; // +Z
    
    core::GlobalPatchGenerator::GlobalPatch rightPatch;
    rightPatch.minBounds = glm::vec3(0.0f, -0.5f, 1.0f);
    rightPatch.maxBounds = glm::vec3(0.5f, 0.5f, 1.0f);
    rightPatch.center = (rightPatch.minBounds + rightPatch.maxBounds) * 0.5f;
    rightPatch.level = 1;
    rightPatch.faceId = 4; // +Z
    
    std::cout << "\nTest 2: Same Face Adjacent Patches\n";
    std::cout << "Testing edge between two +Z patches\n\n";
    
    for (double t = 0.0; t <= 1.0; t += 0.5) {
        char testName[100];
        sprintf(testName, "Point at t=%.1f", t);
        
        // Left patch right edge (u=1) meets right patch left edge (u=0)
        testBoundary(testName, leftPatch, rightPatch,
                    glm::dvec2(1.0, t), glm::dvec2(0.0, t));
    }
    
    return 0;
}