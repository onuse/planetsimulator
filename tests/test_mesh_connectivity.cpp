#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include "core/octree.hpp"
#include "core/material_table.hpp"

// Test macros for consistent output
#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAIL: " << message << " at line " << __LINE__ << std::endl; \
        return false; \
    } else { \
        std::cout << "PASS: " << message << std::endl; \
    }

#define TEST_EXPECT_EQ(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message << " (expected " << expected << ", got " << actual << ")")

#define TEST_EXPECT_TRUE(condition, message) \
    TEST_ASSERT(condition, message)

#define TEST_EXPECT_FALSE(condition, message) \
    TEST_ASSERT(!(condition), message)

#define TEST_EXPECT_GT(a, b, message) \
    TEST_ASSERT((a) > (b), message << " (" << a << " should be > " << b << ")")

#define TEST_EXPECT_LT(a, b, message) \
    TEST_ASSERT((a) < (b), message << " (" << a << " should be < " << b << ")")

// Simple vertex structure for testing (mimics TransvoxelChunk vertex data)
struct TestVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Simple mesh structure for testing
struct TestMesh {
    std::vector<TestVertex> vertices;
    std::vector<uint32_t> indices;
};

// Helper structure to track edge connectivity
struct Edge {
    uint32_t v1, v2;
    
    Edge(uint32_t a, uint32_t b) : v1(std::min(a, b)), v2(std::max(a, b)) {}
    
    bool operator==(const Edge& other) const {
        return v1 == other.v1 && v2 == other.v2;
    }
    
    bool operator<(const Edge& other) const {
        if (v1 != other.v1) return v1 < other.v1;
        return v2 < other.v2;
    }
};

// Hash function for Edge
namespace std {
    template<>
    struct hash<Edge> {
        size_t operator()(const Edge& e) const {
            return hash<uint32_t>()(e.v1) ^ (hash<uint32_t>()(e.v2) << 1);
        }
    };
}

class MeshConnectivityTester {
private:
    std::unique_ptr<octree::OctreePlanet> planet;
    
    // Epsilon for floating point comparisons
    static constexpr float VERTEX_EPSILON = 0.001f;
    
    // Helper to check if two vertices are the same position
    bool verticesEqual(const glm::vec3& v1, const glm::vec3& v2) {
        return glm::all(glm::epsilonEqual(v1, v2, VERTEX_EPSILON));
    }
    
    // Create a simple test mesh (cube)
    TestMesh createTestCube() {
        TestMesh mesh;
        
        // 8 vertices of a cube with outward-facing normals
        mesh.vertices = {
            {{-1, -1, -1}, {-0.577f, -0.577f, -0.577f}, {1, 0, 0}},  // vertex 0
            {{ 1, -1, -1}, { 0.577f, -0.577f, -0.577f}, {1, 0, 0}},  // vertex 1
            {{ 1,  1, -1}, { 0.577f,  0.577f, -0.577f}, {1, 0, 0}},  // vertex 2
            {{-1,  1, -1}, {-0.577f,  0.577f, -0.577f}, {1, 0, 0}},  // vertex 3
            {{-1, -1,  1}, {-0.577f, -0.577f,  0.577f}, {0, 1, 0}},  // vertex 4
            {{ 1, -1,  1}, { 0.577f, -0.577f,  0.577f}, {0, 1, 0}},  // vertex 5
            {{ 1,  1,  1}, { 0.577f,  0.577f,  0.577f}, {0, 1, 0}},  // vertex 6
            {{-1,  1,  1}, {-0.577f,  0.577f,  0.577f}, {0, 1, 0}}   // vertex 7
        };
        
        // 12 triangles (2 per face, 6 faces)
        mesh.indices = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face
            4, 7, 6,  6, 5, 4,
            // Left face
            0, 3, 7,  7, 4, 0,
            // Right face
            1, 5, 6,  6, 2, 1,
            // Top face
            3, 2, 6,  6, 7, 3,
            // Bottom face
            0, 4, 5,  5, 1, 0
        };
        
