// GPU compute implementation for adaptive sphere generation
#include "rendering/vulkan_renderer.hpp"
#include "rendering/gpu_octree.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdlib>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace rendering {

// Compute pipeline objects for adaptive sphere
static VkPipeline adaptiveSphereComputePipeline = VK_NULL_HANDLE;
static VkPipelineLayout adaptiveSphereComputePipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSetLayout adaptiveSphereDescriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorSet adaptiveSphereDescriptorSet = VK_NULL_HANDLE;

// GPU buffers for adaptive sphere
static VkBuffer adaptiveSphereUniformBuffer = VK_NULL_HANDLE;
static VkDeviceMemory adaptiveSphereUniformBufferMemory = VK_NULL_HANDLE;
static void* adaptiveSphereUniformBufferMapped = nullptr;

static VkBuffer adaptiveSphereCounterBuffer = VK_NULL_HANDLE;
static VkDeviceMemory adaptiveSphereCounterBufferMemory = VK_NULL_HANDLE;

// Structure matching the shader's UBO
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
    float padding[3]; // Ensure 16-byte alignment
};

bool VulkanRenderer::createAdaptiveSphereComputePipeline() {
    std::cout << "\n=== Creating Adaptive Sphere Compute Pipeline ===\n";
    
    // Get current directory for debug
    char cwd[1024];
    #ifdef _WIN32
        _getcwd(cwd, sizeof(cwd));
    #else
        getcwd(cwd, sizeof(cwd));
    #endif
    std::cout << "Current working directory: " << cwd << "\n";
    
    // Load the compiled shader
    const char* shaderPath = "shaders/adaptive_sphere.comp.spv";
    std::cout << "Loading shader from: " << shaderPath << "\n";
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open adaptive_sphere.comp.spv!\n";
        std::cerr << "Make sure to compile shaders first.\n";
        std::cerr << "Looking in: " << cwd << "/" << shaderPath << "\n";
        
        // Try alternate path
        const char* altPath = "build/bin/Release/shaders/adaptive_sphere.comp.spv";
        std::cerr << "Trying alternate path: " << altPath << "\n";
        file.open(altPath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "ERROR: Also failed to open from alternate path!\n";
            return false;
        }
        std::cout << "SUCCESS: Found shader at alternate path!\n";
    }
    
    size_t fileSize = (size_t)file.tellg();
    std::cout << "Shader file size: " << fileSize << " bytes\n";
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();
    std::cout << "Successfully loaded shader bytecode\n";
    
    // Create shader module
    VkShaderModuleCreateInfo shaderModuleInfo{};
    shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleInfo.codeSize = code.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    VkResult shaderResult = vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule);
    if (shaderResult != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create shader module! Result: " << shaderResult << "\n";
        return false;
    }
    std::cout << "Successfully created shader module\n";
    
    // Create descriptor set layout
    std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
    
    // Binding 0: Uniform buffer (camera, planet params)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: Storage buffer (vertex output)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 2: Storage buffer (index output)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 3: Storage buffer (counters)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 4: Storage buffer (octree nodes)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 5: SKIPPED - shader expects it unused
    
    // Binding 6: Storage buffer (voxel data)
    bindings[5].binding = 6;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &adaptiveSphereDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create descriptor set layout!\n";
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &adaptiveSphereDescriptorSetLayout;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &adaptiveSphereComputePipelineLayout) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create pipeline layout!\n";
        vkDestroyDescriptorSetLayout(device, adaptiveSphereDescriptorSetLayout, nullptr);
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
    pipelineInfo.layout = adaptiveSphereComputePipelineLayout;
    
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &adaptiveSphereComputePipeline) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create compute pipeline!\n";
        vkDestroyPipelineLayout(device, adaptiveSphereComputePipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, adaptiveSphereDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Clean up shader module (pipeline retains its own reference)
    vkDestroyShaderModule(device, shaderModule, nullptr);
    
    std::cout << "Adaptive sphere compute pipeline created successfully!\n";
    return true;
}

