#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>

// Test GPU vertex attribute binding configuration
class GPUAttributeTest {
public:
    // Expected configuration for our pipeline
    struct ExpectedAttribute {
        uint32_t location;
        uint32_t binding;
        VkFormat format;
        uint32_t offset;
        const char* name;
        const char* glslType;
    };
    
    static void runTest() {
        std::cout << "=== GPU VERTEX ATTRIBUTE CONFIGURATION TEST ===" << std::endl;
        
        // Define expected attributes
        std::vector<ExpectedAttribute> expectedAttributes = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0, "inPosition", "vec3"},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12, "inNormal", "vec3"},
            {2, 1, VK_FORMAT_R32G32B32_SFLOAT, 0, "instanceCenter", "vec3"},
            {3, 1, VK_FORMAT_R32_SFLOAT, 12, "instanceHalfSize", "float"},
            {4, 1, VK_FORMAT_R32G32B32_SFLOAT, 16, "instanceColor", "vec3"},
            {5, 1, VK_FORMAT_R32_UINT, 28, "instanceMaterialType", "uint"}
        };
        
        std::cout << "\nExpected Vertex Input Configuration:" << std::endl;
        std::cout << "Loc | Bind | Offset | Format              | GLSL Type | Name" << std::endl;
        std::cout << "----|------|--------|---------------------|-----------|---------------------" << std::endl;
        
        for (const auto& attr : expectedAttributes) {
            printf("%3d | %4d | %6d | %-19s | %-9s | %s\n",
                   attr.location,
                   attr.binding,
                   attr.offset,
                   getFormatString(attr.format),
                   attr.glslType,
                   attr.name);
        }
        
        std::cout << "\n=== CRITICAL CHECKS ===" << std::endl;
        
        // Check 1: Material type attribute
        std::cout << "1. Material Type Attribute (Location 5):" << std::endl;
        std::cout << "   - Must be at binding 1, offset 28" << std::endl;
        std::cout << "   - Format MUST be VK_FORMAT_R32_UINT for uint in shader" << std::endl;
        std::cout << "   - Shader declaration: layout(location = 5) in uint instanceMaterialType;" << std::endl;
        
        // Check 2: Instance buffer stride
        std::cout << "\n2. Instance Buffer Stride:" << std::endl;
        std::cout << "   - Must be exactly 32 bytes" << std::endl;
        std::cout << "   - VkVertexInputBindingDescription for binding 1:" << std::endl;
        std::cout << "     * binding = 1" << std::endl;
        std::cout << "     * stride = 32" << std::endl;
        std::cout << "     * inputRate = VK_VERTEX_INPUT_RATE_INSTANCE" << std::endl;
        
        // Check 3: Pipeline recreation
        std::cout << "\n3. Pipeline State:" << std::endl;
        std::cout << "   - Pipeline MUST be recreated after adding 6th attribute" << std::endl;
        std::cout << "   - Clear any pipeline cache files" << std::endl;
        std::cout << "   - Verify VkPipelineVertexInputStateCreateInfo has:" << std::endl;
        std::cout << "     * vertexAttributeDescriptionCount = 6" << std::endl;
        std::cout << "     * vertexBindingDescriptionCount = 2" << std::endl;
        
        // Check 4: Shader compilation
        std::cout << "\n4. Shader Requirements:" << std::endl;
        std::cout << "   - Vertex shader must declare all 6 input attributes" << std::endl;
        std::cout << "   - Fragment shader must receive: layout(location = 3) flat in uint fragMaterialType;" << std::endl;
        std::cout << "   - Shaders must be recompiled after any changes" << std::endl;
        
        std::cout << "\n=== DEBUGGING STEPS ===" << std::endl;
        std::cout << "1. Enable Vulkan validation layers" << std::endl;
        std::cout << "2. Check for validation errors about vertex attributes" << std::endl;
        std::cout << "3. Use RenderDoc to inspect actual GPU state:" << std::endl;
        std::cout << "   - Verify instance buffer contents" << std::endl;
        std::cout << "   - Check vertex attribute bindings" << std::endl;
        std::cout << "   - Inspect shader input values" << std::endl;
        std::cout << "4. Add debug output in vertex shader:" << std::endl;
        std::cout << "   - if (gl_VertexIndex == 0 && gl_InstanceIndex < 3) {" << std::endl;
        std::cout << "       // Force output based on material type for debugging" << std::endl;
        std::cout << "     }" << std::endl;
        
        std::cout << "\n=== COMMON ISSUES ===" << std::endl;
        std::cout << "✗ Shader compiled with old attribute count" << std::endl;
        std::cout << "✗ Pipeline cached with 5 attributes instead of 6" << std::endl;
        std::cout << "✗ Format mismatch (UINT vs SINT vs FLOAT)" << std::endl;
        std::cout << "✗ Offset calculation error due to padding" << std::endl;
        std::cout << "✗ Instance buffer stride mismatch" << std::endl;
        std::cout << "✗ Vertex input state not updated" << std::endl;
    }
    
private:
    static const char* getFormatString(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
            case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
            case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
            case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
            default: return "UNKNOWN";
        }
    }
};

int main() {
    GPUAttributeTest::runTest();
    return 0;
}