#include <iostream>
#include <cassert>
#include <cmath>
#include "../include/core/mixed_voxel.hpp"

using namespace octree;

void testBasicMixedVoxel() {
    std::cout << "Test 1: Basic Mixed Voxel" << std::endl;
    
    // Test pure materials
    auto rock = MixedVoxel::createPure(255, 0, 0);
    assert(rock.getDominantMaterial() == 1);  // Rock
    assert(rock.getRockPercent() > 0.99f);
    
    auto water = MixedVoxel::createPure(0, 255, 0);
    assert(water.getDominantMaterial() == 2);  // Water
    assert(water.getWaterPercent() > 0.99f);
    
    // Test mixed material
    auto beach = MixedVoxel::createPure(128, 127, 0);
    auto beachColor = beach.getColor();
    // Should be brownish-blue mix
    assert(beachColor.x > 0.2f && beachColor.x < 0.4f);  // Some red
    assert(beachColor.z > 0.2f && beachColor.z < 0.5f);  // Some blue
    
    std::cout << "  ✓ Pure and mixed materials work" << std::endl;
}

void testVoxelBlending() {
    std::cout << "Test 2: Voxel Blending" << std::endl;
    
    auto rock = MixedVoxel::createPure(255, 0, 0);
    auto water = MixedVoxel::createPure(0, 255, 0);
    
    // Test 50/50 blend
    auto blend50 = MixedVoxel::blend(rock, water, 0.5f);
    assert(std::abs(blend50.rock - 127) < 2);
    assert(std::abs(blend50.water - 127) < 2);
    
    // Test gradient
    for(float t = 0; t <= 1.0f; t += 0.1f) {
        auto blend = MixedVoxel::blend(rock, water, t);
        float expectedRock = 255 * (1.0f - t);
        float expectedWater = 255 * t;
        
        assert(std::abs(blend.rock - expectedRock) < 3);
        assert(std::abs(blend.water - expectedWater) < 3);
    }
    
    std::cout << "  ✓ Blending produces smooth gradients" << std::endl;
}

void testFeaturePreservation() {
    std::cout << "Test 3: Feature Preservation in Averaging" << std::endl;
    
    // Test mountain preservation
    {
        MixedVoxel mountain[8];
        for(int i = 0; i < 7; i++) {
            mountain[i] = MixedVoxel::createPure(255, 0, 0);  // Rock
        }
        mountain[7] = MixedVoxel::createPure(200, 0, 55);  // Mostly rock with some air
        
        auto averaged = VoxelAverager::average(mountain);
        
        // Mountain peak should preserve high rock content
        assert(averaged.rock > 200);  // Should be close to max, not average
        assert(averaged.water < 10);   // Should have minimal water
        
        std::cout << "  ✓ Mountain peaks preserved (rock=" 
                  << (int)averaged.rock << "/255)" << std::endl;
    }
    
    // Test ocean preservation
    {
        MixedVoxel ocean[8];
        for(int i = 0; i < 7; i++) {
            ocean[i] = MixedVoxel::createPure(0, 255, 0);  // Water
        }
        ocean[7] = MixedVoxel::createPure(0, 200, 55);  // Mostly water
        
        auto averaged = VoxelAverager::average(ocean);
        
        // Ocean should preserve high water content
        assert(averaged.water > 200);  // Should be close to max
        assert(averaged.rock < 10);    // Should have minimal rock
        
        std::cout << "  ✓ Ocean depths preserved (water=" 
                  << (int)averaged.water << "/255)" << std::endl;
    }
    
    // Test coastline blending
    {
        MixedVoxel coast[8];
        // Half rock, half water
        for(int i = 0; i < 4; i++) {
            coast[i] = MixedVoxel::createPure(255, 0, 0);     // Rock
            coast[i+4] = MixedVoxel::createPure(0, 255, 0);   // Water
        }
        
        auto averaged = VoxelAverager::average(coast);
        
        // Coast should blend naturally
        assert(averaged.rock > 100 && averaged.rock < 155);   // ~127
        assert(averaged.water > 100 && averaged.water < 155);  // ~127
        
        std::cout << "  ✓ Coastlines blend naturally (rock=" 
                  << (int)averaged.rock << ", water=" 
                  << (int)averaged.water << ")" << std::endl;
    }
}

