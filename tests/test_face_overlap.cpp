#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <iomanip>
#include <glm/glm.hpp>

// Test to detect and analyze overlapping geometry between cube faces

struct Vertex {
    glm::vec3 position;
    uint32_t faceId;
    uint32_t patchIndex;
};

// Helper to check if two positions are very close
bool arePositionsClose(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.001f) {
    return glm::length(a - b) < epsilon;
}

void generateFaceVertices(std::vector<Vertex>& vertices, uint32_t faceId, 
                         const glm::vec3& min, const glm::vec3& max, 
                         uint32_t resolution = 5) {
    // Generate a simple grid of vertices for this face
    for (uint32_t y = 0; y < resolution; y++) {
        for (uint32_t x = 0; x < resolution; x++) {
            float u = static_cast<float>(x) / (resolution - 1);
            float v = static_cast<float>(y) / (resolution - 1);
            
            Vertex vert;
            vert.position.x = min.x + (max.x - min.x) * u;
            vert.position.y = min.y + (max.y - min.y) * v;
            vert.position.z = min.z + (max.z - min.z) * ((u + v) * 0.5f); // Just for variation
            
            // For faces with one fixed dimension, fix it properly
            if (faceId == 0) vert.position.x = 1.0f;      // +X face
            else if (faceId == 1) vert.position.x = -1.0f; // -X face
            else if (faceId == 2) vert.position.y = 1.0f;  // +Y face
            else if (faceId == 3) vert.position.y = -1.0f; // -Y face
            else if (faceId == 4) vert.position.z = 1.0f;  // +Z face
            else if (faceId == 5) vert.position.z = -1.0f; // -Z face
            
            // Re-calculate the varying dimensions
            if (faceId == 0 || faceId == 1) { // X-faces
                vert.position.y = -1.0f + 2.0f * u;
                vert.position.z = -1.0f + 2.0f * v;
            } else if (faceId == 2 || faceId == 3) { // Y-faces
                vert.position.x = -1.0f + 2.0f * u;
                vert.position.z = -1.0f + 2.0f * v;
            } else { // Z-faces
                vert.position.x = -1.0f + 2.0f * u;
                vert.position.y = -1.0f + 2.0f * v;
            }
            
            vert.faceId = faceId;
            vert.patchIndex = y * resolution + x;
            vertices.push_back(vert);
        }
    }
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "    FACE OVERLAP DETECTION TEST\n";
    std::cout << "==========================================\n\n";
    
    // Test configurations
    const float ORIGINAL_EXTENT = 1.0f;
    const float INSET_EXTENT = 0.9999f;
    
    std::cout << "TEST 1: Original extents (±1.0)\n";
    std::cout << "--------------------------------\n";
    
    // Generate vertices for all 6 faces with original extents
    std::vector<Vertex> allVertices;
    for (uint32_t face = 0; face < 6; face++) {
        generateFaceVertices(allVertices, face, glm::vec3(-1), glm::vec3(1), 5);
    }
    
    // Check for overlaps
    int overlapCount = 0;
    std::vector<std::pair<Vertex, Vertex>> overlaps;
    
    for (size_t i = 0; i < allVertices.size(); i++) {
        for (size_t j = i + 1; j < allVertices.size(); j++) {
            if (allVertices[i].faceId != allVertices[j].faceId) {
                if (arePositionsClose(allVertices[i].position, allVertices[j].position, 0.01f)) {
                    overlapCount++;
                    if (overlaps.size() < 5) { // Just show first 5
                        overlaps.push_back({allVertices[i], allVertices[j]});
                    }
                }
            }
        }
    }
    
    std::cout << "Found " << overlapCount << " overlapping vertex pairs\n";
    for (const auto& pair : overlaps) {
        std::cout << "  Face " << pair.first.faceId << " vertex at ("
                  << pair.first.position.x << ", " 
                  << pair.first.position.y << ", "
                  << pair.first.position.z << ")\n";
        std::cout << "  Face " << pair.second.faceId << " vertex at ("
                  << pair.second.position.x << ", " 
                  << pair.second.position.y << ", "
                  << pair.second.position.z << ")\n";
        std::cout << "  Distance: " << glm::length(pair.first.position - pair.second.position) << "\n\n";
    }
    
    std::cout << "\nTEST 2: With inset (±" << INSET_EXTENT << ")\n";
    std::cout << "--------------------------------\n";
    
    // Clear and regenerate with inset
    allVertices.clear();
    overlaps.clear();
    
    // Generate vertices with inset for non-fixed dimensions
    for (uint32_t face = 0; face < 6; face++) {
        std::vector<Vertex> faceVerts;
        generateFaceVertices(faceVerts, face, glm::vec3(-1), glm::vec3(1), 5);
        
        // Apply inset to non-fixed dimensions
        for (auto& vert : faceVerts) {
            if (face == 0 || face == 1) { // X-faces - inset Y and Z
                vert.position.y *= INSET_EXTENT;
                vert.position.z *= INSET_EXTENT;
            } else if (face == 2 || face == 3) { // Y-faces - inset X and Z
                vert.position.x *= INSET_EXTENT;
                vert.position.z *= INSET_EXTENT;
            } else { // Z-faces - inset X and Y
                vert.position.x *= INSET_EXTENT;
                vert.position.y *= INSET_EXTENT;
            }
        }
        
        allVertices.insert(allVertices.end(), faceVerts.begin(), faceVerts.end());
    }
    
    // Check for overlaps again
    overlapCount = 0;
    for (size_t i = 0; i < allVertices.size(); i++) {
        for (size_t j = i + 1; j < allVertices.size(); j++) {
            if (allVertices[i].faceId != allVertices[j].faceId) {
                if (arePositionsClose(allVertices[i].position, allVertices[j].position, 0.01f)) {
                    overlapCount++;
                    if (overlaps.size() < 5) {
                        overlaps.push_back({allVertices[i], allVertices[j]});
                    }
                }
            }
        }
    }
    
    std::cout << "Found " << overlapCount << " overlapping vertex pairs\n";
    for (const auto& pair : overlaps) {
        std::cout << "  Face " << pair.first.faceId << " vertex at ("
                  << pair.first.position.x << ", " 
                  << pair.first.position.y << ", "
                  << pair.first.position.z << ")\n";
        std::cout << "  Face " << pair.second.faceId << " vertex at ("
                  << pair.second.position.x << ", " 
                  << pair.second.position.y << ", "
                  << pair.second.position.z << ")\n";
        std::cout << "  Distance: " << glm::length(pair.first.position - pair.second.position) << "\n\n";
    }
    
    std::cout << "\nTEST 3: Critical Edge Analysis\n";
    std::cout << "--------------------------------\n";
    
    // Specifically check the edges where faces meet
    struct Edge {
        const char* name;
        glm::vec3 start;
        glm::vec3 end;
        uint32_t face1;
        uint32_t face2;
    };
    
    Edge criticalEdges[] = {
        {"X+/Y+ edge", glm::vec3(1, 1, -1), glm::vec3(1, 1, 1), 0, 2},  // +X meets +Y
        {"X+/Z+ edge", glm::vec3(1, -1, 1), glm::vec3(1, 1, 1), 0, 4},  // +X meets +Z
        {"Y+/Z+ edge", glm::vec3(-1, 1, 1), glm::vec3(1, 1, 1), 2, 4},  // +Y meets +Z
    };
    
    for (const auto& edge : criticalEdges) {
        std::cout << edge.name << " (Face " << edge.face1 << " meets Face " << edge.face2 << ")\n";
        std::cout << "  From (" << edge.start.x << ", " << edge.start.y << ", " << edge.start.z << ")";
        std::cout << " to (" << edge.end.x << ", " << edge.end.y << ", " << edge.end.z << ")\n";
        
        // Check if vertices from both faces exist at this edge
        int face1Count = 0, face2Count = 0;
        for (const auto& vert : allVertices) {
            // Check if vertex is on this edge (within tolerance)
            glm::vec3 edgeDir = glm::normalize(edge.end - edge.start);
            glm::vec3 toVert = vert.position - edge.start;
            float t = glm::dot(toVert, edgeDir);
            
            if (t >= 0 && t <= glm::length(edge.end - edge.start)) {
                glm::vec3 closest = edge.start + edgeDir * t;
                float dist = glm::length(vert.position - closest);
                
                if (dist < 0.1f) { // On the edge
                    if (vert.faceId == edge.face1) face1Count++;
                    if (vert.faceId == edge.face2) face2Count++;
                }
            }
        }
        
        std::cout << "  Face " << edge.face1 << " has " << face1Count << " vertices on this edge\n";
        std::cout << "  Face " << edge.face2 << " has " << face2Count << " vertices on this edge\n";
        if (face1Count > 0 && face2Count > 0) {
            std::cout << "  ⚠ OVERLAP DETECTED: Both faces have vertices on same edge!\n";
        }
        std::cout << "\n";
    }
    
    std::cout << "\nCONCLUSION:\n";
    std::cout << "===========\n";
    if (overlapCount > 0) {
        std::cout << "✗ FAIL: Faces have overlapping vertices that will cause z-fighting\n";
        std::cout << "  The dots you see are from z-fighting between overlapping triangles\n";
        std::cout << "  Solution: Either increase inset or use different rendering approach\n";
    } else {
        std::cout << "✓ PASS: No overlapping vertices detected between faces\n";
    }
    
    return (overlapCount > 0) ? 1 : 0;
}