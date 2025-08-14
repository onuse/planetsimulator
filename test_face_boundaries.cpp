#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

// ============================================================================
// TEST: Why do face boundary patches have 2.5 million meter gaps?
// ============================================================================

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

// Create transform matrix for a patch (from GlobalPatchGenerator)
glm::dmat4 createTransform(int face, const glm::dvec3& center, double size) {
    glm::dmat4 transform(1.0);
    
    double halfSize = size * 0.5;
    glm::dvec3 bl, br, tl;
    
    switch(face) {
        case 0: // +X
            bl = glm::dvec3(1.0, center.y - halfSize, center.z - halfSize);
            br = glm::dvec3(1.0, center.y - halfSize, center.z + halfSize);
            tl = glm::dvec3(1.0, center.y + halfSize, center.z - halfSize);
            break;
        case 1: // -X
            bl = glm::dvec3(-1.0, center.y - halfSize, center.z + halfSize);
            br = glm::dvec3(-1.0, center.y - halfSize, center.z - halfSize);
            tl = glm::dvec3(-1.0, center.y + halfSize, center.z + halfSize);
            break;
        case 2: // +Y
            bl = glm::dvec3(center.x - halfSize, 1.0, center.z - halfSize);
            br = glm::dvec3(center.x + halfSize, 1.0, center.z - halfSize);
            tl = glm::dvec3(center.x - halfSize, 1.0, center.z + halfSize);
            break;
        case 3: // -Y
            bl = glm::dvec3(center.x - halfSize, -1.0, center.z + halfSize);
            br = glm::dvec3(center.x + halfSize, -1.0, center.z + halfSize);
            tl = glm::dvec3(center.x - halfSize, -1.0, center.z - halfSize);
            break;
        case 4: // +Z
            bl = glm::dvec3(center.x - halfSize, center.y - halfSize, 1.0);
            br = glm::dvec3(center.x + halfSize, center.y - halfSize, 1.0);
            tl = glm::dvec3(center.x - halfSize, center.y + halfSize, 1.0);
            break;
        case 5: // -Z
            bl = glm::dvec3(center.x + halfSize, center.y - halfSize, -1.0);
            br = glm::dvec3(center.x - halfSize, center.y - halfSize, -1.0);
            tl = glm::dvec3(center.x + halfSize, center.y + halfSize, -1.0);
            break;
    }
    
    // Build transform columns
    transform[0] = glm::dvec4(br - bl, 0.0);  // Right vector (U direction)
    transform[1] = glm::dvec4(tl - bl, 0.0);  // Up vector (V direction)
    transform[2] = glm::dvec4(0, 0, 0, 0);    // Not used
    transform[3] = glm::dvec4(bl, 1.0);       // Origin
    
    return transform;
}

// Transform UV to world position (emulating vertex shader)
glm::dvec3 transformVertex(glm::dvec2 uv, const glm::dmat4& transform) {
    // UV to local space
    glm::dvec4 localPos(uv.x, uv.y, 0.0, 1.0);
    
    // Transform to cube position
    glm::dvec3 cubePos = glm::dvec3(transform * localPos);
    
    // Snap to boundaries (with current epsilon)
    const double EPSILON = 0.001;
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

void testFaceBoundary() {
    std::cout << "=== Testing Face Boundary Patches ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Test case: Where +Z face meets +X face
    // These patches should share an edge
    
    // Patch on +Z face at the edge (x=1)
    int face1 = 4; // +Z
    glm::dvec3 center1(0.5, 0.0, 1.0); // Right side of +Z face
    double size1 = 1.0;
    glm::dmat4 transform1 = createTransform(face1, center1, size1);
    
    // Patch on +X face at the edge (z=1)  
    int face2 = 0; // +X
    glm::dvec3 center2(1.0, 0.0, 0.5); // Top side of +X face
    double size2 = 1.0;
    glm::dmat4 transform2 = createTransform(face2, center2, size2);
    
    std::cout << "Patch 1: Face +Z, center (" << center1.x << "," << center1.y << "," << center1.z << ")\n";
    std::cout << "Patch 2: Face +X, center (" << center2.x << "," << center2.y << "," << center2.z << ")\n\n";
    
    // Test points along what should be the shared edge
    std::cout << "Testing shared edge vertices:\n";
    std::cout << "These should be at the same 3D position!\n\n";
    
    for (int i = 0; i <= 4; i++) {
        double v = i / 4.0;
        
        // Right edge of +Z patch (u=1, varying v)
        glm::dvec2 uv1(1.0, v);
        glm::dvec3 world1 = transformVertex(uv1, transform1);
        
        // Top edge of +X patch (v=1, varying u)  
        glm::dvec2 uv2(v, 1.0);
        glm::dvec3 world2 = transformVertex(uv2, transform2);
        
        double gap = glm::length(world1 - world2);
        
        std::cout << "v=" << v << ":\n";
        std::cout << "  +Z patch UV(" << uv1.x << "," << uv1.y << ") -> ";
        std::cout << "(" << world1.x/1000 << "," << world1.y/1000 << "," << world1.z/1000 << ") km\n";
        std::cout << "  +X patch UV(" << uv2.x << "," << uv2.y << ") -> ";
        std::cout << "(" << world2.x/1000 << "," << world2.y/1000 << "," << world2.z/1000 << ") km\n";
        std::cout << "  Gap: " << gap << " meters ";
        
        if (gap < 1.0) {
            std::cout << "✓";
        } else if (gap < 100.0) {
            std::cout << "⚠";
        } else {
            std::cout << "✗ HUGE GAP!";
        }
        std::cout << "\n\n";
    }
    
    // Now test with correct mapping
    std::cout << "\n=== ANALYSIS ===\n";
    std::cout << "The issue is clear: patches at face boundaries don't share vertices!\n";
    std::cout << "The UV mapping is incorrect - the edges don't align.\n";
    
    // Show the cube positions to understand the problem
    std::cout << "\nCube positions at boundary:\n";
    for (int i = 0; i <= 2; i++) {
        double v = i / 2.0;
        
        glm::dvec4 localPos1(1.0, v, 0.0, 1.0);
        glm::dvec3 cubePos1 = glm::dvec3(transform1 * localPos1);
        
        glm::dvec4 localPos2(v, 1.0, 0.0, 1.0);
        glm::dvec3 cubePos2 = glm::dvec3(transform2 * localPos2);
        
        std::cout << "+Z right edge v=" << v << ": cube=(" 
                  << cubePos1.x << "," << cubePos1.y << "," << cubePos1.z << ")\n";
        std::cout << "+X top edge u=" << v << ": cube=(" 
                  << cubePos2.x << "," << cubePos2.y << "," << cubePos2.z << ")\n\n";
    }
}

int main() {
    testFaceBoundary();
    
    std::cout << "\n=== CONCLUSION ===\n";
    std::cout << "The transform matrices are not producing matching cube positions\n";
    std::cout << "at face boundaries. This needs to be fixed in createTransform().\n";
    
    return 0;
}