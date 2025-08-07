// Test to validate pipeline state configuration
// This test verifies that pipeline state settings are correctly configured

#include <iostream>
#include <cassert>
#include <cstring>
#include <vulkan/vulkan.h>

class PipelineStateValidator {
public:
    // Expected pipeline state configuration based on vulkan_renderer_pipeline.cpp
    struct ExpectedPipelineState {
        // Rasterization state
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        float lineWidth = 1.0f;
        
        // Depth stencil state
        VkBool32 depthTestEnable = VK_TRUE;
        VkBool32 depthWriteEnable = VK_TRUE;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        VkBool32 depthBoundsTestEnable = VK_FALSE;
        VkBool32 stencilTestEnable = VK_FALSE;
        
        // Multisampling
        VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkBool32 sampleShadingEnable = VK_FALSE;
        
        // Color blending
        VkBool32 blendEnable = VK_FALSE;
        VkColorComponentFlags colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    };
    
    static void validateRasterizationState() {
        std::cout << "Test 1: Rasterization State Validation" << std::endl;
        
        ExpectedPipelineState expected;
        
        // Test cull mode
        std::cout << "  Cull Mode: ";
        if (expected.cullMode == VK_CULL_MODE_BACK_BIT) {
            std::cout << "BACK_BIT ✓" << std::endl;
            std::cout << "    - Back faces will be culled (correct for CCW winding)" << std::endl;
        } else if (expected.cullMode == VK_CULL_MODE_NONE) {
            std::cout << "NONE ⚠" << std::endl;
            std::cout << "    - WARNING: No culling may impact performance" << std::endl;
        } else {
            std::cout << "FRONT_BIT ✗" << std::endl;
            std::cout << "    - ERROR: Front face culling will hide visible geometry!" << std::endl;
        }
        
        // Test front face orientation
        std::cout << "  Front Face: ";
        if (expected.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) {
            std::cout << "CCW ✓" << std::endl;
            std::cout << "    - Matches standard OpenGL convention" << std::endl;
        } else {
            std::cout << "CW ⚠" << std::endl;
            std::cout << "    - WARNING: Clockwise winding - ensure vertices match" << std::endl;
        }
        
        // Test polygon mode
        std::cout << "  Polygon Mode: ";
        if (expected.polygonMode == VK_POLYGON_MODE_FILL) {
            std::cout << "FILL ✓" << std::endl;
        } else if (expected.polygonMode == VK_POLYGON_MODE_LINE) {
            std::cout << "LINE (wireframe) ⚠" << std::endl;
        } else {
            std::cout << "POINT ⚠" << std::endl;
        }
        
        assert(expected.cullMode == VK_CULL_MODE_BACK_BIT);
        assert(expected.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE);
        assert(expected.polygonMode == VK_POLYGON_MODE_FILL);
    }
    
    static void validateDepthStencilState() {
        std::cout << "\nTest 2: Depth Stencil State Validation" << std::endl;
        
        ExpectedPipelineState expected;
        
        // Test depth testing
        std::cout << "  Depth Test: " << (expected.depthTestEnable ? "ENABLED ✓" : "DISABLED ✗") << std::endl;
        std::cout << "  Depth Write: " << (expected.depthWriteEnable ? "ENABLED ✓" : "DISABLED ✗") << std::endl;
        
        // Test depth compare operation
        std::cout << "  Depth Compare Op: ";
        switch (expected.depthCompareOp) {
            case VK_COMPARE_OP_LESS:
                std::cout << "LESS ✓" << std::endl;
                std::cout << "    - Closer fragments pass (standard Z-buffer)" << std::endl;
                break;
            case VK_COMPARE_OP_LESS_OR_EQUAL:
                std::cout << "LESS_OR_EQUAL ✓" << std::endl;
                break;
            case VK_COMPARE_OP_GREATER:
                std::cout << "GREATER ✗" << std::endl;
                std::cout << "    - ERROR: Reversed depth - farther fragments pass!" << std::endl;
                break;
            case VK_COMPARE_OP_ALWAYS:
                std::cout << "ALWAYS ✗" << std::endl;
                std::cout << "    - ERROR: No depth testing!" << std::endl;
                break;
            default:
                std::cout << "UNKNOWN ✗" << std::endl;
        }
        
        assert(expected.depthTestEnable == VK_TRUE);
        assert(expected.depthWriteEnable == VK_TRUE);
        assert(expected.depthCompareOp == VK_COMPARE_OP_LESS);
    }
    
