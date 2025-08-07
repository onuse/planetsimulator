// Test that simulates the EXACT rendering path the actual program uses
// This should catch why the planet is still black

#include <iostream>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Simulate the exact GPU upload process
void test_exact_gpu_upload_path() {
    std::cout << "TEST: Simulating exact GPU upload path..." << std::endl;
    
    // Create Earth-scale planet like the real program
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 7);
    planet.generate(42);
    
    // Camera position like in the real program (from vulkan_renderer.cpp)
    glm::vec3 viewPos(0, 0, earthRadius * 3.0f); // Default camera distance
    
    // Build proper view-projection matrix
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1920.0f/1080.0f, 0.1f, 100000000.0f);
    glm::mat4 viewProj = proj * view;
    
    // Call prepareRenderData
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    std::cout << "  RenderData: " << renderData.nodes.size() << " nodes, " 
              << renderData.voxels.size() << " voxels" << std::endl;
    
    // Analyze the NODE FLAGS (this is what determines material on GPU)
    int airNodes = 0, rockNodes = 0, waterNodes = 0, magmaNodes = 0;
    
    for (const auto& node : renderData.nodes) {
        bool isLeaf = (node.flags & 1) != 0;
        if (isLeaf) {
            uint32_t material = (node.flags >> 8) & 0xFF;
            if (material == 0) airNodes++;
            else if (material == 1) rockNodes++;
            else if (material == 2) waterNodes++;
            else if (material == 3) magmaNodes++;
            
            // Debug first few air nodes
            if (material == 0 && airNodes <= 3) {
                std::cout << "  Air node at (" << node.center.x << ", " 
                          << node.center.y << ", " << node.center.z 
                          << ") flags=0x" << std::hex << node.flags << std::dec << std::endl;
                
                // Check the voxels for this node
                if (node.voxelIndex != 0xFFFFFFFF && 
                    node.voxelIndex + 8 <= renderData.voxels.size()) {
                    std::cout << "    Voxels: ";
                    for (int i = 0; i < 8; i++) {
                        auto& voxel = renderData.voxels[node.voxelIndex + i];
                        std::cout << (int)voxel.getDominantMaterial() << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n  Node material distribution:" << std::endl;
    std::cout << "    Air nodes: " << airNodes << std::endl;
    std::cout << "    Rock nodes: " << rockNodes << std::endl;
    std::cout << "    Water nodes: " << waterNodes << std::endl;
    std::cout << "    Magma nodes: " << magmaNodes << std::endl;
    
    // Also check voxel materials
    int voxelAir = 0, voxelRock = 0, voxelWater = 0;
    for (const auto& voxel : renderData.voxels) {
        uint8_t mat = voxel.getDominantMaterial();
        if (mat == 0) voxelAir++;
        else if (mat == 1) voxelRock++;
        else if (mat == 2) voxelWater++;
    }
    
    std::cout << "\n  Voxel material distribution:" << std::endl;
    std::cout << "    Air voxels: " << voxelAir << std::endl;
    std::cout << "    Rock voxels: " << voxelRock << std::endl;
    std::cout << "    Water voxels: " << voxelWater << std::endl;
    
    // CRITICAL TEST: Node materials should match voxel materials
    float nodeAirPercent = (float)airNodes / renderData.nodes.size() * 100.0f;
    float voxelAirPercent = (float)voxelAir / renderData.voxels.size() * 100.0f;
    
    std::cout << "\n  Air percentage:" << std::endl;
    std::cout << "    In nodes: " << nodeAirPercent << "%" << std::endl;
    std::cout << "    In voxels: " << voxelAirPercent << "%" << std::endl;
    
    // The bug: nodes show air but voxels don't!
    if (nodeAirPercent > 50.0f && voxelAirPercent < 20.0f) {
        std::cerr << "\n  ❌ BUG DETECTED: Nodes are marked as Air despite having non-air voxels!" << std::endl;
        std::cerr << "     This is why the planet renders black!" << std::endl;
        assert(false && "Node flags not matching voxel materials");
    }
    
    // Nodes should have similar material distribution as voxels
    assert(std::abs(nodeAirPercent - voxelAirPercent) < 30.0f && 
           "Node and voxel materials should be similar");
    
    std::cout << "  ✓ Node flags correctly represent voxel materials" << std::endl;
}

// Test the VoxelAverager that determines node material
void test_voxel_averager() {
    std::cout << "\nTEST: VoxelAverager for node material determination..." << std::endl;
    
    // Test case 1: All air voxels
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(0, 0, 255); // Pure air
        }
        
        MixedVoxel avg = VoxelAverager::average(voxels);
        uint8_t dominant = avg.getDominantMaterial();
        std::cout << "  All air voxels -> dominant material: " << (int)dominant << std::endl;
        assert(dominant == 0 && "All air should give air");
    }
    
    // Test case 2: All rock voxels
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(255, 0, 0); // Pure rock
        }
        
        MixedVoxel avg = VoxelAverager::average(voxels);
        uint8_t dominant = avg.getDominantMaterial();
        std::cout << "  All rock voxels -> dominant material: " << (int)dominant << std::endl;
        assert(dominant == 1 && "All rock should give rock");
    }
    
    // Test case 3: Mixed rock and water (like actual planet surface)
    {
        MixedVoxel voxels[8];
        for (int i = 0; i < 4; i++) {
            voxels[i] = MixedVoxel::createPure(255, 0, 0); // Rock
        }
        for (int i = 4; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(0, 255, 0); // Water
        }
        
        MixedVoxel avg = VoxelAverager::average(voxels);
        uint8_t dominant = avg.getDominantMaterial();
        std::cout << "  50% rock, 50% water -> dominant material: " << (int)dominant << std::endl;
        // Should be either rock or water, not air!
        assert(dominant != 0 && "Mixed rock/water should not give air");
    }
    
    // Test case 4: What the actual program might be seeing
    {
        MixedVoxel voxels[8];
        // Initialize as the octree constructor does
        for (int i = 0; i < 8; i++) {
            voxels[i] = MixedVoxel::createPure(0, 0, 255); // Constructor default
            voxels[i].temperature = 10;
            voxels[i].pressure = 0;
        }
        
        // Now set some to rock/water (like generateTestSphere does)
        voxels[0] = MixedVoxel::createPure(255, 0, 0); // Rock
        voxels[1] = MixedVoxel::createPure(0, 255, 0); // Water
        
        MixedVoxel avg = VoxelAverager::average(voxels);
        uint8_t dominant = avg.getDominantMaterial();
        std::cout << "  2 materials, 6 air -> dominant material: " << (int)dominant << std::endl;
        
        // This might be the issue - mostly air voxels!
        if (dominant == 0) {
            std::cout << "    WARNING: Sparse materials result in Air dominant!" << std::endl;
        }
    }
    
    std::cout << "  ✓ VoxelAverager tests complete" << std::endl;
}

