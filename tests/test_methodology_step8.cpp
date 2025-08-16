// STEP 8: Debug the exact vertex ordering issue
// The transforms work, but CPUVertexGenerator produces gaps. WHY?

#include <iostream>
#include <iomanip>
#include "rendering/cpu_vertex_generator.hpp"
#include "core/global_patch_generator.hpp"
#include "core/spherical_quadtree.hpp"

int main() {
    std::cout << "=== DEBUGGING VERTEX ORDERING ===\n\n";
    
    rendering::CPUVertexGenerator::Config config;
    config.planetRadius = 6371000.0;
    config.gridResolution = 3;  // Just 3x3 for debugging
    config.enableSkirts = false;
    config.enableVertexCaching = false;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create simple adjacent patches on the SAME face first
    // This should definitely work
    
    // Patch 1: Left half of +X face
    core::GlobalPatchGenerator::GlobalPatch patch1;
    patch1.minBounds = glm::dvec3(1.0, -1.0, -0.5);
    patch1.maxBounds = glm::dvec3(1.0, 1.0, 0.0);
    patch1.center = glm::dvec3(1.0, 0.0, -0.25);
    patch1.level = 1;
    patch1.faceId = 0;
    
    // Patch 2: Right half of +X face
    core::GlobalPatchGenerator::GlobalPatch patch2;
    patch2.minBounds = glm::dvec3(1.0, -1.0, 0.0);
    patch2.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    patch2.center = glm::dvec3(1.0, 0.0, 0.25);
    patch2.level = 1;
    patch2.faceId = 0;
    
    std::cout << "Two patches on SAME FACE (+X), sharing edge at Z=0\n";
    std::cout << "Patch 1: Z from -0.5 to 0.0\n";
    std::cout << "Patch 2: Z from 0.0 to 0.5\n\n";
    
    // Generate transforms
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    // Print transforms for debugging
    std::cout << "Transform 1 (maps UV to cube):\n";
    std::cout << "  Origin: (" << transform1[3][0] << ", " << transform1[3][1] << ", " << transform1[3][2] << ")\n";
    std::cout << "  U-axis: (" << transform1[0][0] << ", " << transform1[0][1] << ", " << transform1[0][2] << ")\n";
    std::cout << "  V-axis: (" << transform1[1][0] << ", " << transform1[1][1] << ", " << transform1[1][2] << ")\n\n";
    
    std::cout << "Transform 2 (maps UV to cube):\n";
    std::cout << "  Origin: (" << transform2[3][0] << ", " << transform2[3][1] << ", " << transform2[3][2] << ")\n";
    std::cout << "  U-axis: (" << transform2[0][0] << ", " << transform2[0][1] << ", " << transform2[0][2] << ")\n";
    std::cout << "  V-axis: (" << transform2[1][0] << ", " << transform2[1][1] << ", " << transform2[1][2] << ")\n\n";
    
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
    auto mesh1 = generator.generatePatchMesh(quad1, transform1);
    auto mesh2 = generator.generatePatchMesh(quad2, transform2);
    
    std::cout << "Generated " << mesh1.vertices.size() << " vertices for each patch\n\n";
    
    // The shared edge is at Z=0
    // For patch 1: This is the RIGHT edge (U=1)
    // For patch 2: This is the LEFT edge (U=0)
    
    std::cout << "=== PATCH 1 VERTICES (should have Z=-0.5 to 0) ===\n";
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            int idx = y * 3 + x;
            const auto& v = mesh1.vertices[idx];
            std::cout << "  [" << x << "," << y << "]: pos=(" 
                      << std::fixed << std::setprecision(2)
                      << v.position.x << ", " << v.position.y << ", " << v.position.z << ")";
            
            // Calculate what cube position this SHOULD be
            double u = x / 2.0;
            double v_coord = y / 2.0;
            glm::dvec3 expectedCube = glm::dvec3(transform1 * glm::dvec4(u, v_coord, 0, 1));
            
            // Back-calculate to sphere to compare
            glm::dvec3 pos2 = expectedCube * expectedCube;
            glm::dvec3 spherePos;
            spherePos.x = expectedCube.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
            spherePos.y = expectedCube.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
            spherePos.z = expectedCube.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
            glm::vec3 expectedSphere = glm::vec3(glm::normalize(spherePos) * static_cast<double>(config.planetRadius));
            
            float error = glm::length(v.position - expectedSphere);
            if (error > 1.0f) {
                std::cout << " ERROR: " << error << " meters off!";
            }
            std::cout << "\n";
        }
    }
    
    std::cout << "\n=== PATCH 2 VERTICES (should have Z=0 to 0.5) ===\n";
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            int idx = y * 3 + x;
            const auto& v = mesh2.vertices[idx];
            std::cout << "  [" << x << "," << y << "]: pos=(" 
                      << std::fixed << std::setprecision(2)
                      << v.position.x << ", " << v.position.y << ", " << v.position.z << ")\n";
        }
    }
    
    std::cout << "\n=== CHECKING SHARED EDGE (Z=0) ===\n";
    std::cout << "Patch 1 right edge (x=2):\n";
    for (int y = 0; y < 3; y++) {
        int idx = y * 3 + 2;  // x=2 (right edge)
        const auto& v = mesh1.vertices[idx];
        std::cout << "  Y=" << y << ": (" << v.position.x << ", " << v.position.y << ", " << v.position.z << ")\n";
    }
    
    std::cout << "\nPatch 2 left edge (x=0):\n";
    for (int y = 0; y < 3; y++) {
        int idx = y * 3 + 0;  // x=0 (left edge)
        const auto& v = mesh2.vertices[idx];
        std::cout << "  Y=" << y << ": (" << v.position.x << ", " << v.position.y << ", " << v.position.z << ")\n";
    }
    
    std::cout << "\n=== GAP ANALYSIS ===\n";
    float maxGap = 0.0f;
    for (int y = 0; y < 3; y++) {
        int idx1 = y * 3 + 2;  // Patch 1 right edge
        int idx2 = y * 3 + 0;  // Patch 2 left edge
        
        float gap = glm::length(mesh1.vertices[idx1].position - mesh2.vertices[idx2].position);
        std::cout << "  Y=" << y << ": gap = " << gap << " meters";
        
        if (gap > 1.0f) {
            std::cout << " ✗ LARGE GAP!";
            maxGap = std::max(maxGap, gap);
        } else {
            std::cout << " ✓";
        }
        std::cout << "\n";
    }
    
    std::cout << "\nMaximum gap: " << maxGap << " meters\n";
    
    if (maxGap > 1.0f) {
        std::cout << "\n✗ PROBLEM FOUND: Even same-face patches have gaps!\n";
        std::cout << "The issue is in CPUVertexGenerator itself.\n";
    } else {
        std::cout << "\n✓ Same-face patches work correctly.\n";
        std::cout << "Next: Test cross-face patches.\n";
    }
    
    return 0;
}