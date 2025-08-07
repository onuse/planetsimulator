#include <iostream>
#include <vector>
#include <cstring>
#include "rendering/instance_buffer_manager.hpp"

// Test that directly creates instances with known material values
void testMaterialPassthrough() {
    std::cout << "\n=== Material Passthrough Test ===" << std::endl;
    
    using InstanceData = rendering::InstanceBufferManager::InstanceData;
    std::vector<InstanceData> testInstances;
    
    // Create a grid of instances with different materials
    float spacing = 10.0f;
    int gridSize = 4;
    
    for (int x = 0; x < gridSize; x++) {
        for (int y = 0; y < gridSize; y++) {
            for (int z = 0; z < gridSize; z++) {
                InstanceData instance;
                instance.center = glm::vec3(x * spacing, y * spacing, z * spacing);
                instance.halfSize = 4.0f;
                
                // Alternate between materials in a pattern
                int index = x + y * gridSize + z * gridSize * gridSize;
                float material = static_cast<float>(index % 4); // 0, 1, 2, 3
                instance.materialType = material;
                
                // Set debug colors
                switch (static_cast<int>(material)) {
                    case 0: // Air (shouldn't render)
                        instance.color = glm::vec3(1, 0, 0); // Red
                        break;
                    case 1: // Rock
                        instance.color = glm::vec3(0, 1, 0); // Green
                        break;
                    case 2: // Water
                        instance.color = glm::vec3(0, 0, 1); // Blue
                        break;
                    case 3: // Magma
                        instance.color = glm::vec3(1, 1, 0); // Yellow
                        break;
                }
                
                testInstances.push_back(instance);
                
                if (material == 2.0f) {
                    std::cout << "Created WATER instance at index " << testInstances.size() - 1
                              << " with materialType=" << material << std::endl;
                }
            }
        }
    }
    
    // Verify the data
    int counts[4] = {0, 0, 0, 0};
    for (const auto& inst : testInstances) {
        int matInt = static_cast<int>(inst.materialType + 0.5f);
        if (matInt >= 0 && matInt < 4) {
            counts[matInt]++;
        }
    }
    
    std::cout << "Test instances created:" << std::endl;
    std::cout << "  Air:   " << counts[0] << std::endl;
    std::cout << "  Rock:  " << counts[1] << std::endl;
    std::cout << "  Water: " << counts[2] << std::endl;
    std::cout << "  Magma: " << counts[3] << std::endl;
    
    // Now check memory layout
    std::cout << "\nMemory layout check:" << std::endl;
    std::cout << "sizeof(InstanceData) = " << sizeof(InstanceData) << " bytes" << std::endl;
    std::cout << "offsetof(center) = " << offsetof(InstanceData, center) << std::endl;
    std::cout << "offsetof(halfSize) = " << offsetof(InstanceData, halfSize) << std::endl;
    std::cout << "offsetof(color) = " << offsetof(InstanceData, color) << std::endl;
    std::cout << "offsetof(materialType) = " << offsetof(InstanceData, materialType) << std::endl;
    
    // Check first water instance in detail
    for (size_t i = 0; i < testInstances.size() && i < 10; i++) {
        if (static_cast<int>(testInstances[i].materialType + 0.5f) == 2) {
            std::cout << "\nFirst water instance (index " << i << "):" << std::endl;
            std::cout << "  Raw bytes: ";
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&testInstances[i]);
            for (size_t b = 0; b < sizeof(InstanceData); b++) {
                if (b == 28) std::cout << "["; // Mark material type bytes
                printf("%02x ", bytes[b]);
                if (b == 31) std::cout << "]"; // End material type bytes
            }
            std::cout << std::endl;
            
            // Show float value at offset 28
            const float* matPtr = reinterpret_cast<const float*>(bytes + 28);
            std::cout << "  Material at offset 28 as float: " << *matPtr << std::endl;
            std::cout << "  Material field value: " << testInstances[i].materialType << std::endl;
            break;
        }
    }
}

int main() {
    testMaterialPassthrough();
    return 0;
}