#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include "core/global_patch_generator.hpp"

// ============================================================================
// TEST: Face Boundary Alignment
// This test verifies that patches at cube face boundaries share vertices
// ============================================================================

const double PLANET_RADIUS = 6371000.0;
const double EPSILON = 1e-7;  // After our fix
const double MAX_ALLOWED_GAP = 1.0; // 1 meter max gap allowed

// Test result tracking
struct TestResult {
    bool passed = true;
    std::string message;
    double maxGap = 0.0;
};

// Cube to sphere projection (matching shader)
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

// Create transform using the actual GlobalPatchGenerator (with our fix!)
glm::dmat4 createTransform(int face, const glm::dvec3& center, double size) {
    // Create a GlobalPatch to use the FIXED transform generation
    core::GlobalPatchGenerator::GlobalPatch patch;
    
    double halfSize = size * 0.5;
    
    // Set bounds based on face and center
    switch(face) {
        case 0: // +X face
            patch.minBounds = glm::vec3(1.0, center.y - halfSize, center.z - halfSize);
            patch.maxBounds = glm::vec3(1.0, center.y + halfSize, center.z + halfSize);
            break;
        case 1: // -X face
            patch.minBounds = glm::vec3(-1.0, center.y - halfSize, center.z - halfSize);
            patch.maxBounds = glm::vec3(-1.0, center.y + halfSize, center.z + halfSize);
            break;
        case 2: // +Y face
            patch.minBounds = glm::vec3(center.x - halfSize, 1.0, center.z - halfSize);
            patch.maxBounds = glm::vec3(center.x + halfSize, 1.0, center.z + halfSize);
            break;
        case 3: // -Y face
            patch.minBounds = glm::vec3(center.x - halfSize, -1.0, center.z - halfSize);
            patch.maxBounds = glm::vec3(center.x + halfSize, -1.0, center.z + halfSize);
            break;
        case 4: // +Z face
            patch.minBounds = glm::vec3(center.x - halfSize, center.y - halfSize, 1.0);
            patch.maxBounds = glm::vec3(center.x + halfSize, center.y + halfSize, 1.0);
            break;
        case 5: // -Z face
            patch.minBounds = glm::vec3(center.x - halfSize, center.y - halfSize, -1.0);
            patch.maxBounds = glm::vec3(center.x + halfSize, center.y + halfSize, -1.0);
            break;
    }
    
    patch.center = glm::vec3(center);
    patch.level = 0;
    patch.faceId = face;
    
    // Use the ACTUAL fixed transform generation!
    return patch.createTransform();
}

