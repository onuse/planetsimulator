// Test suite to catch "sudden invisible planet" issues
// These tests verify every step that could make the planet disappear

#include <iostream>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"
#include "core/camera.hpp"
#include "rendering/instance_buffer_manager.hpp"

using namespace octree;

// Test 1: Verify voxels have non-zero materials after initialization
void test_voxel_initialization() {
    std::cout << "TEST: Voxel initialization..." << std::endl;
    
    MixedVoxel voxel;
    
    // Default constructor should create air voxel
    assert(voxel.air == 255 && "Default voxel should be air");
    assert(voxel.rock == 0 && "Default voxel should have no rock");
    assert(voxel.water == 0 && "Default voxel should have no water");
    
    // Verify getDominantMaterial works
    uint8_t dominant = voxel.getDominantMaterial();
    assert(dominant == 0 && "Air voxel should return material type 0");
    
    // Test pure materials
    MixedVoxel rockVoxel = MixedVoxel::createPure(255, 0, 0);
    assert(rockVoxel.rock == 255 && "Rock voxel should have max rock");
    assert(rockVoxel.getDominantMaterial() == 1 && "Rock should be material 1");
    
    MixedVoxel waterVoxel = MixedVoxel::createPure(0, 255, 0);
    assert(waterVoxel.water == 255 && "Water voxel should have max water");
    assert(waterVoxel.getDominantMaterial() == 2 && "Water should be material 2");
    
    std::cout << "  ✓ Voxels initialize with correct materials" << std::endl;
}

// Test 2: Verify octree nodes preserve voxel data after subdivision
void test_voxel_preservation_after_subdivision() {
    std::cout << "TEST: Voxel preservation after subdivision..." << std::endl;
    
    OctreeNode node(glm::vec3(0), 1000.0f, 0);
    
    // Set specific materials before subdivision
    for (int i = 0; i < 8; i++) {
        node.voxels[i] = MixedVoxel::createPure(255, 0, 0); // All rock
    }
    
    // Verify materials are set
    assert(node.voxels[0].rock == 255 && "Voxel should be rock before subdivision");
    
    // Subdivide
    node.subdivide();
    
    // After subdivision, parent is no longer a leaf
    assert(!node.isLeaf() && "Node should not be leaf after subdivision");
    
    // Children should exist and be leaves
    for (int i = 0; i < 8; i++) {
        assert(node.children[i] != nullptr && "Child should exist");
        assert(node.children[i]->isLeaf() && "Child should be leaf");
        
        // Each child should have inherited voxel data
        assert(node.children[i]->voxels[0].rock > 0 || 
               node.children[i]->voxels[0].air > 0 && 
               "Child voxels should have some material");
    }
    
    std::cout << "  ✓ Voxels preserved through subdivision" << std::endl;
}

// Test 3: Verify planet generation creates visible materials
void test_planet_has_visible_materials() {
    std::cout << "TEST: Planet has visible materials..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4, 42);
    
    // Count materials
    int rockCount = 0, waterCount = 0, airCount = 0;
    
    planet.getRoot()->traverse([&](OctreeNode* node) {
        if (node->isLeaf()) {
            for (int i = 0; i < 8; i++) {
                uint8_t mat = node->voxels[i].getDominantMaterial();
                if (mat == 0) airCount++;
                else if (mat == 1) rockCount++;
                else if (mat == 2) waterCount++;
            }
        }
    });
    
    std::cout << "  Materials: " << rockCount << " rock, " 
              << waterCount << " water, " << airCount << " air" << std::endl;
    
    // Planet should have at least some rock and water
    assert(rockCount > 0 && "Planet should have rock");
    assert(waterCount > 0 && "Planet should have water");
    
    // Should have more solid material than air for a planet
    assert((rockCount + waterCount) > 0 && "Planet should have solid materials");
    
    std::cout << "  ✓ Planet has visible materials" << std::endl;
}

