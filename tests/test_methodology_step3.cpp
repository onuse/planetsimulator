// STEP 3: Test the ACTUAL production code path
// Let's use the real CPUVertexGenerator and see what happens

#include <iostream>
#include <iomanip>
#include "rendering/cpu_vertex_generator.hpp"
#include "core/global_patch_generator.hpp"
#include "core/spherical_quadtree.hpp"

int main() {
    std::cout << "=== TESTING ACTUAL PRODUCTION CODE ===\n\n";
    
    // Set up the vertex generator with minimal config
    rendering::CPUVertexGenerator::Config config;
    config.planetRadius = 6371000.0;
    config.gridResolution = 5;  // Very small for debugging
    config.enableSkirts = false;
    config.enableVertexCaching = false;  // Disable to see raw output
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create two patches that SHOULD share an edge
    // Patch 1: +X face, bottom half
    core::GlobalPatchGenerator::GlobalPatch patch1;
    patch1.minBounds = glm::dvec3(1.0, -1.0, -1.0);
    patch1.maxBounds = glm::dvec3(1.0, 0.0, 1.0);
    patch1.center = glm::dvec3(1.0, -0.5, 0.0);
    patch1.level = 1;
    patch1.faceId = 0; // +X face
    
    // Patch 2: +X face, top half (shares edge at Y=0)
    core::GlobalPatchGenerator::GlobalPatch patch2;
    patch2.minBounds = glm::dvec3(1.0, 0.0, -1.0);
    patch2.maxBounds = glm::dvec3(1.0, 1.0, 1.0);
    patch2.center = glm::dvec3(1.0, 0.5, 0.0);
    patch2.level = 1;
    patch2.faceId = 0; // Same face
    
    // Generate transforms
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    // Create QuadtreePatch structures
    core::QuadtreePatch quad1;
    quad1.center = patch1.center;
    quad1.minBounds = patch1.minBounds;
    quad1.maxBounds = patch1.maxBounds;
    quad1.level = patch1.level;
    quad1.faceId = patch1.faceId;
    quad1.size = 1.0f;
    quad1.morphFactor = 0.0f;
    quad1.screenSpaceError = 0.0f;
    
    core::QuadtreePatch quad2;
    quad2.center = patch2.center;
    quad2.minBounds = patch2.minBounds;
    quad2.maxBounds = patch2.maxBounds;
    quad2.level = patch2.level;
    quad2.faceId = patch2.faceId;
    quad2.size = 1.0f;
    quad2.morphFactor = 0.0f;
    quad2.screenSpaceError = 0.0f;
    
    // Generate meshes
    std::cout << "Generating meshes...\n";
    auto mesh1 = generator.generatePatchMesh(quad1, transform1);
    auto mesh2 = generator.generatePatchMesh(quad2, transform2);
    
    std::cout << "Patch 1: " << mesh1.vertices.size() << " vertices\n";
    std::cout << "Patch 2: " << mesh2.vertices.size() << " vertices\n\n";
    
    // Check the shared edge (Y=0)
    std::cout << "=== CHECKING SHARED EDGE ===\n\n";
    std::cout << "Patch 1 top edge (should be at Y=0):\n";
    
    // Top row of patch 1 is the last row
    int res = config.gridResolution;
    for (int x = 0; x < res; x++) {
        int idx = (res - 1) * res + x;
        const auto& v = mesh1.vertices[idx];
        std::cout << "  [" << x << "]: pos=(" 
                  << std::fixed << std::setprecision(2)
                  << v.position.x << ", " 
                  << v.position.y << ", " 
                  << v.position.z << ") "
                  << "dist=" << glm::length(v.position) << "\n";
    }
    
    std::cout << "\nPatch 2 bottom edge (should be at Y=0):\n";
    
    // Bottom row of patch 2 is the first row
    for (int x = 0; x < res; x++) {
        int idx = x;
        const auto& v = mesh2.vertices[idx];
        std::cout << "  [" << x << "]: pos=(" 
                  << std::fixed << std::setprecision(2)
                  << v.position.x << ", " 
                  << v.position.y << ", " 
                  << v.position.z << ") "
                  << "dist=" << glm::length(v.position) << "\n";
    }
    
    // Compare them
    std::cout << "\n=== COMPARISON ===\n";
    float maxGap = 0.0f;
    for (int x = 0; x < res; x++) {
        int idx1 = (res - 1) * res + x;
        int idx2 = x;
        
        glm::vec3 pos1 = mesh1.vertices[idx1].position;
        glm::vec3 pos2 = mesh2.vertices[idx2].position;
        float gap = glm::length(pos1 - pos2);
        
        std::cout << "  Point " << x << ": gap = " << gap << " meters";
        
        if (gap < 1.0f) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ LARGE GAP!\n";
            maxGap = std::max(maxGap, gap);
        }
    }
    
    std::cout << "\nMaximum gap: " << maxGap << " meters\n";
    
    if (maxGap < 1.0f) {
        std::cout << "\n✓ PRODUCTION CODE WORKS for same-face patches\n";
        std::cout << "NEXT: Test patches from different faces\n";
    } else {
        std::cout << "\n✗ PRODUCTION CODE HAS GAPS even on same face!\n";
        std::cout << "THIS IS THE BUG - patches on same face don't align\n";
    }
    
    return 0;
}