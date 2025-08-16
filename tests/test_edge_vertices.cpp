#include <iostream>
#include <iomanip>
#include <vector>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

int main() {
    std::cout << "=== EDGE VERTEX ANALYSIS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // Create quadtree
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    config.maxLevel = 3;
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);
    glm::mat4 viewProj = glm::perspective(glm::radians(75.0f), 1280.0f/720.0f, 1000.0f, 100000000.0f);
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    // Setup vertex generator
    rendering::CPUVertexGenerator::Config genConfig;
    genConfig.gridResolution = 33;
    genConfig.planetRadius = config.planetRadius;
    genConfig.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(genConfig);
    
    // Find all patches on face 0 (+X) that touch the Y=1 edge
    std::vector<const core::QuadtreePatch*> face0Edge;
    // Find all patches on face 2 (+Y) that touch the X=1 edge  
    std::vector<const core::QuadtreePatch*> face2Edge;
    
    for (const auto& patch : patches) {
        if (patch.faceId == 0 && std::abs(patch.maxBounds.y - 1.0) < 0.01) {
            face0Edge.push_back(&patch);
        }
        if (patch.faceId == 2 && std::abs(patch.maxBounds.x - 1.0) < 0.01) {
            face2Edge.push_back(&patch);
        }
    }
    
    std::cout << "Face 0 (+X) patches at Y=1 edge: " << face0Edge.size() << std::endl;
    std::cout << "Face 2 (+Y) patches at X=1 edge: " << face2Edge.size() << std::endl;
    
    // Collect all edge vertices from face 0 patches
    std::cout << "\n=== COLLECTING EDGE VERTICES ===" << std::endl;
    
    std::set<std::tuple<double, double, double>> face0EdgeVerts;
    std::set<std::tuple<double, double, double>> face2EdgeVerts;
    
    for (auto patchPtr : face0Edge) {
        auto mesh = generator.generatePatchMesh(*patchPtr, patchPtr->patchTransform);
        
        // Find vertices at Y=1 edge
        for (const auto& v : mesh.vertices) {
            glm::dvec3 pos = glm::dvec3(v.position);
            if (std::abs(pos.y - config.planetRadius) < 100000) { // Within 100km of expected Y
                // Check if this is near the edge in cube space
                glm::dvec3 cubePos = glm::normalize(pos);
                if (std::abs(cubePos.y - 1.0) < 0.1) { // Near Y=1 in cube space
                    face0EdgeVerts.insert(std::make_tuple(pos.x, pos.y, pos.z));
                }
            }
        }
    }
    
    for (auto patchPtr : face2Edge) {
        auto mesh = generator.generatePatchMesh(*patchPtr, patchPtr->patchTransform);
        
        // Find vertices at X=1 edge
        for (const auto& v : mesh.vertices) {
            glm::dvec3 pos = glm::dvec3(v.position);
            if (std::abs(pos.x - config.planetRadius) < 100000) { // Within 100km of expected X
                // Check if this is near the edge in cube space
                glm::dvec3 cubePos = glm::normalize(pos);
                if (std::abs(cubePos.x - 1.0) < 0.1) { // Near X=1 in cube space
                    face2EdgeVerts.insert(std::make_tuple(pos.x, pos.y, pos.z));
                }
            }
        }
    }
    
    std::cout << "Face 0 edge vertices: " << face0EdgeVerts.size() << std::endl;
    std::cout << "Face 2 edge vertices: " << face2EdgeVerts.size() << std::endl;
    
    // Check for exact matches
    int exactMatches = 0;
    for (const auto& v0 : face0EdgeVerts) {
        if (face2EdgeVerts.count(v0) > 0) {
            exactMatches++;
        }
    }
    
    std::cout << "\nExact matches: " << exactMatches << std::endl;
    
    // Check for near matches
    int nearMatches = 0;
    double minDist = 1e10;
    
    for (const auto& v0 : face0EdgeVerts) {
        glm::dvec3 p0(std::get<0>(v0), std::get<1>(v0), std::get<2>(v0));
        
        for (const auto& v2 : face2EdgeVerts) {
            glm::dvec3 p2(std::get<0>(v2), std::get<1>(v2), std::get<2>(v2));
            double dist = glm::length(p0 - p2);
            
            if (dist < minDist) minDist = dist;
            if (dist < 1.0) { // Within 1 meter
                nearMatches++;
                if (nearMatches <= 5) {
                    std::cout << "  Near match: distance = " << dist << " meters" << std::endl;
                }
            }
        }
    }
    
    std::cout << "\nNear matches (within 1m): " << nearMatches << std::endl;
    std::cout << "Minimum distance between edge vertices: " << minDist << " meters" << std::endl;
    
    if (minDist > 1000) {
        std::cout << "\nWARNING: Large gap at edge! (" << minDist/1000 << " km)" << std::endl;
    }
    
    // Sample a few vertices to see their actual positions
    std::cout << "\n=== SAMPLE EDGE VERTICES ===" << std::endl;
    
    std::cout << "\nFace 0 (+X) edge vertices (first 3):" << std::endl;
    int count = 0;
    for (const auto& v : face0EdgeVerts) {
        if (count++ >= 3) break;
        glm::dvec3 pos(std::get<0>(v), std::get<1>(v), std::get<2>(v));
        glm::dvec3 cubePos = glm::normalize(pos);
        std::cout << "  World: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "  Cube:  (" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
    }
    
    std::cout << "\nFace 2 (+Y) edge vertices (first 3):" << std::endl;
    count = 0;
    for (const auto& v : face2EdgeVerts) {
        if (count++ >= 3) break;
        glm::dvec3 pos(std::get<0>(v), std::get<1>(v), std::get<2>(v));
        glm::dvec3 cubePos = glm::normalize(pos);
        std::cout << "  World: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "  Cube:  (" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
    }
    
    return 0;
}