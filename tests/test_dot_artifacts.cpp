#include <iostream>
#include <cmath>
#include <iomanip>

// Analysis of the dot artifacts at face boundaries

int main() {
    std::cout << "==========================================================\n";
    std::cout << "          DOT ARTIFACT ANALYSIS AT FACE BOUNDARIES\n";
    std::cout << "==========================================================\n\n";
    
    std::cout << "OBSERVATION:\n";
    std::cout << "- Blue face shows salmon dots on left edge (from +X face)\n";
    std::cout << "- Blue face shows green dots on bottom edge (from -Y face)\n";
    std::cout << "- Pattern suggests vertices from adjacent faces 'bleeding' through\n\n";
    
    std::cout << "HYPOTHESIS 1: T-Junction Problem\n";
    std::cout << "------------------------------------\n";
    std::cout << "When patches of different LOD levels meet:\n";
    std::cout << "  High LOD:  *---*---*---*  (4 vertices)\n";
    std::cout << "  Low LOD:   *-----------*  (2 vertices)\n";
    std::cout << "  Gap:           ^   ^      (T-junctions form here)\n";
    std::cout << "\nT-junctions can cause pixels to 'leak' from adjacent faces.\n\n";
    
    std::cout << "HYPOTHESIS 2: Face Boundary Precision\n";
    std::cout << "---------------------------------------\n";
    std::cout << "At face corners, three faces meet at a single point:\n";
    std::cout << "  Corner (1,1,1) is shared by +X, +Y, and +Z faces\n";
    std::cout << "\nFloating point errors can cause vertices to be slightly off:\n";
    
    // Simulate precision issue
    double boundary = 1.0;
    float boundaryF = 1.0f;
    
    // After transforms and calculations
    double epsilon = 1e-6;
    double vertex1 = boundary - epsilon;  // Slightly inside
    double vertex2 = boundary + epsilon;  // Slightly outside
    
    std::cout << "  Exact boundary: " << std::setprecision(15) << boundary << "\n";
    std::cout << "  Vertex 1: " << vertex1 << " (gap of " << (boundary - vertex1) << ")\n";
    std::cout << "  Vertex 2: " << vertex2 << " (overshoot of " << (vertex2 - boundary) << ")\n\n";
    
    std::cout << "HYPOTHESIS 3: Vertex Attribute Mixing\n";
    std::cout << "--------------------------------------\n";
    std::cout << "In CPU vertex mode with instanceCount=1:\n";
    std::cout << "- All vertices are in a single buffer\n";
    std::cout << "- FaceId might be getting mixed at boundaries\n";
    std::cout << "- Shared vertices between faces might use wrong faceId\n\n";
    
    std::cout << "EVIDENCE FROM SCREENSHOT:\n";
    std::cout << "-------------------------\n";
    std::cout << "1. Dots appear EXACTLY at face boundaries\n";
    std::cout << "2. Dot colors match adjacent face colors\n";
    std::cout << "3. Pattern is regular (grid-like) suggesting vertex positions\n";
    std::cout << "4. No dots appear in face interiors\n\n";
    
    std::cout << "MOST LIKELY CAUSE:\n";
    std::cout << "==================\n";
    std::cout << "Vertices at face boundaries are being generated with the wrong faceId\n";
    std::cout << "or are being duplicated with different faceIds, causing some vertices\n";
    std::cout << "to render with the adjacent face's color.\n\n";
    
    std::cout << "This happens because:\n";
    std::cout << "1. Face boundaries share exact coordinates (e.g., X=1 for +X face edge)\n";
    std::cout << "2. Vertex caching might return vertices with wrong faceId\n";
    std::cout << "3. Floating-point precision causes slight misalignment\n\n";
    
    std::cout << "SOLUTION IDEAS:\n";
    std::cout << "===============\n";
    std::cout << "1. Ensure vertex cache includes faceId in the key\n";
    std::cout << "2. Slightly inset face boundaries (e.g., 0.9999 instead of 1.0)\n";
    std::cout << "3. Use separate vertex buffers per face\n";
    std::cout << "4. Fix T-junctions with proper edge morphing\n";
    
    return 0;
}