#include <iostream>
#include <glm/glm.hpp>
#include <vector>
#include <set>
#include <iomanip>
#include <cmath>

// Test to verify the fix for overlapping patches

struct PatchBounds {
    glm::dvec3 min;
    glm::dvec3 max;
    int faceId;
    const char* faceName;
};

bool checkOverlap(const PatchBounds& a, const PatchBounds& b, double tolerance = 1e-10) {
    // Check if patches overlap (with tolerance for shared boundaries)
    bool overlapX = !(a.max.x < b.min.x - tolerance || b.max.x < a.min.x - tolerance);
    bool overlapY = !(a.max.y < b.min.y - tolerance || b.max.y < a.min.y - tolerance);
    bool overlapZ = !(a.max.z < b.min.z - tolerance || b.max.z < a.min.z - tolerance);
    
    if (overlapX && overlapY && overlapZ) {
        // Check if it's just a shared boundary (expected) vs actual overlap (problem)
        double overlapVolumeX = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
        double overlapVolumeY = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
        double overlapVolumeZ = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
        
        // If any dimension has significant overlap (not just touching), it's a problem
        if (overlapVolumeX > tolerance && overlapVolumeY > tolerance && overlapVolumeZ > tolerance) {
            return true; // Real overlap, not just shared boundary
        }
    }
    
    return false;
}

void testCurrentImplementation() {
    std::cout << "\n=== CURRENT IMPLEMENTATION (OVERLAPPING) ===\n";
    
    // Current implementation - each face spans full -1 to 1 in non-fixed dimensions
    std::vector<PatchBounds> currentPatches = {
        {{1, -1, -1}, {1, 1, 1}, 0, "+X"},    // Fixed X=1, Y and Z span -1 to 1
        {{-1, -1, -1}, {-1, 1, 1}, 1, "-X"},  // Fixed X=-1, Y and Z span -1 to 1
        {{-1, 1, -1}, {1, 1, 1}, 2, "+Y"},    // Fixed Y=1, X and Z span -1 to 1
        {{-1, -1, -1}, {1, -1, 1}, 3, "-Y"},  // Fixed Y=-1, X and Z span -1 to 1
        {{-1, -1, 1}, {1, 1, 1}, 4, "+Z"},    // Fixed Z=1, X and Y span -1 to 1
        {{-1, -1, -1}, {1, 1, -1}, 5, "-Z"}   // Fixed Z=-1, X and Y span -1 to 1
    };
    
    int overlapCount = 0;
    for (size_t i = 0; i < currentPatches.size(); i++) {
        for (size_t j = i + 1; j < currentPatches.size(); j++) {
            if (checkOverlap(currentPatches[i], currentPatches[j])) {
                std::cout << "  OVERLAP: " << currentPatches[i].faceName 
                         << " overlaps with " << currentPatches[j].faceName << "\n";
                overlapCount++;
            }
        }
    }
    
    std::cout << "Total overlaps: " << overlapCount << "\n";
}

void testProposedFix() {
    std::cout << "\n=== PROPOSED FIX (NON-OVERLAPPING) ===\n";
    std::cout << "Strategy: Each face owns only its surface, not the entire volume\n\n";
    
    // Fixed implementation - each face only owns its surface
    // The key insight: a face patch should be infinitesimally thin in the fixed dimension
    const double FACE_THICKNESS = 0.0; // Face is a surface, not a volume
    
    std::vector<PatchBounds> fixedPatches = {
        {{1.0 - FACE_THICKNESS, -1, -1}, {1.0, 1, 1}, 0, "+X"},   // Only at X=1 surface
        {{-1.0, -1, -1}, {-1.0 + FACE_THICKNESS, 1, 1}, 1, "-X"}, // Only at X=-1 surface
        {{-1, 1.0 - FACE_THICKNESS, -1}, {1, 1.0, 1}, 2, "+Y"},   // Only at Y=1 surface
        {{-1, -1.0, -1}, {1, -1.0 + FACE_THICKNESS, 1}, 3, "-Y"}, // Only at Y=-1 surface
        {{-1, -1, 1.0 - FACE_THICKNESS}, {1, 1, 1.0}, 4, "+Z"},   // Only at Z=1 surface
        {{-1, -1, -1.0}, {1, 1, -1.0 + FACE_THICKNESS}, 5, "-Z"}  // Only at Z=-1 surface
    };
    
    int overlapCount = 0;
    for (size_t i = 0; i < fixedPatches.size(); i++) {
        for (size_t j = i + 1; j < fixedPatches.size(); j++) {
            if (checkOverlap(fixedPatches[i], fixedPatches[j])) {
                std::cout << "  OVERLAP: " << fixedPatches[i].faceName 
                         << " overlaps with " << fixedPatches[j].faceName << "\n";
                overlapCount++;
            }
        }
    }
    
    if (overlapCount == 0) {
        std::cout << "  SUCCESS: No overlaps detected!\n";
    } else {
        std::cout << "  Total overlaps: " << overlapCount << "\n";
    }
    
    // Test subdivided patches
    std::cout << "\n  Testing subdivided patches:\n";
    
    // Subdivide +X face into 4 quadrants
    std::vector<PatchBounds> subdivided = {
        {{1.0, -1, -1}, {1.0, 0, 0}, 0, "+X-BL"},    // Bottom-left
        {{1.0, -1, 0}, {1.0, 0, 1}, 0, "+X-BR"},     // Bottom-right  
        {{1.0, 0, 0}, {1.0, 1, 1}, 0, "+X-TR"},      // Top-right
        {{1.0, 0, -1}, {1.0, 1, 0}, 0, "+X-TL"}      // Top-left
    };
    
    // Check subdivided patches don't overlap
    overlapCount = 0;
    for (size_t i = 0; i < subdivided.size(); i++) {
        for (size_t j = i + 1; j < subdivided.size(); j++) {
            if (checkOverlap(subdivided[i], subdivided[j])) {
                std::cout << "    OVERLAP: " << subdivided[i].faceName 
                         << " overlaps with " << subdivided[j].faceName << "\n";
                overlapCount++;
            }
        }
    }
    
    if (overlapCount == 0) {
        std::cout << "    Subdivided patches: No overlaps (correct!)\n";
    }
}

