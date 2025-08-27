// GPU Mesh Generation using Compute Shaders
#include "rendering/vulkan_renderer.hpp"
#include <iostream>
#include <vector>

namespace rendering {

bool VulkanRenderer::generateGPUMesh(octree::OctreePlanet* planet, core::Camera* camera) {
// DEBUG_CPU_REFERENCE is now optional - don't force it
    std::cout << "\n=== Step 5: GPU Mesh Generation (Compute Shader) ===\n";
    std::cout << "[DEBUG] generateGPUMesh called, gpuOctree ptr = " << gpuOctree.get() << "\n";
    
    // Make sure octree is uploaded
    if (!gpuOctree) {
        std::cerr << "ERROR: GPU octree not initialized!\n";
        return false;
    }
    
    // Upload octree data to GPU if needed
    if (planet && camera) {
        std::cout << "[DEBUG] Uploading octree to GPU...\n";
        
        // Get camera matrices for frustum culling during upload
        glm::vec3 viewPos = camera->getPosition();
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 proj = camera->getProjectionMatrix();
        glm::mat4 viewProj = proj * view;
        
        gpuOctree->uploadOctree(planet, viewPos, viewProj, commandPool, graphicsQueue);
    }
    
    std::cout << "[DEBUG] gpuOctree exists, about to call getNodeCount()\n";
    
    try {
        uint32_t nodeCount = gpuOctree->getNodeCount();
        std::cout << "[DEBUG] getNodeCount() returned " << nodeCount << "\n";
        std::cout << "Processing " << nodeCount << " octree nodes for mesh generation...\n";
    
    // Create buffers for mesh output
    const size_t MAX_VERTICES = 1000000;  // 1M vertices
    const size_t MAX_INDICES = 3000000;   // 3M indices (1M triangles)
    const size_t vertexSize = sizeof(float) * 11; // pos(3) + color(3) + normal(3) + texcoord(2)
    const size_t vertexBufferSize = MAX_VERTICES * vertexSize;
    const size_t indexBufferSize = MAX_INDICES * sizeof(uint32_t);
    
    // Create vertex buffer with storage bit for compute shader write
    if (meshVertexBuffer == VK_NULL_HANDLE) {
        createBuffer(vertexBufferSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     meshVertexBuffer, meshVertexBufferMemory);
    }
    
    // Create index buffer with storage bit for compute shader write
    if (meshIndexBuffer == VK_NULL_HANDLE) {
        createBuffer(indexBufferSize,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     meshIndexBuffer, meshIndexBufferMemory);
    }
    
    // Create counter buffer for atomic operations
    VkBuffer counterBuffer;
    VkDeviceMemory counterBufferMemory;
    const size_t counterBufferSize = sizeof(uint32_t) * 2; // vertexCount, indexCount
    createBuffer(counterBufferSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 counterBuffer, counterBufferMemory);
    
    // Initialize counters to zero
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(counterBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    uint32_t zeros[2] = {0, 0};
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, counterBufferSize, 0, &data);
    memcpy(data, zeros, counterBufferSize);
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Create compute pipeline
    VkDescriptorSetLayout computeDescriptorSetLayout;
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings(4);
        
        // Binding 0: Octree nodes (input)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // Binding 1: Vertex buffer (output)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // Binding 2: Index buffer (output)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        // Binding 3: Counter buffer
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute descriptor set layout!");
        }
    }
    
