#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iomanip>
#include <cmath>

// Test to scientifically determine why X-faces (Face 0 and Face 1) don't render

struct TestResult {
    std::string testName;
    bool passed;
    std::string details;
};

std::vector<TestResult> results;

void recordTest(const std::string& name, bool passed, const std::string& details) {
    results.push_back({name, passed, details});
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << ": " << details << std::endl;
}

// Test 1: Verify transform matrix structure for X-faces
void testTransformMatrix() {
    std::cout << "\n=== TEST 1: Transform Matrix Structure ===\n";
    
    // X-face transform (from debug output)
    glm::dmat4 xFaceTransform(
        0.0, 0.0, 0.5, 0.0,  // Column 0
        0.0, 0.5, 0.0, 0.0,  // Column 1  
        0.5, 0.0, 0.0, 0.0,  // Column 2
        1.0, -1.0, -1.0, 1.0  // Column 3
    );
    
    // Test UV corners
    glm::dvec4 corners[4] = {
        glm::dvec4(0.0, 0.0, 0.0, 1.0),  // UV(0,0)
        glm::dvec4(1.0, 0.0, 0.0, 1.0),  // UV(1,0)
        glm::dvec4(0.0, 1.0, 0.0, 1.0),  // UV(0,1)
        glm::dvec4(1.0, 1.0, 0.0, 1.0)   // UV(1,1)
    };
    
    bool allOnXFace = true;
    for (int i = 0; i < 4; i++) {
        glm::dvec3 transformed = glm::dvec3(xFaceTransform * corners[i]);
        
        // Check if X is fixed at 1.0
        if (std::abs(transformed.x - 1.0) > 0.001) {
            allOnXFace = false;
            recordTest("X-face transform corner " + std::to_string(i), false, 
                      "X not fixed at 1.0: " + std::to_string(transformed.x));
        }
        
        std::cout << "  UV(" << corners[i].x << "," << corners[i].y 
                  << ") -> Cube(" << transformed.x << ", " << transformed.y 
                  << ", " << transformed.z << ")" << std::endl;
    }
    
    recordTest("X-face transform correctness", allOnXFace, 
               allOnXFace ? "All corners correctly on X=1 face" : "Some corners not on X face");
}

// Test 2: Verify cube-to-sphere mapping for X-face points
void testCubeToSphere() {
    std::cout << "\n=== TEST 2: Cube-to-Sphere Mapping ===\n";
    
    // Test points on X=1 face
    glm::dvec3 testPoints[5] = {
        glm::dvec3(1.0, 0.0, 0.0),    // Center
        glm::dvec3(1.0, -1.0, -1.0),  // Corner
        glm::dvec3(1.0, 1.0, 1.0),    // Opposite corner
        glm::dvec3(1.0, 0.5, 0.0),    // Mid-point
        glm::dvec3(1.0, 0.0, 0.5)     // Another mid-point
    };
    
    bool allValid = true;
    for (int i = 0; i < 5; i++) {
        // Simple cube-to-sphere: normalize the vector
        glm::dvec3 spherePos = glm::normalize(testPoints[i]);
        
        // Check if result is valid (not NaN, reasonable magnitude)
        bool valid = std::isfinite(spherePos.x) && std::isfinite(spherePos.y) && std::isfinite(spherePos.z);
        double length = glm::length(spherePos);
        valid = valid && (std::abs(length - 1.0) < 0.001);
        
        if (!valid) {
            allValid = false;
            recordTest("Cube-to-sphere point " + std::to_string(i), false,
                      "Invalid sphere position or length");
        }
        
        std::cout << "  Cube(" << testPoints[i].x << ", " << testPoints[i].y << ", " << testPoints[i].z 
                  << ") -> Sphere(" << spherePos.x << ", " << spherePos.y << ", " << spherePos.z 
                  << ") Length=" << length << std::endl;
    }
    
    recordTest("Cube-to-sphere mapping", allValid, 
               allValid ? "All points map correctly" : "Some points failed mapping");
}

// Test 3: Camera-relative transformation
void testCameraTransform() {
    std::cout << "\n=== TEST 3: Camera-Relative Transform ===\n";
    
    // Typical camera position from debug output
    glm::dvec3 cameraPos(-1.11493e7, 4.77825e6, -9.5565e6);
    double planetRadius = 6.371e6;
    
    // Test vertex on +X face
    glm::dvec3 vertexWorldPos(planetRadius, 0, 0);  // On +X axis
    
    // Camera-relative position
    glm::dvec3 relativePos = vertexWorldPos - cameraPos;
    
    std::cout << "  Camera pos: (" << cameraPos.x/1e6 << ", " << cameraPos.y/1e6 
              << ", " << cameraPos.z/1e6 << ") million meters" << std::endl;
    std::cout << "  Vertex world: (" << vertexWorldPos.x/1e6 << ", " << vertexWorldPos.y/1e6 
              << ", " << vertexWorldPos.z/1e6 << ") million meters" << std::endl;
    std::cout << "  Relative pos: (" << relativePos.x/1e6 << ", " << relativePos.y/1e6 
              << ", " << relativePos.z/1e6 << ") million meters" << std::endl;
    
    // Check if relative position is behind camera (negative Z in view space)
    // This is a simplified check - actual would use view matrix
    double distanceFromCamera = glm::length(relativePos);
    bool inFrontOfCamera = distanceFromCamera > 0 && distanceFromCamera < 1e9;  // Reasonable range
    
    recordTest("Camera-relative transform", inFrontOfCamera,
               "Distance from camera: " + std::to_string(distanceFromCamera/1e6) + " million meters");
}