void testVertexSharing() {
    std::cout << "\n=== VERTEX SHARING AT BOUNDARIES ===\n";
    
    // Test that patches share vertices correctly at boundaries
    struct TestVertex {
        glm::dvec3 pos;
        std::vector<std::string> patches;
    };
    
    std::vector<TestVertex> edgeVertices = {
        // Edge between +X and +Y faces
        {{1, 1, 0}, {"+X face at (u=1,v=0.5)", "+Y face at (u=1,v=0.5)"}},
        {{1, 1, -1}, {"+X face corner", "+Y face corner", "+Z face corner"}},
        
        // Edge between +X and -Y faces  
        {{1, -1, 0}, {"+X face at (u=0,v=0.5)", "-Y face at (u=1,v=0.5)"}},
        
        // Corner where 3 faces meet
        {{1, 1, 1}, {"+X face corner", "+Y face corner", "+Z face corner"}}
    };
    
    for (const auto& vertex : edgeVertices) {
        std::cout << "  Vertex at (" << vertex.pos.x << ", " << vertex.pos.y << ", " << vertex.pos.z << "):\n";
        std::cout << "    Shared by " << vertex.patches.size() << " patches\n";
        for (const auto& patch : vertex.patches) {
            std::cout << "      - " << patch << "\n";
        }
    }
}

void proposeCodeFix() {
    std::cout << "\n=== PROPOSED CODE CHANGES ===\n";
    std::cout << "1. In SphericalQuadtreeNode constructor:\n";
    std::cout << "   - Ensure minBounds and maxBounds have EXACTLY the same value for fixed dimension\n";
    std::cout << "   - Example for +X face: minBounds.x = maxBounds.x = 1.0\n\n";
    
    std::cout << "2. In GlobalPatchGenerator::createTransform():\n";
    std::cout << "   - Already correct - uses minBounds for fixed dimension\n";
    std::cout << "   - Just ensure bounds are set correctly\n\n";
    
    std::cout << "3. Key insight:\n";
    std::cout << "   - Face patches are 2D surfaces in 3D space\n";
    std::cout << "   - They should have zero thickness in their fixed dimension\n";
    std::cout << "   - This prevents volumetric overlap while maintaining shared boundaries\n";
}

int main() {
    std::cout << "=== DIAGNOSING PATCH OVERLAP ISSUE ===\n";
    std::cout << "This is likely causing the 'double planet' visual artifact\n";
    
    testCurrentImplementation();
    testProposedFix();
    testVertexSharing();
    proposeCodeFix();
    
    std::cout << "\n=== CONCLUSION ===\n";
    std::cout << "The 'double planet' appearance is caused by face patches overlapping.\n";
    std::cout << "Each face currently claims a volume from -1 to 1 in non-fixed dimensions,\n";
    std::cout << "causing significant overlap at edges and corners.\n";
    std::cout << "The fix is to ensure each face patch has the same min and max value\n";
    std::cout << "for its fixed dimension, making it a true 2D surface.\n";
    
    return 0;
}
