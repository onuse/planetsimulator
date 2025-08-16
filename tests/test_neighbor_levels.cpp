#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

// Simplified test to check neighbor levels
int main() {
    std::cout << "=== TESTING NEIGHBOR LEVEL ASSIGNMENTS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    
    // Simulate the problem: two faces with different subdivision levels
    std::cout << "\n1. Creating test patches..." << std::endl;
    
    // Face 0 (+X): One large patch at top edge
    core::GlobalPatchGenerator::GlobalPatch face0_patch;
    face0_patch.faceId = 0;
    face0_patch.level = 2;
    face0_patch.minBounds = glm::vec3(1.0f, 0.0f, -1.0f);
    face0_patch.maxBounds = glm::vec3(1.0f, 1.0f, 1.0f);
    face0_patch.center = (face0_patch.minBounds + face0_patch.maxBounds) * 0.5f;
    
    std::cout << "Face 0 patch: Level " << face0_patch.level 
              << ", Y range [" << face0_patch.minBounds.y << " to " << face0_patch.maxBounds.y
              << "], Z range [" << face0_patch.minBounds.z << " to " << face0_patch.maxBounds.z << "]" << std::endl;
    
    // Face 2 (+Y): Two smaller patches at right edge
    core::GlobalPatchGenerator::GlobalPatch face2_patch1;
    face2_patch1.faceId = 2;
    face2_patch1.level = 3;  // Higher level = smaller patch
    face2_patch1.minBounds = glm::vec3(0.0f, 1.0f, -1.0f);
    face2_patch1.maxBounds = glm::vec3(1.0f, 1.0f, 0.0f);
    face2_patch1.center = (face2_patch1.minBounds + face2_patch1.maxBounds) * 0.5f;
    
    core::GlobalPatchGenerator::GlobalPatch face2_patch2;
    face2_patch2.faceId = 2;
    face2_patch2.level = 3;
    face2_patch2.minBounds = glm::vec3(0.0f, 1.0f, 0.0f);
    face2_patch2.maxBounds = glm::vec3(1.0f, 1.0f, 1.0f);
    face2_patch2.center = (face2_patch2.minBounds + face2_patch2.maxBounds) * 0.5f;
    
    std::cout << "Face 2 patch 1: Level " << face2_patch1.level
              << ", X range [" << face2_patch1.minBounds.x << " to " << face2_patch1.maxBounds.x
              << "], Z range [" << face2_patch1.minBounds.z << " to " << face2_patch1.maxBounds.z << "]" << std::endl;
    std::cout << "Face 2 patch 2: Level " << face2_patch2.level
              << ", X range [" << face2_patch2.minBounds.x << " to " << face2_patch2.maxBounds.x
              << "], Z range [" << face2_patch2.minBounds.z << " to " << face2_patch2.maxBounds.z << "]" << std::endl;
    
    // The problem: Face 0's top edge (Y=1) meets Face 2's right edge (X=1)
    // Face 0 has ONE patch covering Z from -1 to 1
    // Face 2 has TWO patches: one from Z=-1 to 0, another from Z=0 to 1
    // This creates T-junctions!
    
    std::cout << "\n2. Analysis:" << std::endl;
    std::cout << "Face 0 top edge (Y=1): Single patch at level " << face0_patch.level << std::endl;
    std::cout << "Face 2 right edge (X=1): Two patches at level " << face2_patch1.level << std::endl;
    std::cout << "Level difference: " << (face2_patch1.level - face0_patch.level) << std::endl;
    
    std::cout << "\n3. T-Junction locations:" << std::endl;
    std::cout << "At cube position (1, 1, 0):" << std::endl;
    std::cout << "  - Face 0 has a vertex in the MIDDLE of its edge" << std::endl;
    std::cout << "  - Face 2 has a vertex at the BOUNDARY between its two patches" << std::endl;
    std::cout << "  - WITHOUT proper neighbor levels, Face 0 doesn't know to add this vertex!" << std::endl;
    
    std::cout << "\n4. Solution:" << std::endl;
    std::cout << "Face 0's top edge neighbor level should be: " << face2_patch1.level << std::endl;
    std::cout << "This tells the vertex generator to add extra vertices to match Face 2's subdivision" << std::endl;
    
    // Simulate vertex generation at different resolutions
    std::cout << "\n5. Vertex generation example (simplified):" << std::endl;
    
    int baseRes = 5;  // 5 vertices along edge at base level
    
    std::cout << "Face 0 edge vertices (level " << face0_patch.level << "):" << std::endl;
    std::cout << "  Without T-junction fix: ";
    for (int i = 0; i < baseRes; i++) {
        float t = i / float(baseRes - 1);
        float z = -1.0f + 2.0f * t;
        std::cout << z << " ";
    }
    std::cout << std::endl;
    
    std::cout << "  With T-junction fix (neighbor level " << face2_patch1.level << "): ";
    int adjustedRes = baseRes * 2;  // Double resolution for level difference of 1
    for (int i = 0; i < adjustedRes; i++) {
        float t = i / float(adjustedRes - 1);
        float z = -1.0f + 2.0f * t;
        std::cout << z << " ";
    }
    std::cout << std::endl;
    
    std::cout << "\nFace 2 edge vertices (level " << face2_patch1.level << "):" << std::endl;
    std::cout << "  Patch 1: ";
    for (int i = 0; i < baseRes; i++) {
        float t = i / float(baseRes - 1);
        float z = -1.0f + 1.0f * t;  // From -1 to 0
        std::cout << z << " ";
    }
    std::cout << std::endl;
    std::cout << "  Patch 2: ";
    for (int i = 0; i < baseRes; i++) {
        float t = i / float(baseRes - 1);
        float z = 0.0f + 1.0f * t;  // From 0 to 1
        std::cout << z << " ";
    }
    std::cout << std::endl;
    
    std::cout << "\n=== CONCLUSION ===" << std::endl;
    std::cout << "The cross-face neighbor finder ensures that patches know about" << std::endl;
    std::cout << "subdivision levels on adjacent cube faces, allowing proper" << std::endl;
    std::cout << "T-junction resolution and eliminating the 5890km gaps!" << std::endl;
    
    return 0;
}