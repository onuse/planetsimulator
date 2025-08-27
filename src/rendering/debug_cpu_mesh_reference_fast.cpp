// CPU Reference Mesh Generation with Vertex Deduplication
// Fast approach using post-processing to fix gaps

#include "rendering/vulkan_renderer.hpp"
#include "core/octree.hpp"
#include "algorithms/marching_cubes_tables.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <functional>
#include <cmath>
#include <string>

using marching_cubes::edgeTable;
using marching_cubes::triTable;

namespace std {
    template<>
    struct hash<glm::vec3> {
        size_t operator()(const glm::vec3& v) const {
            // Quantize to grid (0.1% of leaf size for precision)
            const float GRID_SIZE = 0.1f;
            int x = static_cast<int>(v.x / GRID_SIZE);
            int y = static_cast<int>(v.y / GRID_SIZE);
            int z = static_cast<int>(v.z / GRID_SIZE);
            
            size_t h1 = std::hash<int>()(x);
            size_t h2 = std::hash<int>()(y);
            size_t h3 = std::hash<int>()(z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

namespace rendering {

// Vertex structure (matching VulkanRenderer's internal structure)
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texCoord;
};

// Hybrid density function combining sphere and voxel data
static float getHybridDensity(const glm::vec3& pos, float radius, const octree::MixedVoxel& voxel) {
    float sphereDist = glm::length(pos) - radius;
    float sphereDensity = -sphereDist; // Negative inside, positive outside
    
    // Get voxel density
    core::MaterialID mat = voxel.getDominantMaterialID();
    float voxelInfluence = 0.0f;
    
    if (mat != core::MaterialID::Air && mat != core::MaterialID::Vacuum) {
        voxelInfluence = 1.0f; // Solid material
    } else {
        voxelInfluence = -1.0f; // Air/vacuum
    }
    
    // Blend sphere and voxel influence
    float surfaceProximity = 1.0f - std::min(std::abs(sphereDist) / (radius * 0.1f), 1.0f);
    float density = sphereDensity * (1.0f - surfaceProximity * 0.5f) + 
                   voxelInfluence * radius * 0.05f * surfaceProximity;
    
    return density;
}

// Generate color from voxel material - realistic earth/ocean colors
static glm::vec3 getVoxelColor(const octree::MixedVoxel& voxel) {
    // Define realistic material colors
    auto getMaterialColor = [](core::MaterialID mat) -> glm::vec3 {
        switch(mat) {
            case core::MaterialID::Water:
                return glm::vec3(0.05f, 0.25f, 0.45f); // Deep ocean blue
            case core::MaterialID::Rock:
                return glm::vec3(0.35f, 0.3f, 0.25f); // Mountain gray-brown
            case core::MaterialID::Sand:
                return glm::vec3(0.8f, 0.75f, 0.6f); // Beach sand
            case core::MaterialID::Grass:
                return glm::vec3(0.25f, 0.45f, 0.2f); // Vegetation green
            case core::MaterialID::Snow:
                return glm::vec3(0.95f, 0.95f, 0.97f); // Fresh snow
            case core::MaterialID::Lava:
                return glm::vec3(0.9f, 0.3f, 0.1f); // Molten lava
            case core::MaterialID::Air:
            case core::MaterialID::Vacuum:
                return glm::vec3(0.7f, 0.8f, 0.9f); // Sky blue (shouldn't happen on surface)
            default:
                return glm::vec3(0.4f, 0.35f, 0.3f); // Default earth tone
        }
    };
    
    // MixedVoxel can have up to 4 materials, blend them by their amounts
    glm::vec3 finalColor(0.0f);
    float totalAmount = 0.0f;
    
    for (int i = 0; i < 4; i++) {
        uint8_t amount = voxel.getMaterialAmount(i);
        if (amount > 0) {
            core::MaterialID mat = voxel.getMaterialID(i);
            glm::vec3 matColor = getMaterialColor(mat);
            finalColor += matColor * float(amount);
            totalAmount += float(amount);
        }
    }
    
    // Normalize by total amount
    if (totalAmount > 0.0f) {
        finalColor /= totalAmount;
    } else {
        // No materials? Use dominant material as fallback
        finalColor = getMaterialColor(voxel.getDominantMaterialID());
    }
    
    return finalColor;
}

// Vertex interpolation for marching cubes
static glm::vec3 vertexInterp(float isolevel, const glm::vec3& p1, const glm::vec3& p2, float v1, float v2) {
    if (std::abs(isolevel - v1) < 0.00001f) return p1;
    if (std::abs(isolevel - v2) < 0.00001f) return p2;
    if (std::abs(v1 - v2) < 0.00001f) return p1;
    
    float mu = (isolevel - v1) / (v2 - v1);
    return p1 + mu * (p2 - p1);
}

bool VulkanRenderer::generateCPUReferenceMesh(octree::OctreePlanet* planet) {
    #pragma message("WARNING: CPU marching cubes is temporary debugging code - remove once GPU mesh works!")
    
    if (!planet) {
        std::cerr << "[CPU_REF] No planet data!" << std::endl;
        return false;
    }
    
    std::cout << "[CPU_REF] Starting fast marching cubes with vertex deduplication..." << std::endl;
    float radius = planet->getRadius();
    
    // Storage for vertices before deduplication
    std::vector<glm::vec3> rawPositions;
    std::vector<glm::vec3> rawColors;
    rawPositions.reserve(500000);
    rawColors.reserve(500000);
    
    int leafCount = 0;
    int processedLeaves = 0;
    
    // Process each leaf node
    // Note: We need non-const access for traverse, but getRoot() returns const
    // This is safe since we're only reading data, not modifying the tree
    const_cast<octree::OctreeNode*>(planet->getRoot())->traverse([&](octree::OctreeNode* node) {
        if (!node->isLeaf()) return;
        
        leafCount++;
        
        // Quick check if near surface
        float dist = glm::length(node->getCenter());
        float nodeRadius = node->getHalfSize() * 1.732f; // sqrt(3) for diagonal
        
        // Check if node could possibly intersect the surface
        if (dist - nodeRadius > radius * 1.2f || dist + nodeRadius < radius * 0.8f) {
            return; // Skip nodes that definitely don't intersect surface
        }
        
        processedLeaves++;
        
        // Debug first few leaves to understand the pattern
        static int debugCount = 0;
        bool debugThisLeaf = (debugCount++ < 5);
        
        if (debugThisLeaf) {
            std::cout << "\n[DEBUG] Leaf " << debugCount << " at position (" 
                      << node->getCenter().x << ", " << node->getCenter().y << ", " 
                      << node->getCenter().z << ")" << std::endl;
            
            // Check all 8 voxels in this leaf
            std::cout << "  Voxel materials in leaf:" << std::endl;
            const auto& leafVoxels = node->getVoxels();
            for (int v = 0; v < 8; v++) {
                core::MaterialID mat = leafVoxels[v].getDominantMaterialID();
                glm::vec3 color = getVoxelColor(leafVoxels[v]);
                std::cout << "    Voxel[" << v << "]: mat=" << static_cast<int>(mat) 
                          << " color=(" << color.r << "," << color.g << "," << color.b << ")";
                if (mat == core::MaterialID::Water) std::cout << " (Water)";
                else if (mat == core::MaterialID::Rock) std::cout << " (Rock)";
                else if (mat == core::MaterialID::Sand) std::cout << " (Sand)";
                else if (mat == core::MaterialID::Grass) std::cout << " (Grass)";
                std::cout << std::endl;
            }
        }
        
        // Process leaf as single marching cube
        float halfSize = node->getHalfSize();
        glm::vec3 minCorner = node->getCenter() - glm::vec3(halfSize);
        
        // Corner positions
        glm::vec3 corners[8] = {
            minCorner + glm::vec3(0, 0, 0),
            minCorner + glm::vec3(halfSize*2, 0, 0),
            minCorner + glm::vec3(halfSize*2, halfSize*2, 0),
            minCorner + glm::vec3(0, halfSize*2, 0),
            minCorner + glm::vec3(0, 0, halfSize*2),
            minCorner + glm::vec3(halfSize*2, 0, halfSize*2),
            minCorner + glm::vec3(halfSize*2, halfSize*2, halfSize*2),
            minCorner + glm::vec3(0, halfSize*2, halfSize*2)
        };
        
        // Sample density at corners using voxel data
        float densities[8];
        octree::MixedVoxel voxels[8];
        
        // Get voxels from the 2x2x2 leaf structure
        // Use the actual corner positions to sample density, not voxel centers
        for (int i = 0; i < 8; i++) {
            // For corners, use sphere-based density primarily
            // This ensures continuity across leaf boundaries
            float sphereDist = glm::length(corners[i]) - radius;
            densities[i] = -sphereDist; // Negative inside, positive outside
            
            // Get the nearest voxel for material color
            glm::vec3 localPos = corners[i] - node->getCenter();
            int voxelIndex = 0;
            if (localPos.x > 0) voxelIndex |= 1;
            if (localPos.y > 0) voxelIndex |= 2;
            if (localPos.z > 0) voxelIndex |= 4;
            
            voxels[i] = node->getVoxels()[voxelIndex];
            
            if (debugThisLeaf && i < 2) {
                core::MaterialID mat = voxels[i].getDominantMaterialID();
                glm::vec3 color = getVoxelColor(voxels[i]);
                std::cout << "  Corner " << i << ": voxelIndex=" << voxelIndex 
                          << ", material=" << static_cast<int>(mat)
                          << ", color=(" << color.r << "," << color.g << "," << color.b << ")" << std::endl;
            }
        }
        
        // Calculate cube index
        int cubeIndex = 0;
        for (int i = 0; i < 8; i++) {
            if (densities[i] < 0) { // Inside surface
                cubeIndex |= (1 << i);
            }
        }
        
        // Skip if completely inside or outside
        if (edgeTable[cubeIndex] == 0) {
            return;
        }
        
        // Find edge vertices
        glm::vec3 edgeVerts[12];
        glm::vec3 edgeColors[12];
        
        // Edge list for marching cubes
        static const int edges[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},
            {4,5}, {5,6}, {6,7}, {7,4},
            {0,4}, {1,5}, {2,6}, {3,7}
        };
        
        for (int i = 0; i < 12; i++) {
            if (edgeTable[cubeIndex] & (1 << i)) {
                int v1 = edges[i][0];
                int v2 = edges[i][1];
                
                edgeVerts[i] = vertexInterp(0.0f, corners[v1], corners[v2], densities[v1], densities[v2]);
                
                // Interpolate color
                float t = glm::length(edgeVerts[i] - corners[v1]) / glm::length(corners[v2] - corners[v1]);
                glm::vec3 color1 = getVoxelColor(voxels[v1]);
                glm::vec3 color2 = getVoxelColor(voxels[v2]);
                
                // TEMPORARY: Force a single color to test if rainbow comes from here
                bool FORCE_SINGLE_COLOR = true;  // Set to true to test
                if (FORCE_SINGLE_COLOR) {
                    edgeColors[i] = glm::vec3(0.3f, 0.5f, 0.7f);  // Blue-gray test color
                } else {
                    edgeColors[i] = glm::mix(color1, color2, t);
                }
            }
        }
        
        // Generate triangles
        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
            for (int j = 0; j < 3; j++) {
                int edge = triTable[cubeIndex][i + j];
                rawPositions.push_back(edgeVerts[edge]);
                rawColors.push_back(edgeColors[edge]);
            }
        }
    });
    
    std::cout << "[CPU_REF] Processed " << processedLeaves << " surface leaves (of " << leafCount << " total)" << std::endl;
    std::cout << "[CPU_REF] Generated " << rawPositions.size() << " raw vertices" << std::endl;
    
    // Debug: Sample some material types
    if (rawColors.size() > 0) {
        std::map<std::string, int> colorCounts;
        for (size_t i = 0; i < std::min(size_t(1000), rawColors.size()); i++) {
            glm::vec3 c = rawColors[i];
            std::string colorKey = std::to_string(int(c.r*100)) + "," + 
                                  std::to_string(int(c.g*100)) + "," + 
                                  std::to_string(int(c.b*100));
            colorCounts[colorKey]++;
        }
        std::cout << "[CPU_REF] Sample of first 1000 vertex colors (unique: " << colorCounts.size() << "):" << std::endl;
        int shown = 0;
        for (const auto& pair : colorCounts) {
            if (shown++ < 5) {
                std::cout << "  Color " << pair.first << ": " << pair.second << " vertices" << std::endl;
            }
        }
    }
    
    // Perform vertex deduplication
    std::unordered_map<glm::vec3, uint32_t> vertexMap;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    vertices.reserve(rawPositions.size() / 4); // Estimate ~4x duplication
    indices.reserve(rawPositions.size());
    
    const float EPSILON = 0.01f; // 1cm tolerance for deduplication
    
    for (size_t i = 0; i < rawPositions.size(); i++) {
        glm::vec3 pos = rawPositions[i];
        glm::vec3 color = rawColors[i];
        
        // Quantize position for deduplication
        glm::vec3 quantized = glm::vec3(
            std::round(pos.x / EPSILON) * EPSILON,
            std::round(pos.y / EPSILON) * EPSILON,
            std::round(pos.z / EPSILON) * EPSILON
        );
        
        auto it = vertexMap.find(quantized);
        uint32_t index;
        
        if (it != vertexMap.end()) {
            // Reuse existing vertex
            index = it->second;
        } else {
            // Create new vertex
            index = static_cast<uint32_t>(vertices.size());
            vertexMap[quantized] = index;
            
            Vertex vertex{};
            vertex.position = pos; // Use original position, not quantized
            vertex.normal = glm::normalize(pos); // Sphere normal
            vertex.color = color;
            
            // Store altitude for shader
            float altitude = glm::length(pos) - radius;
            vertex.texCoord = glm::vec2(altitude, 0.0f);
            
            vertices.push_back(vertex);
        }
        
        indices.push_back(index);
    }
    
    std::cout << "[CPU_REF] After deduplication: " << vertices.size() << " unique vertices" << std::endl;
    std::cout << "[CPU_REF] Generated " << indices.size() << " indices (" 
              << (indices.size() / 3) << " triangles)" << std::endl;
    
    if (vertices.empty()) {
        std::cerr << "[CPU_REF] No vertices generated!" << std::endl;
        return false;
    }
    
    // Sample vertex for debugging
    std::cout << "[CPU_REF] Sample vertex colors:" << std::endl;
    const auto& v = vertices[0];
    std::cout << "  Vertex 0: color(" 
              << v.color.r << ", " << v.color.g << ", " << v.color.b << ")" << std::endl;
    
    // Upload to GPU
    std::cout << "[CPU_REF] Uploading mesh to GPU buffers..." << std::endl;
    
    size_t vertexDataSize = vertices.size() * sizeof(Vertex);
    size_t indexDataSize = indices.size() * sizeof(uint32_t);
    
    bool success = uploadCPUReferenceMesh(
        vertices.data(), vertexDataSize,
        indices.data(), indexDataSize,
        static_cast<uint32_t>(vertices.size()),  // vertexCount
        static_cast<uint32_t>(indices.size())    // indexCount
    );
    
    if (success) {
        std::cout << "[CPU_REF] Upload complete! Ready to render " 
                  << (indices.size() / 3) << " triangles" << std::endl;
    } else {
        std::cerr << "[CPU_REF] Failed to upload mesh to GPU!" << std::endl;
    }
    
    return success;
}

} // namespace rendering