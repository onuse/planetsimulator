#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <array>
#include <optional>
#include <chrono>

#include "core/octree.hpp"
#include "core/camera.hpp"
#include "rendering/imgui_manager.hpp"
#include "rendering/transvoxel_renderer.hpp"

namespace rendering {

// Input state structure
struct InputState {
    bool keys[512] = {false};
    bool prevKeys[512] = {false};
    bool mouseButtons[8] = {false};
    bool prevMouseButtons[8] = {false};
    glm::vec2 mousePos = {0, 0};
    glm::vec2 lastMousePos = {0, 0};
    glm::vec2 mouseDelta = {0, 0};
    glm::vec2 scrollDelta = {0, 0};
    bool firstMouse = true;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    
    bool isComplete() const {
        return graphicsFamily.has_value() && 
               presentFamily.has_value() && 
               computeFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 viewProj;
    alignas(16) glm::vec3 viewPos;
    float time;
    alignas(16) glm::vec3 lightDir;
    float padding;
};

// Instance data must match shader expectations exactly
struct InstanceData {
    glm::vec3 center;        // offset 0, size 12
    float halfSize;          // offset 12, size 4
    glm::vec4 colorAndMaterial; // offset 16, size 16 - xyz=color, w=material
    // Total size: 32 bytes
};

class VulkanRenderer {
public:
    VulkanRenderer(uint32_t width, uint32_t height);
    ~VulkanRenderer();
    
    // Main interface
    bool initialize();
    void render(octree::OctreePlanet* planet, core::Camera* camera);
    void cleanup();
    
    // Window management
    void resize(uint32_t width, uint32_t height);
    bool shouldClose() const;
    void pollEvents();
    
    // Input handling
    const InputState& getInputState() const { return inputState; }
    void updateInput();
    GLFWwindow* getWindow() const { return window; }
    
    // Settings
    void setRenderMode(int mode) { renderMode = mode; }
    void setWireframe(bool enabled) { wireframeEnabled = enabled; }
    void setVSync(bool enabled);
    // Hierarchical GPU octree is always enabled - no option to disable
    
    // Removed parallel paths - only transvoxel rendering now
    
    // Screenshot support
    bool captureScreenshot(const std::string& filename);
    
    // Debug support
    void dumpVertexData();  // Dump vertex data for debugging
    
    // Stats
    float getFrameTime() const { return frameTime; }
    uint32_t getNodeCount() const { return visibleNodeCount; }
    uint32_t getChunkCount() const { return static_cast<uint32_t>(activeChunks.size()); }
    uint32_t getTriangleCount() const { 
        return transvoxelRenderer ? transvoxelRenderer->getTriangleCount() : 0; 
    }
    
private:
    // Window
    GLFWwindow* window = nullptr;
    uint32_t windowWidth;
    uint32_t windowHeight;
    bool framebufferResized = false;
    
    // Core Vulkan objects
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkSurfaceKHR surface;
    
    // Swap chain
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    // Render pass
    VkRenderPass renderPass;
    
    // Depth buffer
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    
    // Pipeline
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkPipeline wireframePipeline;
    
    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    
    // Buffers
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceBufferMemory;
    void* instanceBufferMapped;
    
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
    
    // GPU octree data
    VkBuffer octreeNodeBuffer;
    VkDeviceMemory octreeNodeBufferMemory;
    VkBuffer voxelDataBuffer;
    VkDeviceMemory voxelDataBufferMemory;
    
    // Material table buffer
    VkBuffer materialTableBuffer;
    VkDeviceMemory materialTableBufferMemory;
    
    // Command buffers
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    
    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    uint32_t lastRenderedImageIndex = 0; // Track which swap chain image was last rendered
    
    // Rendering state
    int renderMode = 0; // 0: material, 1: temperature, 2: elevation, etc.
    bool wireframeEnabled = false;
    // Removed parallel rendering paths
    uint32_t visibleNodeCount = 0;
    std::vector<InstanceData> instances;
    
    // Performance tracking
    std::chrono::steady_clock::time_point lastFrameTime;
    float frameTime = 0.0f;
    
    // Input state
    InputState inputState;
    
    // ImGui manager
    ImGuiManager imguiManager;
    
    // Transvoxel renderer - THE ONLY rendering path
    std::unique_ptr<TransvoxelRenderer> transvoxelRenderer;
    
    // Chunk management
    std::vector<TransvoxelChunk> activeChunks;
    
    // Hierarchical pipeline (single rendering path)
    VkPipeline hierarchicalPipeline = VK_NULL_HANDLE;
    VkPipelineLayout hierarchicalPipelineLayout = VK_NULL_HANDLE;
    
    // Triangle mesh pipeline for Transvoxel rendering
    VkPipeline trianglePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout hierarchicalDescriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> hierarchicalDescriptorSets;
    
    // Initialization functions
    void createWindow();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createDepthResources();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    
    // Helper functions
    void cleanupSwapChain();
    void recreateSwapChain();
    void updateUniformBuffer(uint32_t currentImage, core::Camera* camera);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame(octree::OctreePlanet* planet, core::Camera* camera);
    void createCubeGeometry();
    void updateInstanceBuffer(const octree::OctreePlanet::RenderData& renderData);
    void createMaterialTableBuffer();
    void updateMaterialTableBuffer();
    
    // Device selection helpers
    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    
    // Swap chain helpers
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
    // Buffer helpers
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Image helpers
    void createImage(uint32_t width, uint32_t height, VkFormat format, 
                    VkImageTiling tiling, VkImageUsageFlags usage, 
                    VkMemoryPropertyFlags properties, VkImage& image, 
                    VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    
    // Shader loading
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
    
    // Transvoxel rendering
    void createTransvoxelPipeline();
    void createTransvoxelDescriptorSets();
    void createTrianglePipeline();
    void updateChunks(octree::OctreePlanet* planet, core::Camera* camera);
    void generateChunkMeshes(octree::OctreePlanet* planet);
    
    // Removed parallel rendering paths
    
    // Legacy hierarchical functions (still used by Transvoxel for MVP)
    void createHierarchicalPipeline();
    void createHierarchicalDescriptorSets();
    
    // Callbacks
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    
    // Validation layers
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
    
    // Required extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    // Validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif
};

} // namespace rendering