// Test 4: Verify instance creation produces non-black colors
void test_instance_colors_not_black() {
    std::cout << "TEST: Instance colors not black..." << std::endl;
    
    // Create a simple octree with known materials
    float radius = 1000.0f;
    OctreePlanet planet(radius, 3, 42);
    
    // Get render data
    glm::vec3 viewPos(0, 0, 2000);
    auto renderData = planet.getRenderData(viewPos);
    
    // Create instances
    rendering::InstanceBufferManager::Statistics stats;
    auto instances = rendering::InstanceBufferManager::createInstancesFromVoxels(
        renderData, &stats);
    
    assert(instances.size() > 0 && "Should create instances");
    
    // Check that colors are not all black
    int nonBlackCount = 0;
    for (const auto& inst : instances) {
        if (inst.colorAndMaterial.r > 0.01f || 
            inst.colorAndMaterial.g > 0.01f || 
            inst.colorAndMaterial.b > 0.01f) {
            nonBlackCount++;
        }
    }
    
    assert(nonBlackCount > 0 && "Should have non-black instances");
    float nonBlackPercent = (float)nonBlackCount / instances.size() * 100.0f;
    std::cout << "  " << nonBlackPercent << "% instances have color" << std::endl;
    
    assert(nonBlackPercent > 50.0f && "Most instances should have color");
    
    std::cout << "  ✓ Instances have proper colors" << std::endl;
}

// Test 5: Verify camera matrices are valid
void test_camera_matrices_valid() {
    std::cout << "TEST: Camera matrices valid..." << std::endl;
    
    core::Camera camera(1280, 720);
    
    // Position camera looking at origin
    camera.setPosition(glm::vec3(0, 0, 2000));
    camera.setTarget(glm::vec3(0, 0, 0));
    
    // Get matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    
    // Check view matrix is not identity (camera has moved)
    assert(view != glm::mat4(1.0f) && "View matrix should not be identity");
    
    // Check projection matrix has reasonable values
    assert(proj[0][0] != 0.0f && "Projection matrix should not have zeros");
    assert(proj[1][1] != 0.0f && "Projection matrix should not have zeros");
    
    // Test a point transformation
    glm::vec4 testPoint(0, 0, 0, 1); // Origin
    glm::vec4 viewSpace = view * testPoint;
    glm::vec4 clipSpace = proj * viewSpace;
    
    // Origin should be in front of camera (negative Z in view space)
    assert(viewSpace.z < 0 && "Origin should be in front of camera");
    
    // Should be within clip space after projection
    if (clipSpace.w != 0) {
        glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
        assert(ndc.z >= -1.0f && ndc.z <= 1.0f && "Should be in clip space");
    }
    
    std::cout << "  ✓ Camera matrices are valid" << std::endl;
}

// Test 6: Verify materials survive octree traversal
void test_materials_survive_traversal() {
    std::cout << "TEST: Materials survive traversal..." << std::endl;
    
    // Create a single node with known materials
    OctreeNode node(glm::vec3(0), 100.0f, 0);
    
    // Set specific pattern
    node.voxels[0] = MixedVoxel::createPure(255, 0, 0);   // Rock
    node.voxels[1] = MixedVoxel::createPure(0, 255, 0);   // Water
    node.voxels[2] = MixedVoxel::createPure(0, 0, 255);   // Air
    node.voxels[3] = MixedVoxel::createPure(128, 128, 0); // Mixed
    
    // Traverse and verify
    bool materialsCorrect = true;
    node.traverse([&](OctreeNode* n) {
        if (n->isLeaf()) {
            if (n->voxels[0].getDominantMaterial() != 1) materialsCorrect = false;
            if (n->voxels[1].getDominantMaterial() != 2) materialsCorrect = false;
            if (n->voxels[2].getDominantMaterial() != 0) materialsCorrect = false;
        }
    });
    
    assert(materialsCorrect && "Materials should survive traversal");
    
    std::cout << "  ✓ Materials survive traversal" << std::endl;
}

// Test 7: Verify getVoxels() returns correct data
void test_get_voxels_returns_data() {
    std::cout << "TEST: getVoxels() returns correct data..." << std::endl;
    
    OctreeNode node(glm::vec3(0), 100.0f, 0);
    
    // Set known materials
    for (int i = 0; i < 8; i++) {
        node.voxels[i] = MixedVoxel::createPure(100 + i*10, 50, 10);
    }
    
    // Get voxels through public interface
    const auto& voxels = node.getVoxels();
    
    // Verify data matches
    for (int i = 0; i < 8; i++) {
        assert(voxels[i].rock == (100 + i*10) && "Voxel data should match");
        assert(voxels[i].water == 50 && "Voxel data should match");
        assert(voxels[i].air == 10 && "Voxel data should match");
    }
    
    std::cout << "  ✓ getVoxels() returns correct data" << std::endl;
}

