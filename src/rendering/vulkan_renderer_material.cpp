#include "rendering/vulkan_renderer.hpp"
#include "core/material_table.hpp"
#include <array>
#include <cstring>
#include <iostream>

namespace rendering {

// GPU material structure matching shader layout
struct GPUMaterial {
    glm::vec4 color;        // RGB color + alpha/reserved
    glm::vec4 properties;   // density, state, reserved, reserved
};

void VulkanRenderer::createMaterialTableBuffer() {
    std::cout << "Creating material table buffer..." << std::endl;
    
    // Get the material table instance
    auto& materialTable = core::MaterialTable::getInstance();
    
    // Prepare GPU materials array
    std::array<GPUMaterial, 16> gpuMaterials;
    
    // Fill the GPU materials from the material table
    for (int i = 0; i < 16; i++) {
        core::MaterialID id = static_cast<core::MaterialID>(i);
        
        // Get color from material table
        glm::vec3 color = materialTable.getColor(id);
        gpuMaterials[i].color = glm::vec4(color, 1.0f);
        
        // Get properties
        float density = materialTable.getDensity(id);
        float state = static_cast<float>(materialTable.getState(id));
        
        // Pack properties (density, state, reserved, reserved)
        gpuMaterials[i].properties = glm::vec4(density, state, 0.0f, 0.0f);
    }
    
    // Calculate buffer size
    VkDeviceSize bufferSize = sizeof(GPUMaterial) * 16;
    
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);
    
    // Copy data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, gpuMaterials.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Create device local buffer for material table
    createBuffer(bufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                materialTableBuffer, materialTableBufferMemory);
    
    // Copy from staging to device buffer
    copyBuffer(stagingBuffer, materialTableBuffer, bufferSize);
    
    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    std::cout << "Material table buffer created with " << bufferSize << " bytes" << std::endl;
}

void VulkanRenderer::updateMaterialTableBuffer() {
    // This function can be called if we need to update materials at runtime
    // For now, materials are static, so this is a placeholder
    
    // In the future, this could:
    // 1. Get updated materials from MaterialTable
    // 2. Update the GPU buffer via staging buffer
    // 3. Potentially use push constants for small updates
}

} // namespace rendering