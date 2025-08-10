#include "rendering/vulkan_renderer.hpp"
#include <stdexcept>
#include <array>
#include <iostream>
#include <glm/glm.hpp>

namespace rendering {

// ============================================================================
// Command Buffer Infrastructure
// ============================================================================

void VulkanRenderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
    
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void VulkanRenderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
    
    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Start ImGui frame
    imguiManager.newFrame();
    
    // Render ImGui UI (pass camera for debug display)
    imguiManager.renderDebugUI(this, currentCamera);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;
    
    // Clear values for attachments
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.02f, 1.0f}}; // Dark background
    clearValues[1].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    
    // Set scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // EXCLUSIVE RENDERING PATHS - Only one system renders at a time
    bool renderedSomething = false;
    
    // LOD SYSTEM RENDERING
    if (lodManager) {
        auto lodMode = lodManager->getCurrentMode();
        
        if (lodMode == rendering::LODManager::QUADTREE_ONLY) {
            // Render LOD quadtree patches when at far distance
            if (quadtreePipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadtreePipeline);
                
                // Bind quadtree descriptor sets
                if (!quadtreeDescriptorSets.empty()) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                           quadtreePipelineLayout, 0, 1,
                                           &quadtreeDescriptorSets[currentFrame], 0, nullptr);
                }
                
                // Render the LOD terrain
                lodManager->render(commandBuffer, quadtreePipelineLayout, glm::mat4(1.0f));
                renderedSomething = true;
            }
        } else if (lodMode == rendering::LODManager::TRANSITION_ZONE) {
            // In transition zone, render both with blending
            // For now, just render quadtree until transition is fully implemented
            if (quadtreePipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadtreePipeline);
                if (!quadtreeDescriptorSets.empty()) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                           quadtreePipelineLayout, 0, 1,
                                           &quadtreeDescriptorSets[currentFrame], 0, nullptr);
                }
                lodManager->render(commandBuffer, quadtreePipelineLayout, glm::mat4(1.0f));
                renderedSomething = true;
            }
        } else if (lodMode == rendering::LODManager::OCTREE_TRANSVOXEL) {
            // Close range - use transvoxel rendering
            // Fall through to transvoxel rendering below
            renderedSomething = false; // Let transvoxel handle it
        }
    }
    
    // TRANSVOXEL MESH RENDERING - Only if LOD didn't render
    
    // Debug output for investigating chunk rendering issue
    static int debugFrameCount = 0;
    if (debugFrameCount++ % 60 == 0) { // Every 60 frames
        int validChunks = 0;
        int chunksWithBuffers = 0;
        for (const auto& chunk : activeChunks) {
            if (chunk.hasValidMesh) validChunks++;
            if (chunk.vertexBuffer != VK_NULL_HANDLE && chunk.indexBuffer != VK_NULL_HANDLE) {
                chunksWithBuffers++;
            }
        }
        // std::cout << "[RENDER DEBUG] activeChunks: " << activeChunks.size() 
        //           << ", validMeshes: " << validChunks 
        //           << ", chunksWithBuffers: " << chunksWithBuffers
        //           << ", transvoxelRenderer: " << (transvoxelRenderer ? "valid" : "null") << std::endl;
    }
    
    // Only render transvoxel if LOD system didn't render anything
    if (!renderedSomething) {
        if (transvoxelRenderer && !activeChunks.empty()) {
            // Bind the triangle mesh pipeline for Transvoxel rendering
            if (trianglePipeline == VK_NULL_HANDLE) {
                std::cerr << "ERROR: trianglePipeline is NULL when rendering chunks!" << std::endl;
            }
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
            
            // Bind uniform descriptor sets
            if (!hierarchicalDescriptorSets.empty()) {
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       hierarchicalPipelineLayout, 0, 1, 
                                       &hierarchicalDescriptorSets[currentFrame], 0, nullptr);
            }
            
            // Render all active chunks with valid meshes
            transvoxelRenderer->render(activeChunks, commandBuffer, hierarchicalPipelineLayout);
        } else if (!lodManager) {
            // Only use fallback triangle if no LOD manager exists
            // Use trianglePipeline for fallback render since hierarchicalPipeline is not created
            if (trianglePipeline == VK_NULL_HANDLE) {
                std::cerr << "ERROR: trianglePipeline is NULL in fallback render!" << std::endl;
            }
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        }
    }
    
    // Render ImGui
    imguiManager.render(commandBuffer);
    
    // End render pass
    vkCmdEndRenderPass(commandBuffer);
    
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

// ============================================================================
// Synchronization Objects
// ============================================================================

void VulkanRenderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

// ============================================================================
// Frame Drawing
// ============================================================================

void VulkanRenderer::drawFrame(octree::OctreePlanet* /*planet*/, core::Camera* camera) {
    // Debug output
    // static int frameNum = 0;
    // std::cout << "DrawFrame " << frameNum++ << " starting..." << std::endl;
    
    // Wait for previous frame
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, 100000000); // 100ms timeout
    
    // Acquire image from swap chain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, // No timeout - wait indefinitely
                                           imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result == VK_TIMEOUT) {
        std::cerr << "Warning: Swap chain acquire timed out, retrying..." << std::endl;
        return; // Skip this frame
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Swap chain acquire failed with error code: " << result << std::endl;
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    
    // Store the image index for screenshot capture
    lastRenderedImageIndex = imageIndex;
    
    // Update uniform buffer with camera matrices
    updateUniformBuffer(currentFrame, camera);
    
    // No instance buffer needed for Transvoxel rendering
    // Chunks are managed and rendered directly via vertex/index buffers
    
    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    
    // Record command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);
    
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
    
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }
    
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace rendering