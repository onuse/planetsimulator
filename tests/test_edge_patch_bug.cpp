#include <iostream>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

// Test for the edge/corner patch transform bug

int main() {
    std::cout << "=== Testing Edge/Corner Patch Transform Bug ===\n\n";
    
    // Create patches that aren't clearly on one face
    struct TestCase {
        const char* name;
        glm::dvec3 minBounds;
        glm::dvec3 maxBounds;
    };
    
    TestCase cases[] = {
        {"Face patch (normal)", glm::dvec3(1.0, -0.5, -0.5), glm::dvec3(1.0, 0.5, 0.5)},
        {"Edge patch (X-Z edge)", glm::dvec3(0.5, -0.5, 0.5), glm::dvec3(1.0, 0.5, 1.0)},
        {"Corner patch", glm::dvec3(0.5, 0.5, 0.5), glm::dvec3(1.0, 1.0, 1.0)},
        {"Center patch (shouldn't exist)", glm::dvec3(-0.25, -0.25, -0.25), glm::dvec3(0.25, 0.25, 0.25)}
    };
    
    for (auto& tc : cases) {
        core::GlobalPatchGenerator::GlobalPatch patch;
        patch.minBounds = tc.minBounds;
        patch.maxBounds = tc.maxBounds;
        patch.center = (patch.minBounds + patch.maxBounds) * 0.5;
        patch.level = 1;
        patch.faceId = 0;
        
        std::cout << tc.name << ":\n";
        std::cout << "  Bounds: (" << patch.minBounds.x << "," << patch.minBounds.y << "," << patch.minBounds.z 
                  << ") to (" << patch.maxBounds.x << "," << patch.maxBounds.y << "," << patch.maxBounds.z << ")\n";
        
        glm::dvec3 range = patch.maxBounds - patch.minBounds;
        std::cout << "  Range: (" << range.x << "," << range.y << "," << range.z << ")\n";
        
        // Check which condition would trigger
        const double eps = 1e-6;
        if (range.x < eps) {
            std::cout << "  -> X-fixed face transform\n";
        } else if (range.y < eps) {
            std::cout << "  -> Y-fixed face transform\n";
        } else if (range.z < eps) {
            std::cout << "  -> Z-fixed face transform\n";
        } else {
            std::cout << "  -> ✗ NO TRANSFORM CASE! USING UNINITIALIZED/IDENTITY!\n";
        }
        
        auto transform = patch.createTransform();
        
        // Test a few UV points
        glm::dvec4 testUV(0.5, 0.5, 0, 1);
        glm::dvec3 result = glm::dvec3(transform * testUV);
        std::cout << "  UV(0.5,0.5) -> (" << result.x << "," << result.y << "," << result.z << ")\n";
        
        // Check if transform looks wrong (all zeros or identity)
        bool isIdentity = true;
        bool hasProperTransform = false;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (i == j && std::abs(transform[i][j] - 1.0) > 0.001) isIdentity = false;
                if (i != j && std::abs(transform[i][j]) > 0.001) {
                    isIdentity = false;
                    hasProperTransform = true;
                }
            }
        }
        
        if (!hasProperTransform) {
            std::cout << "  ✗ TRANSFORM IS BROKEN (identity or uninitialized)!\n";
        }
        
        std::cout << "\n";
    }
    
    std::cout << "=== CONCLUSION ===\n";
    std::cout << "Patches that don't have a clearly fixed dimension (edge/corner patches)\n";
    std::cout << "are getting WRONG transforms! This creates geometry in the wrong place,\n";
    std::cout << "causing the 'double planet' and black hole artifacts we're seeing.\n";
    
    return 0;
}