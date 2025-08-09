#include "algorithms/mesh_generation.hpp"
#include "core/material_table.hpp"
#include <unordered_map>
#include <functional>
#include <cmath>
#include <iostream>

namespace algorithms {

// Hash and equality for vertex deduplication
struct Vec3Hash {
    std::size_t operator()(const glm::vec3& v) const {
        int x = static_cast<int>(std::round(v.x * 1000.0f));
        int y = static_cast<int>(std::round(v.y * 1000.0f));
        int z = static_cast<int>(std::round(v.z * 1000.0f));
        
        std::size_t h1 = std::hash<int>{}(x);
        std::size_t h2 = std::hash<int>{}(y);
        std::size_t h3 = std::hash<int>{}(z);
        
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct Vec3Equal {
    bool operator()(const glm::vec3& a, const glm::vec3& b) const {
        const float epsilon = 0.001f;
        return std::abs(a.x - b.x) < epsilon &&
               std::abs(a.y - b.y) < epsilon &&
               std::abs(a.z - b.z) < epsilon;
    }
};

// Simple cube-based mesh generation
MeshData generateSimpleCubeMesh(const MeshGenParams& params, const octree::OctreePlanet& planet) {
    MeshData mesh;
    std::unordered_map<glm::vec3, uint32_t, Vec3Hash, Vec3Equal> vertexMap;
    
    auto isSolid = [&](int x, int y, int z) -> bool {
        glm::vec3 worldPos = params.worldPos + glm::vec3(x, y, z) * params.voxelSize;
        const octree::Voxel* voxel = planet.getVoxel(worldPos);
        return voxel && voxel->getDominantMaterialID() != core::MaterialID::Air;
    };
    
    // Generate surface faces for each solid voxel
    for (int z = 0; z < params.dimensions.z; z++) {
        for (int y = 0; y < params.dimensions.y; y++) {
            for (int x = 0; x < params.dimensions.x; x++) {
                glm::vec3 voxelWorldPos = params.worldPos + glm::vec3(x, y, z) * params.voxelSize;
                
                const octree::Voxel* voxel = planet.getVoxel(voxelWorldPos);
                if (!voxel || voxel->getDominantMaterialID() == core::MaterialID::Air) continue;
                
                float halfSize = params.voxelSize * 0.5f;
                glm::vec3 center = voxelWorldPos;
                glm::vec3 color = voxel->getColor();
                
                // Define corners
                glm::vec3 corners[8] = {
                    center + glm::vec3(-halfSize, -halfSize, -halfSize),
                    center + glm::vec3( halfSize, -halfSize, -halfSize),
                    center + glm::vec3( halfSize,  halfSize, -halfSize),
                    center + glm::vec3(-halfSize,  halfSize, -halfSize),
                    center + glm::vec3(-halfSize, -halfSize,  halfSize),
                    center + glm::vec3( halfSize, -halfSize,  halfSize),
                    center + glm::vec3( halfSize,  halfSize,  halfSize),
                    center + glm::vec3(-halfSize,  halfSize,  halfSize)
                };
                
                // Face definitions
                struct Face {
                    glm::ivec3 neighborOffset;
                    glm::vec3 normal;
                    int indices[4];
                };
                
                Face faces[6] = {
                    { glm::ivec3( 0,  0, -1), glm::vec3( 0,  0, -1), {0, 1, 2, 3} },
                    { glm::ivec3( 0,  0,  1), glm::vec3( 0,  0,  1), {5, 4, 7, 6} },
                    { glm::ivec3(-1,  0,  0), glm::vec3(-1,  0,  0), {4, 0, 3, 7} },
                    { glm::ivec3( 1,  0,  0), glm::vec3( 1,  0,  0), {1, 5, 6, 2} },
                    { glm::ivec3( 0, -1,  0), glm::vec3( 0, -1,  0), {4, 5, 1, 0} },
                    { glm::ivec3( 0,  1,  0), glm::vec3( 0,  1,  0), {3, 2, 6, 7} }
                };
                
                // Check each face
                for (const auto& face : faces) {
                    int nx = x + face.neighborOffset.x;
                    int ny = y + face.neighborOffset.y;
                    int nz = z + face.neighborOffset.z;
                    
                    if (isSolid(nx, ny, nz)) continue; // Skip internal faces
                    
                    // Generate face with shared vertices
                    uint32_t vertIndices[4];
                    
                    for (int i = 0; i < 4; i++) {
                        glm::vec3 pos = corners[face.indices[i]];
                        
                        auto it = vertexMap.find(pos);
                        if (it != vertexMap.end()) {
                            vertIndices[i] = it->second;
                        } else {
                            uint32_t newIndex = static_cast<uint32_t>(mesh.vertices.size());
                            mesh.vertices.emplace_back(pos, face.normal, color);
                            vertexMap[pos] = newIndex;
                            vertIndices[i] = newIndex;
                        }
                    }
                    
                    // Generate two triangles (CCW winding)
                    mesh.indices.push_back(vertIndices[0]);
                    mesh.indices.push_back(vertIndices[1]);
                    mesh.indices.push_back(vertIndices[2]);
                    
                    mesh.indices.push_back(vertIndices[0]);
                    mesh.indices.push_back(vertIndices[2]);
                    mesh.indices.push_back(vertIndices[3]);
                }
            }
        }
    }
    
    return mesh;
}

// Transvoxel lookup tables
static const uint8_t regularCellClass[256] = {
    0x00, 0x01, 0x01, 0x03, 0x01, 0x03, 0x02, 0x04, 0x01, 0x02, 0x03, 0x04, 0x03, 0x04, 0x04, 0x03,
    0x01, 0x03, 0x02, 0x04, 0x02, 0x04, 0x06, 0x0C, 0x02, 0x05, 0x05, 0x0B, 0x05, 0x0A, 0x07, 0x04,
    0x01, 0x02, 0x03, 0x04, 0x02, 0x05, 0x05, 0x0A, 0x03, 0x04, 0x04, 0x03, 0x05, 0x07, 0x07, 0x04,
    0x03, 0x04, 0x05, 0x03, 0x05, 0x07, 0x07, 0x04, 0x04, 0x03, 0x0A, 0x04, 0x0B, 0x04, 0x08, 0x03,
    0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x05, 0x07, 0x02, 0x06, 0x05, 0x07, 0x04, 0x0C, 0x0A, 0x04,
    0x02, 0x05, 0x06, 0x07, 0x05, 0x0A, 0x07, 0x04, 0x06, 0x08, 0x0E, 0x04, 0x07, 0x04, 0x09, 0x03,
    0x03, 0x04, 0x04, 0x03, 0x05, 0x0A, 0x0B, 0x04, 0x04, 0x0C, 0x03, 0x04, 0x07, 0x04, 0x04, 0x03,
    0x04, 0x03, 0x07, 0x04, 0x0B, 0x04, 0x08, 0x03, 0x0C, 0x04, 0x04, 0x03, 0x08, 0x03, 0x02, 0x01,
    0x01, 0x03, 0x02, 0x04, 0x02, 0x04, 0x06, 0x0C, 0x03, 0x05, 0x04, 0x03, 0x04, 0x03, 0x0C, 0x04,
    0x02, 0x04, 0x06, 0x0C, 0x06, 0x0C, 0x08, 0x04, 0x05, 0x07, 0x0A, 0x04, 0x0A, 0x04, 0x04, 0x03,
    0x02, 0x06, 0x05, 0x07, 0x06, 0x08, 0x0A, 0x04, 0x04, 0x0C, 0x03, 0x04, 0x0A, 0x04, 0x04, 0x03,
    0x05, 0x0A, 0x07, 0x04, 0x07, 0x04, 0x04, 0x03, 0x03, 0x04, 0x04, 0x03, 0x04, 0x03, 0x02, 0x01,
    0x03, 0x05, 0x05, 0x0B, 0x04, 0x03, 0x0A, 0x04, 0x05, 0x07, 0x0B, 0x04, 0x03, 0x04, 0x04, 0x03,
    0x05, 0x0B, 0x07, 0x04, 0x0A, 0x04, 0x04, 0x03, 0x07, 0x08, 0x04, 0x03, 0x04, 0x03, 0x02, 0x01,
    0x04, 0x0A, 0x07, 0x04, 0x03, 0x04, 0x04, 0x03, 0x0C, 0x04, 0x04, 0x03, 0x04, 0x03, 0x02, 0x01,
    0x03, 0x04, 0x04, 0x03, 0x04, 0x03, 0x02, 0x01, 0x04, 0x03, 0x02, 0x01, 0x02, 0x01, 0x01, 0x00
};

static const uint8_t regularCellData[16][12] = {
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x00, 0x08, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x00, 0x01, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x01, 0x08, 0x03, 0x09, 0x08, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x01, 0x02, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x00, 0x08, 0x03, 0x01, 0x02, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x09, 0x02, 0x0A, 0x00, 0x02, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x02, 0x08, 0x03, 0x02, 0x0A, 0x08, 0x0A, 0x09, 0x08, 0xFF, 0xFF, 0xFF },
    { 0x03, 0x0B, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x00, 0x0B, 0x02, 0x08, 0x0B, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x01, 0x09, 0x00, 0x02, 0x03, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x01, 0x0B, 0x02, 0x01, 0x09, 0x0B, 0x09, 0x08, 0x0B, 0xFF, 0xFF, 0xFF },
    { 0x03, 0x0A, 0x01, 0x03, 0x0B, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x00, 0x0A, 0x01, 0x00, 0x08, 0x0A, 0x08, 0x0B, 0x0A, 0xFF, 0xFF, 0xFF },
    { 0x03, 0x09, 0x00, 0x03, 0x0B, 0x09, 0x0B, 0x0A, 0x09, 0xFF, 0xFF, 0xFF },
    { 0x09, 0x08, 0x0B, 0x0B, 0x08, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};

// Helper functions for Transvoxel
static float sampleDensity(const octree::OctreePlanet& planet, const glm::vec3& position) {
    const octree::Voxel* voxel = planet.getVoxel(position);
    if (!voxel) return 1.0f; // Outside = empty
    
    if (voxel->getDominantMaterialID() == core::MaterialID::Air) {
        return 1.0f;  // Air = positive (outside)
    } else {
        return -1.0f; // Solid = negative (inside)
    }
}

static glm::vec3 interpolateVertex(const glm::vec3& p1, const glm::vec3& p2, float val1, float val2) {
    if (std::abs(val1) < 1e-5f) return p1;
    if (std::abs(val2) < 1e-5f) return p2;
    if (std::abs(val1 - val2) < 1e-5f) return p1;
    
    float t = val1 / (val1 - val2);
    return p1 + t * (p2 - p1);
}

static glm::vec3 calculateNormal(const octree::OctreePlanet& planet, const glm::vec3& position, float voxelSize) {
    float h = voxelSize * 0.5f;
    
    float dx = sampleDensity(planet, position + glm::vec3(h, 0, 0)) - 
               sampleDensity(planet, position - glm::vec3(h, 0, 0));
    float dy = sampleDensity(planet, position + glm::vec3(0, h, 0)) - 
               sampleDensity(planet, position - glm::vec3(0, h, 0));
    float dz = sampleDensity(planet, position + glm::vec3(0, 0, h)) - 
               sampleDensity(planet, position - glm::vec3(0, 0, h));
    
    glm::vec3 normal(-dx, -dy, -dz);
    
    if (glm::length(normal) > 0.001f) {
        return glm::normalize(normal);
    }
    return glm::vec3(0, 1, 0);
}

// Transvoxel mesh generation
MeshData generateTransvoxelMesh(const MeshGenParams& params, const octree::OctreePlanet& planet) {
    MeshData mesh;
    
    // Validate that octree has been initialized
    if (!planet.getRoot()) {
        std::cerr << "ERROR: generateTransvoxelMesh called with uninitialized octree. Call planet.generate() first!\n";
        return mesh; // Return empty mesh
    }
    
    // Quick test to see if octree has valid data
    const octree::Voxel* testVoxel = planet.getVoxel(params.worldPos);
    if (!testVoxel) {
        std::cerr << "WARNING: generateTransvoxelMesh - octree appears to have no voxel data at position ("
                  << params.worldPos.x << ", " << params.worldPos.y << ", " << params.worldPos.z << ")\n";
        // Continue anyway, as this might be intentional (empty region)
    }
    
    std::unordered_map<glm::vec3, uint32_t, Vec3Hash, Vec3Equal> vertexMap;
    
    // Create density field
    int sizeX = params.dimensions.x + 1;
    int sizeY = params.dimensions.y + 1;
    int sizeZ = params.dimensions.z + 1;
    
    std::vector<float> densityField(sizeX * sizeY * sizeZ);
    
    // Sample density at corners
    for (int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                glm::vec3 worldPos = params.worldPos + glm::vec3(x, y, z) * params.voxelSize;
                int idx = x + y * sizeX + z * sizeX * sizeY;
                densityField[idx] = sampleDensity(planet, worldPos);
            }
        }
    }
    
