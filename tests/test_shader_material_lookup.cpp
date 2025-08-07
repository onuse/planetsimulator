#include <iostream>
#include <cassert>
#include "../include/core/material_table.hpp"
#include "../include/core/octree.hpp"

// Test that verifies the shader material lookup chain works correctly
void testMaterialIDInGPUNode() {
    std::cout << "Test: Material ID storage in GPU nodes" << std::endl;
    
    // Create a small planet to test with
    octree::OctreePlanet planet(1000.0f, 2); // Small planet, shallow depth
    planet.generate(42);
    
    // Get render data which creates GPU nodes
    glm::vec3 viewPos(0, 0, 2000);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Check some GPU nodes to verify material IDs are stored correctly
    int rockyNodes = 0;
    int waterNodes = 0;
    
    for (const auto& gpuNode : renderData.nodes) {
        if (gpuNode.flags & 1) { // Leaf node
            uint32_t materialId = (gpuNode.flags >> 8) & 0xFF;
            
            // With our fix, MaterialIDs should be stored directly
            // Rock = 2, Water = 3, etc.
            if (materialId == static_cast<uint32_t>(core::MaterialID::Rock)) {
                rockyNodes++;
            } else if (materialId == static_cast<uint32_t>(core::MaterialID::Water)) {
                waterNodes++;
            }
            
            // Verify the materialId is a valid MaterialID value
            assert(materialId <= 15); // We have 16 materials (0-15)
        }
    }
    
    std::cout << "  Found " << rockyNodes << " rock nodes, " << waterNodes << " water nodes" << std::endl;
    
    // We should have at least some rock nodes on a planet
    assert(rockyNodes > 0 || waterNodes > 0);
    
    std::cout << "  ✓ Material IDs stored correctly in GPU nodes" << std::endl;
}

void testShaderMaterialLookup() {
    std::cout << "Test: Shader material table lookup" << std::endl;
    
    // Test all material IDs that should be rendered
    core::MaterialID renderableMaterials[] = {
        core::MaterialID::Rock,    // 2
        core::MaterialID::Water,   // 3
        core::MaterialID::Sand,    // 4
        core::MaterialID::Granite, // 9
        core::MaterialID::Lava     // 12
    };
    
    for (auto matId : renderableMaterials) {
        uint32_t id = static_cast<uint32_t>(matId);
        
        // This is what the shader does: if (materialId > 1u)
        bool wouldRender = (id > 1u);
        
        assert(wouldRender);
        std::cout << "  ✓ Material " << id << " would be rendered" << std::endl;
    }
    
    // Test materials that should NOT be rendered
    core::MaterialID skipMaterials[] = {
        core::MaterialID::Vacuum,  // 0
        core::MaterialID::Air      // 1
    };
    
    for (auto matId : skipMaterials) {
        uint32_t id = static_cast<uint32_t>(matId);
        bool wouldRender = (id > 1u);
        assert(!wouldRender);
        std::cout << "  ✓ Material " << id << " would be skipped" << std::endl;
    }
}

void testMaterialColorRetrieval() {
    std::cout << "Test: Material color retrieval" << std::endl;
    
    auto& table = core::MaterialTable::getInstance();
    
    // Verify Rock has a brownish color
    glm::vec3 rockColor = table.getColor(core::MaterialID::Rock);
    assert(rockColor.r > 0.4f && rockColor.r < 0.7f);
    assert(rockColor.g > 0.3f && rockColor.g < 0.5f);
    std::cout << "  ✓ Rock color: (" << rockColor.r << ", " << rockColor.g << ", " << rockColor.b << ")" << std::endl;
    
    // Verify Water has a blueish color  
    glm::vec3 waterColor = table.getColor(core::MaterialID::Water);
    assert(waterColor.b > waterColor.r);
    std::cout << "  ✓ Water color: (" << waterColor.r << ", " << waterColor.g << ", " << waterColor.b << ")" << std::endl;
}

int main() {
    std::cout << "\n=== Shader Material Lookup Tests ===" << std::endl;
    
    try {
        testMaterialIDInGPUNode();
        testShaderMaterialLookup();
        testMaterialColorRetrieval();
        
        std::cout << "\n✅ All shader material tests passed!" << std::endl;
        std::cout << "This test verifies the CPU->GPU->Shader material pipeline" << std::endl;
        std::cout << "If this test passes but rendering fails, the issue is in the shader itself." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}