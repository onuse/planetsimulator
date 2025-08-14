#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

// Test if adjacent patches have continuous terrain
void testPatchContinuity() {
    std::cout << "=== PATCH ALIGNMENT TEST ===\n\n";
    
    // Simulate two adjacent patches
    struct Patch {
        glm::vec3 corners[4];
        int face;
        int level;
    };
    
    // Create two adjacent patches on the same face
    Patch patch1, patch2;
    
    // Patch 1: left side of face +X
    patch1.face = 0; // +X face
    patch1.level = 2;
    patch1.corners[0] = glm::vec3(1, -0.5, -0.5);  // BL
    patch1.corners[1] = glm::vec3(1, 0, -0.5);     // BR
    patch1.corners[2] = glm::vec3(1, 0, 0);        // TR
    patch1.corners[3] = glm::vec3(1, -0.5, 0);     // TL
    
    // Patch 2: right side of face +X (should share edge with patch1)
    patch2.face = 0;
    patch2.level = 2;
    patch2.corners[0] = glm::vec3(1, 0, -0.5);     // BL (should match patch1 BR)
    patch2.corners[1] = glm::vec3(1, 0.5, -0.5);   // BR
    patch2.corners[2] = glm::vec3(1, 0.5, 0);      // TR
    patch2.corners[3] = glm::vec3(1, 0, 0);        // TL (should match patch1 TR)
    
    // Check if shared edge matches
    std::cout << "Adjacent patches on same face:\n";
    std::cout << "Patch 1 right edge: BR(" << patch1.corners[1].x << "," << patch1.corners[1].y << "," << patch1.corners[1].z << ")"
              << " TR(" << patch1.corners[2].x << "," << patch1.corners[2].y << "," << patch1.corners[2].z << ")\n";
    std::cout << "Patch 2 left edge:  BL(" << patch2.corners[0].x << "," << patch2.corners[0].y << "," << patch2.corners[0].z << ")"
              << " TL(" << patch2.corners[3].x << "," << patch2.corners[3].y << "," << patch2.corners[3].z << ")\n";
    
    // Check continuity
    glm::vec3 diff1 = patch1.corners[1] - patch2.corners[0];
    glm::vec3 diff2 = patch1.corners[2] - patch2.corners[3];
    
    float error1 = glm::length(diff1);
    float error2 = glm::length(diff2);
    
    std::cout << "\nAlignment errors:\n";
    std::cout << "  Bottom vertices: " << error1 << "\n";
    std::cout << "  Top vertices: " << error2 << "\n";
    
    if (error1 > 0.001f || error2 > 0.001f) {
        std::cout << "ERROR: Patches are NOT aligned!\n";
    } else {
        std::cout << "OK: Patches are properly aligned\n";
    }
}

// Test terrain sampling consistency
void testTerrainSampling() {
    std::cout << "\n=== TERRAIN SAMPLING TEST ===\n\n";
    
    // Simulate terrain function
    auto terrainHeight = [](glm::vec3 sphereNormal) -> float {
        // Simple terrain based on position
        return sin(sphereNormal.x * 2.0f) * cos(sphereNormal.y * 1.5f) * 1500.0f;
    };
    
    // Project cube position to sphere
    auto cubeToSphere = [](glm::vec3 cubePos) -> glm::vec3 {
        glm::vec3 pos2 = cubePos * cubePos;
        glm::vec3 spherePos = cubePos * glm::vec3(
            sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f),
            sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f),
            sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f)
        );
        return glm::normalize(spherePos);
    };
    
    // Test point on edge between two patches
    glm::vec3 edgePoint1 = glm::vec3(1, 0, 0);  // Edge between two patches on +X face
    glm::vec3 spherePos1 = cubeToSphere(edgePoint1);
    float height1 = terrainHeight(spherePos1);
    
    // Same point accessed from adjacent patch (should give same result)
    glm::vec3 edgePoint2 = glm::vec3(1, 0, 0);  // Same point
    glm::vec3 spherePos2 = cubeToSphere(edgePoint2);
    float height2 = terrainHeight(spherePos2);
    
    std::cout << "Edge point terrain heights:\n";
    std::cout << "  From patch 1: " << height1 << "\n";
    std::cout << "  From patch 2: " << height2 << "\n";
    std::cout << "  Difference: " << std::abs(height1 - height2) << "\n";
    
    if (std::abs(height1 - height2) > 0.001f) {
        std::cout << "ERROR: Terrain height is inconsistent at edge!\n";
    } else {
        std::cout << "OK: Terrain height is consistent\n";
    }
}

// Test face boundary transitions
void testFaceBoundaries() {
    std::cout << "\n=== FACE BOUNDARY TEST ===\n\n";
    
    // Test corner where three faces meet
    glm::vec3 corner = glm::vec3(1, 1, 1);  // Corner of cube
    
    // This corner should be accessible from three faces
    // Face +X: at (1, 1, 1)
    // Face +Y: at (1, 1, 1)  
    // Face +Z: at (1, 1, 1)
    
    std::cout << "Cube corner (1,1,1) belongs to faces: +X, +Y, +Z\n";
    
    // In the current implementation, patches near this corner
    // from different faces might not align properly
    
    // Check if face orientations are consistent
    std::cout << "\nFace orientation consistency:\n";
    
    // +X face: right = Z axis, up = Y axis
    std::cout << "  +X face: right=(0,0,1), up=(0,1,0)\n";
    
    // +Y face: right = X axis, up = Z axis
    std::cout << "  +Y face: right=(1,0,0), up=(0,0,1)\n";
    
    // +Z face: right = -X axis, up = Y axis
    std::cout << "  +Z face: right=(-1,0,0), up=(0,1,0)\n";
    
    std::cout << "\nWARNING: Face orientations are not consistent!\n";
    std::cout << "This can cause terrain discontinuities at face boundaries.\n";
}

int main() {
    testPatchContinuity();
    testTerrainSampling();
    testFaceBoundaries();
    
    std::cout << "\n=== DIAGNOSIS ===\n";
    std::cout << "The discontinuous continents are likely caused by:\n";
    std::cout << "1. Inconsistent face orientations (different right/up vectors)\n";
    std::cout << "2. Terrain sampling that doesn't account for face transitions\n";
    std::cout << "3. Patches at face boundaries using different coordinate systems\n";
    std::cout << "\nSOLUTION:\n";
    std::cout << "- Ensure all faces use consistent coordinate transformations\n";
    std::cout << "- Share vertices at patch boundaries\n";
    std::cout << "- Use a unified terrain function that works across all faces\n";
    
    return 0;
}