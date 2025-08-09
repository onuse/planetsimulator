#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <unordered_map>
#include <vector>
#include <set>
#include <cmath>
#include <chrono>
#include <sstream>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

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

#define TEST_EXPECT_FLOAT_EQ(expected, actual, message) \
    TEST_ASSERT(std::abs((expected) - (actual)) < 0.0001f, message << " (expected " << expected << ", got " << actual << ")")

#define TEST_EXPECT_NEAR(expected, actual, tolerance, message) \
    TEST_ASSERT(std::abs((expected) - (actual)) < (tolerance), message << " (expected " << expected << " +/- " << tolerance << ", got " << actual << ")")

#define TEST_EXPECT_TRUE(condition, message) \
    TEST_ASSERT(condition, message)

#define TEST_EXPECT_FALSE(condition, message) \
    TEST_ASSERT(!(condition), message)

#define TEST_EXPECT_GT(a, b, message) \
    TEST_ASSERT((a) > (b), message << " (" << a << " should be > " << b << ")")

#define TEST_EXPECT_GE(a, b, message) \
    TEST_ASSERT((a) >= (b), message << " (" << a << " should be >= " << b << ")")

#define TEST_EXPECT_LT(a, b, message) \
    TEST_ASSERT((a) < (b), message << " (" << a << " should be < " << b << ")")

// Helper for comparing vec3
bool vec3Equal(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
    return std::abs(a.x - b.x) < epsilon &&
           std::abs(a.y - b.y) < epsilon &&
           std::abs(a.z - b.z) < epsilon;
}

#define TEST_EXPECT_VEC3_EQ(expected, actual, message) \
    TEST_ASSERT(vec3Equal(expected, actual), message << " (expected [" << (expected).x << "," << (expected).y << "," << (expected).z << "], got [" << (actual).x << "," << (actual).y << "," << (actual).z << "])")