bool VulkanRenderer::allocateGPUMeshBuffers(size_t maxVertices, size_t maxIndices) {
    std::cout << "Allocating GPU mesh buffers: " << maxVertices << " vertices, " << maxIndices << " indices\n";
    
    // Calculate buffer sizes
    size_t vertexBufferSize = maxVertices * 11 * sizeof(float);  // 11 floats per vertex
    size_t indexBufferSize = maxIndices * sizeof(uint32_t);
    
    // Create vertex buffer (reuse existing meshVertexBuffer if possible)
    if (!meshVertexBuffer) {
        createBuffer(vertexBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    meshVertexBuffer, meshVertexBufferMemory);
    }
    
    // Create index buffer
    if (!meshIndexBuffer) {
        createBuffer(indexBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    meshIndexBuffer, meshIndexBufferMemory);
    }
    
    // Create uniform buffer for compute shader parameters (only if not already created)
    if (!adaptiveSphereUniformBuffer) {
        size_t uniformBufferSize = sizeof(AdaptiveSphereUBO);
        createBuffer(uniformBufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    adaptiveSphereUniformBuffer, adaptiveSphereUniformBufferMemory);
        
        // Map uniform buffer
        vkMapMemory(device, adaptiveSphereUniformBufferMemory, 0, uniformBufferSize, 0, &adaptiveSphereUniformBufferMapped);
    }
    
    // Create counter buffer (only if not already created)
    if (!adaptiveSphereCounterBuffer) {
        size_t counterBufferSize = 4 * sizeof(uint32_t);  // 4 counters
        createBuffer(counterBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    adaptiveSphereCounterBuffer, adaptiveSphereCounterBufferMemory);
    }
    
    // Allocate descriptor set (only if not already allocated)
    if (!adaptiveSphereDescriptorSet) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &adaptiveSphereDescriptorSetLayout;
        
        if (vkAllocateDescriptorSets(device, &allocInfo, &adaptiveSphereDescriptorSet) != VK_SUCCESS) {
            std::cerr << "ERROR: Failed to allocate descriptor set!\n";
            return false;
        }
    }
    
    // Check if gpuOctree is available for binding - REQUIRED
    if (!gpuOctree) {
        std::cerr << "FATAL ERROR: GPU octree not initialized! Cannot bind octree buffers to compute shader.\n";
        std::cerr << "This is a critical error - the GPU compute shader requires octree data.\n";
        assert(false && "GPU octree must be initialized before allocating GPU mesh buffers");
        std::abort();  // Force crash with core dump
    }
    
    // Update descriptor set - now with 7 bindings for octree data
    std::array<VkWriteDescriptorSet, 6> descriptorWrites{};
    
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = adaptiveSphereUniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(AdaptiveSphereUBO);
    
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = adaptiveSphereDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uniformBufferInfo;
    
    VkDescriptorBufferInfo vertexBufferInfo{};
    vertexBufferInfo.buffer = meshVertexBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = vertexBufferSize;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = adaptiveSphereDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &vertexBufferInfo;
    
    VkDescriptorBufferInfo indexBufferInfo{};
    indexBufferInfo.buffer = meshIndexBuffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range = indexBufferSize;
    
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = adaptiveSphereDescriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &indexBufferInfo;
    
    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = adaptiveSphereCounterBuffer;
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = 4 * sizeof(uint32_t);  // 4 counters
    
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = adaptiveSphereDescriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &counterBufferInfo;
    
    // Octree buffers - bindings 4 and 6 (skip 5)
    VkDescriptorBufferInfo octreeNodeInfo{};
    VkDescriptorBufferInfo voxelInfo{};
    
    // No fallback - gpuOctree is required at this point
    assert(gpuOctree && "GPU octree must exist");
    
    // Use actual octree buffers
    octreeNodeInfo.buffer = gpuOctree->getNodeBuffer();
    octreeNodeInfo.offset = 0;
    octreeNodeInfo.range = VK_WHOLE_SIZE;
    
    // For now, use the same buffer for indices (we'll need to split this later)
    
    voxelInfo.buffer = gpuOctree->getVoxelBuffer();
    voxelInfo.offset = 0;
    voxelInfo.range = VK_WHOLE_SIZE;
    
    // Validate buffers are not null
    if (!octreeNodeInfo.buffer || !voxelInfo.buffer) {
        std::cerr << "FATAL ERROR: GPU octree buffers are NULL!\n";
        std::cerr << "Node buffer: " << octreeNodeInfo.buffer << "\n";
        std::cerr << "Voxel buffer: " << voxelInfo.buffer << "\n";
        assert(false && "GPU octree buffers must be valid");
        std::abort();
    }
    
    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = adaptiveSphereDescriptorSet;
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pBufferInfo = &octreeNodeInfo;
    
    // Skip binding 5, write to binding 6 using array index 5
    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = adaptiveSphereDescriptorSet;
    descriptorWrites[5].dstBinding = 6;  // Binding point 6, not 5!
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &voxelInfo;
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    
    std::cout << "GPU mesh buffers allocated successfully!\n";
    return true;
}

