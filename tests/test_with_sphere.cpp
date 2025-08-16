// STEP 3: Add cube-to-sphere transformation

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

glm::vec3 cubeToSphere(const glm::vec3& cubePos, float radius) {
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
    spherePos.y = cubePos.y * sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
    spherePos.z = cubePos.z * sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
    return glm::normalize(spherePos) * radius;
}

int main() {
    std::cout << "=== ISOLATION TEST: With Sphere Mapping ===\n\n";
    
    const float radius = 100.0f; // Small radius for easy numbers
    
    // Two adjacent patches on +X face
    glm::mat4 transform1 = glm::mat4(1.0f);
    transform1[0] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    transform1[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    transform1[3] = glm::vec4(1.0f, -0.5f, -0.5f, 1.0f);
    
    glm::mat4 transform2 = glm::mat4(1.0f);
    transform2[0] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    transform2[1] = glm::vec4(0.0f, 0.5f, 0.0f, 0.0f);
    transform2[3] = glm::vec4(1.0f, 0.5f, -0.5f, 1.0f);
    
    std::cout << "Testing shared edge after sphere mapping:\n\n";
    
    for (int x = 0; x <= 4; x++) {
        float u = x * 0.25f;
        
        // Patch 1 top edge
        glm::vec3 cube1 = glm::vec3(transform1 * glm::vec4(u, 1.0f, 0, 1));
        glm::vec3 sphere1 = cubeToSphere(cube1, radius);
        
        // Patch 2 bottom edge
        glm::vec3 cube2 = glm::vec3(transform2 * glm::vec4(u, 0.0f, 0, 1));
        glm::vec3 sphere2 = cubeToSphere(cube2, radius);
        
        float distance = glm::length(sphere1 - sphere2);
        
        std::cout << "Point " << x << " (u=" << u << "):\n";
        std::cout << "  Cube space: " << glm::length(cube1 - cube2) << " apart\n";
        std::cout << "  Sphere space: " << distance << " apart\n";
        
        if (distance < 0.001f) {
            std::cout << "  ✓ Match\n";
        } else {
            std::cout << "  ✗ Gap of " << distance << " units\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}