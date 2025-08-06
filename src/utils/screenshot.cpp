#include "utils/screenshot.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #define MKDIR(dir) _mkdir(dir)
    #define ACCESS(path, mode) _access(path, mode)
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define MKDIR(dir) mkdir(dir, 0755)
    #define ACCESS(path, mode) access(path, mode)
#endif

namespace utils {

void Screenshot::initialize() {
    // Create screenshot_dev directory if it doesn't exist
    const char* screenshotDir = "screenshot_dev";
    
    // Check if directory exists
    if (ACCESS(screenshotDir, 0) != 0) {
        // Directory doesn't exist, create it
        if (MKDIR(screenshotDir) == 0) {
            std::cout << "Created screenshot_dev directory\n";
        }
    }
    
    // Clean up old screenshots on startup
    cleanupOldScreenshots();
}

void Screenshot::cleanupOldScreenshots() {
    // Simple cleanup - just note that we're starting fresh
    std::cout << "Ready for new screenshots in screenshot_dev\n";
    // In a real implementation, we'd iterate through files
    // For now, this is just a placeholder
}

bool Screenshot::captureVulkan(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage sourceImage,
    VkFormat imageFormat,
    VkExtent2D imageExtent,
    const std::string& filename) {
    
    // For stub implementation, just create a dummy image
    return saveRGBA(nullptr, imageExtent.width, imageExtent.height, filename);
}

bool Screenshot::saveRGBA(const uint8_t* data, uint32_t width, uint32_t height, const std::string& filename) {
    std::string filepath = std::string("screenshot_dev/") + filename;
    
    // If no data provided, create a dummy gradient
    std::vector<uint8_t> dummyData;
    if (!data) {
        dummyData.resize(width * height * 4);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t idx = (y * width + x) * 4;
                dummyData[idx + 0] = static_cast<uint8_t>((x * 255) / width);     // R
                dummyData[idx + 1] = static_cast<uint8_t>((y * 255) / height);    // G
                dummyData[idx + 2] = 128;                                         // B
                dummyData[idx + 3] = 255;                                         // A
            }
        }
        data = dummyData.data();
    }
    
    // Use stb_image_write to save PNG
    int result = stbi_write_png(filepath.c_str(), width, height, 4, data, width * 4);
    
    if (result == 0) {
        std::cerr << "Failed to save screenshot: " << filepath << "\n";
        return false;
    }
    
    return true;
}

bool Screenshot::saveRGB(const uint8_t* data, uint32_t width, uint32_t height, const std::string& filename) {
    std::string filepath = std::string("screenshot_dev/") + filename;
    
    // Use stb_image_write to save PNG
    int result = stbi_write_png(filepath.c_str(), width, height, 3, data, width * 3);
    
    if (result == 0) {
        std::cerr << "Failed to save screenshot: " << filepath << "\n";
        return false;
    }
    
    return true;
}

std::string Screenshot::generateFilename(float elapsedSeconds, float simulationYears) {
    std::stringstream ss;
    ss << "screenshot_"
       << std::fixed << std::setprecision(0) << elapsedSeconds << "s_"
       << std::fixed << std::setprecision(2) << simulationYears << "My.png";
    return ss.str();
}

// Stub implementations for Vulkan-specific functions
bool Screenshot::createStagingBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBuffer& buffer,
    VkDeviceMemory& memory) {
    return false; // Stub
}

void Screenshot::transitionImageLayout(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout) {
    // Stub
}

void Screenshot::copyImageToBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkBuffer buffer,
    VkExtent2D extent) {
    // Stub
}

uint32_t Screenshot::findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties) {
    return 0; // Stub
}

VkCommandBuffer Screenshot::beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
    return VK_NULL_HANDLE; // Stub
}

void Screenshot::endSingleTimeCommands(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkCommandBuffer commandBuffer) {
    // Stub
}

} // namespace utils