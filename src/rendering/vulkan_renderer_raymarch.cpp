#include "rendering/vulkan_renderer.hpp"

namespace rendering {

// DEPRECATED: Ray-marching pipeline replaced by Transvoxel triangle mesh rendering
// These functions are stubbed out to maintain compilation compatibility

void VulkanRenderer::createHierarchicalPipeline() {
    // No-op: Ray-marching pipeline no longer used
}

void VulkanRenderer::createHierarchicalDescriptorSets() {
    // No-op: Ray-marching descriptor sets no longer used
}

} // namespace rendering