bool VulkanRenderer::dispatchAdaptiveSphereCompute(
    const glm::vec3& cameraPos, 
    float planetRadius,
    int highDetailLevel, 
    int lowDetailLevel, 
    bool flipFrontBack) {
    
    std::cout << "Dispatching adaptive sphere compute shader...\n";
    
    // Wait for any previous operations to complete
    vkQueueWaitIdle(graphicsQueue);
    
    // Check if uniform buffer is still mapped
    if (!adaptiveSphereUniformBufferMapped) {
        std::cerr << "ERROR: Uniform buffer not mapped!\n";
        return false;
    }
    
    // Update uniform buffer
    AdaptiveSphereUBO ubo{};
    ubo.viewMatrix = currentCamera ? currentCamera->getViewMatrix() : glm::mat4(1.0f);
    ubo.projMatrix = currentCamera ? currentCamera->getProjectionMatrix() : glm::mat4(1.0f);
    ubo.cameraPos = cameraPos;
    ubo.planetRadius = planetRadius;
    ubo.planetCenter = glm::vec3(0.0f);
    ubo.time = 0.0f;  // Could use actual time for animation
    ubo.highDetailLevel = highDetailLevel;
    ubo.lowDetailLevel = lowDetailLevel;
    ubo.flipFrontBack = flipFrontBack ? 1 : 0;
    ubo.maxVertices = 1000000;  // 1M vertices max
    ubo.maxIndices = 3000000;   // 3M indices max (1M triangles)
    
    memcpy(adaptiveSphereUniformBufferMapped, &ubo, sizeof(ubo));
    
    // Create a separate command buffer for compute operations
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
    
    // Reset counters to zero
    vkCmdFillBuffer(commandBuffer, adaptiveSphereCounterBuffer, 0, 16, 0);
    
    // Memory barrier after fill
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);
    
    // Bind compute pipeline and descriptor set
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, adaptiveSphereComputePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           adaptiveSphereComputePipelineLayout, 0, 1,
                           &adaptiveSphereDescriptorSet, 0, nullptr);
    
    // Dispatch compute shader (20 triangles, 32 threads per group)
    vkCmdDispatch(commandBuffer, 1, 1, 1);  // Single dispatch, threads handle triangles
    
    // Memory barrier before using the generated mesh
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                        0, 1, &barrier, 0, nullptr, 0, nullptr);
    
    // End command buffer for compute work
    vkEndCommandBuffer(commandBuffer);
    
    // Submit compute work
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    // Create fence for synchronization
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence computeFence;
    if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to create compute fence!\n";
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }
    
    // Submit with fence
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, computeFence) != VK_SUCCESS) {
        std::cerr << "ERROR: Failed to submit compute command buffer!\n";
        vkDestroyFence(device, computeFence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }
    
    // Wait for completion with timeout (2 seconds)
    VkResult result = vkWaitForFences(device, 1, &computeFence, VK_TRUE, 2000000000);
    if (result == VK_TIMEOUT) {
        std::cerr << "ERROR: Compute shader timeout after 2 seconds!\n";
        vkDestroyFence(device, computeFence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    } else if (result != VK_SUCCESS) {
        std::cerr << "ERROR: Fence wait failed with result: " << result << "\n";
        vkDestroyFence(device, computeFence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }
    
    // Clean up fence
    vkDestroyFence(device, computeFence, nullptr);
    
    // Free the compute command buffer
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    
    // Read back the actual counts from the counter buffer
    uint32_t counters[4] = {0};
    
    // Create staging buffer to read back counter values
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(16, // 4 uint32_t values
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);
    
    // Copy counter buffer to staging buffer
    VkCommandBuffer copyCmd;
    VkCommandBufferAllocateInfo copyAllocInfo{};
    copyAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    copyAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    copyAllocInfo.commandPool = commandPool;
    copyAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &copyAllocInfo, &copyCmd);
    
    VkCommandBufferBeginInfo copyBeginInfo{};
    copyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    copyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(copyCmd, &copyBeginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = 16;
    vkCmdCopyBuffer(copyCmd, adaptiveSphereCounterBuffer, stagingBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(copyCmd);
    
    VkSubmitInfo copySubmitInfo{};
    copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    copySubmitInfo.commandBufferCount = 1;
    copySubmitInfo.pCommandBuffers = &copyCmd;
    vkQueueSubmit(graphicsQueue, 1, &copySubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    // Read back the counter values
    void* data;
    vkMapMemory(device, stagingMemory, 0, 16, 0, &data);
    memcpy(counters, data, 16);
    vkUnmapMemory(device, stagingMemory);
    
    // Clean up
    vkFreeCommandBuffers(device, commandPool, 1, &copyCmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    
    // Update mesh counts from actual counters
    meshVertexCount = counters[0];
    meshIndexCount = counters[1];
    
    std::cout << "[GPU COMPUTE] Actual counts from shader: " 
              << meshVertexCount << " vertices, " 
              << meshIndexCount << " indices ("
              << counters[2] << " front faces, "
              << counters[3] << " back faces)\n";
    
    // Fallback if shader didn't generate anything
    if (meshVertexCount == 0 || meshIndexCount == 0) {
        std::cerr << "WARNING: Compute shader generated no geometry! Using fallback values.\n";
        meshVertexCount = 12;
        meshIndexCount = 60;
    }
    
    std::cout << "Compute shader dispatched and completed!\n";
    return true;
}

// Modified generateGPUMesh to use our new compute pipeline
bool VulkanRenderer::generateGPUAdaptiveSphere(octree::OctreePlanet* planet, core::Camera* camera) {
    std::cout << "\n=== GPU Adaptive Sphere Generation ===\n";
    
    // Validate inputs
    if (!planet) {
        std::cerr << "FATAL ERROR: planet is NULL in generateGPUAdaptiveSphere!\n";
        assert(false && "Planet must not be NULL");
        std::abort();
    }
    
    // Initialize GPU octree FIRST (required before pipeline for buffer allocation)
    if (!gpuOctree || !gpuOctree->getNodeBuffer() || !gpuOctree->getVoxelBuffer()) {
        std::cout << "Initializing GPU octree...\n";
        
        if (!device || !physicalDevice) {
            std::cerr << "FATAL ERROR: Vulkan device is NULL!\n";
            assert(false && "Vulkan device must be initialized");
            std::abort();
        }
        
        gpuOctree = std::make_unique<rendering::GPUOctree>(device, physicalDevice);
        
        // Upload octree data to GPU
        glm::vec3 viewPos = camera ? camera->getPosition() : glm::vec3(0, 0, 10000);
        glm::mat4 viewProj = camera ? camera->getViewProjectionMatrix() : glm::mat4(1.0f);
        
        gpuOctree->uploadOctree(planet, viewPos, viewProj, commandPool, graphicsQueue);
        
        // Validate octree was properly uploaded
        if (!gpuOctree->getNodeBuffer() || !gpuOctree->getVoxelBuffer()) {
            std::cerr << "FATAL ERROR: GPU octree upload failed! Buffers are NULL.\n";
            assert(false && "GPU octree buffers must be valid after upload");
            std::abort();
        }
        
        std::cout << "GPU octree initialized and uploaded successfully\n";
    }
    
    // Initialize pipeline AFTER octree (needs octree buffers for descriptor sets)
    static bool pipelineInitialized = false;
    static bool pipelineInitializing = false;  // Prevent concurrent initialization
    
    // FORCE RELOAD for debugging - clean up old pipeline first
    if (pipelineInitialized && adaptiveSphereComputePipeline != VK_NULL_HANDLE) {
        std::cout << "DEBUG: Cleaning up old pipeline for reload...\n";
        vkDeviceWaitIdle(device);
        vkDestroyPipeline(device, adaptiveSphereComputePipeline, nullptr);
        vkDestroyPipelineLayout(device, adaptiveSphereComputePipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, adaptiveSphereDescriptorSetLayout, nullptr);
        adaptiveSphereComputePipeline = VK_NULL_HANDLE;
        adaptiveSphereComputePipelineLayout = VK_NULL_HANDLE;
        adaptiveSphereDescriptorSetLayout = VK_NULL_HANDLE;
        adaptiveSphereDescriptorSet = VK_NULL_HANDLE;
        pipelineInitialized = false;
    }
    
    if (!pipelineInitialized) {
        if (pipelineInitializing) {
            std::cerr << "ERROR: Pipeline initialization already in progress!\n";
            return false;
        }
        
        pipelineInitializing = true;
        
        if (!createAdaptiveSphereComputePipeline()) {
            std::cerr << "ERROR: Failed to create compute pipeline!\n";
            pipelineInitializing = false;
            return false;
        }
        
        if (!allocateGPUMeshBuffers(1000000, 3000000)) {  // 1M vertices, 3M indices max
            std::cerr << "ERROR: Failed to allocate GPU buffers!\n";
            pipelineInitializing = false;
            return false;
        }
        
        pipelineInitialized = true;
        pipelineInitializing = false;
    }
    
    // Get camera position and planet radius
    glm::vec3 cameraPos = camera ? camera->getPosition() : glm::vec3(0, 0, 10000);
    float planetRadius = planet ? planet->getRadius() : 6371000.0f;
    
    // Calculate LOD levels based on camera distance (adaptive)
    float distanceToCenter = glm::length(cameraPos);
    float altitude = distanceToCenter - planetRadius;
    float altitudeRatio = altitude / planetRadius;
    
    // Adaptive LOD - more detail when closer
    // LIMIT to level 5 max to prevent GPU hang
    int highDetailLevel, lowDetailLevel;
    if (altitudeRatio > 10.0f) {
        highDetailLevel = 2;  // Very far
        lowDetailLevel = 1;
    } else if (altitudeRatio > 5.0f) {
        highDetailLevel = 3;  // Far
        lowDetailLevel = 2;
    } else if (altitudeRatio > 2.0f) {
        highDetailLevel = 4;  // Medium
        lowDetailLevel = 2;
    } else if (altitudeRatio > 1.0f) {
        highDetailLevel = 4;  // Close (limited)
        lowDetailLevel = 3;
    } else if (altitudeRatio > 0.5f) {
        highDetailLevel = 5;  // Very close (max safe level)
        lowDetailLevel = 3;
    } else {
        highDetailLevel = 5;  // Extreme close (capped at 5)
        lowDetailLevel = 3;
    }
    
    std::cout << "[GPU LOD] Distance ratio: " << altitudeRatio 
              << ", Detail: " << highDetailLevel << "/" << lowDetailLevel << "\n";
    
    // Dispatch compute shader
    if (!dispatchAdaptiveSphereCompute(cameraPos, planetRadius, 
                                      highDetailLevel, lowDetailLevel,
                                      adaptiveSphereFlipFrontBack)) {
        std::cerr << "ERROR: Failed to dispatch compute shader!\n";
        return false;
    }
    
    std::cout << "GPU adaptive sphere generated successfully!\n";
    return true;
}

} // namespace rendering