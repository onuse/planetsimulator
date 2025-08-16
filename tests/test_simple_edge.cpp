#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main() {
    std::cout << "=== SIMPLE EDGE TEST ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Simulate a patch at the edge of face 0 (+X face)
    // This patch should have vertices at Y=1.0
    glm::dvec3 minBounds(1.0, 0.75, 0.75);
    glm::dvec3 maxBounds(1.0, 1.0, 1.0);
    
    std::cout << "Patch bounds:" << std::endl;
    std::cout << "  Min: (" << minBounds.x << ", " << minBounds.y << ", " << minBounds.z << ")" << std::endl;
    std::cout << "  Max: (" << maxBounds.x << ", " << maxBounds.y << ", " << maxBounds.z << ")" << std::endl;
    
    // Create transform (simplified from GlobalPatchGenerator)
    glm::dvec3 range = maxBounds - minBounds;
    std::cout << "\nRange: (" << range.x << ", " << range.y << ", " << range.z << ")" << std::endl;
    
    glm::dmat4 transform(1.0);
    const double eps = 1e-6;
    
    if (range.x < eps) {
        // X is fixed
        double x = (minBounds.x + maxBounds.x) * 0.5;
        transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
        
        std::cout << "\nX is fixed at " << x << std::endl;
    }
    
    // Test UV corners
    std::cout << "\nTransformed corners:" << std::endl;
    glm::dvec4 corners[4] = {
        glm::dvec4(0.0, 0.0, 0.0, 1.0),  // Bottom-left
        glm::dvec4(1.0, 0.0, 0.0, 1.0),  // Bottom-right
        glm::dvec4(1.0, 1.0, 0.0, 1.0),  // Top-right
        glm::dvec4(0.0, 1.0, 0.0, 1.0)   // Top-left
    };
    
    for (int i = 0; i < 4; i++) {
        glm::dvec3 cubePos = glm::dvec3(transform * corners[i]);
        std::cout << "  UV(" << corners[i].x << "," << corners[i].y << ") -> ";
        std::cout << "(" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
        
        // Check if any vertex is at Y=1.0
        if (std::abs(cubePos.y - 1.0) < 0.001) {
            std::cout << "    -> This vertex is at the Y=1.0 edge!" << std::endl;
        }
    }
    
    // Test edge vertices (along V=1.0 line)
    std::cout << "\nEdge vertices (V=1.0):" << std::endl;
    for (int i = 0; i <= 4; i++) {
        double u = i / 4.0;
        glm::dvec4 edgeUV(u, 1.0, 0.0, 1.0);
        glm::dvec3 cubePos = glm::dvec3(transform * edgeUV);
        std::cout << "  UV(" << u << ",1.0) -> ";
        std::cout << "(" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
    }
    
    return 0;
}