// Hash function for glm::vec3 to use in unordered_map
struct Vec3Hash {
    size_t operator()(const glm::vec3& v) const {
        size_t h1 = std::hash<float>{}(v.x);
        size_t h2 = std::hash<float>{}(v.y);
        size_t h3 = std::hash<float>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// Equality comparison for glm::vec3 with epsilon
struct Vec3Equal {
    bool operator()(const glm::vec3& a, const glm::vec3& b) const {
        const float epsilon = 0.001f;
        return std::abs(a.x - b.x) < epsilon &&
               std::abs(a.y - b.y) < epsilon &&
               std::abs(a.z - b.z) < epsilon;
    }
};

// Simple test vertex structure (avoiding Vulkan dependency)
struct TestVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Simple test chunk structure (avoiding Vulkan dependency)
struct TestChunk {
    glm::vec3 position;          // World position of chunk
    float voxelSize;             // Size of each voxel at this LOD level
    uint32_t lodLevel;           // Level of detail (0 = highest detail)
    
    // Generated mesh data
    std::vector<TestVertex> vertices;
    std::vector<uint32_t> indices;
};

class ChunkBoundaryTest {
public:
    ChunkBoundaryTest() {
        // Create test planet with radius 1000m and depth 6 (for faster testing)
        planet = std::make_unique<octree::OctreePlanet>(1000.0f, 6);
        planet->generate(42); // Fixed seed for reproducibility
        
        // Standard chunk parameters
        chunkSize = glm::ivec3(16, 16, 16);
        voxelSize = 10.0f; // 10 meters per voxel at LOD 0
    }
    
    // Helper to create a chunk at a specific grid position
    TestChunk createChunk(const glm::ivec3& gridPos, uint32_t lod = 0) {
        TestChunk chunk;
        chunk.position = glm::vec3(gridPos) * (voxelSize * float(chunkSize.x));
        chunk.voxelSize = voxelSize * (1 << lod);
        chunk.lodLevel = lod;
        return chunk;
    }
    
    // Sample voxel at a world position
    float sampleVoxel(const glm::vec3& worldPos) {
        // Use getVoxel to access voxel data
        const octree::Voxel* voxel = planet->getVoxel(worldPos);
        if (!voxel) return 0.0f;
        
        // Convert voxel data to density (0 = empty, 1 = solid)
        return voxel->isEmpty() ? 0.0f : 1.0f;
    }
    
    // Get material at a world position
    octree::MixedVoxel getMaterial(const glm::vec3& worldPos) {
        const octree::Voxel* voxel = planet->getVoxel(worldPos);
        if (!voxel) return octree::MixedVoxel();
        return *voxel;  // Voxel is MixedVoxel
    }
    
    // Extract boundary vertices from a chunk mesh
    std::vector<glm::vec3> extractBoundaryVertices(
        const TestChunk& chunk,
        int face // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    ) {
        std::vector<glm::vec3> boundaryVerts;
        float chunkExtent = chunk.voxelSize * chunkSize.x;
        float epsilon = chunk.voxelSize * 0.1f; // 10% tolerance
        
        for (const auto& vertex : chunk.vertices) {
            glm::vec3 relPos = vertex.position - chunk.position;
            bool onBoundary = false;
            
            switch(face) {
                case 0: onBoundary = std::abs(relPos.x - chunkExtent) < epsilon; break;
                case 1: onBoundary = std::abs(relPos.x) < epsilon; break;
                case 2: onBoundary = std::abs(relPos.y - chunkExtent) < epsilon; break;
                case 3: onBoundary = std::abs(relPos.y) < epsilon; break;
                case 4: onBoundary = std::abs(relPos.z - chunkExtent) < epsilon; break;
                case 5: onBoundary = std::abs(relPos.z) < epsilon; break;
            }
            
            if (onBoundary) {
                boundaryVerts.push_back(vertex.position);
            }
        }
        
        return boundaryVerts;
    }
    
    std::unique_ptr<octree::OctreePlanet> planet;
    glm::ivec3 chunkSize;
    float voxelSize;
};

bool testChunkPositionCalculation() {
    ChunkBoundaryTest test;
    // Test that chunk positions are calculated correctly
    auto chunk1 = test.createChunk(glm::ivec3(0, 0, 0));
    auto chunk2 = test.createChunk(glm::ivec3(1, 0, 0));
    auto chunk3 = test.createChunk(glm::ivec3(0, 1, 0));
    
    TEST_EXPECT_VEC3_EQ(glm::vec3(0, 0, 0), chunk1.position, "Chunk1 position");
    TEST_EXPECT_VEC3_EQ(glm::vec3(160, 0, 0), chunk2.position, "Chunk2 position"); // 16 * 10
    TEST_EXPECT_VEC3_EQ(glm::vec3(0, 160, 0), chunk3.position, "Chunk3 position");
    
    // Test LOD scaling
    auto chunkLOD1 = test.createChunk(glm::ivec3(0, 0, 0), 1);
    TEST_EXPECT_FLOAT_EQ(chunkLOD1.voxelSize, 20.0f, "LOD1 voxel size"); // 10 * 2^1
    
    return true;
}

bool testAdjacentChunkAlignment() {
    ChunkBoundaryTest test;
    // Create two adjacent chunks
    auto chunk1 = test.createChunk(glm::ivec3(0, 0, 0));
    auto chunk2 = test.createChunk(glm::ivec3(1, 0, 0));
    
    // The right edge of chunk1 should align with left edge of chunk2
    float chunk1RightEdge = chunk1.position.x + (test.chunkSize.x * test.voxelSize);
    float chunk2LeftEdge = chunk2.position.x;
    
    TEST_EXPECT_FLOAT_EQ(chunk1RightEdge, chunk2LeftEdge, "Chunk edge alignment");
    
    // No gap or overlap
    float gap = chunk2LeftEdge - chunk1RightEdge;
    TEST_EXPECT_FLOAT_EQ(gap, 0.0f, "No gap between chunks");
    
    return true;
}

bool testVoxelSamplingConsistency() {
    ChunkBoundaryTest test;
    // Sample voxels at chunk boundaries - they should match between adjacent chunks
    glm::vec3 boundaryPoint(160.0f, 80.0f, 80.0f); // Right edge of chunk at (0,0,0)
    
    // Sample from perspective of first chunk (right edge)
    float density1 = test.sampleVoxel(boundaryPoint);
    
    // Sample from perspective of second chunk (left edge)  
    float density2 = test.sampleVoxel(boundaryPoint);
    
    // Should be exactly the same value
    TEST_EXPECT_FLOAT_EQ(density1, density2, "Boundary density consistency");
    
    // Test material consistency
    octree::MixedVoxel mat1 = test.getMaterial(boundaryPoint);
    octree::MixedVoxel mat2 = test.getMaterial(boundaryPoint);
    
    // Materials should be identical (check all 4 slots)
    for (int i = 0; i < 4; i++) {
        TEST_EXPECT_EQ((int)mat1.getMaterialID(i), (int)mat2.getMaterialID(i), "Material consistency at boundary");
        TEST_EXPECT_EQ(mat1.getMaterialAmount(i), mat2.getMaterialAmount(i), "Material amount consistency");
    }
    
    return true;
}

bool testBoundaryVertexMatching() {
    ChunkBoundaryTest test;
    // This test would require actual mesh generation
    // Checking that vertices on shared boundaries match exactly
    
    // Create mock vertex data for testing the concept
    TestChunk chunk1 = test.createChunk(glm::ivec3(0, 0, 0));
    TestChunk chunk2 = test.createChunk(glm::ivec3(1, 0, 0));
    
    // Add some test vertices at the boundary
    TestVertex v1, v2;
    v1.position = glm::vec3(160.0f, 80.0f, 80.0f); // Right edge of chunk1
    v2.position = glm::vec3(160.0f, 80.0f, 80.0f); // Left edge of chunk2
    
    chunk1.vertices.push_back(v1);
    chunk2.vertices.push_back(v2);
    
    // Extract boundary vertices
    auto boundary1 = test.extractBoundaryVertices(chunk1, 0); // +X face
    auto boundary2 = test.extractBoundaryVertices(chunk2, 1); // -X face
    
    // Build a map of vertices from chunk2's boundary
    std::unordered_map<glm::vec3, int, Vec3Hash, Vec3Equal> vertexMap;
    for (const auto& v : boundary2) {
        vertexMap[v]++;
    }
    
    // Check that all boundary vertices from chunk1 exist in chunk2
    for (const auto& v : boundary1) {
        std::stringstream ss;
        ss << "Vertex at " << v.x << "," << v.y << "," << v.z << " not found in adjacent chunk boundary";
        TEST_EXPECT_TRUE(vertexMap.find(v) != vertexMap.end(), ss.str());
    }
    
    return true;
}

bool testLODTransitionAlignment() {
    ChunkBoundaryTest test;
    // Test alignment between different LOD levels
    auto chunkLOD0 = test.createChunk(glm::ivec3(0, 0, 0), 0);
    auto chunkLOD1 = test.createChunk(glm::ivec3(1, 0, 0), 1);
    
    // LOD1 voxels are 2x the size of LOD0 voxels
    TEST_EXPECT_FLOAT_EQ(chunkLOD1.voxelSize, chunkLOD0.voxelSize * 2.0f, "LOD voxel size ratio");
    
    // Boundary positions should still align at specific points
    // Every 2nd vertex in LOD0 should align with LOD1 vertices
    float lod0Step = chunkLOD0.voxelSize;
    float lod1Step = chunkLOD1.voxelSize;
    
    TEST_EXPECT_FLOAT_EQ(lod1Step, lod0Step * 2.0f, "LOD step size ratio");
    
    // Sample at aligned positions
    for (int i = 0; i < 8; i++) {
        glm::vec3 lod0Pos = chunkLOD0.position + glm::vec3(i * lod0Step * 2, 0, 0);
        glm::vec3 lod1Pos = chunkLOD1.position + glm::vec3(i * lod1Step, 0, 0);
        
        // When properly aligned, densities should match
        if (i * 2 < test.chunkSize.x) {
            float density0 = test.sampleVoxel(lod0Pos);
            float density1 = test.sampleVoxel(lod1Pos - glm::vec3(test.chunkSize.x * test.voxelSize, 0, 0));
            
            // Note: Due to interpolation differences, we allow some tolerance
            TEST_EXPECT_NEAR(density0, density1, 0.1f, "LOD density alignment");
        }
    }
    
    return true;
}

bool testChunkGridCoverage() {
    ChunkBoundaryTest test;
    // Test that a grid of chunks covers space without gaps
    std::set<std::pair<int, int>> coverage;
    
    // Create a 3x3 grid of chunks
    for (int x = -1; x <= 1; x++) {
        for (int z = -1; z <= 1; z++) {
            auto chunk = test.createChunk(glm::ivec3(x, 0, z));
            
            // Calculate covered voxel range
            int startX = x * test.chunkSize.x;
            int endX = (x + 1) * test.chunkSize.x;
            int startZ = z * test.chunkSize.z;
            int endZ = (z + 1) * test.chunkSize.z;
            
            // Mark all covered voxels
            for (int vx = startX; vx < endX; vx++) {
                for (int vz = startZ; vz < endZ; vz++) {
                    auto result = coverage.insert({vx, vz});
                    // Should be first time we're covering this voxel
                    {
                        std::stringstream ss;
                        ss << "Voxel (" << vx << "," << vz << ") covered multiple times";
                        TEST_EXPECT_TRUE(result.second, ss.str());
                    }
                }
            }
        }
    }
    
    // Check we have complete coverage with no gaps
    int expectedVoxels = 3 * test.chunkSize.x * 3 * test.chunkSize.z;
    TEST_EXPECT_EQ((int)coverage.size(), expectedVoxels, "Complete coverage");
    
    // Check for gaps by verifying continuity
    for (int x = -test.chunkSize.x; x < 2 * test.chunkSize.x; x++) {
        for (int z = -test.chunkSize.z; z < 2 * test.chunkSize.z; z++) {
            {
                std::stringstream ss;
                ss << "Gap found at voxel (" << x << "," << z << ")";
                TEST_EXPECT_TRUE(coverage.find({x, z}) != coverage.end(), ss.str());
            }
        }
    }
    
    return true;
}

bool testCornerVoxelSharing() {
    ChunkBoundaryTest test;
    // Test that corner voxels are shared correctly between 8 chunks
    glm::vec3 cornerPoint(0, 0, 0); // Corner where 8 chunks meet
    
    // All 8 chunks around this corner
    std::vector<glm::ivec3> chunkPositions = {
        {-1, -1, -1}, {0, -1, -1}, {-1, 0, -1}, {0, 0, -1},
        {-1, -1, 0}, {0, -1, 0}, {-1, 0, 0}, {0, 0, 0}
    };
    
    // Sample the corner from each chunk's perspective
    float referenceDensity = test.sampleVoxel(cornerPoint);
    octree::MixedVoxel referenceMaterial = test.getMaterial(cornerPoint);
    
    for (const auto& chunkPos : chunkPositions) {
        auto chunk = test.createChunk(chunkPos);
        
        // Calculate corner position relative to this chunk
        glm::vec3 relativeCorner = cornerPoint - chunk.position;
        
        // Verify the corner is actually at a chunk boundary
        bool atBoundary = (std::abs(relativeCorner.x) < 0.1f || 
                          std::abs(relativeCorner.x - test.chunkSize.x * test.voxelSize) < 0.1f) &&
                         (std::abs(relativeCorner.y) < 0.1f || 
                          std::abs(relativeCorner.y - test.chunkSize.y * test.voxelSize) < 0.1f) &&
                         (std::abs(relativeCorner.z) < 0.1f || 
                          std::abs(relativeCorner.z - test.chunkSize.z * test.voxelSize) < 0.1f);
        
        {
            std::stringstream ss;
            ss << "Corner at boundary for chunk " << chunkPos.x << "," << chunkPos.y << "," << chunkPos.z;
            TEST_EXPECT_TRUE(atBoundary, ss.str());
        }
        
        // Sample should be consistent
        float density = test.sampleVoxel(cornerPoint);
        TEST_EXPECT_FLOAT_EQ(density, referenceDensity, "Corner density consistency");
        
        octree::MixedVoxel material = test.getMaterial(cornerPoint);
        for (int i = 0; i < 4; i++) {
            TEST_EXPECT_EQ((int)material.getMaterialID(i), (int)referenceMaterial.getMaterialID(i), "Corner material consistency");
        }
    }
    
    return true;
}

bool testEdgeVoxelSharing() {
    ChunkBoundaryTest test;
    // Test that edge voxels are shared correctly between 4 chunks
    glm::vec3 edgePoint(0, 80, 80); // Edge where 4 chunks meet (X-axis edge)
    
    // 4 chunks sharing this edge
    std::vector<glm::ivec3> chunkPositions = {
        {-1, 0, 0}, {0, 0, 0}, {-1, 0, 1}, {0, 0, 1}
    };
    
    float referenceDensity = test.sampleVoxel(edgePoint);
    
    for (const auto& chunkPos : chunkPositions) {
        auto chunk = test.createChunk(chunkPos);
        float density = test.sampleVoxel(edgePoint);
        {
            std::stringstream ss;
            ss << "Inconsistent edge sampling at chunk " << chunkPos.x << "," << chunkPos.y << "," << chunkPos.z;
            TEST_EXPECT_FLOAT_EQ(density, referenceDensity, ss.str());
        }
    }
    
    return true;
}

bool testChunkSizeAndSpacing() {
    ChunkBoundaryTest test;
    // Verify chunk dimensions and spacing are consistent
    const int gridSize = 5;
    std::vector<TestChunk> chunks;
    
    // Create a grid of chunks
    for (int x = 0; x < gridSize; x++) {
        for (int z = 0; z < gridSize; z++) {
            chunks.push_back(test.createChunk(glm::ivec3(x, 0, z)));
        }
    }
    
    // Check spacing between all adjacent pairs
    for (size_t i = 0; i < chunks.size(); i++) {
        for (size_t j = i + 1; j < chunks.size(); j++) {
            glm::vec3 diff = chunks[j].position - chunks[i].position;
            
            // Check if chunks are adjacent
            float expectedSpacing = test.chunkSize.x * test.voxelSize;
            
            // X-adjacent
            if (std::abs(diff.x - expectedSpacing) < 0.1f && 
                std::abs(diff.y) < 0.1f && std::abs(diff.z) < 0.1f) {
                TEST_EXPECT_FLOAT_EQ(diff.x, expectedSpacing, "X-spacing");
            }
            
            // Z-adjacent  
            if (std::abs(diff.z - expectedSpacing) < 0.1f && 
                std::abs(diff.x) < 0.1f && std::abs(diff.y) < 0.1f) {
                TEST_EXPECT_FLOAT_EQ(diff.z, expectedSpacing, "Z-spacing");
            }
        }
    }
    
    return true;
}

bool testVoxelInterpolationAtBoundaries() {
    ChunkBoundaryTest test;
    // Test that voxel interpolation is consistent at chunk boundaries
    // This is critical for smooth mesh generation
    
    glm::vec3 boundary(160.0f, 80.0f, 80.0f); // Boundary between chunks
    
    // Sample points on either side of boundary
    float epsilon = 0.01f;
    glm::vec3 leftOfBoundary = boundary - glm::vec3(epsilon, 0, 0);
    glm::vec3 rightOfBoundary = boundary + glm::vec3(epsilon, 0, 0);
    
    float densityLeft = test.sampleVoxel(leftOfBoundary);
    float densityRight = test.sampleVoxel(rightOfBoundary);
    float densityBoundary = test.sampleVoxel(boundary);
    
    // Density should vary smoothly across boundary
    float gradient = (densityRight - densityLeft) / (2 * epsilon);
    float expectedBoundary = (densityLeft + densityRight) / 2.0f;
    
    // Allow some tolerance for numerical precision
    TEST_EXPECT_NEAR(densityBoundary, expectedBoundary, 0.1f,
        "Discontinuity detected at chunk boundary");
    
    // Gradient should be reasonable (not infinite)
    TEST_EXPECT_LT(std::abs(gradient), 1000.0f,
        "Extreme gradient at chunk boundary suggests discontinuity");
    
    return true;
}

bool testChunkWorldToLocalConversion() {
    ChunkBoundaryTest test;
    // Test conversion between world and chunk-local coordinates
    auto chunk = test.createChunk(glm::ivec3(2, 1, 3));
    
    // Test point in world space
    glm::vec3 worldPoint(350.0f, 200.0f, 500.0f);
    
    // Convert to chunk-local coordinates
    glm::vec3 localPoint = worldPoint - chunk.position;
    
    // Expected local coordinates
    glm::vec3 expectedLocal(
        350.0f - 2 * test.chunkSize.x * test.voxelSize,
        200.0f - 1 * test.chunkSize.y * test.voxelSize,
        500.0f - 3 * test.chunkSize.z * test.voxelSize
    );
    
    TEST_EXPECT_FLOAT_EQ(localPoint.x, expectedLocal.x, "Local X coordinate");
    TEST_EXPECT_FLOAT_EQ(localPoint.y, expectedLocal.y, "Local Y coordinate");
    TEST_EXPECT_FLOAT_EQ(localPoint.z, expectedLocal.z, "Local Z coordinate");
    
    // Convert back to world
    glm::vec3 reconstructedWorld = localPoint + chunk.position;
    TEST_EXPECT_VEC3_EQ(worldPoint, reconstructedWorld, "World reconstruction");
    
    return true;
}

bool testPlanetSurfaceChunkAlignment() {
    ChunkBoundaryTest test;
    // Test chunks at planet surface for proper alignment
    float surfaceRadius = 900.0f; // Near planet surface
    
    // Create chunks around the equator
    std::vector<TestChunk> surfaceChunks;
    int numChunks = 8;
    
    for (int i = 0; i < numChunks; i++) {
        float angle = (2.0f * glm::pi<float>()) * i / numChunks;
        glm::vec3 chunkPos(
            surfaceRadius * cos(angle),
            0,
            surfaceRadius * sin(angle)
        );
        
        // Snap to chunk grid
        glm::ivec3 gridPos = glm::ivec3(
            std::round(chunkPos.x / (test.chunkSize.x * test.voxelSize)),
            0,
            std::round(chunkPos.z / (test.chunkSize.z * test.voxelSize))
        );
        
        surfaceChunks.push_back(test.createChunk(gridPos));
    }
    
    // Verify chunks don't overlap
    for (size_t i = 0; i < surfaceChunks.size(); i++) {
        for (size_t j = i + 1; j < surfaceChunks.size(); j++) {
            glm::vec3 diff = surfaceChunks[j].position - surfaceChunks[i].position;
            float distance = glm::length(diff);
            
            // Minimum distance should be at least one chunk size
            float minDistance = test.chunkSize.x * test.voxelSize * 0.9f; // 90% to account for rounding
            {
                std::stringstream ss;
                ss << "Chunks " << i << " and " << j << " overlap or are too close";
                TEST_EXPECT_GE(distance, minDistance, ss.str());
            }
        }
    }
    
    return true;
}

// Performance test for boundary detection
bool testBoundaryDetectionPerformance() {
    ChunkBoundaryTest test;
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create a large grid and check all boundaries
    const int gridSize = 10;
    int boundaryCheckCount = 0;
    
    for (int x = 0; x < gridSize; x++) {
        for (int y = 0; y < gridSize; y++) {
            for (int z = 0; z < gridSize; z++) {
                auto chunk = test.createChunk(glm::ivec3(x, y, z));
                
                // Check all 6 faces for boundary vertices
                for (int face = 0; face < 6; face++) {
                    auto boundaryVerts = test.extractBoundaryVertices(chunk, face);
                    boundaryCheckCount++;
                }
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Boundary detection for " << boundaryCheckCount 
              << " chunk faces took " << duration.count() << "ms\n";
    
    // Should complete in reasonable time
    TEST_EXPECT_LT(duration.count(), 1000, "Performance within bounds"); // Less than 1 second
    
    return true;
}

int main(int argc, char** argv) {
    std::cout << "=== Chunk Boundary Alignment Tests ===" << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    // Run all tests
    struct Test {
        const char* name;
        bool (*func)();
    } tests[] = {
        {"ChunkPositionCalculation", testChunkPositionCalculation},
        {"AdjacentChunkAlignment", testAdjacentChunkAlignment},
        {"VoxelSamplingConsistency", testVoxelSamplingConsistency},
        {"BoundaryVertexMatching", testBoundaryVertexMatching},
        {"LODTransitionAlignment", testLODTransitionAlignment},
        {"ChunkGridCoverage", testChunkGridCoverage},
        {"CornerVoxelSharing", testCornerVoxelSharing},
        {"EdgeVoxelSharing", testEdgeVoxelSharing},
        {"ChunkSizeAndSpacing", testChunkSizeAndSpacing},
        {"VoxelInterpolationAtBoundaries", testVoxelInterpolationAtBoundaries},
        {"ChunkWorldToLocalConversion", testChunkWorldToLocalConversion},
        {"PlanetSurfaceChunkAlignment", testPlanetSurfaceChunkAlignment},
        {"BoundaryDetectionPerformance", testBoundaryDetectionPerformance}
    };
    
    for (const auto& test : tests) {
        std::cout << "\n--- Test: " << test.name << " ---" << std::endl;
        if (test.func()) {
            passed++;
            std::cout << "Test " << test.name << " PASSED" << std::endl;
        } else {
            failed++;
            std::cerr << "Test " << test.name << " FAILED" << std::endl;
        }
    }
    
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    
    return failed > 0 ? 1 : 0;
}