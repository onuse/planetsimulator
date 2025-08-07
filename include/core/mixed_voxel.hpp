#pragma once

#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>
#include "material_table.hpp"

namespace octree {

// Feature types for LOD preservation (keeping from old version)
enum class FeatureType : uint8_t {
    GENERIC = 0,
    MOUNTAIN_PEAK = 1,
    MOUNTAIN_SLOPE = 2,
    VALLEY = 3,
    OCEAN_DEEP = 4,
    OCEAN_SHALLOW = 5,
    COAST = 6,
    RIVER = 7,
    PLATEAU = 8
};

// New MixedVoxel with flexible material system (still 8 bytes)
struct MixedVoxel {
    // Material amounts (4 bytes) - how much of each material
    uint8_t amounts[4];
    
    // Material IDs (2 bytes) - which materials (4 bits each)
    // materialIds[0] = [material1_id:4][material0_id:4]
    // materialIds[1] = [material3_id:4][material2_id:4]
    uint8_t materialIds[2];
    
    // Physical properties (2 bytes)
    uint8_t temperature;  // 0-255 mapped to real temperature
    uint8_t pressure;     // 0-255 mapped to pressure/depth
    
    // Default constructor - creates empty/vacuum voxel
    MixedVoxel() : amounts{0, 0, 0, 0}, materialIds{0, 0}, 
                   temperature(128), pressure(128) {
        // Set first material to Vacuum with full amount
        setMaterial(0, core::MaterialID::Vacuum, 255);
    }
    
    // === Material Access Methods ===
    
    // Set a specific material slot
    void setMaterial(int slot, core::MaterialID id, uint8_t amount) {
        if (slot < 0 || slot > 3) return;
        
        amounts[slot] = amount;
        
        // Pack material ID into the right nibble
        if (slot == 0) {
            materialIds[0] = (materialIds[0] & 0xF0) | (static_cast<uint8_t>(id) & 0x0F);
        } else if (slot == 1) {
            materialIds[0] = (materialIds[0] & 0x0F) | ((static_cast<uint8_t>(id) & 0x0F) << 4);
        } else if (slot == 2) {
            materialIds[1] = (materialIds[1] & 0xF0) | (static_cast<uint8_t>(id) & 0x0F);
        } else { // slot == 3
            materialIds[1] = (materialIds[1] & 0x0F) | ((static_cast<uint8_t>(id) & 0x0F) << 4);
        }
    }
    
    // Set all materials at once
    void setMaterials(core::MaterialID id0, uint8_t amt0,
                      core::MaterialID id1, uint8_t amt1,
                      core::MaterialID id2, uint8_t amt2,
                      core::MaterialID id3, uint8_t amt3) {
        setMaterial(0, id0, amt0);
        setMaterial(1, id1, amt1);
        setMaterial(2, id2, amt2);
        setMaterial(3, id3, amt3);
    }
    
    // Get material ID at slot
    core::MaterialID getMaterialID(int slot) const {
        if (slot < 0 || slot > 3) return core::MaterialID::Vacuum;
        
        if (slot == 0) {
            return static_cast<core::MaterialID>(materialIds[0] & 0x0F);
        } else if (slot == 1) {
            return static_cast<core::MaterialID>((materialIds[0] >> 4) & 0x0F);
        } else if (slot == 2) {
            return static_cast<core::MaterialID>(materialIds[1] & 0x0F);
        } else { // slot == 3
            return static_cast<core::MaterialID>((materialIds[1] >> 4) & 0x0F);
        }
    }
    
    // Get material amount at slot
    uint8_t getMaterialAmount(int slot) const {
        if (slot < 0 || slot > 3) return 0;
        return amounts[slot];
    }
    
    // === Analysis Methods ===
    
    // Get the dominant material ID
    core::MaterialID getDominantMaterialID() const {
        uint8_t maxAmount = 0;
        core::MaterialID dominantId = core::MaterialID::Vacuum;
        
        for (int i = 0; i < 4; i++) {
            if (amounts[i] > maxAmount) {
                maxAmount = amounts[i];
                dominantId = getMaterialID(i);
            }
        }
        
        return dominantId;
    }
    
    // Check if voxel is empty (only vacuum/air)
    bool isEmpty() const {
        for (int i = 0; i < 4; i++) {
            core::MaterialID id = getMaterialID(i);
            if (amounts[i] > 0 && id != core::MaterialID::Vacuum && id != core::MaterialID::Air) {
                return false;
            }
        }
        return true;
    }
    
    // Should this voxel be rendered?
    bool shouldRender() const {
        // Don't render pure vacuum or pure air
        if (isEmpty()) return false;
        
        // Check if we have any solid materials
        for (int i = 0; i < 4; i++) {
            core::MaterialID id = getMaterialID(i);
            if (amounts[i] > 0 && 
                id != core::MaterialID::Vacuum && 
                id != core::MaterialID::Air) {
                return true;
            }
        }
        
        return false;
    }
    
