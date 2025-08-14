#include "core/vertex_id_system.hpp"
#include <iostream>
#include <iomanip>

namespace PlanetRenderer {

// ============================================================================
// Canonical ID Resolution
// 
// This is where we handle the special cases for vertices that appear on
// multiple faces. The canonical ID ensures that the same 3D position
// always gets the same ID, regardless of which face requests it.
// ============================================================================

VertexID makeCanonical(const VertexID& id) {
    // For now, the VertexID::fromCubePosition already creates canonical IDs
    // by using the 3D position directly. This function is here for:
    // 1. Future optimizations (caching, lookup tables)
    // 2. Special case handling if needed
    // 3. Debugging and validation
    
    return id;  // Already canonical due to position-based encoding
}

// ============================================================================
// Debugging utilities
// ============================================================================

void printVertexID(const VertexID& id, const std::string& label) {
    glm::dvec3 pos = id.toCubePosition();
    std::cout << label << ": ID=" << std::hex << id.raw() << std::dec
              << " Pos=(" << std::fixed << std::setprecision(4)
              << pos.x << ", " << pos.y << ", " << pos.z << ")";
    
    if (id.isOnCorner()) {
        std::cout << " [CORNER]";
    } else if (id.isOnEdge()) {
        std::cout << " [EDGE]";
    } else if (id.isOnFaceBoundary()) {
        std::cout << " [BOUNDARY]";
    }
    
    std::cout << std::endl;
}

// ============================================================================
// Validation utilities
// ============================================================================

bool validateVertexSharing(int face1, int face2) {
    // Test that vertices on shared boundaries have the same IDs
    const int SAMPLES = 10;
    int sharedCount = 0;
    int totalChecked = 0;
    
    for (int i = 0; i <= SAMPLES; i++) {
        double t = i / double(SAMPLES);
        
        // Generate vertices that should be shared
        VertexID id1, id2;
        
        // This depends on which faces we're checking
        // For example, +X and +Z share an edge at x=1, z=1
        if (face1 == 0 && face2 == 4) {  // +X and +Z
            // +X face: u varies along Z, v varies along Y
            id1 = VertexID::fromFaceUV(face1, 1.0, t);  
            // +Z face: u varies along X, v varies along Y
            id2 = VertexID::fromFaceUV(face2, 1.0, t);
        }
        // Add more face pairs as needed...
        
        totalChecked++;
        if (id1 == id2) {
            sharedCount++;
        }
    }
    
    std::cout << "Face " << face1 << " <-> Face " << face2 
              << ": " << sharedCount << "/" << totalChecked << " vertices shared"
              << std::endl;
    
    return sharedCount > 0;
}

} // namespace PlanetRenderer