// Transform UV to world position (emulating vertex shader)
glm::dvec3 transformVertex(glm::dvec2 uv, const glm::dmat4& transform) {
    // UV to local space
    glm::dvec4 localPos(uv.x, uv.y, 0.0, 1.0);
    
    // Transform to cube position
    glm::dvec3 cubePos = glm::dvec3(transform * localPos);
    
    // Snap to boundaries (with current epsilon)
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

// Test a specific face boundary
TestResult testFaceBoundary(int face1, int face2, 
                           const glm::dvec3& center1, const glm::dvec3& center2,
                           double size, const std::string& description) {
    TestResult result;
    
    glm::dmat4 transform1 = createTransform(face1, center1, size);
    glm::dmat4 transform2 = createTransform(face2, center2, size);
    
    double maxGap = 0.0;
    int failCount = 0;
    
    // Test 11 points along the edge
    for (int i = 0; i <= 10; i++) {
        double t = i / 10.0;
        
        // Determine which edges should match based on face adjacency
        glm::dvec2 uv1, uv2;
        
        // CORRECTED: Map to the SAME edge using proper parameterization
        // The shared edge between faces needs consistent parameterization
        if (face1 == 4 && face2 == 0) {
            // +Z/+X share edge at (1, y-varies, 1)
            // +Z face: UV(1,t) maps correctly to this edge
            // +X face: ALSO needs UV(1,t) because U->Z, V->Y
            uv1 = glm::dvec2(1.0, t);  // Right edge of +Z
            uv2 = glm::dvec2(1.0, t);  // Correct parameterization for +X!
        } else if (face1 == 2 && face2 == 4) {
            // +Y/+Z boundary
            // They share edge at (x-varies, 1, 1)
            // +Y face: U->X, V->Z, so UV(t,1) gives (x-varies, 1, 1)
            // +Z face: U->X, V->Y, so UV(t,1) gives (x-varies, 1, 1)
            uv1 = glm::dvec2(t, 1.0);   // Top edge of +Y
            uv2 = glm::dvec2(t, 1.0);    // Top edge of +Z
        } else {
            // Same face adjacent patches
            uv1 = glm::dvec2(1.0, t);
            uv2 = glm::dvec2(0.0, t);
        }
        
        glm::dvec3 world1 = transformVertex(uv1, transform1);
        glm::dvec3 world2 = transformVertex(uv2, transform2);
        
        double gap = glm::length(world1 - world2);
        maxGap = std::max(maxGap, gap);
        
        if (gap > MAX_ALLOWED_GAP) {
            failCount++;
        }
    }
    
    result.maxGap = maxGap;
    
    if (failCount > 0) {
        result.passed = false;
        std::stringstream ss;
        ss << description << ": " << failCount 
           << " vertices have gaps > " << MAX_ALLOWED_GAP 
           << "m (max: " << maxGap << "m)";
        result.message = ss.str();
    } else {
        std::stringstream ss;
        ss << description << ": All vertices aligned (max gap: " 
           << maxGap << "m)";
        result.message = ss.str();
    }
    
    return result;
}

int main() {
    std::cout << "=== Face Boundary Alignment Test ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    std::vector<TestResult> results;
    
    // Test 1: +Z face meets +X face
    results.push_back(testFaceBoundary(
        4, 0, // +Z and +X faces
        glm::dvec3(0.5, 0.0, 1.0),  // Right side of +Z
        glm::dvec3(1.0, 0.0, 0.5),  // Top side of +X
        1.0,
        "Test 1: +Z/+X boundary"
    ));
    
    // Test 2: +Y face meets +Z face
    results.push_back(testFaceBoundary(
        2, 4, // +Y and +Z faces
        glm::dvec3(0.0, 1.0, 0.5),  // Top of +Y
        glm::dvec3(0.0, 0.5, 1.0),  // Top of +Z
        1.0,
        "Test 2: +Y/+Z boundary"
    ));
    
    // Test 3: Same face adjacent patches (should always work)
    results.push_back(testFaceBoundary(
        4, 4, // Both +Z face
        glm::dvec3(-0.5, 0.0, 1.0),  // Left patch
        glm::dvec3(0.5, 0.0, 1.0),   // Right patch
        1.0,
        "Test 3: Same face adjacent patches"
    ));
    
    // Print results
    std::cout << "Test Results:\n";
    std::cout << "----------------------------------------\n";
    
    bool allPassed = true;
    for (const auto& result : results) {
        std::cout << (result.passed ? "[PASS] " : "[FAIL] ");
        std::cout << result.message << "\n";
        allPassed = allPassed && result.passed;
    }
    
    std::cout << "----------------------------------------\n";
    
    if (allPassed) {
        std::cout << "\nALL TESTS PASSED ✓\n";
        std::cout << "Face boundaries are properly aligned!\n";
        return 0;
    } else {
        std::cout << "\nTESTS FAILED ✗\n";
        std::cout << "Face boundaries have alignment issues that need to be fixed.\n";
        std::cout << "\nExpected: All edges at face boundaries should have < " 
                  << MAX_ALLOWED_GAP << " meter gaps\n";
        std::cout << "Actual: Large gaps found at face boundaries\n";
        return 1;
    }
}