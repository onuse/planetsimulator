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
    // First check the octree for voxel data
    const octree::Voxel* voxel = planet.getVoxel(position);
    
    if (voxel) {
        // Use actual voxel data from octree
        auto materialID = voxel->getDominantMaterialID();
        
        // Air/Vacuum = positive (outside), solid = negative (inside)
        if (materialID == core::MaterialID::Air || materialID == core::MaterialID::Vacuum) {
            return 1.0f;
        } else {
            return -1.0f;
        }
    }
    
    // Fallback: use distance-based density for areas without voxel data
    float distFromCenter = glm::length(position);
    float planetRadius = planet.getRadius();
    
    // Simple threshold
    if (distFromCenter < planetRadius) {
        return -1.0f; // Inside = solid
    } else {
        return 1.0f;  // Outside = air
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
    // For a spherical planet, the normal should point outward from the center
    // This gives us smooth shading that follows the sphere's curvature
    glm::vec3 normal = glm::normalize(position); // Position relative to origin (planet center)
    
    // Optional: blend with gradient-based normal for surface details
    // But for now, just use the spherical normal for smooth shading
    return normal;
}

// Transvoxel mesh generation
MeshData generateTransvoxelMesh(const MeshGenParams& params, const octree::OctreePlanet& planet) {
    MeshData mesh;
    
    static int debugCallCount = 0;
    bool doDebug = (debugCallCount++ < 3);
    
    if (doDebug) {
        std::cout << "[DEBUG Transvoxel] Call " << debugCallCount << ": Generating mesh at position (" 
                  << params.worldPos.x << ", " << params.worldPos.y << ", " 
                  << params.worldPos.z << ") with voxel size " << params.voxelSize 
                  << " and grid " << params.dimensions.x << "x" 
                  << params.dimensions.y << "x" << params.dimensions.z << "\n";
    }
    
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
    
    int solidCount = 0;
    int airCount = 0;
    
    // Sample density at corners
    // Check if we should use cube-to-sphere mapping
    bool useCubeSphere = (params.faceId >= 0 && params.faceId < 6);
    
    if (useCubeSphere) {
        // CUBE-SPHERE MAPPING: Transform grid points through cube-to-sphere
        float planetRadius = planet.getRadius();
        
        // Get face basis vectors
        glm::vec3 faceNormal, faceUp, faceRight;
        switch (params.faceId) {
            case 0: // +X
                faceNormal = glm::vec3(1, 0, 0);
                faceUp = glm::vec3(0, 1, 0);
                faceRight = glm::vec3(0, 0, 1);
                break;
            case 1: // -X
                faceNormal = glm::vec3(-1, 0, 0);
                faceUp = glm::vec3(0, 1, 0);
                faceRight = glm::vec3(0, 0, -1);
                break;
            case 2: // +Y
                faceNormal = glm::vec3(0, 1, 0);
                faceUp = glm::vec3(0, 0, 1);
                faceRight = glm::vec3(1, 0, 0);
                break;
            case 3: // -Y
                faceNormal = glm::vec3(0, -1, 0);
                faceUp = glm::vec3(0, 0, -1);
                faceRight = glm::vec3(1, 0, 0);
                break;
            case 4: // +Z
                faceNormal = glm::vec3(0, 0, 1);
                faceUp = glm::vec3(0, 1, 0);
                faceRight = glm::vec3(-1, 0, 0);
                break;
            case 5: // -Z
                faceNormal = glm::vec3(0, 0, -1);
                faceUp = glm::vec3(0, 1, 0);
                faceRight = glm::vec3(1, 0, 0);
                break;
        }
        
        for (int z = 0; z < sizeZ; z++) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    // Map grid coordinates to face coordinates (-1 to 1)
                    float u = (x / float(params.dimensions.x)) * 2.0f - 1.0f;
                    float v = (y / float(params.dimensions.y)) * 2.0f - 1.0f;
                    
                    // Depth coordinate (for shell thickness)
                    float depth = (z / float(params.dimensions.z)) * 2.0f - 1.0f;
                    float radiusScale = 1.0f + depth * 0.1f; // ±10% radius variation
                    
                    // Create cube position on face
                    glm::vec3 cubePos = faceNormal + u * faceRight + v * faceUp;
                    
                    // Transform cube position to sphere
                    glm::vec3 sphereDir = glm::normalize(cubePos);
                    
                    // Apply cube-to-sphere mapping for smooth deformation
                    float x2 = cubePos.x * cubePos.x;
                    float y2 = cubePos.y * cubePos.y;
                    float z2 = cubePos.z * cubePos.z;
                    
                    glm::vec3 spherePos;
                    spherePos.x = cubePos.x * sqrt(1.0f - y2 * 0.5f - z2 * 0.5f + y2 * z2 / 3.0f);
                    spherePos.y = cubePos.y * sqrt(1.0f - x2 * 0.5f - z2 * 0.5f + x2 * z2 / 3.0f);
                    spherePos.z = cubePos.z * sqrt(1.0f - x2 * 0.5f - y2 * 0.5f + x2 * y2 / 3.0f);
                    
                    // Scale to planet radius with depth variation
                    glm::vec3 worldPos = spherePos * planetRadius * radiusScale;
                    
                    int idx = x + y * sizeX + z * sizeX * sizeY;
                    densityField[idx] = sampleDensity(planet, worldPos);
                    
                    if (densityField[idx] < 0) solidCount++;
                    else airCount++;
                }
            }
        }
    } else {
        // Original Cartesian grid approach
        for (int z = 0; z < sizeZ; z++) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    glm::vec3 worldPos = params.worldPos + 
                        glm::vec3(x * params.voxelSize,
                                 y * params.voxelSize,
                                 z * params.voxelSize);
                    
                    int idx = x + y * sizeX + z * sizeX * sizeY;
                    densityField[idx] = sampleDensity(planet, worldPos);
                    
                    if (densityField[idx] < 0) solidCount++;
                    else airCount++;
                    
                    // Debug first few samples
                    if (doDebug && x < 2 && y < 2 && z < 2) {
                        std::cout << "[DEBUG] Sample at (" << worldPos.x << ", " << worldPos.y 
                                  << ", " << worldPos.z << ") = " << densityField[idx] 
                                  << " (dist from center: " << glm::length(worldPos) << ")\n";
                    }
                }
            }
        }
    }
    
    if (doDebug) {
        std::cout << "[DEBUG] Density field: " << solidCount << " solid, " 
                  << airCount << " air samples\n";
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
                
                // Corner positions - either cube-sphere or world space
                glm::vec3 cornerPositions[8];
                
                if (useCubeSphere) {
                    // Use same transformation as density sampling
                    float planetRadius = planet.getRadius();
                    glm::vec3 faceNormal, faceUp, faceRight;
                    
                    // Get face basis (same as above)
                    switch (params.faceId) {
                        case 0: faceNormal = glm::vec3(1,0,0); faceUp = glm::vec3(0,1,0); faceRight = glm::vec3(0,0,1); break;
                        case 1: faceNormal = glm::vec3(-1,0,0); faceUp = glm::vec3(0,1,0); faceRight = glm::vec3(0,0,-1); break;
                        case 2: faceNormal = glm::vec3(0,1,0); faceUp = glm::vec3(0,0,1); faceRight = glm::vec3(1,0,0); break;
                        case 3: faceNormal = glm::vec3(0,-1,0); faceUp = glm::vec3(0,0,-1); faceRight = glm::vec3(1,0,0); break;
                        case 4: faceNormal = glm::vec3(0,0,1); faceUp = glm::vec3(0,1,0); faceRight = glm::vec3(-1,0,0); break;
                        case 5: faceNormal = glm::vec3(0,0,-1); faceUp = glm::vec3(0,1,0); faceRight = glm::vec3(1,0,0); break;
                    }
                    
                    // Generate corner positions using cube-sphere mapping
                    for (int i = 0; i < 8; i++) {
                        int cx = x + (i & 1);
                        int cy = y + ((i >> 1) & 1);
                        int cz = z + ((i >> 2) & 1);
                        
                        float u = (cx / float(params.dimensions.x)) * 2.0f - 1.0f;
                        float v = (cy / float(params.dimensions.y)) * 2.0f - 1.0f;
                        float depth = (cz / float(params.dimensions.z)) * 2.0f - 1.0f;
                        float radiusScale = 1.0f + depth * 0.1f;
                        
                        glm::vec3 cubePos = faceNormal + u * faceRight + v * faceUp;
                        
                        // Cube-to-sphere transformation
                        float x2 = cubePos.x * cubePos.x;
                        float y2 = cubePos.y * cubePos.y;
                        float z2 = cubePos.z * cubePos.z;
                        
                        glm::vec3 spherePos;
                        spherePos.x = cubePos.x * sqrt(1.0f - y2*0.5f - z2*0.5f + y2*z2/3.0f);
                        spherePos.y = cubePos.y * sqrt(1.0f - x2*0.5f - z2*0.5f + x2*z2/3.0f);
                        spherePos.z = cubePos.z * sqrt(1.0f - x2*0.5f - y2*0.5f + x2*y2/3.0f);
                        
                        cornerPositions[i] = spherePos * planetRadius * radiusScale;
                    }
                } else {
                    // Original world-space positions
                    cornerPositions[0] = params.worldPos + glm::vec3(x + 0, y + 0, z + 0) * params.voxelSize;
                    cornerPositions[1] = params.worldPos + glm::vec3(x + 1, y + 0, z + 0) * params.voxelSize;
                    cornerPositions[2] = params.worldPos + glm::vec3(x + 1, y + 0, z + 1) * params.voxelSize;
                    cornerPositions[3] = params.worldPos + glm::vec3(x + 0, y + 0, z + 1) * params.voxelSize;
                    cornerPositions[4] = params.worldPos + glm::vec3(x + 0, y + 1, z + 0) * params.voxelSize;
                    cornerPositions[5] = params.worldPos + glm::vec3(x + 1, y + 1, z + 0) * params.voxelSize;
                    cornerPositions[6] = params.worldPos + glm::vec3(x + 1, y + 1, z + 1) * params.voxelSize;
                    cornerPositions[7] = params.worldPos + glm::vec3(x + 0, y + 1, z + 1) * params.voxelSize;
                }
                
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
                    
                    // Debug first cell
                    static bool debuggedFirst = false;
                    bool debugThis = (!debuggedFirst && x == 0 && y == 0 && z == 0);
                    if (debugThis) {
                        debuggedFirst = true;
                        std::cout << "[DEBUG] First cell triangle, edge indices: " 
                                  << (int)cellData[i] << ", " << (int)cellData[i+1] 
                                  << ", " << (int)cellData[i+2] << "\n";
                    }
                    
                    for (int j = 0; j < 3; j++) {
                        uint8_t edgeIndex = cellData[i + j];
                        if (edgeIndex >= 12) {
                            std::cerr << "ERROR: Invalid edge index " << (int)edgeIndex << " in cell data!\n";
                            continue;
                        }
                        
                        glm::vec3 pos = edgeVertices[edgeIndex];
                        
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
                            
                            // KEEP VERTICES IN WORLD SPACE - camera-relative transform happens in shader
                            mesh.vertices.emplace_back(pos, normal, color);
                            vertexMap[pos] = newIndex;
                            vertIndices[j] = newIndex;
                        }
                    }
                    
                    // Only add triangle if all vertices are valid
                    if (vertIndices[0] != vertIndices[1] && 
                        vertIndices[1] != vertIndices[2] && 
                        vertIndices[0] != vertIndices[2]) {
                        mesh.indices.push_back(vertIndices[0]);
                        mesh.indices.push_back(vertIndices[1]);
                        mesh.indices.push_back(vertIndices[2]);
                    } else if (debugThis) {
                        std::cout << "[DEBUG] Skipping degenerate triangle: " 
                                  << vertIndices[0] << ", " << vertIndices[1] 
                                  << ", " << vertIndices[2] << "\n";
                    }
                }
            }
        }
    }
    
    return mesh;
}

} // namespace algorithms