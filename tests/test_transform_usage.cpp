#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

// Test if patches are using their transforms correctly

void printTransform(const glm::dmat4& transform) {
    std::cout << "  Transform:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "    [" << i << "]: ";
        for (int j = 0; j < 4; j++) {
            std::cout << transform[j][i] << " ";
        }
        std::cout << "\n";
    }
}

int main() {
    std::cout << "=== Testing Transform Usage ===\n\n";
    
    // Create root patches
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    // Test each face's transform
    for (int face = 0; face < 6; face++) {
        auto& patch = roots[face];
        std::cout << "Face " << face << " (";
        switch(face) {
            case 0: std::cout << "+X"; break;
            case 1: std::cout << "-X"; break;
            case 2: std::cout << "+Y"; break;
            case 3: std::cout << "-Y"; break;
            case 4: std::cout << "+Z"; break;
            case 5: std::cout << "-Z"; break;
        }
        std::cout << "):\n";
        std::cout << "  Bounds: (" << patch.minBounds.x << "," << patch.minBounds.y << "," << patch.minBounds.z 
                  << ") to (" << patch.maxBounds.x << "," << patch.maxBounds.y << "," << patch.maxBounds.z << ")\n";
        
        auto transform = patch.createTransform();
        printTransform(transform);
        
        // Test UV corners to see where they map
        std::cout << "  UV mapping test:\n";
        glm::dvec4 corners[4] = {
            glm::dvec4(0, 0, 0, 1),  // UV(0,0)
            glm::dvec4(1, 0, 0, 1),  // UV(1,0)
            glm::dvec4(1, 1, 0, 1),  // UV(1,1)
            glm::dvec4(0, 1, 0, 1)   // UV(0,1)
        };
        
        for (int i = 0; i < 4; i++) {
            glm::dvec3 worldPos = glm::dvec3(transform * corners[i]);
            std::cout << "    UV(" << corners[i].x << "," << corners[i].y << ") -> ("
                      << worldPos.x << "," << worldPos.y << "," << worldPos.z << ")\n";
        }
        
        // Check if any coordinates are outside [-1, 1] range
        bool hasError = false;
        for (int i = 0; i < 4; i++) {
            glm::dvec3 worldPos = glm::dvec3(transform * corners[i]);
            if (std::abs(worldPos.x) > 1.001 || std::abs(worldPos.y) > 1.001 || std::abs(worldPos.z) > 1.001) {
                std::cout << "  ✗ ERROR: Corner " << i << " is outside cube bounds!\n";
                hasError = true;
            }
        }
        if (!hasError) {
            std::cout << "  ✓ All corners within cube bounds\n";
        }
        
        std::cout << "\n";
    }
    
    // Now test a problematic scenario: patches near face boundaries
    std::cout << "=== Testing Boundary Patches ===\n\n";
    
    // Create a patch that's at the boundary between +X and +Z faces
    core::GlobalPatchGenerator::GlobalPatch boundaryPatch;
    boundaryPatch.minBounds = glm::dvec3(0.5, -0.5, 0.5);
    boundaryPatch.maxBounds = glm::dvec3(1.0, 0.5, 1.0);
    boundaryPatch.center = (boundaryPatch.minBounds + boundaryPatch.maxBounds) * 0.5;
    boundaryPatch.level = 1;
    
    // Test with both face IDs to see if there's a difference
    for (int testFace : {0, 4}) {  // +X and +Z
        boundaryPatch.faceId = testFace;
        
        std::cout << "Boundary patch with faceId=" << testFace << ":\n";
        std::cout << "  Center: (" << boundaryPatch.center.x << "," << boundaryPatch.center.y << "," << boundaryPatch.center.z << ")\n";
        
        auto transform = boundaryPatch.createTransform();
        
        // Test specific UV points
        glm::dvec4 testPoints[] = {
            glm::dvec4(0.5, 0.5, 0, 1),  // Center
            glm::dvec4(1.0, 0.5, 0, 1),  // Right edge
            glm::dvec4(0.5, 1.0, 0, 1),  // Top edge
            glm::dvec4(1.0, 1.0, 0, 1)   // Corner
        };
        
        for (auto& uv : testPoints) {
            glm::dvec3 worldPos = glm::dvec3(transform * uv);
            std::cout << "  UV(" << uv.x << "," << uv.y << ") -> ("
                      << worldPos.x << "," << worldPos.y << "," << worldPos.z << ")";
            
            // Check if this is where we expect
            if (std::abs(worldPos.x) > 1.001 || std::abs(worldPos.y) > 1.001 || std::abs(worldPos.z) > 1.001) {
                std::cout << " ✗ OUTSIDE CUBE!";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}