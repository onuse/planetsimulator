#include "rendering/vulkan_renderer.hpp"
#include "rendering/gpu_octree.hpp"
#include "utils/screenshot.hpp"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <set>

namespace rendering {

// Define static members
VulkanRenderer::MeshPipeline VulkanRenderer::meshPipeline = VulkanRenderer::MeshPipeline::CPU_ADAPTIVE;

// ============================================================================
// Constructor and Main Interface
// ============================================================================

VulkanRenderer::VulkanRenderer(uint32_t width, uint32_t height) 
    : windowWidth(width), windowHeight(height), instanceBuffer(VK_NULL_HANDLE), 
      instanceBufferMemory(VK_NULL_HANDLE), instanceBufferMapped(nullptr),
      materialTableBuffer(VK_NULL_HANDLE), materialTableBufferMemory(VK_NULL_HANDLE),
      pipelineLayout(VK_NULL_HANDLE), graphicsPipeline(VK_NULL_HANDLE), wireframePipeline(VK_NULL_HANDLE) {
    lastFrameTime = std::chrono::steady_clock::now();
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::initialize() {
    try {
        createWindow();
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
//         createGraphicsPipeline();
        createCommandPool();
        createDepthResources();
        createFramebuffers();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createMaterialTableBuffer();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
        
        // Initialize Transvoxel descriptor sets
        
        // Initialize ImGui
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        if (!imguiManager.initialize(window, instance, physicalDevice, device,
                                    indices.graphicsFamily.value(), graphicsQueue,
                                    renderPass, static_cast<uint32_t>(swapChainImages.size()))) {
            throw std::runtime_error("Failed to initialize ImGui");
        }
        
        // Initialize GPU octree for mesh generation
        gpuOctree = std::make_unique<rendering::GPUOctree>(device, physicalDevice);
        std::cout << "[VulkanRenderer] GPU octree initialized at " << gpuOctree.get() << std::endl;
        
        // Initialize GPU mesh pipeline
        createTransvoxelPipeline();
        
        // Create Quadtree pipeline (if needed for visualization)
        createQuadtreePipeline();
        
        // Vulkan renderer initialized successfully
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Vulkan renderer: " << e.what() << std::endl;
        return false;
    }
}

void VulkanRenderer::render(octree::OctreePlanet* planet, core::Camera* camera) {
    // Store camera for debug display
    currentCamera = camera;
    
    // static int renderCalls = 0;
    // std::cout << "VulkanRenderer::render called (call #" << ++renderCalls << ")" << std::endl;
    
    // Update frame timing
    auto currentTime = std::chrono::steady_clock::now();
    frameTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;
    
    // Update LOD level based on camera distance every frame
    if (camera && planet) {
        float distanceToOrigin = glm::length(camera->getPosition());
        float planetRadius = planet->getRadius();
        float distanceToSurface = distanceToOrigin - planetRadius;
        
        // More granular LOD levels based on distance
        // Each level roughly doubles triangle count
        if (distanceToSurface > planetRadius * 100.0f) {
            currentLODLevel = 1; // Extreme distance: 20 triangles
        } else if (distanceToSurface > planetRadius * 50.0f) {
            currentLODLevel = 2; // Very far: 80 triangles
        } else if (distanceToSurface > planetRadius * 20.0f) {
            currentLODLevel = 3; // Far: 320 triangles
        } else if (distanceToSurface > planetRadius * 10.0f) {
            currentLODLevel = 4; // Medium-far: 1,280 triangles
        } else if (distanceToSurface > planetRadius * 5.0f) {
            currentLODLevel = 5; // Medium: 5,120 triangles
        } else if (distanceToSurface > planetRadius * 2.0f) {
            currentLODLevel = 6; // Medium-close: 20,480 triangles
        } else if (distanceToSurface > planetRadius * 1.0f) {
            currentLODLevel = 7; // Close: 81,920 triangles
        } else if (distanceToSurface > planetRadius * 0.5f) {
            currentLODLevel = 8; // Very close: 327,680 triangles
        } else if (distanceToSurface > planetRadius * 0.1f) {
            currentLODLevel = 9; // Extremely close: 1,310,720 triangles
        } else if (distanceToSurface > planetRadius * 0.01f) {
            currentLODLevel = 10; // Surface level: 5,242,880 triangles
        } else {
            currentLODLevel = 11; // Below surface: maximum detail
        }
    }
    
    // Regenerate mesh when LOD changes OR camera moves significantly (for dual-detail adaptation)
    static int lastLODLevel = -1;
    static glm::vec3 lastCameraPos = glm::vec3(0, 0, 0);
    
    // Check if camera moved significantly (more than 10% of its distance to planet)
    glm::vec3 currentCameraPos = camera->getPosition();
    float cameraMoveDistance = glm::length(currentCameraPos - lastCameraPos);
    float distanceToCenter = glm::length(currentCameraPos);
    bool significantCameraMove = (cameraMoveDistance > distanceToCenter * 0.1f);
    
    // TEMPORARILY disable camera movement trigger to test dual-detail
    if (planet && camera && (currentLODLevel != lastLODLevel /* || significantCameraMove */)) {
        // LOD changed or camera moved significantly - regenerate mesh
        if (currentLODLevel != lastLODLevel) {
            std::cout << "[LOD] Level changed from " << lastLODLevel << " to " << currentLODLevel << " - regenerating mesh\n";
        } else if (significantCameraMove) {
            std::cout << "[CAMERA] Significant camera movement detected (moved " 
                      << cameraMoveDistance / planet->getRadius() << " radii) - regenerating mesh for dual-detail adaptation\n";
        }
        lastLODLevel = currentLODLevel;
        lastCameraPos = currentCameraPos;
        
        // MASTER PIPELINE SWITCH - Crystal clear pipeline selection
        bool meshGenerated = false;
        
        switch (meshPipeline) {
            case MeshPipeline::CPU_ADAPTIVE:
                // Current working implementation
                meshGenerated = generateAdaptiveSphere(planet, camera);
                if (!meshGenerated) {
                    std::cerr << "ERROR: CPU adaptive sphere generation failed!\n";
                }
                break;
                
            case MeshPipeline::GPU_COMPUTE:
                // Future GPU implementation
                meshGenerated = generateGPUMesh(planet, camera);
                if (!meshGenerated) {
                    std::cerr << "ERROR: GPU compute mesh generation failed!\n";
                }
                break;
                
            case MeshPipeline::GPU_WITH_CPU_VERIFY: {
                // Debug mode - run both separately but use GPU result
                std::cout << "[VERIFY MODE] Running GPU mesh generation for verification...\n";
                
                // Run GPU first
                bool gpuSuccess = generateGPUMesh(planet, camera);
                
                // Store GPU mesh counts for comparison
                size_t gpuVertexCount = meshVertexCount;
                size_t gpuIndexCount = meshIndexCount;
                
                // Now run CPU to compare counts (but don't use its buffers)
                // We need a separate function that doesn't overwrite buffers
                // For now, just report GPU results
                
                if (gpuSuccess) {
                    std::cout << "[VERIFY MODE] GPU generated " << gpuVertexCount 
                              << " vertices, " << (gpuIndexCount/3) << " triangles\n";
                    meshGenerated = true;
                } else {
                    std::cerr << "[VERIFY MODE] GPU generation failed!\n";
                    // Fall back to CPU
                    meshGenerated = generateAdaptiveSphere(planet, camera);
                }
                break;
            }
        }
        
        if (!meshGenerated) {
            // NO FALLBACKS! FAIL LOUDLY AND CLEARLY!
            std::cerr << "\n================================\n";
            std::cerr << "MESH GENERATION FAILED!\n";
            std::cerr << "Pipeline: " << (int)meshPipeline << "\n";
            std::cerr << "Press G to switch pipeline\n";
            std::cerr << "================================\n\n";
            // DO NOT try another method - that way lies madness
        }
    }
    
    // Draw the frame
    // std::cout << "  About to call drawFrame..." << std::endl;
    drawFrame(planet, camera);
    // std::cout << "  drawFrame returned" << std::endl;
}

void VulkanRenderer::cleanup() {
    // Try to wait for device idle, but don't hang forever
    // This prevents zombie processes when GPU is hung
    VkResult result = vkDeviceWaitIdle(device);
    if (result == VK_ERROR_DEVICE_LOST) {
        std::cerr << "WARNING: Device lost during cleanup, skipping wait" << std::endl;
        // Continue cleanup anyway to release resources
    } else if (result != VK_SUCCESS) {
        std::cerr << "WARNING: vkDeviceWaitIdle failed with error: " << result << std::endl;
    }
    
    // Cleanup ImGui
    imguiManager.cleanup();
    
    // GPU mesh cleanup is handled by buffer destruction
    
    // Cleanup Transvoxel pipelines
    if (hierarchicalPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, hierarchicalPipeline, nullptr);
        hierarchicalPipeline = VK_NULL_HANDLE;
    }
    if (trianglePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, trianglePipeline, nullptr);
        trianglePipeline = VK_NULL_HANDLE;
    }
    if (hierarchicalPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, hierarchicalPipelineLayout, nullptr);
        hierarchicalPipelineLayout = VK_NULL_HANDLE;
    }
    if (hierarchicalDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, hierarchicalDescriptorSetLayout, nullptr);
    }
    
    // Cleanup Quadtree pipeline
    if (quadtreePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, quadtreePipeline, nullptr);
        quadtreePipeline = VK_NULL_HANDLE;
    }
    if (quadtreePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, quadtreePipelineLayout, nullptr);
        quadtreePipelineLayout = VK_NULL_HANDLE;
    }
    if (quadtreeDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, quadtreeDescriptorSetLayout, nullptr);
        quadtreeDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    cleanupSwapChain();
    
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }
    
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    
    // Clean up material table buffer
    if (materialTableBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, materialTableBuffer, nullptr);
        vkFreeMemory(device, materialTableBufferMemory, nullptr);
    }
    
    // Clean up instance buffer if it exists
    if (instanceBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, instanceBuffer, nullptr);
    }
    if (instanceBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, instanceBufferMemory, nullptr);
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    
    vkDestroyCommandPool(device, commandPool, nullptr);
    
    vkDestroyDevice(device, nullptr);
    
    if (enableValidationLayers) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, nullptr);
        }
    }
    
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    
    glfwDestroyWindow(window);
    glfwTerminate();
}

