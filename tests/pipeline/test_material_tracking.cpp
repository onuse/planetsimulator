#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <cstring>
#include <cstdio>
#include "core/octree.hpp"
#include "rendering/instance_buffer_manager.hpp"

// Test that tracks material types through the entire pipeline
class MaterialPipelineTracker {
public:
    struct StageResult {
        std::string stageName;
        int waterCount = 0;
        int rockCount = 0;
        int airCount = 0;
        int magmaCount = 0;
        bool passed = false;
        std::string error;
    };
    
private:
    std::vector<StageResult> results;
    
public:
    // Stage 1: Voxel Generation
    StageResult testVoxelGeneration() {
        StageResult result{"Voxel Generation"};
        
        octree::OctreePlanet planet(1000.0f, 4);
        planet.generate(12345);
        
        // Count materials at voxel level
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 3000), glm::mat4(1.0f));
        
        for (const auto& nodeIdx : renderData.visibleNodes) {
            const auto& node = renderData.nodes[nodeIdx];
            if (node.flags & 1) { // Leaf node
                uint32_t voxelIdx = node.voxelIndex;
                if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                    for (int i = 0; i < 8; i++) {
                        const auto& voxel = renderData.voxels[voxelIdx + i];
                        // Check dominant material using new API
                        core::MaterialID mat = voxel.getDominantMaterialID();
                        if (mat == core::MaterialID::Air || mat == core::MaterialID::Vacuum) {
                            result.airCount++;
                        } else if (mat == core::MaterialID::Rock || mat == core::MaterialID::Granite || mat == core::MaterialID::Basalt) {
                            result.rockCount++;
                        } else if (mat == core::MaterialID::Water) {
                            result.waterCount++;
                        } else if (mat == core::MaterialID::Lava) {
                            result.magmaCount++;
                        } else {
                            // Default to rock for other solid materials
                            result.rockCount++;
                        }
                    }
                }
            }
        }
        
        result.passed = result.waterCount > 0;
        if (!result.passed) {
            result.error = "No water voxels generated";
        }
        
        return result;
    }
    
    // Stage 2: Node Material Encoding
    StageResult testNodeEncoding() {
        StageResult result{"Node Material Encoding"};
        
        octree::OctreePlanet planet(1000.0f, 4);
        planet.generate(12345);
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 3000), glm::mat4(1.0f));
        
        for (const auto& node : renderData.nodes) {
            if (node.flags & 1) { // Leaf node
                uint32_t material = (node.flags >> 8) & 0xFF;
                switch (material) {
                    case 0: result.airCount++; break;
                    case 1: result.rockCount++; break;
                    case 2: result.waterCount++; break;
                    case 3: result.magmaCount++; break;
                }
            }
        }
        
        result.passed = result.waterCount > 0 || result.rockCount > 0;
        if (!result.passed) {
            result.error = "No materials encoded in node flags";
        }
        
        return result;
    }
    
    // Stage 3: Instance Creation
    StageResult testInstanceCreation() {
        StageResult result{"Instance Creation"};
        
        octree::OctreePlanet planet(1000.0f, 4);
        planet.generate(12345);
        auto renderData = planet.prepareRenderData(glm::vec3(0, 0, 3000), glm::mat4(1.0f));
        
        rendering::InstanceBufferManager::Statistics stats;
        auto instances = rendering::InstanceBufferManager::createInstancesFromVoxels(renderData, &stats);
        
        result.airCount = stats.airCount;
        result.rockCount = stats.rockCount;
        result.waterCount = stats.waterCount;
        result.magmaCount = stats.magmaCount;
        
        result.passed = stats.waterCount > 0;
        if (!result.passed) {
            result.error = "No water instances created";
        }
        
        return result;
    }
    
    // Stage 4: Buffer Memory Layout
    StageResult testBufferLayout() {
        StageResult result{"Buffer Memory Layout"};
        
        // Create test instances
        std::vector<rendering::InstanceBufferManager::InstanceData> instances;
        
        // Add known test data
        rendering::InstanceBufferManager::InstanceData waterInstance;
        waterInstance.center = glm::vec3(100, 200, 300);
        waterInstance.halfSize = 50.0f;
        waterInstance.colorAndMaterial = glm::vec4(0.0f, 0.3f, 0.7f, 2.0f); // xyz=color, w=material
        instances.push_back(waterInstance);
        
        rendering::InstanceBufferManager::InstanceData rockInstance;
        rockInstance.center = glm::vec3(400, 500, 600);
        rockInstance.halfSize = 60.0f;
        rockInstance.colorAndMaterial = glm::vec4(0.5f, 0.4f, 0.3f, 1.0f); // xyz=color, w=material
        instances.push_back(rockInstance);
        
        // Simulate buffer copy
        std::vector<uint8_t> buffer(instances.size() * sizeof(rendering::InstanceBufferManager::InstanceData));
        memcpy(buffer.data(), instances.data(), buffer.size());
        
        // Read back and verify
        auto* readBack = reinterpret_cast<rendering::InstanceBufferManager::InstanceData*>(buffer.data());
        
        for (size_t i = 0; i < instances.size(); i++) {
            int material = static_cast<int>(readBack[i].colorAndMaterial.w + 0.5f);
            switch (material) {
                case 1: result.rockCount++; break;
                case 2: result.waterCount++; break;
            }
            
            if (readBack[i].colorAndMaterial.w != instances[i].colorAndMaterial.w) {
                result.error = "Material type corrupted in buffer copy";
                result.passed = false;
                return result;
            }
        }
        
        result.passed = (result.waterCount == 1 && result.rockCount == 1);
        if (!result.passed) {
            result.error = "Buffer copy failed to preserve material types";
        }
        
        return result;
    }
    
    // Stage 5: Attribute Offset Validation
    StageResult testAttributeOffsets() {
        StageResult result{"Attribute Offsets"};
        
        size_t expectedOffsets[] = {0, 12, 16};
        size_t actualOffsets[] = {
            offsetof(rendering::InstanceBufferManager::InstanceData, center),
            offsetof(rendering::InstanceBufferManager::InstanceData, halfSize),
            offsetof(rendering::InstanceBufferManager::InstanceData, colorAndMaterial)
        };
        
        bool offsetsMatch = true;
        for (int i = 0; i < 3; i++) {
            if (expectedOffsets[i] != actualOffsets[i]) {
                offsetsMatch = false;
                result.error = "Offset mismatch at field " + std::to_string(i);
                break;
            }
        }
        
        result.passed = offsetsMatch && sizeof(rendering::InstanceBufferManager::InstanceData) == 32;
        if (!result.passed && result.error.empty()) {
            result.error = "Struct size is not 32 bytes";
        }
        
        // This stage doesn't track materials, just validates structure
        return result;
    }
    
    void runAllTests() {
        std::cout << "=== MATERIAL PIPELINE TRACKING TEST ===" << std::endl;
        std::cout << "Tracking water materials through each pipeline stage...\n" << std::endl;
        
        results.clear();
        results.push_back(testVoxelGeneration());
        results.push_back(testNodeEncoding());
        results.push_back(testInstanceCreation());
        results.push_back(testBufferLayout());
        results.push_back(testAttributeOffsets());
        
        // Print results table
        std::cout << "Stage                    | Water | Rock | Air | Magma | Status" << std::endl;
        std::cout << "-------------------------|-------|------|-----|-------|--------" << std::endl;
        
        for (const auto& result : results) {
            printf("%-24s | %5d | %4d | %3d | %5d | %s\n",
                   result.stageName.c_str(),
                   result.waterCount,
                   result.rockCount,
                   result.airCount,
                   result.magmaCount,
                   result.passed ? "✓" : "✗");
            
            if (!result.passed) {
                std::cout << "  ERROR: " << result.error << std::endl;
            }
        }
        
        // Find where water disappears
        std::cout << "\n=== ANALYSIS ===" << std::endl;
        
        int lastWaterStage = -1;
        for (size_t i = 0; i < results.size(); i++) {
            if (results[i].waterCount > 0) {
                lastWaterStage = static_cast<int>(i);
            }
        }
        
        if (lastWaterStage >= 0 && lastWaterStage < static_cast<int>(results.size()) - 1) {
            std::cout << "Water DISAPPEARS between stage " << lastWaterStage + 1 
                      << " (" << results[lastWaterStage].stageName << ")"
                      << " and stage " << lastWaterStage + 2 
                      << " (" << results[lastWaterStage + 1].stageName << ")" << std::endl;
        } else if (lastWaterStage == static_cast<int>(results.size()) - 1) {
            std::cout << "Water is present through all CPU-side stages." << std::endl;
            std::cout << "The issue is likely in GPU vertex attribute binding or shader." << std::endl;
        } else {
            std::cout << "No water found in any stage!" << std::endl;
        }
    }
};

int main() {
    MaterialPipelineTracker tracker;
    tracker.runAllTests();
    return 0;
}