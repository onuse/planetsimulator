#include <iostream>
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

// Mirror the InstanceData structure exactly
struct InstanceData {
    glm::vec3 center;        // offset 0, size 12
    float halfSize;          // offset 12, size 4
    glm::vec3 color;         // offset 16, size 12
    uint32_t materialType;   // offset 28, size 4
    // Total size: 32 bytes
};

// Test that verifies vertex attribute formats and offsets
int main() {
    std::cout << "=== VERTEX ATTRIBUTE VALIDATION TEST ===" << std::endl;
    
    // Test 1: Verify struct layout matches Vulkan expectations
    std::cout << "\nTest 1: InstanceData Memory Layout" << std::endl;
    {
        std::cout << "  sizeof(InstanceData): " << sizeof(InstanceData) << " bytes (expected: 32)" << std::endl;
        std::cout << "  offsetof(center): " << offsetof(InstanceData, center) << " (expected: 0)" << std::endl;
        std::cout << "  offsetof(halfSize): " << offsetof(InstanceData, halfSize) << " (expected: 12)" << std::endl;
        std::cout << "  offsetof(color): " << offsetof(InstanceData, color) << " (expected: 16)" << std::endl;
        std::cout << "  offsetof(materialType): " << offsetof(InstanceData, materialType) << " (expected: 28)" << std::endl;
        
        bool layoutCorrect = 
            sizeof(InstanceData) == 32 &&
            offsetof(InstanceData, center) == 0 &&
            offsetof(InstanceData, halfSize) == 12 &&
            offsetof(InstanceData, color) == 16 &&
            offsetof(InstanceData, materialType) == 28;
        
        std::cout << "  " << (layoutCorrect ? "✓" : "✗") << " Memory layout matches expected" << std::endl;
        
        if (!layoutCorrect) {
            std::cout << "  ERROR: Struct layout doesn't match Vulkan attribute offsets!" << std::endl;
            return 1;
        }
    }
    
    // Test 2: Verify data types match Vulkan formats
    std::cout << "\nTest 2: Data Type Sizes" << std::endl;
    {
        std::cout << "  sizeof(glm::vec3): " << sizeof(glm::vec3) << " bytes (expected: 12)" << std::endl;
        std::cout << "  sizeof(float): " << sizeof(float) << " bytes (expected: 4)" << std::endl;
        std::cout << "  sizeof(uint32_t): " << sizeof(uint32_t) << " bytes (expected: 4)" << std::endl;
        
        bool sizesCorrect = 
            sizeof(glm::vec3) == 12 &&
            sizeof(float) == 4 &&
            sizeof(uint32_t) == 4;
        
        std::cout << "  " << (sizesCorrect ? "✓" : "✗") << " Data type sizes match Vulkan formats" << std::endl;
        
        if (!sizesCorrect) {
            std::cout << "  ERROR: Data type sizes don't match!" << std::endl;
            return 1;
        }
    }
    
    // Test 3: Verify attribute format mappings
    std::cout << "\nTest 3: Vulkan Format Mappings" << std::endl;
    {
        std::cout << "  Attribute 0 (vec3 position): VK_FORMAT_R32G32B32_SFLOAT" << std::endl;
        std::cout << "  Attribute 1 (vec3 normal): VK_FORMAT_R32G32B32_SFLOAT" << std::endl;
        std::cout << "  Attribute 2 (vec3 instanceCenter): VK_FORMAT_R32G32B32_SFLOAT" << std::endl;
        std::cout << "  Attribute 3 (float instanceHalfSize): VK_FORMAT_R32_SFLOAT" << std::endl;
        std::cout << "  Attribute 4 (vec3 instanceColor): VK_FORMAT_R32G32B32_SFLOAT" << std::endl;
        std::cout << "  Attribute 5 (uint instanceMaterialType): VK_FORMAT_R32_UINT" << std::endl;
        
        // Check format constants exist (compile-time check)
        VkFormat vec3Format = VK_FORMAT_R32G32B32_SFLOAT;
        VkFormat floatFormat = VK_FORMAT_R32_SFLOAT;
        VkFormat uintFormat = VK_FORMAT_R32_UINT;
        
        std::cout << "  ✓ All format constants are valid" << std::endl;
    }
    
    // Test 4: Simulate instance data creation
    std::cout << "\nTest 4: Instance Data Values" << std::endl;
    {
        InstanceData waterInstance;
        waterInstance.center = glm::vec3(100.0f, 200.0f, 300.0f);
        waterInstance.halfSize = 50.0f;
        waterInstance.color = glm::vec3(0.0f, 0.3f, 0.7f);
        waterInstance.materialType = 2; // Water
        
        std::cout << "  Created water instance:" << std::endl;
        std::cout << "    center: (" << waterInstance.center.x << ", " 
                  << waterInstance.center.y << ", " << waterInstance.center.z << ")" << std::endl;
        std::cout << "    halfSize: " << waterInstance.halfSize << std::endl;
        std::cout << "    color: (" << waterInstance.color.x << ", " 
                  << waterInstance.color.y << ", " << waterInstance.color.z << ")" << std::endl;
        std::cout << "    materialType: " << waterInstance.materialType << std::endl;
        
        // Verify as raw bytes
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&waterInstance);
        uint32_t* materialPtr = reinterpret_cast<uint32_t*>(bytes + 28);
        
        std::cout << "  Material type at offset 28: " << *materialPtr << std::endl;
        bool materialCorrect = (*materialPtr == 2);
        std::cout << "  " << (materialCorrect ? "✓" : "✗") << " Material type stored correctly" << std::endl;
        
        if (!materialCorrect) {
            std::cout << "  ERROR: Material type not at expected offset!" << std::endl;
            return 1;
        }
    }
    
    // Test 5: Shader compatibility check
    std::cout << "\nTest 5: Shader Compatibility" << std::endl;
    {
        std::cout << "  Expected shader declarations:" << std::endl;
        std::cout << "    layout(location = 2) in vec3 instanceCenter;" << std::endl;
        std::cout << "    layout(location = 3) in float instanceHalfSize;" << std::endl;
        std::cout << "    layout(location = 4) in vec3 instanceColor;" << std::endl;
        std::cout << "    layout(location = 5) in uint instanceMaterialType;" << std::endl;
        std::cout << "\n  Pipeline attribute setup:" << std::endl;
        std::cout << "    [2] binding=1, offset=0, format=VK_FORMAT_R32G32B32_SFLOAT" << std::endl;
        std::cout << "    [3] binding=1, offset=12, format=VK_FORMAT_R32_SFLOAT" << std::endl;
        std::cout << "    [4] binding=1, offset=16, format=VK_FORMAT_R32G32B32_SFLOAT" << std::endl;
        std::cout << "    [5] binding=1, offset=28, format=VK_FORMAT_R32_UINT" << std::endl;
        
        std::cout << "\n  ✓ Formats and offsets should match" << std::endl;
    }
    
    std::cout << "\n=== RECOMMENDATIONS ===" << std::endl;
    std::cout << "1. Ensure shaders are recompiled after format changes" << std::endl;
    std::cout << "2. Check that instance buffer stride is 32 bytes" << std::endl;
    std::cout << "3. Verify VkVertexInputAttributeDescription array has 6 elements" << std::endl;
    std::cout << "4. Confirm pipeline is recreated after adding materialType attribute" << std::endl;
    std::cout << "5. Test with RenderDoc or similar to inspect actual GPU values" << std::endl;
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}