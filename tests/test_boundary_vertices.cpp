#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>

// Simplified cube-to-sphere for testing
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos, double radius) {
    return glm::normalize(cubePos) * radius;
}

int main() {
    std::cout << "=== TESTING BOUNDARY VERTEX PRECISION ===" << std::endl;
    std::cout << std::fixed << std::setprecision(15);
    
    double radius = 6371000.0;
    
    // Test: vertices at the edge between +X and +Y faces
    // These should produce the EXACT same world position
    
    // From +X face perspective: edge at y=1.0
    glm::dvec3 fromXFace(1.0, 1.0, 0.0);
    
    // From +Y face perspective: edge at x=1.0  
    glm::dvec3 fromYFace(1.0, 1.0, 0.0);
    
    // Transform both
    glm::dvec3 sphereFromX = cubeToSphere(fromXFace, radius);
    glm::dvec3 sphereFromY = cubeToSphere(fromYFace, radius);
    
    std::cout << "Edge point (1,1,0):" << std::endl;
    std::cout << "  From +X face: (" << sphereFromX.x << ", " << sphereFromX.y << ", " << sphereFromX.z << ")" << std::endl;
    std::cout << "  From +Y face: (" << sphereFromY.x << ", " << sphereFromY.y << ", " << sphereFromY.z << ")" << std::endl;
    std::cout << "  Difference: " << glm::length(sphereFromX - sphereFromY) << " meters" << std::endl;
    
    // Test with slight numerical differences (like from different calculations)
    glm::dvec3 fromXFace2(1.0, 0.999999999999999, 0.0);  // Tiny difference
    glm::dvec3 sphereFromX2 = cubeToSphere(fromXFace2, radius);
    
    std::cout << "\nWith tiny numerical error:" << std::endl;
    std::cout << "  From +X face (with error): (" << sphereFromX2.x << ", " << sphereFromX2.y << ", " << sphereFromX2.z << ")" << std::endl;
    std::cout << "  Difference: " << glm::length(sphereFromX - sphereFromX2) << " meters" << std::endl;
    
    // Test corner point (shared by 3 faces)
    glm::dvec3 corner(1.0, 1.0, 1.0);
    glm::dvec3 sphereCorner = cubeToSphere(corner, radius);
    
    // With tiny differences
    glm::dvec3 corner2(0.999999999999999, 1.0, 1.0);
    glm::dvec3 corner3(1.0, 0.999999999999999, 1.0);
    
    glm::dvec3 sphereCorner2 = cubeToSphere(corner2, radius);
    glm::dvec3 sphereCorner3 = cubeToSphere(corner3, radius);
    
    std::cout << "\nCorner point (1,1,1):" << std::endl;
    std::cout << "  Exact: (" << sphereCorner.x << ", " << sphereCorner.y << ", " << sphereCorner.z << ")" << std::endl;
    std::cout << "  With X error: difference = " << glm::length(sphereCorner - sphereCorner2) << " meters" << std::endl;
    std::cout << "  With Y error: difference = " << glm::length(sphereCorner - sphereCorner3) << " meters" << std::endl;
    
    return 0;
}
