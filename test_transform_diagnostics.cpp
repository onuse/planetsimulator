#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/global_patch_generator.hpp"
#include "core/spherical_quadtree.hpp"

void analyzePatch(int patchId, const core::QuadtreePatch& patch) {
    std::cout << "\n=== PATCH " << patchId << " ANALYSIS ===" << std::endl;
    std::cout << "Face: " << patch.faceId << ", Level: " << patch.level << std::endl;
    std::cout << "Center: (" << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << ")" << std::endl;
    std::cout << "Corners:" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "  [" << i << "]: (" << patch.corners[i].x << ", " 
                  << patch.corners[i].y << ", " << patch.corners[i].z << ")" << std::endl;
    }
    
    // Calculate bounds from corners
    glm::vec3 minBounds(1e9f), maxBounds(-1e9f);
    for (int i = 0; i < 4; i++) {
        minBounds = glm::min(minBounds, patch.corners[i]);
        maxBounds = glm::max(maxBounds, patch.corners[i]);
    }
    
    std::cout << "Calculated bounds:" << std::endl;
    std::cout << "  Min: (" << minBounds.x << ", " << minBounds.y << ", " << minBounds.z << ")" << std::endl;
    std::cout << "  Max: (" << maxBounds.x << ", " << maxBounds.y << ", " << maxBounds.z << ")" << std::endl;
    
    // Create GlobalPatch and test transform
    core::GlobalPatchGenerator::GlobalPatch globalPatch;
    globalPatch.minBounds = minBounds;
    globalPatch.maxBounds = maxBounds;
    globalPatch.center = patch.center;
    globalPatch.level = patch.level;
    globalPatch.faceId = patch.faceId;
    
    glm::dmat4 transform = globalPatch.createTransform();
    double det = glm::determinant(transform);
    
    std::cout << "Transform determinant: " << det << std::endl;
    if (det < 0) {
        std::cout << "*** INVERTED TRANSFORM DETECTED ***" << std::endl;
        
        // Analyze why it's inverted
        glm::dvec3 range = glm::dvec3(maxBounds - minBounds);
        std::cout << "Range: (" << range.x << ", " << range.y << ", " << range.z << ")" << std::endl;
        
        // Show transform columns
        std::cout << "Transform columns:" << std::endl;
        for (int col = 0; col < 4; col++) {
            std::cout << "  Col " << col << ": (" << transform[col][0] << ", " 
                      << transform[col][1] << ", " << transform[col][2] << ", " 
                      << transform[col][3] << ")" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== TRANSFORM DIAGNOSTIC TOOL ===" << std::endl;
    
    // Create a simple quadtree to get some patches
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.maxLevel = 5;
    auto densityField = std::make_shared<core::DensityField>(config.planetRadius, 42);
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Update to generate patches
    glm::vec3 viewPos(config.planetRadius * 2.5f, 0, 0);
    glm::mat4 viewProj = glm::perspective(glm::radians(45.0f), 1.0f, 1.0f, 1e8f);
    quadtree.update(viewPos, viewProj, 0.016f);
    
    const auto& patches = quadtree.getVisiblePatches();
    std::cout << "\nTotal patches: " << patches.size() << std::endl;
    
    // Analyze patches that typically fail (71-123)
    int failCount = 0;
    for (size_t i = 0; i < patches.size() && i < 150; i++) {
        // Calculate bounds and test transform
        glm::vec3 minBounds(1e9f), maxBounds(-1e9f);
        for (int j = 0; j < 4; j++) {
            minBounds = glm::min(minBounds, patches[i].corners[j]);
            maxBounds = glm::max(maxBounds, patches[i].corners[j]);
        }
        
        core::GlobalPatchGenerator::GlobalPatch globalPatch;
        globalPatch.minBounds = minBounds;
        globalPatch.maxBounds = maxBounds;
        globalPatch.center = patches[i].center;
        globalPatch.level = patches[i].level;
        globalPatch.faceId = patches[i].faceId;
        
        glm::dmat4 transform = globalPatch.createTransform();
        double det = glm::determinant(transform);
        
        if (det < 0) {
            if (failCount < 5) {  // Analyze first 5 failures in detail
                analyzePatch(i, patches[i]);
            }
            failCount++;
        }
    }
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Failed patches: " << failCount << " out of " << patches.size() << std::endl;
    
    // Group failures by face and level
    int faceFailures[6] = {0};
    int levelFailures[10] = {0};
    
    for (size_t i = 0; i < patches.size(); i++) {
        glm::vec3 minBounds(1e9f), maxBounds(-1e9f);
        for (int j = 0; j < 4; j++) {
            minBounds = glm::min(minBounds, patches[i].corners[j]);
            maxBounds = glm::max(maxBounds, patches[i].corners[j]);
        }
        
        core::GlobalPatchGenerator::GlobalPatch globalPatch;
        globalPatch.minBounds = minBounds;
        globalPatch.maxBounds = maxBounds;
        globalPatch.center = patches[i].center;
        globalPatch.level = patches[i].level;
        globalPatch.faceId = patches[i].faceId;
        
        glm::dmat4 transform = globalPatch.createTransform();
        double det = glm::determinant(transform);
        
        if (det < 0) {
            if (patches[i].faceId < 6) faceFailures[patches[i].faceId]++;
            if (patches[i].level < 10) levelFailures[patches[i].level]++;
        }
    }
    
    std::cout << "\nFailures by face:" << std::endl;
    for (int i = 0; i < 6; i++) {
        if (faceFailures[i] > 0) {
            std::cout << "  Face " << i << ": " << faceFailures[i] << " failures" << std::endl;
        }
    }
    
    std::cout << "\nFailures by level:" << std::endl;
    for (int i = 0; i < 10; i++) {
        if (levelFailures[i] > 0) {
            std::cout << "  Level " << i << ": " << levelFailures[i] << " failures" << std::endl;
        }
    }
    
    return 0;
}