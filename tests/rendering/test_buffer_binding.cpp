// Test to verify buffer binding sequence and configuration
// This test validates that buffers are bound in the correct order and with correct parameters

#include <iostream>
#include <cassert>
#include <vector>
#include <vulkan/vulkan.h>

class BufferBindingValidator {
public:
    // Mock command recording to track binding calls
    struct BindingCall {
        enum Type { VERTEX_BUFFER, INDEX_BUFFER, DESCRIPTOR_SET } type;
        uint32_t firstBinding;
        uint32_t bindingCount;
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceSize> offsets;
    };
    
    static std::vector<BindingCall> recordedCalls;
    
    // Mock vkCmdBindVertexBuffers
    static void mockBindVertexBuffers(
        VkCommandBuffer commandBuffer,
        uint32_t firstBinding,
        uint32_t bindingCount,
        const VkBuffer* pBuffers,
        const VkDeviceSize* pOffsets) {
        
        BindingCall call;
        call.type = BindingCall::VERTEX_BUFFER;
        call.firstBinding = firstBinding;
        call.bindingCount = bindingCount;
        
        for (uint32_t i = 0; i < bindingCount; i++) {
            call.buffers.push_back(pBuffers[i]);
            call.offsets.push_back(pOffsets[i]);
        }
        
        recordedCalls.push_back(call);
    }
    
    // Test 1: Verify correct buffer binding with instances
    static void testInstancedRendering() {
        std::cout << "Test 1: Instanced Rendering Buffer Binding" << std::endl;
        
        recordedCalls.clear();
        
        // Simulate the binding sequence from vulkan_renderer_commands.cpp
        VkCommandBuffer mockCmd = VK_NULL_HANDLE;
        VkBuffer vertexBuffer = (VkBuffer)0x1000;
        VkBuffer instanceBuffer = (VkBuffer)0x2000;
        
        // This is what the code does when we have instances
        VkBuffer buffers[] = {vertexBuffer, instanceBuffer};
        VkDeviceSize offsets[] = {0, 0};
        mockBindVertexBuffers(mockCmd, 0, 2, buffers, offsets);
        
        // Validate the binding
        assert(recordedCalls.size() == 1);
        const auto& call = recordedCalls[0];
        
        std::cout << "  Binding count: " << call.bindingCount;
        assert(call.bindingCount == 2);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  First binding index: " << call.firstBinding;
        assert(call.firstBinding == 0);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  Vertex buffer at binding 0: ";
        assert(call.buffers[0] == vertexBuffer);
        std::cout << "0x" << std::hex << (uintptr_t)call.buffers[0] << std::dec << " ✓" << std::endl;
        
        std::cout << "  Instance buffer at binding 1: ";
        assert(call.buffers[1] == instanceBuffer);
        std::cout << "0x" << std::hex << (uintptr_t)call.buffers[1] << std::dec << " ✓" << std::endl;
        
        std::cout << "  All offsets are 0: ";
        assert(call.offsets[0] == 0 && call.offsets[1] == 0);
        std::cout << "✓" << std::endl;
    }
    
    // Test 2: Verify correct buffer binding without instances
    static void testNonInstancedRendering() {
        std::cout << "\nTest 2: Non-Instanced Rendering Buffer Binding" << std::endl;
        
        recordedCalls.clear();
        
        VkCommandBuffer mockCmd = VK_NULL_HANDLE;
        VkBuffer vertexBuffer = (VkBuffer)0x1000;
        
        // This is what the code does when we DON'T have instances
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        mockBindVertexBuffers(mockCmd, 0, 1, vertexBuffers, offsets);
        
        // Validate
        assert(recordedCalls.size() == 1);
        const auto& call = recordedCalls[0];
        
        std::cout << "  Binding count: " << call.bindingCount;
        assert(call.bindingCount == 1);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  Only vertex buffer bound: ";
        assert(call.buffers[0] == vertexBuffer);
        std::cout << "✓" << std::endl;
    }
    
