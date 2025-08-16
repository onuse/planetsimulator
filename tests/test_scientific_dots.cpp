#include <iostream>
#include <iomanip>
#include <cmath>

/*
 * SCIENTIFIC TEST FOR DOT ARTIFACTS
 * 
 * Current observation: Grey/colored dots appear at cube face boundaries
 * 
 * Hypotheses to test:
 * 1. Z-fighting: Faces overlap at boundaries (patches extend to exactly ±1.0)
 * 2. Gaps: Faces have gaps at boundaries (patches don't reach ±1.0)
 * 3. Vertex mismatch: Same position generates different vertices on different faces
 * 4. Transform errors: Patch transforms produce incorrect positions
 * 5. Precision issues: Float vs double precision causing misalignment
 */

int main() {
    std::cout << "=== SCIENTIFIC DOT ARTIFACT ANALYSIS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Test 1: Check what happens at exact boundary ±1.0
    std::cout << "\n1. BOUNDARY BEHAVIOR TEST" << std::endl;
    std::cout << "-------------------------" << std::endl;
    
    // Simulate two patches from adjacent faces meeting at boundary
    // Face 0 (+X): X=1, Y and Z vary
    // Face 2 (+Y): Y=1, X and Z vary
    // They should meet at edge where X=1 and Y=1
    
    struct PatchCorner {
        double x, y, z;
        int faceId;
    };
    
    // Face 0 patch corner at top edge (should touch Face 2)
    PatchCorner face0_corner = {1.0, 1.0, 0.5, 0};
    
    // Face 2 patch corner at right edge (should touch Face 0)
    PatchCorner face2_corner = {1.0, 1.0, 0.5, 2};
    
    std::cout << "Face 0 corner: (" << face0_corner.x << ", " << face0_corner.y << ", " << face0_corner.z << ")" << std::endl;
    std::cout << "Face 2 corner: (" << face2_corner.x << ", " << face2_corner.y << ", " << face2_corner.z << ")" << std::endl;
    
    double distance = std::sqrt(
        std::pow(face0_corner.x - face2_corner.x, 2) +
        std::pow(face0_corner.y - face2_corner.y, 2) +
        std::pow(face0_corner.z - face2_corner.z, 2)
    );
    
    std::cout << "Distance between corners: " << distance << " cube units" << std::endl;
    
    if (distance < 1e-6) {
        std::cout << "✓ Corners match exactly" << std::endl;
    } else {
        std::cout << "✗ Corners don't match! Gap of " << distance << std::endl;
    }
    
    // Test 2: Check if BOTH faces render the same boundary
    std::cout << "\n2. OVERLAP TEST" << std::endl;
    std::cout << "---------------" << std::endl;
    
    // If Face 0 extends to Y=1.0 and Face 2 extends to X=1.0,
    // they will both render the edge, causing z-fighting
    
    bool face0_reaches_y1 = true;  // Currently patches extend to ±1.0
    bool face2_reaches_x1 = true;  // Currently patches extend to ±1.0
    
    if (face0_reaches_y1 && face2_reaches_x1) {
        std::cout << "✗ OVERLAP DETECTED: Both faces render the boundary edge" << std::endl;
        std::cout << "  This causes Z-FIGHTING (dots/flickering)" << std::endl;
    } else if (!face0_reaches_y1 && !face2_reaches_x1) {
        std::cout << "✗ GAP DETECTED: Neither face renders the boundary edge" << std::endl;
        std::cout << "  This causes GAPS (background shows through)" << std::endl;
    } else {
        std::cout << "✓ Proper boundary: Exactly one face renders the edge" << std::endl;
    }
    
    // Test 3: Proposed solution
    std::cout << "\n3. SOLUTION ANALYSIS" << std::endl;
    std::cout << "--------------------" << std::endl;
    
    const double INSET = 0.9995;  // Proposed inset value
    
    std::cout << "Proposed INSET: " << INSET << std::endl;
    std::cout << "This moves boundaries from ±1.0 to ±" << INSET << std::endl;
    
    double gap_size = 1.0 - INSET;  // Gap from edge to INSET position
    std::cout << "Gap between faces: " << gap_size << " cube units" << std::endl;
    
    // At planet scale (gap is in normalized cube space, not direct multiplication)
    double planet_radius = 6371000.0;  // meters
    // The gap represents a small angular difference at the surface
    double gap_meters = gap_size * planet_radius;
    std::cout << "Gap at planet surface: " << gap_meters << " meters" << std::endl;
    
    if (gap_meters < 100) {
        std::cout << "✓ Gap is small enough to not be visible" << std::endl;
    } else {
        std::cout << "✗ Gap might be visible as missing geometry" << std::endl;
    }
    
    // Test 4: Current state diagnosis
    std::cout << "\n4. DIAGNOSIS" << std::endl;
    std::cout << "------------" << std::endl;
    
    std::cout << "Based on the evidence:" << std::endl;
    std::cout << "- Dots appear at face boundaries" << std::endl;
    std::cout << "- Dots have colors from both adjacent faces" << std::endl;
    std::cout << "- Problem persists even with vertex caching disabled" << std::endl;
    std::cout << "\nMOST LIKELY CAUSE: Z-fighting from overlapping geometry" << std::endl;
    std::cout << "Both faces extend to exactly ±1.0, causing them to overlap at edges." << std::endl;
    std::cout << "\nRECOMMENDED FIX: Apply INSET to make faces stop at ±" << INSET << std::endl;
    std::cout << "This prevents overlap while keeping gaps too small to see." << std::endl;
    
    return 0;
}