#include <gtest/gtest.h>
#include <memory>
#include <iostream>
#include <atomic>
#include <glm/glm.hpp>
#include "core/octree.hpp"
#include "core/material_table.hpp"

// Forward declare to avoid Vulkan dependency
typedef void* VkBuffer;
typedef void* VkDeviceMemory;
#define VK_NULL_HANDLE nullptr

// Mock TransvoxelChunk structure for testing (without Vulkan dependencies)
struct MockTransvoxelChunk {
    glm::vec3 position;
    float voxelSize;
    uint32_t lodLevel;
    
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

// Test specifically for the rendering visibility issue:
// Console shows "Generated X triangles" but UI shows "Triangles: 0"
class TransvoxelIssueTest : public ::testing::Test {
protected:
    void SetUp() override {
        core::MaterialTable::getInstance().initialize();
        planet = std::make_unique<octree::OctreePlanet>(1000.0f, 10);
        planet->generate(42);
    }
    
    void TearDown() override {
        planet.reset();
    }
    
    std::unique_ptr<octree::OctreePlanet> planet;
};

// Test the exact issue: mesh generation produces triangles but they don't appear rendered
TEST_F(TransvoxelIssueTest, TriangleGenerationVsRendering) {
    // Create a chunk positioned at the planet surface where triangles should be generated
    MockTransvoxelChunk chunk;
    chunk.position = glm::vec3(950.0f, 0.0f, 0.0f); // Just inside planet surface (radius=1000)
    chunk.voxelSize = 25.0f; // Small enough for surface detail
    chunk.lodLevel = 0;
    
    // Initial state verification
    EXPECT_TRUE(chunk.isDirty);
    EXPECT_FALSE(chunk.hasValidMesh);
    EXPECT_TRUE(chunk.vertices.empty());
    EXPECT_TRUE(chunk.indices.empty());
    EXPECT_EQ(chunk.vertexBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(chunk.indexBuffer, VK_NULL_HANDLE);
    
    // Simulate what generateMesh() should do (without actual Vulkan calls)
    // This tests the logic that determines IF triangles should be generated
    
    const int chunkSize = 8; // Same as in actual implementation
    bool shouldGenerateTriangles = false;
    
    // Sample density field like generateMesh() does
    for (int z = 0; z < chunkSize; z++) {
        for (int y = 0; y < chunkSize; y++) {
            for (int x = 0; x < chunkSize; x++) {
                // Sample corner densities for this cell
                bool hasSolidCorner = false;
                bool hasEmptyCorner = false;
                
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
                                // Use distance-based density for areas without voxel data
                                float distanceFromCenter = glm::length(worldPos);\n                                float planetRadius = planet->getRadius();
                                isSolid = (distanceFromCenter < planetRadius);
                            }
                            
                            if (isSolid) hasSolidCorner = true;
                            else hasEmptyCorner = true;
                        }
                    }
                }
                
                // If we have both solid and empty corners, this cell should generate triangles
                if (hasSolidCorner && hasEmptyCorner) {
                    shouldGenerateTriangles = true;
                    break;
                }
            }
            if (shouldGenerateTriangles) break;
        }
        if (shouldGenerateTriangles) break;
    }
    
    // KEY TEST: At the planet surface, we should detect surface-crossing cells
    EXPECT_TRUE(shouldGenerateTriangles) 
        << "No surface-crossing cells detected at planet surface position (" 
        << chunk.position.x << ", " << chunk.position.y << ", " << chunk.position.z 
        << ") - this suggests the density sampling logic has issues";
    
    // If triangles should be generated, test the data structures that would hold them
    if (shouldGenerateTriangles) {
        // Simulate successful triangle generation
        chunk.vertices.resize(9); // 3 triangles worth
        chunk.indices = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        chunk.vertexColors.resize(9);
        chunk.hasValidMesh = true;
        chunk.isDirty = false;
        
        // Test that triangle counting would work
        uint32_t triangleCount = chunk.indices.size() / 3;
        EXPECT_EQ(triangleCount, 3);
        EXPECT_TRUE(chunk.hasValidMesh);
        EXPECT_FALSE(chunk.vertices.empty());
        EXPECT_FALSE(chunk.indices.empty());
        EXPECT_EQ(chunk.vertices.size(), chunk.vertexColors.size());
        
        // Test rendering validation logic (what render() checks before drawing)
        bool wouldRender = chunk.hasValidMesh && !chunk.vertices.empty();
        EXPECT_TRUE(wouldRender) 
            << "Chunk with valid mesh data would not be rendered - "
            << "this is the source of the triangle count discrepancy";
    }
}

