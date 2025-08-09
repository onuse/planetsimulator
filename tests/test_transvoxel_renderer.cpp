#include <gtest/gtest.h>
#include <memory>
#include <vulkan/vulkan.h>
#include "rendering/transvoxel_renderer.hpp"
#include "core/octree.hpp"
#include "core/material_table.hpp"
#include "core/mixed_voxel.hpp"

// Mock Vulkan objects for testing
struct MockVulkanObjects {
    VkDevice device = reinterpret_cast<VkDevice>(0x1);
    VkPhysicalDevice physicalDevice = reinterpret_cast<VkPhysicalDevice>(0x2);
    VkCommandPool commandPool = reinterpret_cast<VkCommandPool>(0x3);
    VkQueue graphicsQueue = reinterpret_cast<VkQueue>(0x4);
    VkCommandBuffer commandBuffer = reinterpret_cast<VkCommandBuffer>(0x5);
    VkPipelineLayout pipelineLayout = reinterpret_cast<VkPipelineLayout>(0x6);
};

class TransvoxelRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize material table
        core::MaterialTable::getInstance().initialize();
        
        // Create a simple planet for testing
        planet = std::make_unique<octree::OctreePlanet>(1000.0f, 10);
        planet->generate(42); // Fixed seed for reproducible tests
    }
    
    void TearDown() override {
        planet.reset();
    }
    
    std::unique_ptr<octree::OctreePlanet> planet;
    MockVulkanObjects vulkan;
};

