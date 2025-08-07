#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>

// Test to validate what GPU actually receives
class GPUValidation {
public:
    static void validateAttributeBinding() {
        std::cout << "=== GPU ATTRIBUTE BINDING VALIDATION ===" << std::endl;
        
        // What we THINK we're sending
        struct ExpectedData {
            float center[3];     // 0-11
            float halfSize;      // 12-15
            float color[3];      // 16-27
            uint32_t material;   // 28-31
        };
        
        // Create test data
        ExpectedData testData;
        testData.center[0] = 100.0f;
        testData.center[1] = 200.0f;
        testData.center[2] = 300.0f;
        testData.halfSize = 50.0f;
        testData.color[0] = 0.0f;
        testData.color[1] = 0.3f;
        testData.color[2] = 0.7f;
        testData.material = 2; // Water
        
        // Dump raw bytes
        std::cout << "\nRaw bytes of instance data:" << std::endl;
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&testData);
        for (int row = 0; row < 4; row++) {
            std::cout << "Offset " << (row*8) << "-" << (row*8+7) << ": ";
            for (int col = 0; col < 8; col++) {
                printf("%02X ", bytes[row*8 + col]);
            }
            std::cout << std::endl;
        }
        
        // Validate material at offset 28
        uint32_t* materialPtr = reinterpret_cast<uint32_t*>(bytes + 28);
        std::cout << "\nMaterial at offset 28: " << *materialPtr;
        if (*materialPtr == 2) {
            std::cout << " ✓ Correct!" << std::endl;
        } else {
            std::cout << " ✗ WRONG! Expected 2" << std::endl;
        }
        
        // Check what happens with different formats
        std::cout << "\n=== FORMAT INTERPRETATION ===" << std::endl;
        
        // As UINT
        uint32_t asUint = *reinterpret_cast<uint32_t*>(bytes + 28);
        std::cout << "As VK_FORMAT_R32_UINT: " << asUint << std::endl;
        
        // As SINT
        int32_t asSint = *reinterpret_cast<int32_t*>(bytes + 28);
        std::cout << "As VK_FORMAT_R32_SINT: " << asSint << std::endl;
        
        // As FLOAT (if we stored as float)
        float asFloat = static_cast<float>(testData.material);
        std::cout << "If stored as float 2.0: " << asFloat << std::endl;
        uint32_t floatBits = *reinterpret_cast<uint32_t*>(&asFloat);
        printf("  Float bits: 0x%08X\n", floatBits);
        
        // Recommendations
        std::cout << "\n=== RECOMMENDATIONS ===" << std::endl;
        std::cout << "1. If VK_FORMAT_R32_UINT fails, try:" << std::endl;
        std::cout << "   - VK_FORMAT_R32_SINT with int in shader" << std::endl;
        std::cout << "   - VK_FORMAT_R32_SFLOAT with float, cast to int in shader" << std::endl;
        std::cout << "2. Check Vulkan validation for format warnings" << std::endl;
        std::cout << "3. Use RenderDoc to inspect actual GPU values" << std::endl;
        std::cout << "4. Test on different GPUs/drivers" << std::endl;
    }
    
    static void testFloatWorkaround() {
        std::cout << "\n=== FLOAT WORKAROUND TEST ===" << std::endl;
        
        // Store material as float
        float materials[] = {0.0f, 1.0f, 2.0f, 3.0f};
        
        std::cout << "Storing materials as floats:" << std::endl;
        for (int i = 0; i < 4; i++) {
            std::cout << "  Material " << i << " -> " << materials[i] << "f" << std::endl;
            
            // Show how to recover in shader
            uint32_t recovered = static_cast<uint32_t>(materials[i] + 0.5f);
            std::cout << "    Recovered as uint: " << recovered;
            if (recovered == i) {
                std::cout << " ✓" << std::endl;
            } else {
                std::cout << " ✗" << std::endl;
            }
        }
        
        std::cout << "\nShader code for float workaround:" << std::endl;
        std::cout << "  layout(location = 5) in float instanceMaterialType;" << std::endl;
        std::cout << "  uint materialType = uint(instanceMaterialType + 0.5);" << std::endl;
    }
};

int main() {
    GPUValidation::validateAttributeBinding();
    GPUValidation::testFloatWorkaround();
    return 0;
}