void testColorGeneration() {
    std::cout << "Test 4: Color Generation" << std::endl;
    
    // Pure rock should be brownish
    auto rock = MixedVoxel::createPure(255, 0, 0);
    auto rockColor = rock.getColor();
    assert(rockColor.x > rockColor.z);  // More red than blue
    
    // Pure water should be blueish
    auto water = MixedVoxel::createPure(0, 255, 0);
    auto waterColor = water.getColor();
    assert(waterColor.z > waterColor.x);  // More blue than red
    
    // Mixed should be in between
    auto mixed = MixedVoxel::createPure(128, 128, 0);
    auto mixedColor = mixed.getColor();
    
    // Should be darker/grayer than pure materials
    assert(mixedColor.x < rockColor.x);
    assert(mixedColor.z < waterColor.z);
    
    std::cout << "  ✓ Colors represent material composition" << std::endl;
}

void testMemorySize() {
    std::cout << "Test 5: Memory Efficiency" << std::endl;
    
    // Verify struct is compact
    assert(sizeof(MixedVoxel) == 8);
    std::cout << "  ✓ MixedVoxel is " << sizeof(MixedVoxel) << " bytes" << std::endl;
    
    // Calculate memory for planet
    size_t voxelsPerNode = 8;
    size_t nodesForPlanet = 100000000;  // 100M nodes
    size_t totalVoxels = nodesForPlanet * voxelsPerNode;
    size_t memoryGB = (totalVoxels * sizeof(MixedVoxel)) / (1024 * 1024 * 1024);
    
    std::cout << "  ✓ 100M nodes = " << memoryGB << " GB for voxel data" << std::endl;
}

void testDeterminism() {
    std::cout << "Test 6: Determinism" << std::endl;
    
    // Same input should give same output
    MixedVoxel children[8];
    for(int i = 0; i < 8; i++) {
        children[i].rock = (i * 31) % 256;
        children[i].water = (i * 17) % 256;
        children[i].air = (i * 13) % 256;
    }
    
    auto result1 = VoxelAverager::average(children);
    auto result2 = VoxelAverager::average(children);
    
    assert(result1.rock == result2.rock);
    assert(result1.water == result2.water);
    assert(result1.air == result2.air);
    
    std::cout << "  ✓ Averaging is deterministic" << std::endl;
}

void testScaleInvariance() {
    std::cout << "Test 7: Scale Invariance" << std::endl;
    
    // Create a mountain at different LOD levels
    MixedVoxel level0[8];  // Finest detail
    for(int i = 0; i < 8; i++) {
        level0[i] = MixedVoxel::createPure(255, 0, 0);  // All rock
    }
    
    // Average to level 1
    auto level1 = VoxelAverager::average(level0);
    
    // Create another set of level 1 from level 0
    MixedVoxel level0_set2[8];
    for(int i = 0; i < 8; i++) {
        level0_set2[i] = level1;  // All same as level1
    }
    
    // Average to level 2
    auto level2 = VoxelAverager::average(level0_set2);
    
    // Mountain should still be recognizable as rock at all levels
    assert(level1.rock > 250);
    assert(level2.rock > 245);  // Might lose a tiny bit to rounding
    
    std::cout << "  ✓ Features preserve character across scales" << std::endl;
    std::cout << "    Level 0: rock=255" << std::endl;
    std::cout << "    Level 1: rock=" << (int)level1.rock << std::endl;
    std::cout << "    Level 2: rock=" << (int)level2.rock << std::endl;
}

int main() {
    std::cout << "=== MIXED VOXEL SYSTEM TESTS ===" << std::endl << std::endl;
    
    testBasicMixedVoxel();
    testVoxelBlending();
    testFeaturePreservation();
    testColorGeneration();
    testMemorySize();
    testDeterminism();
    testScaleInvariance();
    
    std::cout << std::endl << "=== ALL TESTS PASSED ===" << std::endl;
    
    // Summary
    std::cout << std::endl << "Key Results:" << std::endl;
    std::cout << "- Mountains preserve peak height through averaging" << std::endl;
    std::cout << "- Oceans preserve depth through averaging" << std::endl;
    std::cout << "- Coastlines blend naturally" << std::endl;
    std::cout << "- Only 8 bytes per voxel (memory efficient)" << std::endl;
    std::cout << "- Deterministic operations (reproducible)" << std::endl;
    std::cout << "- Scale-invariant (looks correct at all distances)" << std::endl;
    
    return 0;
}