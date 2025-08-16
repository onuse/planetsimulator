#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "core/global_patch_generator.hpp"

void analyzePatchBounds(const core::QuadtreePatch& patch, const std::string& label) {
    std::cout << "\n=== " << label << " ===" << std::endl;
    std::cout << "Level: " << patch.level << ", FaceId: " << patch.faceId << std::endl;
    std::cout << "Center: (" << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << ")" << std::endl;
    std::cout << "MinBounds: (" << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z << ")" << std::endl;
    std::cout << "MaxBounds: (" << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z << ")" << std::endl;
    
    // Check which dimensions are at boundaries
    for (int i = 0; i < 3; i++) {
        const char* dim = (i == 0 ? "X" : (i == 1 ? "Y" : "Z"));
        double minVal = (i == 0 ? patch.minBounds.x : (i == 1 ? patch.minBounds.y : patch.minBounds.z));
        double maxVal = (i == 0 ? patch.maxBounds.x : (i == 1 ? patch.maxBounds.y : patch.maxBounds.z));
        
        std::cout << dim << " dimension: [" << minVal << ", " << maxVal << "]";
        
        // Check if fixed
        if (std::abs(minVal - maxVal) < 0.001) {
            std::cout << " -> FIXED at " << minVal;
            if (std::abs(std::abs(minVal) - 1.0) < 0.001) {
                std::cout << " (AT BOUNDARY ±1.0)";
            }
        } else {
            std::cout << " -> VARYING";
            // Check if reaches boundaries
            if (std::abs(std::abs(minVal) - 1.0) < 0.01) {
                std::cout << " (min at boundary)";
            }
            if (std::abs(std::abs(maxVal) - 1.0) < 0.01) {
                std::cout << " (max at boundary)";
            }
            // Check for INSET
            if (std::abs(std::abs(minVal) - 0.9995) < 0.0001) {
                std::cout << " (min has INSET!)";
            }
            if (std::abs(std::abs(maxVal) - 0.9995) < 0.0001) {
                std::cout << " (max has INSET!)";
            }
        }
        std::cout << std::endl;
    }
    
    // Check corners
    std::cout << "Corners:" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: (" << patch.corners[i].x << ", " 
                  << patch.corners[i].y << ", " << patch.corners[i].z << ")" << std::endl;
    }
}

int main() {
    std::cout << "=== PATCH BOUNDS DIAGNOSTIC TEST ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    
    // Test 1: Check GlobalPatchGenerator
    std::cout << "\n--- Testing GlobalPatchGenerator ---" << std::endl;
    auto globalRoots = core::GlobalPatchGenerator::createRootPatches();
    std::cout << "Created " << globalRoots.size() << " global root patches" << std::endl;
    
    for (size_t i = 0; i < globalRoots.size() && i < 2; i++) {
        auto& root = globalRoots[i];
        std::cout << "\nGlobal Face " << i << ":" << std::endl;
        std::cout << "  MinBounds: (" << root.minBounds.x << ", " << root.minBounds.y << ", " << root.minBounds.z << ")" << std::endl;
        std::cout << "  MaxBounds: (" << root.maxBounds.x << ", " << root.maxBounds.y << ", " << root.maxBounds.z << ")" << std::endl;
    }
    
    // Test 2: Check SphericalQuadtree
    std::cout << "\n--- Testing SphericalQuadtree ---" << std::endl;
    
    // Create density field
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    
    // Create quadtree
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Get patches
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    
    std::vector<core::QuadtreePatch> patches;
    quadtree.update(viewPos, viewProj, 0.016f);
    patches = quadtree.getVisiblePatches();
    
    std::cout << "\nGot " << patches.size() << " visible patches" << std::endl;
    
    // Analyze root-level patches
    int rootCount = 0;
    int level1Count = 0;
    for (const auto& patch : patches) {
        if (patch.level == 0 && rootCount < 2) {
            analyzePatchBounds(patch, "Quadtree Root Face " + std::to_string(patch.faceId));
            rootCount++;
        } else if (patch.level == 1 && level1Count < 2) {
            analyzePatchBounds(patch, "Quadtree Level 1 Patch");
            level1Count++;
        }
    }
    
    // Test 3: Check for overlaps at boundaries
    std::cout << "\n--- Checking for Boundary Overlaps ---" << std::endl;
    
    // Find patches at X=1 boundary (Face 0 patches)
    std::vector<const core::QuadtreePatch*> face0Patches;
    std::vector<const core::QuadtreePatch*> face2Patches;
    
    for (const auto& patch : patches) {
        if (patch.faceId == 0) {
            face0Patches.push_back(&patch);
        } else if (patch.faceId == 2) {
            face2Patches.push_back(&patch);
        }
    }
    
    std::cout << "Face 0 patches: " << face0Patches.size() << std::endl;
    std::cout << "Face 2 patches: " << face2Patches.size() << std::endl;
    
    // Check if Face 0 patches extend to exactly 1.0 in Y/Z
    bool hasExactBoundary = false;
    bool hasInset = false;
    
    for (const auto* patch : face0Patches) {
        // Check Y boundaries
        if (std::abs(patch->minBounds.y + 1.0) < 0.001 || std::abs(patch->maxBounds.y - 1.0) < 0.001) {
            hasExactBoundary = true;
            std::cout << "Face 0 patch reaches Y=±1.0 exactly!" << std::endl;
        }
        if (std::abs(std::abs(patch->minBounds.y) - 0.9995) < 0.0001 || 
            std::abs(std::abs(patch->maxBounds.y) - 0.9995) < 0.0001) {
            hasInset = true;
            std::cout << "Face 0 patch has INSET at Y!" << std::endl;
        }
        
        // Check Z boundaries
        if (std::abs(patch->minBounds.z + 1.0) < 0.001 || std::abs(patch->maxBounds.z - 1.0) < 0.001) {
            hasExactBoundary = true;
            std::cout << "Face 0 patch reaches Z=±1.0 exactly!" << std::endl;
        }
        if (std::abs(std::abs(patch->minBounds.z) - 0.9995) < 0.0001 || 
            std::abs(std::abs(patch->maxBounds.z) - 0.9995) < 0.0001) {
            hasInset = true;
            std::cout << "Face 0 patch has INSET at Z!" << std::endl;
        }
    }
    
    std::cout << "\n=== DIAGNOSIS ===" << std::endl;
    if (hasExactBoundary && !hasInset) {
        std::cout << "❌ PROBLEM: Patches extend to exactly ±1.0 without INSET" << std::endl;
        std::cout << "This will cause z-fighting between adjacent faces!" << std::endl;
    } else if (hasInset && !hasExactBoundary) {
        std::cout << "✓ GOOD: Patches are properly inset from boundaries" << std::endl;
    } else if (hasExactBoundary && hasInset) {
        std::cout << "⚠ MIXED: Some patches have inset, some don't" << std::endl;
    } else {
        std::cout << "? UNCLEAR: Need more patches to diagnose" << std::endl;
    }
    
    return 0;
}