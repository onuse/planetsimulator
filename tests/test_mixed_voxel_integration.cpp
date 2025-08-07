#include <iostream>
#include <cassert>
#include "../include/core/octree.hpp"
#include "../include/rendering/instance_buffer_manager.hpp"

using namespace octree;
using namespace rendering;

void testMixedVoxelInOctree() {
    std::cout << "Test 1: Mixed Voxel Creation" << std::endl;
    
    // Test that MixedVoxel works with new API
    MixedVoxel voxel1 = MixedVoxel::createPure(core::MaterialID::Rock);
    MixedVoxel voxel2 = MixedVoxel::createPure(core::MaterialID::Water);
    MixedVoxel voxel3 = MixedVoxel::createMix(core::MaterialID::Rock, 128, core::MaterialID::Water, 128);
    
    assert(voxel1.getDominantMaterialID() == core::MaterialID::Rock);
    assert(voxel2.getDominantMaterialID() == core::MaterialID::Water);
    
    // Test color generation
    glm::vec3 rockColor = voxel1.getColor();
    glm::vec3 waterColor = voxel2.getColor();
    glm::vec3 mixedColor = voxel3.getColor();
    
    // Rock should be brownish
    assert(rockColor.x > rockColor.z);
    // Water should be bluish
    assert(waterColor.z > waterColor.x);
    
    std::cout << "  ✓ Mixed voxels work correctly" << std::endl;
}

void testInstanceColorGeneration() {
    std::cout << "Test 2: Instance Buffer Color Generation" << std::endl;
    
    // Create render data with mixed voxels
    OctreePlanet::RenderData renderData;
    
    // Add a test node
    OctreeNode::GPUNode gpuNode;
    gpuNode.center = glm::vec3(0.0f);
    gpuNode.halfSize = 100.0f;
    gpuNode.flags = 1; // Leaf node
    gpuNode.voxelIndex = 0;
    
    renderData.nodes.push_back(gpuNode);
    renderData.visibleNodes.push_back(0);
    
    // Add mixed voxels
    for (int i = 0; i < 8; i++) {
        MixedVoxel voxel;
        if (i == 0) {
            // Pure rock - should be brownish
            voxel = MixedVoxel::createPure(core::MaterialID::Rock);
        } else if (i == 1) {
            // Pure water - should be blueish
            voxel = MixedVoxel::createPure(core::MaterialID::Water);
        } else if (i == 2) {
            // Mixed coast - should be blend
            voxel = MixedVoxel::createMix(core::MaterialID::Rock, 128, core::MaterialID::Water, 128);
        } else {
            // Air - should be skipped
            voxel = MixedVoxel::createPure(core::MaterialID::Air);
        }
        renderData.voxels.push_back(voxel);
    }
    
    // Generate instances
    InstanceBufferManager::Statistics stats;
    auto instances = InstanceBufferManager::createInstancesFromVoxels(renderData, &stats);
    
    // Should have 3 instances (rock, water, coast; air skipped)
    assert(instances.size() == 3);
    
    // Check colors
    auto& rockInst = instances[0];
    auto& waterInst = instances[1];
    auto& coastInst = instances[2];
    
    // Rock should be brownish (more red than blue)
    assert(rockInst.colorAndMaterial.x > rockInst.colorAndMaterial.z);
    
    // Water should be blueish (more blue than red)
    assert(waterInst.colorAndMaterial.z > waterInst.colorAndMaterial.x);
    
    // Coast should be in between
    float coastRed = coastInst.colorAndMaterial.x;
    float coastBlue = coastInst.colorAndMaterial.z;
    assert(coastRed < rockInst.colorAndMaterial.x);  // Less red than pure rock
    assert(coastBlue < waterInst.colorAndMaterial.z); // Less blue than pure water
    
    std::cout << "  ✓ Instance buffer generates correct blended colors" << std::endl;
    std::cout << "    Rock color: (" << rockInst.colorAndMaterial.x << ", " 
              << rockInst.colorAndMaterial.y << ", " << rockInst.colorAndMaterial.z << ")" << std::endl;
    std::cout << "    Water color: (" << waterInst.colorAndMaterial.x << ", " 
              << waterInst.colorAndMaterial.y << ", " << waterInst.colorAndMaterial.z << ")" << std::endl;
    std::cout << "    Coast color: (" << coastInst.colorAndMaterial.x << ", " 
              << coastInst.colorAndMaterial.y << ", " << coastInst.colorAndMaterial.z << ")" << std::endl;
}

void testPlanetGeneration() {
    std::cout << "Test 3: Planet Generation with Mixed Materials" << std::endl;
    
    // Create a small planet
    float radius = 1000000.0f; // 1000km
    OctreePlanet planet(radius, 5);
    
    // Generate with seed
    planet.generate(42);
    
    // Get render data
    glm::vec3 viewPos(0.0f, 0.0f, radius * 2.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Analyze voxel composition
    int pureRock = 0, pureWater = 0, mixed = 0, air = 0;
    
    for (const auto& voxel : renderData.voxels) {
        core::MaterialID dominant = voxel.getDominantMaterialID();
        if (dominant == core::MaterialID::Air || dominant == core::MaterialID::Vacuum) {
            air++;
        } else if (dominant == core::MaterialID::Rock) {
            pureRock++;
        } else if (dominant == core::MaterialID::Water) {
            pureWater++;
        } else {
            mixed++;
        }
    }
    
    std::cout << "  Voxel composition:" << std::endl;
    std::cout << "    Pure rock: " << pureRock << std::endl;
    std::cout << "    Pure water: " << pureWater << std::endl;
    std::cout << "    Mixed: " << mixed << std::endl;
    std::cout << "    Air: " << air << std::endl;
    
    // Should have significant water and rock
    assert(pureWater > 0);
    assert(pureRock > 0);
    
    // Mixed materials should exist at boundaries
    assert(mixed > 0);
    
    std::cout << "  ✓ Planet generates with proper material mixing" << std::endl;
}

int main() {
    std::cout << "=== MIXED VOXEL INTEGRATION TESTS ===" << std::endl << std::endl;
    
    testMixedVoxelInOctree();
    testInstanceColorGeneration();
    testPlanetGeneration();
    
    std::cout << std::endl << "=== ALL INTEGRATION TESTS PASSED ===" << std::endl;
    
    return 0;
}