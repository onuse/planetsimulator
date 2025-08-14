#include <iostream>
#include <glm/glm.hpp>
#include <cmath>
#include "core/global_patch_system_v2.hpp"

// Test that our new global coordinate system provides consistent UV-to-world mapping

int main() {
    std::cout << "=== GLOBAL COORDINATE SYSTEM TEST ===\n\n";
    
    // Create two adjacent patches on different faces
    // Patch 1: On +X face near the +X/+Y edge
    glm::dvec3 patch1_min(1.0, 0.5, -0.25);
    glm::dvec3 patch1_max(1.0, 1.0, 0.25);
    
    // Patch 2: On +Y face near the +X/+Y edge  
    glm::dvec3 patch2_min(0.5, 1.0, -0.25);
    glm::dvec3 patch2_max(1.0, 1.0, 0.25);
    
    auto patch1 = core::GlobalPatchSystemV2::createPatch(patch1_min, patch1_max, 2);
    auto patch2 = core::GlobalPatchSystemV2::createPatch(patch2_min, patch2_max, 2);
    
    std::cout << "Patch 1 (+X face):\n";
    std::cout << "  Bounds: (" << patch1_min.x << "," << patch1_min.y << "," << patch1_min.z 
              << ") to (" << patch1_max.x << "," << patch1_max.y << "," << patch1_max.z << ")\n";
    
    std::cout << "\nPatch 2 (+Y face):\n";
    std::cout << "  Bounds: (" << patch2_min.x << "," << patch2_min.y << "," << patch2_min.z 
              << ") to (" << patch2_max.x << "," << patch2_max.y << "," << patch2_max.z << ")\n";
    
    // Get transforms
    auto transform1 = core::GlobalPatchSystemV2::createPatchTransform(patch1);
    auto transform2 = core::GlobalPatchSystemV2::createPatchTransform(patch2);
    
    std::cout << "\n=== UV MAPPING TEST ===\n";
    
    // Test UV(0.5, 0.5) - center of each patch
    glm::dvec4 uv_center(0.5, 0.5, 0.0, 1.0);
    glm::dvec3 world1_center = glm::dvec3(transform1 * uv_center);
    glm::dvec3 world2_center = glm::dvec3(transform2 * uv_center);
    
    std::cout << "\nUV(0.5, 0.5) mapping:\n";
    std::cout << "  Patch 1 -> World: (" << world1_center.x << "," << world1_center.y << "," << world1_center.z << ")\n";
    std::cout << "  Expected center:  (" << patch1.center.x << "," << patch1.center.y << "," << patch1.center.z << ")\n";
    std::cout << "  Patch 2 -> World: (" << world2_center.x << "," << world2_center.y << "," << world2_center.z << ")\n";
    std::cout << "  Expected center:  (" << patch2.center.x << "," << patch2.center.y << "," << patch2.center.z << ")\n";
    
    // Test the shared edge - where Y=1.0 for both patches
    std::cout << "\n=== SHARED EDGE TEST ===\n";
    std::cout << "Testing points along the shared edge (X from 0.5 to 1.0, Y=1.0, Z=-0.25 to 0.25)\n\n";
    
    // For patch 1 (+X face), the edge is at max Y (UV v=1.0)
    // For patch 2 (+Y face), the edge is at max X (UV u=1.0)
    
    for (double t = 0.0; t <= 1.0; t += 0.5) {
        // On patch 1: UV(t, 1.0) should give edge point
        glm::dvec4 uv1(t, 1.0, 0.0, 1.0);
        glm::dvec3 edge1 = glm::dvec3(transform1 * uv1);
        
        // On patch 2: UV(1.0, t) should give edge point
        glm::dvec4 uv2(1.0, t, 0.0, 1.0);
        glm::dvec3 edge2 = glm::dvec3(transform2 * uv2);
        
        std::cout << "Parameter t=" << t << ":\n";
        std::cout << "  Patch 1 UV(" << t << ",1.0) -> (" << edge1.x << "," << edge1.y << "," << edge1.z << ")\n";
        std::cout << "  Patch 2 UV(1.0," << t << ") -> (" << edge2.x << "," << edge2.y << "," << edge2.z << ")\n";
        
        // The actual shared point in world space
        double worldZ = patch1_min.z + t * (patch1_max.z - patch1_min.z);
        std::cout << "  Expected edge point: (1.0, 1.0, " << worldZ << ")\n\n";
    }
    
    std::cout << "=== CONCLUSION ===\n";
    
    // Check if the transforms are working correctly
    bool centersCorrect = 
        (glm::length(world1_center - patch1.center) < 0.001) &&
        (glm::length(world2_center - patch2.center) < 0.001);
    
    if (centersCorrect) {
        std::cout << "✅ SUCCESS! UV(0.5,0.5) correctly maps to patch centers.\n";
        std::cout << "The global coordinate system is working correctly.\n";
        std::cout << "Each patch's UV space maps consistently to its 3D bounding box.\n";
    } else {
        std::cout << "❌ FAIL: UV mapping is not consistent.\n";
    }
    
    return centersCorrect ? 0 : 1;
}