#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== DEBUG: What is GlobalPatchGenerator Actually Doing? ===\n\n";
    std::cout << std::fixed << std::setprecision(10);
    
    // Create a patch on the +Z face at the right edge (should touch +X face)
    core::GlobalPatchGenerator::GlobalPatch zPatch;
    zPatch.minBounds = glm::vec3(0.5, -0.5, 1.0);  // Right half of +Z face
    zPatch.maxBounds = glm::vec3(1.0, 0.5, 1.0);
    zPatch.center = (zPatch.minBounds + zPatch.maxBounds) * 0.5f;
    zPatch.level = 1;
    zPatch.faceId = 4; // +Z
    
    std::cout << "+Z Patch:\n";
    std::cout << "  minBounds: (" << zPatch.minBounds.x << ", " << zPatch.minBounds.y << ", " << zPatch.minBounds.z << ")\n";
    std::cout << "  maxBounds: (" << zPatch.maxBounds.x << ", " << zPatch.maxBounds.y << ", " << zPatch.maxBounds.z << ")\n";
    
    glm::dvec3 range = glm::dvec3(zPatch.maxBounds - zPatch.minBounds);
    std::cout << "  range: (" << range.x << ", " << range.y << ", " << range.z << ")\n";
    
    // Get the transform
    glm::dmat4 transform = zPatch.createTransform();
    
    std::cout << "\nTransform matrix:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: ";
        for (int j = 0; j < 4; j++) {
            std::cout << transform[j][i] << " ";
        }
        std::cout << "\n";
    }
    
    // Test what UV (1,0) maps to (right bottom corner of patch)
    glm::dvec4 uv(1.0, 0.0, 0.0, 1.0);
    glm::dvec3 worldPos = glm::dvec3(transform * uv);
    
    std::cout << "\nUV (1,0) maps to world: (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")\n";
    std::cout << "Expected: (1.0, -0.5, 1.0) - the corner where +Z meets +X\n";
    
    // Check if the issue is in the transform generation
    std::cout << "\n=== CHECKING TRANSFORM SCALE ===\n";
    
    // The transform should map [0,1] to the patch bounds
    // Column 0 is what U maps to
    glm::dvec3 uScale(transform[0][0], transform[0][1], transform[0][2]);
    std::cout << "U maps to direction: (" << uScale.x << ", " << uScale.y << ", " << uScale.z << ")\n";
    std::cout << "U scale magnitude: " << glm::length(uScale) << "\n";
    
    // Column 1 is what V maps to
    glm::dvec3 vScale(transform[1][0], transform[1][1], transform[1][2]);
    std::cout << "V maps to direction: (" << vScale.x << ", " << vScale.y << ", " << vScale.z << ")\n";
    std::cout << "V scale magnitude: " << glm::length(vScale) << "\n";
    
    // Column 3 is the translation (where UV(0,0) maps to)
    glm::dvec3 translation(transform[3][0], transform[3][1], transform[3][2]);
    std::cout << "Translation (UV 0,0 maps to): (" << translation.x << ", " << translation.y << ", " << translation.z << ")\n";
    
    // Test all 4 corners
    std::cout << "\n=== CORNER MAPPING ===\n";
    glm::dvec2 corners[4] = {
        glm::dvec2(0, 0), glm::dvec2(1, 0), 
        glm::dvec2(1, 1), glm::dvec2(0, 1)
    };
    
    for (int i = 0; i < 4; i++) {
        glm::dvec4 corner(corners[i].x, corners[i].y, 0, 1);
        glm::dvec3 mapped = glm::dvec3(transform * corner);
        std::cout << "UV(" << corners[i].x << "," << corners[i].y << ") -> ("
                  << mapped.x << ", " << mapped.y << ", " << mapped.z << ")\n";
    }
    
    std::cout << "\n=== DIAGNOSIS ===\n";
    if (glm::length(uScale) < 0.01) {
        std::cout << "ERROR: U scale is near zero (" << glm::length(uScale) << ")!\n";
        std::cout << "This would cause the patch to be collapsed in the U direction.\n";
    }
    if (glm::length(vScale) < 0.01) {
        std::cout << "ERROR: V scale is near zero (" << glm::length(vScale) << ")!\n";
        std::cout << "This would cause the patch to be collapsed in the V direction.\n";
    }
    
    // Check if we're getting the expected 0.5 x 1.0 size
    double expectedUSize = 0.5;  // X goes from 0.5 to 1.0
    double expectedVSize = 1.0;  // Y goes from -0.5 to 0.5
    
    if (std::abs(glm::length(uScale) - expectedUSize) > 0.001) {
        std::cout << "WARNING: U scale is " << glm::length(uScale) 
                  << " but expected " << expectedUSize << "\n";
    }
    if (std::abs(glm::length(vScale) - expectedVSize) > 0.001) {
        std::cout << "WARNING: V scale is " << glm::length(vScale) 
                  << " but expected " << expectedVSize << "\n";
    }
    
    return 0;
}