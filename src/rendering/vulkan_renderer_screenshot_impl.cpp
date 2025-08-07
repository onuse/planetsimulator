#include "rendering/vulkan_renderer.hpp"
#include "stb_image_write.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <filesystem>

namespace rendering {

bool VulkanRenderer::captureScreenshot(const std::string& filename) {
    // Wait for the device to be idle before capturing
    vkDeviceWaitIdle(device);
    
    // Get the last rendered swap chain image (not currentFrame which is for frame-in-flight resources)
    VkImage srcImage = swapChainImages[lastRenderedImageIndex];
    
    // Create a staging buffer for the image data
    VkDeviceSize imageSize = swapChainExtent.width * swapChainExtent.height * 4; // RGBA
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);
    
    // Transition the swap chain image to transfer source
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    // Transition image layout from present to transfer source
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy image to buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {swapChainExtent.width, swapChainExtent.height, 1};
    
    vkCmdCopyImageToBuffer(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          stagingBuffer, 1, &region);
    
    // Transition back to present
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    vkEndCommandBuffer(commandBuffer);
    
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Map buffer and read pixel data
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    
    // Convert from BGRA to RGBA
    std::vector<uint8_t> pixels(imageSize);
    memcpy(pixels.data(), data, imageSize);
    
    // Swap R and B channels (BGRA -> RGBA)
    for (size_t i = 0; i < pixels.size(); i += 4) {
        std::swap(pixels[i], pixels[i + 2]);
    }
    
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Get executable directory and create screenshot_dev there
    std::filesystem::path exePath = std::filesystem::current_path();
    std::filesystem::path screenshotDir = exePath / "screenshot_dev";
    
    // Create directory if it doesn't exist
    if (!std::filesystem::exists(screenshotDir)) {
        std::filesystem::create_directories(screenshotDir);
        std::cout << "Created screenshot directory: " << screenshotDir.string() << std::endl;
    }
    
    // Save to file using stb_image_write
    std::filesystem::path fullPath = screenshotDir / filename;
    int result = stbi_write_png(fullPath.string().c_str(), swapChainExtent.width, swapChainExtent.height,
                               4, pixels.data(), swapChainExtent.width * 4);
    
    // Cleanup
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    if (result) {
        std::cout << "Screenshot saved: " << fullPath.string() << std::endl;
        return true;
    } else {
        std::cerr << "Failed to save screenshot: " << fullPath.string() << std::endl;
        return false;
    }
}

} // namespace rendering