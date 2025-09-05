// CPU mesh upload functionality
#include "rendering/vulkan_renderer.hpp"
#include <iostream>
#include <cstring>

namespace rendering {

bool VulkanRenderer::uploadCPUReferenceMesh(const void* vertexData, size_t vertexDataSize,
                                            const void* indexData, size_t indexDataSize,
                                            uint32_t vertexCount, uint32_t indexCount) {
    // Store counts
    meshVertexCount = vertexCount;
    meshIndexCount = indexCount;
    
    // Create or recreate vertex buffer
    if (meshVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshVertexBuffer, nullptr);
        vkFreeMemory(device, meshVertexBufferMemory, nullptr);
    }
    
    createBuffer(vertexDataSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 meshVertexBuffer, meshVertexBufferMemory);
    
    // Create or recreate index buffer
    if (meshIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshIndexBuffer, nullptr);
        vkFreeMemory(device, meshIndexBufferMemory, nullptr);
    }
    
    createBuffer(indexDataSize,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 meshIndexBuffer, meshIndexBufferMemory);
    
    // Create staging buffers
    VkBuffer vertexStagingBuffer;
    VkDeviceMemory vertexStagingBufferMemory;
    createBuffer(vertexDataSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 vertexStagingBuffer, vertexStagingBufferMemory);
    
    VkBuffer indexStagingBuffer;
    VkDeviceMemory indexStagingBufferMemory;
    createBuffer(indexDataSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexStagingBuffer, indexStagingBufferMemory);
    
    // Copy data to staging buffers
    void* data;
    vkMapMemory(device, vertexStagingBufferMemory, 0, vertexDataSize, 0, &data);
    memcpy(data, vertexData, vertexDataSize);
    vkUnmapMemory(device, vertexStagingBufferMemory);
    
    vkMapMemory(device, indexStagingBufferMemory, 0, indexDataSize, 0, &data);
    memcpy(data, indexData, indexDataSize);
    vkUnmapMemory(device, indexStagingBufferMemory);
    
    // Copy staging buffers to device local buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.size = vertexDataSize;
    vkCmdCopyBuffer(commandBuffer, vertexStagingBuffer, meshVertexBuffer, 1, &vertexCopyRegion);
    
    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.size = indexDataSize;
    vkCmdCopyBuffer(commandBuffer, indexStagingBuffer, meshIndexBuffer, 1, &indexCopyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Cleanup staging buffers
    vkDestroyBuffer(device, vertexStagingBuffer, nullptr);
    vkFreeMemory(device, vertexStagingBufferMemory, nullptr);
    vkDestroyBuffer(device, indexStagingBuffer, nullptr);
    vkFreeMemory(device, indexStagingBufferMemory, nullptr);
    
    return true;
}

} // namespace rendering