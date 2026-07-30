#include "rendering/vulkan_renderer.hpp"
#include "utils/log.hpp"
#include "algorithms/mesh_generation.hpp"
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
    util::vlog() << "Creating Transvoxel triangle mesh pipeline..." << std::endl;
    
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
    // Each patch is drawn with its own offset from the camera, worked out on
    // the CPU in double precision and handed over as a push constant.
    VkPushConstantRange patchRange{};
    patchRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    patchRange.offset = 0;
    patchRange.size = sizeof(PatchPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &hierarchicalDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &patchRange;
    
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &hierarchicalPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Transvoxel pipeline layout!");
    }
    
    // Create triangle mesh pipeline
    createTrianglePipeline();
    
    util::vlog() << "Transvoxel pipeline created successfully\n";
    
    // Create descriptor sets now that layout is created
    createTransvoxelDescriptorSets();
}

void VulkanRenderer::createTransvoxelDescriptorSets() {
    util::vlog() << "Creating Transvoxel descriptor sets..." << std::endl;
    
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
    
    util::vlog() << "Transvoxel descriptor sets created successfully\n";
}

void VulkanRenderer::createTrianglePipeline() {
    util::vlog() << "Creating triangle mesh pipeline..." << std::endl;
    
    // These two are the only shaders the renderer uses. There were three
    // nested fallback tiers here - hardcoded NDC triangles, then "test simple",
    // then the real ones - from when nothing drew at all and the question was
    // whether Vulkan was working. It was; the vertex shader was missing.
    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;

    try {
        vertShaderCode = readFile("shaders/triangle.vert.spv");
        fragShaderCode = readFile("shaders/triangle.frag.spv");
        util::vlog() << "Loaded triangle shaders: vert=" << vertShaderCode.size()
                  << " bytes, frag=" << fragShaderCode.size() << " bytes" << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load triangle shaders: " + std::string(e.what()));
    }


    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    
    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Failed to create shader modules!" << std::endl;
        std::cerr << "  Vert module: " << (vertShaderModule != VK_NULL_HANDLE ? "OK" : "FAILED") << std::endl;
        std::cerr << "  Frag module: " << (fragShaderModule != VK_NULL_HANDLE ? "OK" : "FAILED") << std::endl;
        throw std::runtime_error("Shader module creation failed!");
    }
    
    util::vlog() << "Created shader modules successfully" << std::endl;
    util::vlog() << "  Vert: 0x" << std::hex << reinterpret_cast<uint64_t>(vertShaderModule) << std::dec << std::endl;
    util::vlog() << "  Frag: 0x" << std::hex << reinterpret_cast<uint64_t>(fragShaderModule) << std::dec << std::endl;
    
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
    
    // Vertex layout, matching algorithms::MeshVertex exactly: position,
    // normal, colour, three vec3s and nothing else. The old path built a
    // separate float array in a different order and the pipeline described
    // that instead, so uploading the struct directly produced geometry made of
    // colour values.
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(algorithms::MeshVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;   // inPosition
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(algorithms::MeshVertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;   // inColor
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(algorithms::MeshVertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;   // inNormal
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(algorithms::MeshVertex, normal);

    // Cloud cover, so the sky can be drawn from the same geometry as the
    // ground rather than from a second set of patches built to hold it.
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(algorithms::MeshVertex, cloudCover);

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
    
    // Viewport state, declared dynamic.
    //
    // Without this the viewport is baked into the pipeline at creation, and
    // vkCmdSetViewport does nothing - so the window could be resized, the swap
    // chain would correctly follow it, the render area would grow, and the
    // geometry would keep being drawn into the rectangle the window happened to
    // be when the pipeline was built. The planet stayed the size and position it
    // had at startup with fresh empty space around it, which looks like the
    // camera failing to recentre and is nothing of the kind.
    //
    // The values below are only what the pipeline is created with; the real ones
    // come from the command buffer every frame.
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
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disable culling to see all triangles
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
    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                            VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = hierarchicalPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &trianglePipeline);
    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateGraphicsPipelines failed with error code: " << result << std::endl;
        throw std::runtime_error("Failed to create triangle graphics pipeline!");
    }

    // The cloud layer, from the same state and the same geometry.
    //
    // Clouds are drawn by taking each surface vertex, throwing away its
    // elevation and putting it back on a sphere a few kilometres up. That
    // reuses the patch tree, the culling, the buffer pool and the draw calls
    // exactly as they are - the alternative was a second set of patches
    // holding nothing but an altitude, which would double the memory and the
    // build cost to express one number.
    //
    // Only three pieces of state differ. It blends rather than overwrites,
    // because a cloud is not opaque. It does not write depth, because writing
    // it would let a nearer cloud hide a further one and turn an overcast sky
    // into a single flat shell. And it draws after the ground, so there is
    // something behind it to blend with.
    {
        std::vector<char> cloudVert = readFile("shaders/cloud.vert.spv");
        std::vector<char> cloudFrag = readFile("shaders/cloud.frag.spv");
        VkShaderModule cloudVertModule = createShaderModule(cloudVert);
        VkShaderModule cloudFragModule = createShaderModule(cloudFrag);

        VkPipelineShaderStageCreateInfo cloudStages[2] = {vertShaderStageInfo, fragShaderStageInfo};
        cloudStages[0].module = cloudVertModule;
        cloudStages[1].module = cloudFragModule;

        VkPipelineColorBlendAttachmentState cloudBlend = colorBlendAttachment;
        cloudBlend.blendEnable = VK_TRUE;
        cloudBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cloudBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cloudBlend.colorBlendOp = VK_BLEND_OP_ADD;
        cloudBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cloudBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cloudBlend.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo cloudBlending = colorBlending;
        cloudBlending.pAttachments = &cloudBlend;

        VkPipelineDepthStencilStateCreateInfo cloudDepth = depthStencil;
        cloudDepth.depthWriteEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo cloudInfo = pipelineInfo;
        cloudInfo.pStages = cloudStages;
        cloudInfo.pColorBlendState = &cloudBlending;
        cloudInfo.pDepthStencilState = &cloudDepth;

        result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &cloudInfo, nullptr,
                                           &cloudPipeline);
        if (result != VK_SUCCESS) {
            std::cerr << "Cloud pipeline creation failed: " << result << std::endl;
            cloudPipeline = VK_NULL_HANDLE;
        }

        vkDestroyShaderModule(device, cloudFragModule, nullptr);
        vkDestroyShaderModule(device, cloudVertModule, nullptr);
    }
    
    util::vlog() << "Pipeline creation returned: " << result << ", handle: 0x" << std::hex << reinterpret_cast<uint64_t>(trianglePipeline) << std::dec << std::endl;
    
    if (trianglePipeline == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Pipeline handle is NULL despite successful creation!" << std::endl;
    }
    
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    
    util::vlog() << "Triangle mesh pipeline created, handle=0x" << std::hex << reinterpret_cast<uint64_t>(trianglePipeline) << std::dec << std::endl;
}

// ============================================================================
// Chunk Management System
// ============================================================================

// Include sphere patch generator

void VulkanRenderer::updateChunks(octree::OctreePlanet* /*planet*/, core::Camera* /*camera*/) {
    // GPU-ONLY: CPU chunk management removed
    // All mesh generation now happens on GPU via compute shaders
    return;
}


void VulkanRenderer::generateChunkMeshes(octree::OctreePlanet* /*planet*/) {
    // CPU chunk generation removed - using GPU mesh generation only
    return;
}

// ============================================================================
// Quadtree Pipeline Creation
// ============================================================================


} // namespace rendering