// Test 4: Analyze patch distribution
void testPatchDistribution() {
    std::cout << "\n=== TEST 4: Patch Distribution Analysis ===\n";
    
    // From debug output: "Per face: 16 52 34 19 16 49"
    int patchesPerFace[6] = {16, 52, 34, 19, 16, 49};
    const char* faceNames[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    
    int totalPatches = 0;
    for (int i = 0; i < 6; i++) {
        totalPatches += patchesPerFace[i];
        std::cout << "  Face " << i << " (" << faceNames[i] << "): " 
                  << patchesPerFace[i] << " patches" << std::endl;
    }
    
    // Check if X-faces have patches
    bool xFacesHavePatches = (patchesPerFace[0] > 0) && (patchesPerFace[1] > 0);
    recordTest("X-faces have patches", xFacesHavePatches,
               "+X has " + std::to_string(patchesPerFace[0]) + 
               ", -X has " + std::to_string(patchesPerFace[1]));
    
    // Calculate vertices for X-faces (65x65 grid per patch)
    int verticesPerPatch = 65 * 65;
    int xFaceVertices = (patchesPerFace[0] + patchesPerFace[1]) * verticesPerPatch;
    std::cout << "  Expected X-face vertices: " << xFaceVertices << std::endl;
    
    recordTest("X-face vertex count", xFaceVertices > 0,
               std::to_string(xFaceVertices) + " vertices should be generated");
}

// Test 5: Clipping plane analysis
void testClippingPlanes() {
    std::cout << "\n=== TEST 5: Clipping Plane Analysis ===\n";
    
    // From debug: near=1, far=1e8
    double nearPlane = 1.0;
    double farPlane = 1e8;
    
    // Test if X-face vertices might be clipped
    glm::dvec3 cameraPos(-1.11493e7, 4.77825e6, -9.5565e6);
    glm::dvec3 xFaceCenter(6.371e6, 0, 0);  // Center of +X face
    
    double distance = glm::length(xFaceCenter - cameraPos);
    bool withinClipRange = (distance > nearPlane) && (distance < farPlane);
    
    std::cout << "  Near plane: " << nearPlane << " meters" << std::endl;
    std::cout << "  Far plane: " << farPlane/1e6 << " million meters" << std::endl;
    std::cout << "  X-face distance: " << distance/1e6 << " million meters" << std::endl;
    
    recordTest("X-face within clip range", withinClipRange,
               withinClipRange ? "Should be visible" : "Outside clip range!");
}

// Test 6: Winding order check
void testWindingOrder() {
    std::cout << "\n=== TEST 6: Triangle Winding Order ===\n";
    
    // For a patch at UV (0-1, 0-1) on X=1 face
    // After transform: maps to varying Y and Z
    // Triangle winding should be counter-clockwise when viewed from outside
    
    // Sample triangle vertices (simplified)
    glm::vec3 v0(1.0f, -1.0f, -1.0f);   // Bottom-left
    glm::vec3 v1(1.0f, -1.0f, -0.5f);   // Bottom-right
    glm::vec3 v2(1.0f, -0.5f, -1.0f);   // Top-left
    
    // Calculate normal using cross product
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 normal = glm::cross(edge1, edge2);
    
    // For +X face, normal should point in +X direction
    bool correctWinding = normal.x > 0;
    
    std::cout << "  Triangle normal: (" << normal.x << ", " << normal.y << ", " << normal.z << ")" << std::endl;
    std::cout << "  Expected: positive X component for +X face" << std::endl;
    
    recordTest("Triangle winding order", correctWinding,
               correctWinding ? "Correct CCW winding" : "Incorrect winding - may be culled!");
}

// Main analysis
int main() {
    std::cout << "==========================================================\n";
    std::cout << "   SCIENTIFIC ANALYSIS: X-FACE RENDERING FAILURE\n";
    std::cout << "==========================================================\n";
    
    testTransformMatrix();
    testCubeToSphere();
    testCameraTransform();
    testPatchDistribution();
    testClippingPlanes();
    testWindingOrder();
    
    std::cout << "\n==========================================================\n";
    std::cout << "                    ANALYSIS SUMMARY\n";
    std::cout << "==========================================================\n\n";
    
    int passed = 0, failed = 0;
    for (const auto& result : results) {
        if (result.passed) passed++;
        else failed++;
        
        std::cout << (result.passed ? "[✓] " : "[✗] ") 
                  << std::setw(40) << std::left << result.testName 
                  << " | " << result.details << std::endl;
    }
    
    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "Tests Passed: " << passed << "/" << (passed + failed) << std::endl;
    
    std::cout << "\n==========================================================\n";
    std::cout << "                  HYPOTHESIS & CONCLUSION\n";
    std::cout << "==========================================================\n\n";
    
    if (failed == 0) {
        std::cout << "All tests passed, but X-faces still don't render.\n";
        std::cout << "This suggests the issue is in the GPU pipeline:\n";
        std::cout << "1. Shader transformation bug\n";
        std::cout << "2. Instance buffer not properly set for X-faces\n";
        std::cout << "3. Depth test failing for X-face fragments\n";
    } else {
        std::cout << "Failed tests indicate specific problems:\n";
        for (const auto& result : results) {
            if (!result.passed) {
                std::cout << "- " << result.testName << ": " << result.details << std::endl;
            }
        }
    }
    
    std::cout << "\nMost likely root cause based on symptoms:\n";
    std::cout << "The X-face patches use a different transform mapping (U->Z, V->Y)\n";
    std::cout << "which may cause vertices to be transformed incorrectly or\n";
    std::cout << "triangles to have incorrect winding order, leading to backface culling.\n";
    
    return failed > 0 ? 1 : 0;
}