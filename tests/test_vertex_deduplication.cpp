#include <iostream>
#include <unordered_set>
#include <glm/glm.hpp>
#include "algorithms/mesh_generation.hpp"
#include "core/octree.hpp"
#include "core/material_table.hpp"

int main() {
    std::cout << "=== Vertex Deduplication Test ===" << std::endl;
    
    // Create a simple planet
    octree::OctreePlanet planet(2.0f, 5);
    planet.generate(42);
    
    // Generate mesh for a region
    algorithms::MeshGenParams params(glm::vec3(-3), 0.5f, glm::ivec3(12), 0);
    
    // Test Transvoxel mesh
    auto transvoxelMesh = algorithms::generateTransvoxelMesh(params, planet);
    std::cout << "Transvoxel: " << transvoxelMesh.getTriangleCount() << " triangles, "
              << transvoxelMesh.vertices.size() << " vertices" << std::endl;
    
    // Test Simple Cubes mesh for comparison
    auto simpleMesh = algorithms::generateSimpleCubeMesh(params, planet);
    std::cout << "Simple Cubes: " << simpleMesh.getTriangleCount() << " triangles, "
              << simpleMesh.vertices.size() << " vertices" << std::endl;
    
    // Check for duplicate vertices in Transvoxel mesh
    std::unordered_set<std::string> uniquePositions;
    int duplicates = 0;
    
    for (const auto& vertex : transvoxelMesh.vertices) {
        // Create a string key for the position (rounded to avoid float precision issues)
        int x = static_cast<int>(std::round(vertex.position.x * 1000));
        int y = static_cast<int>(std::round(vertex.position.y * 1000));
        int z = static_cast<int>(std::round(vertex.position.z * 1000));
        std::string key = std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z);
        
        if (uniquePositions.find(key) != uniquePositions.end()) {
            duplicates++;
        } else {
            uniquePositions.insert(key);
        }
    }
    
    std::cout << "\nVertex deduplication check:" << std::endl;
    std::cout << "  Total vertices: " << transvoxelMesh.vertices.size() << std::endl;
    std::cout << "  Unique positions: " << uniquePositions.size() << std::endl;
    std::cout << "  Duplicates found: " << duplicates << std::endl;
    
    // Calculate efficiency
    float expectedVertices = transvoxelMesh.getTriangleCount() * 3.0f;
    float actualVertices = static_cast<float>(transvoxelMesh.vertices.size());
    float efficiency = (1.0f - actualVertices / expectedVertices) * 100.0f;
    
    std::cout << "\nVertex sharing efficiency:" << std::endl;
    std::cout << "  Without sharing: " << static_cast<int>(expectedVertices) << " vertices" << std::endl;
    std::cout << "  With sharing: " << static_cast<int>(actualVertices) << " vertices" << std::endl;
    std::cout << "  Efficiency: " << efficiency << "%" << std::endl;
    
    // Test result
    bool passed = duplicates == 0 && efficiency > 50.0f;
    
    std::cout << "\n" << (passed ? "TEST PASSED" : "TEST FAILED") << std::endl;
    
    return passed ? 0 : 1;
}