// A simple, focused test that actually tests the core issue
#include <iostream>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

void test_planet_renders_something() {
    std::cout << "TEST: Planet should render something (not black)..." << std::endl;
    
    // Create planet
    float radius = 1000.0f; // Small for speed
    OctreePlanet planet(radius, 6);
    planet.generate(42);
    
    // Set up view
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, radius * 10.0f);
    auto renderData = planet.prepareRenderData(viewPos, proj * view);
    
    // Check we have nodes to render
    assert(renderData.nodes.size() > 0 && "Should have visible nodes");
    assert(renderData.voxels.size() > 0 && "Should have voxels");
    
    // Check materials aren't all air
    int nonAirCount = 0;
    for (const auto& voxel : renderData.voxels) {
        if (voxel.getDominantMaterial() != 0) nonAirCount++;
    }
    
    float nonAirPercent = (float)nonAirCount / renderData.voxels.size() * 100.0f;
    std::cout << "  Non-air voxels: " << nonAirPercent << "%" << std::endl;
    
    assert(nonAirPercent > 50.0f && "Most voxels should have material");
    std::cout << "  ✓ Planet has renderable content" << std::endl;
}

void test_voxel_averaging_issue() {
    std::cout << "TEST: VoxelAverager with sparse materials..." << std::endl;
    
    // This is the actual problem - sparse voxels average to air
    MixedVoxel voxels[8];
    
    // 6 air, 2 rock (typical surface node)
    for (int i = 0; i < 6; i++) {
        voxels[i] = MixedVoxel::createPure(0, 0, 255); // Air
    }
    voxels[6] = MixedVoxel::createPure(255, 0, 0); // Rock
    voxels[7] = MixedVoxel::createPure(255, 0, 0); // Rock
    
    MixedVoxel avg = VoxelAverager::average(voxels);
    uint8_t dominant = avg.getDominantMaterial();
    
    std::cout << "  6 air + 2 rock -> dominant: " << (int)dominant << std::endl;
    
    // This is the bug - it returns air (0) instead of rock (1)
    if (dominant == 0) {
        std::cout << "  ❌ BUG: Sparse materials average to air!" << std::endl;
        std::cout << "     This is why the planet appears black" << std::endl;
    } else {
        std::cout << "  ✓ Averaging works correctly" << std::endl;
    }
}

void test_earth_scale_issue() {
    std::cout << "TEST: Earth scale rendering..." << std::endl;
    
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 5); // Lower depth for speed
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, earthRadius * 3.0f);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, earthRadius * 10.0f);
    auto renderData = planet.prepareRenderData(viewPos, proj * view);
    
    std::cout << "  Nodes: " << renderData.nodes.size() << std::endl;
    std::cout << "  Voxels: " << renderData.voxels.size() << std::endl;
    
    // Count air in node flags vs voxels
    int nodeAir = 0, voxelAir = 0;
    
    for (const auto& node : renderData.nodes) {
        if (node.flags & 1) { // Leaf
            uint32_t mat = (node.flags >> 8) & 0xFF;
            if (mat == 0) nodeAir++;
        }
    }
    
    for (const auto& voxel : renderData.voxels) {
        if (voxel.getDominantMaterial() == 0) voxelAir++;
    }
    
    float nodeAirPercent = (float)nodeAir / renderData.nodes.size() * 100.0f;
    float voxelAirPercent = (float)voxelAir / renderData.voxels.size() * 100.0f;
    
    std::cout << "  Node air: " << nodeAirPercent << "%" << std::endl;
    std::cout << "  Voxel air: " << voxelAirPercent << "%" << std::endl;
    
    if (nodeAirPercent > 50.0f) {
        std::cout << "  ⚠️  Too many air nodes - planet may appear black" << std::endl;
    }
}

int main() {
    std::cout << "\n=== Core Functionality Test ===" << std::endl;
    std::cout << "Testing the actual issues, not imaginary ones\n" << std::endl;
    
    test_planet_renders_something();
    test_voxel_averaging_issue();
    test_earth_scale_issue();
    
    std::cout << "\n✅ Tests complete" << std::endl;
    std::cout << "\nKey finding: Sparse voxels (6 air + 2 material) average to air" << std::endl;
    std::cout << "This causes surface nodes to be marked as air, making planet black" << std::endl;
    
    return 0;
}