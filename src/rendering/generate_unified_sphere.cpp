// Generate UNIFIED sphere mesh using recursive icosphere subdivision
// This creates a single continuous mesh with no seams

#include "rendering/vulkan_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <cmath>

namespace rendering {

// Helper to normalize and scale vector to sphere radius
static glm::vec3 projectToSphere(const glm::vec3& v, float radius) {
    return glm::normalize(v) * radius;
}

// Hash for edge (pair of vertex indices)
struct EdgeHash {
    std::size_t operator()(const std::pair<uint32_t, uint32_t>& edge) const {
        return std::hash<uint32_t>()(edge.first) ^ (std::hash<uint32_t>()(edge.second) << 1);
    }
};

bool VulkanRenderer::generateUnifiedSphere(octree::OctreePlanet* planet, core::Camera* camera) {
    std::cout << "\n=== Generating UNIFIED Sphere Mesh with LOD Support ===\n";
    
    if (!planet) {
        std::cerr << "ERROR: No planet provided!\n";
        return false;
    }
    
    float planetRadius = planet->getRadius();
    
    // Use the LOD level that was already calculated in render()
    int subdivisions = currentLODLevel;
    
    if (subdivisions <= 0) {
        // Fallback: calculate if not set
        subdivisions = 5; // Default level
        if (camera) {
            glm::vec3 cameraPos = camera->getPosition();
            float distanceToCenter = glm::length(cameraPos);
            float distanceToSurface = distanceToCenter - planetRadius;
            
            // LOD selection based on distance from surface
            if (distanceToSurface > planetRadius * 10.0f) {
                subdivisions = 2; // Very far: minimal detail (80 triangles)
            } else if (distanceToSurface > planetRadius * 5.0f) {
                subdivisions = 3; // Far: low detail (320 triangles)
            } else if (distanceToSurface > planetRadius * 2.0f) {
                subdivisions = 4; // Medium distance (1,280 triangles)
            } else if (distanceToSurface > planetRadius * 0.5f) {
                subdivisions = 5; // Close: high detail (5,120 triangles)
            } else {
                subdivisions = 6; // Very close: maximum detail (20,480 triangles)
            }
        }
    }
    
    // Raise cap to 10 for better detail when close (20M triangles max)
    // Modern GPUs can handle this, especially with frustum culling
    if (subdivisions > 10) {
        std::cout << "Capping LOD at level 10 (requested " << subdivisions << ")\n";
        subdivisions = 10;
    }
    
    std::cout << "Using LOD level: " << subdivisions << " subdivisions (" 
              << (20 * static_cast<int>(pow(4, subdivisions))) << " triangles)\n";
    
    // Start with an icosahedron (20-sided polyhedron) - perfect for sphere approximation
    // It has 12 vertices and 20 triangular faces
    const float phi = (1.0f + sqrt(5.0f)) / 2.0f; // Golden ratio
    const float a = 1.0f;
    const float b = 1.0f / phi;
    
    // Initial icosahedron vertices (normalized later)
    std::vector<glm::vec3> vertices = {
        // Rectangle in XY plane
        glm::vec3(-b, a, 0), glm::vec3(b, a, 0), glm::vec3(-b, -a, 0), glm::vec3(b, -a, 0),
        // Rectangle in YZ plane  
        glm::vec3(0, -b, a), glm::vec3(0, b, a), glm::vec3(0, -b, -a), glm::vec3(0, b, -a),
        // Rectangle in XZ plane
        glm::vec3(a, 0, -b), glm::vec3(a, 0, b), glm::vec3(-a, 0, -b), glm::vec3(-a, 0, b)
    };
    
    // Project vertices to sphere surface
    for (auto& v : vertices) {
        v = projectToSphere(v, planetRadius);
    }
    
    // Initial icosahedron faces (20 triangles)
    std::vector<uint32_t> indices = {
        // 5 faces around vertex 0
        0, 11, 5,   0, 5, 1,   0, 1, 7,   0, 7, 10,   0, 10, 11,
        // 5 adjacent faces
        1, 5, 9,   5, 11, 4,   11, 10, 2,   10, 7, 6,   7, 1, 8,
        // 5 faces around vertex 3
        3, 9, 4,   3, 4, 2,   3, 2, 6,   3, 6, 8,   3, 8, 9,
        // 5 adjacent faces
        4, 9, 5,   2, 4, 11,   6, 2, 10,   8, 6, 7,   9, 8, 1
    };
    
    // RECURSIVE SUBDIVISION for smooth sphere
    // Each subdivision splits each triangle into 4 smaller triangles
    // int subdivisions = 5; // 5 subdivisions = 20 * 4^5 = 20,480 triangles
    // Subdivision levels: 2=80, 3=320, 4=1280, 5=5120, 6=20480 triangles
    
    std::cout << "Subdividing icosahedron " << subdivisions << " times...\n";
    
    for (int level = 0; level < subdivisions; level++) {
        std::vector<uint32_t> newIndices;
        std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, EdgeHash> midpointCache;
        
        // Process each triangle
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t v0 = indices[i];
            uint32_t v1 = indices[i + 1];
            uint32_t v2 = indices[i + 2];
            
            // Get or create midpoint vertices
            auto getMidpoint = [&](uint32_t a, uint32_t b) -> uint32_t {
                // Ensure consistent edge ordering
                if (a > b) std::swap(a, b);
                auto edge = std::make_pair(a, b);
                
                // Check cache
                auto it = midpointCache.find(edge);
                if (it != midpointCache.end()) {
                    return it->second;
                }
                
                // Create new midpoint vertex
                glm::vec3 midpoint = (vertices[a] + vertices[b]) * 0.5f;
                midpoint = projectToSphere(midpoint, planetRadius); // Project to sphere
                
                uint32_t midIndex = static_cast<uint32_t>(vertices.size());
                vertices.push_back(midpoint);
                midpointCache[edge] = midIndex;
                
                return midIndex;
            };
            
            uint32_t m01 = getMidpoint(v0, v1);
            uint32_t m12 = getMidpoint(v1, v2);
            uint32_t m20 = getMidpoint(v2, v0);
            
            // Create 4 new triangles
            //     v0
            //    / \
            //   m01-m20
            //  / \ / \
            // v1--m12--v2
            
            newIndices.push_back(v0); newIndices.push_back(m01); newIndices.push_back(m20);
            newIndices.push_back(v1); newIndices.push_back(m12); newIndices.push_back(m01);
            newIndices.push_back(v2); newIndices.push_back(m20); newIndices.push_back(m12);
            newIndices.push_back(m01); newIndices.push_back(m12); newIndices.push_back(m20);
        }
        
        indices = std::move(newIndices);
        std::cout << "  Level " << (level + 1) << ": " 
                  << vertices.size() << " vertices, " 
                  << indices.size()/3 << " triangles\n";
    }
    
