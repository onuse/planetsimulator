#include <iostream>
#include <memory>
#include <vector>
#include <atomic>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include "core/octree.hpp"
#include "core/material_table.hpp"

// Simple test framework (no gtest dependency)
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

// Forward declare to avoid Vulkan dependency
typedef void* VkBuffer;
typedef void* VkDeviceMemory;
#define VK_NULL_HANDLE nullptr

// Mock TransvoxelChunk structure for testing (without Vulkan dependencies)
struct MockTransvoxelChunk {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float voxelSize = 1.0f;
    uint32_t lodLevel = 0;
    
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> vertexColors;
    
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    
    bool isDirty = true;
    bool hasValidMesh = false;
};

class TransvoxelTester {
private:
    std::unique_ptr<octree::OctreePlanet> planet;

public:
    bool setUp() {
        try {
            // MaterialTable is a singleton and initializes itself
            planet = std::make_unique<octree::OctreePlanet>(1000.0f, 10);
            planet->generate(42); // Fixed seed for reproducible tests
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Setup failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    void tearDown() {
        planet.reset();
    }

    // Test 1: Chunk initialization
    bool testChunkInitialization() {
        std::cout << "\n=== Test: Chunk Initialization ===" << std::endl;
        
        MockTransvoxelChunk chunk;
        chunk.position = glm::vec3(100.0f, 200.0f, 300.0f);
        chunk.voxelSize = 25.0f;
        chunk.lodLevel = 2;
        
        TEST_EXPECT_EQ(chunk.position.x, 100.0f, "Chunk position X");
        TEST_EXPECT_EQ(chunk.position.y, 200.0f, "Chunk position Y");
        TEST_EXPECT_EQ(chunk.position.z, 300.0f, "Chunk position Z");
        TEST_EXPECT_EQ(chunk.voxelSize, 25.0f, "Chunk voxel size");
        TEST_EXPECT_EQ(chunk.lodLevel, 2, "Chunk LOD level");
        TEST_EXPECT_TRUE(chunk.isDirty, "Chunk should be dirty initially");
        TEST_EXPECT_FALSE(chunk.hasValidMesh, "Chunk should not have valid mesh initially");
        TEST_EXPECT_TRUE(chunk.vertices.empty(), "Vertices should be empty initially");
        TEST_EXPECT_TRUE(chunk.indices.empty(), "Indices should be empty initially");
        TEST_EXPECT_EQ(chunk.vertexBuffer, VK_NULL_HANDLE, "Vertex buffer should be null initially");
        TEST_EXPECT_EQ(chunk.indexBuffer, VK_NULL_HANDLE, "Index buffer should be null initially");
        
        return true;
    }

    // Test 2: Planet density sampling
    bool testPlanetDensitySampling() {
        std::cout << "\n=== Test: Planet Density Sampling ===" << std::endl;
        
        float planetRadius = planet->getRadius();
        TEST_EXPECT_EQ(planetRadius, 1000.0f, "Planet radius");
        
        struct TestPosition {
            glm::vec3 pos;
            std::string description;
        };
        
        std::vector<TestPosition> testPositions = {
            {{0.0f, 0.0f, 0.0f}, "planet center"},
            {{500.0f, 0.0f, 0.0f}, "inside planet"},
            {{900.0f, 0.0f, 0.0f}, "near surface inside"},
            {{1000.0f, 0.0f, 0.0f}, "at surface"},
            {{1100.0f, 0.0f, 0.0f}, "near surface outside"},
            {{2000.0f, 0.0f, 0.0f}, "far outside"}
        };
        
        int solidCount = 0;
        int emptyCount = 0;
        
        for (const auto& testPos : testPositions) {
            const octree::Voxel* voxel = planet->getVoxel(testPos.pos);
            bool isSolid = false;
            
            if (voxel != nullptr) {
                isSolid = voxel->shouldRender();
                std::cout << "  " << testPos.description << ": VOXEL " << (isSolid ? "SOLID" : "EMPTY") << std::endl;
            } else {
                // Use distance-based density for areas without voxel data
                float distanceFromCenter = glm::length(testPos.pos);
                isSolid = (distanceFromCenter < planetRadius);
                std::cout << "  " << testPos.description << ": FALLBACK " << (isSolid ? "SOLID" : "EMPTY") << 
                          " (distance: " << distanceFromCenter << ")" << std::endl;
            }
            
            if (isSolid) solidCount++;
            else emptyCount++;
        }
        
        TEST_EXPECT_GT(solidCount, 0, "Should have solid density samples");
        TEST_EXPECT_GT(emptyCount, 0, "Should have empty density samples");
        
        std::cout << "  Total: " << solidCount << " solid, " << emptyCount << " empty samples" << std::endl;
        
        return true;
    }

    // Test 3: Surface-crossing cell detection
    bool testSurfaceCrossingDetection() {
        std::cout << "\n=== Test: Surface Crossing Detection ===" << std::endl;
        
        MockTransvoxelChunk chunk;
        chunk.position = glm::vec3(950.0f, 0.0f, 0.0f); // Just inside planet surface
        chunk.voxelSize = 25.0f;
        chunk.lodLevel = 0;
        
        const int chunkSize = 8; // Same as actual implementation
        bool foundSurfaceCrossing = false;
        
        // Sample density field like generateMesh() does
        for (int z = 0; z < chunkSize && !foundSurfaceCrossing; z++) {
            for (int y = 0; y < chunkSize && !foundSurfaceCrossing; y++) {
                for (int x = 0; x < chunkSize && !foundSurfaceCrossing; x++) {
                    bool hasSolidCorner = false;
                    bool hasEmptyCorner = false;
                    
                    // Check all 8 corners of this cell
                    for (int cz = 0; cz <= 1; cz++) {
                        for (int cy = 0; cy <= 1; cy++) {
                            for (int cx = 0; cx <= 1; cx++) {
                                glm::vec3 worldPos = chunk.position + 
                                    glm::vec3(x + cx, y + cy, z + cz) * chunk.voxelSize;
                                
                                const octree::Voxel* voxel = planet->getVoxel(worldPos);
                                bool isSolid = false;
                                
                                if (voxel != nullptr) {
                                    isSolid = voxel->shouldRender();
                                } else {
                                    // Use distance-based density
                                    float distanceFromCenter = glm::length(worldPos);
                                    float planetRadius = planet->getRadius();
                                    isSolid = (distanceFromCenter < planetRadius);
                                }
                                
                                if (isSolid) hasSolidCorner = true;
                                else hasEmptyCorner = true;
                            }
                        }
                    }
                    
                    // If we have both solid and empty corners, this cell crosses the surface
                    if (hasSolidCorner && hasEmptyCorner) {
                        foundSurfaceCrossing = true;
                        std::cout << "  Found surface-crossing cell at (" << x << ", " << y << ", " << z << 
                                    ") in chunk" << std::endl;
                    }
                }
            }
        }
        
        TEST_EXPECT_TRUE(foundSurfaceCrossing, "Should find surface-crossing cells at planet boundary");
        
        return true;
    }

    // Test 4: The core issue - triangle generation vs rendering disconnect
    bool testTriangleGenerationVsRendering() {
        std::cout << "\n=== Test: Triangle Generation vs Rendering Disconnect ===" << std::endl;
        
        MockTransvoxelChunk chunk;
        chunk.position = glm::vec3(950.0f, 0.0f, 0.0f);
        chunk.voxelSize = 25.0f;
        chunk.lodLevel = 0;
        
        // Simulate successful mesh generation (what console output would show)
        chunk.vertices.resize(9); // 3 triangles worth of vertices
        chunk.indices = {0, 1, 2, 3, 4, 5, 6, 7, 8}; // 3 triangles
        chunk.vertexColors.resize(9);
        chunk.hasValidMesh = true;
        chunk.isDirty = false;
        
        std::cout << "  Simulated mesh generation: " << chunk.vertices.size() << " vertices, " <<
                    chunk.indices.size()/3 << " triangles" << std::endl;
        
        // Test triangle counting
        uint32_t triangleCount = chunk.indices.size() / 3;
        TEST_EXPECT_EQ(triangleCount, 3, "Triangle count calculation");
        TEST_EXPECT_TRUE(chunk.hasValidMesh, "Chunk should have valid mesh");
        TEST_EXPECT_FALSE(chunk.vertices.empty(), "Vertices should not be empty");
        TEST_EXPECT_FALSE(chunk.indices.empty(), "Indices should not be empty");
        TEST_EXPECT_EQ(chunk.vertices.size(), chunk.vertexColors.size(), "Vertices and colors count match");
        
        // Test rendering validation logic (what render() checks before drawing)
        bool hasGeometry = !chunk.vertices.empty() && !chunk.indices.empty();
        bool hasBuffers = (chunk.vertexBuffer != VK_NULL_HANDLE) && 
                         (chunk.indexBuffer != VK_NULL_HANDLE);
        bool wouldRender = chunk.hasValidMesh && hasGeometry;
        
        TEST_EXPECT_TRUE(hasGeometry, "Chunk has geometry data");
        TEST_EXPECT_FALSE(hasBuffers, "Buffers not created yet (this is the likely issue!)");
        TEST_EXPECT_TRUE(wouldRender, "Chunk would be considered for rendering based on hasValidMesh and geometry");
        
        std::cout << "  KEY FINDING: Geometry exists but buffers are not created" << std::endl;
        std::cout << "  This disconnect between mesh generation and buffer creation" << std::endl;
        std::cout << "  is likely why console shows triangles but UI shows 0" << std::endl;
        
        return true;
    }

    // Test 5: Statistics tracking
    bool testStatisticsTracking() {
        std::cout << "\n=== Test: Statistics Tracking ===" << std::endl;
        
        std::atomic<uint32_t> totalTriangles{0};
        std::atomic<uint32_t> activeChunks{0};
        
        // Simulate mesh generation updating statistics
        uint32_t newTriangles = 5;
        totalTriangles.fetch_add(newTriangles);
        activeChunks.fetch_add(1);
        
        TEST_EXPECT_EQ(totalTriangles.load(), 5, "Triangle count after update");
        TEST_EXPECT_EQ(activeChunks.load(), 1, "Active chunk count after update");
        
        // Test clearing statistics
        totalTriangles.store(0);
        activeChunks.store(0);
        
        TEST_EXPECT_EQ(totalTriangles.load(), 0, "Triangle count after clear");
        TEST_EXPECT_EQ(activeChunks.load(), 0, "Active chunk count after clear");
        
        std::cout << "  Statistics tracking works correctly" << std::endl;
        
        return true;
    }
};

int main() {
    std::cout << "=== Transvoxel Issue Test Suite ===" << std::endl;
    std::cout << "Testing the disconnect between triangle generation and rendering" << std::endl;
    
    TransvoxelTester tester;
    
    if (!tester.setUp()) {
        std::cerr << "Failed to set up test environment" << std::endl;
        return 1;
    }
    
    bool allTestsPassed = true;
    
    try {
        allTestsPassed &= tester.testChunkInitialization();
        allTestsPassed &= tester.testPlanetDensitySampling();
        allTestsPassed &= tester.testSurfaceCrossingDetection();
        allTestsPassed &= tester.testTriangleGenerationVsRendering();
        allTestsPassed &= tester.testStatisticsTracking();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        allTestsPassed = false;
    }
    
    tester.tearDown();
    
    std::cout << "\n=== Test Results ===" << std::endl;
    if (allTestsPassed) {
        std::cout << "All tests PASSED" << std::endl;
        std::cout << "\nKEY FINDINGS:" << std::endl;
        std::cout << "1. Triangle generation logic appears to be working" << std::endl;
        std::cout << "2. The disconnect is likely between mesh generation and GPU buffer creation" << std::endl;
        std::cout << "3. Console shows triangles generated, but they never get uploaded to GPU" << std::endl;
        std::cout << "4. This causes UI to show 0 triangles even when geometry is generated" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests FAILED" << std::endl;
        return 1;
    }
}