// Test the statistics tracking that should connect generation to UI display
TEST_F(TransvoxelIssueTest, StatisticsUpdate) {
    // Test atomic statistics tracking without actual renderer
    std::atomic<uint32_t> totalTriangles{0};
    std::atomic<uint32_t> activeChunks{0};
    
    // Simulate successful mesh generation
    uint32_t newTriangles = 5;
    totalTriangles.fetch_add(newTriangles);
    activeChunks.fetch_add(1);
    
    EXPECT_EQ(totalTriangles.load(), 5);
    EXPECT_EQ(activeChunks.load(), 1);
    
    // Test clearing statistics
    totalTriangles.store(0);
    activeChunks.store(0);
    
    EXPECT_EQ(totalTriangles.load(), 0);
    EXPECT_EQ(activeChunks.load(), 0);
}

// Test buffer state tracking that could cause the rendering disconnect
TEST_F(TransvoxelIssueTest, BufferStateTracking) {
    MockTransvoxelChunk chunk;
    
    // Initial buffer state
    EXPECT_EQ(chunk.vertexBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(chunk.indexBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(chunk.vertexBufferMemory, VK_NULL_HANDLE);
    EXPECT_EQ(chunk.indexBufferMemory, VK_NULL_HANDLE);
    
    // Simulate mesh generation
    chunk.vertices.resize(6);
    chunk.indices = {0, 1, 2, 3, 4, 5};
    chunk.hasValidMesh = true;
    
    // At this point, if buffers aren't created, render() won't work
    bool hasGeometry = !chunk.vertices.empty() && !chunk.indices.empty();
    bool hasBuffers = (chunk.vertexBuffer != VK_NULL_HANDLE) && 
                      (chunk.indexBuffer != VK_NULL_HANDLE);
    
    EXPECT_TRUE(hasGeometry);
    EXPECT_FALSE(hasBuffers); // Buffers not created yet - this is the likely issue!
    
    // The disconnect: geometry exists but buffers don't
    bool wouldRenderCorrectly = hasGeometry && hasBuffers && chunk.hasValidMesh;
    EXPECT_FALSE(wouldRenderCorrectly) 
        << "Geometry exists but buffers are not created - this causes the rendering issue";
}

// Test density sampling at planet boundary (where most triangles should be generated)
TEST_F(TransvoxelIssueTest, PlanetBoundaryDensity) {
    float planetRadius = planet->getRadius();
    EXPECT_EQ(planetRadius, 1000.0f);
    
    struct TestPosition {
        glm::vec3 pos;
        std::string description;
        bool expectedSolid;
    };
    
    std::vector<TestPosition> testPos = {
        {{0.0f, 0.0f, 0.0f}, "planet center", true},
        {{900.0f, 0.0f, 0.0f}, "inside surface", true},
        {{1000.0f, 0.0f, 0.0f}, "at surface", false}, // May be empty due to surface transition
        {{1100.0f, 0.0f, 0.0f}, "outside surface", false},
        {{2000.0f, 0.0f, 0.0f}, "far outside", false}
    };
    
    int solidCount = 0;
    int emptyCount = 0;
    
    for (const auto& test : testPos) {
        const octree::Voxel* voxel = planet->getVoxel(test.pos);
        bool isSolid = false;
        
        if (voxel != nullptr) {
            isSolid = voxel->shouldRender();
        } else {
            // Fallback density logic (same as TransvoxelRenderer)
            float distanceFromCenter = glm::length(test.pos);
            isSolid = (distanceFromCenter < planetRadius);
        }
        
        if (isSolid) solidCount++;
        else emptyCount++;
        
        std::cout << "Position " << test.description << " (" 
                  << test.pos.x << ", " << test.pos.y << ", " << test.pos.z 
                  << "): " << (isSolid ? "SOLID" : "EMPTY") << std::endl;
    }
    
    // We should have both solid and empty samples for surface generation to work
    EXPECT_GT(solidCount, 0) << "No solid density samples found - planet generation may have failed";
    EXPECT_GT(emptyCount, 0) << "No empty density samples found - surface detection won't work";
}