    // Get interpolated color for rendering
    glm::vec3 getColor() const {
        auto& matTable = core::MaterialTable::getInstance();
        
        // Calculate total for normalization
        float total = 0;
        for (int i = 0; i < 4; i++) {
            total += amounts[i];
        }
        
        if (total == 0) {
            return matTable.getColor(core::MaterialID::Vacuum);
        }
        
        // Blend colors based on composition
        glm::vec3 color(0.0f);
        for (int i = 0; i < 4; i++) {
            if (amounts[i] > 0) {
                color += matTable.getColor(getMaterialID(i)) * (amounts[i] / total);
            }
        }
        
        // Apply temperature tint (optional, from old version)
        float temp_norm = temperature / 255.0f;
        if (temp_norm < 0.5f) {
            // Cold: add blue tint
            color = glm::mix(color, glm::vec3(0.5f, 0.7f, 1.0f), 
                           (0.5f - temp_norm) * 0.2f);
        } else if (temp_norm > 0.5f) {
            // Hot: add red tint
            color = glm::mix(color, glm::vec3(1.0f, 0.5f, 0.3f), 
                           (temp_norm - 0.5f) * 0.2f);
        }
        
        return color;
    }
    
    // === Factory Methods ===
    
    // Create a pure single-material voxel
    static MixedVoxel createPure(core::MaterialID material) {
        MixedVoxel v;
        v.setMaterial(0, material, 255);
        v.setMaterial(1, core::MaterialID::Vacuum, 0);
        v.setMaterial(2, core::MaterialID::Vacuum, 0);
        v.setMaterial(3, core::MaterialID::Vacuum, 0);
        return v;
    }
    
    // Create a two-material mix
    static MixedVoxel createMix(core::MaterialID mat1, uint8_t amt1,
                                core::MaterialID mat2, uint8_t amt2) {
        MixedVoxel v;
        v.setMaterial(0, mat1, amt1);
        v.setMaterial(1, mat2, amt2);
        v.setMaterial(2, core::MaterialID::Vacuum, 0);
        v.setMaterial(3, core::MaterialID::Vacuum, 0);
        return v;
    }
    
    // Create empty/vacuum voxel
    static MixedVoxel createEmpty() {
        MixedVoxel v;
        v.setMaterial(0, core::MaterialID::Vacuum, 255);
        v.setMaterial(1, core::MaterialID::Vacuum, 0);
        v.setMaterial(2, core::MaterialID::Vacuum, 0);
        v.setMaterial(3, core::MaterialID::Vacuum, 0);
        return v;
    }
    
    // Average multiple voxels (for LOD)
    static MixedVoxel average(const MixedVoxel children[], int count = 8) {
        // Count total amounts for each material type
        struct MaterialSum {
            core::MaterialID id;
            uint32_t total;
        };
        
        MaterialSum sums[16] = {};  // Support all 16 material types
        
        // Sum up all materials from children
        for (int child = 0; child < count; child++) {
            for (int slot = 0; slot < 4; slot++) {
                core::MaterialID id = children[child].getMaterialID(slot);
                uint8_t amount = children[child].getMaterialAmount(slot);
                
                // Find or add this material to sums
                bool found = false;
                for (int i = 0; i < 16; i++) {
                    if (sums[i].id == id || sums[i].total == 0) {
                        sums[i].id = id;
                        sums[i].total += amount;
                        found = true;
                        break;
                    }
                }
            }
        }
        
        // Sort by total amount (descending)
        std::sort(sums, sums + 16, [](const MaterialSum& a, const MaterialSum& b) {
            return a.total > b.total;
        });
        
        // Create result with top 4 materials
        MixedVoxel result;
        for (int i = 0; i < 4; i++) {
            if (sums[i].total > 0) {
                // Average the amount
                uint8_t avgAmount = static_cast<uint8_t>(
                    std::min(255u, sums[i].total / count)
                );
                result.setMaterial(i, sums[i].id, avgAmount);
            } else {
                result.setMaterial(i, core::MaterialID::Vacuum, 0);
            }
        }
        
        // Average temperature and pressure
        uint32_t tempSum = 0, pressSum = 0;
        for (int i = 0; i < count; i++) {
            tempSum += children[i].temperature;
            pressSum += children[i].pressure;
        }
        result.temperature = static_cast<uint8_t>(tempSum / count);
        result.pressure = static_cast<uint8_t>(pressSum / count);
        
        return result;
    }
    
};

// VoxelAverager class - now just delegates to MixedVoxel
class VoxelAverager {
public:
    static MixedVoxel average(const MixedVoxel children[8]) {
        return MixedVoxel::average(children, 8);
    }
};

} // namespace octree