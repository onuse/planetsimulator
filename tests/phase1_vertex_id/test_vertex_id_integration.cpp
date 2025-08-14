// ============================================================================
// PHASE 1: Vertex Identity System Integration Test
// Hooks up the vertex ID system to one cube face for visual validation
// ============================================================================

#include <iostream>
#include <unordered_map>
#include <map>
#include <vector>
#include <fstream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "include/core/vertex_id_system.hpp"

using namespace PlanetRenderer;

const double PLANET_RADIUS = 6371000.0;

// ============================================================================
// Simple patch generation using vertex identity system
// ============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

class VertexCache {
public:
    std::unordered_map<VertexID, uint32_t> indexMap;
    std::vector<Vertex> vertices;
    
    uint32_t getOrCreateVertex(const VertexID& id, const glm::vec3& position, 
                               const glm::vec3& normal, const glm::vec3& color) {
        auto it = indexMap.find(id);
        if (it != indexMap.end()) {
            return it->second;
        }
        
        uint32_t index = vertices.size();
        vertices.push_back({position, normal, color});
        indexMap[id] = index;
        return index;
    }
    
    void printStats() {
        std::cout << "Vertex cache stats:\n";
        std::cout << "  Total vertices: " << vertices.size() << "\n";
        std::cout << "  Unique IDs: " << indexMap.size() << "\n";
        
        // Count boundary vertices
        int boundaryCount = 0;
        int edgeCount = 0;
        int cornerCount = 0;
        
        for (const auto& [id, index] : indexMap) {
            if (id.isOnCorner()) cornerCount++;
            else if (id.isOnEdge()) edgeCount++;
            else if (id.isOnFaceBoundary()) boundaryCount++;
        }
        
        std::cout << "  Corner vertices: " << cornerCount << "\n";
        std::cout << "  Edge vertices: " << edgeCount << "\n";
        std::cout << "  Face boundary vertices: " << boundaryCount << "\n";
    }
};

glm::vec3 cubeToSphere(const glm::vec3& cubePos) {
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos) * static_cast<float>(PLANET_RADIUS);
}

struct Patch {
    int face;
    glm::dvec2 center;
    double size;
    std::vector<uint32_t> indices;
};

Patch generatePatchWithVertexID(int face, glm::dvec2 center, double size, 
                                VertexCache& cache, int resolution = 16) {
    Patch patch;
    patch.face = face;
    patch.center = center;
    patch.size = size;
    
    std::vector<uint32_t> localIndices;
    
    // Generate vertices using vertex IDs
    for (int y = 0; y <= resolution; y++) {
        for (int x = 0; x <= resolution; x++) {
            double u = (center.x - size/2.0) + (x / double(resolution)) * size;
            double v = (center.y - size/2.0) + (y / double(resolution)) * size;
            
            // Clamp to valid UV range
            u = std::max(0.0, std::min(1.0, u));
            v = std::max(0.0, std::min(1.0, v));
            
            // Create vertex ID from face UV
            VertexID vid = VertexID::fromFaceUV(face, u, v);
            
            // Get the cube position for sphere mapping
            glm::dvec3 cubePos = vid.toCubePosition();
            glm::vec3 spherePos = cubeToSphere(cubePos);
            
            // Normal is just normalized position for a sphere
            glm::vec3 normal = glm::normalize(spherePos);
            
            // Color based on face for visualization
            glm::vec3 color;
            switch(face) {
                case 0: color = glm::vec3(1.0f, 0.5f, 0.5f); break; // +X red
                case 1: color = glm::vec3(0.5f, 0.0f, 0.0f); break; // -X dark red
                case 2: color = glm::vec3(0.5f, 1.0f, 0.5f); break; // +Y green
                case 3: color = glm::vec3(0.0f, 0.5f, 0.0f); break; // -Y dark green
                case 4: color = glm::vec3(0.5f, 0.5f, 1.0f); break; // +Z blue
                case 5: color = glm::vec3(0.0f, 0.0f, 0.5f); break; // -Z dark blue
            }
            
            // Highlight boundaries for debugging
            if (vid.isOnCorner()) {
                color = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow for corners
            } else if (vid.isOnEdge()) {
                color = glm::vec3(1.0f, 0.5f, 0.0f); // Orange for edges
            } else if (vid.isOnFaceBoundary()) {
                color = glm::vec3(0.8f, 0.8f, 0.8f); // Light gray for boundaries
            }
            
            uint32_t index = cache.getOrCreateVertex(vid, spherePos, normal, color);
            localIndices.push_back(index);
        }
    }
    
    // Generate triangle indices
    for (int y = 0; y < resolution; y++) {
        for (int x = 0; x < resolution; x++) {
            int idx = y * (resolution + 1) + x;
            
            // Two triangles per quad
            patch.indices.push_back(localIndices[idx]);
            patch.indices.push_back(localIndices[idx + 1]);
            patch.indices.push_back(localIndices[idx + resolution + 1]);
            
            patch.indices.push_back(localIndices[idx + 1]);
            patch.indices.push_back(localIndices[idx + resolution + 2]);
            patch.indices.push_back(localIndices[idx + resolution + 1]);
        }
    }
    
    return patch;
}

