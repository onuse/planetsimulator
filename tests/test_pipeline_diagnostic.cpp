#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

void analyzePatch(const core::QuadtreePatch& patch, const std::string& label) {
    std::cout << "\n" << label << ":" << std::endl;
    std::cout << "  Level: " << patch.level << ", Face: " << patch.faceId << std::endl;
    std::cout << "  MinBounds: (" << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z << ")" << std::endl;
    std::cout << "  MaxBounds: (" << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z << ")" << std::endl;
    std::cout << "  Center: (" << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << ")" << std::endl;
    
    // Check which dimensions are fixed
    glm::dvec3 range = patch.maxBounds - patch.minBounds;
    if (range.x < 1e-6) std::cout << "  Fixed dimension: X at " << patch.center.x << std::endl;
    if (range.y < 1e-6) std::cout << "  Fixed dimension: Y at " << patch.center.y << std::endl;
    if (range.z < 1e-6) std::cout << "  Fixed dimension: Z at " << patch.center.z << std::endl;
}

int main() {
    std::cout << "=== RENDERING PIPELINE DIAGNOSTIC ===" << std::endl;
    std::cout << std::fixed << std::setprecision(10);
    
    // 1. Create quadtree and get patches
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.enableFaceCulling = false;
    config.maxLevel = 5;  // Limit for testing
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Update from a viewpoint
    glm::vec3 viewPos(15000000.0f, 0.0f, 0.0f);
    glm::mat4 viewProj = glm::perspective(glm::radians(75.0f), 1280.0f/720.0f, 1000.0f, 100000000.0f);
    quadtree.update(viewPos, viewProj, 0.016f);
    auto patches = quadtree.getVisiblePatches();
    
    std::cout << "\nGenerated " << patches.size() << " patches" << std::endl;
    
    // 2. Find patches at face boundaries
    std::cout << "\n=== ANALYZING FACE BOUNDARIES ===" << std::endl;
    
    // Look for patches at the boundary between faces
    std::vector<const core::QuadtreePatch*> boundaryPatches;
    for (const auto& patch : patches) {
        // Check if patch is at a face boundary (one dimension at ±1.0)
        bool atBoundary = false;
        if (std::abs(std::abs(patch.minBounds.x) - 1.0) < 0.01 || std::abs(std::abs(patch.maxBounds.x) - 1.0) < 0.01) atBoundary = true;
        if (std::abs(std::abs(patch.minBounds.y) - 1.0) < 0.01 || std::abs(std::abs(patch.maxBounds.y) - 1.0) < 0.01) atBoundary = true;
        if (std::abs(std::abs(patch.minBounds.z) - 1.0) < 0.01 || std::abs(std::abs(patch.maxBounds.z) - 1.0) < 0.01) atBoundary = true;
        
        if (atBoundary) {
            boundaryPatches.push_back(&patch);
        }
    }
    
    std::cout << "Found " << boundaryPatches.size() << " patches at face boundaries" << std::endl;
    
    // Analyze a few boundary patches
    int count = 0;
    for (auto patchPtr : boundaryPatches) {
        if (count++ >= 3) break;
        analyzePatch(*patchPtr, "Boundary Patch");
    }
    
    // 3. Test vertex generation for adjacent patches
    std::cout << "\n=== TESTING VERTEX GENERATION ===" << std::endl;
    
    rendering::CPUVertexGenerator::Config genConfig;
    genConfig.gridResolution = 33;
    genConfig.planetRadius = config.planetRadius;
    genConfig.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(genConfig);
    
    // Find two patches that should share an edge
    const core::QuadtreePatch* patch1 = nullptr;
    const core::QuadtreePatch* patch2 = nullptr;
    
    // Look for patches on faces that share a cube edge
    // For example: +X face (face 0) and +Y face (face 2) share the edge at X=1, Y=1
    for (size_t i = 0; i < patches.size() && !patch2; i++) {
        for (size_t j = i+1; j < patches.size() && !patch2; j++) {
            const auto& p1 = patches[i];
            const auto& p2 = patches[j];
            
            // Check specific face pairs that share edges
            if ((p1.faceId == 0 && p2.faceId == 2) || (p1.faceId == 2 && p2.faceId == 0)) {
                // +X and +Y faces share edge at X=1, Y=1
                bool p1AtEdge = (std::abs(p1.maxBounds.y - 1.0) < 0.1 || std::abs(p1.minBounds.y - 1.0) < 0.1);
                bool p2AtEdge = (std::abs(p2.maxBounds.x - 1.0) < 0.1 || std::abs(p2.minBounds.x - 1.0) < 0.1);
                
                if (p1AtEdge && p2AtEdge) {
                    // Check if they overlap in Z
                    bool overlapZ = !(p1.maxBounds.z < p2.minBounds.z || p2.maxBounds.z < p1.minBounds.z);
                    if (overlapZ) {
                        patch1 = &p1;
                        patch2 = &p2;
                        std::cout << "\nFound patches at +X/+Y edge" << std::endl;
                        break;
                    }
                }
            }
        }
    }
    
    if (patch1 && patch2) {
        std::cout << "\nFound adjacent patches on different faces:" << std::endl;
        analyzePatch(*patch1, "Patch 1");
        analyzePatch(*patch2, "Patch 2");
        
        // Generate vertices for both
        auto mesh1 = generator.generatePatchMesh(*patch1, patch1->patchTransform);
        auto mesh2 = generator.generatePatchMesh(*patch2, patch2->patchTransform);
        
        std::cout << "\nMesh 1: " << mesh1.vertices.size() << " vertices" << std::endl;
        std::cout << "Mesh 2: " << mesh2.vertices.size() << " vertices" << std::endl;
        
        // Check for matching vertices at boundaries
        int matches = 0;
        double minDist = 1e10;
        for (const auto& v1 : mesh1.vertices) {
            for (const auto& v2 : mesh2.vertices) {
                double dist = glm::length(glm::dvec3(v1.position - v2.position));
                if (dist < minDist) minDist = dist;
                if (dist < 0.1) {  // Within 10cm
                    matches++;
                }
            }
        }
        
        std::cout << "\nVertex matching:" << std::endl;
        std::cout << "  Matching vertices (within 10cm): " << matches << std::endl;
        std::cout << "  Minimum distance between vertices: " << minDist << " meters" << std::endl;
        
        if (minDist > 1000) {
            std::cout << "  WARNING: Large gap detected! (" << minDist/1000 << " km)" << std::endl;
        }
    } else {
        std::cout << "Could not find adjacent patches on different faces" << std::endl;
    }
    
    // 4. Check the transform matrices
    std::cout << "\n=== CHECKING TRANSFORMS ===" << std::endl;
    
    for (int i = 0; i < std::min(3, (int)patches.size()); i++) {
        const auto& patch = patches[i];
        std::cout << "\nPatch " << i << " (Face " << patch.faceId << "):" << std::endl;
        
        // Test corner transformation
        glm::dvec4 corners[4] = {
            glm::dvec4(0.0, 0.0, 0.0, 1.0),
            glm::dvec4(1.0, 0.0, 0.0, 1.0),
            glm::dvec4(1.0, 1.0, 0.0, 1.0),
            glm::dvec4(0.0, 1.0, 0.0, 1.0)
        };
        
        for (int c = 0; c < 4; c++) {
            glm::dvec3 transformed = glm::dvec3(patch.patchTransform * corners[c]);
            std::cout << "  Corner " << c << ": UV(" << corners[c].x << "," << corners[c].y 
                      << ") -> (" << transformed.x << ", " << transformed.y << ", " << transformed.z << ")" << std::endl;
            
            // Check for degenerate transforms
            if (transformed.x == 0.0 && transformed.y == 0.0 && transformed.z == 0.0) {
                std::cout << "    ERROR: Transform produced (0,0,0)!" << std::endl;
            }
        }
    }
    
    return 0;
}