#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"

int main() {
    std::cout << "=== TESTING ACTUAL PATCH GENERATION ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Create a quadtree like the real program does
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Get patches
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    std::cout << "Generated " << patches.size() << " patches" << std::endl;
    
    // Check for problematic patches
    int problemCount = 0;
    for (size_t i = 0; i < patches.size() && i < 10; i++) {
        const auto& patch = patches[i];
        
        // Check if bounds are degenerate
        glm::dvec3 range = patch.maxBounds - patch.minBounds;
        if (std::abs(range.x) < 1e-10 && std::abs(range.y) < 1e-10 && std::abs(range.z) < 1e-10) {
            std::cout << "\nPROBLEM PATCH " << i << ":" << std::endl;
            std::cout << "  Level: " << patch.level << ", Face: " << patch.faceId << std::endl;
            std::cout << "  MinBounds: (" << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z << ")" << std::endl;
            std::cout << "  MaxBounds: (" << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z << ")" << std::endl;
            std::cout << "  Range: (" << range.x << ", " << range.y << ", " << range.z << ")" << std::endl;
            problemCount++;
        }
    }
    
    if (problemCount == 0) {
        std::cout << "No degenerate patches found in first 10 patches" << std::endl;
    } else {
        std::cout << "\nFound " << problemCount << " degenerate patches!" << std::endl;
    }
    
    return 0;
}
