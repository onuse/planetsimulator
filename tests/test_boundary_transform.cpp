#include <iostream>
#include <glm/glm.hpp>
#include <cmath>
#include "core/global_patch_generator.hpp"

// Test boundary transforms to see if they produce the same vertex positions

int main() {
    std::cout << "=== Testing Boundary Vertex Generation ===\n\n";
    
    // Create two patches that share a boundary
    // Patch 1: On +X face, at the +Y boundary
    core::GlobalPatchGenerator::GlobalPatch xPatch;
    xPatch.minBounds = glm::dvec3(1.0, 0.5, -0.5);
    xPatch.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    xPatch.center = (xPatch.minBounds + xPatch.maxBounds) * 0.5;
    xPatch.level = 1;
    xPatch.faceId = 0; // +X face
    
    // Patch 2: On +Y face, at the +X boundary  
    core::GlobalPatchGenerator::GlobalPatch yPatch;
    yPatch.minBounds = glm::dvec3(0.5, 1.0, -0.5);
    yPatch.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    yPatch.center = (yPatch.minBounds + yPatch.maxBounds) * 0.5;
    yPatch.level = 1;
    yPatch.faceId = 2; // +Y face
    
    std::cout << "X Patch (on +X face): bounds (" 
              << xPatch.minBounds.x << "," << xPatch.minBounds.y << "," << xPatch.minBounds.z
              << ") to ("
              << xPatch.maxBounds.x << "," << xPatch.maxBounds.y << "," << xPatch.maxBounds.z << ")\n";
    
    std::cout << "Y Patch (on +Y face): bounds (" 
              << yPatch.minBounds.x << "," << yPatch.minBounds.y << "," << yPatch.minBounds.z
              << ") to ("
              << yPatch.maxBounds.x << "," << yPatch.maxBounds.y << "," << yPatch.maxBounds.z << ")\n\n";
    
    // Get transforms
    auto xTransform = xPatch.createTransform();
    auto yTransform = yPatch.createTransform();
    
    // Test shared edge vertices
    // The edge at X=1, Y=1 should be shared between these patches
    std::cout << "Testing shared edge vertices (should produce identical positions):\n\n";
    
    // Test corner vertex at (1, 1, -0.5)
    {
        // From X patch perspective: UV(1, 0) should map to corner
        glm::dvec4 xUV(0.0, 1.0, 0, 1);  // Top-left corner for X-face patch
        glm::dvec3 xPos = glm::dvec3(xTransform * xUV);
        
        // From Y patch perspective: UV(1, 0) should map to same corner
        glm::dvec4 yUV(1.0, 0.0, 0, 1);  // Bottom-right for Y-face patch
        glm::dvec3 yPos = glm::dvec3(yTransform * yUV);
        
        std::cout << "Corner at (1,1,-0.5):\n";
        std::cout << "  X patch UV(0,1) -> (" << xPos.x << "," << xPos.y << "," << xPos.z << ")\n";
        std::cout << "  Y patch UV(1,0) -> (" << yPos.x << "," << yPos.y << "," << yPos.z << ")\n";
        
        double distance = glm::length(xPos - yPos);
        std::cout << "  Distance between vertices: " << distance << " meters\n";
        if (distance > 0.001) {
            std::cout << "  ✗ ERROR: Vertices don't match!\n";
        } else {
            std::cout << "  ✓ Vertices match\n";
        }
    }
    
    std::cout << "\n";
    
    // Test corner vertex at (1, 1, 0.5)
    {
        // From X patch perspective: UV(1, 1) 
        glm::dvec4 xUV(1.0, 1.0, 0, 1);
        glm::dvec3 xPos = glm::dvec3(xTransform * xUV);
        
        // From Y patch perspective: UV(1, 1)
        glm::dvec4 yUV(1.0, 1.0, 0, 1);
        glm::dvec3 yPos = glm::dvec3(yTransform * yUV);
        
        std::cout << "Corner at (1,1,0.5):\n";
        std::cout << "  X patch UV(1,1) -> (" << xPos.x << "," << xPos.y << "," << xPos.z << ")\n";
        std::cout << "  Y patch UV(1,1) -> (" << yPos.x << "," << yPos.y << "," << yPos.z << ")\n";
        
        double distance = glm::length(xPos - yPos);
        std::cout << "  Distance between vertices: " << distance << " meters\n";
        if (distance > 0.001) {
            std::cout << "  ✗ ERROR: Vertices don't match!\n";
        } else {
            std::cout << "  ✓ Vertices match\n";
        }
    }
    
    std::cout << "\n";
    
    // Test midpoint of shared edge
    {
        // From X patch perspective: UV(0.5, 1)
        glm::dvec4 xUV(0.5, 1.0, 0, 1);
        glm::dvec3 xPos = glm::dvec3(xTransform * xUV);
        
        // From Y patch perspective: UV(1, 0.5)
        glm::dvec4 yUV(1.0, 0.5, 0, 1);
        glm::dvec3 yPos = glm::dvec3(yTransform * yUV);
        
        std::cout << "Midpoint at (1,1,0):\n";
        std::cout << "  X patch UV(0.5,1) -> (" << xPos.x << "," << xPos.y << "," << xPos.z << ")\n";
        std::cout << "  Y patch UV(1,0.5) -> (" << yPos.x << "," << yPos.y << "," << yPos.z << ")\n";
        
        double distance = glm::length(xPos - yPos);
        std::cout << "  Distance between vertices: " << distance << " meters\n";
        if (distance > 0.001) {
            std::cout << "  ✗ ERROR: Vertices don't match!\n";
        } else {
            std::cout << "  ✓ Vertices match\n";
        }
    }
    
    return 0;
}