#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

void savePatchData(const std::string& filename, const std::vector<core::QuadtreePatch>& patches) {
    std::ofstream file(filename);
    file << std::fixed << std::setprecision(10);
    
    file << "# Planet Patch Data\n";
    file << "# Format: faceId level minBounds maxBounds center\n";
    file << "PATCH_COUNT " << patches.size() << "\n\n";
    
    for (size_t i = 0; i < patches.size(); i++) {
        const auto& p = patches[i];
        file << "PATCH " << i << "\n";
        file << "  faceId: " << p.faceId << "\n";
        file << "  level: " << p.level << "\n";
        file << "  minBounds: " << p.minBounds.x << " " << p.minBounds.y << " " << p.minBounds.z << "\n";
        file << "  maxBounds: " << p.maxBounds.x << " " << p.maxBounds.y << " " << p.maxBounds.z << "\n";
        file << "  center: " << p.center.x << " " << p.center.y << " " << p.center.z << "\n";
        file << "\n";
    }
    
    file.close();
    std::cout << "Saved " << patches.size() << " patches to " << filename << std::endl;
}

void analyzeGaps(const std::vector<core::QuadtreePatch>& patches, 
                 rendering::CPUVertexGenerator& generator) {
    
    std::cout << "\n=== ANALYZING GAPS IN REAL DATA ===" << std::endl;
    
    // Find patches on different faces that should share edges
    std::vector<std::pair<int, int>> adjacentPairs;
    
    for (size_t i = 0; i < patches.size(); i++) {
        for (size_t j = i + 1; j < patches.size(); j++) {
            const auto& p1 = patches[i];
            const auto& p2 = patches[j];
            
            // Skip if same face
            if (p1.faceId == p2.faceId) continue;
            
            // Check if they might share an edge (rough check)
            bool possibleEdge = false;
            
            // Check if patches share a cube edge
            // Face 0 (+X) at Y=1 edge meets Face 2 (+Y) at X=1 edge
            if (p1.faceId == 0 && p2.faceId == 2) {
                // Check if p1 is at Y=1 and p2 is at X=1
                bool p1AtY1 = std::abs(p1.maxBounds.y - 1.0) < 0.01 || std::abs(p1.minBounds.y - 1.0) < 0.01;
                bool p2AtX1 = std::abs(p2.maxBounds.x - 1.0) < 0.01 || std::abs(p2.minBounds.x - 1.0) < 0.01;
                
                if (p1AtY1 && p2AtX1) {
                    // Check Z overlap
                    bool zOverlap = !(p1.maxBounds.z < p2.minBounds.z - 0.01 || p2.maxBounds.z < p1.minBounds.z - 0.01);
                    if (zOverlap) {
                        possibleEdge = true;
                        std::cout << "  Found possible edge: Patch " << i << " and " << j 
                                  << " (Z overlap: " << std::max(p1.minBounds.z, p2.minBounds.z) 
                                  << " to " << std::min(p1.maxBounds.z, p2.maxBounds.z) << ")\n";
                    }
                }
            }
            
            if (possibleEdge) {
                adjacentPairs.push_back({i, j});
            }
        }
    }
    
    std::cout << "Found " << adjacentPairs.size() << " potentially adjacent patch pairs\n";
    
    // Analyze first few pairs
    int analyzed = 0;
    for (const auto& pair : adjacentPairs) {
        if (analyzed++ >= 3) break;
        
        const auto& p1 = patches[pair.first];
        const auto& p2 = patches[pair.second];
        
        std::cout << "\nAnalyzing pair: Patch " << pair.first 
                  << " (face " << p1.faceId << ", level " << p1.level << ")"
                  << " and Patch " << pair.second
                  << " (face " << p2.faceId << ", level " << p2.level << ")\n";
        
        // Generate vertices for both
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
        
        std::cout << "  Min distance: " << minDist << " meters\n";
        std::cout << "  Vertices within 1m: " << closeVertices << "\n";
        
        if (minDist > 1000) {
            std::cout << "  WARNING: Large gap detected (" << minDist/1000 << " km)\n";
        }
    }
}

int main() {
    std::cout << "=== CAPTURING REAL PLANET DATA ===" << std::endl;
    
    // Create the planet with actual parameters
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;  // Get all faces
    config.maxLevel = 10;  // Use actual max level
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Use the actual view position that shows the problem
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);  // ~8600km altitude
    glm::mat4 proj = glm::perspective(glm::radians(75.0f), 1280.0f/720.0f, 1000.0f, 100000000.0f);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 viewProj = proj * view;
    
    // Update quadtree to generate patches
    std::cout << "Generating patches from viewpoint...\n";
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    std::cout << "Generated " << patches.size() << " patches\n";
    
    // Save patch data
    savePatchData("planet_patches.txt", patches);
    
    // Generate meshes with vertex generator to get statistics
    rendering::CPUVertexGenerator::Config genConfig;
    genConfig.gridResolution = 33;
    genConfig.planetRadius = config.planetRadius;
    genConfig.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(genConfig);
    
    std::cout << "\nGenerating meshes for all patches...\n";
    size_t totalVertices = 0;
    size_t totalIndices = 0;
    
    // Generate first 10 patches to get stats
    int patchCount = std::min(10, (int)patches.size());
    for (int i = 0; i < patchCount; i++) {
        auto mesh = generator.generatePatchMesh(patches[i], patches[i].patchTransform);
        totalVertices += mesh.vertices.size();
        totalIndices += mesh.indices.size();
    }
    
    auto stats = generator.getStats();
    std::cout << "Generated " << totalVertices << " vertices from " << patchCount << " patches\n";
    std::cout << "Cache hits: " << stats.cacheHits << "\n";
    std::cout << "Cache misses: " << stats.cacheMisses << "\n";
    
    // Analyze gaps
    analyzeGaps(patches, generator);
    
    // Find patches at specific problem areas
    std::cout << "\n=== CHECKING KNOWN PROBLEM AREAS ===" << std::endl;
    
    // Look for patches at cube face boundaries
    int boundaryPatches = 0;
    for (const auto& p : patches) {
        bool atBoundary = false;
        
        // Check if any bound is at ±1.0
        if (std::abs(std::abs(p.minBounds.x) - 1.0) < 0.01 || 
            std::abs(std::abs(p.maxBounds.x) - 1.0) < 0.01) atBoundary = true;
        if (std::abs(std::abs(p.minBounds.y) - 1.0) < 0.01 || 
            std::abs(std::abs(p.maxBounds.y) - 1.0) < 0.01) atBoundary = true;
        if (std::abs(std::abs(p.minBounds.z) - 1.0) < 0.01 || 
            std::abs(std::abs(p.maxBounds.z) - 1.0) < 0.01) atBoundary = true;
            
        if (atBoundary) {
            boundaryPatches++;
            if (boundaryPatches <= 5) {
                std::cout << "Boundary patch: face=" << p.faceId 
                          << " level=" << p.level
                          << " bounds=[" << p.minBounds.x << "," << p.minBounds.y << "," << p.minBounds.z
                          << " to " << p.maxBounds.x << "," << p.maxBounds.y << "," << p.maxBounds.z << "]\n";
            }
        }
    }
    
    std::cout << "\nTotal boundary patches: " << boundaryPatches << " / " << patches.size() << std::endl;
    
    return 0;
}