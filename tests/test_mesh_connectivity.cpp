#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include "rendering/surface_extractor.hpp"
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

namespace rendering {
    // Forward declaration from implementation
    std::unique_ptr<ISurfaceExtractor> createSimpleSurfaceExtractor();
}

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
    std::unique_ptr<rendering::ISurfaceExtractor> extractor;
    
    // Epsilon for floating point comparisons
    static constexpr float VERTEX_EPSILON = 0.001f;
    
    // Helper to check if two vertices are the same position
    bool verticesEqual(const glm::vec3& v1, const glm::vec3& v2) {
        return glm::all(glm::epsilonEqual(v1, v2, VERTEX_EPSILON));
    }
    
    // Find duplicate vertices in mesh
    std::vector<std::pair<uint32_t, uint32_t>> findDuplicateVertices(const rendering::ExtractedMesh& mesh) {
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
    std::unordered_map<Edge, std::vector<uint32_t>> buildEdgeMap(const rendering::ExtractedMesh& mesh) {
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
            // Material table is auto-initialized as singleton
            core::MaterialTable::getInstance();
            
            // Create planet with known properties
            planet = std::make_unique<octree::OctreePlanet>(1000.0f, 10);
            planet->generate(42); // Fixed seed
            
            // Create surface extractor
            extractor = rendering::createSimpleSurfaceExtractor();
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Setup failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    void tearDown() {
        extractor.reset();
        planet.reset();
    }
    
    // Test 1: Verify adjacent triangles share vertices (no gaps)
    bool testAdjacentTriangleVertexSharing() {
        std::cout << "\n=== Test: Adjacent Triangle Vertex Sharing ===" << std::endl;
        
        // Create a region at planet surface where we expect mesh
        rendering::VoxelRegion region(
            glm::vec3(950.0f, 0.0f, 0.0f),
            25.0f,
            glm::ivec3(8, 8, 8),
            0
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        if (mesh.isEmpty()) {
            std::cout << "WARNING: No mesh generated at planet surface - this indicates the core issue!" << std::endl;
            return false;
        }
        
        std::cout << "  Mesh has " << mesh.vertices.size() << " vertices, " 
                  << mesh.getTriangleCount() << " triangles" << std::endl;
        
        // Build edge-to-triangle map
        auto edgeMap = buildEdgeMap(mesh);
        
        // Check edge sharing
        int sharedEdges = 0;
        int boundaryEdges = 0;
        int disconnectedEdges = 0;
        
        for (const auto& [edge, triangles] : edgeMap) {
            if (triangles.size() == 2) {
                sharedEdges++;
            } else if (triangles.size() == 1) {
                boundaryEdges++;
            } else if (triangles.size() == 0) {
                disconnectedEdges++; // Should never happen
            } else {
                std::cout << "  WARNING: Edge shared by " << triangles.size() << " triangles (non-manifold)" << std::endl;
            }
        }
        
        std::cout << "  Edge statistics:" << std::endl;
        std::cout << "    Shared edges: " << sharedEdges << std::endl;
        std::cout << "    Boundary edges: " << boundaryEdges << std::endl;
        std::cout << "    Disconnected edges: " << disconnectedEdges << std::endl;
        
        TEST_EXPECT_EQ(disconnectedEdges, 0, "No disconnected edges");
        TEST_EXPECT_GT(sharedEdges, 0, "Should have shared edges between triangles");
        
        // Check if mesh appears scattered (too many boundary edges relative to shared)
        float connectivityRatio = (float)sharedEdges / (float)(sharedEdges + boundaryEdges);
        std::cout << "  Connectivity ratio: " << connectivityRatio << std::endl;
        
        // A well-connected mesh should have more shared edges than boundary edges
        // Scattered triangles would have mostly boundary edges
        TEST_EXPECT_GT(connectivityRatio, 0.3f, "Mesh connectivity ratio (scattered if < 0.3)");
        
        return true;
    }
    
    // Test 2: Test that mesh forms watertight surface
    bool testWatertightSurface() {
        std::cout << "\n=== Test: Watertight Surface ===" << std::endl;
        
        // Create a small region completely inside the planet
        rendering::VoxelRegion region(
            glm::vec3(0.0f, 0.0f, 0.0f), // Planet center
            100.0f,
            glm::ivec3(4, 4, 4),
            0
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        if (mesh.isEmpty()) {
            std::cout << "  No mesh at planet center (all solid)" << std::endl;
            return true;
        }
        
        auto edgeMap = buildEdgeMap(mesh);
        
        // For a watertight mesh, every edge should be shared by exactly 2 triangles
        // (or 1 if it's on the boundary of our extraction region)
        bool isWatertight = true;
        int nonManifoldEdges = 0;
        
        for (const auto& [edge, triangles] : edgeMap) {
            if (triangles.size() > 2) {
                nonManifoldEdges++;
                isWatertight = false;
                std::cout << "  Non-manifold edge between vertices " << edge.v1 << " and " << edge.v2 
                          << " (shared by " << triangles.size() << " triangles)" << std::endl;
            }
        }
        
        TEST_EXPECT_EQ(nonManifoldEdges, 0, "No non-manifold edges for watertight mesh");
        TEST_EXPECT_TRUE(isWatertight, "Mesh should be watertight (manifold)");
        
        return true;
    }
    
    // Test 3: Verify vertex deduplication is working
    bool testVertexDeduplication() {
        std::cout << "\n=== Test: Vertex Deduplication ===" << std::endl;
        
        rendering::VoxelRegion region(
            glm::vec3(950.0f, 0.0f, 0.0f),
            50.0f,
            glm::ivec3(4, 4, 4),
            0
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        if (mesh.isEmpty()) {
            std::cout << "WARNING: No mesh generated - cannot test vertex deduplication!" << std::endl;
            return false;
        }
        
        // Find duplicate vertices
        auto duplicates = findDuplicateVertices(mesh);
        
        std::cout << "  Found " << duplicates.size() << " duplicate vertex pairs" << std::endl;
        
        if (!duplicates.empty()) {
            std::cout << "  First few duplicates:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), duplicates.size()); i++) {
                auto [v1, v2] = duplicates[i];
                std::cout << "    Vertices " << v1 << " and " << v2 << " at position ("
                          << mesh.vertices[v1].position.x << ", "
                          << mesh.vertices[v1].position.y << ", "
                          << mesh.vertices[v1].position.z << ")" << std::endl;
            }
        }
        
        // Some duplicates might be acceptable at region boundaries,
        // but excessive duplicates indicate a deduplication failure
        float duplicateRatio = (float)duplicates.size() / (float)mesh.vertices.size();
        std::cout << "  Duplicate ratio: " << duplicateRatio << std::endl;
        
        TEST_EXPECT_LT(duplicateRatio, 0.1f, "Less than 10% duplicate vertices");
        
        // Check if duplicates are actually referenced differently in triangles
        if (!duplicates.empty()) {
            // Check if any triangles use both vertices of a duplicate pair
            uint32_t triangleCount = mesh.indices.size() / 3;
            int trianglesWithDuplicates = 0;
            
            for (uint32_t t = 0; t < triangleCount; t++) {
                uint32_t i0 = mesh.indices[t * 3];
                uint32_t i1 = mesh.indices[t * 3 + 1];
                uint32_t i2 = mesh.indices[t * 3 + 2];
                
                for (const auto& [d1, d2] : duplicates) {
                    bool hasD1 = (i0 == d1 || i1 == d1 || i2 == d1);
                    bool hasD2 = (i0 == d2 || i1 == d2 || i2 == d2);
                    if (hasD1 && hasD2) {
                        trianglesWithDuplicates++;
                        break;
                    }
                }
            }
            
            std::cout << "  Triangles using duplicate vertices: " << trianglesWithDuplicates << std::endl;
            TEST_EXPECT_EQ(trianglesWithDuplicates, 0, "No triangles should use duplicate vertices");
        }
        
        return true;
    }
    
    // Test 4: Test normal consistency across connected triangles
    bool testNormalConsistency() {
        std::cout << "\n=== Test: Normal Consistency ===" << std::endl;
        
        rendering::VoxelRegion region(
            glm::vec3(950.0f, 0.0f, 0.0f),
            25.0f,
            glm::ivec3(6, 6, 6),
            0
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        if (mesh.isEmpty()) {
            std::cout << "WARNING: No mesh generated!" << std::endl;
            return false;
        }
        
        // Calculate triangle normals and check consistency
        uint32_t triangleCount = mesh.indices.size() / 3;
        std::vector<glm::vec3> triangleNormals;
        
        for (uint32_t t = 0; t < triangleCount; t++) {
            uint32_t i0 = mesh.indices[t * 3];
            uint32_t i1 = mesh.indices[t * 3 + 1];
            uint32_t i2 = mesh.indices[t * 3 + 2];
            
            glm::vec3 v0 = mesh.vertices[i0].position;
            glm::vec3 v1 = mesh.vertices[i1].position;
            glm::vec3 v2 = mesh.vertices[i2].position;
            
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
            
            triangleNormals.push_back(normal);
        }
        
        // Build edge map to find adjacent triangles
        auto edgeMap = buildEdgeMap(mesh);
        
        // Check normal consistency for adjacent triangles
        int consistentPairs = 0;
        int inconsistentPairs = 0;
        float maxDotProduct = -1.0f;
        float minDotProduct = 1.0f;
        
        for (const auto& [edge, triangles] : edgeMap) {
            if (triangles.size() == 2) {
                // Two triangles share this edge
                glm::vec3 n1 = triangleNormals[triangles[0]];
                glm::vec3 n2 = triangleNormals[triangles[1]];
                
                float dot = glm::dot(n1, n2);
                maxDotProduct = std::max(maxDotProduct, dot);
                minDotProduct = std::min(minDotProduct, dot);
                
                // Normals should point in roughly the same direction
                if (dot > 0.5f) {
                    consistentPairs++;
                } else {
                    inconsistentPairs++;
                    if (inconsistentPairs <= 5) {
                        std::cout << "  Inconsistent normals: dot product = " << dot << std::endl;
                    }
                }
            }
        }
        
        std::cout << "  Normal consistency statistics:" << std::endl;
        std::cout << "    Consistent pairs: " << consistentPairs << std::endl;
        std::cout << "    Inconsistent pairs: " << inconsistentPairs << std::endl;
        std::cout << "    Dot product range: [" << minDotProduct << ", " << maxDotProduct << "]" << std::endl;
        
        float consistencyRatio = (float)consistentPairs / (float)(consistentPairs + inconsistentPairs);
        std::cout << "  Consistency ratio: " << consistencyRatio << std::endl;
        
        TEST_EXPECT_GT(consistencyRatio, 0.8f, "At least 80% of adjacent triangles have consistent normals");
        
        return true;
    }
    
    // Test 5: Validate triangle winding order consistency
    bool testTriangleWindingOrder() {
        std::cout << "\n=== Test: Triangle Winding Order ===" << std::endl;
        
        rendering::VoxelRegion region(
            glm::vec3(950.0f, 0.0f, 0.0f),
            30.0f,
            glm::ivec3(5, 5, 5),
            0
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        if (mesh.isEmpty()) {
            std::cout << "WARNING: No mesh generated!" << std::endl;
            return false;
        }
        
        // For a surface mesh, all triangles should have consistent winding
        // when viewed from outside the surface
        uint32_t triangleCount = mesh.indices.size() / 3;
        
        // Calculate mesh centroid
        glm::vec3 centroid(0.0f);
        for (const auto& vertex : mesh.vertices) {
            centroid += vertex.position;
        }
        centroid /= (float)mesh.vertices.size();
        
        int outwardFacing = 0;
        int inwardFacing = 0;
        
        for (uint32_t t = 0; t < triangleCount; t++) {
            uint32_t i0 = mesh.indices[t * 3];
            uint32_t i1 = mesh.indices[t * 3 + 1];
            uint32_t i2 = mesh.indices[t * 3 + 2];
            
            glm::vec3 v0 = mesh.vertices[i0].position;
            glm::vec3 v1 = mesh.vertices[i1].position;
            glm::vec3 v2 = mesh.vertices[i2].position;
            
            // Calculate triangle center and normal
            glm::vec3 triangleCenter = (v0 + v1 + v2) / 3.0f;
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
            
            // Vector from centroid to triangle
            glm::vec3 toTriangle = glm::normalize(triangleCenter - centroid);
            
            // Check if normal points outward
            float dot = glm::dot(normal, toTriangle);
            if (dot > 0) {
                outwardFacing++;
            } else {
                inwardFacing++;
            }
        }
        
        std::cout << "  Winding order statistics:" << std::endl;
        std::cout << "    Outward facing: " << outwardFacing << std::endl;
        std::cout << "    Inward facing: " << inwardFacing << std::endl;
        
        // All triangles should have the same winding order
        float windingConsistency = std::max((float)outwardFacing, (float)inwardFacing) / (float)triangleCount;
        std::cout << "  Winding consistency: " << windingConsistency << std::endl;
        
        TEST_EXPECT_GT(windingConsistency, 0.9f, "At least 90% consistent winding order");
        
        return true;
    }
    
    // Test 6: Check for isolated triangles (scattered rendering issue)
    bool testIsolatedTriangles() {
        std::cout << "\n=== Test: Isolated Triangles Detection ===" << std::endl;
        
        rendering::VoxelRegion region(
            glm::vec3(950.0f, 0.0f, 0.0f),
            20.0f,
            glm::ivec3(10, 10, 10),
            0
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        if (mesh.isEmpty()) {
            std::cout << "WARNING: No mesh generated - this is the core issue!" << std::endl;
            return false;
        }
        
        // Build triangle adjacency graph
        uint32_t triangleCount = mesh.indices.size() / 3;
        auto edgeMap = buildEdgeMap(mesh);
        
        // Create adjacency list for triangles
        std::vector<std::set<uint32_t>> triangleAdjacency(triangleCount);
        for (const auto& [edge, triangles] : edgeMap) {
            if (triangles.size() == 2) {
                triangleAdjacency[triangles[0]].insert(triangles[1]);
                triangleAdjacency[triangles[1]].insert(triangles[0]);
            }
        }
        
        // Count isolated triangles (no neighbors)
        int isolatedTriangles = 0;
        int connectedTriangles = 0;
        int maxNeighbors = 0;
        
        for (uint32_t t = 0; t < triangleCount; t++) {
            int neighborCount = triangleAdjacency[t].size();
            maxNeighbors = std::max(maxNeighbors, neighborCount);
            
            if (neighborCount == 0) {
                isolatedTriangles++;
                if (isolatedTriangles <= 5) {
                    std::cout << "  Triangle " << t << " is isolated (no shared edges)" << std::endl;
                }
            } else {
                connectedTriangles++;
            }
        }
        
        std::cout << "  Triangle connectivity:" << std::endl;
        std::cout << "    Total triangles: " << triangleCount << std::endl;
        std::cout << "    Isolated triangles: " << isolatedTriangles << std::endl;
        std::cout << "    Connected triangles: " << connectedTriangles << std::endl;
        std::cout << "    Max neighbors for a triangle: " << maxNeighbors << std::endl;
        
        float isolationRatio = (float)isolatedTriangles / (float)triangleCount;
        std::cout << "  Isolation ratio: " << isolationRatio << std::endl;
        
        // This test will FAIL if triangles are scattered/disconnected
        TEST_EXPECT_LT(isolationRatio, 0.1f, "Less than 10% isolated triangles");
        TEST_EXPECT_EQ(isolatedTriangles, 0, "No isolated triangles in continuous surface");
        
        // Find connected components
        std::vector<bool> visited(triangleCount, false);
        std::vector<uint32_t> componentSizes;
        
        for (uint32_t t = 0; t < triangleCount; t++) {
            if (!visited[t]) {
                // BFS to find component size
                std::vector<uint32_t> queue = {t};
                uint32_t componentSize = 0;
                
                while (!queue.empty()) {
                    uint32_t current = queue.back();
                    queue.pop_back();
                    
                    if (visited[current]) continue;
                    visited[current] = true;
                    componentSize++;
                    
                    for (uint32_t neighbor : triangleAdjacency[current]) {
                        if (!visited[neighbor]) {
                            queue.push_back(neighbor);
                        }
                    }
                }
                
                componentSizes.push_back(componentSize);
            }
        }
        
        std::cout << "  Connected components: " << componentSizes.size() << std::endl;
        if (!componentSizes.empty()) {
            std::cout << "    Component sizes: ";
            for (size_t i = 0; i < std::min(size_t(10), componentSizes.size()); i++) {
                std::cout << componentSizes[i] << " ";
            }
            if (componentSizes.size() > 10) {
                std::cout << "...";
            }
            std::cout << std::endl;
        }
        
        // A continuous surface should have very few components (ideally 1)
        TEST_EXPECT_LT(componentSizes.size(), size_t(5), "Less than 5 disconnected components");
        
        return true;
    }
    
    // Test 7: Validate mesh generation at typical chunk parameters
    bool testChunkParameterMeshGeneration() {
        std::cout << "\n=== Test: Chunk Parameter Mesh Generation ===" << std::endl;
        
        // Test with the same parameters as TransvoxelRenderer would use
        glm::vec3 chunkPosition = glm::vec3(950.0f, 0.0f, 0.0f);
        float voxelSize = 25.0f;
        uint32_t lodLevel = 0;
        
        // Extract mesh for this chunk position
        rendering::VoxelRegion region(
            chunkPosition,
            voxelSize,
            glm::ivec3(8, 8, 8), // Standard chunk size
            lodLevel
        );
        
        rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
        
        std::cout << "  Chunk at " << chunkPosition.x << ", " << chunkPosition.y << ", " << chunkPosition.z << std::endl;
        std::cout << "  Voxel size: " << voxelSize << std::endl;
        
        if (mesh.isEmpty()) {
            std::cout << "  ERROR: No mesh generated for chunk that should contain surface!" << std::endl;
            std::cout << "  This confirms the core rendering issue - chunks aren't generating meshes" << std::endl;
            return false;
        }
        
        // Validate mesh data that would be passed to renderer
        std::cout << "  Generated mesh:" << std::endl;
        std::cout << "    Vertices: " << mesh.vertices.size() << std::endl;
        std::cout << "    Triangles: " << mesh.getTriangleCount() << std::endl;
        std::cout << "    Indices: " << mesh.indices.size() << std::endl;
        
        TEST_EXPECT_FALSE(mesh.vertices.empty(), "Mesh should have vertices");
        TEST_EXPECT_FALSE(mesh.indices.empty(), "Mesh should have indices");
        TEST_EXPECT_GT(mesh.getTriangleCount(), uint32_t(0), "Mesh should have triangles");
        TEST_EXPECT_EQ(mesh.indices.size() % 3, size_t(0), "Indices should form complete triangles");
        
        // Validate vertex colors are present (check only first few to avoid spam)
        bool allColorsValid = true;
        for (size_t i = 0; i < mesh.vertices.size(); i++) {
            const auto& vertex = mesh.vertices[i];
            if (!(vertex.color.x >= 0.0f && vertex.color.x <= 1.0f) ||
                !(vertex.color.y >= 0.0f && vertex.color.y <= 1.0f) ||
                !(vertex.color.z >= 0.0f && vertex.color.z <= 1.0f)) {
                allColorsValid = false;
                std::cout << "  Invalid color at vertex " << i << ": (" 
                          << vertex.color.x << ", " << vertex.color.y << ", " << vertex.color.z << ")" << std::endl;
                break;
            }
        }
        TEST_EXPECT_TRUE(allColorsValid, "All vertex colors in valid range [0,1]");
        
        return true;
    }
};

int main() {
    std::cout << "=== Mesh Connectivity Test Suite ===" << std::endl;
    std::cout << "Testing for scattered triangle and surface continuity issues\n" << std::endl;
    
    MeshConnectivityTester tester;
    
    if (!tester.setUp()) {
        std::cerr << "Failed to set up test environment" << std::endl;
        return 1;
    }
    
    bool allTestsPassed = true;
    
    try {
        // Run all connectivity tests
        allTestsPassed &= tester.testAdjacentTriangleVertexSharing();
        allTestsPassed &= tester.testWatertightSurface();
        allTestsPassed &= tester.testVertexDeduplication();
        allTestsPassed &= tester.testNormalConsistency();
        allTestsPassed &= tester.testTriangleWindingOrder();
        allTestsPassed &= tester.testIsolatedTriangles();
        allTestsPassed &= tester.testChunkParameterMeshGeneration();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        allTestsPassed = false;
    }
    
    tester.tearDown();
    
    std::cout << "\n=== Test Results ===" << std::endl;
    if (allTestsPassed) {
        std::cout << "All tests PASSED" << std::endl;
        std::cout << "\nSUMMARY: Mesh connectivity tests passed." << std::endl;
        std::cout << "If rendering still shows scattered triangles, the issue is likely in:" << std::endl;
        std::cout << "1. GPU buffer upload/creation" << std::endl;
        std::cout << "2. Render command execution" << std::endl;
        std::cout << "3. Pipeline state configuration" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests FAILED" << std::endl;
        std::cout << "\nKEY FINDINGS:" << std::endl;
        std::cout << "1. Mesh generation may not be producing connected triangles" << std::endl;
        std::cout << "2. Vertex deduplication might be failing" << std::endl;
        std::cout << "3. Triangle winding order could be inconsistent" << std::endl;
        std::cout << "4. Isolated triangles indicate scattered rendering issue" << std::endl;
        return 1;
    }
}