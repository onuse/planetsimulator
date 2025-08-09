#include "rendering/vulkan_renderer.hpp"
#include "rendering/hierarchical_gpu_octree.hpp"
#include <iostream>
#include <cmath>
#include <cstddef>
#include <array>
#include <cfloat>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering {

// ============================================================================
// Transvoxel Pipeline Creation
// ============================================================================

void VulkanRenderer::createTransvoxelPipeline() {
    std::cout << "Creating Transvoxel triangle mesh pipeline..." << std::endl;
    
    // Create descriptor set layout for triangle mesh rendering
    // Binding 0: UBO (camera matrices)
    // Binding 1: SSBO (material table)
    
std::array<VkDescriptorSetLayoutBinding, 4> layoutBindings{};
    
    // Binding 0: UBO for camera matrices
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Node Buffer (storage buffer)
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].pImmutableSamplers = nullptr;

    // Binding 2: Voxel Buffer (storage buffer)
    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[2].pImmutableSamplers = nullptr;

    // Binding 3: Material Table (storage buffer)
    layoutBindings[3].binding = 3;
    layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[3].descriptorCount = 1;
    layoutBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[3].pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &hierarchicalDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Transvoxel descriptor set layout!");
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &hierarchicalDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &hierarchicalPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Transvoxel pipeline layout!");
    }
    
    // Create triangle mesh pipeline
    createTrianglePipeline();
    
    std::cout << "Transvoxel pipeline created successfully\n";
    
    // Create descriptor sets now that layout is created
    createTransvoxelDescriptorSets();
}

void VulkanRenderer::createTransvoxelDescriptorSets() {
    std::cout << "Creating Transvoxel descriptor sets..." << std::endl;
    
    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, hierarchicalDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    hierarchicalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, hierarchicalDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Transvoxel descriptor sets!");
    }
    
    // Update descriptor sets to point to buffers
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO (camera matrices)
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffers[i];
        uboInfo.offset = 0;
        uboInfo.range = sizeof(UniformBufferObject);
        
        // SSBO for material table
        VkDescriptorBufferInfo materialTableInfo{};
        materialTableInfo.buffer = materialTableBuffer;
        materialTableInfo.offset = 0;
        materialTableInfo.range = VK_WHOLE_SIZE;
        
std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = hierarchicalDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboInfo;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = hierarchicalDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &materialTableInfo;

        // Binding 2: Dummy voxel buffer (use material table as placeholder)
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = hierarchicalDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &materialTableInfo;

        // Binding 3: Material table
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = hierarchicalDescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &materialTableInfo;
        
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
    
    std::cout << "Transvoxel descriptor sets created successfully\n";
}

void VulkanRenderer::createTrianglePipeline() {
    std::cout << "Creating triangle mesh pipeline..." << std::endl;
    
    // Load shaders - for now, use simple vertex/fragment shaders
    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;
    
    try {
        vertShaderCode = readFile("shaders/triangle.vert.spv");
        fragShaderCode = readFile("shaders/triangle.frag.spv");
        std::cout << "Loaded triangle shaders: vert=" << vertShaderCode.size() << " bytes, frag=" << fragShaderCode.size() << " bytes" << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load triangle shaders for Transvoxel: " + std::string(e.what()));
    }
    
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    std::cout << "Created shader modules successfully" << std::endl;
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    // Vertex input state - define the vertex format
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
    
    // Position attribute - location 0
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);
    
    // Color attribute - location 1
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    
    // Normal attribute - location 2
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, normal);
    
    // Texture coordinate attribute - location 3
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, texCoord);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    // Input assembly - triangles
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) windowWidth;
    viewport.height = (float) windowHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {windowWidth, windowHeight};
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disable culling to test
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = hierarchicalPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &trianglePipeline);
    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateGraphicsPipelines failed with error code: " << result << std::endl;
        throw std::runtime_error("Failed to create triangle graphics pipeline!");
    }
    
    std::cout << "Pipeline creation returned: " << result << ", handle: 0x" << std::hex << reinterpret_cast<uint64_t>(trianglePipeline) << std::dec << std::endl;
    
    if (trianglePipeline == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Pipeline handle is NULL despite successful creation!" << std::endl;
    }
    
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    
    std::cout << "Triangle mesh pipeline created, handle=0x" << std::hex << reinterpret_cast<uint64_t>(trianglePipeline) << std::dec << std::endl;
}

// ============================================================================
// Chunk Management System
// ============================================================================

