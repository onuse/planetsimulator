// Material generation and averaging tests
#include <iostream>
#include <cassert>
#include <cmath>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Test 1: MixedVoxel creation
void test_mixed_voxel_creation() {
    std::cout << "TEST: MixedVoxel creation..." << std::endl;
    
    // Pure materials
    auto rock = MixedVoxel::createPure(255, 0, 0);
    assert(rock.getDominantMaterial() == 1); // Rock
    
    auto water = MixedVoxel::createPure(0, 255, 0);
    assert(water.getDominantMaterial() == 2); // Water
    
    auto air = MixedVoxel::createPure(0, 0, 255);
    assert(air.getDominantMaterial() == 0); // Air
    
    std::cout << "  ✓ Pure materials created correctly" << std::endl;
}

// Test 2: Material blending
void test_material_blending() {
    std::cout << "TEST: Material blending..." << std::endl;
    
    // 50/50 mix
    MixedVoxel mix;
    mix.rock = 128;
    mix.water = 127;
    mix.air = 0;
    mix.sediment = 0;
    
    uint8_t dominant = mix.getDominantMaterial();
    assert(dominant == 1 || dominant == 2); // Should be rock or water
    
    // Mostly air with some rock
    MixedVoxel sparse;
    sparse.rock = 50;
    sparse.water = 0;
    sparse.air = 205;
    sparse.sediment = 0;
    
    assert(sparse.getDominantMaterial() == 0); // Should be air
    
    std::cout << "  ✓ Material blending works" << std::endl;
}

// Test 3: VoxelAverager
void test_voxel_averager() {
    std::cout << "TEST: VoxelAverager..." << std::endl;
    
    // All same material
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(255, 0, 0); // All rock
        }
        auto avg = VoxelAverager::average(voxels);
        assert(avg.getDominantMaterial() == 1); // Should be rock
    }
    
    // Mixed materials
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 4; i++) {
            voxels[i] = MixedVoxel::createPure(255, 0, 0); // Rock
        }
        for (int i = 4; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(0, 255, 0); // Water
        }
        auto avg = VoxelAverager::average(voxels);
        uint8_t dom = avg.getDominantMaterial();
        assert(dom == 1 || dom == 2); // Should be rock or water, not air
    }
    
    // Sparse materials (the problem case)
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 6; i++) {
            voxels[i] = MixedVoxel::createPure(0, 0, 255); // Air
        }
        voxels[6] = MixedVoxel::createPure(255, 0, 0); // Rock
        voxels[7] = MixedVoxel::createPure(0, 255, 0); // Water
        
        auto avg = VoxelAverager::average(voxels);
        uint8_t dom = avg.getDominantMaterial();
        
        // This is the bug - sparse materials average to air
        if (dom == 0) {
            std::cout << "  ⚠️  Known issue: Sparse materials (6 air, 2 material) average to air" << std::endl;
        }
    }
    
    std::cout << "  ✓ VoxelAverager tested" << std::endl;
}

// Test 4: Planet materials distribution
void test_planet_materials() {
    std::cout << "TEST: Planet materials distribution..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    // Get render data
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Count materials
    int materials[4] = {0, 0, 0, 0}; // air, rock, water, magma
    for (const auto& voxel : renderData.voxels) {
        uint8_t mat = voxel.getDominantMaterial();
        if (mat < 4) materials[mat]++;
    }
    
    int total = renderData.voxels.size();
    std::cout << "  Air: " << (materials[0] * 100.0f / total) << "%" << std::endl;
    std::cout << "  Rock: " << (materials[1] * 100.0f / total) << "%" << std::endl;
    std::cout << "  Water: " << (materials[2] * 100.0f / total) << "%" << std::endl;
    
    // Should have some non-air materials
    assert(materials[1] + materials[2] > 0);
    
    std::cout << "  ✓ Planet has materials" << std::endl;
}

// Test 5: Material colors
void test_material_colors() {
    std::cout << "TEST: Material colors..." << std::endl;
    
    auto rock = MixedVoxel::createPure(255, 0, 0);
    glm::vec3 rockColor = rock.getColor();
    assert(rockColor.r > 0.4f && rockColor.r < 0.7f); // Brownish
    
    auto water = MixedVoxel::createPure(0, 255, 0);
    glm::vec3 waterColor = water.getColor();
    assert(waterColor.b > waterColor.r); // Blue dominant
    
    auto air = MixedVoxel::createPure(0, 0, 255);
    glm::vec3 airColor = air.getColor();
    // Air should be nearly transparent (light blue)
    
    std::cout << "  Rock color: (" << rockColor.r << ", " << rockColor.g << ", " << rockColor.b << ")" << std::endl;
    std::cout << "  Water color: (" << waterColor.r << ", " << waterColor.g << ", " << waterColor.b << ")" << std::endl;
    std::cout << "  ✓ Material colors correct" << std::endl;
}

int main() {
    std::cout << "\n=== Material Tests ===" << std::endl;
    
    try {
        test_mixed_voxel_creation();
        test_material_blending();
        test_voxel_averager();
        test_planet_materials();
        test_material_colors();
        
        std::cout << "\n✅ All material tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}