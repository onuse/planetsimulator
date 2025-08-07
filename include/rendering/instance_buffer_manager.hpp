#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "core/octree.hpp"

namespace rendering {

// Separated instance buffer management for better testing
class InstanceBufferManager {
public:
    struct InstanceData {
        glm::vec3 center;        // offset 0, size 12
        float halfSize;          // offset 12, size 4
        glm::vec4 colorAndMaterial; // offset 16, size 16 - xyz=color, w=material
        // Total size: 32 bytes
    };
    
    struct Statistics {
        int airCount = 0;
        int rockCount = 0;
        int waterCount = 0;
        int magmaCount = 0;
        int totalInstances = 0;
    };
    
    // Convert voxels to instances (separated from GPU buffer management)
    static std::vector<InstanceData> createInstancesFromVoxels(
        const octree::OctreePlanet::RenderData& renderData,
        Statistics* stats = nullptr);
    
    // Validate instance data
    static bool validateInstances(const std::vector<InstanceData>& instances);
    
    // Get material color for debugging
    static glm::vec3 getMaterialColor(float materialType);
    
private:
    // Helper to calculate voxel position offset
    static glm::vec3 getVoxelOffset(int voxelIndex, float voxelSize);
};

} // namespace rendering