#include <iostream>
#include <vector>
#include "core/octree.hpp"

// Simple instance data structure for testing
struct TestInstanceData {
    glm::vec3 center;
    float halfSize;
    uint32_t materialType;
};

int main() {
    std::cout << "=== WATER RENDERING PIPELINE TEST (SIMPLIFIED) ===" << std::endl;
    
    // Test 1: Verify water generation in octree
    std::cout << "\nTest 1: Water Generation in Octree" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 4);
        planet.generate(12345);
        
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 3000), glm::mat4(1.0f));
        
        int voxelMaterials[4] = {0, 0, 0, 0};
        int nodeMaterials[4] = {0, 0, 0, 0};
        
        for (const auto& nodeIdx : renderData.visibleNodes) {
            const auto& node = renderData.nodes[nodeIdx];
            
            // Count dominant materials (what's currently being rendered)
            uint32_t dominantMaterial = (node.flags >> 8) & 0xFF;
            if (dominantMaterial < 4) {
                nodeMaterials[dominantMaterial]++;
            }
            
            // Count actual voxel materials
            if (node.flags & 1) { // Leaf node
                uint32_t voxelIdx = node.voxelIndex;
                if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                    for (int i = 0; i < 8; i++) {
                        const auto& voxel = renderData.voxels[voxelIdx + i];
                        uint32_t mat = static_cast<uint32_t>(voxel.material);
                        if (mat < 4) voxelMaterials[mat]++;
                    }
                }
            }
        }
        
        std::cout << "  Voxel-level materials:" << std::endl;
        std::cout << "    Air: " << voxelMaterials[0] << std::endl;
        std::cout << "    Rock: " << voxelMaterials[1] << std::endl;
        std::cout << "    Water: " << voxelMaterials[2] << std::endl;
        std::cout << "    Magma: " << voxelMaterials[3] << std::endl;
        
        std::cout << "\n  Node dominant materials (current rendering):" << std::endl;
        std::cout << "    Air: " << nodeMaterials[0] << std::endl;
        std::cout << "    Rock: " << nodeMaterials[1] << std::endl;
        std::cout << "    Water: " << nodeMaterials[2] << std::endl;
        std::cout << "    Magma: " << nodeMaterials[3] << std::endl;
        
        float voxelWaterPercent = (voxelMaterials[2] * 100.0f) / (voxelMaterials[1] + voxelMaterials[2]);
        float nodeWaterPercent = (nodeMaterials[2] * 100.0f) / (nodeMaterials[1] + nodeMaterials[2]);
        
        std::cout << "\n  Water percentage:" << std::endl;
        std::cout << "    At voxel level: " << voxelWaterPercent << "%" << std::endl;
        std::cout << "    At node level (dominant): " << nodeWaterPercent << "%" << std::endl;
        
        bool hasWaterVoxels = voxelMaterials[2] > 0;
        bool hasWaterNodes = nodeMaterials[2] > 0;
        
        std::cout << "\n  " << (hasWaterVoxels ? "✓" : "✗") << " Water voxels generated" << std::endl;
        std::cout << "  " << (hasWaterNodes ? "✓" : "WARNING:") << " Water nodes visible (dominant material)" << std::endl;
        
        if (!hasWaterVoxels) {
            std::cout << "  ERROR: No water voxels found!" << std::endl;
            return 1;
        }
    }
    
    // Test 2: Simulate per-voxel instance creation
    std::cout << "\nTest 2: Per-Voxel Instance Creation" << std::endl;
    {
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(54321);
        
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 2500), glm::mat4(1.0f));
        
        std::vector<TestInstanceData> instances;
        int instanceMaterials[4] = {0, 0, 0, 0};
        
        for (uint32_t nodeIdx : renderData.visibleNodes) {
            const auto& node = renderData.nodes[nodeIdx];
            
            if (node.flags & 1) { // Leaf node
                uint32_t voxelIdx = node.voxelIndex;
                if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                    float voxelSize = node.halfSize * 0.5f;
                    
                    for (int i = 0; i < 8; i++) {
                        const auto& voxel = renderData.voxels[voxelIdx + i];
                        
                        // Skip air voxels
                        if (voxel.material == octree::MaterialType::Air) {
                            continue;
                        }
                        
                        glm::vec3 offset(
                            (i & 1) ? voxelSize : -voxelSize,
                            (i & 2) ? voxelSize : -voxelSize,
                            (i & 4) ? voxelSize : -voxelSize
                        );
                        
                        TestInstanceData instance;
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
        
        std::cout << "  Created " << instances.size() << " instances from voxels" << std::endl;
        std::cout << "  Instance material distribution:" << std::endl;
        std::cout << "    Air: " << instanceMaterials[0] << " (should be 0)" << std::endl;
        std::cout << "    Rock: " << instanceMaterials[1] << std::endl;
        std::cout << "    Water: " << instanceMaterials[2] << std::endl;
        std::cout << "    Magma: " << instanceMaterials[3] << std::endl;
        
        float instanceWaterPercent = (instanceMaterials[2] * 100.0f) / (instanceMaterials[1] + instanceMaterials[2]);
        std::cout << "  Water percentage in instances: " << instanceWaterPercent << "%" << std::endl;
        
        bool hasWaterInstances = instanceMaterials[2] > 0;
        bool goodWaterRatio = instanceWaterPercent > 20;  // Should have significant water
        
        std::cout << "  " << (hasWaterInstances ? "✓" : "✗") << " Water instances created" << std::endl;
        std::cout << "  " << (goodWaterRatio ? "✓" : "WARNING:") << " Sufficient water ratio" << std::endl;
        
        if (!hasWaterInstances) {
            std::cout << "  ERROR: No water instances!" << std::endl;
            return 1;
        }
    }
    
    // Test 3: Verify material type values
    std::cout << "\nTest 3: Material Type Values" << std::endl;
    {
        std::cout << "  Expected material type values:" << std::endl;
        std::cout << "    Air = " << static_cast<uint32_t>(octree::MaterialType::Air) << std::endl;
        std::cout << "    Rock = " << static_cast<uint32_t>(octree::MaterialType::Rock) << std::endl;
        std::cout << "    Water = " << static_cast<uint32_t>(octree::MaterialType::Water) << std::endl;
        std::cout << "    Magma = " << static_cast<uint32_t>(octree::MaterialType::Magma) << std::endl;
        
        bool correctValues = 
            static_cast<uint32_t>(octree::MaterialType::Air) == 0 &&
            static_cast<uint32_t>(octree::MaterialType::Rock) == 1 &&
            static_cast<uint32_t>(octree::MaterialType::Water) == 2 &&
            static_cast<uint32_t>(octree::MaterialType::Magma) == 3;
        
        std::cout << "  " << (correctValues ? "✓" : "✗") << " Material type values match expected" << std::endl;
        
        if (!correctValues) {
            std::cout << "  ERROR: Material type enum values don't match!" << std::endl;
            return 1;
        }
    }
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Water IS being generated at the voxel level." << std::endl;
    std::cout << "Per-voxel rendering should show water." << std::endl;
    std::cout << "If water is not visible, check:" << std::endl;
    std::cout << "  1. Shader material type uniform binding" << std::endl;
    std::cout << "  2. Vertex attribute layout for materialType" << std::endl;
    std::cout << "  3. Instance buffer memory layout" << std::endl;
    
    return 0;
}