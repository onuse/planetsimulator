// Generate HIGH RESOLUTION sphere using cube-to-sphere mapped chunks
// This creates a dense, smooth planet surface

#include "rendering/vulkan_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <iostream>
#include <vector>

namespace rendering {

bool VulkanRenderer::generateSphereMeshHighRes(octree::OctreePlanet* planet) {
    std::cout << "\n=== Generating HIGH RESOLUTION Sphere Mesh ===\n";
    
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
    
    // Create one chunk for each cube face with VERY HIGH RESOLUTION
    for (int faceId = 0; faceId < 6; faceId++) {
        std::cout << "Generating HIGH-RES mesh for cube face " << faceId << "...\n";
        
        // Set up mesh generation parameters for ULTRA HIGH DETAIL
        glm::vec3 worldPos(0, 0, 0);  // Center of planet
        
        // HIGH RESOLUTION for smooth, dense planet surface  
        // Balance between quality and performance
        float voxelSize = planetRadius / 80.0f;  // Fine detail (80 subdivisions)
        glm::ivec3 dimensions(48, 48, 12);  // 48x48 grid per face, 12 depth layers
        
        std::cout << "  Voxel size: " << voxelSize << " meters\n";
        std::cout << "  Grid dimensions: " << dimensions.x << "x" << dimensions.y << "x" << dimensions.z << "\n";
        
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
    
    std::cout << "\n=== HIGH RESOLUTION SPHERE MESH STATS ===\n";
    std::cout << "Total vertices: " << totalVertices << "\n";
    std::cout << "Total triangles: " << totalIndices/3 << "\n";
    std::cout << "Expected triangles per face: ~" << (128*128*2) << "\n";
    std::cout << "==========================================\n";
    
    // Upload to GPU
    size_t vertexDataSize = combinedVertexData.size() * sizeof(float);
    size_t indexDataSize = combinedIndexData.size() * sizeof(uint32_t);
    
    bool success = uploadCPUReferenceMesh(
        combinedVertexData.data(), vertexDataSize,
        combinedIndexData.data(), indexDataSize,
        totalVertices, totalIndices
    );
    
    if (success) {
        std::cout << "HIGH RESOLUTION sphere mesh successfully uploaded to GPU!\n";
    }
    
    return success;
}

} // namespace rendering