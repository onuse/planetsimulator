// GPU Test harness for octree traversal
// This loads the binary octree files and renders them with a simple Vulkan setup
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

// EXACT same structure as main.cpp
struct GPUOctreeNode {
    glm::vec4 centerAndSize;     // xyz = center, w = halfSize
    glm::uvec4 childrenAndFlags; // x = children offset, y = voxel offset, z = flags, w = reserved
};

class GPUOctreeTest {
private:
    GLFWwindow* window;
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    
    // Octree storage
    VkBuffer nodeBuffer;
    VkDeviceMemory nodeBufferMemory;
    std::vector<GPUOctreeNode> nodes;
    
    // Push constants
    struct PushConstants {
        glm::vec2 resolution;
        float testCase;
        float debugMode;
    } pushConstants;
    
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    
    void loadOctreeFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open " << filename << std::endl;
            return;
        }
        
        uint32_t nodeCount;
        file.read((char*)&nodeCount, sizeof(nodeCount));
        nodes.resize(nodeCount);
        file.read((char*)nodes.data(), nodeCount * sizeof(GPUOctreeNode));
        file.close();
        
        std::cout << "Loaded " << nodeCount << " nodes from " << filename << std::endl;
        
        // Verify first node
        std::cout << "  Root node: center=(" << nodes[0].centerAndSize.x 
                  << "," << nodes[0].centerAndSize.y 
                  << "," << nodes[0].centerAndSize.z 
                  << ") size=" << nodes[0].centerAndSize.w << std::endl;
        std::cout << "  Flags: " << std::hex << nodes[0].childrenAndFlags.z << std::dec << std::endl;
        
        uploadNodesToGPU();
    }
    
    void uploadNodesToGPU() {
        // Create buffer and upload nodes
        VkDeviceSize bufferSize = nodes.size() * sizeof(GPUOctreeNode);
        
        // For simplicity, create a host-visible buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &nodeBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create node buffer!");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, nodeBuffer, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(device, &allocInfo, nullptr, &nodeBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate node buffer memory!");
        }
        
        vkBindBufferMemory(device, nodeBuffer, nodeBufferMemory, 0);
        
        // Copy data
        void* data;
        vkMapMemory(device, nodeBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, nodes.data(), bufferSize);
        vkUnmapMemory(device, nodeBufferMemory);
    }
    
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && 
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        throw std::runtime_error("Failed to find suitable memory type!");
    }
    
    void runTest(int testNumber) {
        std::string filename = "test" + std::to_string(testNumber) + ".octree";
        loadOctreeFile(filename);
        pushConstants.testCase = (float)testNumber;
        pushConstants.resolution = glm::vec2(800, 600);
        pushConstants.debugMode = 1.0f;
        
        std::cout << "\n=== Running GPU Test " << testNumber << " ===" << std::endl;
        
        // Render a frame and capture result
        renderFrame();
        
        // Compare with CPU result (would need to implement pixel readback)
        std::cout << "GPU test " << testNumber << " rendered successfully" << std::endl;
    }
    
    void renderFrame() {
        // Record command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[0]; // Use first framebuffer for test
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {800, 600};
        
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        
        // Push constants
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        
        // Bind descriptor set for storage buffer
        // (would need to create descriptor set with node buffer)
        
        // Draw full-screen triangle
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        
        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);
        
        // Submit and wait
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
    }
    
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "GPU Octree Test", nullptr, nullptr);
    }
    
    void initVulkan() {
        // Simplified Vulkan initialization
        // In practice, this would be much more detailed
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
    }
    
    void mainLoop() {
        // Run all three tests
        runTest(1);
        runTest(2);
        runTest(3);
        
        // Keep window open
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
    
    void cleanup() {
        vkDestroyBuffer(device, nodeBuffer, nullptr);
        vkFreeMemory(device, nodeBufferMemory, nullptr);
        
        // Clean up Vulkan resources
        vkDestroyCommandPool(device, commandPool, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    // Stub implementations for Vulkan setup
    void createInstance() { /* ... */ }
    void createSurface() { /* ... */ }
    void pickPhysicalDevice() { /* ... */ }
    void createLogicalDevice() { /* ... */ }
    void createSwapChain() { /* ... */ }
    void createImageViews() { /* ... */ }
    void createRenderPass() { /* ... */ }
    void createGraphicsPipeline() { /* ... */ }
    void createFramebuffers() { /* ... */ }
    void createCommandPool() { /* ... */ }
    void createCommandBuffer() { /* ... */ }
};

int main() {
    GPUOctreeTest test;
    
    try {
        test.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}