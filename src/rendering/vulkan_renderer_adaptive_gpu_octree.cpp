// GPU compute implementation for adaptive sphere generation with octree sampling
#include "rendering/vulkan_renderer.hpp"
#include "rendering/gpu_octree.hpp"
#include <iostream>
#include <fstream>
#include <cmath>

namespace rendering {

bool VulkanRenderer::generateGPUAdaptiveSphereWithOctree(octree::OctreePlanet* planet, core::Camera* camera) {
    std::cout << "\n=== GPU ADAPTIVE SPHERE WITH OCTREE ===\n";
    
    // First, ensure octree is uploaded to GPU
    if (!gpuOctree) {
        gpuOctree = std::make_unique<GPUOctree>(device, physicalDevice);
    }
    
    // Upload octree data
    glm::vec3 viewPos = camera->getPosition();
    glm::mat4 viewProj = camera->getProjectionMatrix() * camera->getViewMatrix();
    gpuOctree->uploadOctree(planet, viewPos, viewProj, commandPool, graphicsQueue);
    
    // Get octree buffers
    VkBuffer octreeNodeBuffer = gpuOctree->getNodeBuffer();
    VkBuffer octreeVoxelBuffer = gpuOctree->getVoxelBuffer();
    uint32_t nodeCount = gpuOctree->getNodeCount();
    
    std::cout << "Octree uploaded: " << nodeCount << " nodes\n";
    
    // Load the octree-aware shader
    std::string shaderPath = "shaders/adaptive_sphere_octree.comp.spv";
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open shader: " << shaderPath << "\n";
        std::cerr << "Falling back to simple adaptive sphere without octree\n";
        return generateGPUAdaptiveSphere(planet, camera);
    }
    
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();
    
