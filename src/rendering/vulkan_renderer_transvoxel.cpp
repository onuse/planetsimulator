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

// Include sphere patch generator
#include "../sphere_patch_generator.cpp"

void VulkanRenderer::updateChunks(octree::OctreePlanet* planet, core::Camera* camera) {
    glm::vec3 cameraPos = camera->getPosition();
    glm::vec3 planetCenter(0.0f);
    float planetRadius = planet->getRadius();
    
    // Create planet sphere patches once
    static bool spherePatchesCreated = false;
    // DISABLED: Old sphere patches - we're using LOD quadtree now
    if (false && !spherePatchesCreated) {
        activeChunks.clear();
        
        std::cout << "Creating planet sphere patches...\n";
        
        // Generate sphere patches at planet scale
        int resolution = 3;  // 384 patches (8x8 per cube face)
        
        std::cout << "Creating sphere with radius=" << planetRadius/1000000.0f
                  << "M meters, resolution=" << resolution 
                  << " (" << (6 * (1 << (2*resolution))) << " patches)\n";
        
        // Generate all sphere patches with planet material data
        auto patches = sphere_patches::generateSphere(planetRadius, resolution, planet);
        
        // Add all patches as chunks
        for (auto& patch : patches) {
            patch.isDirty = true;
            patch.hasValidMesh = false;
            activeChunks.push_back(patch);
        }
        
        std::cout << "Created " << activeChunks.size() << " sphere patches\n";
        std::cout << "Each patch has " << (patches.empty() ? 0 : patches[0].vertices.size()) 
                  << " vertices\n";
        
        // Position camera to view the full planet
        float viewDistance = planetRadius * 2.5f; // View from 2.5x planet radius
        camera->setPosition(glm::vec3(0, 0, viewDistance));
        camera->lookAt(glm::vec3(0, 0, 0));
        camera->setMode(core::CameraMode::Orbital); // Ensure orbital mode for drag rotation
        std::cout << "Camera positioned at " << viewDistance/1000000.0f << "M meters from planet center\n";
        
        spherePatchesCreated = true;
    }
    
    // Sphere patches are static, no need to update them
    return;
}

void VulkanRenderer::generateChunkMeshes(octree::OctreePlanet* /*planet*/) {
    if (!transvoxelRenderer) return;
    
    // Track mesh generation
    int meshesGenerated = 0;
    int validMeshes = 0;
    int alreadyValid = 0;
    
    // Generate meshes for all dirty chunks
    for (auto& chunk : activeChunks) {
        if (chunk.hasValidMesh) {
            alreadyValid++;
        }
        
        if (chunk.isDirty || !chunk.hasValidMesh) {
            // Sphere patches already have vertices, just create GPU buffers
            if (!chunk.vertices.empty()) {
                try {
                    transvoxelRenderer->createVertexBuffer(chunk);
                    transvoxelRenderer->createIndexBuffer(chunk);
                    chunk.hasValidMesh = true;
                    chunk.isDirty = false;
                    meshesGenerated++;
                    validMeshes++;
                    // Removed verbose output
                } catch (const std::exception& e) {
                    std::cerr << "Failed to create GPU buffers: " << e.what() << std::endl;
                }
            }
        }
    }
    
    // Debug output occasionally
    static int frameCount = 0;
    if (frameCount++ % 600 == 0 && meshesGenerated > 0) {  // Every 600 frames (about 10 seconds)
        std::cout << "Mesh generation: " << meshesGenerated << " new, " 
                  << validMeshes << " valid total\n";
    }
}

// ============================================================================
// Quadtree Pipeline Creation
// ============================================================================

void VulkanRenderer::createQuadtreePipeline() {
    std::cout << "Creating Quadtree LOD pipeline..." << std::endl;
    
    // Create descriptor set layout for quadtree rendering
    // Binding 0: UBO (camera matrices)
    // Binding 1: SSBO (instance data for patches)
    
    std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings{};
    
    // Binding 0: UBO for camera matrices
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Instance data buffer (storage buffer)
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBindings[1].pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &quadtreeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Quadtree descriptor set layout!");
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &quadtreeDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &quadtreePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Quadtree pipeline layout!");
    }
    
    // Load quadtree shaders
    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;
    
    try {
        vertShaderCode = readFile("shaders/quadtree_patch.vert.spv");
        fragShaderCode = readFile("shaders/quadtree_patch.frag.spv");
        std::cout << "Loaded quadtree shaders: vert=" << vertShaderCode.size() << " bytes, frag=" << fragShaderCode.size() << " bytes" << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load quadtree shaders: " + std::string(e.what()));
    }
    
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    
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
    
    // Vertex input state - just 2D UV coordinates for the patch
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(glm::vec2);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding = 0;
    attributeDescription.location = 0;
    attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescription.offset = 0;
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;
    
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
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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
    
    // Dynamic state for viewport and scissor
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    
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
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = quadtreePipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &quadtreePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Quadtree graphics pipeline!");
    }
    
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    
    std::cout << "Quadtree pipeline created successfully\n";
    
    // Create descriptor sets
    createQuadtreeDescriptorSets();
}

void VulkanRenderer::createQuadtreeDescriptorSets() {
    std::cout << "Creating Quadtree descriptor sets..." << std::endl;
    
    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, quadtreeDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    
    quadtreeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, quadtreeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Quadtree descriptor sets!");
    }
    
    // Update descriptor sets to point to buffers
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO (camera matrices)
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffers[i];
        uboInfo.offset = 0;
        uboInfo.range = sizeof(UniformBufferObject);
        
        // For now, use a dummy buffer for instance data (will be updated by LODManager)
        VkDescriptorBufferInfo instanceInfo{};
        instanceInfo.buffer = uniformBuffers[i]; // Use UBO as placeholder
        instanceInfo.offset = 0;
        instanceInfo.range = sizeof(UniformBufferObject);
        
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = quadtreeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uboInfo;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = quadtreeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &instanceInfo;
        
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
    
    std::cout << "Quadtree descriptor sets created successfully\n";
}

void VulkanRenderer::updateQuadtreeInstanceBuffer(VkBuffer instanceBuffer) {
    if (instanceBuffer == VK_NULL_HANDLE) return;
    
    // Update all descriptor sets with the new instance buffer
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo instanceInfo{};
        instanceInfo.buffer = instanceBuffer;
        instanceInfo.offset = 0;
        instanceInfo.range = VK_WHOLE_SIZE;
        
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = quadtreeDescriptorSets[i];
        descriptorWrite.dstBinding = 1; // Binding 1 is instance data
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &instanceInfo;
        
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

} // namespace rendering