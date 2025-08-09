#include "rendering/imgui_manager.hpp"
#include "rendering/vulkan_renderer.hpp"
#include "core/camera.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <iostream>
#include <glm/glm.hpp>

namespace rendering {

ImGuiManager::~ImGuiManager() {
    cleanup();
}

bool ImGuiManager::initialize(GLFWwindow* window, VkInstance instance, 
                              VkPhysicalDevice physicalDevice, VkDevice vkDevice,
                              uint32_t queueFamily, VkQueue queue,
                              VkRenderPass renderPass, uint32_t imageCount,
                              VkSampleCountFlagBits msaaSamples) {
    
    if (initialized) return true;
    
    device = vkDevice;
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    
    // Create descriptor pool for ImGui
    if (!createDescriptorPool()) {
        std::cerr << "Failed to create ImGui descriptor pool\n";
        return false;
    }
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = vkDevice;
    init_info.QueueFamily = queueFamily;
    init_info.Queue = queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = imageCount;
    init_info.MSAASamples = msaaSamples;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = renderPass;
    
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        std::cerr << "Failed to initialize ImGui Vulkan backend\n";
        return false;
    }
    
    initialized = true;
    return true;
}

void ImGuiManager::cleanup() {
    if (!initialized) return;
    
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    
    initialized = false;
}

bool ImGuiManager::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
        return false;
    }
    
    return true;
}

bool ImGuiManager::uploadFonts(VkCommandPool commandPool) {
    // In ImGui 1.92.1, font upload is handled automatically on first NewFrame()
    // The backend creates its own command buffer internally
    // This function is kept for compatibility but is no longer needed
    // You can call ImGui_ImplVulkan_CreateFontsTexture() (no parameters) to manually reload fonts
    (void)commandPool; // Suppress unused parameter warning
    return true;
}

void ImGuiManager::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::render(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
}

void ImGuiManager::renderDebugUI(const VulkanRenderer* renderer, const core::Camera* camera) {
    // Show demo window if enabled
    if (uiState.showDemo) {
        ImGui::ShowDemoWindow(&uiState.showDemo);
    }
    
    // Main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &uiState.showStats);
            ImGui::MenuItem("Camera", nullptr, &uiState.showCamera);
            ImGui::MenuItem("Settings", nullptr, &uiState.showSettings);
            ImGui::MenuItem("Console", nullptr, &uiState.showConsole);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &uiState.showDemo);
            ImGui::EndMenu();
        }
        
        // Display FPS in menu bar
        ImGui::Separator();
        if (renderer) {
            float frameTime = renderer->getFrameTime();
            float fps = frameTime > 0.0f ? 1.0f / frameTime : 0.0f;  // frameTime is in seconds
            ImGui::Text("FPS: %.1f", fps);
        }
        
        ImGui::EndMainMenuBar();
    }
    
    // Render individual windows based on state
    if (uiState.showStats && renderer) {
        float frameTime = renderer->getFrameTime();
        float fps = frameTime > 0.0f ? 1.0f / frameTime : 0.0f;  // frameTime is in seconds
        renderStatsWindow(fps, 
                         renderer->getChunkCount(), 
                         renderer->getTriangleCount()); // Get actual triangle count from Transvoxel
    }
    
    // Show camera window with actual camera data if available
    if (uiState.showCamera) {
        if (camera) {
            renderCameraWindow(camera->getPosition(), camera->getForward());
        } else {
            renderCameraWindow(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
        }
    }
    
    // Show settings window
    if (uiState.showSettings) {
        renderSettingsWindow();
    }
}

void ImGuiManager::renderStatsWindow(float fps, uint32_t chunkCount, uint32_t triangleCount) {
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Statistics", &uiState.showStats)) {
        ImGui::Text("Performance");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame Time: %.3f ms", fps > 0.0f ? 1000.0f / fps : 0.0f);
        
        ImGui::Spacing();
        ImGui::Text("Rendering");
        ImGui::Separator();
        ImGui::Text("Active Chunks: %u", chunkCount);
        ImGui::Text("Triangles: %u", triangleCount);
        ImGui::Text("Vertices: %u", triangleCount * 3);
        
        // Frame time graph
        static float frameTimeHistory[120] = {0};
        static int frameTimeOffset = 0;
        frameTimeHistory[frameTimeOffset] = 1000.0f / fps;
        frameTimeOffset = (frameTimeOffset + 1) % IM_ARRAYSIZE(frameTimeHistory);
        
        ImGui::Spacing();
        ImGui::PlotLines("Frame Time", frameTimeHistory, IM_ARRAYSIZE(frameTimeHistory), 
                        frameTimeOffset, nullptr, 0.0f, 50.0f, ImVec2(0, 60));
    }
    ImGui::End();
}

void ImGuiManager::renderCameraWindow(const glm::vec3& position, const glm::vec3& forward) {
    ImGui::SetNextWindowPos(ImVec2(10, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 320), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Camera & Debug", &uiState.showCamera)) {
        // Critical debug information - always visible!
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "=== RENDER DEBUG ===");
        
        ImGui::Text("Camera Pos: (%.3e, %.3e, %.3e)", position.x, position.y, position.z);
        
        float distanceToOrigin = glm::length(position);
        float planetRadius = 6371000.0f; // Earth radius
        float distanceToSurface = distanceToOrigin - planetRadius;
        
        ImGui::Text("Distance to origin: %.3e m", distanceToOrigin);
        ImGui::Text("Distance to surface: %.3e m", distanceToSurface);
        
        ImGui::Separator();
        ImGui::Text("Clipping & FOV:");
        // These are the values we're using in vulkan_renderer.cpp
        ImGui::Text("  Near: %.3f", 0.1f);
        ImGui::Text("  Far: %.1f", 50000.0f);
        ImGui::Text("  FOV: %.1f degrees", 60.0f);
        
        ImGui::Separator();
        ImGui::Text("View Info:");
        // TODO: Get matrix info when camera is passed through
        ImGui::Text("  View and Proj matrices need camera access");
        
        ImGui::Separator();
        ImGui::Text("Forward: (%.3f, %.3f, %.3f)", forward.x, forward.y, forward.z);
        
        ImGui::Spacing();
        ImGui::SliderFloat("Speed", &uiState.cameraSpeed, 100.0f, 100000.0f, "%.0f m/s", ImGuiSliderFlags_Logarithmic);
    }
    ImGui::End();
}

void ImGuiManager::renderSettingsWindow() {
    ImGui::SetNextWindowPos(ImVec2(320, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Settings", &uiState.showSettings)) {
        ImGui::Text("Render Mode");
        const char* modes[] = {
            "Material", "Temperature", "Velocity", "Age",
            "Plate IDs", "Stress", "Density", "Elevation"
        };
        ImGui::Combo("##rendermode", &uiState.renderMode, modes, IM_ARRAYSIZE(modes));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Display Options");
        ImGui::Checkbox("Wireframe", &uiState.wireframe);
        ImGui::Checkbox("VSync", &uiState.vsync);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("LOD Settings");
        ImGui::SliderFloat("Quality", &uiState.lodQuality, 0.1f, 2.0f);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Simulation");
        ImGui::Checkbox("Pause", &uiState.pauseSimulation);
        
        if (ImGui::Button("Reset Camera")) {
            // TODO: Trigger camera reset
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Screenshot")) {
            // TODO: Trigger screenshot
        }
    }
    ImGui::End();
}

} // namespace rendering