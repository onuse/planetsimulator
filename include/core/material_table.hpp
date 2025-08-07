#pragma once

#include <glm/glm.hpp>
#include <array>
#include <string>

namespace core {

// Material IDs - 4 bits each, supports up to 16 material types
enum class MaterialID : uint8_t {
    // Fundamental materials (always needed)
    Vacuum  = 0,   // Empty space (black)
    Air     = 1,   // Atmosphere (light blue)
    Rock    = 2,   // Generic rock (gray-brown)
    Water   = 3,   // Liquid water (blue)
    
    // Common surface materials
    Sand    = 4,   // Desert/beach (tan)
    Soil    = 5,   // Fertile earth (dark brown)
    Grass   = 6,   // Vegetation (green)
    Snow    = 7,   // Snow/frost (white)
    Ice     = 8,   // Solid ice (light blue-white)
    
    // Geological varieties
    Granite = 9,   // Hard rock (speckled gray)
    Basalt  = 10,  // Volcanic rock (dark gray)
    Clay    = 11,  // Sedimentary (red-brown)
    
    // Special materials
    Lava    = 12,  // Molten rock (orange-red, emissive)
    Metal   = 13,  // Iron/ore (metallic gray)
    Crystal = 14,  // Gems/ice (translucent)
    
    // Reserved for future
    Reserved = 15,
    
    COUNT = 16
};

// Properties for each material type
struct MaterialProperties {
    glm::vec3 color;        // Base RGB color
    float roughness;        // 0=smooth, 1=rough (for lighting)
    float metallic;         // 0=dielectric, 1=metal
    float emissive;         // 0=none, >0=glows
    float density;          // kg/m³ (for physics later)
    float hardness;         // 0=liquid, 1=solid (for erosion)
    
    std::string name;       // For debugging/UI
    
    // Constructor for easy initialization
    MaterialProperties(
        const glm::vec3& col = glm::vec3(0.5f),
        float rough = 0.8f,
        float metal = 0.0f,
        float emit = 0.0f,
        float dens = 2500.0f,
        float hard = 1.0f,
        const std::string& n = "Unknown"
    ) : color(col), roughness(rough), metallic(metal), 
        emissive(emit), density(dens), hardness(hard), name(n) {}
};

// Singleton material table for consistent material properties
class MaterialTable {
public:
    // Get singleton instance
    static MaterialTable& getInstance() {
        static MaterialTable instance;
        return instance;
    }
    
    // Get material properties by ID
    const MaterialProperties& getMaterial(MaterialID id) const {
        return materials[static_cast<size_t>(id)];
    }
    
    // Get material properties by index
    const MaterialProperties& getMaterial(size_t index) const {
        return materials[index];
    }
    
    // Get color for a material
    glm::vec3 getColor(MaterialID id) const {
        return materials[static_cast<size_t>(id)].color;
    }
    
    // For GPU upload - packed format
    struct GPUMaterialData {
        glm::vec4 colorAndRoughness;  // xyz=color, w=roughness
        glm::vec4 properties;          // x=metallic, y=emissive, z=density/1000, w=hardness
    };
    
    // Get all materials in GPU-friendly format
    std::array<GPUMaterialData, 16> getGPUData() const;
    
    // Get size for GPU buffer
    static constexpr size_t getGPUDataSize() {
        return sizeof(GPUMaterialData) * 16;
    }
    
private:
    MaterialTable();
    MaterialTable(const MaterialTable&) = delete;
    MaterialTable& operator=(const MaterialTable&) = delete;
    
    std::array<MaterialProperties, 16> materials;
};

// Helper functions for packing/unpacking material IDs
inline uint8_t packMaterialIDs(MaterialID id0, MaterialID id1) {
    return (static_cast<uint8_t>(id0) & 0x0F) | 
           ((static_cast<uint8_t>(id1) & 0x0F) << 4);
}

inline void unpackMaterialIDs(uint8_t packed, MaterialID& id0, MaterialID& id1) {
    id0 = static_cast<MaterialID>(packed & 0x0F);
    id1 = static_cast<MaterialID>((packed >> 4) & 0x0F);
}

} // namespace core