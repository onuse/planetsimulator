#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>

namespace rendering {

// Forward declaration
class VulkanRenderer;

class ImGuiManager {
public:
    ImGuiManager() = default;
    ~ImGuiManager();
    
    // Initialize ImGui with Vulkan
    bool initialize(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice,
                   VkDevice device, uint32_t queueFamily, VkQueue queue,
                   VkRenderPass renderPass, uint32_t imageCount,
                   VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
    
    // Cleanup
    void cleanup();
    
    // Start a new ImGui frame
    void newFrame();
    
    // Render ImGui draw data
    void render(VkCommandBuffer commandBuffer);
    
    // Upload fonts and other resources
    bool uploadFonts(VkCommandPool commandPool);
    
    // UI rendering functions
    void renderDebugUI(const VulkanRenderer* renderer);
    void renderStatsWindow(float fps, uint32_t nodeCount, uint32_t triangleCount);
    void renderCameraWindow(const glm::vec3& position, const glm::vec3& forward);
    void renderSettingsWindow();
    
    // Getters for UI state
    bool wantsCaptureMouse() const { return ImGui::GetIO().WantCaptureMouse; }
    bool wantsCaptureKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
    
    // UI state
    struct UIState {
        bool showDemo = false;
        bool showStats = true;
        bool showCamera = true;
        bool showSettings = false;
        bool showConsole = false;
        
        // Settings that can be modified
        int renderMode = 0;
        float cameraSpeed = 1000.0f;
        bool wireframe = false;
        bool vsync = true;
        float lodQuality = 1.0f;
        bool pauseSimulation = false;
    } uiState;
    
private:
    bool initialized = false;
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    
    // Helper to create descriptor pool for ImGui
    bool createDescriptorPool();
};

} // namespace rendering