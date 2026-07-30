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
            ImGui::MenuItem("Simulation", nullptr, &uiState.showSimulation);
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

            // There is no single level of detail to report - the surface is a
            // quadtree and every patch chooses for itself - so this is how
            // many patches are on screen and how fine the sharpest one is.
            const VulkanRenderer::PatchStats patches = renderer->getPatchStats();
            ImGui::Text("| %u patches, %u tris", patches.drawn, patches.triangles);
            if (patches.inFlight > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                                   "| building %u", patches.inFlight);
            }
        }
        
        ImGui::EndMainMenuBar();
    }
    
    // Render individual windows based on state
    if (uiState.showStats && renderer) {
        float frameTime = renderer->getFrameTime();
        float fps = frameTime > 0.0f ? 1.0f / frameTime : 0.0f;  // frameTime is in seconds
        renderStatsWindow(renderer, fps, 
                         renderer->getChunkCount(), 
                         renderer->getTriangleCount()); // Get actual triangle count from Transvoxel
    }
    
    // Show camera window with actual camera data if available
    if (uiState.showCamera && renderer) {
        if (camera) {
            renderCameraWindow(camera->getPosition(), camera->getForward(), camera,
                               renderer->getPlanetRadius());
        } else {
            renderCameraWindow(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), nullptr,
                               renderer->getPlanetRadius());
        }
    }
    
    // Show settings window
    if (uiState.showSettings) {
        renderSettingsWindow();
    }

    if (uiState.showSimulation && renderer) {
        renderSimulationWindow(renderer);
    }
}

void ImGuiManager::renderStatsWindow(const VulkanRenderer* renderer, float fps, uint32_t chunkCount, uint32_t triangleCount) {
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 340), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Statistics", &uiState.showStats)) {
        ImGui::Text("Performance");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame Time: %.3f ms", fps > 0.0f ? 1000.0f / fps : 0.0f);
        
        const VulkanRenderer::PatchStats patches = renderer->getPatchStats();

        ImGui::Separator();
        ImGui::Text("Surface");

        // Every line here is a count of something that happened this frame.
        // What was here before reported a global level of detail, which the
        // surface has not had since it became a quadtree - each patch decides
        // for itself - along with the triangle count a table said that level
        // implied, which was a prediction rather than a measurement and was
        // out by two orders of magnitude.
        ImGui::Text("Patches drawn: %u of %u selected", patches.drawn, patches.selected);
        ImGui::Text("  culled: %u over horizon, %u off screen",
                    patches.culledHorizon, patches.culledFrustum);
        ImGui::Text("Triangles: %u", patches.triangles);

        // The level on its own says nothing without knowing the planet, so
        // what it comes to in metres is the number worth reading.
        if (patches.metresPerVertex >= 1000.0f) {
            ImGui::Text("Finest detail: level %u, %.1f km between vertices",
                        patches.finestLevel, patches.metresPerVertex / 1000.0f);
        } else {
            ImGui::Text("Finest detail: level %u, %.1f m between vertices",
                        patches.finestLevel, patches.metresPerVertex);
        }

        ImGui::Spacing();
        ImGui::Text("Patch cache");
        ImGui::Separator();
        ImGui::Text("Cached: %u of %u slots", patches.cached, patches.poolSlots);
        ImGui::ProgressBar(patches.poolSlots > 0
                               ? static_cast<float>(patches.cached) / patches.poolSlots
                               : 0.0f,
                           ImVec2(0, 0), "pool");
        ImGui::Text("Building: %u on %u threads", patches.inFlight, patches.workers);
        
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