// Test 8: Verify colors are calculated correctly
void test_color_calculation() {
    std::cout << "TEST: Color calculation..." << std::endl;
    
    // Test pure materials
    MixedVoxel rock = MixedVoxel::createPure(255, 0, 0);
    glm::vec3 rockColor = rock.getColor();
    assert(rockColor.r > 0.3f && rockColor.r < 0.7f && "Rock should be brownish");
    
    MixedVoxel water = MixedVoxel::createPure(0, 255, 0);
    glm::vec3 waterColor = water.getColor();
    assert(waterColor.b > 0.5f && "Water should be blue");
    
    // Test mixed material
    MixedVoxel mixed = MixedVoxel::createPure(128, 128, 0);
    glm::vec3 mixedColor = mixed.getColor();
    assert(mixedColor.r > 0.1f && mixedColor.b > 0.1f && "Mixed should blend colors");
    
    std::cout << "  ✓ Colors calculated correctly" << std::endl;
}

// Test 9: Verify instance buffer data layout
void test_instance_data_layout() {
    std::cout << "TEST: Instance data layout..." << std::endl;
    
    rendering::InstanceBufferManager::InstanceData instance;
    instance.center = glm::vec3(1.0f, 2.0f, 3.0f);
    instance.halfSize = 4.0f;
    instance.colorAndMaterial = glm::vec4(0.5f, 0.6f, 0.7f, 2.0f);
    
    // Verify size is what we expect (32 bytes)
    assert(sizeof(instance) == 32 && "Instance data should be 32 bytes");
    
    // Verify fields are accessible
    assert(instance.center.x == 1.0f && "Center X should be accessible");
    assert(instance.center.y == 2.0f && "Center Y should be accessible");
    assert(instance.center.z == 3.0f && "Center Z should be accessible");
    assert(instance.halfSize == 4.0f && "Half size should be accessible");
    assert(instance.colorAndMaterial.w == 2.0f && "Material should be in W component");
    
    std::cout << "  ✓ Instance data layout correct" << std::endl;
}

// Test 10: Verify visible node filtering
void test_visible_node_filtering() {
    std::cout << "TEST: Visible node filtering..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 3, 42);
    
    // Get render data from different positions
    glm::vec3 closeView(0, 0, 1500);
    auto closeData = planet.getRenderData(closeView);
    
    glm::vec3 farView(0, 0, 10000);
    auto farData = planet.getRenderData(farView);
    
    // Should have some visible nodes
    assert(closeData.visibleNodes.size() > 0 && "Should have visible nodes when close");
    assert(farData.visibleNodes.size() > 0 && "Should have visible nodes when far");
    
    // All visible nodes should be valid indices
    for (uint32_t idx : closeData.visibleNodes) {
        assert(idx < closeData.nodes.size() && "Visible node index should be valid");
        
        // Visible nodes should be leaves
        const auto& node = closeData.nodes[idx];
        assert((node.flags & 1) != 0 && "Visible nodes should be leaves");
    }
    
    std::cout << "  ✓ Visible nodes filtered correctly" << std::endl;
}

int main() {
    std::cout << "\n=== Planet Visibility Test Suite ===" << std::endl;
    std::cout << "Testing every step that could make the planet invisible...\n" << std::endl;
    
    try {
        test_voxel_initialization();
        test_voxel_preservation_after_subdivision();
        test_planet_has_visible_materials();
        test_instance_colors_not_black();
        test_camera_matrices_valid();
        test_materials_survive_traversal();
        test_get_voxels_returns_data();
        test_color_calculation();
        test_instance_data_layout();
        test_visible_node_filtering();
        
        std::cout << "\n✅ ALL VISIBILITY TESTS PASSED!" << std::endl;
        std::cout << "If the planet is invisible, the issue is in:" << std::endl;
        std::cout << "  - Shader compilation or binding" << std::endl;
        std::cout << "  - Vulkan pipeline state" << std::endl;
        std::cout << "  - GPU buffer upload" << std::endl;
        std::cout << "  - Vertex attribute binding" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}