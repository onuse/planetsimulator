#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"

int main() {
    std::cout << "=== TESTING FUNDAMENTAL ISSUES ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    
    // Let's check if the basic face definitions are correct
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    std::cout << "\n1. ROOT FACE DEFINITIONS:" << std::endl;
    for (size_t i = 0; i < roots.size(); i++) {
        const auto& face = roots[i];
        std::cout << "Face " << i << ": ";
        switch(i) {
            case 0: std::cout << "+X "; break;
            case 1: std::cout << "-X "; break;
            case 2: std::cout << "+Y "; break;
            case 3: std::cout << "-Y "; break;
            case 4: std::cout << "+Z "; break;
            case 5: std::cout << "-Z "; break;
        }
        std::cout << "bounds [" 
                  << face.minBounds.x << "," << face.minBounds.y << "," << face.minBounds.z
                  << "] to ["
                  << face.maxBounds.x << "," << face.maxBounds.y << "," << face.maxBounds.z << "]" << std::endl;
        
        // Check if face has degenerate dimension
        glm::vec3 range = face.maxBounds - face.minBounds;
        if (range.x < 0.001f) std::cout << "   X is fixed at " << face.center.x << std::endl;
        if (range.y < 0.001f) std::cout << "   Y is fixed at " << face.center.y << std::endl;
        if (range.z < 0.001f) std::cout << "   Z is fixed at " << face.center.z << std::endl;
    }
    
    std::cout << "\n2. CHECKING FACE COVERAGE:" << std::endl;
    // A cube should have all 6 faces, each with one dimension fixed at ±1
    bool hasPositiveX = false, hasNegativeX = false;
    bool hasPositiveY = false, hasNegativeY = false;
    bool hasPositiveZ = false, hasNegativeZ = false;
    
    for (const auto& face : roots) {
        glm::vec3 range = face.maxBounds - face.minBounds;
        if (range.x < 0.001f) {
            if (face.center.x > 0.5f) hasPositiveX = true;
            if (face.center.x < -0.5f) hasNegativeX = true;
        }
        if (range.y < 0.001f) {
            if (face.center.y > 0.5f) hasPositiveY = true;
            if (face.center.y < -0.5f) hasNegativeY = true;
        }
        if (range.z < 0.001f) {
            if (face.center.z > 0.5f) hasPositiveZ = true;
            if (face.center.z < -0.5f) hasNegativeZ = true;
        }
    }
    
    std::cout << "Has +X face: " << (hasPositiveX ? "YES" : "NO") << std::endl;
    std::cout << "Has -X face: " << (hasNegativeX ? "YES" : "NO") << std::endl;
    std::cout << "Has +Y face: " << (hasPositiveY ? "YES" : "NO") << std::endl;
    std::cout << "Has -Y face: " << (hasNegativeY ? "YES" : "NO") << std::endl;
    std::cout << "Has +Z face: " << (hasPositiveZ ? "YES" : "NO") << std::endl;
    std::cout << "Has -Z face: " << (hasNegativeZ ? "YES" : "NO") << std::endl;
    
    std::cout << "\n3. TESTING TRANSFORM MATRICES:" << std::endl;
    // Test that UV (0,0) to (1,1) maps to the expected cube region
    for (size_t i = 0; i < std::min(size_t(2), roots.size()); i++) {
        auto& patch = roots[i];
        glm::dmat4 transform = patch.createTransform();
        
        std::cout << "\nFace " << i << " transform test:" << std::endl;
        
        // Test corners
        glm::dvec4 corners[4] = {
            glm::dvec4(0, 0, 0, 1),  // Bottom-left
            glm::dvec4(1, 0, 0, 1),  // Bottom-right  
            glm::dvec4(0, 1, 0, 1),  // Top-left
            glm::dvec4(1, 1, 0, 1)   // Top-right
        };
        
        for (int c = 0; c < 4; c++) {
            glm::dvec3 cubePos = glm::dvec3(transform * corners[c]);
            std::cout << "  UV(" << corners[c].x << "," << corners[c].y << ") -> Cube(" 
                      << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
            
            // Check for (0,0,0) which would cause NaN when normalized
            if (glm::length(cubePos) < 0.001) {
                std::cout << "  ERROR: Transform produced near-zero position!" << std::endl;
            }
        }
    }
    
    std::cout << "\n4. ANALYSIS:" << std::endl;
    std::cout << "If faces are missing or transforms are wrong, the planet will have large gaps." << std::endl;
    std::cout << "T-junctions are a secondary issue compared to missing/misaligned faces." << std::endl;
    
    return 0;
}