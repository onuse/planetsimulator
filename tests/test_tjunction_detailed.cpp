// Detailed T-junction analysis to understand the exact snapping behavior
#include "../shaders/src/lib/shader_math.h"
#include <iostream>
#include <iomanip>
#include <cmath>

void analyzeSnapping() {
    std::cout << "T-JUNCTION SNAPPING ANALYSIS\n";
    std::cout << "=============================\n\n";
    
    // Test different level differences
    for (int levelDiff = 1; levelDiff <= 4; levelDiff++) {
        std::cout << "Level Difference: " << levelDiff << "\n";
        
        float coarseSpacing = 0.5f * pow(2.0f, levelDiff - 1.0f);
        int numCoarseVertices = (int)(1.0f / coarseSpacing) + 1;
        
        std::cout << "  Coarse spacing: " << coarseSpacing << "\n";
        std::cout << "  Coarse vertices at: ";
        for (int i = 0; i < numCoarseVertices; i++) {
            std::cout << (i * coarseSpacing) << " ";
        }
        std::cout << "\n";
        
        // Test key fine vertices
        float testPoints[] = {0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f};
        
        std::cout << "  Fine vertex snapping:\n";
        for (float v : testPoints) {
            vec2 uv = {v, 0.001f};  // On top edge
            vec2 snapped = fixTJunctionEdge(uv, levelDiff, 1);
            
            // Check if it matches a coarse vertex
            bool matchesCoarse = false;
            for (int i = 0; i < numCoarseVertices; i++) {
                if (std::abs(snapped.x - i * coarseSpacing) < 0.001f) {
                    matchesCoarse = true;
                    break;
                }
            }
            
            std::cout << "    " << std::setw(5) << v << " -> " << std::setw(5) << snapped.x;
            if (!matchesCoarse) {
                std::cout << " [ERROR: Not on coarse grid!]";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    
    // Analyze the problematic case
    std::cout << "PROBLEMATIC CASE ANALYSIS\n";
    std::cout << "-------------------------\n";
    std::cout << "When levelDiff=2:\n";
    std::cout << "  Coarse has vertices at: 0, 1 (only 2 vertices)\n";
    std::cout << "  Fine has vertices at: 0, 0.25, 0.5, 0.75, 1 (5 vertices)\n";
    std::cout << "  Current behavior:\n";
    
    float fineVerts[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (float v : fineVerts) {
        vec2 uv = {v, 0.001f};
        vec2 snapped = fixTJunctionEdge(uv, 2.0f, 1);
        std::cout << "    " << v << " -> " << snapped.x;
        
        // Analyze the snapping
        if (v == 0.5f && snapped.x != 0.0f && snapped.x != 1.0f) {
            std::cout << " [WARNING: Middle vertex not snapping to coarse grid!]";
        }
        std::cout << "\n";
    }
    
    std::cout << "\nEXPECTED vs ACTUAL:\n";
    std::cout << "  0.00 -> 0.0 ✓\n";
    std::cout << "  0.25 -> 0.0 (should snap to nearest coarse vertex)\n";
    std::cout << "  0.50 -> ??? (equidistant from 0 and 1)\n";
    std::cout << "  0.75 -> 1.0 (should snap to nearest coarse vertex)\n";
    std::cout << "  1.00 -> 1.0 ✓\n";
    
    // Test the tie-breaking logic
    std::cout << "\nTIE-BREAKING ANALYSIS (levelDiff=2, vertex at 0.5):\n";
    vec2 midPoint = {0.5f, 0.001f};
    float levelDiff = 2.0f;
    float coarseSpacing = 0.5f * pow(2.0f, levelDiff - 1.0f);
    float gridIndex = midPoint.x / coarseSpacing;
    float nearest = round(gridIndex);
    float result = nearest * coarseSpacing;
    
    std::cout << "  Coarse spacing: " << coarseSpacing << "\n";
    std::cout << "  Grid index: " << gridIndex << "\n";
    std::cout << "  Nearest: " << nearest << "\n";
    std::cout << "  Result before tie-break: " << result << "\n";
    
    vec2 fixed = fixTJunctionEdge(midPoint, levelDiff, 1);
    std::cout << "  Final result: " << fixed.x << "\n";
    
    if (std::abs(fixed.x - 0.5f) < 0.001f) {
        std::cout << "  ERROR: 0.5 staying at 0.5 creates a T-junction!\n";
        std::cout << "         Coarse neighbor has no vertex at 0.5!\n";
    }
}

int main() {
    analyzeSnapping();
    return 0;
}