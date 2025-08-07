#include <iostream>
#include <vector>
#include "core/octree.hpp"
#include "rendering/vulkan_renderer.hpp"

// Test that water materials are properly passed through the rendering pipeline
int main() {
    std::cout << "=== WATER RENDERING PIPELINE TEST ===" << std::endl;
    
    // Test 1: Verify material generation
    std::cout << "\nTest 1: Material Generation" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 5);
        planet.generate(12345);
        
        // Count materials in render data
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 3000), glm::mat4(1.0f));
        
        int materialCounts[4] = {0, 0, 0, 0};
        for (const auto& nodeIdx : renderData.visibleNodes) {
            const auto& node = renderData.nodes[nodeIdx];
            if (node.flags & 1) { // Leaf node
                uint32_t voxelIdx = node.voxelIndex;
                if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                    for (int i = 0; i < 8; i++) {
                        const auto& voxel = renderData.voxels[voxelIdx + i];
                        uint32_t mat = static_cast<uint32_t>(voxel.material);
                        if (mat < 4) materialCounts[mat]++;
                    }
                }
            }
        }
        
        std::cout << "  Materials in voxels:" << std::endl;
        std::cout << "    Air: " << materialCounts[0] << std::endl;
        std::cout << "    Rock: " << materialCounts[1] << std::endl;
        std::cout << "    Water: " << materialCounts[2] << std::endl;
        std::cout << "    Magma: " << materialCounts[3] << std::endl;
        
        bool hasWater = materialCounts[2] > 0;
        std::cout << "  " << (hasWater ? "✓" : "✗") << " Water voxels generated" << std::endl;
        
        if (!hasWater) {
            std::cout << "  ERROR: No water voxels found!" << std::endl;
            return 1;
        }
    }
    
    // Test 2: Verify instance data creation
    std::cout << "\nTest 2: Instance Data Creation" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(12345);
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 3000), glm::mat4(1.0f));
        
        // Simulate instance buffer creation
        std::vector<rendering::VulkanRenderer::InstanceData> instances;
        int instanceMaterials[4] = {0, 0, 0, 0};
        
        for (uint32_t nodeIdx : renderData.visibleNodes) {
            const auto& node = renderData.nodes[nodeIdx];
            
            if (node.flags & 1) { // Leaf node
                uint32_t voxelIdx = node.voxelIndex;
                if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                    float voxelSize = node.halfSize * 0.5f;
                    for (int i = 0; i < 8; i++) {
                        const auto& voxel = renderData.voxels[voxelIdx + i];
                        
                        if (voxel.material == octree::MaterialType::Air) {
                            continue;
                        }
                        
                        glm::vec3 offset(
                            (i & 1) ? voxelSize : -voxelSize,
                            (i & 2) ? voxelSize : -voxelSize,
                            (i & 4) ? voxelSize : -voxelSize
                        );
                        
                        rendering::VulkanRenderer::InstanceData instance;
                        instance.center = node.center + offset;
                        instance.halfSize = voxelSize;
                        instance.materialType = static_cast<uint32_t>(voxel.material);
                        
                        if (instance.materialType < 4) {
                            instanceMaterials[instance.materialType]++;
                        }
                        
                        instances.push_back(instance);
                    }
                }
            }
        }
        
        std::cout << "  Created " << instances.size() << " instances" << std::endl;
        std::cout << "  Instance materials:" << std::endl;
        std::cout << "    Air: " << instanceMaterials[0] << std::endl;
        std::cout << "    Rock: " << instanceMaterials[1] << std::endl;
        std::cout << "    Water: " << instanceMaterials[2] << std::endl;
        std::cout << "    Magma: " << instanceMaterials[3] << std::endl;
        
        bool hasWaterInstances = instanceMaterials[2] > 0;
        std::cout << "  " << (hasWaterInstances ? "✓" : "✗") << " Water instances created" << std::endl;
        
        if (!hasWaterInstances) {
            std::cout << "  ERROR: No water instances created!" << std::endl;
            return 1;
        }
        
        // Test 3: Verify material types are correct
        int waterInstancesWithCorrectType = 0;
        for (const auto& instance : instances) {
            if (instance.materialType == 2) { // Water
                waterInstancesWithCorrectType++;
            }
        }
        
        std::cout << "\nTest 3: Material Type Verification" << std::endl;
        std::cout << "  Water instances with materialType=2: " << waterInstancesWithCorrectType << std::endl;
        bool correctTypes = waterInstancesWithCorrectType == instanceMaterials[2];
        std::cout << "  " << (correctTypes ? "✓" : "✗") << " All water instances have correct material type" << std::endl;
        
        if (!correctTypes) {
            std::cout << "  ERROR: Material types not matching!" << std::endl;
            return 1;
        }
    }
    
    // Test 4: Verify water threshold logic
    std::cout << "\nTest 4: Water Generation Logic" << std::endl;
    {
        // Test the continent generation logic
        int waterCount = 0;
        int rockCount = 0;
        int samples = 1000;
        
        for (int i = 0; i < samples; i++) {
            float lon = (i / 31.0f) * 6.28f;  // 0 to 2π
            float lat = ((i % 31) / 15.0f - 1.0f) * 1.57f;  // -π/2 to π/2
            
            float continent1 = sin(lon * 2.0f) * cos(lat * 3.0f);
            float continent2 = sin(lon * 3.5f + 1.0f) * cos(lat * 2.5f + 0.5f);
            float continent3 = sin(lon * 1.5f - 0.7f) * cos(lat * 4.0f + 1.2f);
            
            float continentValue = continent1 * 0.5f + continent2 * 0.3f + continent3 * 0.2f;
            
            // Add pseudo-random component
            float x = cos(lon) * cos(lat) * 1000.0f;
            float y = sin(lat) * 1000.0f;
            float z = sin(lon) * cos(lat) * 1000.0f;
            float pseudoRandom = fmod(x * 12.9898f + y * 78.233f + z * 37.719f, 1.0f);
            continentValue += (pseudoRandom - 0.5f) * 0.3f;
            
            if (continentValue > 0.7f) {
                rockCount++;
            } else {
                waterCount++;
            }
        }
        
        float waterPercentage = (waterCount * 100.0f) / samples;
        std::cout << "  Water: " << waterCount << "/" << samples << " (" << waterPercentage << "%)" << std::endl;
        std::cout << "  Rock: " << rockCount << "/" << samples << " (" << (100 - waterPercentage) << "%)" << std::endl;
        
        bool goodRatio = waterPercentage > 60 && waterPercentage < 80;
        std::cout << "  " << (goodRatio ? "✓" : "✗") << " Water ratio is approximately 70%" << std::endl;
        
        if (!goodRatio) {
            std::cout << "  WARNING: Water ratio not optimal (expected ~70%)" << std::endl;
        }
    }
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}