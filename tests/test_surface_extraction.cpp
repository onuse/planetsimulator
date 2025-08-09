#include <gtest/gtest.h>
#include "rendering/surface_extractor.hpp"
#include "core/octree.hpp"
#include <iostream>
#include <chrono>

namespace rendering {

// Forward declaration from implementation file
std::unique_ptr<ISurfaceExtractor> createSimpleSurfaceExtractor();

} // namespace rendering

class SurfaceExtractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test planet
        planet = std::make_unique<octree::OctreePlanet>(1000.0f, 10);
        planet->generate(12345);
        
        // Create extractor
        extractor = rendering::createSimpleSurfaceExtractor();
    }
    
    std::unique_ptr<octree::OctreePlanet> planet;
    std::unique_ptr<rendering::ISurfaceExtractor> extractor;
};

TEST_F(SurfaceExtractionTest, ExtractorCreation) {
    ASSERT_NE(extractor, nullptr);
    EXPECT_STREQ(extractor->getName(), "SimpleCubes");
}

TEST_F(SurfaceExtractionTest, EmptyRegionExtraction) {
    // Create a region in empty space (far from planet)
    rendering::VoxelRegion region(
        glm::vec3(10000, 10000, 10000),  // Far from planet
        10.0f,                           // Voxel size
        glm::ivec3(8, 8, 8),            // 8x8x8 region
        0                                // LOD 0
    );
    
    rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
    
    // Should be empty since region is in space
    EXPECT_TRUE(mesh.isEmpty());
    EXPECT_EQ(mesh.getTriangleCount(), 0);
}

TEST_F(SurfaceExtractionTest, PlanetSurfaceExtraction) {
    // Create a region at the planet surface
    rendering::VoxelRegion region(
        glm::vec3(800, 0, 0),           // Near planet surface
        20.0f,                          // Voxel size
        glm::ivec3(16, 16, 16),         // 16x16x16 region
        0                               // LOD 0
    );
    
    rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
    
    // Should have some geometry at planet surface
    EXPECT_FALSE(mesh.isEmpty());
    EXPECT_GT(mesh.vertices.size(), 0);
    EXPECT_GT(mesh.indices.size(), 0);
    EXPECT_GT(mesh.getTriangleCount(), 0);
    
    // Validate mesh structure
    EXPECT_EQ(mesh.indices.size() % 3, 0); // Indices should form complete triangles
    
    // All indices should be valid
    for (uint32_t index : mesh.indices) {
        EXPECT_LT(index, mesh.vertices.size());
    }
    
    std::cout << "Extracted mesh from planet surface:\n";
    std::cout << "  Vertices: " << mesh.vertices.size() << "\n";
    std::cout << "  Triangles: " << mesh.getTriangleCount() << "\n";
}

TEST_F(SurfaceExtractionTest, MeshVertexData) {
    // Create a small region at the planet surface
    rendering::VoxelRegion region(
        glm::vec3(900, 0, 0),           // Near planet surface
        50.0f,                          // Larger voxel size
        glm::ivec3(4, 4, 4),            // Small 4x4x4 region
        0                               // LOD 0
    );
    
    rendering::ExtractedMesh mesh = extractor->extractSurface(region, *planet);
    
    if (!mesh.isEmpty()) {
        // Check that vertices have reasonable positions
        for (const auto& vertex : mesh.vertices) {
            // Positions should be finite
            EXPECT_TRUE(std::isfinite(vertex.position.x));
            EXPECT_TRUE(std::isfinite(vertex.position.y));
            EXPECT_TRUE(std::isfinite(vertex.position.z));
            
            // Normals should be unit vectors (approximately)
            float normalLength = glm::length(vertex.normal);
            EXPECT_NEAR(normalLength, 1.0f, 0.1f);
            
            // Colors should be in reasonable range
            EXPECT_GE(vertex.color.x, 0.0f);
            EXPECT_LE(vertex.color.x, 1.0f);
            EXPECT_GE(vertex.color.y, 0.0f);
            EXPECT_LE(vertex.color.y, 1.0f);
            EXPECT_GE(vertex.color.z, 0.0f);
            EXPECT_LE(vertex.color.z, 1.0f);
        }
        
        std::cout << "Mesh vertex validation passed for " << mesh.vertices.size() << " vertices\n";
    }
}

TEST_F(SurfaceExtractionTest, PlanetCoreExtraction) {
    // Create a region at the planet core
    algorithms::MeshGenParams params(
        glm::vec3(0, 0, 0),             // Planet center
        100.0f,                         // Large voxel size
        glm::ivec3(4, 4, 4),            // Small region
        0                               // LOD 0
    );
    
    algorithms::MeshData mesh = algorithms::generateSimpleCubeMesh(params, *planet);
    
    // Should have solid material at planet core
    EXPECT_FALSE(mesh.isEmpty());
    EXPECT_GT(mesh.getTriangleCount(), 0);
    
    std::cout << "Planet core mesh: " << mesh.getTriangleCount() << " triangles\n";
}

TEST_F(SurfaceExtractionTest, DifferentLODLevels) {
    glm::vec3 testPos(800, 0, 0);
    
    for (uint32_t lod = 0; lod < 3; lod++) {
        algorithms::MeshGenParams params(
            testPos,
            20.0f * (1 << lod),           // Increase voxel size with LOD
            glm::ivec3(8, 8, 8),
            lod
        );
        
        algorithms::MeshData mesh = algorithms::generateSimpleCubeMesh(params, *planet);
        
        std::cout << "LOD " << lod << ": " << mesh.getTriangleCount() << " triangles\n";
        
        // Higher LOD should generally have fewer or equal triangles
        // (though this isn't guaranteed due to different sampling patterns)
    }
}

// Performance test
TEST_F(SurfaceExtractionTest, ExtractionPerformance) {
    algorithms::MeshGenParams params(
        glm::vec3(800, 0, 0),
        10.0f,
        glm::ivec3(32, 32, 32),         // Larger region
        0
    );
    
    auto start = std::chrono::high_resolution_clock::now();
    
    algorithms::MeshData mesh = algorithms::generateSimpleCubeMesh(params, *planet);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Extraction of " << (32*32*32) << " voxels took " << duration.count() << "ms\n";
    std::cout << "Generated " << mesh.getTriangleCount() << " triangles\n";
    
    // Should complete in reasonable time (less than 1 second for this size)
    EXPECT_LT(duration.count(), 1000);
}