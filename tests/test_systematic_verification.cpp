#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/global_patch_generator.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

// Test result tracking
struct TestResult {
    std::string name;
    bool passed;
    std::string details;
};

std::vector<TestResult> results;

void reportTest(const std::string& name, bool passed, const std::string& details) {
    results.push_back({name, passed, details});
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if (!details.empty()) {
        std::cout << "       " << details << std::endl;
    }
}

// TEST 1: Verify that patches at cube face boundaries have the expected bounds
void test1_PatchBoundaries() {
    std::cout << "\n=== TEST 1: Patch Boundaries ===" << std::endl;
    
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    config.maxLevel = 3;
    
    core::SphericalQuadtree quadtree(config, densityField);
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    // Check face 0 (+X) patches - should have X=1.0
    bool face0Correct = true;
    for (const auto& patch : patches) {
        if (patch.faceId == 0) {
            glm::dvec3 range = patch.maxBounds - patch.minBounds;
            if (range.x > 0.001) {
                face0Correct = false;
                reportTest("Face 0 X-dimension fixed", false, 
                    "Patch has non-zero X range: " + std::to_string(range.x));
                break;
            }
            if (std::abs(patch.center.x - 1.0) > 0.001 && std::abs(patch.center.x + 1.0) > 0.001) {
                face0Correct = false;
                reportTest("Face 0 X-position", false,
                    "Patch center X not at ±1.0: " + std::to_string(patch.center.x));
                break;
            }
        }
    }
    if (face0Correct) {
        reportTest("Face 0 patches have correct X bounds", true, "");
    }
    
    // Check for patches that touch edges
    int edgePatchCount = 0;
    for (const auto& patch : patches) {
        bool atEdge = false;
        if (std::abs(patch.maxBounds.x - 1.0) < 0.001 || std::abs(patch.minBounds.x + 1.0) < 0.001) atEdge = true;
        if (std::abs(patch.maxBounds.y - 1.0) < 0.001 || std::abs(patch.minBounds.y + 1.0) < 0.001) atEdge = true;
        if (std::abs(patch.maxBounds.z - 1.0) < 0.001 || std::abs(patch.minBounds.z + 1.0) < 0.001) atEdge = true;
        if (atEdge) edgePatchCount++;
    }
    
    reportTest("Edge patches exist", edgePatchCount > 0, 
        "Found " + std::to_string(edgePatchCount) + " patches at cube edges");
}

// TEST 2: Verify transform matrices are correct
void test2_TransformMatrices() {
    std::cout << "\n=== TEST 2: Transform Matrices ===" << std::endl;
    
    // Test a patch at the +X face edge
    core::GlobalPatchGenerator::GlobalPatch patch;
    patch.minBounds = glm::vec3(1.0f, 0.75f, 0.75f);
    patch.maxBounds = glm::vec3(1.0f, 1.0f, 1.0f);
    patch.center = (patch.minBounds + patch.maxBounds) * 0.5f;
    patch.level = 2;
    patch.faceId = 0;
    
    glm::dmat4 transform = patch.createTransform();
    
    // Check that UV corners map to expected cube positions
    glm::dvec4 corners[4] = {
        glm::dvec4(0.0, 0.0, 0.0, 1.0),
        glm::dvec4(1.0, 0.0, 0.0, 1.0),
        glm::dvec4(1.0, 1.0, 0.0, 1.0),
        glm::dvec4(0.0, 1.0, 0.0, 1.0)
    };
    
    bool cornersCorrect = true;
    for (int i = 0; i < 4; i++) {
        glm::dvec3 result = glm::dvec3(transform * corners[i]);
        
        // Check for (0,0,0)
        if (result.x == 0.0 && result.y == 0.0 && result.z == 0.0) {
            reportTest("Transform produces valid positions", false,
                "UV(" + std::to_string(corners[i].x) + "," + std::to_string(corners[i].y) + 
                ") produced (0,0,0)");
            cornersCorrect = false;
            break;
        }
        
        // Check X is fixed at 1.0
        if (std::abs(result.x - 1.0) > 0.001) {
            reportTest("Transform maintains fixed dimension", false,
                "X should be 1.0, got " + std::to_string(result.x));
            cornersCorrect = false;
            break;
        }
    }
    
    if (cornersCorrect) {
        reportTest("Transform matrices are valid", true, "");
    }
    
    // Check that V=1 maps to Y=1
    glm::dvec3 topEdge = glm::dvec3(transform * glm::dvec4(0.5, 1.0, 0.0, 1.0));
    reportTest("V=1 maps to Y=maxBounds", 
        std::abs(topEdge.y - patch.maxBounds.y) < 0.001,
        "V=1 gives Y=" + std::to_string(topEdge.y) + ", expected " + std::to_string(patch.maxBounds.y));
}

