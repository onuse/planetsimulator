#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <cmath>
#include <set>

// Test if the current patch distribution covers the entire sphere
void analyzePatchCoverage() {
    std::cout << "=== PATCH COVERAGE ANALYSIS ===\n\n";
    
    // Simulate the patch distribution we're seeing
    // Face 0 (+X): 55 patches
    // Face 1 (-X): 16 patches  
    // Face 2 (+Y): 34 patches
    // Face 3 (-Y): 19 patches
    // Face 4 (+Z): 49 patches
    // Face 5 (-Z): 16 patches
    
    int patchCounts[6] = {55, 16, 34, 19, 49, 16};
    const char* faceNames[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    
    std::cout << "Current patch distribution:\n";
    int totalPatches = 0;
    for (int i = 0; i < 6; i++) {
        std::cout << "  Face " << i << " (" << faceNames[i] << "): " 
                  << patchCounts[i] << " patches\n";
        totalPatches += patchCounts[i];
    }
    std::cout << "  Total: " << totalPatches << " patches\n\n";
    
    // Analyze the distribution
    std::cout << "Analysis:\n";
    
    // Check for imbalance
    int maxPatches = 0, minPatches = 1000;
    for (int i = 0; i < 6; i++) {
        maxPatches = std::max(maxPatches, patchCounts[i]);
        minPatches = std::min(minPatches, patchCounts[i]);
    }
    
    float imbalanceRatio = (float)maxPatches / minPatches;
    std::cout << "  Patch count imbalance ratio: " << imbalanceRatio << "x\n";
    if (imbalanceRatio > 2.0f) {
        std::cout << "  WARNING: Significant imbalance between faces!\n";
        std::cout << "  This suggests some faces are not subdividing properly.\n";
    }
    
    // Estimate coverage
    // At level 2, each face should have at least 16 patches (4x4)
    // At level 3, each face should have at least 64 patches (8x8) if fully subdivided
    std::cout << "\nExpected patch counts:\n";
    std::cout << "  Level 0: 1 patch per face (6 total)\n";
    std::cout << "  Level 1: 4 patches per face (24 total)\n";
    std::cout << "  Level 2: 16 patches per face (96 total)\n";
    std::cout << "  Level 3: 64 patches per face (384 total)\n";
    
    // Based on the distribution, negative faces seem stuck at level 2
    std::cout << "\nLikely issue:\n";
    std::cout << "  Positive faces (+X, +Y, +Z) have many level 3 patches\n";
    std::cout << "  Negative faces (-X, -Y, -Z) mostly have level 2 patches\n";
    std::cout << "  This creates gaps at the boundaries between faces\n";
    
    // Calculate theoretical gap size
    // Level 2 patch size on cube face: 0.5 units
    // Level 3 patch size on cube face: 0.25 units
    float level2Size = 2.0f / 4.0f;  // Face is 2 units, divided by 4
    float level3Size = 2.0f / 8.0f;  // Face is 2 units, divided by 8
    float gapSize = level2Size - level3Size;
    
    std::cout << "\nGap calculation:\n";
    std::cout << "  Level 2 patch size: " << level2Size << " cube units\n";
    std::cout << "  Level 3 patch size: " << level3Size << " cube units\n";
    std::cout << "  Potential gap: " << gapSize << " cube units\n";
    
    // At planet scale
    float planetRadius = 6371000.0f;
    float gapAtPlanetScale = gapSize * planetRadius;
    std::cout << "  Gap at planet scale: " << gapAtPlanetScale/1000 << " km\n";
}

// Test where black holes would appear
void predictBlackHoleLocations() {
    std::cout << "\n=== BLACK HOLE PREDICTION ===\n\n";
    
    std::cout << "Black holes likely appear at:\n";
    std::cout << "1. Edges between positive and negative faces\n";
    std::cout << "   - Between +X and -Y faces\n";
    std::cout << "   - Between +Y and -Z faces\n";
    std::cout << "   - Between +Z and -X faces\n";
    std::cout << "\n2. Corners where three faces meet\n";
    std::cout << "   - Especially where LOD levels differ\n";
    std::cout << "\n3. T-junctions between level 2 and level 3 patches\n";
    
    // Calculate corner positions on cube
    std::cout << "\nCube corners (most likely black hole locations):\n";
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                std::cout << "  (" << x << ", " << y << ", " << z << ")";
                
                // Check which faces meet here
                std::cout << " - Faces: ";
                if (x > 0) std::cout << "+X ";
                else std::cout << "-X ";
                if (y > 0) std::cout << "+Y ";
                else std::cout << "-Y ";
                if (z > 0) std::cout << "+Z ";
                else std::cout << "-Z ";
                std::cout << "\n";
            }
        }
    }
}

int main() {
    analyzePatchCoverage();
    predictBlackHoleLocations();
    
    std::cout << "\n=== SOLUTION ===\n";
    std::cout << "To fix the black holes:\n";
    std::cout << "1. Ensure all faces subdivide to the same level\n";
    std::cout << "2. Add special handling for face edges and corners\n";
    std::cout << "3. Implement proper T-junction fixing at LOD boundaries\n";
    std::cout << "4. Consider adding 'skirt' geometry to fill gaps\n";
    
    return 0;
}