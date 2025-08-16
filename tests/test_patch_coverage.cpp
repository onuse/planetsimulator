// TEST: Are patches being generated for all 6 cube faces?
// This could explain the missing section

#include <iostream>
#include <vector>
#include <set>

int main() {
    std::cout << "=== PATCH COVERAGE TEST ===\n\n";
    
    // Simulate what patches should exist at the root level
    std::cout << "Expected: 6 root patches (one per cube face)\n";
    std::cout << "  Face 0 (+X): bounds (1, -1, -1) to (1, 1, 1)\n";
    std::cout << "  Face 1 (-X): bounds (-1, -1, -1) to (-1, 1, 1)\n";
    std::cout << "  Face 2 (+Y): bounds (-1, 1, -1) to (1, 1, 1)\n";
    std::cout << "  Face 3 (-Y): bounds (-1, -1, -1) to (1, -1, 1)\n";
    std::cout << "  Face 4 (+Z): bounds (-1, -1, 1) to (1, 1, 1)\n";
    std::cout << "  Face 5 (-Z): bounds (-1, -1, -1) to (1, 1, -1)\n\n";
    
    std::cout << "HYPOTHESIS: One of these faces is not being rendered.\n\n";
    
    // Let's think about what we saw in the screenshot
    std::cout << "From the screenshot:\n";
    std::cout << "- Camera position: (-1.115e+07, 4.778e+06, -9.556e+06)\n";
    std::cout << "- This is in the -X, +Y, -Z octant\n";
    std::cout << "- We're looking roughly toward the origin\n\n";
    
    std::cout << "Visible faces from this angle should be:\n";
    std::cout << "  +X face: YES (facing us)\n";
    std::cout << "  -X face: NO (facing away)\n";
    std::cout << "  +Y face: PARTIAL (we're above, so we see bottom)\n";
    std::cout << "  -Y face: YES (facing us from below)\n";
    std::cout << "  +Z face: YES (facing us)\n";
    std::cout << "  -Z face: NO (facing away)\n\n";
    
    std::cout << "The black rectangular hole appears to be:\n";
    std::cout << "- Vertical (full height of planet)\n";
    std::cout << "- About 1/6 of the planet width\n";
    std::cout << "- On the right side from our view\n\n";
    
    std::cout << "This matches the +X face being missing!\n\n";
    
    std::cout << "POSSIBLE CAUSES:\n";
    std::cout << "1. Face culling incorrectly removing +X face\n";
    std::cout << "2. +X face patches not being generated in selectLOD()\n";
    std::cout << "3. +X face transform placing it at wrong location\n";
    std::cout << "4. Clipping plane cutting off +X face\n";
    std::cout << "5. Instance data not being uploaded for +X face\n\n";
    
    std::cout << "NEXT STEP: Add logging to see which faces are actually\n";
    std::cout << "being processed in SphericalQuadtree::update()\n";
    
    return 0;
}