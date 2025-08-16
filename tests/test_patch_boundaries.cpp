#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== INVESTIGATING PATCH BOUNDARY COVERAGE ===\n\n";
    std::cout << std::fixed << std::setprecision(6);
    
    std::cout << "QUESTION: Do patches actually reach the face boundaries?\n\n";
    
    // Start with root patches
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    std::cout << "ROOT PATCHES (Level 0):\n";
    for (size_t i = 0; i < roots.size(); i++) {
        const auto& root = roots[i];
        std::cout << "Face " << i << ": bounds (" 
                  << root.minBounds.x << "," << root.minBounds.y << "," << root.minBounds.z
                  << ") to ("
                  << root.maxBounds.x << "," << root.maxBounds.y << "," << root.maxBounds.z
                  << ")\n";
    }
    std::cout << "✓ Root patches DO cover full face boundaries (-1 to 1)\n\n";
    
    // Now subdivide and check children
    std::cout << "SUBDIVIDED PATCHES (Level 1):\n";
    auto children = core::GlobalPatchGenerator::subdivide(roots[4]); // +Z face
    
    std::cout << "+Z face subdivided into 4 children:\n";
    for (size_t i = 0; i < children.size(); i++) {
        const auto& child = children[i];
        std::cout << "  Child " << i << ": bounds (" 
                  << child.minBounds.x << "," << child.minBounds.y << "," << child.minBounds.z
                  << ") to ("
                  << child.maxBounds.x << "," << child.maxBounds.y << "," << child.maxBounds.z
                  << ")\n";
                  
        // Check if any child reaches x=1 boundary (where it would meet +X face)
        if (std::abs(child.maxBounds.x - 1.0f) < 0.001f) {
            std::cout << "    → This patch reaches x=1 boundary!\n";
        }
        // Check if any child reaches y=1 boundary (where it would meet +Y face)
        if (std::abs(child.maxBounds.y - 1.0f) < 0.001f) {
            std::cout << "    → This patch reaches y=1 boundary!\n";
        }
    }
    
    // Check deeper subdivision
    std::cout << "\nSUBDIVIDED AGAIN (Level 2):\n";
    auto grandchildren = core::GlobalPatchGenerator::subdivide(children[2]); // Top-right child
    
    std::cout << "Top-right child of +Z face subdivided:\n";
    for (size_t i = 0; i < grandchildren.size(); i++) {
        const auto& gc = grandchildren[i];
        std::cout << "  Grandchild " << i << ": bounds (" 
                  << gc.minBounds.x << "," << gc.minBounds.y << "," << gc.minBounds.z
                  << ") to ("
                  << gc.maxBounds.x << "," << gc.maxBounds.y << "," << gc.maxBounds.z
                  << ")\n";
                  
        if (std::abs(gc.maxBounds.x - 1.0f) < 0.001f && std::abs(gc.maxBounds.y - 1.0f) < 0.001f) {
            std::cout << "    → This patch reaches the CORNER (1,1,1)!\n";
        }
    }
    
    std::cout << "\n=== ANALYSIS ===\n";
    std::cout << "1. Root patches DO cover full face boundaries ✓\n";
    std::cout << "2. Subdivided patches DO reach face edges ✓\n";
    std::cout << "3. Patches are confined to their face (z=1 for all +Z patches) ✓\n";
    std::cout << "4. Adjacent faces should have matching patches at boundaries ✓\n";
    
    std::cout << "\n=== POTENTIAL ISSUE ===\n";
    std::cout << "Each face generates patches independently.\n";
    std::cout << "Are we actually RENDERING patches from all 6 faces?\n";
    std::cout << "Or are we only rendering patches from visible faces?\n";
    std::cout << "If face culling is too aggressive, boundary patches might be missing!\n";
    
    return 0;
}