// TEST 3: Verify vertex cache key generation
void test3_VertexCacheKeys() {
    std::cout << "\n=== TEST 3: Vertex Cache Keys ===" << std::endl;
    
    // Since createVertexKey is private, we'll test it indirectly through mesh generation
    rendering::CPUVertexGenerator::Config config;
    config.gridResolution = 3;  // Very small grid
    config.planetRadius = 6371000.0f;
    config.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create two identical small patches
    core::QuadtreePatch patch1;
    patch1.minBounds = glm::dvec3(1.0, 0.0, 0.0);
    patch1.maxBounds = glm::dvec3(1.0, 0.1, 0.1);
    patch1.center = (patch1.minBounds + patch1.maxBounds) * 0.5;
    patch1.faceId = 0;
    patch1.level = 5;
    
    glm::dvec3 range = patch1.maxBounds - patch1.minBounds;
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);
    transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);
    transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    transform[3] = glm::dvec4(1.0, patch1.minBounds.y, patch1.minBounds.z, 1.0);
    patch1.patchTransform = transform;
    
    // Generate the same patch twice
    auto mesh1 = generator.generatePatchMesh(patch1, patch1.patchTransform);
    auto mesh2 = generator.generatePatchMesh(patch1, patch1.patchTransform);
    
    // If caching works, the second generation should have cache hits
    auto stats = generator.getStats();
    
    reportTest("Vertex cache produces hits for identical patches",
        stats.cacheHits > 0,
        "Cache hits: " + std::to_string(stats.cacheHits) + 
        ", misses: " + std::to_string(stats.cacheMisses));
    
    // Test that vertices are actually identical
    bool verticesMatch = true;
    if (mesh1.vertices.size() == mesh2.vertices.size()) {
        for (size_t i = 0; i < mesh1.vertices.size(); i++) {
            if (glm::length(mesh1.vertices[i].position - mesh2.vertices[i].position) > 0.001) {
                verticesMatch = false;
                break;
            }
        }
    } else {
        verticesMatch = false;
    }
    
    reportTest("Cached vertices are identical",
        verticesMatch,
        "Vertex count: " + std::to_string(mesh1.vertices.size()));
}

// TEST 4: Verify cube-to-sphere mapping
void test4_CubeToSphere() {
    std::cout << "\n=== TEST 4: Cube-to-Sphere Mapping ===" << std::endl;
    
    // Test that shared cube edges produce identical sphere positions
    glm::dvec3 fromFace0(1.0, 1.0, 0.5);  // +X face at Y=1 edge
    glm::dvec3 fromFace2(0.5, 1.0, 0.5);  // +Y face, different position but should NOT match
    
    // These should NOT be the same position - they're different points
    glm::dvec3 sphere0 = glm::normalize(fromFace0);
    glm::dvec3 sphere2 = glm::normalize(fromFace2);
    
    double distance = glm::length(sphere0 - sphere2);
    reportTest("Different cube positions map to different sphere positions",
        distance > 0.01,
        "Distance: " + std::to_string(distance));
    
    // Test actual shared edge point
    glm::dvec3 cornerPoint(1.0, 1.0, 0.0);  // Corner shared by +X and +Y faces
    glm::dvec3 sphereCorner = glm::normalize(cornerPoint);
    
    // This corner should be at exactly the same position from either face's perspective
    reportTest("Corner point normalizes consistently",
        std::abs(glm::length(sphereCorner) - 1.0) < 1e-10,
        "Length: " + std::to_string(glm::length(sphereCorner)));
}