    // Now sample the planet's octree voxel data and apply terrain
    // Higher LOD = sample deeper into octree for more detail
    int octreeSampleDepth = std::min(subdivisions + 2, planet->getMaxDepth());
    std::cout << "Sampling octree voxel data at depth " << octreeSampleDepth << " for terrain...\n";
    
    std::vector<algorithms::MeshVertex> finalVertices;
    finalVertices.reserve(vertices.size());
    
    // Debug statistics
    std::map<int, int> materialCounts;
    int surfaceFound = 0;
    
    for (const auto& pos : vertices) {
        algorithms::MeshVertex vertex;
        glm::vec3 normal = glm::normalize(pos);
        
        // Raycast sampling: Find the actual voxel surface
        const octree::Voxel* voxel = nullptr;
        
        // Sample multiple positions from outside to inside
        for (float r = 1.05f; r >= 0.95f; r -= 0.01f) {
            glm::vec3 samplePos = normal * (planetRadius * r);
            const octree::Voxel* test = planet->getVoxel(samplePos);
            
            if (test) {
                auto matId = test->getDominantMaterialID();
                // Found non-air voxel? Use it!
                if (matId != core::MaterialID::Air && matId != core::MaterialID::Vacuum) {
                    voxel = test;
                    surfaceFound++;
                    break;
                }
            }
        }
        
        // Fallback to original position
        if (!voxel) {
            voxel = planet->getVoxel(pos);
        }
        
        float displacement = 0.0f;
        glm::vec3 color(0.5f, 0.5f, 0.5f); // Default gray
        
        // Scale displacement based on LOD - more detail when closer
        float detailScale = 1.0f + (subdivisions - 2) * 0.2f; // Higher LOD = more terrain variation
        
        if (voxel) {
            // Get dominant material from MixedVoxel
            auto materialID = voxel->getDominantMaterialID();
            
            // Track material counts for debugging
            materialCounts[static_cast<int>(materialID)]++;
            
            // Map material to terrain height and color with LOD-scaled detail
            switch(materialID) {
                case core::MaterialID::Water:
                    displacement = -200.0f * detailScale; // Ocean depth ~200-600m
                    color = glm::vec3(0.1f, 0.3f, 0.6f);  // Deep blue
                    break;
                case core::MaterialID::Sand:
                    displacement = 10.0f * detailScale;  // Beach level ~10-30m
                    color = glm::vec3(0.9f, 0.85f, 0.65f); // Sandy
                    break;
                case core::MaterialID::Grass:
                    displacement = 50.0f * detailScale;  // Plains ~50-150m
                    color = glm::vec3(0.2f, 0.6f, 0.2f);   // Green
                    break;
                case core::MaterialID::Rock:
                    displacement = 200.0f * detailScale;  // Hills ~200-600m
                    color = glm::vec3(0.4f, 0.3f, 0.2f);   // Brown
                    break;
                case core::MaterialID::Snow:
                    displacement = 500.0f * detailScale;  // Mountains ~500-1500m
                    color = glm::vec3(0.95f, 0.95f, 0.98f); // White
                    break;
                case core::MaterialID::Lava:
                    displacement = 30.0f * detailScale;  // Volcanic ~30-90m
                    color = glm::vec3(0.8f, 0.2f, 0.0f);   // Red-orange
                    break;
                default:
                    // Air/Vacuum or unknown - use base sphere radius
                    displacement = 0.0f;
                    color = glm::vec3(0.7f, 0.7f, 0.8f);   // Light gray
                    break;
            }
            
            // DISABLED mixing for now - just use dominant material
            // The mixing code was broken anyway (using same color for all materials)
        } else {
            // No voxel data - use distance from center for basic sphere
            float distFromCenter = glm::length(pos);
            if (distFromCenter < planetRadius * 0.98f) {
                // Below sea level
                displacement = -200.0f;
                color = glm::vec3(0.2f, 0.4f, 0.7f); // Ocean
            } else {
                // Above sea level - simple height-based coloring
                displacement = (distFromCenter - planetRadius) * 0.1f;
                color = glm::vec3(0.3f, 0.6f, 0.3f); // Land
            }
        }
        
        // Apply displacement along normal
        vertex.position = pos + normal * displacement;
        vertex.normal = normal; // Will be recalculated based on actual surface
        vertex.color = color;
        
        finalVertices.push_back(vertex);
    }
    
