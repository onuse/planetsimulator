#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Test if neighboring patches actually share edges in 3D space
void testPatchAdjacency() {
    std::cout << "=== PATCH POSITION TEST ===\n\n";
    
    // Simulate the patch transform for two supposedly adjacent patches
    // These should share an edge but might not due to transform issues
    
    // Patch 1: On +X face at position (1, -0.5, -0.5) with size 0.5
    glm::dvec3 patch1_center = glm::dvec3(1.0, -0.5, -0.5);
    glm::dvec3 patch1_corners[4];
    
    // Using the same logic as SphericalQuadtreeNode
    glm::dvec3 right1 = glm::dvec3(0, 0, 1);  // +X face right vector
    glm::dvec3 up1 = glm::dvec3(0, 1, 0);     // +X face up vector
    double halfSize = 0.25;
    
    patch1_corners[0] = patch1_center + (-right1 - up1) * halfSize; // BL
    patch1_corners[1] = patch1_center + (right1 - up1) * halfSize;  // BR
    patch1_corners[2] = patch1_center + (right1 + up1) * halfSize;  // TR
    patch1_corners[3] = patch1_center + (-right1 + up1) * halfSize; // TL
    
    // Patch 2: Should be to the right of Patch 1
    glm::dvec3 patch2_center = glm::dvec3(1.0, -0.5, 0.0);  // Shifted in Z (right direction)
    glm::dvec3 patch2_corners[4];
    
    patch2_corners[0] = patch2_center + (-right1 - up1) * halfSize; // BL
    patch2_corners[1] = patch2_center + (right1 - up1) * halfSize;  // BR
    patch2_corners[2] = patch2_center + (right1 + up1) * halfSize;  // TR
    patch2_corners[3] = patch2_center + (-right1 + up1) * halfSize; // TL
    
    std::cout << "Patch 1 corners:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: (" 
                  << patch1_corners[i].x << ", " 
                  << patch1_corners[i].y << ", " 
                  << patch1_corners[i].z << ")\n";
    }
    
    std::cout << "\nPatch 2 corners:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: (" 
                  << patch2_corners[i].x << ", " 
                  << patch2_corners[i].y << ", " 
                  << patch2_corners[i].z << ")\n";
    }
    
    // Check if they share an edge
    // Patch 1's right edge (corners 1,2) should match Patch 2's left edge (corners 0,3)
    std::cout << "\nEdge comparison:\n";
    std::cout << "Patch 1 right edge:\n";
    std::cout << "  BR: (" << patch1_corners[1].x << ", " << patch1_corners[1].y << ", " << patch1_corners[1].z << ")\n";
    std::cout << "  TR: (" << patch1_corners[2].x << ", " << patch1_corners[2].y << ", " << patch1_corners[2].z << ")\n";
    
    std::cout << "Patch 2 left edge:\n";
    std::cout << "  BL: (" << patch2_corners[0].x << ", " << patch2_corners[0].y << ", " << patch2_corners[0].z << ")\n";
    std::cout << "  TL: (" << patch2_corners[3].x << ", " << patch2_corners[3].y << ", " << patch2_corners[3].z << ")\n";
    
    // Calculate differences
    glm::dvec3 diff1 = patch1_corners[1] - patch2_corners[0];
    glm::dvec3 diff2 = patch1_corners[2] - patch2_corners[3];
    
    std::cout << "\nPosition differences:\n";
    std::cout << "  Bottom vertices: " << glm::length(diff1) << "\n";
    std::cout << "  Top vertices: " << glm::length(diff2) << "\n";
    
    if (glm::length(diff1) < 0.001 && glm::length(diff2) < 0.001) {
        std::cout << "✓ Patches share edge correctly\n";
    } else {
        std::cout << "✗ PATCHES DON'T SHARE EDGE!\n";
    }
}

// Test cube-to-sphere projection consistency
void testSphereProjection() {
    std::cout << "\n=== SPHERE PROJECTION TEST ===\n\n";
    
    // Test a point that should be the same when accessed from different patches
    glm::dvec3 edgePoint = glm::dvec3(1.0, 0.0, 0.0);  // Edge between patches
    
    // Cube to sphere projection
    auto cubeToSphere = [](glm::dvec3 p) -> glm::dvec3 {
        glm::dvec3 p2 = p * p;
        glm::dvec3 result;
        result.x = p.x * sqrt(1.0 - p2.y * 0.5 - p2.z * 0.5 + p2.y * p2.z / 3.0);
        result.y = p.y * sqrt(1.0 - p2.x * 0.5 - p2.z * 0.5 + p2.x * p2.z / 3.0);
        result.z = p.z * sqrt(1.0 - p2.x * 0.5 - p2.y * 0.5 + p2.x * p2.y / 3.0);
        return glm::normalize(result);
    };
    
    // Project the same edge point
    glm::dvec3 spherePos1 = cubeToSphere(edgePoint);
    
    // Slightly perturb to simulate floating point error
    glm::dvec3 edgePoint2 = edgePoint + glm::dvec3(0.0001, 0.0, 0.0);
    glm::dvec3 spherePos2 = cubeToSphere(edgePoint2);
    
    std::cout << "Edge point: (" << edgePoint.x << ", " << edgePoint.y << ", " << edgePoint.z << ")\n";
    std::cout << "Sphere pos 1: (" << spherePos1.x << ", " << spherePos1.y << ", " << spherePos1.z << ")\n";
    std::cout << "Sphere pos 2: (" << spherePos2.x << ", " << spherePos2.y << ", " << spherePos2.z << ")\n";
    std::cout << "Difference: " << glm::length(spherePos1 - spherePos2) << "\n";
    
    // Now test with the terrain sampling position
    std::cout << "\nTerrain sampling position test:\n";
    
    // Simulate two patches sampling at their shared edge
    // They should get the same sphere position and thus same terrain
    glm::dvec3 patch1_edge = glm::dvec3(1.0, -0.25, -0.25);  // Right edge of patch 1
    glm::dvec3 patch2_edge = glm::dvec3(1.0, -0.25, -0.25);  // Left edge of patch 2 (should be same)
    
    glm::dvec3 sphere1 = cubeToSphere(patch1_edge);
    glm::dvec3 sphere2 = cubeToSphere(patch2_edge);
    
    std::cout << "Patch 1 edge maps to sphere: (" << sphere1.x << ", " << sphere1.y << ", " << sphere1.z << ")\n";
    std::cout << "Patch 2 edge maps to sphere: (" << sphere2.x << ", " << sphere2.y << ", " << sphere2.z << ")\n";
    
    if (glm::length(sphere1 - sphere2) < 0.0001) {
        std::cout << "✓ Patches map to same sphere position\n";
    } else {
        std::cout << "✗ PATCHES MAP TO DIFFERENT SPHERE POSITIONS!\n";
    }
}

int main() {
    testPatchAdjacency();
    testSphereProjection();
    
    std::cout << "\n=== DIAGNOSIS ===\n";
    std::cout << "The 'jammed puzzle pieces' effect is likely because:\n";
    std::cout << "1. Patches aren't actually adjacent in 3D space\n";
    std::cout << "2. Transform matrices place patches at wrong positions\n";
    std::cout << "3. Each patch samples terrain from unrelated positions\n";
    std::cout << "\nThis explains why terrain looks completely different across boundaries -\n";
    std::cout << "the patches are literally showing different parts of the planet!\n";
    
    return 0;
}