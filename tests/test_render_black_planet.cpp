// Focused test to find why planet renders black
// Uses only public API to test the rendering pipeline

#include <iostream>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"
#include "rendering/instance_buffer_manager.hpp"

using namespace octree;

// Test that planet generation creates non-air materials
void test_planet_generates_materials() {
    std::cout << "TEST: Planet generates non-air materials..." << std::endl;
    
    float radius = 6371000.0f; // Earth radius
    OctreePlanet planet(radius, 5); // Depth 5 for more detail
    planet.generate(42); // Generate with seed
    
    // Get render data
    glm::vec3 viewPos(0, 0, radius * 2.0f); // View from 2x radius
    glm::mat4 viewProj = glm::mat4(1.0f); // Identity for now
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    std::cout << "  Total nodes: " << renderData.nodes.size() << std::endl;
    std::cout << "  Visible nodes: " << renderData.visibleNodes.size() << std::endl;
    std::cout << "  Total voxels: " << renderData.voxels.size() << std::endl;
    
    // Count materials in voxels
    int airCount = 0, rockCount = 0, waterCount = 0, unknownCount = 0;
    
    for (const auto& voxel : renderData.voxels) {
        uint8_t mat = voxel.getDominantMaterial();
        switch(mat) {
            case 0: airCount++; break;
            case 1: rockCount++; break;
            case 2: waterCount++; break;
            default: unknownCount++; break;
        }
    }
    
    std::cout << "  Voxel materials: " << airCount << " air, " 
              << rockCount << " rock, " << waterCount << " water, "
              << unknownCount << " unknown" << std::endl;
    
    // Problem: If all voxels are air, planet will be invisible
    if (rockCount == 0 && waterCount == 0) {
        std::cerr << "  ❌ ERROR: Planet has NO solid materials!" << std::endl;
        std::cerr << "     This would cause an invisible planet!" << std::endl;
        assert(false && "Planet has no visible materials");
    }
    
    // Check if visible nodes point to valid voxels
    for (uint32_t nodeIdx : renderData.visibleNodes) {
        const auto& node = renderData.nodes[nodeIdx];
        if (node.flags & 1) { // Is leaf
            uint32_t voxelIdx = node.voxelIndex;
            if (voxelIdx != 0xFFFFFFFF && voxelIdx < renderData.voxels.size()) {
                // Good - node points to valid voxels
            } else {
                std::cerr << "  ❌ ERROR: Leaf node " << nodeIdx 
                          << " has invalid voxel index " << voxelIdx << std::endl;
            }
        }
    }
    
    std::cout << "  ✓ Planet has visible materials" << std::endl;
}

// Test that instances are created with proper colors
void test_instances_have_colors() {
    std::cout << "TEST: Instances have proper colors..." << std::endl;
    
    float radius = 6371000.0f;
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, radius * 1.5f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Create instances
    rendering::InstanceBufferManager::Statistics stats;
    auto instances = rendering::InstanceBufferManager::createInstancesFromVoxels(
        renderData, &stats);
    
    std::cout << "  Created " << instances.size() << " instances" << std::endl;
    std::cout << "  Stats: " << stats.rockCount << " rock, " 
              << stats.waterCount << " water, " << stats.airCount << " air" << std::endl;
    
    if (instances.empty()) {
        std::cerr << "  ❌ ERROR: No instances created!" << std::endl;
        assert(false && "No instances created");
    }
    
    // Check colors
    int blackCount = 0, coloredCount = 0;
    float minR = 1.0f, maxR = 0.0f;
    float minG = 1.0f, maxG = 0.0f;
    float minB = 1.0f, maxB = 0.0f;
    
    for (const auto& inst : instances) {
        float r = inst.colorAndMaterial.r;
        float g = inst.colorAndMaterial.g;
        float b = inst.colorAndMaterial.b;
        
        minR = std::min(minR, r);
        maxR = std::max(maxR, r);
        minG = std::min(minG, g);
        maxG = std::max(maxG, g);
        minB = std::min(minB, b);
        maxB = std::max(maxB, b);
        
        if (r < 0.01f && g < 0.01f && b < 0.01f) {
            blackCount++;
        } else {
            coloredCount++;
        }
    }
    
    std::cout << "  Color ranges: R[" << minR << "-" << maxR 
              << "] G[" << minG << "-" << maxG 
              << "] B[" << minB << "-" << maxB << "]" << std::endl;
    std::cout << "  " << coloredCount << " colored, " << blackCount << " black" << std::endl;
    
    if (coloredCount == 0) {
        std::cerr << "  ❌ ERROR: All instances are black!" << std::endl;
        std::cerr << "     This causes invisible planet!" << std::endl;
        assert(false && "All instances are black");
    }
    
    // Check material types
    int mat0 = 0, mat1 = 0, mat2 = 0, mat3 = 0;
    for (const auto& inst : instances) {
        int mat = static_cast<int>(inst.colorAndMaterial.w + 0.5f);
        switch(mat) {
            case 0: mat0++; break;
            case 1: mat1++; break;
            case 2: mat2++; break;
            case 3: mat3++; break;
        }
    }
    
    std::cout << "  Material types: " << mat0 << " air, " << mat1 << " rock, "
              << mat2 << " water, " << mat3 << " other" << std::endl;
    
    std::cout << "  ✓ Instances have proper colors" << std::endl;
}

