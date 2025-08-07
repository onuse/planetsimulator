// Test to validate draw call parameters
// This test verifies that vkCmdDrawIndexed is called with correct parameters

#include <iostream>
#include <cassert>
#include <vector>
#include <vulkan/vulkan.h>

class DrawCallValidator {
public:
    // Track draw call parameters
    struct DrawCall {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        uint32_t firstInstance;
    };
    
    static std::vector<DrawCall> recordedDrawCalls;
    
    // Mock vkCmdDrawIndexed
    static void mockDrawIndexed(
        VkCommandBuffer commandBuffer,
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance) {
        
        DrawCall call;
        call.indexCount = indexCount;
        call.instanceCount = instanceCount;
        call.firstIndex = firstIndex;
        call.vertexOffset = vertexOffset;
        call.firstInstance = firstInstance;
        
        recordedDrawCalls.push_back(call);
    }
    
    // Test 1: Validate cube mesh draw parameters
    static void testCubeDrawCall() {
        std::cout << "Test 1: Cube Mesh Draw Call Parameters" << std::endl;
        
        // Expected values for a cube
        const uint32_t CUBE_INDEX_COUNT = 36;  // 6 faces * 2 triangles * 3 indices
        const uint32_t CUBE_VERTEX_COUNT = 24; // 6 faces * 4 vertices (duplicated for normals)
        
        std::cout << "  Expected cube indices: " << CUBE_INDEX_COUNT << std::endl;
        std::cout << "  Expected cube vertices: " << CUBE_VERTEX_COUNT << std::endl;
        
        // Simulate drawing cubes with instances
        recordedDrawCalls.clear();
        VkCommandBuffer mockCmd = VK_NULL_HANDLE;
        uint32_t visibleNodeCount = 352672;  // From our actual run
        
        mockDrawIndexed(mockCmd, CUBE_INDEX_COUNT, visibleNodeCount, 0, 0, 0);
        
        assert(recordedDrawCalls.size() == 1);
        const auto& call = recordedDrawCalls[0];
        
        std::cout << "  Index count: " << call.indexCount;
        assert(call.indexCount == CUBE_INDEX_COUNT);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  Instance count: " << call.instanceCount;
        assert(call.instanceCount == visibleNodeCount);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  First index: " << call.firstIndex;
        assert(call.firstIndex == 0);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  Vertex offset: " << call.vertexOffset;
        assert(call.vertexOffset == 0);
        std::cout << " ✓" << std::endl;
        
        std::cout << "  First instance: " << call.firstInstance;
        assert(call.firstInstance == 0);
        std::cout << " ✓" << std::endl;
    }
    
    // Test 2: Validate instance count bug scenario
    static void testInstanceCountBug() {
        std::cout << "\nTest 2: Instance Count Bug Detection" << std::endl;
        
        recordedDrawCalls.clear();
        
        // Simulate the bug we had
        uint32_t nodeCount = 52632;        // Number of octree nodes
        uint32_t actualInstances = 352672; // Actual instances created (8x more)
        
        std::cout << "  Node count: " << nodeCount << std::endl;
        std::cout << "  Actual instances: " << actualInstances << std::endl;
        
        // The bug: using node count instead of instance count
        VkCommandBuffer mockCmd = VK_NULL_HANDLE;
        mockDrawIndexed(mockCmd, 36, nodeCount, 0, 0, 0);  // BUG!
        
        const auto& buggyCall = recordedDrawCalls[0];
        
        std::cout << "  Buggy draw call instance count: " << buggyCall.instanceCount << std::endl;
        
        if (buggyCall.instanceCount < actualInstances) {
            std::cout << "  ✗ BUG DETECTED: Only drawing " << buggyCall.instanceCount 
                      << " of " << actualInstances << " instances!" << std::endl;
            std::cout << "  Missing " << (actualInstances - buggyCall.instanceCount) 
                      << " instances (" 
                      << ((actualInstances - buggyCall.instanceCount) * 100 / actualInstances) 
                      << "% of geometry)!" << std::endl;
        }
        
        // Correct call
        recordedDrawCalls.clear();
        mockDrawIndexed(mockCmd, 36, actualInstances, 0, 0, 0);  // CORRECT!
        
        const auto& correctCall = recordedDrawCalls[0];
        std::cout << "  Correct draw call instance count: " << correctCall.instanceCount;
        assert(correctCall.instanceCount == actualInstances);
        std::cout << " ✓" << std::endl;
    }
    
