#include <iostream>
#include <vector>
#include <cstring>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// This test validates that shader attributes match the data we're sending
class ShaderAttributeValidator {
public:
    // This MUST match the shader's expected layout EXACTLY
    struct ShaderExpectedLayout {
        // Vertex attributes (binding 0)
        struct {
            glm::vec3 position;  // location 0
            glm::vec3 normal;    // location 1
        } vertex;
        
        // Instance attributes (binding 1)
        struct {
            glm::vec3 center;       // location 2
            float halfSize;         // location 3
            glm::vec3 color;        // location 4
            uint32_t materialType;  // location 5
        } instance;
    };
    
    // What we're actually sending
    struct ActualInstanceData {
        glm::vec3 center;        // offset 0
        float halfSize;          // offset 12
        glm::vec3 color;         // offset 16
        uint32_t materialType;   // offset 28
    };
    
    static void runValidation() {
        std::cout << "=== SHADER ATTRIBUTE VALIDATION TEST ===" << std::endl;
        
        // Test 1: Verify data structure alignment
        std::cout << "\nTest 1: Data Structure Alignment" << std::endl;
        {
            std::cout << "  ActualInstanceData:" << std::endl;
            std::cout << "    Total size: " << sizeof(ActualInstanceData) << " bytes" << std::endl;
            std::cout << "    center offset: " << offsetof(ActualInstanceData, center) << std::endl;
            std::cout << "    halfSize offset: " << offsetof(ActualInstanceData, halfSize) << std::endl;
            std::cout << "    color offset: " << offsetof(ActualInstanceData, color) << std::endl;
            std::cout << "    materialType offset: " << offsetof(ActualInstanceData, materialType) << std::endl;
            
            bool alignmentCorrect = 
                sizeof(ActualInstanceData) == 32 &&
                offsetof(ActualInstanceData, materialType) == 28;
            
            std::cout << "  " << (alignmentCorrect ? "✓" : "✗") << " Structure alignment correct" << std::endl;
            
            if (!alignmentCorrect) {
                std::cout << "  ERROR: Structure alignment is wrong!" << std::endl;
                std::cout << "  Expected materialType at offset 28, got " 
                          << offsetof(ActualInstanceData, materialType) << std::endl;
            }
        }
        
        // Test 2: Binary validation of material type storage
        std::cout << "\nTest 2: Binary Material Type Storage" << std::endl;
        {
            ActualInstanceData testData;
            testData.center = glm::vec3(100, 200, 300);
            testData.halfSize = 50.0f;
            testData.color = glm::vec3(0.0f, 0.3f, 0.7f);
            testData.materialType = 2; // Water
            
            // Read as raw bytes
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&testData);
            
            // Check material type at offset 28
            uint32_t* materialAtOffset28 = reinterpret_cast<uint32_t*>(bytes + 28);
            
            std::cout << "  Set materialType to: 2 (Water)" << std::endl;
            std::cout << "  Value at offset 28: " << *materialAtOffset28 << std::endl;
            std::cout << "  Raw bytes at offset 28-31: ";
            for (int i = 28; i < 32; i++) {
                printf("%02X ", bytes[i]);
            }
            std::cout << std::endl;
            
            bool storageCorrect = (*materialAtOffset28 == 2);
            std::cout << "  " << (storageCorrect ? "✓" : "✗") << " Material type stored correctly" << std::endl;
            
            if (!storageCorrect) {
                std::cout << "  ERROR: Material type not stored at correct offset!" << std::endl;
            }
        }
        
        // Test 3: Vulkan format compatibility
        std::cout << "\nTest 3: Vulkan Format Compatibility" << std::endl;
        {
            std::cout << "  C++ type: uint32_t (size=" << sizeof(uint32_t) << ")" << std::endl;
            std::cout << "  Vulkan format: VK_FORMAT_R32_UINT" << std::endl;
            std::cout << "  GLSL type: uint" << std::endl;
            
            // Check if uint32_t is 4 bytes
            bool sizeCorrect = (sizeof(uint32_t) == 4);
            std::cout << "  " << (sizeCorrect ? "✓" : "✗") << " Size matches (4 bytes)" << std::endl;
        }
        
        // Test 4: Attribute declaration matching
        std::cout << "\nTest 4: Shader Declaration Requirements" << std::endl;
        {
            std::cout << "  Required vertex shader declarations:" << std::endl;
            std::cout << "    layout(location = 0) in vec3 inPosition;" << std::endl;
            std::cout << "    layout(location = 1) in vec3 inNormal;" << std::endl;
            std::cout << "    layout(location = 2) in vec3 instanceCenter;" << std::endl;
            std::cout << "    layout(location = 3) in float instanceHalfSize;" << std::endl;
            std::cout << "    layout(location = 4) in vec3 instanceColor;" << std::endl;
            std::cout << "    layout(location = 5) in uint instanceMaterialType;" << std::endl;
            std::cout << "\n  Required fragment shader input:" << std::endl;
            std::cout << "    layout(location = 3) flat in uint fragMaterialType;" << std::endl;
        }
        
        // Test 5: Pipeline configuration requirements
        std::cout << "\nTest 5: Pipeline Configuration Requirements" << std::endl;
        {
            std::cout << "  VkVertexInputAttributeDescription[5] must be:" << std::endl;
            std::cout << "    .binding = 1" << std::endl;
            std::cout << "    .location = 5" << std::endl;
            std::cout << "    .format = VK_FORMAT_R32_UINT" << std::endl;
            std::cout << "    .offset = 28" << std::endl;
            std::cout << "\n  VkVertexInputBindingDescription[1] must be:" << std::endl;
            std::cout << "    .binding = 1" << std::endl;
            std::cout << "    .stride = 32" << std::endl;
            std::cout << "    .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE" << std::endl;
        }
        
        // Test 6: Common failure modes
        std::cout << "\nTest 6: Diagnosing Common Failures" << std::endl;
        {
            std::cout << "  If water renders as rock, check:" << std::endl;
            std::cout << "    1. Pipeline cache not cleared after adding 6th attribute" << std::endl;
            std::cout << "    2. Shaders not recompiled after format change" << std::endl;
            std::cout << "    3. VkVertexInputAttributeDescription array size < 6" << std::endl;
            std::cout << "    4. Material type being overwritten somewhere" << std::endl;
            std::cout << "    5. Shader reading from wrong location" << std::endl;
            std::cout << "    6. Format mismatch (UINT vs SINT vs FLOAT)" << std::endl;
        }
        
        std::cout << "\n=== CRITICAL ACTION ITEMS ===" << std::endl;
        std::cout << "1. DELETE all .spv files and recompile shaders" << std::endl;
        std::cout << "2. DELETE all pipeline cache files" << std::endl;
        std::cout << "3. VERIFY shader source has 'layout(location = 5) in uint instanceMaterialType;'" << std::endl;
        std::cout << "4. REBUILD entire project from scratch" << std::endl;
        std::cout << "5. USE RenderDoc to inspect actual GPU values" << std::endl;
    }
};

int main() {
    ShaderAttributeValidator::runValidation();
    return 0;
}