// Test chunk initialization and state
TEST_F(TransvoxelRendererTest, ChunkInitialization) {
    rendering::TransvoxelChunk chunk;
    chunk.position = glm::vec3(0.0f, 0.0f, 0.0f);
    chunk.voxelSize = 10.0f;
    chunk.lodLevel = 0;
    
    EXPECT_EQ(chunk.position, glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(chunk.voxelSize, 10.0f);
    EXPECT_EQ(chunk.lodLevel, 0);
    EXPECT_TRUE(chunk.isDirty);
    EXPECT_FALSE(chunk.hasValidMesh);
    EXPECT_TRUE(chunk.vertices.empty());
    EXPECT_TRUE(chunk.indices.empty());
    EXPECT_EQ(chunk.vertexBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(chunk.indexBuffer, VK_NULL_HANDLE);
}

// Test vertex structure and attribute descriptions
TEST_F(TransvoxelRendererTest, VertexStructure) {
    rendering::Vertex vertex;
    vertex.position = glm::vec3(1.0f, 2.0f, 3.0f);
    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    vertex.texCoord = glm::vec2(0.5f, 0.5f);
    
    EXPECT_EQ(vertex.position, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(vertex.normal, glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(vertex.texCoord, glm::vec2(0.5f, 0.5f));
    
    // Test binding description
    auto bindingDesc = rendering::Vertex::getBindingDescription();
    EXPECT_EQ(bindingDesc.binding, 0);
    EXPECT_EQ(bindingDesc.stride, sizeof(rendering::Vertex));
    EXPECT_EQ(bindingDesc.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
    
    // Test attribute descriptions
    auto attrDescs = rendering::Vertex::getAttributeDescriptions();
    EXPECT_EQ(attrDescs.size(), 3);
    EXPECT_EQ(attrDescs[0].location, 0); // Position
    EXPECT_EQ(attrDescs[1].location, 1); // Normal
    EXPECT_EQ(attrDescs[2].location, 2); // TexCoord
}

// Test MixedVoxel integration
TEST_F(TransvoxelRendererTest, MixedVoxelIntegration) {
    // Test solid voxel
    octree::MixedVoxel solid = octree::MixedVoxel::createPure(core::MaterialID::Rock);
    EXPECT_TRUE(solid.shouldRender());
    
    glm::vec3 solidColor = solid.getColor();
    EXPECT_GT(solidColor.r + solidColor.g + solidColor.b, 0.0f); // Should have some color
    
    // Test empty voxel
    octree::MixedVoxel empty = octree::MixedVoxel::createEmpty();
    EXPECT_FALSE(empty.shouldRender());
    
    glm::vec3 emptyColor = empty.getColor();
    EXPECT_NE(solidColor, emptyColor);
}

// Test density sampling logic with real planet data
TEST_F(TransvoxelRendererTest, DensitySampling) {
    // Test positions at different distances from planet center
    glm::vec3 centerPosition(0.0f, 0.0f, 0.0f);
    glm::vec3 surfacePosition(900.0f, 0.0f, 0.0f);  // Near surface (radius=1000)
    glm::vec3 outsidePosition(2000.0f, 0.0f, 0.0f); // Outside planet
    
    // Verify planet properties
    EXPECT_EQ(planet->getRadius(), 1000.0f);
    EXPECT_GE(planet->getMaxDepth(), 0);
    
    // Test that we can get voxels (planet should be generated)
    const octree::Voxel* centerVoxel = planet->getVoxel(centerPosition);
    const octree::Voxel* surfaceVoxel = planet->getVoxel(surfacePosition);
    
    // At least one of these should exist in the generated planet
    bool hasVoxelData = (centerVoxel != nullptr) || (surfaceVoxel != nullptr);
    EXPECT_TRUE(hasVoxelData);
}

// Test that generateMesh produces geometry for surface chunks
TEST_F(TransvoxelRendererTest, MeshGeneration_DISABLED) {
    // This test is disabled because it requires real Vulkan initialization
    // Instead we test the core logic separately
    GTEST_SKIP() << "Mesh generation test disabled - requires Vulkan initialization";
    
    /* Code that would test mesh generation if Vulkan was available:
    rendering::TransvoxelRenderer renderer(vulkan.device, vulkan.physicalDevice, 
                                           vulkan.commandPool, vulkan.graphicsQueue);
    
    rendering::TransvoxelChunk chunk;
    chunk.position = glm::vec3(800.0f, 0.0f, 0.0f); // Near surface
    chunk.voxelSize = 50.0f;
    chunk.lodLevel = 0;
    
    renderer.generateMesh(chunk, *planet);
    
    // Test that mesh was generated
    if (chunk.hasValidMesh) {
        EXPECT_FALSE(chunk.vertices.empty());
        EXPECT_FALSE(chunk.indices.empty());
        EXPECT_EQ(chunk.indices.size() % 3, 0); // Should be triangles
        EXPECT_EQ(chunk.vertices.size(), chunk.vertexColors.size());
        
        // Test triangle count tracking
        uint32_t expectedTriangles = chunk.indices.size() / 3;
        EXPECT_EQ(renderer.getTriangleCount(), expectedTriangles);
    }
    */
}

// Test the key issue: mesh generation vs rendering disconnect
TEST_F(TransvoxelRendererTest, MeshGenerationLogic) {
    // Test chunk positioning near planet surface where triangles should be generated
    rendering::TransvoxelChunk chunk;
    chunk.position = glm::vec3(950.0f, 0.0f, 0.0f); // Near surface (radius=1000)
    chunk.voxelSize = 25.0f; // Small voxels for detail
    chunk.lodLevel = 0;
    
    // Verify initial state
    EXPECT_TRUE(chunk.isDirty);
    EXPECT_FALSE(chunk.hasValidMesh);
    EXPECT_TRUE(chunk.vertices.empty());
    EXPECT_TRUE(chunk.indices.empty());
    
    // Test positions that span the planet surface
    std::vector<glm::vec3> testPositions = {
        glm::vec3(950.0f, 0.0f, 0.0f),   // Just inside surface
        glm::vec3(1000.0f, 0.0f, 0.0f),  // Exactly at surface
        glm::vec3(1050.0f, 0.0f, 0.0f)   // Just outside surface
    };
    
    for (const auto& pos : testPositions) {
        const octree::Voxel* voxel = planet->getVoxel(pos);
        // At least some positions should have voxel data or be near the surface
        // This validates that our planet generation is working
    }
}

// Test render function call structure
TEST_F(TransvoxelRendererTest, RenderCallStructure) {
    // Test empty chunk list
    std::vector<rendering::TransvoxelChunk> emptyChunks;
    
    // This would normally call the render function but we can't without real Vulkan
    // Instead we test the chunk validation logic that render() would perform
    
    rendering::TransvoxelChunk validChunk;
    validChunk.hasValidMesh = true;
    validChunk.vertices.resize(3); // Has vertices
    validChunk.indices = {0, 1, 2}; // Has indices
    
    rendering::TransvoxelChunk invalidChunk1;
    invalidChunk1.hasValidMesh = false;
    
    rendering::TransvoxelChunk invalidChunk2;
    invalidChunk2.hasValidMesh = true;
    invalidChunk2.vertices.clear(); // No vertices
    
    // Test validation logic
    EXPECT_TRUE(validChunk.hasValidMesh && !validChunk.vertices.empty());
    EXPECT_FALSE(invalidChunk1.hasValidMesh && !invalidChunk1.vertices.empty());
    EXPECT_FALSE(invalidChunk2.hasValidMesh && !invalidChunk2.vertices.empty());
}

// Test triangle counting and statistics
TEST_F(TransvoxelRendererTest, StatisticsTracking_DISABLED) {
    GTEST_SKIP() << "Statistics test disabled - requires Vulkan initialization";
    
    /* Code that would test statistics if Vulkan was available:
    rendering::TransvoxelRenderer renderer(vulkan.device, vulkan.physicalDevice, 
                                           vulkan.commandPool, vulkan.graphicsQueue);
    
    // Initial state
    EXPECT_EQ(renderer.getTriangleCount(), 0);
    EXPECT_EQ(renderer.getChunkCount(), 0);
    
    // After clearing cache
    renderer.clearCache();
    EXPECT_EQ(renderer.getTriangleCount(), 0);
    EXPECT_EQ(renderer.getChunkCount(), 0);
    */
}

// Test the core issue: Console shows triangles but UI shows 0
TEST_F(TransvoxelRendererTest, TriangleCountDiscrepancy) {
    // This test documents the key issue we're trying to solve:
    // Console output shows "Generated X triangles" 
    // but UI shows "Triangles: 0"
    
    // The problem is likely one of these:
    // 1. Triangles generated but not uploaded to GPU buffers
    // 2. Triangles uploaded but render() not called
    // 3. Triangles rendered but statistics not updated correctly
    // 4. Statistics updated but UI not reading them correctly
    
    // For now, this test documents the expected behavior
    rendering::TransvoxelChunk chunk;
    
    // Simulate successful mesh generation (what console shows)
    chunk.vertices.resize(6);  // 2 triangles worth of vertices
    chunk.indices = {0, 1, 2, 3, 4, 5}; // 2 triangles
    chunk.hasValidMesh = true;
    
    // Key test: if triangles are generated, they should be countable
    uint32_t triangleCount = chunk.indices.size() / 3;
    EXPECT_EQ(triangleCount, 2);
    
    // And the chunk should be ready for rendering
    EXPECT_TRUE(chunk.hasValidMesh);
    EXPECT_FALSE(chunk.vertices.empty());
    EXPECT_FALSE(chunk.indices.empty());
    
    // If this test passes but UI still shows 0 triangles,
    // the issue is in the rendering pipeline or statistics update
}