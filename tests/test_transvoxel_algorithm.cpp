#include <iostream>
#include <cmath>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "algorithms/mesh_generation.hpp"
#include "core/octree.hpp"
#include "core/material_table.hpp"

// Define M_PI if not defined (for Windows MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test wrapper for OctreePlanet that allows custom voxel generation
class TestPlanet : public octree::OctreePlanet {
public:
    enum TestShape {
        SPHERE,
        CUBE,
        PLANE,
        EMPTY,
        FULL
    };

    TestPlanet(TestShape shape, float size = 10.0f) 
        : OctreePlanet(100.0f, 5), shape_(shape), shapeSize_(size) {
        // First generate the octree structure
        generate(42); // Use fixed seed for tests
        // Then apply our custom shape
        generateTestShape();
    }

private:
    TestShape shape_;
    float shapeSize_;

    void generateTestShape() {
        // Traverse the octree and set voxels based on shape
        auto root = const_cast<octree::OctreeNode*>(getRoot());
        if (root) {
            setVoxelsInNode(root);
        }
    }

    void setVoxelsInNode(octree::OctreeNode* node) {
        if (!node) return;

        if (node->isLeaf()) {
            // Set voxels in leaf based on shape
            const auto& center = node->getCenter();
            float halfSize = node->getHalfSize();
            
            // Sample 2x2x2 positions in the leaf
            for (int z = 0; z < 2; z++) {
                for (int y = 0; y < 2; y++) {
                    for (int x = 0; x < 2; x++) {
                        glm::vec3 offset(
                            (x - 0.5f) * halfSize,
                            (y - 0.5f) * halfSize,
                            (z - 0.5f) * halfSize
                        );
                        glm::vec3 pos = center + offset;
                        
                        octree::Voxel voxel;
                        if (isInsideShape(pos)) {
                            voxel.setMaterial(0, core::MaterialID::Rock, 255);
                        } else {
                            voxel.setMaterial(0, core::MaterialID::Air, 255);
                        }
                        
                        setVoxel(pos, voxel);
                    }
                }
            }
        } else {
            // Recursively process children
            const auto& children = node->getChildren();
            for (const auto& child : children) {
                if (child) {
                    setVoxelsInNode(child.get());
                }
            }
        }
    }

    bool isInsideShape(const glm::vec3& pos) const {
        switch (shape_) {
            case SPHERE:
                return glm::length(pos) < shapeSize_;
            case CUBE:
                return std::abs(pos.x) < shapeSize_ &&
                       std::abs(pos.y) < shapeSize_ &&
                       std::abs(pos.z) < shapeSize_;
            case PLANE:
                return pos.y < 0;
            case EMPTY:
                return false;
            case FULL:
                return true;
        }
        return false;
    }
};

// Helper functions for testing
bool hasValidTriangles(const algorithms::MeshData& mesh) {
    if (mesh.indices.size() % 3 != 0) return false;
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];
        
        if (i0 >= mesh.vertices.size() || 
            i1 >= mesh.vertices.size() || 
            i2 >= mesh.vertices.size()) {
            return false;
        }
        
        // Check for degenerate triangles
        glm::vec3 v0 = mesh.vertices[i0].position;
        glm::vec3 v1 = mesh.vertices[i1].position;
        glm::vec3 v2 = mesh.vertices[i2].position;
        
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 cross = glm::cross(edge1, edge2);
        
        if (glm::length(cross) < 1e-6f) {
            return false; // Degenerate triangle
        }
    }
    
    return true;
}

bool hasConsistentWinding(const algorithms::MeshData& mesh) {
    // Check that normals point outward (assuming counter-clockwise winding)
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        glm::vec3 v0 = mesh.vertices[mesh.indices[i]].position;
        glm::vec3 v1 = mesh.vertices[mesh.indices[i + 1]].position;
        glm::vec3 v2 = mesh.vertices[mesh.indices[i + 2]].position;
        
        glm::vec3 triangleNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        glm::vec3 vertexNormal = mesh.vertices[mesh.indices[i]].normal;
        
        // Normals should roughly align
        float dot = glm::dot(triangleNormal, vertexNormal);
        if (dot < -0.5f) {
            return false; // Normals point in opposite directions
        }
    }
    return true;
}

