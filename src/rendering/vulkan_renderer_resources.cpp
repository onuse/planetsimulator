#include "rendering/vulkan_renderer.hpp"
#include <stdexcept>
#include <cstring>
#include <array>
#include <iostream>
#include <cstddef>  // for offsetof
#include <cassert>
#include <cmath>
#include <chrono>
#include <cstdlib>  // for std::abort

namespace rendering {

// ============================================================================
// Memory Management Helpers
// ============================================================================

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                 VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                                 VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
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
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// ============================================================================
// Vertex and Index Buffers (Cube Geometry)
// ============================================================================

void VulkanRenderer::createVertexBuffer() {
    // Create cube vertices (position + normal)
    // Slightly expand vertices to 0.501 to eliminate gaps between adjacent nodes
    const float s = 0.501f; // Slight expansion to prevent gaps
    const std::array<float, 144> vertices = {
        // Front face
        -s, -s,  s,  0.0f,  0.0f,  1.0f,
         s, -s,  s,  0.0f,  0.0f,  1.0f,
         s,  s,  s,  0.0f,  0.0f,  1.0f,
        -s,  s,  s,  0.0f,  0.0f,  1.0f,
        
        // Back face
        -s, -s, -s,  0.0f,  0.0f, -1.0f,
        -s,  s, -s,  0.0f,  0.0f, -1.0f,
         s,  s, -s,  0.0f,  0.0f, -1.0f,
         s, -s, -s,  0.0f,  0.0f, -1.0f,
        
        // Top face
        -s,  s, -s,  0.0f,  1.0f,  0.0f,
        -s,  s,  s,  0.0f,  1.0f,  0.0f,
         s,  s,  s,  0.0f,  1.0f,  0.0f,
         s,  s, -s,  0.0f,  1.0f,  0.0f,
        
        // Bottom face
        -s, -s, -s,  0.0f, -1.0f,  0.0f,
         s, -s, -s,  0.0f, -1.0f,  0.0f,
         s, -s,  s,  0.0f, -1.0f,  0.0f,
        -s, -s,  s,  0.0f, -1.0f,  0.0f,
        
        // Right face
         s, -s, -s,  1.0f,  0.0f,  0.0f,
         s,  s, -s,  1.0f,  0.0f,  0.0f,
         s,  s,  s,  1.0f,  0.0f,  0.0f,
         s, -s,  s,  1.0f,  0.0f,  0.0f,
        
        // Left face
        -s, -s, -s, -1.0f,  0.0f,  0.0f,
        -s, -s,  s, -1.0f,  0.0f,  0.0f,
        -s,  s,  s, -1.0f,  0.0f,  0.0f,
        -s,  s, -s, -1.0f,  0.0f,  0.0f
    };
    
    VkDeviceSize bufferSize = sizeof(vertices);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);
    
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::createIndexBuffer() {
    const std::array<uint16_t, 36> indices = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Top
        12, 13, 14, 14, 15, 12, // Bottom
        16, 17, 18, 18, 19, 16, // Right
        20, 21, 22, 22, 23, 20  // Left
    };
    
    indexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize bufferSize = sizeof(indices);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);
    
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    
    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

// ============================================================================
// Uniform Buffers
// ============================================================================

void VulkanRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    uniformBuffers[i], uniformBuffersMemory[i]);
        
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void VulkanRenderer::updateUniformBuffer(uint32_t currentImage, core::Camera* camera) {
    UniformBufferObject ubo{};
    
    // CAMERA-RELATIVE RENDERING for precision at planetary scales
    // We transform vertices relative to camera position to maintain precision
    // This avoids large world coordinates causing float precision issues
    
    glm::mat4 viewF = camera->getViewMatrix();
    glm::mat4 projF = camera->getProjectionMatrix();
    glm::vec3 viewPosF = camera->getPosition();
    
    // Extract rotation-only view matrix (remove translation)
    // We'll handle translation by offsetting vertices on CPU
    glm::mat4 viewRelative = viewF;
    viewRelative[3][0] = 0.0f;  // Remove translation X
    viewRelative[3][1] = 0.0f;  // Remove translation Y  
    viewRelative[3][2] = 0.0f;  // Remove translation Z
    viewRelative[3][3] = 1.0f;
    
    // No scaling needed for camera-relative rendering
    // Vertices will be transformed relative to camera on CPU/GPU
    const float WORLD_SCALE = 1.0f;
    
    // Get camera parameters
    float fov = camera->getFieldOfView();
    // Calculate aspect ratio from swap chain extent
    float aspect = swapChainExtent.width / (float) swapChainExtent.height;
    float nearPlane = camera->getNearPlane();
    float farPlane = camera->getFarPlane();
    
    // DEBUG: Print actual values being used
    static int debugCount = 0;
    if (false && debugCount++ % 60 == 0) {  // Disabled verbose debug
        std::cout << "\n[UNIFORM BUFFER DEBUG]" << std::endl;
        std::cout << "  Camera position: " << viewPosF.x << ", " << viewPosF.y << ", " << viewPosF.z << std::endl;
        std::cout << "  WORLD_SCALE: " << WORLD_SCALE << std::endl;
        std::cout << "  Original near/far: " << camera->getNearPlane() << " / " << camera->getFarPlane() << std::endl;
        std::cout << "  Scaled near/far: " << nearPlane << " / " << farPlane << std::endl;
        std::cout << "  FOV: " << fov << ", Aspect: " << aspect << std::endl;
        
        // Debug the raw view matrix from camera
        std::cout << "  Raw view matrix from camera:" << std::endl;
        for (int i = 0; i < 4; i++) {
            std::cout << "    [" << viewF[i][0] << ", " << viewF[i][1] 
                      << ", " << viewF[i][2] << ", " << viewF[i][3] << "]" << std::endl;
        }
    }
    
    // Use camera's projection matrix directly
    glm::mat4 scaledProj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    
    // Build ViewProj with the rotation-only view matrix for camera-relative rendering
    glm::mat4 viewProjF = scaledProj * viewRelative;
    
    // Use single precision throughout (matches vertex data precision)
    ubo.view = viewRelative;
    ubo.proj = scaledProj;
    ubo.viewProj = viewProjF;
    // Camera position used as reference origin for camera-relative rendering
    ubo.viewPos = viewPosF;
    
    // Validate matrices - crash if they're invalid
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            double val = ubo.viewProj[i][j];
            if (std::isnan(val) || std::isinf(val)) {
                std::cerr << "FATAL ERROR: Invalid viewProj matrix at [" << i << "][" << j << "] = " << val << std::endl;
                std::abort();  // Force crash even in Release mode
            }
        }
    }
    
    // Check if matrix is degenerate (determinant near zero would mean no inverse exists)
    double det = glm::determinant(ubo.viewProj);
    if (std::abs(det) < 1e-10) {
        std::cerr << "FATAL ERROR: Degenerate viewProj matrix, determinant = " << det << std::endl;
        std::cerr << "ViewProj matrix:\n";
        for (int i = 0; i < 4; i++) {
            std::cerr << "  [" << ubo.viewProj[i][0] << ", " << ubo.viewProj[i][1] 
                      << ", " << ubo.viewProj[i][2] << ", " << ubo.viewProj[i][3] << "]\n";
        }
        std::abort();  // Force crash even in Release mode
    }
    
    // Debug: Print first frame's matrices to check they're reasonable
    static bool firstFrame = true;
    if (firstFrame) {
        firstFrame = false;
        std::cout << "DEBUG: Camera matrices on first frame (double precision):\n";
        std::cout << "  Camera position: (" << ubo.viewPos.x << ", " << ubo.viewPos.y << ", " << ubo.viewPos.z << ")\n";
        
        // Print view matrix
        std::cout << "  View matrix:\n";
        for (int i = 0; i < 4; i++) {
            std::cout << "    [" << ubo.view[i][0] << ", " << ubo.view[i][1] 
                      << ", " << ubo.view[i][2] << ", " << ubo.view[i][3] << "]\n";
        }
        
        // Print projection matrix
        std::cout << "  Projection matrix:\n";
        for (int i = 0; i < 4; i++) {
            std::cout << "    [" << ubo.proj[i][0] << ", " << ubo.proj[i][1] 
                      << ", " << ubo.proj[i][2] << ", " << ubo.proj[i][3] << "]\n";
        }
        
        // Print combined ViewProj
        std::cout << "  ViewProj (Proj * View):\n";
        std::cout << "    [0]: " << ubo.viewProj[0][0] << ", " << ubo.viewProj[0][1] << ", " << ubo.viewProj[0][2] << ", " << ubo.viewProj[0][3] << "\n";
        std::cout << "    [1]: " << ubo.viewProj[1][0] << ", " << ubo.viewProj[1][1] << ", " << ubo.viewProj[1][2] << ", " << ubo.viewProj[1][3] << "\n";
        std::cout << "    [2]: " << ubo.viewProj[2][0] << ", " << ubo.viewProj[2][1] << ", " << ubo.viewProj[2][2] << ", " << ubo.viewProj[2][3] << "\n";
        std::cout << "    [3]: " << ubo.viewProj[3][0] << ", " << ubo.viewProj[3][1] << ", " << ubo.viewProj[3][2] << ", " << ubo.viewProj[3][3] << "\n";
        std::cout << "  ViewProj[3]: " << ubo.viewProj[3][0] << ", " << ubo.viewProj[3][1] << ", " << ubo.viewProj[3][2] << ", " << ubo.viewProj[3][3] << "\n";
        
        // Test transform a sample vertex with double precision
        glm::dvec4 testVertex(4470575.0, 4534870.0, 14112.0, 1.0);
        glm::dvec4 transformed = ubo.viewProj * testVertex;
        std::cout << "  Test vertex (" << testVertex.x << ", " << testVertex.y << ", " << testVertex.z << ") -> (" 
                  << transformed.x << ", " << transformed.y << ", " << transformed.z << ", w=" << transformed.w << ")\n";
        if (transformed.w != 0) {
            std::cout << "  After perspective divide: (" << transformed.x/transformed.w << ", " 
                      << transformed.y/transformed.w << ", " << transformed.z/transformed.w << ")\n";
        }
    }
    
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    ubo.time = time;
    
    // Simple directional light (sun)
    ubo.lightDir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    
    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

// ============================================================================
// Instance Buffer for Octree Nodes
// ============================================================================

void VulkanRenderer::createCubeGeometry() {
    // Already created in createVertexBuffer and createIndexBuffer
}

// Instance buffer update removed - using Transvoxel mesh rendering instead

// ============================================================================
// Descriptor Pool and Sets
// ============================================================================

void VulkanRenderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 100; // Increased for compute and multiple pipelines
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 200; // Increased for compute shader buffers
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Allow freeing individual sets
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 100; // Increased to handle compute shader allocations
    
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

// ============================================================================
// Depth Resources
// ============================================================================

void VulkanRenderer::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    
    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat,
               VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat VulkanRenderer::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                            VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    
    throw std::runtime_error("failed to find supported format!");
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format,
                                VkImageTiling tiling, VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties, VkImage& image,
                                VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }
    
    vkBindImageMemory(device, image, imageMemory, 0);
}

} // namespace rendering