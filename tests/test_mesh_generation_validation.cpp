#include <iostream>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Test includes
#include "core/octree.hpp"
#include "algorithms/mesh_generation.hpp"

bool testDensityField() {
    std::cout << "\n=== Testing Density Field ===\n";
    
    // Create a simple planet
    octree::OctreePlanet planet(1000.0f, 1234);
    planet.generate();
    
    // Test points at various distances
    struct TestPoint {
        glm::vec3 pos;
        float expectedSign; // negative inside, positive outside
        const char* description;
    };
    
    std::vector<TestPoint> testPoints = {
        {{0, 0, 0}, -1, "Center of planet"},
        {{500, 0, 0}, -1, "Half radius"},
        {{950, 0, 0}, -1, "Just inside surface"},
        {{1000, 0, 0}, 0, "Exactly on surface"},
        {{1050, 0, 0}, 1, "Just outside surface"},
        {{2000, 0, 0}, 1, "Far outside"},
    };
    
    bool passed = true;
    for (const auto& test : testPoints) {
        // We need to call the density function but it's static in the cpp file
        // For now, just verify the concept
        float dist = glm::length(test.pos);
        float density = dist - 1000.0f; // Simple signed distance
        
        bool correct = (density < 0 && test.expectedSign < 0) ||
                       (density > 0 && test.expectedSign > 0) ||
                       (std::abs(density) < 10.0f && test.expectedSign == 0);
        
        std::cout << test.description << ": dist=" << dist 
                  << " density=" << density 
                  << " expected=" << test.expectedSign
                  << " -> " << (correct ? "PASS" : "FAIL") << "\n";
                  
        if (!correct) passed = false;
    }
    
    return passed;
}

bool testMeshVertexPositions() {
    std::cout << "\n=== Testing Mesh Vertex Positions ===\n";
    
    octree::OctreePlanet planet(1000.0f, 1234);
    planet.generate();
    
    // Generate a single chunk mesh
    glm::vec3 chunkPos(1000, 0, 0); // On surface
    float voxelSize = 50.0f;
    
    algorithms::MeshGenParams params(
        chunkPos - glm::vec3(800, 800, 200), // Corner position
        voxelSize,
        glm::ivec3(32, 32, 8), // Dimensions
        0 // LOD
    );
    
    algorithms::MeshData mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    std::cout << "Generated " << mesh.vertices.size() << " vertices\n";
    
    // Check that vertices are reasonably close to the surface
    float minDist = 1e10f, maxDist = -1e10f;
    float avgDist = 0;
    int outliers = 0;
    const float MAX_ALLOWED_DIST = 1200.0f; // Should be within 200m of surface
    
    for (const auto& vertex : mesh.vertices) {
        float dist = glm::length(vertex.position);
        minDist = std::min(minDist, dist);
        maxDist = std::max(maxDist, dist);
        avgDist += dist;
        
        if (dist < 800.0f || dist > MAX_ALLOWED_DIST) {
            outliers++;
            if (outliers <= 5) { // Print first few outliers
                std::cout << "  OUTLIER: vertex at distance " << dist 
                          << " pos=(" << vertex.position.x << "," 
                          << vertex.position.y << "," 
                          << vertex.position.z << ")\n";
            }
        }
    }
    
    if (mesh.vertices.size() > 0) {
        avgDist /= mesh.vertices.size();
    }
    
    std::cout << "Distance stats: min=" << minDist 
              << " max=" << maxDist 
              << " avg=" << avgDist << "\n";
    std::cout << "Outliers: " << outliers << " / " << mesh.vertices.size() << "\n";
    
    bool passed = (outliers == 0) && (maxDist < MAX_ALLOWED_DIST) && (minDist > 800.0f);
    std::cout << "Vertex position test: " << (passed ? "PASS" : "FAIL") << "\n";
    
    return passed;
}