    // Print material statistics
    std::cout << "\nMaterial distribution found by raycasting:\n";
    std::cout << "  Surface vertices hit by raycast: " << surfaceFound << "/" << vertices.size() 
              << " (" << (100.0f * surfaceFound / vertices.size()) << "%)\n";
    for (const auto& [materialId, count] : materialCounts) {
        std::cout << "  Material " << materialId << ": " << count << " vertices\n";
    }
    std::cout << "\n";
    
    // Recalculate normals based on actual triangle geometry
    std::cout << "Calculating smooth normals...\n";
    std::vector<glm::vec3> vertexNormals(finalVertices.size(), glm::vec3(0));
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        
        glm::vec3 v0 = finalVertices[i0].position;
        glm::vec3 v1 = finalVertices[i1].position;
        glm::vec3 v2 = finalVertices[i2].position;
        
        glm::vec3 faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        
        vertexNormals[i0] += faceNormal;
        vertexNormals[i1] += faceNormal;
        vertexNormals[i2] += faceNormal;
    }
    
    for (size_t i = 0; i < finalVertices.size(); i++) {
        finalVertices[i].normal = glm::normalize(vertexNormals[i]);
    }
    
    uint32_t totalVertices = static_cast<uint32_t>(finalVertices.size());
    uint32_t totalIndices = static_cast<uint32_t>(indices.size());
    
    std::cout << "\n=== UNIFIED SPHERE MESH COMPLETE ===\n";
    std::cout << "Total vertices: " << totalVertices << "\n";
    std::cout << "Total triangles: " << totalIndices/3 << "\n";
    std::cout << "COMPLETELY SEAMLESS - Single continuous mesh!\n";
    std::cout << "====================================\n";
    
    // Convert to flat array for GPU upload
    std::vector<float> vertexData;
    vertexData.reserve(totalVertices * 11);
    
    for (const auto& vertex : finalVertices) {
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
        
        // Texture coordinates (2 floats) - could calculate spherical UVs here
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
    }
    
    // Upload to GPU
    size_t vertexDataSize = vertexData.size() * sizeof(float);
    size_t indexDataSize = indices.size() * sizeof(uint32_t);
    
    bool success = uploadCPUReferenceMesh(
        vertexData.data(), vertexDataSize,
        indices.data(), indexDataSize,
        totalVertices, totalIndices
    );
    
    if (success) {
        std::cout << "Unified sphere mesh successfully uploaded to GPU!\n";
    }
    
    return success;
}

} // namespace rendering