void VulkanRenderer::updateChunks(octree::OctreePlanet* planet, core::Camera* camera) {
    glm::vec3 cameraPos = camera->getPosition();
    glm::vec3 planetCenter(0.0f);
    float planetRadius = planet->getRadius();
    
    // Calculate direction from planet center to camera
    glm::vec3 directionToCamera = glm::normalize(cameraPos - planetCenter);
    
    // Check if we need to update chunks (only when camera has moved significantly)
    static glm::vec3 lastChunkUpdatePos = glm::vec3(0);
    float distanceMoved = glm::length(cameraPos - lastChunkUpdatePos);
    
    // Adaptive update distance based on altitude
    const float cameraDistance = glm::length(cameraPos - planetCenter);
    const float altitude = cameraDistance - planetRadius;
    const float updateThreshold = (altitude < 10000.0f) ? 100.0f :     // Update every 100m near surface
                                  (altitude < 100000.0f) ? 1000.0f :    // Update every 1km at low altitude
                                  (altitude < 1000000.0f) ? 5000.0f :   // Update every 5km at medium altitude
                                  10000.0f;                              // Update every 10km from space
    
    if (distanceMoved < updateThreshold && !activeChunks.empty()) {
        return; // Keep existing chunks
    }
    
    lastChunkUpdatePos = cameraPos;
    
    // Clean up existing chunks before creating new ones
    if (transvoxelRenderer && !activeChunks.empty()) {
        // Wait for GPU to finish using the buffers before destroying them
        vkDeviceWaitIdle(device);
        
        for (auto& chunk : activeChunks) {
            transvoxelRenderer->destroyChunkBuffers(chunk);
        }
    }
    activeChunks.clear();
    
    // Adaptive chunk grid based on camera distance
    // (cameraDistance and altitude already calculated above)
    
    // More chunks when closer, fewer when farther
    // Increase chunk count for better coverage
    const int chunkGridSize = (altitude < 100000.0f) ? 15 :   // Very close: 15x15 grid
                             (altitude < 1000000.0f) ? 11 :    // Medium: 11x11 grid
                             (altitude < 10000000.0f) ? 9 : 7; // Far: 9x9 or 7x7 grid
    
    // Adaptive chunk size based on altitude - smaller for better resolution
    // Close to surface: smaller chunks for detail
    // Far from surface: larger chunks for coverage
    const float chunkSize = (altitude < 10000.0f) ? 500.0f :      // 500m chunks near surface
                           (altitude < 100000.0f) ? 2000.0f :      // 2km chunks at low altitude
                           (altitude < 1000000.0f) ? 10000.0f :    // 10km chunks at medium altitude
                           25000.0f;                                // 25km chunks from space
    
    // Debug output removed to reduce console spam
    // std::cout << "Creating chunks on planet surface...\n";
    // std::cout << "Planet radius: " << planetRadius << "m\n";
    // std::cout << "Camera position: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")\n";
    // std::cout << "Camera distance from planet center: " << glm::length(cameraPos - planetCenter) << "m\n";
    
    // Position chunks on planet surface in the direction of the camera
    glm::vec3 surfacePoint = directionToCamera * planetRadius;
    
    // Debug output to understand chunk positioning
    static int updateCount = 0;
    if (updateCount++ < 3) {
        std::cout << "UpdateChunks: Camera at " << glm::length(cameraPos) 
                  << "m from center, placing chunks at surface point (" 
                  << surfacePoint.x << ", " << surfacePoint.y << ", " << surfacePoint.z 
                  << ") distance=" << glm::length(surfacePoint) << "m\n";
    }
    
    // Create orthogonal basis for the surface patch
    glm::vec3 up = glm::vec3(0, 1, 0);
    if (abs(glm::dot(directionToCamera, up)) > 0.9f) {
        up = glm::vec3(1, 0, 0); // Use different up vector if nearly parallel
    }
    
    glm::vec3 right = glm::normalize(glm::cross(directionToCamera, up));
    glm::vec3 forward = glm::normalize(glm::cross(right, directionToCamera));
    
    int chunksCreated = 0;
    
    // Create a spherical distribution of chunks around the view point
    // Use spherical coordinates to distribute chunks evenly on the sphere surface
    for (int x = 0; x < chunkGridSize; x++) {
        for (int z = 0; z < chunkGridSize; z++) {
            TransvoxelChunk chunk;
            
            // Create angular offsets for spherical distribution
            // Increase angular coverage to avoid gaps between chunks
            float angleX = ((float)(x - chunkGridSize/2) / (float)chunkGridSize) * 1.2f; // Wider coverage
            float angleZ = ((float)(z - chunkGridSize/2) / (float)chunkGridSize) * 1.2f;
            
            // Rotate the surface point direction by these angles
            glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), angleX, right);
            glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), angleZ, forward);
            glm::vec3 chunkDirection = glm::vec3(rotationZ * rotationX * glm::vec4(directionToCamera, 0.0f));
            
            // Place chunk on sphere surface in this direction
            chunk.position = glm::normalize(chunkDirection) * planetRadius;
            
            // Higher resolution voxel grid for better mesh quality
            // Use 32x32x32 grid for detailed surface representation
            chunk.voxelSize = chunkSize / 32.0f; // 32x32x32 voxel grid per chunk
            chunk.lodLevel = 0;
            chunk.isDirty = true;
            chunk.hasValidMesh = false;
            
            activeChunks.push_back(chunk);
            chunksCreated++;
            
            float distanceFromOrigin = glm::length(chunk.position);
            float distanceFromCamera = glm::length(chunk.position - cameraPos);
            
            // Debug output removed
            // std::cout << "Created chunk " << chunksCreated << " at (" 
            //           << chunk.position.x << ", " << chunk.position.y << ", " << chunk.position.z 
            //           << "), distance from origin: " << distanceFromOrigin << "m"
            //           << ", distance from camera: " << distanceFromCamera << "m"
            //           << ", voxelSize: " << chunk.voxelSize << "m\n";
        }
    }
    
    // std::cout << "Created " << chunksCreated << " chunks on planet surface\n";
}

void VulkanRenderer::generateChunkMeshes(octree::OctreePlanet* planet) {
    if (!transvoxelRenderer) return;
    
    // Generate meshes for all dirty chunks
    for (auto& chunk : activeChunks) {
        if (chunk.isDirty || !chunk.hasValidMesh) {
            try {
                // Use transvoxel renderer to generate mesh
                if (transvoxelRenderer) {
                    transvoxelRenderer->generateMesh(chunk, *planet);
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to generate mesh for chunk: " << e.what() << std::endl;
            }
        }
    }
}

} // namespace rendering