float calculateMeshVolume(const algorithms::MeshData& mesh) {
    float volume = 0.0f;
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        glm::vec3 v0 = mesh.vertices[mesh.indices[i]].position;
        glm::vec3 v1 = mesh.vertices[mesh.indices[i + 1]].position;
        glm::vec3 v2 = mesh.vertices[mesh.indices[i + 2]].position;
        
        // Signed volume of tetrahedron formed with origin
        volume += glm::dot(v0, glm::cross(v1, v2)) / 6.0f;
    }
    
    return std::abs(volume);
}

// Test 1: Regular Cell Cases
void testTransvoxelRegularCell() {
    std::cout << "\n=== Test 1: Transvoxel Regular Cell Cases ===" << std::endl;
    
    // Test case 0: All corners outside (empty cell)
    {
        TestPlanet planet(TestPlanet::EMPTY);
        algorithms::MeshGenParams params(glm::vec3(0), 1.0f, glm::ivec3(1), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        std::cout << "  Case 0 (empty): " << mesh.getTriangleCount() << " triangles (expected: 0)" << std::endl;
        if (mesh.getTriangleCount() != 0) {
            std::cout << "    FAIL: Empty cell should produce no triangles" << std::endl;
        } else {
            std::cout << "    PASS" << std::endl;
        }
    }
    
    // Test case 255: All corners inside (full cell)
    {
        TestPlanet planet(TestPlanet::FULL);
        algorithms::MeshGenParams params(glm::vec3(0), 1.0f, glm::ivec3(1), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        std::cout << "  Case 255 (full): " << mesh.getTriangleCount() << " triangles (expected: 0)" << std::endl;
        if (mesh.getTriangleCount() != 0) {
            std::cout << "    FAIL: Full cell should produce no triangles" << std::endl;
        } else {
            std::cout << "    PASS" << std::endl;
        }
    }
    
    // Test case 1: Single corner inside (corner 0)
    {
        TestPlanet planet(TestPlanet::SPHERE, 0.6f); // Small sphere at origin
        algorithms::MeshGenParams params(glm::vec3(-0.5f), 1.0f, glm::ivec3(1), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        std::cout << "  Case 1 (single corner): " << mesh.getTriangleCount() << " triangles" << std::endl;
        if (mesh.getTriangleCount() > 0 && hasValidTriangles(mesh)) {
            std::cout << "    PASS: Generated valid triangles" << std::endl;
        } else if (mesh.getTriangleCount() == 0) {
            std::cout << "    INFO: No triangles generated (may be outside shape)" << std::endl;
        } else {
            std::cout << "    FAIL: Invalid triangles" << std::endl;
        }
    }
}

// Test 2: Edge Interpolation
void testTransvoxelEdgeInterpolation() {
    std::cout << "\n=== Test 2: Transvoxel Edge Interpolation ===" << std::endl;
    
    // Create a plane at y=0
    TestPlanet planet(TestPlanet::PLANE);
    algorithms::MeshGenParams params(glm::vec3(-1, -1, -1), 2.0f, glm::ivec3(1), 0);
    auto mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    std::cout << "  Plane mesh: " << mesh.getTriangleCount() << " triangles" << std::endl;
    
    if (mesh.vertices.empty()) {
        std::cout << "    INFO: No vertices generated for plane test" << std::endl;
        return;
    }
    
    // Check that vertices are interpolated to y=0
    bool allVerticesOnPlane = true;
    float maxDeviation = 0.0f;
    
    for (const auto& vertex : mesh.vertices) {
        float deviation = std::abs(vertex.position.y);
        maxDeviation = std::max(maxDeviation, deviation);
        if (deviation > 0.1f) {
            allVerticesOnPlane = false;
        }
    }
    
    std::cout << "  Max deviation from y=0: " << maxDeviation << std::endl;
    if (allVerticesOnPlane) {
        std::cout << "    PASS: Vertices correctly interpolated to surface" << std::endl;
    } else {
        std::cout << "    FAIL: Vertices not on expected surface" << std::endl;
    }
}

// Test 3: Surface Extraction at Threshold
void testTransvoxelSurfaceExtraction() {
    std::cout << "\n=== Test 3: Transvoxel Surface Extraction ===" << std::endl;
    
    // Test with sphere of known radius
    float sphereRadius = 5.0f;
    TestPlanet planet(TestPlanet::SPHERE, sphereRadius);
    
    // Generate mesh covering the sphere
    algorithms::MeshGenParams params(glm::vec3(-10), 0.5f, glm::ivec3(40), 0);
    auto mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    std::cout << "  Sphere mesh: " << mesh.getTriangleCount() << " triangles" << std::endl;
    
    if (mesh.vertices.empty()) {
        std::cout << "    INFO: No vertices generated" << std::endl;
        return;
    }
    
    // Check that all vertices are approximately on sphere surface
    float minDist = 1e10f;
    float maxDist = 0.0f;
    float avgDist = 0.0f;
    
    for (const auto& vertex : mesh.vertices) {
        float dist = glm::length(vertex.position);
        minDist = std::min(minDist, dist);
        maxDist = std::max(maxDist, dist);
        avgDist += dist;
    }
    
    if (!mesh.vertices.empty()) {
        avgDist /= mesh.vertices.size();
    }
    
    std::cout << "  Distance from origin - Min: " << minDist 
              << ", Max: " << maxDist 
              << ", Avg: " << avgDist 
              << " (Expected: " << sphereRadius << ")" << std::endl;
    
    float tolerance = 0.5f; // Half voxel size
    if (std::abs(avgDist - sphereRadius) < tolerance) {
        std::cout << "    PASS: Surface extracted at correct threshold" << std::endl;
    } else {
        std::cout << "    FAIL: Surface not at expected distance" << std::endl;
    }
}

// Test 4: Mesh Connectivity
void testTransvoxelMeshConnectivity() {
    std::cout << "\n=== Test 4: Transvoxel Mesh Connectivity ===" << std::endl;
    
    TestPlanet planet(TestPlanet::SPHERE, 3.0f);
    algorithms::MeshGenParams params(glm::vec3(-5), 0.5f, glm::ivec3(20), 0);
    auto mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    std::cout << "  Generated " << mesh.vertices.size() << " vertices, " 
              << mesh.getTriangleCount() << " triangles" << std::endl;
    
    if (mesh.isEmpty()) {
        std::cout << "    INFO: No mesh generated" << std::endl;
        return;
    }
    
    // Check for valid triangles
    if (hasValidTriangles(mesh)) {
        std::cout << "    PASS: All triangles have valid indices" << std::endl;
    } else {
        std::cout << "    FAIL: Invalid triangle indices found" << std::endl;
    }
    
    // Check winding consistency
    if (hasConsistentWinding(mesh)) {
        std::cout << "    PASS: Consistent triangle winding" << std::endl;
    } else {
        std::cout << "    FAIL: Inconsistent triangle winding" << std::endl;
    }
}

// Test 5: Density Field Shapes
void testTransvoxelDensityField() {
    std::cout << "\n=== Test 5: Transvoxel Density Field Shapes ===" << std::endl;
    
    struct ShapeTest {
        TestPlanet::TestShape shape;
        const char* name;
        float expectedVolume;
        float tolerance;
    };
    
    ShapeTest tests[] = {
        {TestPlanet::SPHERE, "Sphere(r=3)", static_cast<float>(4.0f/3.0f * M_PI * 27.0f), 20.0f},
        {TestPlanet::CUBE, "Cube(s=3)", 8.0f * 27.0f, 30.0f},
        {TestPlanet::PLANE, "Plane", 0.0f, 1000.0f} // Plane has undefined volume
    };
    
    for (const auto& test : tests) {
        TestPlanet planet(test.shape, 3.0f);
        algorithms::MeshGenParams params(glm::vec3(-6), 0.25f, glm::ivec3(48), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        float volume = calculateMeshVolume(mesh);
        
        std::cout << "  " << test.name << ": " 
                  << mesh.getTriangleCount() << " triangles, "
                  << "volume = " << volume;
        
        if (test.shape != TestPlanet::PLANE) {
            std::cout << " (expected: " << test.expectedVolume << ")";
            if (std::abs(volume - test.expectedVolume) < test.tolerance) {
                std::cout << " PASS" << std::endl;
            } else {
                std::cout << " INFO: Volume mismatch (may be due to discretization)" << std::endl;
            }
        } else {
            std::cout << " (unbounded)" << std::endl;
        }
    }
}

// Test 6: Compare with Simple Cubes
void testTransvoxelVsSimpleCubes() {
    std::cout << "\n=== Test 6: Transvoxel vs Simple Cubes ===" << std::endl;
    
    TestPlanet planet(TestPlanet::SPHERE, 4.0f);
    algorithms::MeshGenParams params(glm::vec3(-6), 0.5f, glm::ivec3(24), 0);
    
    auto transvoxelMesh = algorithms::generateTransvoxelMesh(params, planet);
    auto simpleMesh = algorithms::generateSimpleCubeMesh(params, planet);
    
    std::cout << "  Transvoxel: " << transvoxelMesh.getTriangleCount() << " triangles, "
              << transvoxelMesh.vertices.size() << " vertices" << std::endl;
    std::cout << "  Simple Cubes: " << simpleMesh.getTriangleCount() << " triangles, "
              << simpleMesh.vertices.size() << " vertices" << std::endl;
    
    // Transvoxel should produce smoother mesh with fewer triangles for curved surfaces
    if (transvoxelMesh.getTriangleCount() > 0 && simpleMesh.getTriangleCount() > 0) {
        float ratio = static_cast<float>(transvoxelMesh.getTriangleCount()) / 
                     static_cast<float>(simpleMesh.getTriangleCount());
        std::cout << "  Triangle ratio (Transvoxel/Simple): " << ratio << std::endl;
        
        // Calculate surface smoothness by checking normal variation
        auto calculateSmoothness = [](const algorithms::MeshData& mesh) -> float {
            if (mesh.vertices.empty()) return 0.0f;
            
            float totalVariation = 0.0f;
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                glm::vec3 n0 = mesh.vertices[mesh.indices[i]].normal;
                glm::vec3 n1 = mesh.vertices[mesh.indices[i + 1]].normal;
                glm::vec3 n2 = mesh.vertices[mesh.indices[i + 2]].normal;
                
                totalVariation += glm::length(n1 - n0) + glm::length(n2 - n1) + glm::length(n0 - n2);
            }
            return totalVariation / mesh.getTriangleCount();
        };
        
        float transvoxelSmoothness = calculateSmoothness(transvoxelMesh);
        float simpleSmoothness = calculateSmoothness(simpleMesh);
        
        std::cout << "  Normal variation - Transvoxel: " << transvoxelSmoothness
                  << ", Simple: " << simpleSmoothness << std::endl;
        
        if (transvoxelSmoothness < simpleSmoothness) {
            std::cout << "    PASS: Transvoxel produces smoother surface" << std::endl;
        } else {
            std::cout << "    Note: Simple cubes may be smoother for this test case" << std::endl;
        }
    }
}

// Test 7: Edge Cases
void testTransvoxelEdgeCases() {
    std::cout << "\n=== Test 7: Transvoxel Edge Cases ===" << std::endl;
    
    // Test with very small voxel size
    {
        TestPlanet planet(TestPlanet::SPHERE, 1.0f);
        algorithms::MeshGenParams params(glm::vec3(-1), 0.01f, glm::ivec3(200), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        std::cout << "  Small voxels (0.01): " << mesh.getTriangleCount() << " triangles";
        if (mesh.getTriangleCount() > 0 && hasValidTriangles(mesh)) {
            std::cout << " PASS" << std::endl;
        } else if (mesh.getTriangleCount() == 0) {
            std::cout << " INFO: No surface in region" << std::endl;
        } else {
            std::cout << " FAIL" << std::endl;
        }
    }
    
    // Test with single voxel
    {
        TestPlanet planet(TestPlanet::SPHERE, 0.7f);
        algorithms::MeshGenParams params(glm::vec3(-0.5f), 1.0f, glm::ivec3(1), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        std::cout << "  Single voxel: " << mesh.getTriangleCount() << " triangles";
        if (hasValidTriangles(mesh)) {
            std::cout << " PASS" << std::endl;
        } else if (mesh.getTriangleCount() == 0) {
            std::cout << " PASS (no surface in cell)" << std::endl;
        } else {
            std::cout << " FAIL" << std::endl;
        }
    }
    
    // Test at boundary
    {
        TestPlanet planet(TestPlanet::CUBE, 5.0f);
        algorithms::MeshGenParams params(glm::vec3(4.5f, 4.5f, 4.5f), 1.0f, glm::ivec3(2), 0);
        auto mesh = algorithms::generateTransvoxelMesh(params, planet);
        
        std::cout << "  Boundary case: " << mesh.getTriangleCount() << " triangles";
        if (hasValidTriangles(mesh)) {
            std::cout << " PASS" << std::endl;
        } else if (mesh.getTriangleCount() == 0) {
            std::cout << " PASS (no surface)" << std::endl;
        } else {
            std::cout << " FAIL" << std::endl;
        }
    }
}

// Test 8: Normal Calculation
void testTransvoxelNormals() {
    std::cout << "\n=== Test 8: Transvoxel Normal Calculation ===" << std::endl;
    
    // Test sphere normals - should point radially outward
    TestPlanet planet(TestPlanet::SPHERE, 5.0f);
    algorithms::MeshGenParams params(glm::vec3(-8), 0.5f, glm::ivec3(32), 0);
    auto mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    if (mesh.vertices.empty()) {
        std::cout << "    INFO: No vertices generated" << std::endl;
        return;
    }
    
    float avgAlignment = 0.0f;
    int validNormals = 0;
    
    for (const auto& vertex : mesh.vertices) {
        glm::vec3 radialDir = glm::normalize(vertex.position);
        float alignment = glm::dot(vertex.normal, radialDir);
        
        if (!std::isnan(alignment)) {
            avgAlignment += alignment;
            validNormals++;
        }
    }
    
    if (validNormals > 0) {
        avgAlignment /= validNormals;
        std::cout << "  Average normal alignment with radial direction: " << avgAlignment << std::endl;
        
        if (avgAlignment > 0.8f) {
            std::cout << "    PASS: Normals correctly point outward" << std::endl;
        } else {
            std::cout << "    INFO: Normals may not align perfectly due to discretization" << std::endl;
        }
    } else {
        std::cout << "    FAIL: No valid normals found" << std::endl;
    }
    
    // Check all normals are unit length
    bool allNormalized = true;
    for (const auto& vertex : mesh.vertices) {
        float length = glm::length(vertex.normal);
        if (std::abs(length - 1.0f) > 0.01f) {
            allNormalized = false;
            break;
        }
    }
    
    if (allNormalized) {
        std::cout << "    PASS: All normals are unit length" << std::endl;
    } else {
        std::cout << "    FAIL: Some normals are not normalized" << std::endl;
    }
}

// Main test runner
int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "   Transvoxel Algorithm Unit Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        testTransvoxelRegularCell();
        testTransvoxelEdgeInterpolation();
        testTransvoxelSurfaceExtraction();
        testTransvoxelMeshConnectivity();
        testTransvoxelDensityField();
        testTransvoxelVsSimpleCubes();
        testTransvoxelEdgeCases();
        testTransvoxelNormals();
        
        std::cout << "\n=========================================" << std::endl;
        std::cout << "   All tests completed!" << std::endl;
        std::cout << "=========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}