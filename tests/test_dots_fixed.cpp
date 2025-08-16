#include <iostream>
#include <iomanip>
#include <cmath>
#include <glm/glm.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

int main() {
    std::cout << "=== DOT ARTIFACT TEST ===" << std::endl;
    std::cout << "Testing if boundary dots are fixed with INSET..." << std::endl;
    
    // Create components
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Simulate view from distance
    glm::vec3 viewPos(15000000.0f, 5000000.0f, 5000000.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    
    // Update quadtree
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    std::cout << "\nAnalyzing " << patches.size() << " patches..." << std::endl;
    
    // Check for boundary overlaps
    int exactBoundaryCount = 0;
    int insetBoundaryCount = 0;
    
    for (const auto& patch : patches) {
        // Check Y boundaries  
        if (std::abs(std::abs(patch.minBounds.y) - 1.0) < 0.001 || 
            std::abs(std::abs(patch.maxBounds.y) - 1.0) < 0.001) {
            exactBoundaryCount++;
        }
        if (std::abs(std::abs(patch.minBounds.y) - 0.9995) < 0.0001 || 
            std::abs(std::abs(patch.maxBounds.y) - 0.9995) < 0.0001) {
            insetBoundaryCount++;
        }
        
        // Check Z boundaries
        if (std::abs(std::abs(patch.minBounds.z) - 1.0) < 0.001 || 
            std::abs(std::abs(patch.maxBounds.z) - 1.0) < 0.001) {
            exactBoundaryCount++;
        }
        if (std::abs(std::abs(patch.minBounds.z) - 0.9995) < 0.0001 || 
            std::abs(std::abs(patch.maxBounds.z) - 0.9995) < 0.0001) {
            insetBoundaryCount++;
        }
    }
    
    std::cout << "\nResults:" << std::endl;
    std::cout << "  Patches at exact ±1.0 boundary: " << exactBoundaryCount << std::endl;
    std::cout << "  Patches with INSET (0.9995): " << insetBoundaryCount << std::endl;
    
    // Generate vertices for boundary patches to check for gaps
    rendering::CPUVertexGenerator::Config vertexConfig;
    vertexConfig.planetRadius = 6371000.0f;
    vertexConfig.gridResolution = 65;
    vertexConfig.enableVertexCaching = false;  // Disabled for testing
    
    rendering::CPUVertexGenerator generator(vertexConfig);
    
    // Find two patches from different faces that share a boundary
    const core::QuadtreePatch* face0Patch = nullptr;
    const core::QuadtreePatch* face2Patch = nullptr;
    
    for (const auto& patch : patches) {
        if (patch.faceId == 0 && patch.maxBounds.y > 0.99) {
            face0Patch = &patch;
        }
        if (patch.faceId == 2 && patch.maxBounds.x > 0.99) {
            face2Patch = &patch;
        }
        if (face0Patch && face2Patch) break;
    }
    
    if (face0Patch && face2Patch) {
        std::cout << "\nChecking boundary between Face 0 and Face 2..." << std::endl;
        
        // Generate vertices
        auto mesh0 = generator.generatePatchMesh(*face0Patch, face0Patch->patchTransform);
        auto mesh2 = generator.generatePatchMesh(*face2Patch, face2Patch->patchTransform);
        
        // Find closest vertices at boundary
        float minDistance = 1e10f;
        float maxDistance = 0.0f;
        int closeVertexPairs = 0;
        
        for (const auto& v0 : mesh0.vertices) {
            // Only check boundary vertices (Y near 1.0 for face 0)
            if (std::abs(v0.position.y / 6371000.0f - 1.0) > 0.01) continue;
            
            for (const auto& v2 : mesh2.vertices) {
                // Only check boundary vertices (X near 1.0 for face 2)
                if (std::abs(v2.position.x / 6371000.0f - 1.0) > 0.01) continue;
                
                float dist = glm::length(v0.position - v2.position);
                if (dist < 100.0f) {  // Within 100 meters
                    closeVertexPairs++;
                    minDistance = std::min(minDistance, dist);
                    maxDistance = std::max(maxDistance, dist);
                }
            }
        }
        
        std::cout << "  Close vertex pairs (<100m): " << closeVertexPairs << std::endl;
        if (closeVertexPairs > 0) {
            std::cout << "  Min distance: " << minDistance << " meters" << std::endl;
            std::cout << "  Max distance: " << maxDistance << " meters" << std::endl;
        }
    }
    
    std::cout << "\n=== VERDICT ===" << std::endl;
    if (exactBoundaryCount > insetBoundaryCount) {
        std::cout << "❌ DOTS LIKELY STILL PRESENT - patches extend to exact boundaries" << std::endl;
        std::cout << "   Z-fighting will occur between adjacent cube faces!" << std::endl;
    } else if (insetBoundaryCount > exactBoundaryCount) {
        std::cout << "✓ DOTS SHOULD BE FIXED - patches are properly inset from boundaries" << std::endl;
        std::cout << "   Adjacent faces should no longer overlap!" << std::endl;
    } else {
        std::cout << "⚠ INCONCLUSIVE - mixed results, needs visual verification" << std::endl;
    }
    
    return (exactBoundaryCount > insetBoundaryCount) ? 1 : 0;
}