// Generate sphere using cube-to-sphere mapped chunks
// This fixes the "paper Christmas ornament" appearance issue

#include "rendering/vulkan_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <iostream>
#include <vector>

namespace rendering {

bool VulkanRenderer::generateSphereMesh(octree::OctreePlanet* planet) {
    std::cout << "\n=== Generating Sphere Mesh with Cube-to-Sphere Mapping ===\n";
    
    if (!planet) {
        std::cerr << "ERROR: No planet provided!\n";
        return false;
    }
    
    // Storage for combined mesh data
    std::vector<float> combinedVertexData;
    std::vector<uint32_t> combinedIndexData;
    uint32_t vertexOffset = 0;
    
    // Generate 6 cube face chunks for proper sphere mapping
    float planetRadius = planet->getRadius();
    
    // Create one chunk for each cube face
    for (int faceId = 0; faceId < 6; faceId++) {
        std::cout << "Generating mesh for cube face " << faceId << "...\n";
        
        // Set up mesh generation parameters
        glm::vec3 worldPos(0, 0, 0);  // Center of planet
        float voxelSize = planetRadius / 100.0f;  // Very fine detail - 100 subdivisions
        glm::ivec3 dimensions(64, 64, 16);  // 64x64 grid per face for smooth surface
        algorithms::MeshGenParams params(worldPos, voxelSize, dimensions, 0, faceId);
        
        // Generate the mesh with sphere mapping
        algorithms::MeshData meshData = algorithms::generateTransvoxelMesh(params, *planet);
        
        if (!meshData.vertices.empty()) {
            std::cout << "  Face " << faceId << ": " 
                      << meshData.vertices.size() << " vertices, "
                      << meshData.indices.size()/3 << " triangles\n";
            
            // Add vertices from this face
            for (const auto& vertex : meshData.vertices) {
                // Position (3 floats)
                combinedVertexData.push_back(vertex.position.x);
                combinedVertexData.push_back(vertex.position.y);
                combinedVertexData.push_back(vertex.position.z);
                
                // Color (3 floats)
                combinedVertexData.push_back(vertex.color.x);
                combinedVertexData.push_back(vertex.color.y);
                combinedVertexData.push_back(vertex.color.z);
                
                // Normal (3 floats)
                combinedVertexData.push_back(vertex.normal.x);
                combinedVertexData.push_back(vertex.normal.y);
                combinedVertexData.push_back(vertex.normal.z);
                
                // Texture coordinates (2 floats) - use default UV for now
                combinedVertexData.push_back(0.0f);  // u
                combinedVertexData.push_back(0.0f);  // v
            }
            
            // Add indices from this face (with offset)
            for (uint32_t index : meshData.indices) {
                combinedIndexData.push_back(index + vertexOffset);
            }
            
            vertexOffset += static_cast<uint32_t>(meshData.vertices.size());
        } else {
            std::cout << "  Face " << faceId << ": No mesh generated (might be empty)\n";
        }
    }
    
    if (combinedVertexData.empty() || combinedIndexData.empty()) {
        std::cerr << "ERROR: No mesh data generated for sphere!\n";
        return false;
    }
    
    uint32_t totalVertices = vertexOffset;
    uint32_t totalIndices = static_cast<uint32_t>(combinedIndexData.size());
    
    std::cout << "\nTotal sphere mesh: " << totalVertices << " vertices, " 
              << totalIndices/3 << " triangles\n";
    
    // Upload to GPU
    size_t vertexDataSize = combinedVertexData.size() * sizeof(float);
    size_t indexDataSize = combinedIndexData.size() * sizeof(uint32_t);
    
    bool success = uploadCPUReferenceMesh(
        combinedVertexData.data(), vertexDataSize,
        combinedIndexData.data(), indexDataSize,
        totalVertices, totalIndices
    );
    
    if (success) {
        std::cout << "Sphere mesh successfully uploaded to GPU!\n";
    }
    
    return success;
}

} // namespace rendering