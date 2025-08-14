#include <iostream>
#include <vector>
#include <set>
#include <glm/glm.hpp>
#include "../include/core/vertex_id_system.hpp"

using namespace PlanetRenderer;

// Test result tracking
int testsRun = 0;
int testsPassed = 0;

#define TEST(name) void test_##name(); \
    struct TestRunner_##name { \
        TestRunner_##name() { \
            std::cout << "Running: " #name << "... "; \
            testsRun++; \
            test_##name(); \
            testsPassed++; \
            std::cout << "PASSED\n"; \
        } \
    } runner_##name; \
    void test_##name()

#define ASSERT(condition) \
    if (!(condition)) { \
        std::cout << "FAILED\n"; \
        std::cout << "  Assertion failed: " #condition "\n"; \
        std::cout << "  At line " << __LINE__ << "\n"; \
        exit(1); \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cout << "FAILED\n"; \
        std::cout << "  Expected: " << (a) << " == " << (b) << "\n"; \
        std::cout << "  At line " << __LINE__ << "\n"; \
        exit(1); \
    }

// ============================================================================
// Test 1: Basic VertexID creation and equality
// ============================================================================
TEST(BasicVertexID) {
    glm::dvec3 pos1(0.5, 0.5, 0.5);
    glm::dvec3 pos2(0.5, 0.5, 0.5);
    glm::dvec3 pos3(0.5, 0.5, 0.501);  // Different position (resolution: 0.0001)
    
    VertexID id1 = VertexID::fromCubePosition(pos1);
    VertexID id2 = VertexID::fromCubePosition(pos2);
    VertexID id3 = VertexID::fromCubePosition(pos3);
    
    ASSERT_EQ(id1, id2);  // Same position should give same ID
    ASSERT(id1 != id3);   // Different positions should give different IDs
}

// ============================================================================
// Test 2: Cube corner vertices have same ID from all 3 faces
// ============================================================================
TEST(CornerVertexSharing) {
    // The corner at (1, 1, 1) should have the same ID from all 3 faces
    
    // From +X face (face 0): u=1, v=1 maps to (1, 1, 1)
    VertexID fromX = VertexID::fromFaceUV(0, 1.0, 1.0);
    
    // From +Y face (face 2): u=1, v=1 maps to (1, 1, 1)
    VertexID fromY = VertexID::fromFaceUV(2, 1.0, 1.0);
    
    // From +Z face (face 4): u=1, v=1 maps to (1, 1, 1)
    VertexID fromZ = VertexID::fromFaceUV(4, 1.0, 1.0);
    
    // All three should be equal
    ASSERT_EQ(fromX, fromY);
    ASSERT_EQ(fromY, fromZ);
    ASSERT_EQ(fromX, fromZ);
    
    // And they should all be identified as corner vertices
    ASSERT(fromX.isOnCorner());
    ASSERT(fromY.isOnCorner());
    ASSERT(fromZ.isOnCorner());
}

// ============================================================================
// Test 3: Edge vertices are shared between exactly 2 faces
// ============================================================================
TEST(EdgeVertexSharing) {
    // Test edge between +X and +Z faces (at x=1, z=1)
    
    const int SAMPLES = 10;
    for (int i = 1; i < SAMPLES; i++) {  // Skip corners (i=0 and i=SAMPLES)
        double t = i / double(SAMPLES);
        
        // Create the same edge vertex from both faces
        glm::dvec3 edgePos(1.0, -1.0 + 2.0 * t, 1.0);
        
        VertexID id1 = VertexID::fromCubePosition(edgePos);
        VertexID id2 = VertexID::fromCubePosition(edgePos);  // Should be identical
        
        ASSERT_EQ(id1, id2);
        ASSERT(id1.isOnEdge());
        ASSERT(!id1.isOnCorner());
    }
}

// ============================================================================
// Test 4: Position encoding is reversible
// ============================================================================
TEST(PositionEncodingReversible) {
    std::vector<glm::dvec3> testPositions = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.5, 0.5, 0.5),
        glm::dvec3(-1.0, -1.0, -1.0),
        glm::dvec3(0.12345, -0.67890, 0.98765)
    };
    
    for (const auto& pos : testPositions) {
        VertexID id = VertexID::fromCubePosition(pos);
        glm::dvec3 decoded = id.toCubePosition();
        
        // Check that decoded position is very close to original
        double error = glm::length(decoded - pos);
        ASSERT(error < 1e-3);  // Should be within quantization error
    }
}

