#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

namespace utils {

class Screenshot {
public:
    // Initialize screenshot system (creates/cleans screenshot_dev folder)
    static void initialize();
    
    // Clean up old screenshots in screenshot_dev folder
    static void cleanupOldScreenshots();
    
    // Capture screenshot from Vulkan swapchain image
    static bool captureVulkan(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue queue,
        VkImage sourceImage,
        VkFormat imageFormat,
        VkExtent2D imageExtent,
        const std::string& filename
    );
    
    // Save raw RGBA data to PNG file
    static bool saveRGBA(
        const uint8_t* data,
        uint32_t width,
        uint32_t height,
        const std::string& filename
    );
    
    // Save raw RGB data to PNG file
    static bool saveRGB(
        const uint8_t* data,
        uint32_t width,
        uint32_t height,
        const std::string& filename
    );
    
    // Generate filename with timestamp
    static std::string generateFilename(float elapsedSeconds, float simulationYears);
    
private:
    // Helper to create staging buffer for image data
    static bool createStagingBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBuffer& buffer,
        VkDeviceMemory& memory
    );
    
    // Helper to transition image layout
    static void transitionImageLayout(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
    
    // Helper to copy image to buffer
    static void copyImageToBuffer(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkImage image,
        VkBuffer buffer,
        VkExtent2D extent
    );
    
    // Find memory type index
    static uint32_t findMemoryType(
        VkPhysicalDevice physicalDevice,
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties
    );
    
    // Single-use command buffer helpers
    static VkCommandBuffer beginSingleTimeCommands(
        VkDevice device,
        VkCommandPool commandPool
    );
    
    static void endSingleTimeCommands(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkCommandBuffer commandBuffer
    );
};

} // namespace utils