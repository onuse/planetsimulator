#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/global_patch_generator.hpp"

void testEdgePoint(const std::string& desc, const glm::dvec3& cubePos) {
    // This is a point that should be shared between faces
    std::cout << "\nTesting " << desc << ": (" 
              << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
    
    // Project to sphere
    glm::dvec3 spherePos = glm::normalize(cubePos) * 6371000.0;
    std::cout << "  Sphere position: (" 
              << spherePos.x/1000 << "km, " 
              << spherePos.y/1000 << "km, " 
              << spherePos.z/1000 << "km)" << std::endl;
}

int main() {
    std::cout << "=== TRANSFORM ANALYSIS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Test case: Two patches that should share an edge
    // Patch A: Face 0 (+X), at top edge (Y=1)
    // Patch B: Face 2 (+Y), at right edge (X=1)
    // They should meet at the edge where X=1, Y=1
    
    std::cout << "\n--- PATCH A: Face 0 (+X), top edge ---" << std::endl;
    core::GlobalPatchGenerator::GlobalPatch patchA;
    patchA.minBounds = glm::vec3(1.0f, 0.75f, 0.75f);
    patchA.maxBounds = glm::vec3(1.0f, 1.0f, 1.0f);
    patchA.center = (patchA.minBounds + patchA.maxBounds) * 0.5f;
    patchA.faceId = 0;
    
    std::cout << "Bounds: [" << patchA.minBounds.x << "," << patchA.minBounds.y << "," << patchA.minBounds.z 
              << "] to [" << patchA.maxBounds.x << "," << patchA.maxBounds.y << "," << patchA.maxBounds.z << "]" << std::endl;
    
    glm::dmat4 transformA = patchA.createTransform();
    
    // Test corner that should be at edge
    glm::dvec4 uvTopRight(1.0, 1.0, 0.0, 1.0);  // UV(1,1) = top-right corner
    glm::dvec3 cubePosA = glm::dvec3(transformA * uvTopRight);
    std::cout << "UV(1,1) -> Cube: (" << cubePosA.x << ", " << cubePosA.y << ", " << cubePosA.z << ")" << std::endl;
    
    std::cout << "\n--- PATCH B: Face 2 (+Y), right edge ---" << std::endl;
    core::GlobalPatchGenerator::GlobalPatch patchB;
    patchB.minBounds = glm::vec3(0.75f, 1.0f, 0.75f);
    patchB.maxBounds = glm::vec3(1.0f, 1.0f, 1.0f);
    patchB.center = (patchB.minBounds + patchB.maxBounds) * 0.5f;
    patchB.faceId = 2;
    
    std::cout << "Bounds: [" << patchB.minBounds.x << "," << patchB.minBounds.y << "," << patchB.minBounds.z 
              << "] to [" << patchB.maxBounds.x << "," << patchB.maxBounds.y << "," << patchB.maxBounds.z << "]" << std::endl;
    
    glm::dmat4 transformB = patchB.createTransform();
    
    // Test corner that should be at same edge  
    glm::dvec4 uvBottomRight(1.0, 0.0, 0.0, 1.0);  // UV(1,0) for face 2
    glm::dvec3 cubePosB = glm::dvec3(transformB * uvBottomRight);
    std::cout << "UV(1,0) -> Cube: (" << cubePosB.x << ", " << cubePosB.y << ", " << cubePosB.z << ")" << std::endl;
    
    // These should be at the same position!
    std::cout << "\n=== COMPARISON ===" << std::endl;
    std::cout << "Patch A corner: (" << cubePosA.x << ", " << cubePosA.y << ", " << cubePosA.z << ")" << std::endl;
    std::cout << "Patch B corner: (" << cubePosB.x << ", " << cubePosB.y << ", " << cubePosB.z << ")" << std::endl;
    
    double cubeDist = glm::length(cubePosA - cubePosB);
    std::cout << "Distance in cube space: " << cubeDist << std::endl;
    
    // Project to sphere
    glm::dvec3 spherePosA = glm::normalize(cubePosA) * 6371000.0;
    glm::dvec3 spherePosB = glm::normalize(cubePosB) * 6371000.0;
    
    double sphereDist = glm::length(spherePosA - spherePosB);
    std::cout << "Distance on sphere: " << sphereDist << " meters (" << sphereDist/1000 << " km)" << std::endl;
    
    if (sphereDist > 1000) {
        std::cout << "\nERROR: Huge gap detected!" << std::endl;
        std::cout << "Sphere A: (" << spherePosA.x/1000 << "km, " 
                  << spherePosA.y/1000 << "km, " << spherePosA.z/1000 << "km)" << std::endl;
        std::cout << "Sphere B: (" << spherePosB.x/1000 << "km, " 
                  << spherePosB.y/1000 << "km, " << spherePosB.z/1000 << "km)" << std::endl;
    }
    
    // Test the shared edge between +X and +Y faces
    std::cout << "\n=== TESTING SHARED EDGE POINTS ===" << std::endl;
    
    // The edge where X=1, Y=1 should be shared
    testEdgePoint("Edge point 1", glm::dvec3(1.0, 1.0, 0.75));
    testEdgePoint("Edge point 2", glm::dvec3(1.0, 1.0, 0.875));
    testEdgePoint("Edge point 3", glm::dvec3(1.0, 1.0, 1.0));
    
    // Now test if patches generate these correctly
    std::cout << "\n=== TESTING UV MAPPING ===" << std::endl;
    
    // For Patch A (Face 0), what UV gives us the edge?
    for (double v = 0; v <= 1.0; v += 0.5) {
        glm::dvec4 uv(0.5, v, 0.0, 1.0);
        glm::dvec3 cube = glm::dvec3(transformA * uv);
        std::cout << "Patch A: UV(" << uv.x << "," << uv.y << ") -> (" 
                  << cube.x << ", " << cube.y << ", " << cube.z << ")" << std::endl;
    }
    
    // For Patch B (Face 2), what UV gives us the edge?
    for (double u = 0; u <= 1.0; u += 0.5) {
        glm::dvec4 uv(u, 0.5, 0.0, 1.0);
        glm::dvec3 cube = glm::dvec3(transformB * uv);
        std::cout << "Patch B: UV(" << uv.x << "," << uv.y << ") -> (" 
                  << cube.x << ", " << cube.y << ", " << cube.z << ")" << std::endl;
    }
    
    return 0;
}