    // Create shader module
    VkShaderModuleCreateInfo shaderModuleInfo{};
    shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleInfo.codeSize = code.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create shader module!\n";
        return false;
    }
    
    // Create descriptor set layout with octree buffers
    std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
    
    // Binding 0: Uniform buffer (camera, planet params)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: Storage buffer (octree nodes)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 2: Storage buffer (octree voxels)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 3: Storage buffer (vertex output)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 4: Storage buffer (index output)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 5: Storage buffer (counters)
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create descriptor set layout!\n";
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Create pipeline layout
    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create pipeline layout!\n";
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Create compute pipeline
    VkPipeline computePipeline;
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;
    
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create compute pipeline!\n";
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Clean up shader module
    vkDestroyShaderModule(device, shaderModule, nullptr);
    
    // Allocate descriptor set
    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to allocate descriptor set!\n";
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        return false;
    }
    
    // Create uniform buffer for compute shader parameters
    struct AdaptiveSphereUBO {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec3 cameraPos;
        float planetRadius;
        glm::vec3 planetCenter;
        float time;
        int highDetailLevel;
        int lowDetailLevel;
        int flipFrontBack;
        int maxVertices;
        int maxIndices;
        float padding[3];
    } uboData;
    
    // Fill UBO data
    uboData.viewMatrix = camera->getViewMatrix();
    uboData.projMatrix = camera->getProjectionMatrix();
    uboData.cameraPos = camera->getPosition();
    uboData.planetRadius = planet->getRadius();
    uboData.planetCenter = glm::vec3(0.0f);
    uboData.time = 0.0f;
    
    // Adaptive LOD based on camera distance (matching CPU implementation)
    float distanceToCenter = glm::length(uboData.cameraPos);
    float distanceToSurface = distanceToCenter - uboData.planetRadius;
    
    // Dual-detail LOD levels with MORE aggressive difference (matching CPU)
    int highDetailLevel = 5;  // Front hemisphere
    int lowDetailLevel = 1;   // Back hemisphere - MUCH lower detail for testing
    
    // Adjust high detail based on distance (matching CPU logic exactly)
    if (distanceToSurface > uboData.planetRadius * 10.0f) {
        highDetailLevel = 4;  // Far away - but still higher than back
        lowDetailLevel = 1;   // Keep back very low
    } else if (distanceToSurface > uboData.planetRadius * 5.0f) {
        highDetailLevel = 5;
        lowDetailLevel = 1;
    } else if (distanceToSurface > uboData.planetRadius * 2.0f) {
        highDetailLevel = 6;
        lowDetailLevel = 2;
    } else if (distanceToSurface > uboData.planetRadius * 0.5f) {
        highDetailLevel = 7;
        lowDetailLevel = 2;
    } else if (distanceToSurface > uboData.planetRadius * 0.1f) {
        highDetailLevel = 8;
        lowDetailLevel = 3;
    } else {
        highDetailLevel = 9;  // Very close - maximum detail
        lowDetailLevel = 3;
    }
    
    // Cap at reasonable level
    if (highDetailLevel > 9) highDetailLevel = 9;
    
    uboData.highDetailLevel = highDetailLevel;
    uboData.lowDetailLevel = lowDetailLevel;
    
    std::cout << "GPU LOD: Camera distance: " << distanceToSurface / uboData.planetRadius << "x radius\n";
    std::cout << "  Front hemisphere: " << highDetailLevel << " (~" << (20 * (int)pow(4, highDetailLevel)) << " tris)\n";
    std::cout << "  Back hemisphere: " << lowDetailLevel << " (~" << (20 * (int)pow(4, lowDetailLevel)) << " tris)\n";
    
    uboData.flipFrontBack = adaptiveSphereFlipFrontBack ? 1 : 0;
    uboData.maxVertices = 1000000;
    uboData.maxIndices = 3000000;
    
    // Create uniform buffer
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    createBuffer(sizeof(AdaptiveSphereUBO),
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 uniformBuffer, uniformBufferMemory);
    
    // Upload UBO data
    void* data;
    vkMapMemory(device, uniformBufferMemory, 0, sizeof(AdaptiveSphereUBO), 0, &data);
    memcpy(data, &uboData, sizeof(AdaptiveSphereUBO));
    vkUnmapMemory(device, uniformBufferMemory);
    
    // Ensure mesh buffers exist
    if (meshVertexBuffer == VK_NULL_HANDLE || meshIndexBuffer == VK_NULL_HANDLE) {
        allocateGPUMeshBuffers(1000000, 3000000);
    }
    
    // Create counter buffer
    VkBuffer counterBuffer;
    VkDeviceMemory counterBufferMemory;
    createBuffer(4 * sizeof(uint32_t),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 counterBuffer, counterBufferMemory);
    
    // Update descriptor set
    std::array<VkWriteDescriptorSet, 6> descriptorWrites{};
    
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(AdaptiveSphereUBO);
    
    VkDescriptorBufferInfo nodeBufferInfo{};
    nodeBufferInfo.buffer = octreeNodeBuffer;
    nodeBufferInfo.offset = 0;
    nodeBufferInfo.range = VK_WHOLE_SIZE;
    
    VkDescriptorBufferInfo voxelBufferInfo{};
    voxelBufferInfo.buffer = octreeVoxelBuffer;
    voxelBufferInfo.offset = 0;
    voxelBufferInfo.range = VK_WHOLE_SIZE;
    
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
    counterBufferInfo.range = 4 * sizeof(uint32_t);
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uniformBufferInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nodeBufferInfo;
    
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &voxelBufferInfo;
    
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &vertexBufferInfo;
    
    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = descriptorSet;
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pBufferInfo = &indexBufferInfo;
    
    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = descriptorSet;
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &counterBufferInfo;
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    
    // Clear counters
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    vkCmdFillBuffer(commandBuffer, counterBuffer, 0, 4 * sizeof(uint32_t), 0);
    
    // Memory barrier
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    // Dispatch compute shader
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
    // Dispatch with 1 workgroup (32 threads, handles 20 icosahedron faces)
    vkCmdDispatch(commandBuffer, 1, 1, 1);
    
    // Barrier before reading results
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Read back counters to get vertex/index counts
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(4 * sizeof(uint32_t),
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    copyBuffer(counterBuffer, stagingBuffer, 4 * sizeof(uint32_t));
    
    uint32_t counters[4];
    vkMapMemory(device, stagingBufferMemory, 0, 4 * sizeof(uint32_t), 0, &data);
    memcpy(counters, data, 4 * sizeof(uint32_t));
    vkUnmapMemory(device, stagingBufferMemory);
    
    meshVertexCount = counters[0];
    meshIndexCount = counters[1];
    
    std::cout << "GPU Octree Mesh Generation Complete:\n";
    std::cout << "  Vertices: " << meshVertexCount << "\n";
    std::cout << "  Indices: " << meshIndexCount << " (" << meshIndexCount/3 << " triangles)\n";
    std::cout << "  Front faces: " << counters[2] << "\n";
    std::cout << "  Back faces: " << counters[3] << "\n";
    
    // Cleanup
    vkDestroyBuffer(device, uniformBuffer, nullptr);
    vkFreeMemory(device, uniformBufferMemory, nullptr);
    vkDestroyBuffer(device, counterBuffer, nullptr);
    vkFreeMemory(device, counterBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    
    std::cout << "=====================================\n\n";
    
    return meshVertexCount > 0 && meshIndexCount > 0;
}

} // namespace rendering