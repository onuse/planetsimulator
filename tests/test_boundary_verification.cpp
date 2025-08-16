#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

// Helper to quantize positions for comparison
struct VertexKey {
    int64_t x, y, z;
    
    VertexKey(const glm::vec3& pos, double scale = 1000000.0) {
        x = static_cast<int64_t>(std::round(pos.x * scale));
        y = static_cast<int64_t>(std::round(pos.y * scale));
        z = static_cast<int64_t>(std::round(pos.z * scale));
    }
    
    bool operator==(const VertexKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

namespace std {
    template<> struct hash<VertexKey> {
        size_t operator()(const VertexKey& k) const {
            return hash<int64_t>()(k.x) ^ (hash<int64_t>()(k.y) << 1) ^ (hash<int64_t>()(k.z) << 2);
        }
    };
}

void analyzePatchBoundaries() {
    std::cout << "\n=== PATCH BOUNDARY VERIFICATION TEST ===\n\n";
    
    // Create quadtree
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.maxLevel = 10;
    config.enableFaceCulling = false;
    
    auto densityField = std::make_shared<core::DensityField>(config.planetRadius);
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Update with a camera position
    glm::vec3 viewPos(config.planetRadius * 2.5f, 0, 0);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 1.0f, 100000000.0f);
    glm::mat4 viewProj = proj * view;
    
    quadtree.update(viewPos, viewProj, 0.016f);
    
    auto patches = quadtree.getVisiblePatches();
    std::cout << "Got " << patches.size() << " visible patches\n\n";
    
    // Generate vertices for each patch
    rendering::CPUVertexGenerator::Config genConfig;
    genConfig.gridResolution = 65;
    genConfig.planetRadius = config.planetRadius;
    genConfig.enableVertexCaching = false;  // Disable caching for this test
    
    rendering::CPUVertexGenerator generator(genConfig);
    
    // Map to store all vertices by position
    std::unordered_map<VertexKey, std::vector<int>> vertexToPatchMap;
    std::vector<std::vector<glm::vec3>> patchVertices;
    
    // Generate vertices for each patch
    for (size_t i = 0; i < patches.size(); i++) {
        const auto& patch = patches[i];
        auto mesh = generator.generatePatchMesh(patch, patch.patchTransform);
        
        std::vector<glm::vec3> vertices;
        for (const auto& v : mesh.vertices) {
            vertices.push_back(v.position);
            VertexKey key(v.position);
            vertexToPatchMap[key].push_back(i);
        }
        patchVertices.push_back(vertices);
        
        if (i < 5) {  // Print first few patches
            std::cout << "Patch " << i << ":\n";
            std::cout << "  Face: " << patch.faceId << ", Level: " << patch.level << "\n";
            std::cout << "  Center: (" << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << ")\n";
            std::cout << "  Vertices: " << vertices.size() << "\n";
        }
    }
    
    // Find shared vertices
    int sharedCount = 0;
    int boundaryVertices = 0;
    std::unordered_map<int, int> sharingHistogram;
    
    for (const auto& [key, patchList] : vertexToPatchMap) {
        sharingHistogram[patchList.size()]++;
        if (patchList.size() > 1) {
            sharedCount++;
            if (patchList.size() == 2) {
                boundaryVertices++;
            }
        }
    }
    
    std::cout << "\n=== VERTEX SHARING ANALYSIS ===\n";
    std::cout << "Total unique vertices: " << vertexToPatchMap.size() << "\n";
    std::cout << "Shared vertices: " << sharedCount << "\n";
    std::cout << "Boundary vertices (shared by exactly 2 patches): " << boundaryVertices << "\n";
    
    std::cout << "\nSharing histogram:\n";
    for (const auto& [count, freq] : sharingHistogram) {
        std::cout << "  " << freq << " vertices shared by " << count << " patch(es)\n";
    }
    
    // Check specific face boundaries
    std::cout << "\n=== FACE BOUNDARY ANALYSIS ===\n";
    
    // Find patches on different faces
    std::unordered_map<int, std::vector<int>> facePatches;
    for (size_t i = 0; i < patches.size(); i++) {
        facePatches[patches[i].faceId].push_back(i);
    }
    
    std::cout << "Patches per face:\n";
    for (const auto& [face, patchList] : facePatches) {
        std::cout << "  Face " << face << ": " << patchList.size() << " patches\n";
    }
    
    // Check for gaps between adjacent patches
    std::cout << "\n=== GAP DETECTION ===\n";
    
    // For each patch, find its expected neighbors
    for (size_t i = 0; i < std::min(size_t(10), patches.size()); i++) {
        const auto& patch = patches[i];
        
        // Get patch bounds
        glm::vec3 minBounds(1e9f), maxBounds(-1e9f);
        for (const auto& v : patchVertices[i]) {
            minBounds = glm::min(minBounds, v);
            maxBounds = glm::max(maxBounds, v);
        }
        
        std::cout << "\nPatch " << i << " (Face " << patch.faceId << ", Level " << patch.level << "):\n";
        std::cout << "  Bounds: (" << minBounds.x << "," << minBounds.y << "," << minBounds.z 
                  << ") to (" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")\n";
        
        // Check edge vertices
        int edgeVertices = 0;
        int sharedEdgeVertices = 0;
        
        for (const auto& v : patchVertices[i]) {
            // Check if vertex is on an edge (within epsilon of bounds)
            const float eps = 0.001f;
            bool onEdge = false;
            
            if (std::abs(v.x - minBounds.x) < eps || std::abs(v.x - maxBounds.x) < eps ||
                std::abs(v.y - minBounds.y) < eps || std::abs(v.y - maxBounds.y) < eps ||
                std::abs(v.z - minBounds.z) < eps || std::abs(v.z - maxBounds.z) < eps) {
                onEdge = true;
                edgeVertices++;
                
                VertexKey key(v);
                if (vertexToPatchMap[key].size() > 1) {
                    sharedEdgeVertices++;
                }
            }
        }
        
        std::cout << "  Edge vertices: " << edgeVertices << "\n";
        std::cout << "  Shared edge vertices: " << sharedEdgeVertices << "\n";
        
        if (edgeVertices > 0) {
            float shareRatio = float(sharedEdgeVertices) / float(edgeVertices) * 100.0f;
            std::cout << "  Edge sharing ratio: " << std::fixed << std::setprecision(1) 
                      << shareRatio << "%\n";
            
            if (shareRatio < 50.0f) {
                std::cout << "  WARNING: Low edge sharing! Possible gap at boundary.\n";
            }
        }
    }
    
    // Check for cross-face boundaries specifically
    std::cout << "\n=== CROSS-FACE BOUNDARY CHECK ===\n";
    
    int crossFaceShared = 0;
    for (const auto& [key, patchList] : vertexToPatchMap) {
        if (patchList.size() > 1) {
            // Check if patches are on different faces
            bool differentFaces = false;
            int firstFace = patches[patchList[0]].faceId;
            for (size_t i = 1; i < patchList.size(); i++) {
                if (patches[patchList[i]].faceId != firstFace) {
                    differentFaces = true;
                    break;
                }
            }
            if (differentFaces) {
                crossFaceShared++;
            }
        }
    }
    
    std::cout << "Vertices shared across face boundaries: " << crossFaceShared << "\n";
    
    if (crossFaceShared == 0) {
        std::cout << "ERROR: No vertices are shared between faces! This explains the gaps.\n";
    } else {
        std::cout << "Good: Faces are sharing " << crossFaceShared << " vertices at boundaries.\n";
    }
    
    std::cout << "\n=== TEST COMPLETE ===\n";
}

int main() {
    try {
        analyzePatchBoundaries();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}