// ============================================================================
// Test 5: Face boundary detection works correctly
// ============================================================================
TEST(BoundaryDetection) {
    // Interior vertex
    VertexID interior = VertexID::fromCubePosition(glm::dvec3(0.5, 0.5, 0.5));
    ASSERT(!interior.isOnFaceBoundary());
    ASSERT(!interior.isOnEdge());
    ASSERT(!interior.isOnCorner());
    
    // Face boundary (but not edge)
    VertexID boundary = VertexID::fromCubePosition(glm::dvec3(1.0, 0.5, 0.5));
    ASSERT(boundary.isOnFaceBoundary());
    ASSERT(!boundary.isOnEdge());
    ASSERT(!boundary.isOnCorner());
    
    // Edge (2 boundaries)
    VertexID edge = VertexID::fromCubePosition(glm::dvec3(1.0, 1.0, 0.5));
    ASSERT(edge.isOnFaceBoundary());
    ASSERT(edge.isOnEdge());
    ASSERT(!edge.isOnCorner());
    
    // Corner (3 boundaries)
    VertexID corner = VertexID::fromCubePosition(glm::dvec3(1.0, 1.0, 1.0));
    ASSERT(corner.isOnFaceBoundary());
    ASSERT(corner.isOnEdge());
    ASSERT(corner.isOnCorner());
}

// ============================================================================
// Test 6: Adjacent patches share edge vertices
// ============================================================================
TEST(AdjacentPatchEdgeSharing) {
    // Two patches on the same face should share edge vertices
    const int EDGE_RESOLUTION = 5;
    
    std::set<VertexID> patch1Vertices;
    std::set<VertexID> patch2Vertices;
    
    // Patch 1: Center at (0.25, 0.5) on +Z face
    for (int i = 0; i <= EDGE_RESOLUTION; i++) {
        double v = i / double(EDGE_RESOLUTION);
        // Right edge of patch 1
        glm::dvec3 pos(0.5, -0.5 + v, 1.0);
        patch1Vertices.insert(VertexID::fromCubePosition(pos));
    }
    
    // Patch 2: Center at (0.75, 0.5) on +Z face  
    for (int i = 0; i <= EDGE_RESOLUTION; i++) {
        double v = i / double(EDGE_RESOLUTION);
        // Left edge of patch 2
        glm::dvec3 pos(0.5, -0.5 + v, 1.0);
        patch2Vertices.insert(VertexID::fromCubePosition(pos));
    }
    
    // Count shared vertices
    int sharedCount = 0;
    for (const auto& id : patch1Vertices) {
        if (patch2Vertices.count(id)) {
            sharedCount++;
        }
    }
    
    // All edge vertices should be shared
    ASSERT_EQ(sharedCount, EDGE_RESOLUTION + 1);
}

// ============================================================================
// Test 7: Hash function distributes well
// ============================================================================
TEST(HashDistribution) {
    std::set<size_t> hashes;
    
    // Generate many vertex IDs
    const int SAMPLES = 100;
    for (int x = 0; x < SAMPLES; x++) {
        for (int y = 0; y < SAMPLES; y++) {
            double fx = (x / double(SAMPLES)) * 2.0 - 1.0;
            double fy = (y / double(SAMPLES)) * 2.0 - 1.0;
            
            VertexID id = VertexID::fromCubePosition(glm::dvec3(fx, fy, 1.0));
            size_t hash = std::hash<VertexID>{}(id);
            hashes.insert(hash);
        }
    }
    
    // Should have unique hashes for unique positions
    ASSERT_EQ(hashes.size(), SAMPLES * SAMPLES);
}

// ============================================================================
// Test 8: EdgeID works correctly
// ============================================================================
TEST(EdgeIDFunctionality) {
    VertexID v1 = VertexID::fromCubePosition(glm::dvec3(0.0, 0.0, 0.0));
    VertexID v2 = VertexID::fromCubePosition(glm::dvec3(1.0, 0.0, 0.0));
    VertexID v3 = VertexID::fromCubePosition(glm::dvec3(0.0, 1.0, 0.0));
    
    // EdgeID should be order-independent
    EdgeID edge1(v1, v2);
    EdgeID edge2(v2, v1);  // Reversed order
    ASSERT_EQ(edge1, edge2);
    
    // Different edges should be different
    EdgeID edge3(v1, v3);
    ASSERT(edge1 != edge3);
}

// ============================================================================
// Main test runner
// ============================================================================
int main() {
    std::cout << "=== Vertex Identity System Tests ===\n\n";
    
    // Tests run automatically via static initialization
    
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Tests run: " << testsRun << "\n";
    std::cout << "Tests passed: " << testsPassed << "\n";
    
    if (testsPassed == testsRun) {
        std::cout << "\nALL TESTS PASSED ✓\n";
        return 0;
    } else {
        std::cout << "\nSOME TESTS FAILED ✗\n";
        return 1;
    }
}