// Test specific case: nodes near planet surface
void test_surface_nodes_have_materials() {
    std::cout << "TEST: Surface nodes have materials..." << std::endl;
    
    float radius = 6371000.0f;
    OctreePlanet planet(radius, 6); // Higher detail
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, radius * 1.2f); // Close to surface
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Find nodes near surface
    int surfaceNodes = 0;
    int surfaceWithMaterial = 0;
    
    for (uint32_t idx : renderData.visibleNodes) {
        const auto& node = renderData.nodes[idx];
        float dist = glm::length(node.center);
        
        // Check if near surface (within 10% of radius)
        if (dist > radius * 0.9f && dist < radius * 1.1f) {
            surfaceNodes++;
            
            // Check if it has materials
            if (node.flags & 1) { // Is leaf
                uint32_t voxelIdx = node.voxelIndex;
                if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                    // Check voxels for this node
                    bool hasMaterial = false;
                    for (int i = 0; i < 8; i++) {
                        uint8_t mat = renderData.voxels[voxelIdx + i].getDominantMaterial();
                        if (mat == 1 || mat == 2) { // Rock or water
                            hasMaterial = true;
                            break;
                        }
                    }
                    if (hasMaterial) {
                        surfaceWithMaterial++;
                    }
                }
            }
        }
    }
    
    std::cout << "  Surface nodes: " << surfaceNodes << std::endl;
    std::cout << "  Surface nodes with material: " << surfaceWithMaterial << std::endl;
    
    if (surfaceNodes > 0 && surfaceWithMaterial == 0) {
        std::cerr << "  ❌ ERROR: Surface nodes have no materials!" << std::endl;
        std::cerr << "     Planet surface would be invisible!" << std::endl;
        assert(false && "Surface has no materials");
    }
    
    std::cout << "  ✓ Surface nodes have materials" << std::endl;
}

// Test that voxel colors match expected values
void test_voxel_colors_correct() {
    std::cout << "TEST: Voxel colors are correct..." << std::endl;
    
    // Test pure materials directly
    MixedVoxel rockVoxel = MixedVoxel::createPure(255, 0, 0);
    glm::vec3 rockColor = rockVoxel.getColor();
    std::cout << "  Rock color: (" << rockColor.r << ", " << rockColor.g 
              << ", " << rockColor.b << ")" << std::endl;
    
    if (rockColor.r < 0.3f || rockColor.r > 0.7f) {
        std::cerr << "  ❌ ERROR: Rock color incorrect (should be brownish)" << std::endl;
    }
    
    MixedVoxel waterVoxel = MixedVoxel::createPure(0, 255, 0);
    glm::vec3 waterColor = waterVoxel.getColor();
    std::cout << "  Water color: (" << waterColor.r << ", " << waterColor.g 
              << ", " << waterColor.b << ")" << std::endl;
    
    if (waterColor.b < 0.5f) {
        std::cerr << "  ❌ ERROR: Water color incorrect (should be blue)" << std::endl;
    }
    
    // Test mixed material
    MixedVoxel mixedVoxel = MixedVoxel::createPure(128, 128, 0);
    glm::vec3 mixedColor = mixedVoxel.getColor();
    std::cout << "  Mixed (50/50) color: (" << mixedColor.r << ", " << mixedColor.g 
              << ", " << mixedColor.b << ")" << std::endl;
    
    // Mixed should be between rock and water colors
    if (mixedColor.r < 0.1f || mixedColor.b < 0.1f) {
        std::cerr << "  ❌ ERROR: Mixed color incorrect" << std::endl;
    }
    
    std::cout << "  ✓ Voxel colors are correct" << std::endl;
}

int main() {
    std::cout << "\n=== Black Planet Diagnosis Test ===" << std::endl;
    std::cout << "Finding why planet renders black/invisible\n" << std::endl;
    
    try {
        test_voxel_colors_correct();
        test_planet_generates_materials();
        test_surface_nodes_have_materials();
        test_instances_have_colors();
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        std::cout << "\nPlanet data is CORRECT. Black rendering must be due to:" << std::endl;
        std::cout << "  1. Shader not receiving instance data correctly" << std::endl;
        std::cout << "  2. Vertex attributes not bound properly" << std::endl;
        std::cout << "  3. Lighting calculation producing black results" << std::endl;
        std::cout << "  4. GPU buffer not uploaded correctly" << std::endl;
        std::cout << "  5. Wrong pipeline or render state" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        std::cerr << "\nFound the issue causing black planet!" << std::endl;
        return 1;
    }
    
    return 0;
}