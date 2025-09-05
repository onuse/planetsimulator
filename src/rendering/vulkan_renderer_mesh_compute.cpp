// GPU Mesh Generation using Compute Shaders
#include "rendering/vulkan_renderer.hpp"
#include <iostream>
#include <vector>

namespace rendering {

bool VulkanRenderer::generateGPUMesh(octree::OctreePlanet* planet, core::Camera* camera) {
    // All GPU compute pipelines now use the adaptive sphere implementation
    if (meshPipeline == MeshPipeline::GPU_COMPUTE || meshPipeline == MeshPipeline::GPU_WITH_CPU_VERIFY) {
        if (meshPipeline == MeshPipeline::GPU_COMPUTE) {
            std::cout << "\n=== GPU COMPUTE PIPELINE ===\n";
        } else {
            std::cout << "\n=== GPU WITH CPU VERIFY PIPELINE ===\n";
        }
        
        // TEMPORARY: Skip Transvoxel to test debug shader directly
        // bool result = generateGPUTransvoxelMesh(planet, camera);
        std::cout << "DEBUG: Skipping Transvoxel, going straight to adaptive sphere for testing\n";
        bool result = generateGPUAdaptiveSphere(planet, camera);
        
        if (!result) {
            std::cout << "WARNING: Adaptive sphere failed, trying with octree\n";
            result = generateGPUAdaptiveSphereWithOctree(planet, camera);
        }
        
        if (!result) {
            std::cout << "ERROR: All GPU compute methods failed!\n";
            std::cout << "Press G to switch back to CPU_ADAPTIVE pipeline.\n";
            std::cout << "===============================================\n";
        }
        return result;
    }
    
    // This function should only be called for GPU pipelines
    std::cerr << "ERROR: generateGPUMesh called with non-GPU pipeline: " << static_cast<int>(meshPipeline) << "\n";
    return false;
}

} // namespace rendering