// Adaptive sphere mesh generation with dual-detail LOD
// Phase 1: Simple dual-detail (front/back hemisphere)

#include "rendering/vulkan_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <cmath>
#include <limits>
#include <algorithm>
#include <chrono>

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
    
    // Displace the sphere by the terrain field.
    //
    // This used to pick displacement from a switch on the voxel's material,
    // which gave the whole planet exactly six possible elevations - the
    // terraced contour rings that made continents look like a topographic map.
    // Sampling the height field directly gives continuous terrain, and it is
    // the same field the octree voxels were filled from.
    const auto displaceStart = std::chrono::steady_clock::now();

    const core::DensityField& field = planet->getDensityField();
    const simulation::CrustGrid* crust = field.getCrustGrid();
    const simulation::CrustGrid::Snapshot* snapshot = field.getCrustSnapshot();
    const bool diagnosticView = surfaceView != SurfaceView::Terrain &&
                                crust != nullptr && snapshot != nullptr;

    // Distinct hues so neighbouring plates never read as the same one.
    const auto plateColour = [](uint16_t id) {
        const float hue = std::fmod(static_cast<float>(id) * 0.618034f, 1.0f) * 6.0f;
        const int sector = static_cast<int>(hue);
        const float f = hue - sector;
        switch (sector) {
            case 0:  return glm::vec3(1.0f, f, 0.0f);
            case 1:  return glm::vec3(1.0f - f, 1.0f, 0.0f);
            case 2:  return glm::vec3(0.0f, 1.0f, f);
            case 3:  return glm::vec3(0.0f, 1.0f - f, 1.0f);
            case 4:  return glm::vec3(f, 0.0f, 1.0f);
            default: return glm::vec3(1.0f, 0.0f, 1.0f - f);
        }
    };

    const float seaLevelHeight = field.getSeaLevelHeight();
    const float maxElevation = std::max(field.getMaxElevation(), 1e-6f);
    const float maxOceanDepth = std::max(field.getMaxOceanDepth(), 1e-6f);

    std::map<int, int> materialCounts;
    float minHeight = std::numeric_limits<float>::max();
    float maxHeight = std::numeric_limits<float>::lowest();

    for (auto& vertex : vertices) {
        const glm::vec3 normal = glm::normalize(vertex.position);
        const float terrainHeight = field.getTerrainHeight(normal);

        minHeight = std::min(minHeight, terrainHeight);
        maxHeight = std::max(maxHeight, terrainHeight);

        // Oceans render as a flat water surface at sea level; the sea floor
        // below is real geometry but is not what the camera sees from orbit.
        const bool submerged = terrainHeight < seaLevelHeight;
        const float surfaceHeight = submerged ? seaLevelHeight : terrainHeight;
        vertex.position = normal * (planetRadius + surfaceHeight);

        // Colour follows the same field, so shading cannot disagree with shape.
        const float elevation = terrainHeight - seaLevelHeight;

        // Diagnostic views colour by what the simulation holds rather than by
        // what the surface would look like, and skip the terrain palette
        // entirely.
        if (diagnosticView) {
            const int cell = crust->findNearestCell(normal);
            glm::vec3 colour(0.5f);
            if (cell >= 0) {
                switch (surfaceView) {
                    case SurfaceView::Plates:
                        colour = plateColour(snapshot->plateId[cell]);
                        // Darken the sea so plate outlines still read against it.
                        if (submerged) colour *= 0.45f;
                        break;
                    case SurfaceView::CrustAge: {
                        const float t = glm::clamp(snapshot->crustAge[cell] / 200.0f, 0.0f, 1.0f);
                        colour = glm::mix(glm::vec3(1.0f, 0.25f, 0.1f),   // young, at ridges
                                          glm::vec3(0.1f, 0.15f, 0.5f),   // old, at trenches
                                          t);
                        break;
                    }
                    case SurfaceView::RockType:
                        switch (static_cast<simulation::CrustGrid::RockType>(snapshot->surfaceRock[cell])) {
                            case simulation::CrustGrid::RockType::Basalt:
                                colour = glm::vec3(0.25f, 0.25f, 0.30f); break;
                            case simulation::CrustGrid::RockType::Granite:
                                colour = glm::vec3(0.85f, 0.55f, 0.50f); break;
                            case simulation::CrustGrid::RockType::Andesite:
                                colour = glm::vec3(0.55f, 0.70f, 0.45f); break;
                            case simulation::CrustGrid::RockType::Sediment:
                                colour = glm::vec3(0.90f, 0.85f, 0.55f); break;
                            default: break;
                        }
                        break;
                    case SurfaceView::Thickness: {
                        const float t = glm::clamp(snapshot->crustThickness[cell] / 70000.0f,
                                                   0.0f, 1.0f);
                        colour = glm::mix(glm::vec3(0.05f, 0.10f, 0.35f),
                                          glm::vec3(1.0f, 0.95f, 0.75f), std::sqrt(t));
                        break;
                    }
                    default: break;
                }
            }
            vertex.color = colour;
            continue;
        }

        glm::vec3 color;
        core::MaterialID materialID;
        if (submerged) {
            // Colour by the resolved seafloor depth, not by the roughened
            // height: sub-grid relief is invisible under kilometres of water,
            // and letting it through mottles the whole ocean.
            const float smoothDepth = -field.getLargeScaleElevation(normal);
            const float depth = glm::clamp(smoothDepth / maxOceanDepth, 0.0f, 1.0f);
            const glm::vec3 water = glm::mix(glm::vec3(0.18f, 0.45f, 0.65f),
                                             glm::vec3(0.02f, 0.10f, 0.30f), std::sqrt(depth));

            // Blend into pack ice across the margin rather than switching at a
            // latitude, which draws a straight line across the pole.
            const float ice = field.getSeaIceCoverage(normal);
            color = glm::mix(water, glm::vec3(0.86f, 0.90f, 0.94f), ice);
            materialID = ice > 0.5f ? core::MaterialID::Snow : core::MaterialID::Water;

            materialCounts[static_cast<int>(materialID)]++;
            vertex.color = color;
            continue;
        }

        // Snow is decided by the shared snow line, which drops towards the
        // poles, rather than by a fixed elevation everywhere.
        const float snowLine = field.getSnowLineElevation(normal);
        const float e = elevation / maxElevation;

        if (elevation > snowLine) {
            const float t = glm::clamp((elevation - snowLine) / (maxElevation * 0.15f), 0.0f, 1.0f);
            color = glm::mix(glm::vec3(0.62f, 0.62f, 0.63f),
                             glm::vec3(0.95f, 0.95f, 0.98f), t);
            materialID = core::MaterialID::Snow;
        } else if (e < 0.012f) {
            color = glm::vec3(0.82f, 0.76f, 0.57f);   // beach sand
            materialID = core::MaterialID::Sand;
        } else if (e < 0.10f) {
            const float t = glm::clamp((e - 0.012f) / 0.088f, 0.0f, 1.0f);
            color = glm::mix(glm::vec3(0.32f, 0.55f, 0.22f),
                             glm::vec3(0.17f, 0.38f, 0.14f), t);  // grass to forest
            materialID = core::MaterialID::Grass;
        } else if (e < 0.30f) {
            const float t = glm::clamp((e - 0.10f) / 0.20f, 0.0f, 1.0f);
            color = glm::mix(glm::vec3(0.17f, 0.38f, 0.14f),
                             glm::vec3(0.44f, 0.36f, 0.27f), t);  // forest to rock
            materialID = core::MaterialID::Rock;
        } else {
            const float t = glm::clamp((e - 0.30f) / 0.25f, 0.0f, 1.0f);
            color = glm::mix(glm::vec3(0.44f, 0.36f, 0.27f),
                             glm::vec3(0.55f, 0.53f, 0.50f), t);  // rock to bare stone
            materialID = core::MaterialID::Rock;
        }

        materialCounts[static_cast<int>(materialID)]++;
        vertex.color = color;
    }

    const float displaceMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - displaceStart).count();

    std::cout << "Terrain height range: [" << minHeight << ", " << maxHeight
              << "] m (max elevation " << maxElevation
              << " m, max ocean depth " << maxOceanDepth << " m)"
              << " [displace " << displaceMs << " ms]\n";

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