    // Process each cell
    for (int z = 0; z < params.dimensions.z; z++) {
        for (int y = 0; y < params.dimensions.y; y++) {
            for (int x = 0; x < params.dimensions.x; x++) {
                // Get corner densities
                float cornerDensities[8];
                cornerDensities[0] = densityField[(x + 0) + (y + 0) * sizeX + (z + 0) * sizeX * sizeY];
                cornerDensities[1] = densityField[(x + 1) + (y + 0) * sizeX + (z + 0) * sizeX * sizeY];
                cornerDensities[2] = densityField[(x + 1) + (y + 0) * sizeX + (z + 1) * sizeX * sizeY];
                cornerDensities[3] = densityField[(x + 0) + (y + 0) * sizeX + (z + 1) * sizeX * sizeY];
                cornerDensities[4] = densityField[(x + 0) + (y + 1) * sizeX + (z + 0) * sizeX * sizeY];
                cornerDensities[5] = densityField[(x + 1) + (y + 1) * sizeX + (z + 0) * sizeX * sizeY];
                cornerDensities[6] = densityField[(x + 1) + (y + 1) * sizeX + (z + 1) * sizeX * sizeY];
                cornerDensities[7] = densityField[(x + 0) + (y + 1) * sizeX + (z + 1) * sizeX * sizeY];
                
                // Calculate case index
                uint8_t caseIndex = 0;
                for (int i = 0; i < 8; i++) {
                    if (cornerDensities[i] < 0) {
                        caseIndex |= (1 << i);
                    }
                }
                
                // Skip empty/full cells
                if (caseIndex == 0 || caseIndex == 255) continue;
                
                // Get cell configuration
                uint8_t cellClass = regularCellClass[caseIndex];
                if (cellClass == 0) continue;
                
                const uint8_t* cellData = regularCellData[cellClass];
                
                // Corner positions
                glm::vec3 cornerPositions[8];
                cornerPositions[0] = params.worldPos + glm::vec3(x + 0, y + 0, z + 0) * params.voxelSize;
                cornerPositions[1] = params.worldPos + glm::vec3(x + 1, y + 0, z + 0) * params.voxelSize;
                cornerPositions[2] = params.worldPos + glm::vec3(x + 1, y + 0, z + 1) * params.voxelSize;
                cornerPositions[3] = params.worldPos + glm::vec3(x + 0, y + 0, z + 1) * params.voxelSize;
                cornerPositions[4] = params.worldPos + glm::vec3(x + 0, y + 1, z + 0) * params.voxelSize;
                cornerPositions[5] = params.worldPos + glm::vec3(x + 1, y + 1, z + 0) * params.voxelSize;
                cornerPositions[6] = params.worldPos + glm::vec3(x + 1, y + 1, z + 1) * params.voxelSize;
                cornerPositions[7] = params.worldPos + glm::vec3(x + 0, y + 1, z + 1) * params.voxelSize;
                
                // Edge table
                static const int edges[12][2] = {
                    {0, 1}, {1, 2}, {2, 3}, {3, 0},
                    {4, 5}, {5, 6}, {6, 7}, {7, 4},
                    {0, 4}, {1, 5}, {2, 6}, {3, 7}
                };
                
                // Generate vertices on edges
                glm::vec3 edgeVertices[12];
                for (int i = 0; i < 12; i++) {
                    int v0 = edges[i][0];
                    int v1 = edges[i][1];
                    edgeVertices[i] = interpolateVertex(
                        cornerPositions[v0], cornerPositions[v1],
                        cornerDensities[v0], cornerDensities[v1]
                    );
                }
                
                // Generate triangles with vertex deduplication
                for (int i = 0; cellData[i] != 0xFF && i < 12; i += 3) {
                    uint32_t vertIndices[3];
                    
                    for (int j = 0; j < 3; j++) {
                        glm::vec3 pos = edgeVertices[cellData[i + j]];
                        
                        // Check if vertex already exists
                        auto it = vertexMap.find(pos);
                        if (it != vertexMap.end()) {
                            vertIndices[j] = it->second;
                        } else {
                            // Create new vertex
                            uint32_t newIndex = static_cast<uint32_t>(mesh.vertices.size());
                            
                            glm::vec3 normal = calculateNormal(planet, pos, params.voxelSize);
                            
                            // Get material color
                            const octree::Voxel* voxel = planet.getVoxel(pos);
                            glm::vec3 color = voxel ? voxel->getColor() : glm::vec3(0.5f);
                            
                            mesh.vertices.emplace_back(pos, normal, color);
                            vertexMap[pos] = newIndex;
                            vertIndices[j] = newIndex;
                        }
                    }
                    
                    // Add triangle indices
                    mesh.indices.push_back(vertIndices[0]);
                    mesh.indices.push_back(vertIndices[1]);
                    mesh.indices.push_back(vertIndices[2]);
                }
            }
        }
    }
    
    return mesh;
}

} // namespace algorithms