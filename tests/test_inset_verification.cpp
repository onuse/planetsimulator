#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"

int main() {
    std::cout << "=== INSET VERIFICATION TEST ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    
    // Create quadtree
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
    
    std::cout << "\nAnalyzing " << patches.size() << " patches..." << std::endl;
    
    // Check if patches respect INSET
    const double EXPECTED_INSET = 0.9995;
    int exactBoundaryCount = 0;
    int insetBoundaryCount = 0;
    int correctPatches = 0;
    int incorrectPatches = 0;
    
    for (const auto& patch : patches) {
        bool hasExactBoundary = false;
        bool hasInset = false;
        
        // Check each dimension
        for (int dim = 0; dim < 3; dim++) {
            double minVal = (dim == 0) ? patch.minBounds.x : (dim == 1) ? patch.minBounds.y : patch.minBounds.z;
            double maxVal = (dim == 0) ? patch.maxBounds.x : (dim == 1) ? patch.maxBounds.y : patch.maxBounds.z;
            
            // Is this the fixed dimension?
            bool isFixed = std::abs(minVal - maxVal) < 0.001;
            
            if (isFixed) {
                // Fixed dimension should be at exactly ±1.0
                if (std::abs(std::abs(minVal) - 1.0) < 0.001) {
                    // Good - fixed dimension at boundary
                } else {
                    std::cout << "ERROR: Fixed dimension not at ±1.0: " << minVal << std::endl;
                }
            } else {
                // Varying dimension should not exceed INSET
                if (std::abs(minVal) > EXPECTED_INSET + 0.0001 || 
                    std::abs(maxVal) > EXPECTED_INSET + 0.0001) {
                    hasExactBoundary = true;
                }
                if (std::abs(std::abs(minVal) - EXPECTED_INSET) < 0.0001 || 
                    std::abs(std::abs(maxVal) - EXPECTED_INSET) < 0.0001) {
                    hasInset = true;
                }
            }
        }
        
        if (hasExactBoundary) {
            exactBoundaryCount++;
            incorrectPatches++;
        }
        if (hasInset) {
            insetBoundaryCount++;
            correctPatches++;
        }
    }
    
    std::cout << "\n=== RESULTS ===" << std::endl;
    std::cout << "Patches with varying dimensions at ±1.0: " << exactBoundaryCount << std::endl;
    std::cout << "Patches with INSET at ±" << EXPECTED_INSET << ": " << insetBoundaryCount << std::endl;
    std::cout << "Correct patches: " << correctPatches << std::endl;
    std::cout << "Incorrect patches: " << incorrectPatches << std::endl;
    
    if (exactBoundaryCount == 0 && insetBoundaryCount > 0) {
        std::cout << "\n✓ SUCCESS: INSET is properly applied!" << std::endl;
        std::cout << "Z-fighting should be eliminated." << std::endl;
        return 0;
    } else if (exactBoundaryCount > 0) {
        std::cout << "\n✗ FAILURE: Some patches still extend to ±1.0" << std::endl;
        std::cout << "Z-fighting may still occur!" << std::endl;
        return 1;
    } else {
        std::cout << "\n? INCONCLUSIVE: Need more patches to verify" << std::endl;
        return 2;
    }
}