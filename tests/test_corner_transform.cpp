#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== TESTING CORNER TRANSFORM ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Create a patch with INSET applied
    glm::vec3 minBounds(0.9995f, -0.9995f, -0.9995f);
    glm::vec3 maxBounds(0.9995f, 0.9995f, 0.9995f);
    
    core::GlobalPatchGenerator::GlobalPatch patch;
    patch.minBounds = minBounds;
    patch.maxBounds = maxBounds;
    patch.center = (minBounds + maxBounds) * 0.5f;
    patch.level = 0;
    patch.faceId = 0;
    
    std::cout << "Test patch with INSET:" << std::endl;
    std::cout << "  MinBounds: (" << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z << ")" << std::endl;
    std::cout << "  MaxBounds: (" << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z << ")" << std::endl;
    
    // Get corners
    glm::vec3 corners[4];
    patch.getCorners(corners);
    
    std::cout << "\nCorners from getCorners():" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "  Corner " << i << ": (" << corners[i].x << ", " << corners[i].y << ", " << corners[i].z << ")" << std::endl;
    }
    
    // Create transform and test UV mapping
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
    
    // Test UV corners through transform
    std::cout << "\nUV to Cube through transform:" << std::endl;
    glm::dvec4 uvCorners[4] = {
        glm::dvec4(0.0, 0.0, 0.0, 1.0),
        glm::dvec4(1.0, 0.0, 0.0, 1.0),
        glm::dvec4(1.0, 1.0, 0.0, 1.0),
        glm::dvec4(0.0, 1.0, 0.0, 1.0)
    };
    
    for (int i = 0; i < 4; i++) {
        glm::dvec3 transformed = glm::dvec3(transform * uvCorners[i]);
        std::cout << "  UV(" << uvCorners[i].x << "," << uvCorners[i].y << ") -> (" 
                  << transformed.x << ", " << transformed.y << ", " << transformed.z << ")" << std::endl;
        
        // Check for (0,0,0)
        if (transformed.x == 0.0 && transformed.y == 0.0 && transformed.z == 0.0) {
            std::cout << "    ERROR: Produced (0,0,0)!" << std::endl;
        }
    }
    
    // Test center UV (0.5, 0.5)
    glm::dvec4 center(0.5, 0.5, 0.0, 1.0);
    glm::dvec3 centerTransformed = glm::dvec3(transform * center);
    std::cout << "\nCenter UV(0.5,0.5) -> (" << centerTransformed.x << ", " 
              << centerTransformed.y << ", " << centerTransformed.z << ")" << std::endl;
    
    if (centerTransformed.x == 0.0 && centerTransformed.y == 0.0 && centerTransformed.z == 0.0) {
        std::cout << "  ERROR: Center produced (0,0,0)!" << std::endl;
    }
    
    return 0;
}
