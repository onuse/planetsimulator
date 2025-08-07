#include "rendering/instance_buffer_manager.hpp"
#include <iostream>

namespace rendering {

std::vector<InstanceBufferManager::InstanceData> 
InstanceBufferManager::createInstancesFromVoxels(
    const octree::OctreePlanet::RenderData& renderData,
    Statistics* stats) {
    
    std::vector<InstanceData> instances;
    instances.reserve(renderData.visibleNodes.size() * 8); // Up to 8 voxels per node
    
    Statistics localStats;
    
    for (uint32_t nodeIdx : renderData.visibleNodes) {
        const auto& node = renderData.nodes[nodeIdx];
        
        // Process leaf nodes that have voxel data
        if (node.flags & 1) { // Check leaf flag
            uint32_t voxelIdx = node.voxelIndex;
            
            if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                float voxelSize = node.halfSize * 0.5f; // Each voxel is half the node size
                
                // Process each of the 8 voxels in the node
                for (int i = 0; i < 8; i++) {
                    const auto& voxel = renderData.voxels[voxelIdx + i];
                    
                    // Skip pure air/vacuum voxels - they're invisible
                    core::MaterialID mat = voxel.getDominantMaterialID();
                    if ((mat == core::MaterialID::Air || mat == core::MaterialID::Vacuum) && 
                        voxel.getMaterialAmount(0) > 200) {
                        localStats.airCount++;
                        continue;
                    }
                    
                    // Create instance for this voxel
                    InstanceData instance;
                    instance.center = node.center + getVoxelOffset(i, voxelSize);
                    instance.halfSize = voxelSize;
                    
                    // Get blended color from mixed voxel
                    glm::vec3 color = voxel.getColor();
                    
                    // Map material ID to old material type numbers for shader compatibility
                    core::MaterialID dominantID = voxel.getDominantMaterialID();
                    float matType = 0.0f;
                    
                    if (dominantID == core::MaterialID::Rock || dominantID == core::MaterialID::Granite || 
                        dominantID == core::MaterialID::Basalt || dominantID == core::MaterialID::Sand ||
                        dominantID == core::MaterialID::Soil || dominantID == core::MaterialID::Clay) {
                        matType = 1.0f;  // Rock-like
                        localStats.rockCount++;
                    } else if (dominantID == core::MaterialID::Water || dominantID == core::MaterialID::Ice) {
                        matType = 2.0f;  // Water-like
                        localStats.waterCount++;
                    } else if (dominantID == core::MaterialID::Lava) {
                        matType = 3.0f;  // Magma/Lava
                        localStats.magmaCount++;
                    }
                    
                    instance.colorAndMaterial = glm::vec4(color, matType);
                    
                    instances.push_back(instance);
                }
            }
        }
    }
    
    localStats.totalInstances = static_cast<int>(instances.size());
    
    if (stats) {
        *stats = localStats;
    }
    
    return instances;
}

bool InstanceBufferManager::validateInstances(const std::vector<InstanceData>& instances) {
    for (size_t i = 0; i < instances.size(); i++) {
        const auto& inst = instances[i];
        
        // Check for NaN or infinite values
        if (!std::isfinite(inst.center.x) || !std::isfinite(inst.center.y) || 
            !std::isfinite(inst.center.z) || !std::isfinite(inst.halfSize)) {
            std::cerr << "Instance " << i << " has invalid position/size values" << std::endl;
            return false;
        }
        
        // Check material type is valid
        if (inst.colorAndMaterial.w > 3) {
            std::cerr << "Instance " << i << " has invalid material type: " 
                      << inst.colorAndMaterial.w << std::endl;
            return false;
        }
        
        // Check color values are in range
        if (inst.colorAndMaterial.x < 0 || inst.colorAndMaterial.x > 1 ||
            inst.colorAndMaterial.y < 0 || inst.colorAndMaterial.y > 1 ||
            inst.colorAndMaterial.z < 0 || inst.colorAndMaterial.z > 1) {
            std::cerr << "Instance " << i << " has color values out of range" << std::endl;
            return false;
        }
    }
    
    return true;
}

glm::vec3 InstanceBufferManager::getMaterialColor(float materialType) {
    int matInt = static_cast<int>(materialType + 0.5f);
    switch (matInt) {
        case 1: // Rock
            return glm::vec3(0.5f, 0.4f, 0.3f);
        case 2: // Water
            return glm::vec3(0.0f, 0.3f, 0.7f);
        case 3: // Magma
            return glm::vec3(1.0f, 0.3f, 0.0f);
        default: // Air or unknown
            return glm::vec3(0.5f, 0.5f, 0.5f);
    }
}

glm::vec3 InstanceBufferManager::getVoxelOffset(int voxelIndex, float voxelSize) {
    // Calculate offset for voxel in 2x2x2 arrangement
    return glm::vec3(
        (voxelIndex & 1) ? voxelSize : -voxelSize,
        (voxelIndex & 2) ? voxelSize : -voxelSize,
        (voxelIndex & 4) ? voxelSize : -voxelSize
    );
}

} // namespace rendering