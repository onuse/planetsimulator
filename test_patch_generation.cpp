#include <iostream>
#include <glm/glm.hpp>
#include <iomanip>

// Test to understand how patches are being generated and why they're incompatible

void analyzePatchGeneration() {
    std::cout << "=== PATCH GENERATION ANALYSIS ===\n\n";
    
    // Simulate how SphericalQuadtreeNode creates patches for each face
    std::cout << "How each face generates its patch corners:\n";
    std::cout << "==========================================\n\n";
    
    // For a patch at center (1, 0, 0) on +X face with size 0.5
    glm::vec3 center = glm::vec3(1.0f, 0.0f, 0.0f);
    float halfSize = 0.25f;
    
    std::cout << "+X FACE (center at 1,0,0):\n";
    glm::vec3 right = glm::vec3(0, 0, 1);  // Z axis
    glm::vec3 up = glm::vec3(0, 1, 0);     // Y axis
    
    glm::vec3 corners[4];
    corners[0] = center + (-right - up) * halfSize;  // BL
    corners[1] = center + (right - up) * halfSize;   // BR
    corners[2] = center + (right + up) * halfSize;   // TR
    corners[3] = center + (-right + up) * halfSize;  // TL
    
    std::cout << "  Right vector: " << right.x << "," << right.y << "," << right.z << " (Z axis)\n";
    std::cout << "  Up vector: " << up.x << "," << up.y << "," << up.z << " (Y axis)\n";
    std::cout << "  Corners:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "    [" << i << "]: (" << corners[i].x << "," << corners[i].y << "," << corners[i].z << ")\n";
    }
    
    // Now for +Y face
    std::cout << "\n+Y FACE (center at 0,1,0):\n";
    center = glm::vec3(0.0f, 1.0f, 0.0f);
    right = glm::vec3(1, 0, 0);  // X axis
    up = glm::vec3(0, 0, 1);     // Z axis
    
    corners[0] = center + (-right - up) * halfSize;
    corners[1] = center + (right - up) * halfSize;
    corners[2] = center + (right + up) * halfSize;
    corners[3] = center + (-right + up) * halfSize;
    
    std::cout << "  Right vector: " << right.x << "," << right.y << "," << right.z << " (X axis)\n";
    std::cout << "  Up vector: " << up.x << "," << up.y << "," << up.z << " (Z axis)\n";
    std::cout << "  Corners:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "    [" << i << "]: (" << corners[i].x << "," << corners[i].y << "," << corners[i].z << ")\n";
    }
    
    std::cout << "\n=== THE PROBLEM ===\n";
    std::cout << "Each face uses DIFFERENT axes for 'right' and 'up':\n";
    std::cout << "- +X face: right=Z, up=Y\n";
    std::cout << "- +Y face: right=X, up=Z\n";
    std::cout << "- +Z face: right=-X, up=Y\n\n";
    
    std::cout << "This means UV(0,0) to UV(1,1) maps to different directions!\n\n";
}

void demonstrateMismatch() {
    std::cout << "=== MISMATCH AT SHARED EDGE ===\n\n";
    
    // Consider patches at the edge between +X and +Y faces
    // They meet at the edge where X=1, Y=1
    
    std::cout << "Patch on +X face near edge (center at 1, 0.75, 0):\n";
    glm::vec3 centerX = glm::vec3(1.0f, 0.75f, 0.0f);
    glm::vec3 rightX = glm::vec3(0, 0, 1);
    glm::vec3 upX = glm::vec3(0, 1, 0);
    float halfSize = 0.25f;
    
    // Top edge of this patch (should touch +Y face)
    glm::vec3 topLeft = centerX + (-rightX + upX) * halfSize;
    glm::vec3 topRight = centerX + (rightX + upX) * halfSize;
    
    std::cout << "  Top edge: (" << topLeft.x << "," << topLeft.y << "," << topLeft.z 
              << ") to (" << topRight.x << "," << topRight.y << "," << topRight.z << ")\n";
    
    std::cout << "\nPatch on +Y face near edge (center at 0.75, 1, 0):\n";
    glm::vec3 centerY = glm::vec3(0.75f, 1.0f, 0.0f);
    glm::vec3 rightY = glm::vec3(1, 0, 0);
    glm::vec3 upY = glm::vec3(0, 0, 1);
    
    // Right edge of this patch (should touch +X face)
    glm::vec3 bottomRight = centerY + (rightY - upY) * halfSize;
    glm::vec3 topRightY = centerY + (rightY + upY) * halfSize;
    
    std::cout << "  Right edge: (" << bottomRight.x << "," << bottomRight.y << "," << bottomRight.z 
              << ") to (" << topRightY.x << "," << topRightY.y << "," << topRightY.z << ")\n";
    
    std::cout << "\n=== OBSERVATION ===\n";
    std::cout << "These edges are at X=1, Y=1 but with different Z ranges!\n";
    std::cout << "They're geometrically adjacent but UV-incompatible.\n";
}

void proposeSolution() {
    std::cout << "\n=== SOLUTION APPROACH ===\n\n";
    
    std::cout << "Option 1: GLOBAL COORDINATE SYSTEM\n";
    std::cout << "  Instead of face-local (right,up) vectors, use global (u,v,w):\n";
    std::cout << "  - All patches use same UV mapping regardless of face\n";
    std::cout << "  - UV always maps to consistent world directions\n\n";
    
    std::cout << "Option 2: TRANSFORM AT SAMPLING TIME\n";
    std::cout << "  - Keep face-local patch generation\n";
    std::cout << "  - But transform to global coords before terrain sampling\n";
    std::cout << "  - Ensure adjacent patches get same global position\n\n";
    
    std::cout << "Option 3: DIFFERENT TESSELLATION\n";
    std::cout << "  - Use icosahedron instead of cube\n";
    std::cout << "  - Or use a single continuous parameterization\n\n";
    
    std::cout << "The key insight: We must ensure that geometrically adjacent\n";
    std::cout << "vertices sample terrain from the SAME world position,\n";
    std::cout << "regardless of which face they belong to!\n";
}

int main() {
    analyzePatchGeneration();
    demonstrateMismatch();
    proposeSolution();
    return 0;
}