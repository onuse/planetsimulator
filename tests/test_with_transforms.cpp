// STEP 2: Add transforms to see if that breaks alignment

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

int main() {
    std::cout << "=== ISOLATION TEST: With Transforms ===\n\n";
    
    // Create transform for patch on +X face at (1, -0.5..0.5, -0.5..0.5)
    glm::mat4 transform1 = glm::mat4(1.0f);
    transform1[0] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);    // U -> Z
    transform1[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);    // V -> Y
    transform1[3] = glm::vec4(1.0f, -0.5f, -0.5f, 1.0f);  // Origin
    
    // Create transform for adjacent patch at (1, 0.5..1.0, -0.5..0.5) - shares top edge
    glm::mat4 transform2 = glm::mat4(1.0f);
    transform2[0] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);    // U -> Z
    transform2[1] = glm::vec4(0.0f, 0.5f, 0.0f, 0.0f);    // V -> Y (half size)
    transform2[3] = glm::vec4(1.0f, 0.5f, -0.5f, 1.0f);   // Origin
    
    // Generate vertices using transforms
    std::cout << "Patch 1 top edge (should be at Y=0.5):\n";
    for (int x = 0; x <= 2; x++) {
        float u = x * 0.5f;
        float v = 1.0f; // Top edge
        glm::vec4 uv(u, v, 0, 1);
        glm::vec3 pos = glm::vec3(transform1 * uv);
        std::cout << "  UV(" << u << ",1) -> (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    }
    
    std::cout << "\nPatch 2 bottom edge (should be at Y=0.5):\n";
    for (int x = 0; x <= 2; x++) {
        float u = x * 0.5f;
        float v = 0.0f; // Bottom edge
        glm::vec4 uv(u, v, 0, 1);
        glm::vec3 pos = glm::vec3(transform2 * uv);
        std::cout << "  UV(" << u << ",0) -> (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    }
    
    // Check if they match
    std::cout << "\nDo the edges match?\n";
    bool match = true;
    for (int x = 0; x <= 2; x++) {
        float u = x * 0.5f;
        
        glm::vec3 pos1 = glm::vec3(transform1 * glm::vec4(u, 1.0f, 0, 1));
        glm::vec3 pos2 = glm::vec3(transform2 * glm::vec4(u, 0.0f, 0, 1));
        
        float dist = glm::length(pos1 - pos2);
        std::cout << "  Point " << x << ": distance = " << dist;
        
        if (dist < 0.0001f) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ MISMATCH\n";
            match = false;
        }
    }
    
    if (match) {
        std::cout << "\n✓ TRANSFORMS WORK: Edges still align\n";
        std::cout << "NEXT: Add cube-to-sphere transformation\n";
    } else {
        std::cout << "\n✗ TRANSFORMS BROKE IT: This is the problem!\n";
        std::cout << "FOUND THE ISSUE: Transform matrices are wrong\n";
    }
    
    return 0;
}