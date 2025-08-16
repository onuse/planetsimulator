#include <iostream>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== VERIFYING CORRECT EDGE PARAMETERIZATION ===\n\n";
    
    // +Z face patch (right side, touching +X)
    core::GlobalPatchGenerator::GlobalPatch zPatch;
    zPatch.minBounds = glm::vec3(0.5, -0.5, 1.0);
    zPatch.maxBounds = glm::vec3(1.0, 0.5, 1.0);
    zPatch.center = (zPatch.minBounds + zPatch.maxBounds) * 0.5f;
    zPatch.faceId = 4;
    
    // +X face patch (top side, touching +Z)  
    core::GlobalPatchGenerator::GlobalPatch xPatch;
    xPatch.minBounds = glm::vec3(1.0, -0.5, 0.5);
    xPatch.maxBounds = glm::vec3(1.0, 0.5, 1.0);
    xPatch.center = (xPatch.minBounds + xPatch.maxBounds) * 0.5f;
    xPatch.faceId = 0;
    
    glm::dmat4 zTrans = zPatch.createTransform();
    glm::dmat4 xTrans = xPatch.createTransform();
    
    std::cout << "The shared edge should be at (1, y-varies from -0.5 to 0.5, 1)\n\n";
    
    for (double t = 0.0; t <= 1.0; t += 0.5) {
        // For +Z: right edge is u=1, v varies
        glm::dvec3 zPoint = glm::dvec3(zTrans * glm::dvec4(1.0, t, 0, 1));
        
        // For +X: The edge at z=1...
        // +X maps UV as: U->Z, V->Y
        // So to get z=1, we need u=1
        // And v=t gives us the Y variation
        glm::dvec3 xPoint = glm::dvec3(xTrans * glm::dvec4(1.0, t, 0, 1));
        
        std::cout << "t=" << t << ":\n";
        std::cout << "  +Z UV(1," << t << ") -> (" << zPoint.x << ", " << zPoint.y << ", " << zPoint.z << ")\n";
        std::cout << "  +X UV(1," << t << ") -> (" << xPoint.x << ", " << xPoint.y << ", " << xPoint.z << ")\n";
        
        double gap = glm::length(zPoint - xPoint);
        std::cout << "  Gap: " << gap << "\n\n";
    }
    
    std::cout << "CONCLUSION: If both use UV(1,t), do they map to the same edge?\n";
    
    return 0;
}