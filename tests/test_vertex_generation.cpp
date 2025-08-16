#include <iostream>
#include <vector>
#include <set>
#include <chrono>
#include <iomanip>
#include <glm/glm.hpp>
#include "../include/core/vertex_generator.hpp"

using namespace PlanetRenderer;

// Test framework
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

#define ASSERT_NEAR(a, b, epsilon) \
    if (std::abs((a) - (b)) > (epsilon)) { \
        std::cout << "FAILED\n"; \
        std::cout << "  Expected " << (a) << " ≈ " << (b) << " (ε=" << (epsilon) << ")\n"; \
        std::cout << "  Actual difference: " << std::abs((a) - (b)) << "\n"; \
        std::cout << "  At line " << __LINE__ << "\n"; \
        exit(1); \
    }

// ============================================================================
// Test 1: Basic vertex generation
// ============================================================================
TEST(BasicVertexGeneration) {
    SimpleVertexGenerator generator;
    
    // Generate a vertex at a known position
    glm::dvec3 cubePos(1.0, 0.0, 0.0);
    VertexID id = VertexID::fromCubePosition(cubePos);
    
    CachedVertex vertex = generator.getVertex(id);
    
    // Check that position is on sphere surface
    float radius = glm::length(vertex.position);
    ASSERT_NEAR(radius, 6371000.0f, 1.0f);
    
    // Check that normal is normalized
    float normalLength = glm::length(vertex.normal);
    ASSERT_NEAR(normalLength, 1.0f, 0.001f);
}

// ============================================================================
// Test 2: Vertex caching works correctly
// ============================================================================
TEST(VertexCaching) {
    SimpleVertexGenerator generator;
    
    // Create vertex ID
    glm::dvec3 cubePos(0.5, 0.5, 0.5);
    VertexID id = VertexID::fromCubePosition(cubePos);
    
    // Get vertex twice
    CachedVertex v1 = generator.getVertex(id);
    CachedVertex v2 = generator.getVertex(id);
    
    // Should be exactly the same (from cache)
    ASSERT(v1.position == v2.position);
    ASSERT(v1.normal == v2.normal);
    
    // Check cache statistics
    auto stats = generator.getStats();
    ASSERT(stats.totalRequests == 2);
    ASSERT(stats.cacheHits == 1);
    ASSERT(stats.cacheMisses == 1);
    
    // Cache hit rate should be 50%
    ASSERT_NEAR(generator.getCacheHitRate(), 0.5f, 0.001f);
}

// ============================================================================
// Test 3: Same VertexID always produces same position
// ============================================================================
TEST(DeterministicGeneration) {
    SimpleVertexGenerator gen1;
    SimpleVertexGenerator gen2;
    
    // Generate same vertex in two different generators
    VertexID id = VertexID::fromFaceUV(0, 0.5, 0.5);
    
    CachedVertex v1 = gen1.getVertex(id);
    CachedVertex v2 = gen2.getVertex(id);
    
    // Positions should be exactly the same
    ASSERT(glm::length(v1.position - v2.position) < 0.001f);
    ASSERT(glm::length(v1.normal - v2.normal) < 0.001f);
}

// ============================================================================
// Test 4: Boundary vertices are exactly shared
// ============================================================================
TEST(BoundaryVertexSharing) {
    SimpleVertexGenerator generator;
    
    // Test corner vertex at (1,1,1)
    VertexID idFromX = VertexID::fromFaceUV(0, 1.0, 1.0);  // +X face
    VertexID idFromY = VertexID::fromFaceUV(2, 1.0, 1.0);  // +Y face
    VertexID idFromZ = VertexID::fromFaceUV(4, 1.0, 1.0);  // +Z face
    
    CachedVertex vx = generator.getVertex(idFromX);
    CachedVertex vy = generator.getVertex(idFromY);
    CachedVertex vz = generator.getVertex(idFromZ);
    
    // All three should have the exact same position
    ASSERT(glm::length(vx.position - vy.position) < 0.001f);
    ASSERT(glm::length(vy.position - vz.position) < 0.001f);
    ASSERT(glm::length(vx.position - vz.position) < 0.001f);
    
    // Check cache worked - should have 3 requests but potentially 3 different IDs
    // (depending on canonical resolution)
    auto stats = generator.getStats();
    ASSERT(stats.totalRequests == 3);
}