// TEST 5: Verify actual vertex generation and sharing
void test5_VertexSharing() {
    std::cout << "\n=== TEST 5: Vertex Sharing ===" << std::endl;
    
    rendering::CPUVertexGenerator::Config config;
    config.gridResolution = 5;  // Small for testing
    config.planetRadius = 6371000.0f;
    config.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create two patches that share an edge
    // Patch 1: +X face, top edge
    core::QuadtreePatch patch1;
    patch1.minBounds = glm::dvec3(1.0, 0.75, -0.25);
    patch1.maxBounds = glm::dvec3(1.0, 1.0, 0.0);
    patch1.center = (patch1.minBounds + patch1.maxBounds) * 0.5;
    patch1.faceId = 0;
    patch1.level = 2;
    
    // Create transform for patch1
    glm::dvec3 range1 = patch1.maxBounds - patch1.minBounds;
    glm::dmat4 transform1(1.0);
    transform1[0] = glm::dvec4(0.0, 0.0, range1.z, 0.0);
    transform1[1] = glm::dvec4(0.0, range1.y, 0.0, 0.0);
    transform1[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    transform1[3] = glm::dvec4(1.0, patch1.minBounds.y, patch1.minBounds.z, 1.0);
    patch1.patchTransform = transform1;
    
    // Patch 2: +Y face, right edge  
    core::QuadtreePatch patch2;
    patch2.minBounds = glm::dvec3(0.75, 1.0, -0.25);
    patch2.maxBounds = glm::dvec3(1.0, 1.0, 0.0);
    patch2.center = (patch2.minBounds + patch2.maxBounds) * 0.5;
    patch2.faceId = 2;
    patch2.level = 2;
    
    // Create transform for patch2
    glm::dvec3 range2 = patch2.maxBounds - patch2.minBounds;
    glm::dmat4 transform2(1.0);
    transform2[0] = glm::dvec4(range2.x, 0.0, 0.0, 0.0);
    transform2[1] = glm::dvec4(0.0, 0.0, range2.z, 0.0);
    transform2[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    transform2[3] = glm::dvec4(patch2.minBounds.x, 1.0, patch2.minBounds.z, 1.0);
    patch2.patchTransform = transform2;
    
    // Generate meshes
    auto mesh1 = generator.generatePatchMesh(patch1, patch1.patchTransform);
    auto mesh2 = generator.generatePatchMesh(patch2, patch2.patchTransform);
    
    std::cout << "  Mesh 1: " << mesh1.vertices.size() << " vertices" << std::endl;
    std::cout << "  Mesh 2: " << mesh2.vertices.size() << " vertices" << std::endl;
    
    // Find matching vertices
    int exactMatches = 0;
    double minDist = 1e10;
    
    for (const auto& v1 : mesh1.vertices) {
        for (const auto& v2 : mesh2.vertices) {
            double dist = glm::length(glm::dvec3(v1.position - v2.position));
            if (dist < minDist) minDist = dist;
            if (dist < 0.001) {  // Within 1mm
                exactMatches++;
            }
        }
    }
    
    reportTest("Vertices are shared at patch boundaries",
        exactMatches > 0,
        "Found " + std::to_string(exactMatches) + " matching vertices, min distance: " + 
        std::to_string(minDist) + " meters");
    
    // Check cache statistics
    auto stats = generator.getStats();
    reportTest("Vertex cache is being used",
        stats.cacheHits > 0,
        "Cache hits: " + std::to_string(stats.cacheHits) + 
        ", misses: " + std::to_string(stats.cacheMisses));
}

int main() {
    std::cout << "=== SYSTEMATIC VERIFICATION TEST SUITE ===" << std::endl;
    std::cout << "Testing each component in isolation..." << std::endl;
    
    test1_PatchBoundaries();
    test2_TransformMatrices();
    test3_VertexCacheKeys();
    test4_CubeToSphere();
    test5_VertexSharing();
    
    // Summary
    std::cout << "\n=== TEST SUMMARY ===" << std::endl;
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : results) {
        if (result.passed) passed++;
        else failed++;
    }
    
    std::cout << "Passed: " << passed << "/" << results.size() << std::endl;
    std::cout << "Failed: " << failed << "/" << results.size() << std::endl;
    
    if (failed > 0) {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& result : results) {
            if (!result.passed) {
                std::cout << "  - " << result.name << std::endl;
                if (!result.details.empty()) {
                    std::cout << "    " << result.details << std::endl;
                }
            }
        }
    }
    
    return failed > 0 ? 1 : 0;
}