void ImGuiManager::renderCameraWindow(const glm::vec3& position, const glm::vec3& forward,
                                      const core::Camera* camera, float planetRadius) {
    ImGui::SetNextWindowPos(ImVec2(10, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 320), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Camera & Debug", &uiState.showCamera)) {
        // Critical debug information - always visible!
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "=== RENDER DEBUG ===");
        
        ImGui::Text("Camera Pos: (%.3e, %.3e, %.3e)", position.x, position.y, position.z);
        
        // The radius here used to be Earth's, written in as a constant while
        // the planet being simulated was a sixth of that - so the altitude
        // read several thousand kilometres below the ground at all times.
        const float distanceToOrigin = glm::length(position);
        const float altitude = distanceToOrigin - planetRadius;

        ImGui::Text("Distance to centre: %.1f km", distanceToOrigin / 1000.0f);
        if (std::abs(altitude) >= 1000.0f) {
            ImGui::Text("Altitude: %.1f km", altitude / 1000.0f);
        } else {
            ImGui::Text("Altitude: %.0f m", altitude);
        }
        
        ImGui::Separator();
        ImGui::Text("Clipping & FOV:");
        if (camera) {
            ImGui::Text("  Near: %.3f", camera->getNearPlane());
            ImGui::Text("  Far: %.1f", camera->getFarPlane());
            ImGui::Text("  FOV: %.1f degrees", camera->getFieldOfView());
        } else {
            ImGui::Text("  Near: %.3f (no camera)", 0.1f);
            ImGui::Text("  Far: %.1f (no camera)", 50000.0f);
            ImGui::Text("  FOV: %.1f degrees (no camera)", 60.0f);
        }
        
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

void ImGuiManager::renderSimulationWindow(const VulkanRenderer* renderer) {
    octree::OctreePlanet* planet = renderer->getPlanet();
    if (planet == nullptr) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(410, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(540, 330), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Simulation", &uiState.showSimulation)) {
        // What the simulation is managing against what was asked of it. The
        // two part company when the tectonics cannot keep up, which is worth
        // seeing rather than guessing at.
        ImGui::Text("Requested: %.3f My/s   Achieved: %.3f My/s",
                    planet->getSimulationRate(), planet->getAchievedSimulationRate());
        ImGui::Separator();

        // Geological time against wall clock time.
        //
        // In thousand-year units, because that is the range worth having.
        // Millions of years is the scale of tectonics and nothing else: rivers
        // cut and abandon channels, glaciers advance and retreat, and coasts
        // move, all of it in thousands of years. At one million years per
        // second every one of those is over before a frame finishes.
        //
        // Logarithmic because the useful span is five orders of magnitude, and
        // a linear slider spends nine tenths of its length on speeds that are
        // all indistinguishably too fast.
        static float pausedRate = 1000.0f;   // kyr/s to return to
        const float currentRate = planet->getSimulationRate();
        const bool paused = currentRate <= 0.0f;

        // The slider owns its own value; it is not re-read from the planet
        // each frame.
        //
        // A logarithmic SliderFloat does not round-trip a value exactly - it
        // converts to a normalised position and back, and the result differs in
        // the last bits. So it reports itself as changed on every frame whether
        // or not anyone touched it, and writing that value back re-quantises it
        // again next frame. The value walks, and with a logarithmic mapping it
        // walks fast: this took the rate from a thousand years a second down to
        // the slider's minimum in a couple of hundred frames and froze the
        // planet, which looked like the simulation thread had died.
        //
        // Two defences. The value lives here rather than being derived from the
        // planet, so there is nothing to re-quantise; and it is only pushed
        // while the control is actually being manipulated.
        static float sliderKyr = 0.0f;
        static bool primed = false;
        if (!primed) {
            primed = true;
            sliderKyr = std::max(currentRate * 1000.0f, 0.001f);
        }
        if (paused) {
            sliderKyr = pausedRate;
        }

        if (ImGui::Button(paused ? "Resume" : "Pause", ImVec2(90, 0))) {
            if (paused) {
                planet->setSimulationRate(pausedRate * 0.001f);
                sliderKyr = pausedRate;
            } else {
                pausedRate = currentRate * 1000.0f;
                planet->setSimulationRate(0.0f);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled(paused ? "the planet is holding still"
                                   : "geological time is running");

        // Down to a thousandth of a thousand years, which is one year per
        // second. That is a deliberate floor rather than an arbitrary one: at a
        // year a second nothing within a year has to be resolved, so there is
        // no need to model seasons to be honest about what is shown.
        const bool moved = ImGui::SliderFloat("thousand years / sec", &sliderKyr,
                                              0.001f, 5000.0f, "%.3f kyr/s",
                                              ImGuiSliderFlags_Logarithmic);
        if (moved && ImGui::IsItemActive()) {
            if (paused) {
                pausedRate = sliderKyr;
            } else {
                planet->setSimulationRate(sliderKyr * 0.001f);
            }
        }

        struct Preset { const char* name; float kyr; const char* what; };
        static const Preset presets[] = {
            {"A year",    0.001f, "one year a second; no seasons to resolve"},
            {"Rivers",    0.5f,   "channels shift and deltas build"},
            {"Ice",       20.0f,  "glaciers advance and retreat"},
            {"Coasts",    200.0f, "shorelines move, ranges rise"},
            {"Tectonics", 2000.0f, "oceans open and close"},
        };

        for (const Preset& preset : presets) {
            if (ImGui::Button(preset.name, ImVec2(90, 0))) {
                planet->setSimulationRate(preset.kyr * 0.001f);
                sliderKyr = preset.kyr;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%.4g kyr/s - %s", preset.kyr, preset.what);
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Surface patches are rebuilt whenever the crust moves, so slowing "
            "time also settles the terrain - which is what makes it possible "
            "to judge detail rather than watch it change.\n\n"
            "Clouds fade out above a few thousand years per second. The "
            "atmosphere is solved as an equilibrium, so at geological speeds "
            "the weather is a still photograph of a sky that should have "
            "changed millions of times.");
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