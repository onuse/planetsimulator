// GPU Transvoxel implementation using compute shaders
#include "rendering/vulkan_renderer.hpp"
#include "rendering/gpu_octree.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

namespace rendering {

// Static pipeline objects for GPU Transvoxel
static VkPipeline transvoxelComputePipeline = VK_NULL_HANDLE;
static VkPipelineLayout transvoxelPipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout transvoxelDescriptorSetLayout = VK_NULL_HANDLE;

bool VulkanRenderer::createTransvoxelComputePipeline() {
    std::cout << "\n=== Creating GPU Transvoxel Compute Pipeline ===\n";
    
    // Load compiled shader
    std::ifstream file("shaders/transvoxel_gpu.comp.spv", std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open transvoxel_gpu.comp.spv!\n";
        std::cerr << "Make sure shaders are compiled.\n";
        return false;
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
        std::cerr << "ERROR: Failed to create Transvoxel shader module!\n";
        return false;
    }
    
    // Create descriptor set layout
    std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
    
    // Binding 0: Uniform buffer (camera, planet params, voxel size)
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
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &transvoxelDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create descriptor set layout!\n";
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &transvoxelDescriptorSetLayout;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &transvoxelPipelineLayout) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create pipeline layout!\n";
        vkDestroyDescriptorSetLayout(device, transvoxelDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = transvoxelPipelineLayout;
    
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transvoxelComputePipeline) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create compute pipeline!\n";
        vkDestroyPipelineLayout(device, transvoxelPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, transvoxelDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Clean up shader module
    vkDestroyShaderModule(device, shaderModule, nullptr);
    
    std::cout << "Transvoxel compute pipeline created successfully!\n";
    return true;
}

bool VulkanRenderer::generateGPUTransvoxelMesh(octree::OctreePlanet* planet, core::Camera* camera) {
    std::cout << "\n=== GPU Transvoxel Mesh Generation ===\n";
    
    // Initialize pipeline if needed
    static bool pipelineInitialized = false;
    if (!pipelineInitialized) {
        if (!createTransvoxelComputePipeline()) {
            std::cerr << "ERROR: Failed to create Transvoxel compute pipeline!\n";
            return false;
        }
        pipelineInitialized = true;
    }
    
    // Ensure octree is uploaded
    if (!gpuOctree) {
        gpuOctree = std::make_unique<GPUOctree>(device, physicalDevice);
    }
    
    glm::vec3 viewPos = camera->getPosition();
    glm::mat4 viewProj = camera->getProjectionMatrix() * camera->getViewMatrix();
    gpuOctree->uploadOctree(planet, viewPos, viewProj, commandPool, graphicsQueue);
    
    // Get octree buffers
    VkBuffer octreeNodeBuffer = gpuOctree->getNodeBuffer();
    VkBuffer octreeVoxelBuffer = gpuOctree->getVoxelBuffer();
    uint32_t nodeCount = gpuOctree->getNodeCount();
    
    std::cout << "Octree uploaded: " << nodeCount << " nodes\n";
    
    // Calculate voxel grid parameters based on camera distance
    float distanceToCenter = glm::length(viewPos);
    float distanceToSurface = distanceToCenter - planet->getRadius();
    
    // Adaptive grid size and voxel size
    int gridSize = 32;  // Start with 32x32x32 grid
    float voxelSize = planet->getRadius() * 0.01f;  // 1% of planet radius
    
    if (distanceToSurface < planet->getRadius() * 0.1f) {
        // Very close - high detail
        gridSize = 64;
        voxelSize = planet->getRadius() * 0.001f;
    } else if (distanceToSurface < planet->getRadius() * 0.5f) {
        // Medium distance
        gridSize = 48;
        voxelSize = planet->getRadius() * 0.005f;
    }
    
    std::cout << "Grid size: " << gridSize << "x" << gridSize << "x" << gridSize << "\n";
    std::cout << "Voxel size: " << voxelSize << " meters\n";
    
    // Create uniform buffer
    struct TransvoxelUBO {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec3 cameraPos;
        float planetRadius;
        glm::vec3 planetCenter;
        float time;
        float voxelSize;
        int gridSize;
        int maxVertices;
        int maxIndices;
    } uboData;
    
    uboData.viewMatrix = camera->getViewMatrix();
    uboData.projMatrix = camera->getProjectionMatrix();
    uboData.cameraPos = viewPos;
    uboData.planetRadius = planet->getRadius();
    uboData.planetCenter = glm::vec3(0.0f);
    uboData.time = 0.0f;
    uboData.voxelSize = voxelSize;
    uboData.gridSize = gridSize;
    uboData.maxVertices = 1000000;
    uboData.maxIndices = 3000000;
    
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    createBuffer(sizeof(TransvoxelUBO),
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 uniformBuffer, uniformBufferMemory);
    
    // Upload UBO data
    void* data;
    vkMapMemory(device, uniformBufferMemory, 0, sizeof(TransvoxelUBO), 0, &data);
    memcpy(data, &uboData, sizeof(TransvoxelUBO));
    vkUnmapMemory(device, uniformBufferMemory);
    
    // Ensure mesh buffers exist
    if (meshVertexBuffer == VK_NULL_HANDLE || meshIndexBuffer == VK_NULL_HANDLE) {
        allocateGPUMeshBuffers(1000000, 3000000);
    }
    
    // Create counter buffer
    VkBuffer counterBuffer;
    VkDeviceMemory counterBufferMemory;
    createBuffer(4 * sizeof(uint32_t),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 counterBuffer, counterBufferMemory);
    
    // Allocate descriptor set
    VkDescriptorSet descriptorSet;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &transvoxelDescriptorSetLayout;
    
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to allocate descriptor set!\n";
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMemory, nullptr);
        vkDestroyBuffer(device, counterBuffer, nullptr);
        vkFreeMemory(device, counterBufferMemory, nullptr);
        return false;
    }
    
    // Update descriptor set
    std::array<VkWriteDescriptorSet, 6> descriptorWrites{};
    
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(TransvoxelUBO);
    
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
    
    for (int i = 0; i < 6; i++) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = descriptorSet;
        descriptorWrites[i].dstBinding = i;
        descriptorWrites[i].descriptorCount = 1;
    }
    
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &uniformBufferInfo;
    
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].pBufferInfo = &nodeBufferInfo;
    
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].pBufferInfo = &voxelBufferInfo;
    
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].pBufferInfo = &vertexBufferInfo;
    
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[4].pBufferInfo = &indexBufferInfo;
    
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[5].pBufferInfo = &counterBufferInfo;
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    
    // Create command buffer
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
    
    // Clear counters
    vkCmdFillBuffer(commandBuffer, counterBuffer, 0, 4 * sizeof(uint32_t), 0);
    
    // Memory barrier after clear
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    // Bind pipeline and descriptors
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, transvoxelComputePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            transvoxelPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
    // Dispatch compute shader
    // Work groups of 8x8x1, so we need gridSize/8 groups in X and Y
    uint32_t workGroupsX = (gridSize + 7) / 8;
    uint32_t workGroupsY = (gridSize + 7) / 8;
    uint32_t workGroupsZ = gridSize;  // Z dimension processes full depth
    
    std::cout << "Dispatching " << workGroupsX << "x" << workGroupsY << "x" << workGroupsZ << " work groups\n";
    vkCmdDispatch(commandBuffer, workGroupsX, workGroupsY, workGroupsZ);
    
    // Barrier before reading results
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    vkEndCommandBuffer(commandBuffer);
    
    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Read back counters
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
    
    std::cout << "GPU Transvoxel Mesh Generation Complete:\n";
    std::cout << "  Vertices: " << meshVertexCount << "\n";
    std::cout << "  Indices: " << meshIndexCount << " (" << meshIndexCount/3 << " triangles)\n";
    std::cout << "  Processed cells: " << counters[2] << "\n";
    std::cout << "  Generated triangles: " << counters[3] << "\n";
    
    // Cleanup
    vkDestroyBuffer(device, uniformBuffer, nullptr);
    vkFreeMemory(device, uniformBufferMemory, nullptr);
    vkDestroyBuffer(device, counterBuffer, nullptr);
    vkFreeMemory(device, counterBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    std::cout << "=====================================\n\n";
    
    return meshVertexCount > 0 && meshIndexCount > 0;
}

void VulkanRenderer::cleanupTransvoxelPipeline() {
    if (transvoxelComputePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, transvoxelComputePipeline, nullptr);
        transvoxelComputePipeline = VK_NULL_HANDLE;
    }
    
    if (transvoxelPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, transvoxelPipelineLayout, nullptr);
        transvoxelPipelineLayout = VK_NULL_HANDLE;
    }
    
    if (transvoxelDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, transvoxelDescriptorSetLayout, nullptr);
        transvoxelDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

} // namespace rendering