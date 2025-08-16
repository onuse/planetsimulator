#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== TESTING DEGENERATE TRANSFORMS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Test a patch that might produce (0,0,0)
    // This could happen if minBounds == maxBounds in all varying dimensions
    glm::vec3 minBounds(1.0f, 0.0f, 0.0f);
    glm::vec3 maxBounds(1.0f, 0.0f, 0.0f);
    
    core::GlobalPatchGenerator::GlobalPatch patch;
    patch.minBounds = minBounds;
    patch.maxBounds = maxBounds;
    patch.center = (minBounds + maxBounds) * 0.5f;
    patch.level = 5;
    patch.faceId = 0;
    
    std::cout << "Degenerate patch:" << std::endl;
    std::cout << "  MinBounds: (" << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z << ")" << std::endl;
    std::cout << "  MaxBounds: (" << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z << ")" << std::endl;
    
    glm::dmat4 transform = patch.createTransform();
    
    std::cout << "\nTransform matrix:" << std::endl;
    for (int row = 0; row < 4; row++) {
        std::cout << "  [";
        for (int col = 0; col < 4; col++) {
            std::cout << std::setw(15) << transform[col][row];
            if (col < 3) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
    
    // Test UV(0.5, 0.5)
    glm::dvec4 center(0.5, 0.5, 0.0, 1.0);
    glm::dvec3 transformed = glm::dvec3(transform * center);
    std::cout << "\nUV(0.5,0.5) -> (" << transformed.x << ", " << transformed.y << ", " << transformed.z << ")" << std::endl;
    
    if (transformed.x == 0.0 && transformed.y == 0.0 && transformed.z == 0.0) {
        std::cout << "ERROR: Transform produces (0,0,0) which will cause NaN in cubeToSphere!" << std::endl;
    }
    
    return 0;
}
