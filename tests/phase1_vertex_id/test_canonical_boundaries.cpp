// ============================================================================
// Test canonical vertex IDs at face boundaries
// This validates that vertices at face boundaries get the same ID
// regardless of which face generates them
// ============================================================================

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include "include/core/vertex_id_system.hpp"

using namespace PlanetRenderer;

void testCornerPoint() {
    std::cout << "\n=== Testing Corner Point (1,1,1) ===\n";
    
    // The corner at (1,1,1) should be accessible from 3 faces
    // +X face: u=1, v=1
    // +Y face: u=1, v=1  
    // +Z face: u=1, v=1
    
    VertexID fromX = VertexID::fromFaceUV(0, 1.0, 1.0);
    VertexID fromY = VertexID::fromFaceUV(2, 1.0, 1.0);
    VertexID fromZ = VertexID::fromFaceUV(4, 1.0, 1.0);
    
    glm::dvec3 posX = fromX.toCubePosition();
    glm::dvec3 posY = fromY.toCubePosition();
    glm::dvec3 posZ = fromZ.toCubePosition();
    
    std::cout << "From +X face: " << fromX << " -> (" 
              << posX.x << ", " << posY.y << ", " << posX.z << ")\n";
    std::cout << "From +Y face: " << fromY << " -> ("
              << posY.x << ", " << posY.y << ", " << posY.z << ")\n";
    std::cout << "From +Z face: " << fromZ << " -> ("
              << posZ.x << ", " << posZ.y << ", " << posZ.z << ")\n";
    
    if (fromX == fromY && fromY == fromZ) {
        std::cout << "✓ All three faces share the same vertex ID!\n";
    } else {
        std::cout << "✗ FAIL: Corner vertices have different IDs!\n";
    }
}

void testEdgeSharing() {
    std::cout << "\n=== Testing Edge Between +X and +Z ===\n";
    
    // Edge at x=1, z=1, y varies from -1 to 1
    // Should be shared between +X and +Z faces
    
    const int SAMPLES = 5;
    int sharedCount = 0;
    
    for (int i = 0; i <= SAMPLES; i++) {
        double t = i / double(SAMPLES);
        double y = -1.0 + 2.0 * t;
        
        // Direct position (what both faces should map to)
        glm::dvec3 edgePos(1.0, y, 1.0);
        VertexID direct = VertexID::fromCubePosition(edgePos);
        
        // From +X face: At u=1 (right edge), v maps to y
        VertexID fromX = VertexID::fromFaceUV(0, 1.0, t);
        
        // From +Z face: At u=1 (right edge), v maps to y  
        VertexID fromZ = VertexID::fromFaceUV(4, 1.0, t);
        
        if (fromX == fromZ && fromX == direct) {
            sharedCount++;
            std::cout << "  y=" << std::fixed << std::setprecision(2) << y 
                      << ": Shared ✓\n";
        } else {
            std::cout << "  y=" << y << ": NOT shared ✗\n";
            std::cout << "    +X: " << fromX << "\n";
            std::cout << "    +Z: " << fromZ << "\n";
            std::cout << "    Direct: " << direct << "\n";
        }
    }
    
    std::cout << "\nShared vertices: " << sharedCount << "/" << (SAMPLES+1) << "\n";
    if (sharedCount == SAMPLES + 1) {
        std::cout << "✓ Edge vertices are properly shared!\n";
    } else {
        std::cout << "✗ FAIL: Edge vertices are not shared correctly!\n";
    }
}

void testFaceBoundaryGaps() {
    std::cout << "\n=== Testing for Gaps at Face Boundaries ===\n";
    
    // Test patches on either side of the +X/+Z boundary
    const int RESOLUTION = 10;
    double maxGap = 0.0;
    
    for (int i = 0; i <= RESOLUTION; i++) {
        double t = i / double(RESOLUTION);
        
        // Vertex on +X face right edge
        VertexID xVertex = VertexID::fromFaceUV(0, 1.0, t);
        glm::dvec3 xPos = xVertex.toCubePosition();
        
        // Vertex on +Z face top edge (should match)
        VertexID zVertex = VertexID::fromFaceUV(4, 1.0, t);
        glm::dvec3 zPos = zVertex.toCubePosition();
        
        // Check if they're the same vertex
        if (xVertex == zVertex) {
            std::cout << "  t=" << t << ": Same vertex ID ✓\n";
        } else {
            double gap = glm::length(xPos - zPos);
            maxGap = std::max(maxGap, gap);
            std::cout << "  t=" << t << ": Different IDs, gap=" << gap << "\n";
        }
    }
    
    if (maxGap < 0.0001) {
        std::cout << "\n✓ No gaps detected at face boundaries!\n";
    } else {
        std::cout << "\n✗ FAIL: Maximum gap = " << maxGap << " units\n";
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "CANONICAL VERTEX ID BOUNDARY TESTS\n";
    std::cout << "========================================\n";
    
    testCornerPoint();
    testEdgeSharing();
    testFaceBoundaryGaps();
    
    std::cout << "\n========================================\n";
    std::cout << "TEST COMPLETE\n";
    std::cout << "========================================\n";
    
    return 0;
}