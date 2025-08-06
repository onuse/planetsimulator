#include "rendering/vulkan_renderer.hpp"
#include <stdexcept>
#include <array>
#include <iostream>

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
    
    // Render ImGui UI
    imguiManager.renderDebugUI(this);
    
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
    
    // ALWAYS use instance-based rendering - no dual pipelines
    if (false) { // DISABLED: ray marching pipeline
        // Use ray marching pipeline for GPU octree
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayMarchPipeline);
        
        // Push constants for runtime parameters
        struct PushConstants {
            float resolution[2];
            float planetRadius;
            int debugMode;
        } pushConstants;
        
        pushConstants.resolution[0] = static_cast<float>(swapChainExtent.width);
        pushConstants.resolution[1] = static_cast<float>(swapChainExtent.height);
        pushConstants.planetRadius = 6371000.0f; // Earth radius in meters
        pushConstants.debugMode = 1; // Debug mode: show planet sphere in red if hit
        
        vkCmdPushConstants(commandBuffer, rayMarchPipelineLayout,
                          VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                          sizeof(pushConstants), &pushConstants);
        
        // Bind ray march descriptor sets
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               rayMarchPipelineLayout, 0, 1, &rayMarchDescriptorSets[currentFrame], 0, nullptr);
        
        // Draw full-screen triangle (3 vertices, no vertex buffer needed)
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    } else {
        // Use traditional instanced rendering
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    
    // Bind vertex and instance buffers together if we have instances
    if (instanceBuffer != VK_NULL_HANDLE && visibleNodeCount > 0) {
        // Bind both vertex and instance buffers in one call
        VkBuffer buffers[] = {vertexBuffer, instanceBuffer};
        VkDeviceSize offsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, buffers, offsets);
        
        // Debug output disabled for performance
        // std::cout << "Bound vertex buffer: " << vertexBuffer << " and instance buffer: " << instanceBuffer 
        //           << " with " << visibleNodeCount << " instances" << std::endl;
    } else {
        // Just bind vertex buffer
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // std::cout << "WARNING: No instance buffer to bind! instanceBuffer=" 
        //           << (instanceBuffer != VK_NULL_HANDLE ? "valid" : "null")
        //           << ", visibleNodeCount=" << visibleNodeCount << std::endl;
    }
    
        // Bind index buffer
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        
        // Bind descriptor sets (uniforms)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
        
        // Draw command
        if (visibleNodeCount > 0) {
            // Draw instanced cubes for octree nodes
            vkCmdDrawIndexed(commandBuffer, indexCount, visibleNodeCount, 0, 0, 0);
        }
    } // End of else (traditional rendering)
    
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

void VulkanRenderer::drawFrame(octree::OctreePlanet* planet, core::Camera* camera) {
    // Debug output
    // static int frameNum = 0;
    // std::cout << "DrawFrame " << frameNum++ << " starting..." << std::endl;
    
    // Wait for previous frame
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, 100000000); // 100ms timeout
    
    // Acquire image from swap chain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, 100000000, // 100ms timeout
                                           imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    
    // Store the image index for screenshot capture
    lastRenderedImageIndex = imageIndex;
    
    // Update uniform buffer with camera matrices
    updateUniformBuffer(currentFrame, camera);
    
    // Update instance buffer with octree nodes
    // static int updateCount = 0;
    // std::cout << "About to call prepareRenderData and updateInstanceBuffer (frame " << ++updateCount << ")" << std::endl;
    auto renderData = planet->prepareRenderData(camera->getPosition(), camera->getViewProjectionMatrix());
    // std::cout << "RenderData has " << renderData.visibleNodes.size() << " visible nodes" << std::endl;
    updateInstanceBuffer(renderData);
    
    static bool debugOnce = true;
    if (debugOnce && visibleNodeCount > 0) {
        std::cout << "Rendering " << visibleNodeCount << " nodes" << std::endl;
        debugOnce = false;
    }
    
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