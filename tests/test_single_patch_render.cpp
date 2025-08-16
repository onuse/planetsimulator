// MINIMAL TEST: Render a single patch to isolate the problem
#include <iostream>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include "core/global_patch_generator.hpp"
#include "rendering/cpu_vertex_generator.hpp"

void dumpOBJ(const std::string& filename, 
             const std::vector<rendering::PatchVertex>& vertices,
             const std::vector<uint32_t>& indices) {
    std::ofstream file(filename);
    
    // Write vertices
    for (const auto& v : vertices) {
        file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
    }
    
    // Write faces (OBJ uses 1-based indexing)
    for (size_t i = 0; i < indices.size(); i += 3) {
        file << "f " << (indices[i]+1) << " " << (indices[i+1]+1) << " " << (indices[i+2]+1) << "\n";
    }
    
    file.close();
    std::cout << "Wrote " << filename << " with " << vertices.size() 
              << " vertices and " << indices.size()/3 << " triangles\n";
}

int main(int argc, char** argv) {
    std::cout << "=== SINGLE PATCH ISOLATION TEST ===\n\n";
    
    // Create ONE patch on the +X face
    core::GlobalPatchGenerator::GlobalPatch patch;
    patch.minBounds = glm::dvec3(1.0, -0.5, -0.5);
    patch.maxBounds = glm::dvec3(1.0, 0.5, 0.5);
    patch.center = glm::dvec3(1.0, 0.0, 0.0);
    patch.level = 1;
    patch.faceId = 0;
    
    std::cout << "Patch bounds: (" << patch.minBounds.x << "," << patch.minBounds.y << "," << patch.minBounds.z
              << ") to (" << patch.maxBounds.x << "," << patch.maxBounds.y << "," << patch.maxBounds.z << ")\n";
    
    // Generate transform
    auto transform = patch.createTransform();
    
    // Test the transform with corner UVs
    std::cout << "\nTransform test:\n";
    glm::dvec4 corners[4] = {
        glm::dvec4(0, 0, 0, 1),  // UV(0,0)
        glm::dvec4(1, 0, 0, 1),  // UV(1,0)
        glm::dvec4(1, 1, 0, 1),  // UV(1,1)
        glm::dvec4(0, 1, 0, 1)   // UV(0,1)
    };
    
    for (int i = 0; i < 4; i++) {
        glm::dvec3 cubePos = glm::dvec3(transform * corners[i]);
        std::cout << "  UV(" << corners[i].x << "," << corners[i].y << ") -> ("
                  << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")\n";
    }
    
    // Generate vertices with MINIMAL configuration
    rendering::CPUVertexGenerator::Config config;
    config.planetRadius = 6371000.0;
    config.gridResolution = 5;  // VERY LOW for debugging
    config.enableSkirts = false;
    config.enableVertexCaching = false;  // DISABLE caching to isolate issues
    config.maxCacheSize = 0;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create QuadtreePatch
    core::QuadtreePatch quadPatch;
    quadPatch.center = patch.center;
    quadPatch.minBounds = patch.minBounds;
    quadPatch.maxBounds = patch.maxBounds;
    quadPatch.level = patch.level;
    quadPatch.faceId = patch.faceId;
    quadPatch.size = 1.0f;
    quadPatch.morphFactor = 0.0f;
    quadPatch.screenSpaceError = 0.0f;
    
    // Generate mesh
    auto mesh = generator.generatePatchMesh(quadPatch, transform);
    
    std::cout << "\nGenerated mesh: " << mesh.vertices.size() << " vertices, "
              << mesh.indices.size() << " indices\n";
    
    // Analyze vertices
    std::cout << "\nVertex analysis:\n";
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        const auto& v = mesh.vertices[i];
        float dist = glm::length(v.position);
        std::cout << "  V" << i << ": pos(" << v.position.x << "," << v.position.y << "," << v.position.z
                  << ") dist=" << dist << " height=" << v.height << "\n";
    }
    
    // Check for degenerate triangles
    std::cout << "\nTriangle analysis:\n";
    int degenerateCount = 0;
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i+1];
        uint32_t i2 = mesh.indices[i+2];
        
        glm::vec3 v0 = mesh.vertices[i0].position;
        glm::vec3 v1 = mesh.vertices[i1].position;
        glm::vec3 v2 = mesh.vertices[i2].position;
        
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        float area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
        
        if (area < 0.001f) {
            degenerateCount++;
            std::cout << "  DEGENERATE triangle " << i/3 << ": vertices " 
                      << i0 << "," << i1 << "," << i2 << " area=" << area << "\n";
        }
    }
    
    std::cout << "\nDegenerate triangles: " << degenerateCount << " / " << mesh.indices.size()/3 << "\n";
    
    // Export to OBJ for visual inspection
    dumpOBJ("single_patch.obj", mesh.vertices, mesh.indices);
    
    // Now test an edge patch
    std::cout << "\n=== TESTING EDGE PATCH ===\n";
    patch.minBounds = glm::dvec3(1.0, 0.5, -0.5);
    patch.maxBounds = glm::dvec3(1.0, 1.0, 0.5);
    patch.center = glm::dvec3(1.0, 0.75, 0.0);
    
    quadPatch.center = patch.center;
    quadPatch.minBounds = patch.minBounds;
    quadPatch.maxBounds = patch.maxBounds;
    
    transform = patch.createTransform();
    mesh = generator.generatePatchMesh(quadPatch, transform);
    
    std::cout << "Edge patch: " << mesh.vertices.size() << " vertices\n";
    
    // Check boundary vertices
    std::cout << "\nBoundary vertices (should be at Y=1):\n";
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        const auto& v = mesh.vertices[i];
        // Last row of vertices should be at boundary
        if (i >= mesh.vertices.size() - config.gridResolution) {
            std::cout << "  V" << i << ": pos(" << v.position.x << "," << v.position.y << "," << v.position.z << ")\n";
        }
    }
    
    dumpOBJ("edge_patch.obj", mesh.vertices, mesh.indices);
    
    return 0;
}