// ============================================================================
// Window Management
// ============================================================================

void VulkanRenderer::createWindow() {
    glfwInit();
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    window = glfwCreateWindow(windowWidth, windowHeight, "Octree Planet Simulator", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    
    // Set input callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

bool VulkanRenderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void VulkanRenderer::pollEvents() {
    glfwPollEvents();
}

void VulkanRenderer::resize(uint32_t width, uint32_t height) {
    windowWidth = width;
    windowHeight = height;
    framebufferResized = true;
}

void VulkanRenderer::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto app = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

// ============================================================================
// Instance Creation
// ============================================================================

void VulkanRenderer::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Octree Planet";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        
        if (!layerFound) {
            return false;
        }
    }
    
    return true;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    return extensions;
}

void VulkanRenderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, &createInfo, nullptr, &debugMessenger);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {
    
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    
    return VK_FALSE;
}

// ============================================================================
// Surface Creation
// ============================================================================

void VulkanRenderer::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

// ============================================================================
// Input Handling
// ============================================================================

void VulkanRenderer::updateInput() {
    // Update previous state
    for (int i = 0; i < 512; i++) {
        inputState.prevKeys[i] = inputState.keys[i];
    }
    for (int i = 0; i < 8; i++) {
        inputState.prevMouseButtons[i] = inputState.mouseButtons[i];
    }
    
    // Reset deltas
    inputState.mouseDelta = glm::vec2(0, 0);
    inputState.scrollDelta = glm::vec2(0, 0);
}

