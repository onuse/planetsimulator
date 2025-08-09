#include <iostream>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

#include "algorithms/mesh_generation.hpp"
#include "core/octree.hpp"
#include "core/material_table.hpp"

// Minimal test planet with debug output
class DebugPlanet : public octree::OctreePlanet {
public:
    DebugPlanet() : OctreePlanet(2.0f, 5) {  // Radius 2, max depth 5
        // Use the built-in generate method which properly subdivides
        generate(42);  // seed=42
    }
};

// Test density sampling
void testDensitySampling() {
    std::cout << "\n=== Debug: Density Sampling ===" << std::endl;
    
    DebugPlanet planet;
    
    // Test some key positions
    struct TestPoint {
        glm::vec3 pos;
        const char* description;
    };
    
    TestPoint points[] = {
        {{0, 0, 0}, "Origin (inside sphere)"},
        {{3, 0, 0}, "Outside sphere"},
        {{1.5f, 0, 0}, "Near surface"},
        {{2.0f, 0, 0}, "On surface"},
        {{0, 0, 1.5f}, "Near surface (z-axis)"}
    };
    
    for (const auto& point : points) {
        const octree::Voxel* voxel = planet.getVoxel(point.pos);
        if (voxel) {
            auto matId = voxel->getDominantMaterialID();
            bool isAir = (matId == core::MaterialID::Air);
            float density = isAir ? 1.0f : -1.0f;
            
            std::cout << "  " << point.description << " at " 
                      << "(" << point.pos.x << ", " << point.pos.y << ", " << point.pos.z << ")"
                      << " -> Material: " << static_cast<int>(matId) 
                      << " (Air=" << static_cast<int>(core::MaterialID::Air) << ")"
                      << ", Density: " << density << std::endl;
        } else {
            std::cout << "  " << point.description << " -> NULL voxel" << std::endl;
        }
    }
}

// Test a single cell
void testSingleCell() {
    std::cout << "\n=== Debug: Single Cell Transvoxel ===" << std::endl;
    
    DebugPlanet planet;
    
    // Generate mesh for a single cell that should cross the surface
    algorithms::MeshGenParams params(glm::vec3(1.5f, 0, 0), 1.0f, glm::ivec3(1), 0);
    
    std::cout << "  Cell at (1.5, 0, 0) with size 1.0" << std::endl;
    
    // Sample the 8 corners manually
    for (int i = 0; i < 8; i++) {
        float x = 1.5f + ((i & 1) ? 1.0f : 0.0f);
        float y = 0.0f + ((i & 2) ? 1.0f : 0.0f);
        float z = 0.0f + ((i & 4) ? 1.0f : 0.0f);
        
        glm::vec3 pos(x, y, z);
        const octree::Voxel* voxel = planet.getVoxel(pos);
        if (voxel) {
            auto matId = voxel->getDominantMaterialID();
            bool isInside = (matId != core::MaterialID::Air);
            std::cout << "    Corner " << i << " at (" << x << ", " << y << ", " << z << ")"
                      << " dist=" << glm::length(pos)
                      << " -> " << (isInside ? "INSIDE" : "OUTSIDE") << std::endl;
        }
    }
    
    auto mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    std::cout << "  Generated " << mesh.getTriangleCount() << " triangles" << std::endl;
    
    if (mesh.getTriangleCount() > 0) {
        std::cout << "  Vertices:" << std::endl;
        for (size_t i = 0; i < mesh.vertices.size(); i++) {
            const auto& v = mesh.vertices[i];
            std::cout << "    " << i << ": pos=(" 
                      << v.position.x << ", " << v.position.y << ", " << v.position.z 
                      << ") dist=" << glm::length(v.position) << std::endl;
        }
    }
}

// Test larger region
void testLargerRegion() {
    std::cout << "\n=== Debug: Larger Region ===" << std::endl;
    
    DebugPlanet planet;
    
    // Generate mesh for region that definitely contains the sphere
    algorithms::MeshGenParams params(glm::vec3(-3), 0.5f, glm::ivec3(12), 0);
    
    auto mesh = algorithms::generateTransvoxelMesh(params, planet);
    
    std::cout << "  Region from (-3,-3,-3) to (3,3,3) with voxel size 0.5" << std::endl;
    std::cout << "  Generated " << mesh.getTriangleCount() << " triangles, "
              << mesh.vertices.size() << " vertices" << std::endl;
    
    if (mesh.vertices.size() > 0) {
        // Calculate statistics
        float minDist = 1e10f, maxDist = 0;
        for (const auto& v : mesh.vertices) {
            float dist = glm::length(v.position);
            minDist = std::min(minDist, dist);
            maxDist = std::max(maxDist, dist);
        }
        std::cout << "  Distance range: " << minDist << " to " << maxDist 
                  << " (expected around 2.0)" << std::endl;
    }
}

// Compare with simple cubes
void testSimpleCubes() {
    std::cout << "\n=== Debug: Simple Cubes Comparison ===" << std::endl;
    
    DebugPlanet planet;
    algorithms::MeshGenParams params(glm::vec3(-3), 0.5f, glm::ivec3(12), 0);
    
    auto simpleMesh = algorithms::generateSimpleCubeMesh(params, planet);
    
    std::cout << "  Simple cubes: " << simpleMesh.getTriangleCount() << " triangles, "
              << simpleMesh.vertices.size() << " vertices" << std::endl;
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "   Transvoxel Algorithm Debug Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        testDensitySampling();
        testSingleCell();
        testLargerRegion();
        testSimpleCubes();
        
        std::cout << "\n=========================================" << std::endl;
        std::cout << "   Debug tests completed!" << std::endl;
        std::cout << "=========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}