// ============================================================================
// Test 5: Batch generation
// ============================================================================
TEST(BatchGeneration) {
    SimpleVertexGenerator generator;
    
    // Create a batch of vertex IDs
    std::vector<VertexID> ids;
    for (int i = 0; i < 100; i++) {
        double t = i / 100.0;
        ids.push_back(VertexID::fromFaceUV(0, t, t));
    }
    
    // Generate batch
    std::vector<CachedVertex> vertices;
    generator.generateBatch(ids, vertices);
    
    ASSERT(vertices.size() == 100);
    
    // All vertices should be on sphere surface
    for (const auto& v : vertices) {
        float radius = glm::length(v.position);
        ASSERT_NEAR(radius, 6371000.0f, 100.0f);
    }
    
    auto stats = generator.getStats();
    ASSERT(stats.batchRequests == 1);
}

// ============================================================================
// Test 6: Cache hit rate improves with repeated access
// ============================================================================
TEST(CacheEfficiency) {
    SimpleVertexGenerator generator;
    
    // Generate same vertices multiple times
    std::vector<VertexID> ids;
    for (int i = 0; i < 10; i++) {
        ids.push_back(VertexID::fromFaceUV(0, i * 0.1, i * 0.1));
    }
    
    // Access each vertex 5 times
    for (int round = 0; round < 5; round++) {
        for (const auto& id : ids) {
            generator.getVertex(id);
        }
    }
    
    auto stats = generator.getStats();
    ASSERT(stats.totalRequests == 50);  // 10 vertices * 5 rounds
    ASSERT(stats.cacheMisses == 10);    // Only first access is a miss
    ASSERT(stats.cacheHits == 40);      // Rest are hits
    
    // Cache hit rate should be 80%
    ASSERT_NEAR(generator.getCacheHitRate(), 0.8f, 0.001f);
}

// ============================================================================
// Test 7: Vertex buffer manager
// ============================================================================
TEST(VertexBufferManager) {
    VertexBufferManager bufferMgr;
    SimpleVertexGenerator generator;
    
    // Create some vertices
    std::vector<VertexID> ids;
    for (int i = 0; i < 5; i++) {
        ids.push_back(VertexID::fromFaceUV(0, i * 0.2, 0.5));
    }
    
    // Get or create indices
    std::vector<uint32_t> indices;
    for (const auto& id : ids) {
        uint32_t idx = bufferMgr.getOrCreateIndex(id, generator);
        indices.push_back(idx);
    }
    
    ASSERT(bufferMgr.size() == 5);
    
    // Request same vertices again - should get same indices
    for (size_t i = 0; i < ids.size(); i++) {
        uint32_t idx = bufferMgr.getOrCreateIndex(ids[i], generator);
        ASSERT(idx == indices[i]);
    }
    
    // Buffer size shouldn't have changed
    ASSERT(bufferMgr.size() == 5);
}

// ============================================================================
// Test 8: Global system singleton
// ============================================================================
TEST(GlobalVertexSystem) {
    auto& system1 = VertexGeneratorSystem::getInstance();
    auto& system2 = VertexGeneratorSystem::getInstance();
    
    // Should be the same instance
    ASSERT(&system1 == &system2);
    
    // Test basic functionality
    system1.setPlanetRadius(1000000.0);
    
    auto& gen = system1.getGenerator();
    auto& bufMgr = system1.getBufferManager();
    
    VertexID id = VertexID::fromFaceUV(0, 0.5, 0.5);
    uint32_t idx = bufMgr.getOrCreateIndex(id, gen);
    
    ASSERT(idx == 0);  // First vertex should have index 0
    
    // Reset and verify clearing works
    system1.reset();
    ASSERT(bufMgr.size() == 0);
    ASSERT(gen.getCacheSize() == 0);
}

// ============================================================================
// Test 9: Performance benchmark
// ============================================================================
TEST(PerformanceBenchmark) {
    SimpleVertexGenerator generator;
    
    const int VERTEX_COUNT = 10000;
    
    // Generate unique vertices
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < VERTEX_COUNT; i++) {
        double t = i / double(VERTEX_COUNT);
        VertexID id = VertexID::fromFaceUV(0, t, t);
        generator.getVertex(id);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto uniqueTime = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Access cached vertices
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < VERTEX_COUNT; i++) {
        double t = i / double(VERTEX_COUNT);
        VertexID id = VertexID::fromFaceUV(0, t, t);
        generator.getVertex(id);
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto cachedTime = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "\n  Performance: ";
    std::cout << VERTEX_COUNT << " unique vertices in " << uniqueTime << "ms, ";
    std::cout << "cached access in " << cachedTime << "ms ";
    std::cout << "(speedup: " << (uniqueTime / cachedTime) << "x) ";
    
    // Cached access should be significantly faster
    ASSERT(cachedTime < uniqueTime * 0.5);  // At least 2x faster
}

// ============================================================================
// Main test runner
// ============================================================================
int main() {
    std::cout << "=== Vertex Generation System Tests ===\n\n";
    
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