void VulkanRenderer::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    auto* renderer = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer || key < 0 || key >= 512) return;
    
    if (action == GLFW_PRESS) {
        renderer->inputState.keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        renderer->inputState.keys[key] = false;
    }
}

void VulkanRenderer::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* renderer = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) return;
    
    renderer->inputState.mousePos = glm::vec2(xpos, ypos);
    
    if (renderer->inputState.firstMouse) {
        renderer->inputState.lastMousePos = renderer->inputState.mousePos;
        renderer->inputState.firstMouse = false;
    }
    
    renderer->inputState.mouseDelta = renderer->inputState.mousePos - renderer->inputState.lastMousePos;
    renderer->inputState.lastMousePos = renderer->inputState.mousePos;
}

void VulkanRenderer::mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* renderer = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer || button < 0 || button >= 8) return;
    
    if (action == GLFW_PRESS) {
        renderer->inputState.mouseButtons[button] = true;
    } else if (action == GLFW_RELEASE) {
        renderer->inputState.mouseButtons[button] = false;
    }
}

void VulkanRenderer::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* renderer = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) return;
    
    renderer->inputState.scrollDelta = glm::vec2(xoffset, yoffset);
}

// This file continues in vulkan_renderer_part2.cpp...

void VulkanRenderer::dumpVertexData() {
    // GPU mesh vertex dump
    if (meshVertexCount > 0 && meshIndexCount > 0) {
        std::cout << "GPU mesh has " << meshVertexCount << " vertices and " 
                  << (meshIndexCount/3) << " triangles\n";
        // TODO: Implement GPU buffer readback for debugging
    } else {
        std::cout << "No GPU mesh data to dump\n";
    }
}