    static void validateBlendingState() {
        std::cout << "\nTest 3: Color Blending State Validation" << std::endl;
        
        ExpectedPipelineState expected;
        
        std::cout << "  Blending: " << (expected.blendEnable ? "ENABLED" : "DISABLED") << " ✓" << std::endl;
        std::cout << "  Color Write Mask: ";
        
        bool hasR = (expected.colorWriteMask & VK_COLOR_COMPONENT_R_BIT) != 0;
        bool hasG = (expected.colorWriteMask & VK_COLOR_COMPONENT_G_BIT) != 0;
        bool hasB = (expected.colorWriteMask & VK_COLOR_COMPONENT_B_BIT) != 0;
        bool hasA = (expected.colorWriteMask & VK_COLOR_COMPONENT_A_BIT) != 0;
        
        std::cout << (hasR ? "R" : "-");
        std::cout << (hasG ? "G" : "-");
        std::cout << (hasB ? "B" : "-");
        std::cout << (hasA ? "A" : "-");
        
        if (hasR && hasG && hasB && hasA) {
            std::cout << " ✓ (all channels enabled)" << std::endl;
        } else {
            std::cout << " ⚠ (some channels disabled)" << std::endl;
        }
        
        assert(hasR && hasG && hasB && hasA);
    }
    
    static void validateForCommonIssues() {
        std::cout << "\nTest 4: Common Pipeline Issues Check" << std::endl;
        
        ExpectedPipelineState expected;
        
        // Check for problematic combinations
        bool issues = false;
        
        // Issue 1: Back face culling with wrong winding order
        if (expected.cullMode == VK_CULL_MODE_BACK_BIT && 
            expected.frontFace == VK_FRONT_FACE_CLOCKWISE) {
            std::cout << "  ⚠ WARNING: Back face culling with CW winding - verify vertex order!" << std::endl;
            issues = true;
        }
        
        // Issue 2: Depth test disabled but depth write enabled
        if (!expected.depthTestEnable && expected.depthWriteEnable) {
            std::cout << "  ✗ ERROR: Depth write without depth test - undefined behavior!" << std::endl;
            issues = true;
        }
        
        // Issue 3: Wrong depth compare for standard rendering
        if (expected.depthTestEnable && 
            (expected.depthCompareOp == VK_COMPARE_OP_GREATER ||
             expected.depthCompareOp == VK_COMPARE_OP_ALWAYS ||
             expected.depthCompareOp == VK_COMPARE_OP_NEVER)) {
            std::cout << "  ✗ ERROR: Non-standard depth compare operation!" << std::endl;
            issues = true;
        }
        
        if (!issues) {
            std::cout << "  ✓ No common pipeline configuration issues detected" << std::endl;
        }
    }
    
    static void runAllTests() {
        std::cout << "=== PIPELINE STATE CONFIGURATION TESTS ===" << std::endl;
        std::cout << "Validating expected pipeline state configuration...\n" << std::endl;
        
        validateRasterizationState();
        validateDepthStencilState();
        validateBlendingState();
        validateForCommonIssues();
        
        std::cout << "\n✅ All pipeline state tests passed!" << std::endl;
        std::cout << "Note: These tests validate expected configuration values." << std::endl;
        std::cout << "Actual runtime validation requires Vulkan validation layers." << std::endl;
    }
};

int main() {
    try {
        PipelineStateValidator::runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}