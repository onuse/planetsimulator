#include "rendering/vulkan_renderer.hpp"
#include "rendering/gpu_octree.hpp"
#include "rendering/hierarchical_gpu_octree.hpp"
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

// ============================================================================
// Constructor and Main Interface
// ============================================================================

VulkanRenderer::VulkanRenderer(uint32_t width, uint32_t height) 
    : windowWidth(width), windowHeight(height), instanceBuffer(VK_NULL_HANDLE), 
      instanceBufferMemory(VK_NULL_HANDLE), instanceBufferMapped(nullptr),
      materialTableBuffer(VK_NULL_HANDLE), materialTableBufferMemory(VK_NULL_HANDLE) {
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
        createGraphicsPipeline();
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
        
        // Initialize ImGui
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        if (!imguiManager.initialize(window, instance, physicalDevice, device,
                                    indices.graphicsFamily.value(), graphicsQueue,
                                    renderPass, static_cast<uint32_t>(swapChainImages.size()))) {
            throw std::runtime_error("Failed to initialize ImGui");
        }
        
        // Enable GPU octree ray marching for better performance
        if (useGPUOctree) {
            gpuOctree = std::make_unique<GPUOctree>(device, physicalDevice);
            createRayMarchPipeline();
            std::cout << "GPU octree ray marching initialized\n";
        }
        
        // Enable hierarchical GPU octree with frustum culling and LOD
        if (useHierarchicalOctree) {
            hierarchicalGpuOctree = std::make_unique<HierarchicalGPUOctree>(device, physicalDevice);
            createRayMarchPipeline();  // Can reuse the same pipeline
            std::cout << "Hierarchical GPU octree with frustum culling initialized\n";
        }
        
        std::cout << "Vulkan renderer initialized successfully\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Vulkan renderer: " << e.what() << std::endl;
        return false;
    }
}

void VulkanRenderer::render(octree::OctreePlanet* planet, core::Camera* camera) {
    // static int renderCalls = 0;
    // std::cout << "VulkanRenderer::render called (call #" << ++renderCalls << ")" << std::endl;
    
    // Update frame timing
    auto currentTime = std::chrono::steady_clock::now();
    frameTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;
    
    // Dynamically adjust near/far planes based on distance to planet
    // This fixes depth buffer precision issues
    glm::vec3 cameraPos = camera->getPosition();
    float distanceToOrigin = glm::length(cameraPos);
    float planetRadius = planet->getRadius();
    float distanceToPlanetSurface = std::max(100.0f, distanceToOrigin - planetRadius);
    
    // Set near/far planes for good depth precision (ratio ~10,000:1)
    float nearPlane = distanceToPlanetSurface * 0.001f;  // 0.1% of distance
    float farPlane = distanceToPlanetSurface * 100.0f;   // 100x distance (enough to see whole planet)
    camera->setNearFar(nearPlane, farPlane);
    
    // Upload octree to GPU if using GPU octree (do this once or when octree changes)
    static bool octreeUploaded = false;
    if (useGPUOctree && gpuOctree && !octreeUploaded) {
        // Get camera matrices for proper filtering
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 proj = camera->getProjectionMatrix(static_cast<float>(windowWidth) / static_cast<float>(windowHeight));
        glm::mat4 viewProj = proj * view;
        glm::vec3 viewPos = camera->getPosition();
        
        // Upload with proper filtering using prepareRenderData
        gpuOctree->uploadOctree(planet, viewPos, viewProj, commandPool, graphicsQueue);
        createRayMarchDescriptorSets();  // Create descriptor sets after uploading
        octreeUploaded = true;
        std::cout << "Octree uploaded to GPU\n";
    }
    
    // Upload hierarchical octree with view-dependent culling
    if (useHierarchicalOctree && hierarchicalGpuOctree) {
        // Build view-projection matrix for frustum culling
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 proj = camera->getProjectionMatrix(static_cast<float>(windowWidth) / static_cast<float>(windowHeight));
        glm::mat4 viewProj = proj * view;
        glm::vec3 viewPos = camera->getPosition();
        
        // Upload with frustum culling and LOD selection
        hierarchicalGpuOctree->uploadOctree(planet, viewProj, viewPos, commandPool, graphicsQueue);
        
        // Update descriptor sets if needed
        if (!octreeUploaded) {
            createRayMarchDescriptorSets();
            octreeUploaded = true;
        }
        
        // Print visibility stats
        const auto& visInfo = hierarchicalGpuOctree->getVisibilityInfo();
        if (visInfo.totalNodes > 0) {
            float cullingRatio = static_cast<float>(visInfo.culledNodes) / visInfo.totalNodes;
            // Debug output only occasionally to avoid spam
            static int frameCounter = 0;
            if (frameCounter++ % 60 == 0) {
                std::cout << "Hierarchical octree: " << visInfo.visibleNodes.size() 
                          << " visible / " << visInfo.totalNodes << " total (" 
                          << (cullingRatio * 100.0f) << "% culled)\n";
            }
        }
    }
    
    // Draw the frame
    // std::cout << "  About to call drawFrame..." << std::endl;
    drawFrame(planet, camera);
    // std::cout << "  drawFrame returned" << std::endl;
}

void VulkanRenderer::cleanup() {
    vkDeviceWaitIdle(device);
    
    // Cleanup ImGui
    imguiManager.cleanup();
    
    // Cleanup ray march pipeline if created
    if (rayMarchPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, rayMarchPipeline, nullptr);
    }
    if (rayMarchPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, rayMarchPipelineLayout, nullptr);
    }
    if (rayMarchDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, rayMarchDescriptorSetLayout, nullptr);
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

void VulkanRenderer::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
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
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
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

void VulkanRenderer::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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

void VulkanRenderer::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
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

} // namespace rendering