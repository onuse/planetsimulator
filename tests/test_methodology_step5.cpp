// STEP 5: WHY do cross-face boundaries have gaps?
// Let's trace the exact vertex generation

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>

// Replicate the exact cube-to-sphere transformation
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos, double radius) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos) * radius;
}

int main() {
    std::cout << "=== ISOLATING THE CROSS-FACE GAP ISSUE ===\n\n";
    
    const double radius = 6371000.0;
    
    // The shared edge should be at cube position (1,1,Z) where Z goes from -0.5 to 0.5
    std::cout << "Testing a single shared vertex at corner (1,1,-0.5):\n\n";
    
    // From +X face perspective:
    // UV(0,1) should map to (1,1,-0.5)
    std::cout << "From +X face (U->Z, V->Y):\n";
    glm::dmat4 xTransform(1.0);
    xTransform[0] = glm::dvec4(0.0, 0.0, 1.0, 0.0);  // U -> Z
    xTransform[1] = glm::dvec4(0.0, 0.5, 0.0, 0.0);  // V -> Y (half range)
    xTransform[3] = glm::dvec4(1.0, 0.5, -0.5, 1.0);  // Origin
    
    glm::dvec3 xCubePos = glm::dvec3(xTransform * glm::dvec4(0, 1, 0, 1));
    std::cout << "  UV(0,1) -> cube(" << xCubePos.x << ", " << xCubePos.y << ", " << xCubePos.z << ")\n";
    
    // From +Y face perspective:
    // UV(1,0) should map to (1,1,-0.5)
    std::cout << "\nFrom +Y face (U->X, V->Z):\n";
    glm::dmat4 yTransform(1.0);
    yTransform[0] = glm::dvec4(0.5, 0.0, 0.0, 0.0);  // U -> X (half range)
    yTransform[1] = glm::dvec4(0.0, 0.0, 1.0, 0.0);  // V -> Z
    yTransform[3] = glm::dvec4(0.5, 1.0, -0.5, 1.0);  // Origin
    
    glm::dvec3 yCubePos = glm::dvec3(yTransform * glm::dvec4(1, 0, 0, 1));
    std::cout << "  UV(1,0) -> cube(" << yCubePos.x << ", " << yCubePos.y << ", " << yCubePos.z << ")\n";
    
    // Check if they're the same
    std::cout << "\n=== CUBE SPACE COMPARISON ===\n";
    glm::dvec3 diff = xCubePos - yCubePos;
    std::cout << "Difference: (" << diff.x << ", " << diff.y << ", " << diff.z << ")\n";
    double cubeDist = glm::length(diff);
    std::cout << "Distance in cube space: " << cubeDist << "\n";
    
    if (cubeDist < 0.0001) {
        std::cout << "✓ Cube positions match!\n";
    } else {
        std::cout << "✗ Cube positions DON'T match!\n";
        std::cout << "THIS IS THE PROBLEM - transforms produce different cube positions!\n";
        return 1;
    }
    
    // Now transform to sphere
    std::cout << "\n=== SPHERE SPACE ===\n";
    glm::dvec3 xSphere = cubeToSphere(xCubePos, radius);
    glm::dvec3 ySphere = cubeToSphere(yCubePos, radius);
    
    std::cout << "+X vertex: (" << std::fixed << std::setprecision(2) 
              << xSphere.x << ", " << xSphere.y << ", " << xSphere.z << ")\n";
    std::cout << "+Y vertex: (" << ySphere.x << ", " << ySphere.y << ", " << ySphere.z << ")\n";
    
    double sphereDist = glm::length(xSphere - ySphere);
    std::cout << "Distance in sphere space: " << sphereDist << " meters\n";
    
    // Test with different parameterization
    std::cout << "\n=== TESTING VERTEX ORDER ===\n";
    std::cout << "Maybe the vertices are in different order?\n\n";
    
    for (int i = 0; i < 5; i++) {
        float t = i / 4.0f;
        
        // +X face: varies Z from -0.5 to 0.5
        glm::dvec3 xPos = glm::dvec3(xTransform * glm::dvec4(t, 1, 0, 1));
        
        // +Y face: also varies Z from -0.5 to 0.5
        glm::dvec3 yPos = glm::dvec3(yTransform * glm::dvec4(1, t, 0, 1));
        
        std::cout << "t=" << t << ":\n";
        std::cout << "  +X: cube(" << xPos.x << ", " << xPos.y << ", " << xPos.z << ")\n";
        std::cout << "  +Y: cube(" << yPos.x << ", " << yPos.y << ", " << yPos.z << ")\n";
        std::cout << "  Match? " << (glm::length(xPos - yPos) < 0.0001 ? "✓" : "✗") << "\n\n";
    }
    
    return 0;
}