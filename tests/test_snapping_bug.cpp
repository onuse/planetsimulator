#include <iostream>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

// Test that demonstrates the snapping bug

int main() {
    std::cout << "=== Testing Boundary Snapping Bug ===\n\n";
    
    // Create root patches
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    // Test subdivision of the +X face
    auto& xFace = roots[0];  // +X face
    std::cout << "+X Face root bounds: (" << xFace.minBounds.x << "," << xFace.minBounds.y << "," << xFace.minBounds.z 
              << ") to (" << xFace.maxBounds.x << "," << xFace.maxBounds.y << "," << xFace.maxBounds.z << ")\n";
    
    auto children = core::GlobalPatchGenerator::subdivide(xFace);
    
    std::cout << "\nChildren of +X face:\n";
    for (size_t i = 0; i < children.size(); i++) {
        auto& child = children[i];
        std::cout << "  Child " << i << ": (" 
                  << child.minBounds.x << "," << child.minBounds.y << "," << child.minBounds.z 
                  << ") to (" 
                  << child.maxBounds.x << "," << child.maxBounds.y << "," << child.maxBounds.z << ")\n";
        
        // Check if X is properly fixed at 1.0
        if (std::abs(child.minBounds.x - 1.0) > 1e-6 || std::abs(child.maxBounds.x - 1.0) > 1e-6) {
            std::cout << "    ERROR: X coordinate not fixed at 1.0!\n";
        }
        
        // Check if any Y or Z coordinates are incorrectly at exactly ±1.0 when they shouldn't be
        bool hasInternalBoundary = false;
        
        // For the +X face, internal boundaries should be at 0, not ±1
        if (std::abs(child.minBounds.y) == 1.0 && child.minBounds.y != xFace.minBounds.y) {
            std::cout << "    WARNING: Y min incorrectly at boundary " << child.minBounds.y << "\n";
            hasInternalBoundary = true;
        }
        if (std::abs(child.maxBounds.y) == 1.0 && child.maxBounds.y != xFace.maxBounds.y) {
            std::cout << "    WARNING: Y max incorrectly at boundary " << child.maxBounds.y << "\n";
            hasInternalBoundary = true;
        }
        if (std::abs(child.minBounds.z) == 1.0 && child.minBounds.z != xFace.minBounds.z) {
            std::cout << "    WARNING: Z min incorrectly at boundary " << child.minBounds.z << "\n";
            hasInternalBoundary = true;
        }
        if (std::abs(child.maxBounds.z) == 1.0 && child.maxBounds.z != xFace.maxBounds.z) {
            std::cout << "    WARNING: Z max incorrectly at boundary " << child.maxBounds.z << "\n";
            hasInternalBoundary = true;
        }
    }
    
    // Now check subdivision one level deeper
    std::cout << "\n=== Testing Level 2 Subdivision ===\n";
    auto grandchildren = core::GlobalPatchGenerator::subdivide(children[0]);
    
    for (size_t i = 0; i < grandchildren.size(); i++) {
        auto& gc = grandchildren[i];
        std::cout << "  Grandchild " << i << ": (" 
                  << gc.minBounds.x << "," << gc.minBounds.y << "," << gc.minBounds.z 
                  << ") to (" 
                  << gc.maxBounds.x << "," << gc.maxBounds.y << "," << gc.maxBounds.z << ")\n";
    }
    
    // Test transform generation for problematic patches
    std::cout << "\n=== Testing Transform Generation ===\n";
    for (auto& child : children) {
        auto transform = child.createTransform();
        
        // Test UV(0.5, 0.5) -> cube position
        glm::dvec4 uv(0.5, 0.5, 0, 1);
        glm::dvec3 cubePos = glm::dvec3(transform * uv);
        
        std::cout << "Patch center " << child.center.x << "," << child.center.y << "," << child.center.z
                  << " -> UV(0.5,0.5) maps to (" << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")\n";
        
        // Check if the result is within the cube bounds
        if (std::abs(cubePos.x) > 1.001 || std::abs(cubePos.y) > 1.001 || std::abs(cubePos.z) > 1.001) {
            std::cout << "  ERROR: UV mapping produces position outside cube!\n";
        }
    }
    
    return 0;
}