    VkPipelineLayout computePipelineLayout;
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline layout!");
        }
    }
    
    // Load compute shader - using sphere_generator.comp
    std::cout << "\n[COMPUTE] Loading shader: shaders/sphere_generator.comp.spv\n";
    auto computeShaderCode = readFile("shaders/sphere_generator.comp.spv");
    std::cout << "[COMPUTE] Shader loaded, size: " << computeShaderCode.size() << " bytes\n";
    VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);
    
    VkPipeline computePipeline;
    {
        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";
        
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;
        
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }
    }
    
    // Create descriptor set
    VkDescriptorSet computeDescriptorSet;
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &computeDescriptorSetLayout;
        
        if (vkAllocateDescriptorSets(device, &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate compute descriptor set!");
        }
        
        // Update descriptor set
        std::vector<VkWriteDescriptorSet> descriptorWrites(4);
        
        std::cout << "[DEBUG] About to get node buffer from gpuOctree at " << gpuOctree.get() << "\n";
        std::cout << "[DEBUG] Double-checking gpuOctree pointer is still valid: " << (gpuOctree ? "YES" : "NO") << "\n";
        
        if (!gpuOctree) {
            std::cerr << "[CRITICAL ERROR] gpuOctree became null between line 21 and 160!\n";
            throw std::runtime_error("gpuOctree unexpectedly became null");
        }
        
        VkDescriptorBufferInfo octreeBufferInfo{};
        std::cout << "[DEBUG] Calling getNodeBuffer()...\n" << std::flush;
        octreeBufferInfo.buffer = gpuOctree->getNodeBuffer();
        std::cout << "[DEBUG] Got node buffer successfully: " << octreeBufferInfo.buffer << "\n";
        octreeBufferInfo.offset = 0;
        octreeBufferInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo vertexBufferInfo{};
        vertexBufferInfo.buffer = meshVertexBuffer;
        vertexBufferInfo.offset = 0;
        vertexBufferInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo indexBufferInfo{};
        indexBufferInfo.buffer = meshIndexBuffer;
        indexBufferInfo.offset = 0;
        indexBufferInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo counterBufferInfo{};
        counterBufferInfo.buffer = counterBuffer;
        counterBufferInfo.offset = 0;
        counterBufferInfo.range = counterBufferSize;
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = computeDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &octreeBufferInfo;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = computeDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &vertexBufferInfo;
        
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = computeDescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &indexBufferInfo;
        
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = computeDescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &counterBufferInfo;
        
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
    
    // Dispatch compute shader
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
    
    // Copy zeros to counter buffer
    VkBufferCopy copyRegion{};
    copyRegion.size = counterBufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, counterBuffer, 1, &copyRegion);
    
    // Barrier to ensure copy completes before compute
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    // Bind pipeline and descriptors
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
    
    // Sphere generator: needs 21 threads (one per latitude band)
    // With local_size_x=64, one workgroup contains 64 threads which is enough
    std::cout << "Dispatching 1 workgroup (64 threads) for sphere generation...\n";
    vkCmdDispatch(commandBuffer, 1, 1, 1);
    
    // Barrier to ensure compute completes before reading
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    // Copy counter buffer back to staging to read results
    vkCmdCopyBuffer(commandBuffer, counterBuffer, stagingBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Read back the counters to see how many vertices/indices were generated
    vkMapMemory(device, stagingBufferMemory, 0, counterBufferSize, 0, &data);
    uint32_t* counters = static_cast<uint32_t*>(data);
    meshVertexCount = counters[0];
    meshIndexCount = counters[1];
    vkUnmapMemory(device, stagingBufferMemory);
    
    std::cout << "GPU Mesh Generation Results:\n";
    std::cout << "  Generated " << meshVertexCount << " vertices\n";
    std::cout << "  Generated " << meshIndexCount << " indices (" << meshIndexCount/3 << " triangles)\n";
    
    // Cleanup
    vkDestroyShaderModule(device, computeShaderModule, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
    vkDestroyBuffer(device, counterBuffer, nullptr);
    vkFreeMemory(device, counterBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    if (meshVertexCount > 0 && meshIndexCount > 0) {
        std::cout << "GPU mesh generation SUCCESSFUL!\n";
        std::cout << "================================\n\n";
        return true;
    } else {
        std::cerr << "WARNING: No mesh generated (0 vertices/indices)\n";
        std::cout << "================================\n\n";
        return false;
    }
    
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in generateGPUMesh: " << e.what() << "\n";
        std::cerr << "This might explain why gpuOctree destructor was called\n";
        return false;
    }
}

} // namespace rendering