// Adaptive sphere mesh generation with dual-detail LOD
// Phase 1: Simple dual-detail (front/back hemisphere)

#include "rendering/vulkan_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <cmath>

namespace rendering {

// Define the static member
bool VulkanRenderer::adaptiveSphereFlipFrontBack = false;

// Helper to normalize and scale vector to sphere radius
static glm::vec3 projectToSphere(const glm::vec3& v, float radius) {
    return glm::normalize(v) * radius;
}

// Hash for vertex position (for deduplication)
struct Vec3Hash {
    std::size_t operator()(const glm::vec3& v) const {
        // Simple hash combining all three components
        std::size_t h1 = std::hash<float>()(v.x);
        std::size_t h2 = std::hash<float>()(v.y);
        std::size_t h3 = std::hash<float>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// Vertex equality with epsilon for floating point comparison
struct Vec3Equal {
    bool operator()(const glm::vec3& a, const glm::vec3& b) const {
        const float epsilon = 0.0001f;
        return glm::length(a - b) < epsilon;
    }
};

// Generate adaptive sphere with dual detail levels
bool VulkanRenderer::generateAdaptiveSphere(octree::OctreePlanet* planet, core::Camera* camera) {
    std::cout << "\n=== Generating DUAL-DETAIL Adaptive Sphere Mesh ===\n";
    if (adaptiveSphereFlipFrontBack) {
        std::cout << "[FLIPPED MODE] Back faces get high detail, front faces get low detail\n";
    }
    
    if (!planet) {
        std::cerr << "ERROR: No planet provided!\n";
        return false;
    }
    
    if (!camera) {
        std::cerr << "WARNING: No camera provided, using uniform detail\n";
    }
    
    float planetRadius = planet->getRadius();
    glm::vec3 cameraPos = camera ? camera->getPosition() : glm::vec3(0, 0, planetRadius * 3);
    glm::vec3 viewDir = glm::normalize(glm::vec3(0, 0, 0) - cameraPos); // Looking at planet center
    
    // Calculate base LOD from distance
    float distanceToCenter = glm::length(cameraPos);
    float distanceToSurface = distanceToCenter - planetRadius;
    
    // Dual-detail LOD levels with MORE aggressive difference
    int highDetailLevel = 5;  // Front hemisphere
    int lowDetailLevel = 1;   // Back hemisphere - MUCH lower detail for testing
    
    // Adjust high detail based on distance
    if (distanceToSurface > planetRadius * 10.0f) {
        highDetailLevel = 4;  // Far away - but still higher than back
        lowDetailLevel = 1;   // Keep back very low
    } else if (distanceToSurface > planetRadius * 5.0f) {
        highDetailLevel = 5;
        lowDetailLevel = 1;
    } else if (distanceToSurface > planetRadius * 2.0f) {
        highDetailLevel = 6;
        lowDetailLevel = 2;
    } else if (distanceToSurface > planetRadius * 0.5f) {
        highDetailLevel = 7;
        lowDetailLevel = 2;
    } else if (distanceToSurface > planetRadius * 0.1f) {
        highDetailLevel = 8;
        lowDetailLevel = 3;
    } else {
        highDetailLevel = 9;  // Very close - maximum detail
        lowDetailLevel = 3;
    }
    
    // Cap at reasonable level
    if (highDetailLevel > 9) highDetailLevel = 9;
    
    std::cout << "Camera distance: " << distanceToSurface / planetRadius << "x radius\n";
    std::cout << "Dual-Detail LOD: Front=" << highDetailLevel 
              << " (~" << (20 * (int)pow(4, highDetailLevel)) << " tris)"
              << ", Back=" << lowDetailLevel 
              << " (~" << (20 * (int)pow(4, lowDetailLevel)) << " tris)\n";
    
    // Vertex deduplication map
    std::unordered_map<glm::vec3, uint32_t, Vec3Hash, Vec3Equal> vertexMap;
    std::vector<algorithms::MeshVertex> vertices;
    std::vector<uint32_t> indices;
    
    // Helper to get or create vertex
    auto getOrCreateVertex = [&](const glm::vec3& pos) -> uint32_t {
        auto it = vertexMap.find(pos);
        if (it != vertexMap.end()) {
            return it->second;
        }
        
        uint32_t idx = static_cast<uint32_t>(vertices.size());
        algorithms::MeshVertex vertex;
        vertex.position = pos;
        vertex.normal = glm::normalize(pos);  // Will be recalculated later
        vertex.color = glm::vec3(0.5f, 0.5f, 0.5f);  // Will be set from voxels
        vertices.push_back(vertex);
        vertexMap[pos] = idx;
        return idx;
    };
    
    // Recursive subdivision function
    std::function<void(glm::vec3, glm::vec3, glm::vec3, int)> subdivideTriangle;
    subdivideTriangle = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, int depth) {
        if (depth <= 0) {
            // Output triangle
            indices.push_back(getOrCreateVertex(v0));
            indices.push_back(getOrCreateVertex(v1));
            indices.push_back(getOrCreateVertex(v2));
            return;
        }
        
        // Create midpoints and project to sphere
        glm::vec3 m01 = projectToSphere((v0 + v1) * 0.5f, planetRadius);
        glm::vec3 m12 = projectToSphere((v1 + v2) * 0.5f, planetRadius);
        glm::vec3 m20 = projectToSphere((v2 + v0) * 0.5f, planetRadius);
        
        // Subdivide into 4 triangles
        subdivideTriangle(v0, m01, m20, depth - 1);
        subdivideTriangle(v1, m12, m01, depth - 1);
        subdivideTriangle(v2, m20, m12, depth - 1);
        subdivideTriangle(m01, m12, m20, depth - 1);
    };
    
    // Create icosahedron base
    const float phi = (1.0f + sqrt(5.0f)) / 2.0f;
    const float a = 1.0f;
    const float b = 1.0f / phi;
    
    // Initial vertices (normalized to sphere)
    std::vector<glm::vec3> icoVertices = {
        projectToSphere(glm::vec3(-b, a, 0), planetRadius),
        projectToSphere(glm::vec3(b, a, 0), planetRadius),
        projectToSphere(glm::vec3(-b, -a, 0), planetRadius),
        projectToSphere(glm::vec3(b, -a, 0), planetRadius),
        projectToSphere(glm::vec3(0, -b, a), planetRadius),
        projectToSphere(glm::vec3(0, b, a), planetRadius),
        projectToSphere(glm::vec3(0, -b, -a), planetRadius),
        projectToSphere(glm::vec3(0, b, -a), planetRadius),
        projectToSphere(glm::vec3(a, 0, -b), planetRadius),
        projectToSphere(glm::vec3(a, 0, b), planetRadius),
        projectToSphere(glm::vec3(-a, 0, -b), planetRadius),
        projectToSphere(glm::vec3(-a, 0, b), planetRadius)
    };
    
    // Icosahedron face indices
    int icoFaces[20][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };
    
    // Process each face with adaptive detail
    int frontFaces = 0, backFaces = 0;
    for (int i = 0; i < 20; i++) {
        glm::vec3 v0 = icoVertices[icoFaces[i][0]];
        glm::vec3 v1 = icoVertices[icoFaces[i][1]];
        glm::vec3 v2 = icoVertices[icoFaces[i][2]];
        
        // Calculate face center and normal
        glm::vec3 faceCenter = (v0 + v1 + v2) / 3.0f;
        glm::vec3 faceNormal = glm::normalize(faceCenter);
        
        // Determine if face is front-facing (towards camera)
        float facingDot = glm::dot(faceNormal, -viewDir);
        bool isFrontFacing = facingDot > 0.0f;  // Strictly front-facing only
        
        // Apply flip if enabled
        if (adaptiveSphereFlipFrontBack) {
            isFrontFacing = !isFrontFacing;  // Flip the decision
        }
        
        // Choose subdivision level based on facing
        int subdivLevel = isFrontFacing ? highDetailLevel : lowDetailLevel;
        
        if (isFrontFacing) frontFaces++;
        else backFaces++;
        
        // Subdivide the face
        subdivideTriangle(v0, v1, v2, subdivLevel);
    }
    
    // Calculate actual triangle counts for each hemisphere
    int frontTriangles = frontFaces * (int)pow(4, highDetailLevel);
    int backTriangles = backFaces * (int)pow(4, lowDetailLevel);
    float detailRatio = (float)frontTriangles / (float)backTriangles;
    
    std::cout << "Face distribution: " << frontFaces << " front (high detail), " 
              << backFaces << " back (low detail)\n";
    std::cout << "Triangle distribution:\n";
    std::cout << "  Front hemisphere: ~" << frontTriangles << " triangles (LOD " << highDetailLevel << ")\n";
    std::cout << "  Back hemisphere: ~" << backTriangles << " triangles (LOD " << lowDetailLevel << ")\n";
    std::cout << "  Detail ratio: " << detailRatio << ":1 (front has " << detailRatio << "x more detail)\n";
    std::cout << "Generated " << vertices.size() << " unique vertices, " 
              << indices.size() / 3 << " triangles\n";
    
    // Now sample terrain and materials from octree
    std::cout << "Sampling terrain from octree...\n";
    
    std::map<int, int> materialCounts;
    for (auto& vertex : vertices) {
        glm::vec3 normal = glm::normalize(vertex.position);
        
        // Sample voxel at this position
        const octree::Voxel* voxel = planet->getVoxel(vertex.position);
        
        float displacement = 0.0f;
        glm::vec3 color(0.5f, 0.5f, 0.5f);
        
        if (voxel) {
            auto materialID = voxel->getDominantMaterialID();
            materialCounts[static_cast<int>(materialID)]++;
            
            // Map material to height and color
            switch(materialID) {
                case core::MaterialID::Water:
                    displacement = -100.0f;
                    color = glm::vec3(0.1f, 0.3f, 0.6f);
                    break;
                case core::MaterialID::Sand:
                    displacement = 10.0f;
                    color = glm::vec3(0.9f, 0.85f, 0.65f);
                    break;
                case core::MaterialID::Grass:
                    displacement = 50.0f;
                    color = glm::vec3(0.2f, 0.6f, 0.2f);
                    break;
                case core::MaterialID::Rock:
                    displacement = 150.0f;
                    color = glm::vec3(0.4f, 0.3f, 0.2f);
                    break;
                case core::MaterialID::Snow:
                    displacement = 300.0f;
                    color = glm::vec3(0.95f, 0.95f, 0.98f);
                    break;
                default:
                    displacement = 0.0f;
                    color = glm::vec3(0.7f, 0.7f, 0.8f);
                    break;
            }
        }
        
        // Apply displacement
        vertex.position = vertex.position + normal * displacement;
        vertex.color = color;
    }
    
    // Print material distribution
    std::cout << "Material distribution:\n";
    for (const auto& [matId, count] : materialCounts) {
        std::cout << "  Material " << matId << ": " << count << " vertices\n";
    }
    
    // Recalculate normals from actual geometry
    std::cout << "Calculating smooth normals...\n";
    std::vector<glm::vec3> vertexNormals(vertices.size(), glm::vec3(0));
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        
        glm::vec3 v0 = vertices[i0].position;
        glm::vec3 v1 = vertices[i1].position;
        glm::vec3 v2 = vertices[i2].position;
        
        glm::vec3 faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        
        vertexNormals[i0] += faceNormal;
        vertexNormals[i1] += faceNormal;
        vertexNormals[i2] += faceNormal;
    }
    
    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].normal = glm::normalize(vertexNormals[i]);
    }
    
    // Convert to GPU format
    std::vector<float> vertexData;
    vertexData.reserve(vertices.size() * 11);
    
    for (const auto& vertex : vertices) {
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
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
    }
    
    // Upload to GPU
    size_t vertexDataSize = vertexData.size() * sizeof(float);
    size_t indexDataSize = indices.size() * sizeof(uint32_t);
    
    std::cout << "\n=== DUAL-DETAIL MESH COMPLETE ===\n";
    std::cout << "Total vertices: " << vertices.size() << "\n";
    std::cout << "Total triangles: " << indices.size() / 3 << "\n";
    std::cout << "Front hemisphere: ~" << (frontFaces * (int)pow(4, highDetailLevel)) << " triangles\n";
    std::cout << "Back hemisphere: ~" << (backFaces * (int)pow(4, lowDetailLevel)) << " triangles\n";
    std::cout << "Triangle ratio: " << ((float)pow(4, highDetailLevel - lowDetailLevel)) << ":1\n";
    std::cout << "=================================\n";
    
    bool success = uploadCPUReferenceMesh(
        vertexData.data(), vertexDataSize,
        indices.data(), indexDataSize,
        static_cast<uint32_t>(vertices.size()),
        static_cast<uint32_t>(indices.size())
    );
    
    if (success) {
        std::cout << "Dual-detail mesh successfully uploaded to GPU!\n";
    }
    
    return success;
}

} // namespace rendering