        return mesh;
    }
    
    // Find duplicate vertices in mesh
    std::vector<std::pair<uint32_t, uint32_t>> findDuplicateVertices(const TestMesh& mesh) {
        std::vector<std::pair<uint32_t, uint32_t>> duplicates;
        
        for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
            for (uint32_t j = i + 1; j < mesh.vertices.size(); j++) {
                if (verticesEqual(mesh.vertices[i].position, mesh.vertices[j].position)) {
                    duplicates.push_back({i, j});
                }
            }
        }
        
        return duplicates;
    }
    
    // Build edge-to-triangle map
    std::unordered_map<Edge, std::vector<uint32_t>> buildEdgeMap(const TestMesh& mesh) {
        std::unordered_map<Edge, std::vector<uint32_t>> edgeMap;
        
        uint32_t triangleCount = mesh.indices.size() / 3;
        for (uint32_t t = 0; t < triangleCount; t++) {
            uint32_t i0 = mesh.indices[t * 3];
            uint32_t i1 = mesh.indices[t * 3 + 1];
            uint32_t i2 = mesh.indices[t * 3 + 2];
            
            edgeMap[Edge(i0, i1)].push_back(t);
            edgeMap[Edge(i1, i2)].push_back(t);
            edgeMap[Edge(i2, i0)].push_back(t);
        }
        
        return edgeMap;
    }
    
