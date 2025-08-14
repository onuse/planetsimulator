#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

const double PLANET_RADIUS = 6371000.0;

// Cube to sphere projection (from shader)
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

void testFaceBoundaryAlignment() {
    std::cout << "=== FACE BOUNDARY ALIGNMENT TEST ===\n\n";
    std::cout << std::fixed << std::setprecision(6);
    
    // Test case: Patches at the edge between +Z and +X faces
    // These patches should share vertices along the edge x=1, z=1
    
    std::cout << "Testing edge between +Z face and +X face:\n";
    std::cout << "Shared edge should be at x=1, z=1\n\n";
    
    // +Z face patch near right edge
    // This patch has z=1 (fixed), x ranges from 0.5 to 1.0, y ranges from -0.5 to 0.5
    std::cout << "+Z Face Patch (near right edge):\n";
    std::cout << "  Bounds: x=[0.5, 1.0], y=[-0.5, 0.5], z=1.0\n";
    
    // Corners of +Z patch
    glm::dvec3 zPatchCorners[4] = {
        glm::dvec3(0.5, -0.5, 1.0),  // BL
        glm::dvec3(1.0, -0.5, 1.0),  // BR (on edge)
        glm::dvec3(1.0, 0.5, 1.0),   // TR (on edge)
        glm::dvec3(0.5, 0.5, 1.0)    // TL
    };
    
    // +X face patch near top edge
    // This patch has x=1 (fixed), y ranges from -0.5 to 0.5, z ranges from 0.5 to 1.0
    std::cout << "\n+X Face Patch (near top edge):\n";
    std::cout << "  Bounds: x=1.0, y=[-0.5, 0.5], z=[0.5, 1.0]\n";
    
    // Corners of +X patch
    glm::dvec3 xPatchCorners[4] = {
        glm::dvec3(1.0, -0.5, 0.5),  // BL
        glm::dvec3(1.0, -0.5, 1.0),  // BR (on edge)
        glm::dvec3(1.0, 0.5, 1.0),   // TR (on edge)
        glm::dvec3(1.0, 0.5, 0.5)    // TL
    };
    
    std::cout << "\nShared Edge Analysis:\n";
    std::cout << "The edge x=1, z=1, y=[-0.5, 0.5] should be shared\n\n";
    
    // Check if the shared corners match
    std::cout << "Corner comparison (cube space):\n";
    std::cout << "  +Z patch BR: (" << zPatchCorners[1].x << ", " << zPatchCorners[1].y << ", " << zPatchCorners[1].z << ")\n";
    std::cout << "  +X patch BR: (" << xPatchCorners[1].x << ", " << xPatchCorners[1].y << ", " << xPatchCorners[1].z << ")\n";
    std::cout << "  Match? " << (zPatchCorners[1] == xPatchCorners[1] ? "YES" : "NO") << "\n\n";
    
    std::cout << "  +Z patch TR: (" << zPatchCorners[2].x << ", " << zPatchCorners[2].y << ", " << zPatchCorners[2].z << ")\n";
    std::cout << "  +X patch TR: (" << xPatchCorners[2].x << ", " << xPatchCorners[2].y << ", " << xPatchCorners[2].z << ")\n";
    std::cout << "  Match? " << (zPatchCorners[2] == xPatchCorners[2] ? "YES" : "NO") << "\n\n";
    
    // Now project to sphere and check gaps
    std::cout << "After sphere projection:\n";
    
    glm::dvec3 zBR_sphere = cubeToSphere(zPatchCorners[1]) * PLANET_RADIUS;
    glm::dvec3 xBR_sphere = cubeToSphere(xPatchCorners[1]) * PLANET_RADIUS;
    
    glm::dvec3 zTR_sphere = cubeToSphere(zPatchCorners[2]) * PLANET_RADIUS;
    glm::dvec3 xTR_sphere = cubeToSphere(xPatchCorners[2]) * PLANET_RADIUS;
    
    double gapBR = glm::length(zBR_sphere - xBR_sphere);
    double gapTR = glm::length(zTR_sphere - xTR_sphere);
    
    std::cout << "  Bottom-right corner gap: " << gapBR << " meters\n";
    std::cout << "  Top-right corner gap: " << gapTR << " meters\n";
    
    // Test vertices along the shared edge
    std::cout << "\nTesting along entire shared edge:\n";
    double maxGap = 0.0;
    for (int i = 0; i <= 10; i++) {
        double t = i / 10.0;
        double y = -0.5 + t * 1.0;  // y from -0.5 to 0.5
        
        // Point on edge from +Z face perspective
        glm::dvec3 zPoint(1.0, y, 1.0);
        glm::dvec3 zSphere = cubeToSphere(zPoint) * PLANET_RADIUS;
        
        // Same point from +X face perspective (should be identical!)
        glm::dvec3 xPoint(1.0, y, 1.0);
        glm::dvec3 xSphere = cubeToSphere(xPoint) * PLANET_RADIUS;
        
        double gap = glm::length(zSphere - xSphere);
        maxGap = std::max(maxGap, gap);
        
        std::cout << "  y=" << std::setw(5) << y << ": gap = " << std::setw(10) << gap << " meters";
        if (gap < 0.01) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ ERROR!\n";
        }
    }
    
    std::cout << "\nMaximum gap along edge: " << maxGap << " meters\n";
    
    if (maxGap < 1.0) {
        std::cout << "\n✓ PASS: Face boundary vertices are properly aligned!\n";
    } else {
        std::cout << "\n✗ FAIL: Large gaps at face boundaries!\n";
    }
}

void testPatchTransform() {
    std::cout << "\n=== PATCH TRANSFORM TEST ===\n\n";
    
    // Test the transform matrix approach
    // +Z face patch
    glm::dmat4 zTransform(1.0);
    zTransform[0] = glm::dvec4(0.5, 0.0, 0.0, 0.0);  // U -> X (range 0.5)
    zTransform[1] = glm::dvec4(0.0, 1.0, 0.0, 0.0);  // V -> Y (range 1.0)
    zTransform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);  // Dummy
    zTransform[3] = glm::dvec4(0.5, -0.5, 1.0, 1.0); // Origin at (0.5, -0.5, 1.0)
    
    // Test UV (1, 1) which should map to top-right corner (1.0, 0.5, 1.0)
    glm::dvec4 uv11(1.0, 1.0, 0.0, 1.0);
    glm::dvec3 result = glm::dvec3(zTransform * uv11);
    
    std::cout << "Transform test for +Z patch:\n";
    std::cout << "  UV(1,1) -> (" << result.x << ", " << result.y << ", " << result.z << ")\n";
    std::cout << "  Expected: (1.0, 0.5, 1.0)\n";
    std::cout << "  Match? " << (std::abs(result.x - 1.0) < 0.001 && 
                                  std::abs(result.y - 0.5) < 0.001 && 
                                  std::abs(result.z - 1.0) < 0.001 ? "YES" : "NO") << "\n";
}

int main() {
    testFaceBoundaryAlignment();
    testPatchTransform();
    
    std::cout << "\n=== CONCLUSION ===\n";
    std::cout << "If the cube-space coordinates match but sphere projections don't,\n";
    std::cout << "the issue is in the cubeToSphere function or precision.\n";
    std::cout << "If the cube-space coordinates don't match, the issue is in patch generation.\n";
    
    return 0;
}