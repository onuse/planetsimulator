// CPU Mesh Upload Implementation
// Handles uploading CPU-generated mesh data to GPU buffers

#include "rendering/vulkan_renderer.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <iostream>

namespace rendering {

bool VulkanRenderer::uploadCPUReferenceMesh(const void* vertexData, size_t vertexDataSize,
                                            const void* indexData, size_t indexDataSize,
                                            uint32_t vertexCount, uint32_t indexCount) {
    
    std::cout << "[CPU_REF] Uploading mesh to GPU buffers..." << std::endl;
    
    // Clean up old buffers if they exist
    if (meshVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshVertexBuffer, nullptr);
        vkFreeMemory(device, meshVertexBufferMemory, nullptr);
        meshVertexBuffer = VK_NULL_HANDLE;
    }
    
    if (meshIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshIndexBuffer, nullptr);
        vkFreeMemory(device, meshIndexBufferMemory, nullptr);
        meshIndexBuffer = VK_NULL_HANDLE;
    }
    
    // Create vertex buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    createBuffer(vertexDataSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    // Copy vertex data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, vertexDataSize, 0, &data);
    memcpy(data, vertexData, vertexDataSize);
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Create device local vertex buffer
    createBuffer(vertexDataSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 meshVertexBuffer, meshVertexBufferMemory);
    
    // Copy from staging to device local
    copyBuffer(stagingBuffer, meshVertexBuffer, vertexDataSize);
    
    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    // Create index buffer
    createBuffer(indexDataSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    // Copy index data to staging buffer
    vkMapMemory(device, stagingBufferMemory, 0, indexDataSize, 0, &data);
    memcpy(data, indexData, indexDataSize);
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Create device local index buffer
    createBuffer(indexDataSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 meshIndexBuffer, meshIndexBufferMemory);
    
    // Copy from staging to device local
    copyBuffer(stagingBuffer, meshIndexBuffer, indexDataSize);
    
    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    // Store counts
    meshVertexCount = vertexCount;
    meshIndexCount = indexCount;
    
    std::cout << "[CPU_REF] Upload complete! Buffers created for " << vertexCount 
              << " vertices and " << indexCount << " indices" << std::endl;
    
    return true;
}

} // namespace rendering