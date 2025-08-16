// MINIMAL REPRODUCTION OF THE 6 MILLION METER BUG
// No dependencies except the transform logic

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>

// Production transform (from GlobalPatchGenerator)
glm::dmat4 createProductionTransform(const glm::dvec3& minBounds, const glm::dvec3& maxBounds) {
    glm::dmat4 transform(1.0);
    glm::dvec3 range = maxBounds - minBounds;
    const double eps = 1e-6;
    
    if (range.x < eps) {
        // X is fixed - +X or -X face
        double x = minBounds.x;
        transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
    }
    else if (range.y < eps) {
        // Y is fixed - +Y or -Y face
        double y = minBounds.y;
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // V -> Z
        transform[3] = glm::dvec4(minBounds.x, y, minBounds.z, 1.0);
    }
    else if (range.z < eps) {
        // Z is fixed - +Z or -Z face
        double z = minBounds.z;
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[3] = glm::dvec4(minBounds.x, minBounds.y, z, 1.0);
    }
    
    transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return transform;
}

// Test transform (from test_face_boundary_alignment.cpp)
glm::dmat4 createTestTransform(int face, const glm::dvec3& center, double size) {
    glm::dmat4 transform(1.0);
    double halfSize = size * 0.5;
    
    switch(face) {
        case 0: // +X face
            transform[0] = glm::dvec4(0.0, 0.0, size, 0.0);    // U -> Z
            transform[1] = glm::dvec4(0.0, size, 0.0, 0.0);    // V -> Y
            transform[3] = glm::dvec4(1.0, center.y - halfSize, center.z - halfSize, 1.0);
            break;
        case 2: // +Y face
            transform[0] = glm::dvec4(size, 0.0, 0.0, 0.0);    // U -> X
            transform[1] = glm::dvec4(0.0, 0.0, size, 0.0);    // V -> Z
            transform[3] = glm::dvec4(center.x - halfSize, 1.0, center.z - halfSize, 1.0);
            break;
        case 4: // +Z face
            transform[0] = glm::dvec4(size, 0.0, 0.0, 0.0);    // U -> X
            transform[1] = glm::dvec4(0.0, size, 0.0, 0.0);    // V -> Y
            transform[3] = glm::dvec4(center.x - halfSize, center.y - halfSize, 1.0, 1.0);
            break;
    }
    transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return transform;
}

// Cube to sphere
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos, double radius) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos) * radius;
}

int main() {
    std::cout << "=== MINIMAL BUG REPRODUCTION ===\n\n";
    
    const double PLANET_RADIUS = 6371000.0;
    
    // Test case: +Z/+X boundary
    // Two patches that should share an edge at X=1, Z=1
    
    std::cout << "TEST: +Z face patch at edge with +X face\n";
    std::cout << "----------------------------------------------\n";
    
    // +Z face patch (near X=1 edge)
    glm::dvec3 zMinBounds(0.5, -0.5, 1.0);
    glm::dvec3 zMaxBounds(1.0, 0.5, 1.0);
    glm::dvec3 zCenter(0.75, 0.0, 1.0);
    
    // +X face patch (near Z=1 edge)  
    glm::dvec3 xMinBounds(1.0, -0.5, 0.5);
    glm::dvec3 xMaxBounds(1.0, 0.5, 1.0);
    glm::dvec3 xCenter(1.0, 0.0, 0.75);
    
    // Generate transforms
    auto prodTransformZ = createProductionTransform(zMinBounds, zMaxBounds);
    auto prodTransformX = createProductionTransform(xMinBounds, xMaxBounds);
    
    auto testTransformZ = createTestTransform(4, zCenter, 0.5);  // face 4 = +Z
    auto testTransformX = createTestTransform(0, xCenter, 0.5);  // face 0 = +X
    
    // Test a shared vertex at UV(1,0) for +Z and UV(0,1) for +X
    // This should map to cube position (1, -0.5, 1)
    
    std::cout << "\nShared vertex at cube position (1, -0.5, 1):\n";
    
    // Production transforms
    glm::dvec3 prodCubeZ = glm::dvec3(prodTransformZ * glm::dvec4(1, 0, 0, 1));
    glm::dvec3 prodCubeX = glm::dvec3(prodTransformX * glm::dvec4(0, 1, 0, 1));
    
    std::cout << "\nPRODUCTION:\n";
    std::cout << "  +Z UV(1,0) -> cube(" << prodCubeZ.x << ", " << prodCubeZ.y << ", " << prodCubeZ.z << ")\n";
    std::cout << "  +X UV(0,1) -> cube(" << prodCubeX.x << ", " << prodCubeX.y << ", " << prodCubeX.z << ")\n";
    
    glm::dvec3 prodSphereZ = cubeToSphere(prodCubeZ, PLANET_RADIUS);
    glm::dvec3 prodSphereX = cubeToSphere(prodCubeX, PLANET_RADIUS);
    double prodGap = glm::length(prodSphereZ - prodSphereX);
    
    std::cout << "  Gap: " << prodGap << " meters\n";
    
    // Test transforms
    glm::dvec3 testCubeZ = glm::dvec3(testTransformZ * glm::dvec4(1, 0, 0, 1));
    glm::dvec3 testCubeX = glm::dvec3(testTransformX * glm::dvec4(0, 1, 0, 1));
    
    std::cout << "\nTEST:\n";
    std::cout << "  +Z UV(1,0) -> cube(" << testCubeZ.x << ", " << testCubeZ.y << ", " << testCubeZ.z << ")\n";
    std::cout << "  +X UV(0,1) -> cube(" << testCubeX.x << ", " << testCubeX.y << ", " << testCubeX.z << ")\n";
    
    glm::dvec3 testSphereZ = cubeToSphere(testCubeZ, PLANET_RADIUS);
    glm::dvec3 testSphereX = cubeToSphere(testCubeX, PLANET_RADIUS);
    double testGap = glm::length(testSphereZ - testSphereX);
    
    std::cout << "  Gap: " << testGap << " meters\n";
    
    std::cout << "\n=== RESULT ===\n";
    if (prodGap > 1000.0) {
        std::cout << "✗ PRODUCTION TRANSFORM HAS THE BUG!\n";
        std::cout << "  Gap is " << prodGap / 1e6 << " million meters\n";
    } else if (testGap > 1000.0) {
        std::cout << "✗ TEST TRANSFORM HAS THE BUG!\n";
        std::cout << "  Gap is " << testGap / 1e6 << " million meters\n";
    } else {
        std::cout << "✓ Both transforms work correctly\n";
    }
    
    return 0;
}