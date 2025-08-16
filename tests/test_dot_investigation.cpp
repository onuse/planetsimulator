#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <iomanip>
#include <algorithm>

// Scientific investigation of the persistent dot artifacts

int main() {
    std::cout << "==========================================\n";
    std::cout << "   SCIENTIFIC DOT ARTIFACT INVESTIGATION\n";
    std::cout << "==========================================\n\n";
    
    std::cout << "OBSERVATIONS FROM SCREENSHOTS:\n";
    std::cout << "------------------------------\n";
    std::cout << "1. Dots appear in a regular grid pattern\n";
    std::cout << "2. Dots are colored differently than their surrounding face\n";
    std::cout << "3. Dots persist despite:\n";
    std::cout << "   - Fixing winding order (X-faces now render)\n";
    std::cout << "   - Adding faceId to vertex cache key\n";
    std::cout << "   - Increasing face boundary inset (0.9999 -> 0.9995)\n";
    std::cout << "4. Pattern suggests vertices, not random pixels\n\n";
    
    std::cout << "HYPOTHESIS TESTING:\n";
    std::cout << "===================\n\n";
    
    std::cout << "HYPOTHESIS 1: Inset didn't actually change\n";
    std::cout << "-------------------------------------------\n";
    std::cout << "Test: Check if 0.9995 is actually being used\n";
    std::cout << "Expected: Patches should generate from -0.9995 to 0.9995\n";
    std::cout << "Actual: Need to verify in patch generation\n\n";
    
    std::cout << "HYPOTHESIS 2: Dots are from LOD transitions\n";
    std::cout << "--------------------------------------------\n";
    std::cout << "Different LOD levels meeting might create artifacts\n";
    std::cout << "Test: Check if dots appear at patch boundaries\n";
    std::cout << "Pattern analysis: Are dots at regular 1/16, 1/32 intervals?\n\n";
    
    std::cout << "HYPOTHESIS 3: Vertex generation precision issue\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << "Vertices might be generated at slightly different positions\n";
    std::cout << "Test: Log exact vertex positions at boundaries\n";
    std::cout << "Check: Are boundary vertices EXACTLY at ±0.9995?\n\n";
    
    std::cout << "HYPOTHESIS 4: Shader interpolation artifacts\n";
    std::cout << "--------------------------------------------\n";
    std::cout << "The fragFaceId might be interpolated incorrectly\n";
    std::cout << "Test: Use flat interpolation qualifier\n";
    std::cout << "Check: Is fragFaceId already marked as 'flat'?\n\n";
    
    std::cout << "HYPOTHESIS 5: Multiple draw calls overlapping\n";
    std::cout << "---------------------------------------------\n";
    std::cout << "Each face might be drawn separately, overlapping\n";
    std::cout << "Test: Count draw calls per frame\n";
    std::cout << "Check: Are we drawing 186 patches in one call or multiple?\n\n";
    
    std::cout << "CRITICAL QUESTIONS:\n";
    std::cout << "===================\n";
    std::cout << "1. Are the dots at EXACT grid positions (e.g., every 64th vertex)?\n";
    std::cout << "2. Do dots appear ONLY at boundaries or also in face interiors?\n";
    std::cout << "3. Are dots from the PREVIOUS face or NEXT face in render order?\n";
    std::cout << "4. Do dots move when camera moves (suggesting vertex issue)?\n";
    std::cout << "5. Are there exactly 65x65 vertices per patch (gridResolution)?\n\n";
    
    std::cout << "MEASUREMENTS NEEDED:\n";
    std::cout << "====================\n";
    std::cout << "1. Count exact number of dots visible\n";
    std::cout << "2. Measure spacing between dots (in vertices)\n";
    std::cout << "3. Identify which face's color each dot shows\n";
    std::cout << "4. Check if pattern is same on all faces\n\n";
    
    std::cout << "TEST PLAN:\n";
    std::cout << "==========\n";
    std::cout << "1. Add logging to show actual patch bounds being generated\n";
    std::cout << "2. Log vertex positions at x=0.9995, y=0.9995, etc.\n";
    std::cout << "3. Count vertices that have faceId != expected face\n";
    std::cout << "4. Temporarily disable vertex caching entirely\n";
    std::cout << "5. Render only one face at a time\n\n";
    
    std::cout << "PATTERN ANALYSIS:\n";
    std::cout << "=================\n";
    
    // Analyze the dot pattern
    const int GRID_RES = 65;  // Vertices per patch edge
    const int PATCHES_PER_FACE = 16;  // Approximate at this LOD
    
    std::cout << "If gridResolution = " << GRID_RES << ":\n";
    std::cout << "  - Each patch has " << (GRID_RES * GRID_RES) << " vertices\n";
    std::cout << "  - Boundary vertices: " << (GRID_RES * 4 - 4) << " per patch\n";
    std::cout << "  - If dots are every 8th vertex: " << (GRID_RES / 8) << " dots per edge\n";
    std::cout << "  - If dots are every 16th vertex: " << (GRID_RES / 16) << " dots per edge\n\n";
    
    std::cout << "Visual pattern suggests dots are approximately every:\n";
    std::cout << "  8-16 vertices apart (rough estimate from screenshot)\n\n";
    
    std::cout << "MOST LIKELY CAUSE:\n";
    std::cout << "==================\n";
    std::cout << "The dots are likely vertices that:\n";
    std::cout << "1. Are being generated multiple times (once per face)\n";
    std::cout << "2. Have slightly different positions due to floating point\n";
    std::cout << "3. Are z-fighting even with the inset\n";
    std::cout << "OR\n";
    std::cout << "4. Are sampling/interpolating faceId incorrectly\n";
    std::cout << "5. Have indices pointing to wrong vertices\n\n";
    
    std::cout << "IMMEDIATE ACTION:\n";
    std::cout << "=================\n";
    std::cout << "1. Verify the inset is actually applied\n";
    std::cout << "2. Log boundary vertex generation\n";
    std::cout << "3. Test with vertex cache disabled\n";
    std::cout << "4. Check if rendering single face eliminates dots\n";
    
    return 0;
}