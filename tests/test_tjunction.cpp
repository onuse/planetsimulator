#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>

// Simulate the T-junction fixing from the shader
glm::vec2 fixTJunction(glm::vec2 uv, float currentLevel, 
                       float topNeighborLevel, float rightNeighborLevel,
                       float bottomNeighborLevel, float leftNeighborLevel) {
    const float edgeThreshold = 0.002f;
    glm::vec2 fixedUV = uv;
    
    // Top edge (v close to 0)
    if (uv.y < edgeThreshold && topNeighborLevel < currentLevel) {
        float levelDiff = std::min(currentLevel - topNeighborLevel, 10.0f);
        
        if (levelDiff >= 2.0f) {
            fixedUV.x = (uv.x < 0.5f) ? 0.0f : 1.0f;
        } else if (levelDiff > 0.0f) {
            float gridIndex = uv.x * 2.0f;
            fixedUV.x = std::round(gridIndex) * 0.5f;
        }
    }
    
    // Bottom edge (v close to 1)
    else if (uv.y > 1.0f - edgeThreshold && bottomNeighborLevel < currentLevel) {
        float levelDiff = std::min(currentLevel - bottomNeighborLevel, 10.0f);
        
        if (levelDiff >= 2.0f) {
            fixedUV.x = (uv.x < 0.5f) ? 0.0f : 1.0f;
        } else if (levelDiff > 0.0f) {
            float gridIndex = uv.x * 2.0f;
            fixedUV.x = std::round(gridIndex) * 0.5f;
        }
    }
    
    // Left edge (u close to 0)
    if (uv.x < edgeThreshold && leftNeighborLevel < currentLevel) {
        float levelDiff = std::min(currentLevel - leftNeighborLevel, 10.0f);
        
        if (levelDiff >= 2.0f) {
            fixedUV.y = (uv.y < 0.5f) ? 0.0f : 1.0f;
        } else if (levelDiff > 0.0f) {
            float gridIndex = uv.y * 2.0f;
            fixedUV.y = std::round(gridIndex) * 0.5f;
        }
    }
    
    // Right edge (u close to 1)
    else if (uv.x > 1.0f - edgeThreshold && rightNeighborLevel < currentLevel) {
        float levelDiff = std::min(currentLevel - rightNeighborLevel, 10.0f);
        
        if (levelDiff >= 2.0f) {
            fixedUV.y = (uv.y < 0.5f) ? 0.0f : 1.0f;
        } else if (levelDiff > 0.0f) {
            float gridIndex = uv.y * 2.0f;
            fixedUV.y = std::round(gridIndex) * 0.5f;
        }
    }
    
    return fixedUV;
}

void testEdgeVertices() {
    std::cout << "=== Testing T-Junction Fixing ===\n\n";
    
    // Test case: Level 2 patch with Level 1 neighbor on top
    float currentLevel = 2.0f;
    float topNeighborLevel = 1.0f;  // Coarser neighbor
    float otherNeighborLevel = 2.0f; // Same level
    
    std::cout << "Test 1: Level 2 patch with Level 1 neighbor on top (level diff = 1)\n";
    std::cout << "Edge vertices should snap to 0.0, 0.5, 1.0 to match coarser neighbor\n\n";
    
    // Test vertices along top edge
    float testU[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    
    for (float u : testU) {
        glm::vec2 uv(u, 0.001f); // Just inside top edge
        glm::vec2 fixed = fixTJunction(uv, currentLevel, topNeighborLevel, 
                                      otherNeighborLevel, otherNeighborLevel, otherNeighborLevel);
        
        std::cout << "  UV(" << std::setw(4) << uv.x << ", " << std::setw(4) << uv.y 
                  << ") -> Fixed(" << std::setw(4) << fixed.x << ", " << std::setw(4) << fixed.y << ")";
        
        // Check if snapping is correct
        if (u == 0.0f || u == 0.5f || u == 1.0f) {
            if (std::abs(fixed.x - u) < 0.001f) {
                std::cout << " ✓ (kept at grid point)\n";
            } else {
                std::cout << " ✗ (should stay at " << u << ")\n";
            }
        } else {
            float expected = std::round(u * 2.0f) * 0.5f;
            if (std::abs(fixed.x - expected) < 0.001f) {
                std::cout << " ✓ (snapped to " << expected << ")\n";
            } else {
                std::cout << " ✗ (should snap to " << expected << ")\n";
            }
        }
    }
    
    std::cout << "\nTest 2: Level 3 patch with Level 1 neighbor on right (level diff = 2)\n";
    std::cout << "Edge vertices should snap to corners only (0.0 or 1.0)\n\n";
    
    currentLevel = 3.0f;
    float rightNeighborLevel = 1.0f;
    
    for (float v : testU) {
        glm::vec2 uv(0.999f, v); // Just inside right edge
        glm::vec2 fixed = fixTJunction(uv, currentLevel, otherNeighborLevel,
                                      rightNeighborLevel, otherNeighborLevel, otherNeighborLevel);
        
        std::cout << "  UV(" << std::setw(5) << uv.x << ", " << std::setw(4) << uv.y 
                  << ") -> Fixed(" << std::setw(5) << fixed.x << ", " << std::setw(4) << fixed.y << ")";
        
        // For level diff >= 2, should snap to 0 or 1
        float expected = (v < 0.5f) ? 0.0f : 1.0f;
        if (std::abs(fixed.y - expected) < 0.001f) {
            std::cout << " ✓ (snapped to " << expected << ")\n";
        } else {
            std::cout << " ✗ (should snap to " << expected << ")\n";
        }
    }
    
    std::cout << "\nTest 3: Interior vertices (should not be modified)\n\n";
    
    glm::vec2 interiorUV(0.5f, 0.5f);
    glm::vec2 fixed = fixTJunction(interiorUV, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    
    std::cout << "  UV(" << interiorUV.x << ", " << interiorUV.y 
              << ") -> Fixed(" << fixed.x << ", " << fixed.y << ")";
    
    if (glm::length(fixed - interiorUV) < 0.001f) {
        std::cout << " ✓ (unchanged)\n";
    } else {
        std::cout << " ✗ (should not change)\n";
    }
}

int main() {
    testEdgeVertices();
    return 0;
}