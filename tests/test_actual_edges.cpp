#include <iostream>
#include <glm/glm.hpp>
#include <cmath>

// Test what edges ACTUALLY connect between faces

int main() {
    std::cout << "Checking actual edge connections:\n\n";
    
    // For +X face (X=1): U->Z, V->Y
    // At U=1, V=0.5: X=1, Y=0, Z=1
    std::cout << "+X face at edge U=1, V=0.5:\n";
    std::cout << "  X = 1 (fixed)\n";
    std::cout << "  Y = minY + V*rangeY = -1 + 0.5*2 = 0\n";
    std::cout << "  Z = minZ + U*rangeZ = -1 + 1*2 = 1\n";
    std::cout << "  Position: (1, 0, 1)\n\n";
    
    // For +Z face (Z=1): U->X, V->Y  
    // At U=1, V=0.5: X=1, Y=0, Z=1
    std::cout << "+Z face at edge U=1, V=0.5:\n";
    std::cout << "  X = minX + U*rangeX = -1 + 1*2 = 1\n";
    std::cout << "  Y = minY + V*rangeY = -1 + 0.5*2 = 0\n";
    std::cout << "  Z = 1 (fixed)\n";
    std::cout << "  Position: (1, 0, 1)\n\n";
    
    std::cout << "THEY MATCH! The mappings ARE compatible!\n\n";
    
    // So why do we have gaps? Let's check with actual transform matrices
    
    // +X face transform for a patch at the boundary
    glm::dmat4 xTransform(1.0);
    xTransform[0] = glm::dvec4(0.0, 0.0, 0.5, 0.0);    // U -> Z, range=0.5
    xTransform[1] = glm::dvec4(0.0, 0.5, 0.0, 0.0);    // V -> Y, range=0.5  
    xTransform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    xTransform[3] = glm::dvec4(1.0, 0.5, 0.5, 1.0);    // offset
    
    // +Z face transform for a patch at the boundary
    glm::dmat4 zTransform(1.0);
    zTransform[0] = glm::dvec4(0.5, 0.0, 0.0, 0.0);    // U -> X, range=0.5
    zTransform[1] = glm::dvec4(0.0, 0.5, 0.0, 0.0);    // V -> Y, range=0.5
    zTransform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    zTransform[3] = glm::dvec4(0.5, 0.5, 1.0, 1.0);    // offset
    
    // Test a point at the shared edge
    glm::dvec4 xUV(1.0, 0.5, 0.0, 1.0);  // +X face at right edge
    glm::dvec4 zUV(1.0, 0.5, 0.0, 1.0);  // +Z face at right edge
    
    glm::dvec3 xPos = glm::dvec3(xTransform * xUV);
    glm::dvec3 zPos = glm::dvec3(zTransform * zUV);
    
    std::cout << "With actual transforms:\n";
    std::cout << "+X face UV(1,0.5) -> " << xPos.x << ", " << xPos.y << ", " << xPos.z << "\n";
    std::cout << "+Z face UV(1,0.5) -> " << zPos.x << ", " << zPos.y << ", " << zPos.z << "\n";
    
    double gap = glm::length(xPos - zPos);
    std::cout << "Gap: " << gap << " units\n";
    
    if (gap > 0.0001) {
        std::cout << "\nThe issue: patches at boundaries might have slightly different transforms!\n";
        std::cout << "Even tiny differences in offset or range can cause gaps.\n";
    }
    
    return 0;
}