void exportToOBJ(const VertexCache& cache, const std::vector<Patch>& patches, 
                 const std::string& filename) {
    std::ofstream file(filename);
    
    file << "# Vertex Identity System Integration Test\n";
    file << "# Vertices: " << cache.vertices.size() << "\n";
    file << "# Patches: " << patches.size() << "\n\n";
    
    // Export vertices
    for (const auto& v : cache.vertices) {
        file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
    }
    
    // Export vertex colors
    for (const auto& v : cache.vertices) {
        file << "vc " << v.color.x << " " << v.color.y << " " << v.color.z << "\n";
    }
    
    // Export normals
    for (const auto& v : cache.vertices) {
        file << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
    }
    
    // Export faces (OBJ uses 1-based indexing)
    for (const auto& patch : patches) {
        file << "# Patch face=" << patch.face << " center=(" 
             << patch.center.x << "," << patch.center.y << ")\n";
        
        for (size_t i = 0; i < patch.indices.size(); i += 3) {
            file << "f ";
            for (int j = 0; j < 3; j++) {
                uint32_t idx = patch.indices[i + j] + 1;
                file << idx << "//" << idx << " ";
            }
            file << "\n";
        }
    }
    
    file.close();
    std::cout << "Exported mesh to " << filename << "\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "VERTEX ID SYSTEM INTEGRATION TEST\n";
    std::cout << "========================================\n\n";
    
    // Create shared vertex cache
    VertexCache cache;
    std::vector<Patch> patches;
    
    // Test 1: Single face with multiple patches
    std::cout << "Test 1: Generate 4 patches on +Z face\n";
    std::cout << "----------------------------------------\n";
    
    patches.push_back(generatePatchWithVertexID(4, glm::dvec2(0.25, 0.25), 0.5, cache));
    patches.push_back(generatePatchWithVertexID(4, glm::dvec2(0.75, 0.25), 0.5, cache));
    patches.push_back(generatePatchWithVertexID(4, glm::dvec2(0.25, 0.75), 0.5, cache));
    patches.push_back(generatePatchWithVertexID(4, glm::dvec2(0.75, 0.75), 0.5, cache));
    
    cache.printStats();
    
    // Test 2: Add patches from adjacent face to test boundary sharing
    std::cout << "\nTest 2: Add patches from +X face (adjacent)\n";
    std::cout << "----------------------------------------\n";
    
    patches.push_back(generatePatchWithVertexID(0, glm::dvec2(0.75, 0.25), 0.5, cache));
    patches.push_back(generatePatchWithVertexID(0, glm::dvec2(0.75, 0.75), 0.5, cache));
    
    cache.printStats();
    
    // Test 3: Add corner patch from +Y face
    std::cout << "\nTest 3: Add corner patch from +Y face\n";
    std::cout << "----------------------------------------\n";
    
    patches.push_back(generatePatchWithVertexID(2, glm::dvec2(0.75, 0.75), 0.5, cache));
    
    cache.printStats();
    
    // Analyze vertex sharing
    std::cout << "\n========================================\n";
    std::cout << "VERTEX SHARING ANALYSIS\n";
    std::cout << "========================================\n";
    
    // Count shared vertices between patches
    std::map<uint32_t, int> vertexUsage;
    for (const auto& patch : patches) {
        for (uint32_t idx : patch.indices) {
            vertexUsage[idx]++;
        }
    }
    
    int sharedCount = 0;
    for (const auto& [idx, count] : vertexUsage) {
        if (count > 3) sharedCount++; // Used by more than one triangle
    }
    
    std::cout << "Vertices used by multiple patches: " << sharedCount << "\n";
    
    // Calculate memory savings
    int totalVerticesWithoutSharing = patches.size() * 17 * 17; // resolution+1 squared
    int actualVertices = cache.vertices.size();
    float savings = 100.0f * (1.0f - float(actualVertices) / float(totalVerticesWithoutSharing));
    
    std::cout << "Without vertex sharing: " << totalVerticesWithoutSharing << " vertices\n";
    std::cout << "With vertex sharing: " << actualVertices << " vertices\n";
    std::cout << "Memory savings: " << std::fixed << std::setprecision(1) << savings << "%\n";
    
    // Export to OBJ for visual inspection
    exportToOBJ(cache, patches, "vertex_id_test.obj");
    
    std::cout << "\n========================================\n";
    std::cout << "TEST COMPLETE\n";
    std::cout << "View vertex_id_test.obj in a 3D viewer\n";
    std::cout << "to verify vertex sharing at boundaries\n";
    std::cout << "========================================\n";
    
    return 0;
}