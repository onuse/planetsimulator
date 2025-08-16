#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "core/cross_face_neighbor_finder.hpp"
#include "rendering/cpu_vertex_generator.hpp"

void analyzePatchPair(const core::QuadtreePatch& p1, const core::QuadtreePatch& p2,
                      rendering::CPUVertexGenerator& generator) {
    std::cout << "\nAnalyzing patches:" << std::endl;
    std::cout << "  Patch 1: Face " << p1.faceId << ", Level " << p1.level 
              << ", Bounds [" << p1.minBounds.x << "," << p1.minBounds.y << "," << p1.minBounds.z
              << "] to [" << p1.maxBounds.x << "," << p1.maxBounds.y << "," << p1.maxBounds.z << "]" << std::endl;
    std::cout << "  Patch 2: Face " << p2.faceId << ", Level " << p2.level
              << ", Bounds [" << p2.minBounds.x << "," << p2.minBounds.y << "," << p2.minBounds.z
              << "] to [" << p2.maxBounds.x << "," << p2.maxBounds.y << "," << p2.maxBounds.z << "]" << std::endl;
    
    // Check neighbor levels
    std::cout << "  Patch 1 neighbor levels: [" 
              << p1.neighborLevels[0] << "," << p1.neighborLevels[1] << ","
              << p1.neighborLevels[2] << "," << p1.neighborLevels[3] << "]" << std::endl;
    std::cout << "  Patch 2 neighbor levels: [" 
              << p2.neighborLevels[0] << "," << p2.neighborLevels[1] << ","
              << p2.neighborLevels[2] << "," << p2.neighborLevels[3] << "]" << std::endl;
    
    // Generate vertices
    auto mesh1 = generator.generatePatchMesh(p1, p1.patchTransform);
    auto mesh2 = generator.generatePatchMesh(p2, p2.patchTransform);
    
    // Find minimum distance
    double minDist = 1e10;
    int closeVertices = 0;
    
    for (const auto& v1 : mesh1.vertices) {
        for (const auto& v2 : mesh2.vertices) {
            double dist = glm::length(glm::dvec3(v1.position - v2.position));
            if (dist < minDist) minDist = dist;
            if (dist < 1.0) closeVertices++;  // Within 1 meter
        }
    }
    
    std::cout << "  Min distance: " << minDist << " meters";
    if (minDist > 1000) {
        std::cout << " (" << minDist/1000 << " km) - LARGE GAP!";
    }
    std::cout << std::endl;
    std::cout << "  Vertices within 1m: " << closeVertices << std::endl;
}

int main() {
    std::cout << "=== TESTING CROSS-FACE NEIGHBOR FIX ===" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    
    // Create planet
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    config.maxLevel = 10;
    config.enableCrackFixes = true;  // Enable our fix!
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Generate patches
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);
    glm::mat4 proj = glm::perspective(glm::radians(75.0f), 1280.0f/720.0f, 1000.0f, 100000000.0f);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 viewProj = proj * view;
    
    std::cout << "\n1. Generating patches WITH cross-face neighbor fix..." << std::endl;
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    std::cout << "   Generated " << patches.size() << " patches" << std::endl;
    
    // Create vertex generator
    rendering::CPUVertexGenerator::Config genConfig;
    genConfig.gridResolution = 33;
    genConfig.planetRadius = config.planetRadius;
    genConfig.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(genConfig);
    
    // Find patches at face boundaries
    std::cout << "\n2. Finding patches at face boundaries..." << std::endl;
    
    std::vector<std::pair<size_t, size_t>> crossFacePairs;
    const float eps = 0.01f;
    
    for (size_t i = 0; i < patches.size(); i++) {
        for (size_t j = i + 1; j < patches.size(); j++) {
            const auto& p1 = patches[i];
            const auto& p2 = patches[j];
            
            // Skip if same face
            if (p1.faceId == p2.faceId) continue;
            
            // Check for potential adjacency
            bool possibleAdjacent = false;
            
            // Example: Face 0 (+X) at Y=1 meets Face 2 (+Y) at X=1
            if (p1.faceId == 0 && p2.faceId == 2) {
                bool p1AtTop = std::abs(p1.maxBounds.y - 1.0) < eps;
                bool p2AtRight = std::abs(p2.maxBounds.x - 1.0) < eps;
                if (p1AtTop && p2AtRight) {
                    // Check Z overlap
                    bool zOverlap = !(p1.maxBounds.z < p2.minBounds.z - eps || 
                                     p2.maxBounds.z < p1.minBounds.z - eps);
                    if (zOverlap) possibleAdjacent = true;
                }
            }
            // Add other face combinations as needed...
            
            if (possibleAdjacent) {
                crossFacePairs.push_back({i, j});
            }
        }
    }
    
    std::cout << "   Found " << crossFacePairs.size() << " cross-face patch pairs" << std::endl;
    
    // Analyze first few pairs
    std::cout << "\n3. Analyzing gaps between cross-face patches..." << std::endl;
    
    int analyzed = 0;
    int withLargeGaps = 0;
    double totalMinDist = 0;
    
    for (const auto& pair : crossFacePairs) {
        if (analyzed >= 5) break;  // Analyze first 5 pairs
        
        const auto& p1 = patches[pair.first];
        const auto& p2 = patches[pair.second];
        
        analyzePatchPair(p1, p2, generator);
        
        // Calculate gap for statistics
        auto mesh1 = generator.generatePatchMesh(p1, p1.patchTransform);
        auto mesh2 = generator.generatePatchMesh(p2, p2.patchTransform);
        
        double minDist = 1e10;
        for (const auto& v1 : mesh1.vertices) {
            for (const auto& v2 : mesh2.vertices) {
                double dist = glm::length(glm::dvec3(v1.position - v2.position));
                if (dist < minDist) minDist = dist;
            }
        }
        
        totalMinDist += minDist;
        if (minDist > 1000) withLargeGaps++;
        analyzed++;
    }
    
    std::cout << "\n=== RESULTS ===" << std::endl;
    if (analyzed > 0) {
        double avgGap = totalMinDist / analyzed;
        std::cout << "Average gap: " << avgGap << " meters";
        if (avgGap > 1000) {
            std::cout << " (" << avgGap/1000 << " km)";
        }
        std::cout << std::endl;
        std::cout << "Pairs with >1km gaps: " << withLargeGaps << " / " << analyzed << std::endl;
        
        if (avgGap < 100) {
            std::cout << "\nSUCCESS: Cross-face neighbor fix appears to be working!" << std::endl;
        } else {
            std::cout << "\nFAILURE: Still have large gaps. More investigation needed." << std::endl;
        }
    }
    
    return 0;
}