// Generate SEAMLESS sphere using cube-to-sphere mapped chunks with proper edge handling
// This fixes discontinuities between cube faces

#include "rendering/vulkan_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace rendering {

// Hash function for vec3 to use in unordered_map
struct Vec3Hash {
    std::size_t operator()(const glm::vec3& v) const {
        // Round to nearest 0.001 to handle floating point precision
        int x = static_cast<int>(std::round(v.x * 1000.0f));
        int y = static_cast<int>(std::round(v.y * 1000.0f));
        int z = static_cast<int>(std::round(v.z * 1000.0f));
        
        std::size_t h1 = std::hash<int>{}(x);
        std::size_t h2 = std::hash<int>{}(y);
        std::size_t h3 = std::hash<int>{}(z);
        
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// Equality comparison for vec3 with epsilon
struct Vec3Equal {
    bool operator()(const glm::vec3& a, const glm::vec3& b) const {
        const float epsilon = 0.001f;
        return std::abs(a.x - b.x) < epsilon &&
               std::abs(a.y - b.y) < epsilon &&
               std::abs(a.z - b.z) < epsilon;
    }
};

bool VulkanRenderer::generateSeamlessSphere(octree::OctreePlanet* planet) {
    std::cout << "\n=== Generating SEAMLESS Sphere Mesh ===\n";
    
    if (!planet) {
        std::cerr << "ERROR: No planet provided!\n";
        return false;
    }
    
    // Storage for combined mesh data with vertex deduplication
    std::vector<algorithms::MeshVertex> uniqueVertices;
    std::vector<uint32_t> combinedIndexData;
    std::unordered_map<glm::vec3, uint32_t, Vec3Hash, Vec3Equal> vertexPositionMap;
    
    // Generate 6 cube face chunks for proper sphere mapping
    float planetRadius = planet->getRadius();
    
    // Create one chunk for each cube face WITH OVERLAP
    for (int faceId = 0; faceId < 6; faceId++) {
        std::cout << "Generating seamless mesh for cube face " << faceId << "...\n";
        
        // Set up mesh generation parameters
        glm::vec3 worldPos(0, 0, 0);  // Center of planet
        
        // High resolution with OVERLAP for seamless edges
        float voxelSize = planetRadius / 100.0f;  // Very fine detail
        
        // Use 65x65 grid to ensure overlap at boundaries
        // This ensures vertices at edges are exactly shared
        glm::ivec3 dimensions(65, 65, 16);  // One extra voxel for overlap
        
        algorithms::MeshGenParams params(worldPos, voxelSize, dimensions, 0, faceId);
        
        // Generate the mesh with sphere mapping
        algorithms::MeshData meshData = algorithms::generateTransvoxelMesh(params, *planet);
        
        if (!meshData.vertices.empty()) {
            // Process vertices with deduplication
            std::vector<uint32_t> indexRemap(meshData.vertices.size());
            
            for (size_t i = 0; i < meshData.vertices.size(); i++) {
                const auto& vertex = meshData.vertices[i];
                
                // Check if we've already seen a vertex at this position
                auto it = vertexPositionMap.find(vertex.position);
                if (it != vertexPositionMap.end()) {
                    // Reuse existing vertex
                    indexRemap[i] = it->second;
                } else {
                    // Add new unique vertex
                    uint32_t newIndex = static_cast<uint32_t>(uniqueVertices.size());
                    uniqueVertices.push_back(vertex);
                    vertexPositionMap[vertex.position] = newIndex;
                    indexRemap[i] = newIndex;
                }
            }
            
            // Add remapped indices
            for (uint32_t index : meshData.indices) {
                combinedIndexData.push_back(indexRemap[index]);
            }
            
            std::cout << "  Face " << faceId << ": " 
                      << meshData.vertices.size() << " vertices ("
                      << (meshData.vertices.size() - indexRemap.size()) << " shared), "
                      << meshData.indices.size()/3 << " triangles\n";
        } else {
            std::cout << "  Face " << faceId << ": No mesh generated (might be empty)\n";
        }
    }
    
    if (uniqueVertices.empty() || combinedIndexData.empty()) {
        std::cerr << "ERROR: No mesh data generated for sphere!\n";
        return false;
    }
    
    uint32_t totalVertices = static_cast<uint32_t>(uniqueVertices.size());
    uint32_t totalIndices = static_cast<uint32_t>(combinedIndexData.size());
    
    std::cout << "\n=== SEAMLESS SPHERE MESH STATS ===\n";
    std::cout << "Unique vertices: " << totalVertices << " (after deduplication)\n";
    std::cout << "Total triangles: " << totalIndices/3 << "\n";
    std::cout << "Vertices saved by sharing: " << vertexPositionMap.size() - totalVertices << "\n";
    std::cout << "===================================\n";
    
    // Convert to flat array for GPU upload
    std::vector<float> vertexData;
    vertexData.reserve(totalVertices * 11);  // 3 pos + 3 color + 3 normal + 2 uv
    
    for (const auto& vertex : uniqueVertices) {
        // Position (3 floats)
        vertexData.push_back(vertex.position.x);
        vertexData.push_back(vertex.position.y);
        vertexData.push_back(vertex.position.z);
        
        // Color (3 floats)
        vertexData.push_back(vertex.color.x);
        vertexData.push_back(vertex.color.y);
        vertexData.push_back(vertex.color.z);
        
        // Normal (3 floats)
        vertexData.push_back(vertex.normal.x);
        vertexData.push_back(vertex.normal.y);
        vertexData.push_back(vertex.normal.z);
        
        // Texture coordinates (2 floats)
        vertexData.push_back(0.0f);  // u
        vertexData.push_back(0.0f);  // v
    }
    
    // Upload to GPU
    size_t vertexDataSize = vertexData.size() * sizeof(float);
    size_t indexDataSize = combinedIndexData.size() * sizeof(uint32_t);
    
    bool success = uploadCPUReferenceMesh(
        vertexData.data(), vertexDataSize,
        combinedIndexData.data(), indexDataSize,
        totalVertices, totalIndices
    );
    
    if (success) {
        std::cout << "SEAMLESS sphere mesh successfully uploaded to GPU!\n";
    }
    
    return success;
}

} // namespace rendering