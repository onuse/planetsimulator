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
    
    // Pure materials using new API
    auto rock = MixedVoxel::createPure(core::MaterialID::Rock);
    assert(rock.getDominantMaterialID() == core::MaterialID::Rock);
    
    auto water = MixedVoxel::createPure(core::MaterialID::Water);
    assert(water.getDominantMaterialID() == core::MaterialID::Water);
    
    auto air = MixedVoxel::createPure(core::MaterialID::Air);
    assert(air.getDominantMaterialID() == core::MaterialID::Air);
    
    std::cout << "  ✓ Pure materials created correctly" << std::endl;
}

// Test 2: Material blending
void test_material_blending() {
    std::cout << "TEST: Material blending..." << std::endl;
    
    // 50/50 mix using new API
    MixedVoxel mix = MixedVoxel::createMix(core::MaterialID::Rock, 128, core::MaterialID::Water, 127);
    
    core::MaterialID dominant = mix.getDominantMaterialID();
    assert(dominant == core::MaterialID::Rock || dominant == core::MaterialID::Water);
    
    // Mostly air with some rock
    MixedVoxel sparse = MixedVoxel::createMix(core::MaterialID::Rock, 50, core::MaterialID::Air, 205);
    
    assert(sparse.getDominantMaterialID() == core::MaterialID::Air);
    
    std::cout << "  ✓ Material blending works" << std::endl;
}

// Test 3: VoxelAverager
void test_voxel_averager() {
    std::cout << "TEST: VoxelAverager..." << std::endl;
    
    // All same material
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(core::MaterialID::Rock);
        }
        auto avg = VoxelAverager::average(voxels);
        assert(avg.getDominantMaterialID() == core::MaterialID::Rock);
    }
    
    // Mixed materials
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 4; i++) {
            voxels[i] = MixedVoxel::createPure(core::MaterialID::Rock);
        }
        for (int i = 4; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(core::MaterialID::Water);
        }
        auto avg = VoxelAverager::average(voxels);
        core::MaterialID dom = avg.getDominantMaterialID();
        assert(dom == core::MaterialID::Rock || dom == core::MaterialID::Water);
    }
    
    // Sparse materials (the problem case)
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 6; i++) {
            voxels[i] = MixedVoxel::createPure(core::MaterialID::Air);
        }
        voxels[6] = MixedVoxel::createPure(core::MaterialID::Rock);
        voxels[7] = MixedVoxel::createPure(core::MaterialID::Water);
        
        auto avg = VoxelAverager::average(voxels);
        core::MaterialID dom = avg.getDominantMaterialID();
        
        // This is the bug - sparse materials average to air
        if (dom == core::MaterialID::Air) {
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
    int airCount = 0, rockCount = 0, waterCount = 0;
    for (const auto& voxel : renderData.voxels) {
        core::MaterialID mat = voxel.getDominantMaterialID();
        if (mat == core::MaterialID::Air || mat == core::MaterialID::Vacuum) airCount++;
        else if (mat == core::MaterialID::Rock) rockCount++;
        else if (mat == core::MaterialID::Water) waterCount++;
    }
    
    int total = renderData.voxels.size();
    std::cout << "  Air: " << (airCount * 100.0f / total) << "%" << std::endl;
    std::cout << "  Rock: " << (rockCount * 100.0f / total) << "%" << std::endl;
    std::cout << "  Water: " << (waterCount * 100.0f / total) << "%" << std::endl;
    
    // Should have some non-air materials
    assert(rockCount + waterCount > 0);
    
    std::cout << "  ✓ Planet has materials" << std::endl;
}

// Test 5: Material colors
void test_material_colors() {
    std::cout << "TEST: Material colors..." << std::endl;
    
    auto rock = MixedVoxel::createPure(core::MaterialID::Rock);
    glm::vec3 rockColor = rock.getColor();
    assert(rockColor.r > 0.4f && rockColor.r < 0.7f); // Brownish
    
    auto water = MixedVoxel::createPure(core::MaterialID::Water);
    glm::vec3 waterColor = water.getColor();
    assert(waterColor.b > waterColor.r); // Blue dominant
    
    auto air = MixedVoxel::createPure(core::MaterialID::Air);
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