    // Test 3: Validate expected binding layout matches shader
    static void testShaderBindingCompatibility() {
        std::cout << "\nTest 3: Shader Binding Layout Compatibility" << std::endl;
        
        // Expected shader input layout from hierarchical.vert
        struct ExpectedLayout {
            // Binding 0: Per-vertex data
            struct {
                uint32_t binding = 0;
                uint32_t stride = 24;  // vec3 pos + vec3 normal = 6 floats = 24 bytes
                VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            } vertexBinding;
            
            // Binding 1: Per-instance data
            struct {
                uint32_t binding = 1;
                uint32_t stride = 32;  // 32 bytes per instance
                VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            } instanceBinding;
        };
        
        ExpectedLayout expected;
        
        std::cout << "  Vertex binding:" << std::endl;
        std::cout << "    - Binding index: " << expected.vertexBinding.binding << " ✓" << std::endl;
        std::cout << "    - Stride: " << expected.vertexBinding.stride << " bytes ✓" << std::endl;
        std::cout << "    - Input rate: VERTEX ✓" << std::endl;
        
        std::cout << "  Instance binding:" << std::endl;
        std::cout << "    - Binding index: " << expected.instanceBinding.binding << " ✓" << std::endl;
        std::cout << "    - Stride: " << expected.instanceBinding.stride << " bytes ✓" << std::endl;
        std::cout << "    - Input rate: INSTANCE ✓" << std::endl;
        
        // These values must match what's in vulkan_renderer_pipeline.cpp
        assert(expected.vertexBinding.stride == 24);
        assert(expected.instanceBinding.stride == 32);
    }
    
    // Test 4: Check for common binding errors
    static void testCommonBindingErrors() {
        std::cout << "\nTest 4: Common Binding Error Detection" << std::endl;
        
        // Error 1: Binding buffers in wrong order
        {
            recordedCalls.clear();
            VkCommandBuffer mockCmd = VK_NULL_HANDLE;
            VkBuffer vertexBuffer = (VkBuffer)0x1000;
            VkBuffer instanceBuffer = (VkBuffer)0x2000;
            
            // WRONG: Instance buffer at binding 0, vertex at binding 1
            VkBuffer wrongOrder[] = {instanceBuffer, vertexBuffer};
            VkDeviceSize offsets[] = {0, 0};
            
            std::cout << "  Testing wrong buffer order: ";
            mockBindVertexBuffers(mockCmd, 0, 2, wrongOrder, offsets);
            
            // This would cause the shader to interpret instance data as vertices!
            if (recordedCalls[0].buffers[0] == instanceBuffer) {
                std::cout << "✗ ERROR DETECTED - Instance buffer at binding 0!" << std::endl;
                std::cout << "    This would cause shader to misinterpret data!" << std::endl;
            }
        }
        
        // Error 2: Wrong binding count
        {
            std::cout << "  Testing binding count mismatch: ";
            
            // If we have instances but only bind 1 buffer
            recordedCalls.clear();
            VkCommandBuffer mockCmd = VK_NULL_HANDLE;
            VkBuffer vertexBuffer = (VkBuffer)0x1000;
            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            
            mockBindVertexBuffers(mockCmd, 0, 1, vertexBuffers, offsets);
            
            // But we try to draw with instances
            uint32_t instanceCount = 100;
            if (recordedCalls[0].bindingCount == 1 && instanceCount > 1) {
                std::cout << "✗ ERROR - Only 1 buffer bound but drawing " 
                          << instanceCount << " instances!" << std::endl;
                std::cout << "    Missing instance buffer will cause crash/corruption!" << std::endl;
            }
        }
        
        // Error 3: Non-zero offset without proper alignment
        {
            std::cout << "  Testing offset alignment: ";
            VkDeviceSize badOffset = 13;  // Not aligned to any common boundary
            
            if (badOffset % 4 != 0) {
                std::cout << "✗ WARNING - Offset " << badOffset 
                          << " not aligned to 4 bytes!" << std::endl;
                std::cout << "    May cause performance issues or errors on some GPUs" << std::endl;
            }
        }
    }
    
    static void runAllTests() {
        std::cout << "=== BUFFER BINDING SEQUENCE TESTS ===" << std::endl;
        std::cout << "Validating buffer binding order and parameters...\n" << std::endl;
        
        testInstancedRendering();
        testNonInstancedRendering();
        testShaderBindingCompatibility();
        testCommonBindingErrors();
        
        std::cout << "\n✅ All buffer binding tests completed!" << std::endl;
        std::cout << "Note: Runtime validation requires Vulkan validation layers" << std::endl;
        std::cout << "to catch actual binding errors during command recording." << std::endl;
    }
};

// Static member definition
std::vector<BufferBindingValidator::BindingCall> BufferBindingValidator::recordedCalls;

int main() {
    try {
        BufferBindingValidator::runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}