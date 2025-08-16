// STEP 9: Simulate EXACTLY what CPUVertexGenerator does
// Without needing to link the whole library

#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

// Simulate the vertex generation
glm::vec3 generateVertex(const glm::dvec3& cubePos, double radius) {
    // Snap to boundary if very close
    glm::dvec3 snapped = cubePos;
    const double EPSILON = 1e-8;
    
    // First pass: snap to exact boundaries
    if (std::abs(snapped.x - 1.0) < EPSILON) snapped.x = 1.0;
    if (std::abs(snapped.x + 1.0) < EPSILON) snapped.x = -1.0;
    if (std::abs(snapped.y - 1.0) < EPSILON) snapped.y = 1.0;
    if (std::abs(snapped.y + 1.0) < EPSILON) snapped.y = -1.0;
    if (std::abs(snapped.z - 1.0) < EPSILON) snapped.z = 1.0;
    if (std::abs(snapped.z + 1.0) < EPSILON) snapped.z = -1.0;
    
    // Cube to sphere transformation
    glm::dvec3 pos2 = snapped * snapped;
    glm::dvec3 spherePos;
    spherePos.x = snapped.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = snapped.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = snapped.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    
    return glm::vec3(glm::normalize(spherePos) * radius);
}

std::vector<glm::vec3> generatePatchVertices(const glm::dmat4& transform, int resolution, double radius) {
    std::vector<glm::vec3> vertices;
    
    for (int y = 0; y < resolution; y++) {
        for (int x = 0; x < resolution; x++) {
            double u = static_cast<double>(x) / (resolution - 1);
            double v = static_cast<double>(y) / (resolution - 1);
            
            glm::dvec4 localPos(u, v, 0.0, 1.0);
            glm::dvec3 cubePos = glm::dvec3(transform * localPos);
            
            vertices.push_back(generateVertex(cubePos, radius));
        }
    }
    
    return vertices;
}

int main() {
    std::cout << "=== SIMULATING CPUVertexGenerator LOGIC ===\n\n";
    
    const double radius = 6371000.0;
    const int resolution = 5;
    
    // Test cross-face patches with EXACT same bounds as step4
    
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
    
    std::cout << "Testing cross-face boundary between +X and +Y\n";
    std::cout << "Shared edge: (1,1,-0.5) to (1,1,0.5)\n\n";
    
    // Generate transforms
    auto transform1 = patch1.createTransform();
    auto transform2 = patch2.createTransform();
    
    // Generate vertices
    auto vertices1 = generatePatchVertices(transform1, resolution, radius);
    auto vertices2 = generatePatchVertices(transform2, resolution, radius);
    
    std::cout << "Generated " << vertices1.size() << " vertices per patch\n\n";
    
    // Check the shared edge
    std::cout << "=== PATCH 1 TOP EDGE (Y=1) ===\n";
    for (int x = 0; x < resolution; x++) {
        int idx = (resolution - 1) * resolution + x;  // Top row
        const auto& v = vertices1[idx];
        
        // What cube position should this be?
        double u = static_cast<double>(x) / (resolution - 1);
        glm::dvec3 cubePos = glm::dvec3(transform1 * glm::dvec4(u, 1.0, 0, 1));
        
        std::cout << "  [" << x << "]: UV(" << u << ",1) -> cube(" 
                  << cubePos.x << ", " << cubePos.y << ", " << cubePos.z 
                  << ") -> world(" << std::fixed << std::setprecision(2)
                  << v.x << ", " << v.y << ", " << v.z << ")\n";
    }
    
    std::cout << "\n=== PATCH 2 RIGHT EDGE (X=1) ===\n";
    for (int y = 0; y < resolution; y++) {
        int idx = y * resolution + (resolution - 1);  // Right column
        const auto& v = vertices2[idx];
        
        // What cube position should this be?
        double v_coord = static_cast<double>(y) / (resolution - 1);
        glm::dvec3 cubePos = glm::dvec3(transform2 * glm::dvec4(1.0, v_coord, 0, 1));
        
        std::cout << "  [" << y << "]: UV(1," << v_coord << ") -> cube(" 
                  << cubePos.x << ", " << cubePos.y << ", " << cubePos.z 
                  << ") -> world(" << std::fixed << std::setprecision(2)
                  << v.x << ", " << v.y << ", " << v.z << ")\n";
    }
    
    // Compare them - they should be in the SAME order!
    std::cout << "\n=== COMPARING SHARED VERTICES ===\n";
    float maxGap = 0.0f;
    
    for (int i = 0; i < resolution; i++) {
        int idx1 = (resolution - 1) * resolution + i;          // Top edge of patch 1
        int idx2 = i * resolution + (resolution - 1);          // Right edge of patch 2 (same order!)
        
        float gap = glm::length(vertices1[idx1] - vertices2[idx2]);
        
        std::cout << "  Point " << i << ": ";
        std::cout << "P1[" << i << "] vs P2[" << i << "] -> ";
        std::cout << "gap = " << gap << " meters";
        
        if (gap < 1.0f) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ LARGE GAP!\n";
            maxGap = std::max(maxGap, gap);
            
            // Debug the mismatch
            const auto& v1 = vertices1[idx1];
            const auto& v2 = vertices2[idx2];
            std::cout << "    P1: (" << v1.x << ", " << v1.y << ", " << v1.z << ")\n";
            std::cout << "    P2: (" << v2.x << ", " << v2.y << ", " << v2.z << ")\n";
            
            // Check cube positions
            double u1 = static_cast<double>(i) / (resolution - 1);
            double v2_coord = static_cast<double>(i) / (resolution - 1);  // Same order!
            
            glm::dvec3 cube1 = glm::dvec3(transform1 * glm::dvec4(u1, 1.0, 0, 1));
            glm::dvec3 cube2 = glm::dvec3(transform2 * glm::dvec4(1.0, v2_coord, 0, 1));
            
            std::cout << "    Cube1: (" << cube1.x << ", " << cube1.y << ", " << cube1.z << ")\n";
            std::cout << "    Cube2: (" << cube2.x << ", " << cube2.y << ", " << cube2.z << ")\n";
            
            double cubeDiff = glm::length(cube1 - cube2);
            std::cout << "    Cube difference: " << cubeDiff << "\n";
        }
    }
    
    std::cout << "\nMaximum gap: " << maxGap << " meters\n";
    
    if (maxGap > 1000.0f) {
        std::cout << "\n✗ FOUND THE PROBLEM: Cross-face boundaries have massive gaps!\n";
        std::cout << "This reproduces the issue without needing the full CPUVertexGenerator.\n";
    } else if (maxGap > 1.0f) {
        std::cout << "\n✗ Some gaps found, but smaller than expected.\n";
    } else {
        std::cout << "\n✓ No significant gaps found.\n";
    }
    
    return 0;
}