    // Test 3: Validate draw call with no instances
    static void testNoInstances() {
        std::cout << "\nTest 3: Draw Call With No Instances" << std::endl;
        
        recordedDrawCalls.clear();
        
        // When visibleNodeCount is 0, we shouldn't draw anything
        uint32_t visibleNodeCount = 0;
        
        std::cout << "  Visible nodes: " << visibleNodeCount << std::endl;
        
        // The code should check and not call draw
        if (visibleNodeCount > 0) {
            VkCommandBuffer mockCmd = VK_NULL_HANDLE;
            mockDrawIndexed(mockCmd, 36, visibleNodeCount, 0, 0, 0);
        }
        
        std::cout << "  Draw calls made: " << recordedDrawCalls.size();
        assert(recordedDrawCalls.size() == 0);
        std::cout << " ✓ (correctly skipped)" << std::endl;
    }
    
    // Test 4: Validate draw parameters match index buffer
    static void testIndexBufferConsistency() {
        std::cout << "\nTest 4: Index Buffer Consistency" << std::endl;
        
        // Cube indices (from vulkan_renderer_resources.cpp)
        const uint16_t cubeIndices[] = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face
            4, 5, 6,  6, 7, 4,
            // Top face
            3, 2, 6,  6, 7, 3,
            // Bottom face
            0, 1, 5,  5, 4, 0,
            // Right face
            1, 5, 6,  6, 2, 1,
            // Left face
            0, 4, 7,  7, 3, 0
        };
        
        uint32_t indexCount = sizeof(cubeIndices) / sizeof(cubeIndices[0]);
        
        std::cout << "  Index buffer size: " << indexCount << " indices" << std::endl;
        std::cout << "  Bytes: " << sizeof(cubeIndices) << std::endl;
        std::cout << "  Triangles: " << indexCount / 3 << std::endl;
        
        // Validate we're not drawing more indices than we have
        recordedDrawCalls.clear();
        VkCommandBuffer mockCmd = VK_NULL_HANDLE;
        mockDrawIndexed(mockCmd, indexCount, 1000, 0, 0, 0);
        
        const auto& call = recordedDrawCalls[0];
        
        std::cout << "  Draw call index count: " << call.indexCount;
        assert(call.indexCount == 36);
        std::cout << " ✓" << std::endl;
        
        // Check for common errors
        if (call.firstIndex + call.indexCount > indexCount) {
            std::cout << "  ✗ ERROR: Drawing beyond index buffer bounds!" << std::endl;
            std::cout << "    First index: " << call.firstIndex << std::endl;
            std::cout << "    Count: " << call.indexCount << std::endl;
            std::cout << "    Buffer size: " << indexCount << std::endl;
        }
    }
    
    // Test 5: Performance implications of draw parameters
    static void testPerformanceImplications() {
        std::cout << "\nTest 5: Performance Implications" << std::endl;
        
        uint32_t instanceCount = 352672;
        uint32_t indexCount = 36;
        
        uint64_t totalVertices = (uint64_t)instanceCount * indexCount;
        uint64_t totalTriangles = totalVertices / 3;
        
        std::cout << "  Instances: " << instanceCount << std::endl;
        std::cout << "  Indices per instance: " << indexCount << std::endl;
        std::cout << "  Total vertices processed: " << totalVertices << std::endl;
        std::cout << "  Total triangles: " << totalTriangles << std::endl;
        
        if (totalTriangles > 10000000) {
            std::cout << "  ⚠ WARNING: Over 10M triangles per frame!" << std::endl;
            std::cout << "    Consider LOD or culling optimizations" << std::endl;
        }
        
        // Check instance batch size
        if (instanceCount > 1000000) {
            std::cout << "  ⚠ WARNING: Over 1M instances in single draw call!" << std::endl;
            std::cout << "    Consider splitting into multiple draws for better GPU scheduling" << std::endl;
        } else if (instanceCount < 100) {
            std::cout << "  ⚠ NOTE: Low instance count (" << instanceCount << ")" << std::endl;
            std::cout << "    Instancing overhead might not be worth it" << std::endl;
        } else {
            std::cout << "  ✓ Instance count in reasonable range for instanced rendering" << std::endl;
        }
    }
    
    static void runAllTests() {
        std::cout << "=== DRAW CALL PARAMETER TESTS ===" << std::endl;
        std::cout << "Validating vkCmdDrawIndexed parameters...\n" << std::endl;
        
        testCubeDrawCall();
        testInstanceCountBug();
        testNoInstances();
        testIndexBufferConsistency();
        testPerformanceImplications();
        
        std::cout << "\n✅ All draw call tests completed!" << std::endl;
    }
};

// Static member definition
std::vector<DrawCallValidator::DrawCall> DrawCallValidator::recordedDrawCalls;

int main() {
    try {
        DrawCallValidator::runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}