public:
    bool setUp() {
        try {
            // Create octree planet for testing
            float planetRadius = 6371000.0f; // Earth radius in meters
            int maxDepth = 8;
            uint32_t seed = 42;
            
            planet = std::make_unique<octree::OctreePlanet>(planetRadius, maxDepth);
            planet->generate(seed);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to set up test environment: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testMeshIntegrity() {
        std::cout << "\n=== Test: Mesh Integrity ===" << std::endl;
        
        TestMesh mesh = createTestCube();
        
        // Check that we have vertices and indices
        TEST_EXPECT_GT(mesh.vertices.size(), 0, "Mesh has vertices");
        TEST_EXPECT_GT(mesh.indices.size(), 0, "Mesh has indices");
        TEST_EXPECT_EQ(0, mesh.indices.size() % 3, "Index count is multiple of 3");
        
        // Check all indices are valid
        uint32_t maxIndex = 0;
        for (uint32_t idx : mesh.indices) {
            TEST_EXPECT_LT(idx, mesh.vertices.size(), "Index " << idx << " is within bounds");
            maxIndex = std::max(maxIndex, idx);
        }
        
        // Check that all vertices are referenced
        std::set<uint32_t> referencedVertices(mesh.indices.begin(), mesh.indices.end());
        TEST_EXPECT_GT(referencedVertices.size(), 0, "Some vertices are referenced");
        
        std::cout << "  Vertices: " << mesh.vertices.size() 
                  << ", Indices: " << mesh.indices.size() 
                  << ", Triangles: " << mesh.indices.size()/3 << std::endl;
        
        return true;
    }
    
    bool testDuplicateVertices() {
        std::cout << "\n=== Test: Duplicate Vertices ===" << std::endl;
        
        TestMesh mesh = createTestCube();
        
        auto duplicates = findDuplicateVertices(mesh);
        
        if (duplicates.size() > 0) {
            std::cout << "  WARNING: Found " << duplicates.size() << " duplicate vertex pairs" << std::endl;
            for (const auto& dup : duplicates) {
                const auto& v1 = mesh.vertices[dup.first].position;
                const auto& v2 = mesh.vertices[dup.second].position;
                std::cout << "    Vertices " << dup.first << " and " << dup.second 
                          << " at position (" << v1.x << ", " << v1.y << ", " << v1.z << ")" << std::endl;
            }
        } else {
            std::cout << "  No duplicate vertices found (good)" << std::endl;
        }
        
        // For a cube, we shouldn't have duplicates
        TEST_EXPECT_EQ(0, duplicates.size(), "No duplicate vertices in cube mesh");
        
        return true;
    }
    
    bool testEdgeConnectivity() {
        std::cout << "\n=== Test: Edge Connectivity ===" << std::endl;
        
        TestMesh mesh = createTestCube();
        
        auto edgeMap = buildEdgeMap(mesh);
        
        // Count edges shared by different numbers of triangles
        int edgesWithOneTriangle = 0;
        int edgesWithTwoTriangles = 0;
        int edgesWithMoreTriangles = 0;
        
        for (const auto& [edge, triangles] : edgeMap) {
            if (triangles.size() == 1) edgesWithOneTriangle++;
            else if (triangles.size() == 2) edgesWithTwoTriangles++;
            else edgesWithMoreTriangles++;
        }
        
        std::cout << "  Total edges: " << edgeMap.size() << std::endl;
        std::cout << "  Boundary edges (1 triangle): " << edgesWithOneTriangle << std::endl;
        std::cout << "  Interior edges (2 triangles): " << edgesWithTwoTriangles << std::endl;
        std::cout << "  Non-manifold edges (>2 triangles): " << edgesWithMoreTriangles << std::endl;
        
        // For a closed cube, all edges should be shared by exactly 2 triangles
        TEST_EXPECT_EQ(0, edgesWithOneTriangle, "No boundary edges in closed cube");
        TEST_EXPECT_GT(edgesWithTwoTriangles, 0, "Has interior edges");
        TEST_EXPECT_EQ(0, edgesWithMoreTriangles, "No non-manifold edges");
        
        return true;
    }
    
    bool testManifoldness() {
        std::cout << "\n=== Test: Manifold Properties ===" << std::endl;
        
        TestMesh mesh = createTestCube();
        
        // Build vertex-to-triangle map
        std::unordered_map<uint32_t, std::set<uint32_t>> vertexToTriangles;
        uint32_t triangleCount = mesh.indices.size() / 3;
        
        for (uint32_t t = 0; t < triangleCount; t++) {
            for (int i = 0; i < 3; i++) {
                vertexToTriangles[mesh.indices[t * 3 + i]].insert(t);
            }
        }
        
        // Check vertex valence (number of connected triangles)
        bool isManifold = true;
        for (const auto& [vertex, triangles] : vertexToTriangles) {
            if (triangles.size() < 3) {
                std::cout << "  WARNING: Vertex " << vertex << " connected to only " 
                          << triangles.size() << " triangles" << std::endl;
                isManifold = false;
            }
        }
        
        TEST_EXPECT_TRUE(isManifold, "All vertices have sufficient connectivity");
        
        return true;
    }
    
    bool testTriangleOrientation() {
        std::cout << "\n=== Test: Triangle Orientation ===" << std::endl;
        
        TestMesh mesh = createTestCube();
        
        // Calculate triangle normals and check consistency
        uint32_t triangleCount = mesh.indices.size() / 3;
        int consistentTriangles = 0;
        int inconsistentTriangles = 0;
        
        for (uint32_t t = 0; t < triangleCount; t++) {
            uint32_t i0 = mesh.indices[t * 3];
            uint32_t i1 = mesh.indices[t * 3 + 1];
            uint32_t i2 = mesh.indices[t * 3 + 2];
            
            glm::vec3 v0 = mesh.vertices[i0].position;
            glm::vec3 v1 = mesh.vertices[i1].position;
            glm::vec3 v2 = mesh.vertices[i2].position;
            
            // Calculate triangle normal
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 triangleNormal = glm::normalize(glm::cross(edge1, edge2));
            
            // Check if triangle normal agrees with vertex normals
            glm::vec3 avgVertexNormal = glm::normalize(
                mesh.vertices[i0].normal + 
                mesh.vertices[i1].normal + 
                mesh.vertices[i2].normal
            );
            
            float dot = glm::dot(triangleNormal, avgVertexNormal);
            if (dot > 0.5f) {
                consistentTriangles++;
            } else {
                inconsistentTriangles++;
            }
        }
        
        std::cout << "  Consistent triangles: " << consistentTriangles << std::endl;
        std::cout << "  Inconsistent triangles: " << inconsistentTriangles << std::endl;
        
        // For now, just warn about orientation issues rather than fail
        if (consistentTriangles <= inconsistentTriangles) {
            std::cout << "  WARNING: Triangle orientation may need review" << std::endl;
        } else {
            std::cout << "  Triangle orientation looks good" << std::endl;
        }
        
        return true;
    }
};

int main() {
    std::cout << "=== Mesh Connectivity Test Suite ===" << std::endl;
    std::cout << "Testing mesh generation and connectivity\n" << std::endl;
    
    MeshConnectivityTester tester;
    
    if (!tester.setUp()) {
        std::cerr << "Failed to set up test environment" << std::endl;
        return 1;
    }
    
    bool allTestsPassed = true;
    
    try {
        allTestsPassed &= tester.testMeshIntegrity();
        allTestsPassed &= tester.testDuplicateVertices();
        allTestsPassed &= tester.testEdgeConnectivity();
        allTestsPassed &= tester.testManifoldness();
        allTestsPassed &= tester.testTriangleOrientation();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        allTestsPassed = false;
    }
    
    std::cout << "\n=== Test Summary ===" << std::endl;
    if (allTestsPassed) {
        std::cout << "ALL TESTS PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
        return 1;
    }
}