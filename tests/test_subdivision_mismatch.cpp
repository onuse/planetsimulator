#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

void printPatch(const core::GlobalPatchGenerator::GlobalPatch& patch, const std::string& label) {
    std::cout << label << ":" << std::endl;
    std::cout << "  Face " << patch.faceId << ", Level " << patch.level << std::endl;
    std::cout << "  Bounds: [" << patch.minBounds.x << "," << patch.minBounds.y << "," << patch.minBounds.z
              << "] to [" << patch.maxBounds.x << "," << patch.maxBounds.y << "," << patch.maxBounds.z << "]" << std::endl;
}

int main() {
    std::cout << "=== SUBDIVISION MISMATCH TEST ===" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    
    // Get root patches
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    std::cout << "\n--- ROOT PATCHES ---" << std::endl;
    printPatch(roots[0], "Face 0 (+X)");
    printPatch(roots[2], "Face 2 (+Y)");
    
    // The edge they share is at X=1, Y=1, Z from -1 to 1
    std::cout << "\nShared edge: X=1, Y=1, Z from -1 to 1" << std::endl;
    
    // Now subdivide Face 0
    std::cout << "\n--- SUBDIVIDING FACE 0 ---" << std::endl;
    auto face0_children = core::GlobalPatchGenerator::subdivide(roots[0]);
    for (size_t i = 0; i < face0_children.size(); i++) {
        printPatch(face0_children[i], "Face 0 child " + std::to_string(i));
    }
    
    // Find which child is at the Y=1 edge
    std::cout << "\nFace 0 children at Y=1 edge:" << std::endl;
    for (size_t i = 0; i < face0_children.size(); i++) {
        if (face0_children[i].maxBounds.y >= 0.99) {
            std::cout << "  Child " << i << ": Z range [" 
                      << face0_children[i].minBounds.z << " to " 
                      << face0_children[i].maxBounds.z << "]" << std::endl;
        }
    }
    
    // Now subdivide Face 2
    std::cout << "\n--- SUBDIVIDING FACE 2 ---" << std::endl;
    auto face2_children = core::GlobalPatchGenerator::subdivide(roots[2]);
    for (size_t i = 0; i < face2_children.size(); i++) {
        printPatch(face2_children[i], "Face 2 child " + std::to_string(i));
    }
    
    // Find which child is at the X=1 edge
    std::cout << "\nFace 2 children at X=1 edge:" << std::endl;
    for (size_t i = 0; i < face2_children.size(); i++) {
        if (face2_children[i].maxBounds.x >= 0.99) {
            std::cout << "  Child " << i << ": Z range [" 
                      << face2_children[i].minBounds.z << " to " 
                      << face2_children[i].maxBounds.z << "]" << std::endl;
        }
    }
    
    // Now let's subdivide one more level to see the problem
    std::cout << "\n--- SUBDIVIDING FACE 0 CHILD 2 (top-right) ---" << std::endl;
    auto face0_child2 = face0_children[2];  // Top-right child at Y=1
    auto face0_grandchildren = core::GlobalPatchGenerator::subdivide(face0_child2);
    
    std::cout << "Grandchildren at Y=1 edge:" << std::endl;
    for (size_t i = 0; i < face0_grandchildren.size(); i++) {
        if (face0_grandchildren[i].maxBounds.y >= 0.99) {
            std::cout << "  Grandchild " << i << ": Z range [" 
                      << face0_grandchildren[i].minBounds.z << " to " 
                      << face0_grandchildren[i].maxBounds.z << "]" << std::endl;
        }
    }
    
    std::cout << "\n=== ANALYSIS ===" << std::endl;
    std::cout << "The problem: When faces subdivide independently, patches that should" << std::endl;
    std::cout << "share an edge end up with different Z ranges because they're at" << std::endl;
    std::cout << "different subdivision levels!" << std::endl;
    
    return 0;
}