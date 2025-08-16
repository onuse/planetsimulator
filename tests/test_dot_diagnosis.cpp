#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

// Diagnostic: Why do dots persist after the cache fix?

int main() {
    std::cout << "========================================================\n";
    std::cout << "         DOT ARTIFACT ROOT CAUSE ANALYSIS\n";
    std::cout << "========================================================\n\n";
    
    std::cout << "SYMPTOMS:\n";
    std::cout << "---------\n";
    std::cout << "1. Pink/red dots (Face 0) appearing on blue face (Face 4)\n";
    std::cout << "2. Regular grid pattern suggests actual vertices\n";
    std::cout << "3. Dots persist despite faceId cache fix\n";
    std::cout << "4. Only appears at face boundaries\n\n";
    
    std::cout << "HYPOTHESIS 1: Overlapping Patches\n";
    std::cout << "----------------------------------\n";
    std::cout << "Different faces might generate patches that overlap in 3D space.\n";
    std::cout << "Example: Face 0 (+X) patch at boundary x=1.0, y in [-1,1], z in [-1,1]\n";
    std::cout << "         Face 4 (+Z) patch at boundary z=1.0, x in [-1,1], y in [-1,1]\n";
    std::cout << "These overlap at edges/corners!\n\n";
    
    std::cout << "TEST: Check if Face 0 and Face 4 generate vertices at same 3D positions\n";
    
    // Simulate Face 0 (+X) boundary vertices
    std::cout << "\nFace 0 (+X) boundary vertices (x=1.0):\n";
    for (int i = 0; i < 3; i++) {
        double y = -1.0 + i * 1.0;  // -1, 0, 1
        double z = 1.0;  // At +Z boundary
        std::cout << "  Vertex: (1.0, " << y << ", " << z << ")\n";
    }
    
    // Simulate Face 4 (+Z) boundary vertices
    std::cout << "\nFace 4 (+Z) boundary vertices (z=1.0):\n";
    for (int i = 0; i < 3; i++) {
        double x = 1.0;  // At +X boundary
        double y = -1.0 + i * 1.0;  // -1, 0, 1
        std::cout << "  Vertex: (" << x << ", " << y << ", 1.0)\n";
    }
    
    std::cout << "\n=> SAME 3D POSITIONS! Both faces generate vertices at cube edges.\n\n";
    
    std::cout << "HYPOTHESIS 2: Vertex Buffer Layout\n";
    std::cout << "-----------------------------------\n";
    std::cout << "All faces' vertices go into ONE big buffer:\n";
    std::cout << "  [Face0 vertices][Face1 vertices]...[Face5 vertices]\n";
    std::cout << "  [0...785849]    [785850...N]      [...]\n";
    std::cout << "\nIf Face 4's indices accidentally reference Face 0's range,\n";
    std::cout << "we'd see Face 0 vertices (with faceId=0) rendered by Face 4.\n\n";
    
    std::cout << "HYPOTHESIS 3: Z-Fighting\n";
    std::cout << "------------------------\n";
    std::cout << "If patches from different faces occupy same 3D space,\n";
    std::cout << "Z-buffer precision determines which is visible.\n";
    std::cout << "Result: Random dots from 'losing' face show through.\n\n";
    
    std::cout << "HYPOTHESIS 4: Cache Key Collision\n";
    std::cout << "----------------------------------\n";
    std::cout << "Current cache key includes faceId, BUT:\n";
    std::cout << "- Face 0 vertex at (1.0, 0.5, 1.0) with faceId=0\n";
    std::cout << "- Face 4 vertex at (1.0, 0.5, 1.0) with faceId=4\n";
    std::cout << "These have DIFFERENT keys, so no cache sharing.\n";
    std::cout << "Both vertices exist in buffer, both get rendered!\n\n";
    
    std::cout << "ROOT CAUSE THEORY:\n";
    std::cout << "==================\n";
    std::cout << "The dots are NOT from cache contamination.\n";
    std::cout << "They're from DUPLICATE VERTICES at same 3D positions:\n";
    std::cout << "1. Face 0 generates vertex at edge (1.0, y, 1.0) with faceId=0\n";
    std::cout << "2. Face 4 generates vertex at edge (1.0, y, 1.0) with faceId=4\n";
    std::cout << "3. Both vertices exist in the buffer\n";
    std::cout << "4. Both patches render their triangles\n";
    std::cout << "5. Z-fighting causes Face 0's pink to show through Face 4's blue\n\n";
    
    std::cout << "SOLUTION OPTIONS:\n";
    std::cout << "=================\n";
    std::cout << "1. PREVENT OVERLAP: Don't generate patches at face boundaries\n";
    std::cout << "   - Shrink patch bounds slightly (e.g., 0.999 instead of 1.0)\n";
    std::cout << "2. CULL DUPLICATE GEOMETRY: Skip patches that overlap other faces\n";
    std::cout << "3. SHARE VERTICES: Use global vertex pool, not per-face\n";
    std::cout << "4. DEPTH OFFSET: Apply small depth bias per face to avoid z-fighting\n\n";
    
    std::cout << "RECOMMENDED FIX:\n";
    std::cout << "================\n";
    std::cout << "Modify patch generation to slightly inset from face boundaries.\n";
    std::cout << "This prevents geometric overlap while maintaining visual continuity.\n";
    
    return 0;
}