void VulkanRenderer::renderGPUMesh() {
    if (meshVertexCount == 0 || meshIndexCount == 0) {
        static int skipCount = 0;
        if (skipCount++ % 60 == 0) {
            std::cout << "[renderGPUMesh] Skipping - no mesh data (" << meshVertexCount << " verts, " << meshIndexCount << " indices)\n";
        }
        return;
    }
    
    // We need currentCommandBuffer to be set - this should be called from drawFrame
    if (!currentCommandBuffer) {
        std::cerr << "[GPU MESH] No current command buffer!\n";
        return;
    }
    
    // Bind the triangle pipeline (reuse existing one)
    if (trianglePipeline == VK_NULL_HANDLE) {
        std::cerr << "[GPU MESH] No triangle pipeline!\n";
        return;
    }
    
    vkCmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
    
    // Bind descriptor sets (uniform buffer etc) - reuse from hierarchical pipeline
    if (hierarchicalDescriptorSets.empty()) {
        std::cerr << "[GPU MESH] No descriptor sets!\n";
        return;
    }
    
    vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            hierarchicalPipelineLayout, 0, 1, 
                            &hierarchicalDescriptorSets[currentFrame], 0, nullptr);
    
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {meshVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(currentCommandBuffer, meshIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw indexed
    vkCmdDrawIndexed(currentCommandBuffer, meshIndexCount, 1, 0, 0, 0);
    
    static int drawCallCount = 0;
    if (drawCallCount++ % 60 == 0) {
        // std::cout << "[renderGPUMesh] DRAW CALL EXECUTED: " << meshIndexCount << " indices (" << meshIndexCount/3 << " triangles)\n";
    }
    
    // Print only when mesh changes
    static size_t lastGpuVertexCount = 0;
    static size_t lastGpuIndexCount = 0;
    if (meshVertexCount != lastGpuVertexCount || meshIndexCount != lastGpuIndexCount) {
        std::cout << "[GPU MESH] Rendering " << meshVertexCount << " vertices, " 
                  << (meshIndexCount/3) << " triangles\n";
        lastGpuVertexCount = meshVertexCount;
        lastGpuIndexCount = meshIndexCount;
    }
}

void VulkanRenderer::createTestNDCPipeline() {
    // Empty stub - this test pipeline has been removed
}

} // namespace rendering