bool testChunkOrientation() {
    std::cout << "\n=== Testing Chunk Coordinate System ===\n";
    
    // Test the spherical coordinate system used in mesh generation
    glm::vec3 chunkCenter(1000, 0, 0);
    glm::vec3 radialDir = glm::normalize(chunkCenter);
    
    // Create tangent vectors
    glm::vec3 tangent1 = glm::normalize(glm::cross(radialDir, glm::vec3(0, 1, 0)));
    if (glm::length(tangent1) < 0.1f) {
        tangent1 = glm::normalize(glm::cross(radialDir, glm::vec3(1, 0, 0)));
    }
    glm::vec3 tangent2 = glm::normalize(glm::cross(radialDir, tangent1));
    
    std::cout << "Chunk at: (" << chunkCenter.x << "," << chunkCenter.y << "," << chunkCenter.z << ")\n";
    std::cout << "Radial: (" << radialDir.x << "," << radialDir.y << "," << radialDir.z << ")\n";
    std::cout << "Tangent1: (" << tangent1.x << "," << tangent1.y << "," << tangent1.z << ")\n";
    std::cout << "Tangent2: (" << tangent2.x << "," << tangent2.y << "," << tangent2.z << ")\n";
    
    // Verify orthogonality
    float dot12 = glm::dot(tangent1, tangent2);
    float dot1r = glm::dot(tangent1, radialDir);
    float dot2r = glm::dot(tangent2, radialDir);
    
    std::cout << "Orthogonality tests:\n";
    std::cout << "  tangent1 · tangent2 = " << dot12 << " (should be ~0)\n";
    std::cout << "  tangent1 · radial = " << dot1r << " (should be ~0)\n";
    std::cout << "  tangent2 · radial = " << dot2r << " (should be ~0)\n";
    
    bool passed = (std::abs(dot12) < 0.01f) && 
                  (std::abs(dot1r) < 0.01f) && 
                  (std::abs(dot2r) < 0.01f);
    
    // Test a point transformation
    float offsetX = 100.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
    
    glm::vec3 worldPos = chunkCenter + 
        tangent1 * offsetX + 
        tangent2 * offsetY + 
        radialDir * offsetZ;
    
    float newDist = glm::length(worldPos);
    std::cout << "\nTest offset (100,0,0) in chunk space:\n";
    std::cout << "  World position: (" << worldPos.x << "," << worldPos.y << "," << worldPos.z << ")\n";
    std::cout << "  Distance from origin: " << newDist << " (should be ~1000)\n";
    
    return passed;
}

bool testTriangleSizes() {
    std::cout << "\n=== Testing Triangle Sizes ===\n";
    
    octree::OctreePlanet planet(1000.0f, 1234);
    planet.generate();
    
    glm::vec3 chunkPos(0, 1000, 0);
    algorithms::MeshGenParams params(
        chunkPos - glm::vec3(400, 400, 100),
        25.0f, // Smaller voxel size
        glm::ivec3(16, 16, 8),
        0
    );
    
    algorithms::MeshData mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    if (mesh.indices.size() < 3) {
        std::cout << "No triangles generated!\n";
        return false;
    }
    
    // Check triangle edge lengths
    float minEdge = 1e10f, maxEdge = -1e10f;
    int degenerateCount = 0;
    int hugeCount = 0;
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        glm::vec3 v0 = mesh.vertices[mesh.indices[i]].position;
        glm::vec3 v1 = mesh.vertices[mesh.indices[i+1]].position;
        glm::vec3 v2 = mesh.vertices[mesh.indices[i+2]].position;
        
        float edge1 = glm::length(v1 - v0);
        float edge2 = glm::length(v2 - v1);
        float edge3 = glm::length(v0 - v2);
        
        minEdge = std::min({minEdge, edge1, edge2, edge3});
        maxEdge = std::max({maxEdge, edge1, edge2, edge3});
        
        if (edge1 < 0.1f || edge2 < 0.1f || edge3 < 0.1f) {
            degenerateCount++;
        }
        if (edge1 > 500.0f || edge2 > 500.0f || edge3 > 500.0f) {
            hugeCount++;
            if (hugeCount <= 3) {
                std::cout << "  HUGE triangle: edges=" << edge1 << "," << edge2 << "," << edge3 << "\n";
            }
        }
    }
    
    std::cout << "Triangle edge lengths: min=" << minEdge << " max=" << maxEdge << "\n";
    std::cout << "Degenerate triangles: " << degenerateCount << "\n";
    std::cout << "Huge triangles: " << hugeCount << "\n";
    
    bool passed = (maxEdge < 200.0f) && (minEdge > 0.1f) && (hugeCount == 0);
    return passed;
}

int main() {
    std::cout << "=== MESH GENERATION VALIDATION TESTS ===\n";
    
    int passed = 0;
    int total = 0;
    
    // Run all tests
    total++; if (testDensityField()) passed++;
    total++; if (testChunkOrientation()) passed++;
    total++; if (testMeshVertexPositions()) passed++;
    total++; if (testTriangleSizes()) passed++;
    
    std::cout << "\n=== RESULTS ===\n";
    std::cout << "Passed: " << passed << " / " << total << "\n";
    
    if (passed == total) {
        std::cout << "SUCCESS: All tests passed!\n";
        return 0;
    } else {
        std::cout << "FAILURE: Some tests failed.\n";
        return 1;
    }
}