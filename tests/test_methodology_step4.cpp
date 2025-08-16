// STEP 4: Test patches from DIFFERENT faces
// This is where we expect to find the problem

#include <iostream>
#include <iomanip>
#include "rendering/cpu_vertex_generator.hpp"
#include "core/global_patch_generator.hpp"
#include "core/spherical_quadtree.hpp"

int main() {
    std::cout << "=== TESTING CROSS-FACE BOUNDARIES ===\n\n";
    
    rendering::CPUVertexGenerator::Config config;
    config.planetRadius = 6371000.0;
    config.gridResolution = 5;
    config.enableSkirts = false;
    config.enableVertexCaching = false;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create patches at the boundary between +X and +Y faces
    // These SHOULD share an edge at X=1, Y=1
    
    // Patch 1: +X face, top edge
    core::GlobalPatchGenerator::GlobalPatch patch1;
    patch1.minBounds = glm::dvec3(1.0, 0.5, -0.5);
    patch1.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    patch1.center = glm::dvec3(1.0, 0.75, 0.0);
    patch1.level = 2;
    patch1.faceId = 0; // +X face
    
    // Patch 2: +Y face, right edge  
    core::GlobalPatchGenerator::GlobalPatch patch2;
    patch2.minBounds = glm::dvec3(0.5, 1.0, -0.5);
    patch2.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    patch2.center = glm::dvec3(0.75, 1.0, 0.0);
    patch2.level = 2;
    patch2.faceId = 2; // +Y face
    
    std::cout << "Patch 1 (+X face): bounds (" 
              << patch1.minBounds.x << "," << patch1.minBounds.y << "," << patch1.minBounds.z
              << ") to ("
              << patch1.maxBounds.x << "," << patch1.maxBounds.y << "," << patch1.maxBounds.z << ")\n";
    
    std::cout << "Patch 2 (+Y face): bounds (" 
              << patch2.minBounds.x << "," << patch2.minBounds.y << "," << patch2.minBounds.z
              << ") to ("
              << patch2.maxBounds.x << "," << patch2.maxBounds.y << "," << patch2.maxBounds.z << ")\n\n";
    
    // They share the edge from (1,1,-0.5) to (1,1,0.5)
    std::cout << "Shared edge should be from (1,1,-0.5) to (1,1,0.5)\n\n";
    
    // Generate transforms
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    // Test the transforms first
    std::cout << "=== TESTING TRANSFORMS ===\n";
    std::cout << "Patch 1 (+X face) UV corners:\n";
    for (int i = 0; i < 4; i++) {
        float u = (i % 2) ? 1.0f : 0.0f;
        float v = (i / 2) ? 1.0f : 0.0f;
        glm::dvec3 cubePos = glm::dvec3(transform1 * glm::dvec4(u, v, 0, 1));
        std::cout << "  UV(" << u << "," << v << ") -> cube(" 
                  << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")\n";
    }
    
    std::cout << "\nPatch 2 (+Y face) UV corners:\n";
    for (int i = 0; i < 4; i++) {
        float u = (i % 2) ? 1.0f : 0.0f;
        float v = (i / 2) ? 1.0f : 0.0f;
        glm::dvec3 cubePos = glm::dvec3(transform2 * glm::dvec4(u, v, 0, 1));
        std::cout << "  UV(" << u << "," << v << ") -> cube(" 
                  << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")\n";
    }
    
    // Create QuadtreePatch structures
    core::QuadtreePatch quad1;
    quad1.center = patch1.center;
    quad1.minBounds = patch1.minBounds;
    quad1.maxBounds = patch1.maxBounds;
    quad1.level = patch1.level;
    quad1.faceId = patch1.faceId;
    quad1.size = 0.5f;
    quad1.morphFactor = 0.0f;
    quad1.screenSpaceError = 0.0f;
    
    core::QuadtreePatch quad2;
    quad2.center = patch2.center;
    quad2.minBounds = patch2.minBounds;
    quad2.maxBounds = patch2.maxBounds;
    quad2.level = patch2.level;
    quad2.faceId = patch2.faceId;
    quad2.size = 0.5f;
    quad2.morphFactor = 0.0f;
    quad2.screenSpaceError = 0.0f;
    
    // Generate meshes
    std::cout << "\n=== GENERATING MESHES ===\n";
    auto mesh1 = generator.generatePatchMesh(quad1, transform1);
    auto mesh2 = generator.generatePatchMesh(quad2, transform2);
    
    std::cout << "Generated " << mesh1.vertices.size() << " and " << mesh2.vertices.size() << " vertices\n\n";
    
    // Check the shared edge
    std::cout << "=== CHECKING SHARED EDGE ===\n\n";
    
    int res = config.gridResolution;
    
    std::cout << "Patch 1 (+X) top edge (Y=1):\n";
    for (int x = 0; x < res; x++) {
        int idx = (res - 1) * res + x;  // Top row
        const auto& v = mesh1.vertices[idx];
        std::cout << "  [" << x << "]: (" 
                  << std::fixed << std::setprecision(2)
                  << v.position.x << ", " 
                  << v.position.y << ", " 
                  << v.position.z << ")\n";
    }
    
    std::cout << "\nPatch 2 (+Y) right edge (X=1):\n";
    for (int y = 0; y < res; y++) {
        int idx = y * res + (res - 1);  // Right column
        const auto& v = mesh2.vertices[idx];
        std::cout << "  [" << y << "]: (" 
                  << std::fixed << std::setprecision(2)
                  << v.position.x << ", " 
                  << v.position.y << ", " 
                  << v.position.z << ")\n";
    }
    
    // They should match (in reverse order perhaps)
    std::cout << "\n=== COMPARISON ===\n";
    float maxGap = 0.0f;
    for (int i = 0; i < res; i++) {
        int idx1 = (res - 1) * res + i;      // Patch 1 top edge
        int idx2 = (res - 1 - i) * res + (res - 1);  // Patch 2 right edge (reversed)
        
        glm::vec3 pos1 = mesh1.vertices[idx1].position;
        glm::vec3 pos2 = mesh2.vertices[idx2].position;
        float gap = glm::length(pos1 - pos2);
        
        std::cout << "  Point " << i << ": gap = " << gap << " meters";
        
        if (gap < 1.0f) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ LARGE GAP!\n";
            maxGap = std::max(maxGap, gap);
            
            // Show details for first mismatch
            if (maxGap == gap) {
                std::cout << "    P1: (" << pos1.x << ", " << pos1.y << ", " << pos1.z << ")\n";
                std::cout << "    P2: (" << pos2.x << ", " << pos2.y << ", " << pos2.z << ")\n";
            }
        }
    }
    
    std::cout << "\nMaximum gap: " << maxGap << " meters\n";
    
    if (maxGap > 1000.0f) {
        std::cout << "\n✗ FOUND THE BUG: Cross-face boundaries have HUGE gaps!\n";
        std::cout << "This is the 6 million meter gap problem.\n";
    } else if (maxGap > 1.0f) {
        std::cout << "\n✗ Cross-face boundaries have gaps, but smaller than expected.\n";
    } else {
        std::cout << "\n✓ Cross-face boundaries work correctly!\n";
    }
    
    return 0;
}