// Test at different camera distances
void test_camera_distances() {
    std::cout << "\nTEST: Different camera distances..." << std::endl;
    
    float earthRadius = 6371000.0f;
    OctreePlanet planet(earthRadius, 5); // Smaller depth for speed
    planet.generate(42);
    
    float distances[] = {
        earthRadius * 1.5f,  // Close
        earthRadius * 3.0f,  // Default
        earthRadius * 10.0f  // Far
    };
    
    for (float dist : distances) {
        glm::vec3 viewPos(0, 0, dist);
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, dist * 10.0f);
        glm::mat4 viewProj = proj * view;
        
        auto renderData = planet.prepareRenderData(viewPos, viewProj);
        
        // Count node materials
        int airNodes = 0;
        for (const auto& node : renderData.nodes) {
            if (node.flags & 1) { // Leaf
                uint32_t mat = (node.flags >> 8) & 0xFF;
                if (mat == 0) airNodes++;
            }
        }
        
        float airPercent = (float)airNodes / renderData.nodes.size() * 100.0f;
        
        std::cout << "  Distance " << dist/earthRadius << "x radius:" << std::endl;
        std::cout << "    Nodes: " << renderData.nodes.size() << std::endl;
        std::cout << "    Air nodes: " << airPercent << "%" << std::endl;
        
        if (airPercent > 80.0f) {
            std::cout << "    ⚠️  Too many air nodes - planet will be invisible!" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== Actual Rendering Path Test ===" << std::endl;
    std::cout << "Testing the exact path the real program uses\n" << std::endl;
    
    try {
        test_exact_gpu_upload_path();
        test_voxel_averager();
        test_camera_distances();
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        std::cerr << "\nThis test failure explains why the planet is black!" << std::endl;
        return 1;
    }
    
    return 0;
}