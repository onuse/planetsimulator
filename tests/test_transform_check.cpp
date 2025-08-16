#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== TRANSFORM CHECK ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Create a test patch with INSET
    glm::vec3 minBounds(1.0f, -0.9995f, -0.9995f);
    glm::vec3 maxBounds(1.0f, 0.9995f, 0.9995f);
    
    core::GlobalPatchGenerator::GlobalPatch patch;
    patch.minBounds = minBounds;
    patch.maxBounds = maxBounds;
    patch.center = (minBounds + maxBounds) * 0.5f;
    patch.level = 0;
    patch.faceId = 0;
    
    std::cout << "Test patch:" << std::endl;
    std::cout << "  MinBounds: (" << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z << ")" << std::endl;
    std::cout << "  MaxBounds: (" << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z << ")" << std::endl;
    std::cout << "  Center: (" << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << ")" << std::endl;
    
    // Create transform
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
    
    // Test UV corners
    std::cout << "\nTransformed corners:" << std::endl;
    glm::dvec4 corners[4] = {
        glm::dvec4(0.0, 0.0, 0.0, 1.0),  // UV(0,0)
        glm::dvec4(1.0, 0.0, 0.0, 1.0),  // UV(1,0)
        glm::dvec4(1.0, 1.0, 0.0, 1.0),  // UV(1,1)
        glm::dvec4(0.0, 1.0, 0.0, 1.0)   // UV(0,1)
    };
    
    for (int i = 0; i < 4; i++) {
        glm::dvec4 transformed = transform * corners[i];
        std::cout << "  UV(" << corners[i].x << "," << corners[i].y << ") -> ";
        std::cout << "(" << transformed.x << ", " << transformed.y << ", " << transformed.z << ")" << std::endl;
        
        // Check for NaN
        if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z)) {
            std::cout << "    ERROR: NaN detected!" << std::endl;
        }
    }
    
    // Check determinant
    double det = glm::determinant(transform);
    std::cout << "\nDeterminant: " << det << std::endl;
    if (std::abs(det) < 1e-10) {
        std::cout << "ERROR: Transform is singular (non-invertible)!" << std::endl;
    }
    
    return 0;
}