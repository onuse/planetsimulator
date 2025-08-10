#include "core/material_table.hpp"

namespace core {

MaterialTable::MaterialTable() {
    // Initialize all materials with reasonable defaults
    // Colors are chosen to be visually distinct and physically plausible
    
    // Fundamental materials
    materials[static_cast<size_t>(MaterialID::Vacuum)] = MaterialProperties(
        glm::vec3(0.0f, 0.0f, 0.0f),  // Pure black
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, "Vacuum"
    );
    
    materials[static_cast<size_t>(MaterialID::Air)] = MaterialProperties(
        glm::vec3(0.7f, 0.85f, 1.0f),  // Light sky blue
        0.0f, 0.0f, 0.0f, 1.2f, 0.0f, "Air"
    );
    
    materials[static_cast<size_t>(MaterialID::Rock)] = MaterialProperties(
        glm::vec3(0.5f, 0.45f, 0.4f),  // Gray-brown
        0.9f, 0.0f, 0.0f, 2700.0f, 0.9f, "Rock"
    );
    
    materials[static_cast<size_t>(MaterialID::Water)] = MaterialProperties(
        glm::vec3(0.05f, 0.3f, 0.55f),  // Deep ocean blue
        0.0f, 0.0f, 0.0f, 1000.0f, 0.0f, "Water"
    );
    
    // Common surface materials
    materials[static_cast<size_t>(MaterialID::Sand)] = MaterialProperties(
        glm::vec3(0.76f, 0.7f, 0.5f),  // Beach tan
        0.8f, 0.0f, 0.0f, 1600.0f, 0.3f, "Sand"
    );
    
    materials[static_cast<size_t>(MaterialID::Soil)] = MaterialProperties(
        glm::vec3(0.3f, 0.2f, 0.1f),  // Dark brown
        0.95f, 0.0f, 0.0f, 1300.0f, 0.4f, "Soil"
    );
    
    materials[static_cast<size_t>(MaterialID::Grass)] = MaterialProperties(
        glm::vec3(0.2f, 0.5f, 0.2f),  // Grass green
        0.85f, 0.0f, 0.0f, 1100.0f, 0.2f, "Grass"
    );
    
    materials[static_cast<size_t>(MaterialID::Snow)] = MaterialProperties(
        glm::vec3(0.95f, 0.95f, 1.0f),  // Almost white with slight blue
        0.3f, 0.0f, 0.0f, 500.0f, 0.5f, "Snow"
    );
    
    materials[static_cast<size_t>(MaterialID::Ice)] = MaterialProperties(
        glm::vec3(0.8f, 0.9f, 1.0f),  // Ice blue
        0.1f, 0.0f, 0.0f, 920.0f, 0.8f, "Ice"
    );
    
    // Geological varieties
    materials[static_cast<size_t>(MaterialID::Granite)] = MaterialProperties(
        glm::vec3(0.6f, 0.6f, 0.6f),  // Light gray
        0.7f, 0.0f, 0.0f, 2750.0f, 0.95f, "Granite"
    );
    
    materials[static_cast<size_t>(MaterialID::Basalt)] = MaterialProperties(
        glm::vec3(0.2f, 0.2f, 0.2f),  // Dark gray
        0.85f, 0.0f, 0.0f, 2900.0f, 0.93f, "Basalt"
    );
    
    materials[static_cast<size_t>(MaterialID::Clay)] = MaterialProperties(
        glm::vec3(0.6f, 0.4f, 0.3f),  // Reddish brown
        0.9f, 0.0f, 0.0f, 1800.0f, 0.6f, "Clay"
    );
    
    // Special materials
    materials[static_cast<size_t>(MaterialID::Lava)] = MaterialProperties(
        glm::vec3(1.0f, 0.3f, 0.0f),  // Bright orange-red
        0.7f, 0.0f, 3.0f,  // Emissive!
        2800.0f, 0.0f, "Lava"
    );
    
    materials[static_cast<size_t>(MaterialID::Metal)] = MaterialProperties(
        glm::vec3(0.5f, 0.5f, 0.5f),  // Metallic gray
        0.3f, 1.0f, 0.0f,  // Metallic!
        7850.0f, 0.98f, "Metal"
    );
    
    materials[static_cast<size_t>(MaterialID::Crystal)] = MaterialProperties(
        glm::vec3(0.7f, 0.8f, 1.0f),  // Light blue crystalline
        0.1f, 0.0f, 0.1f,  // Slightly emissive
        2650.0f, 0.85f, "Crystal"
    );
    
    // Reserved - make it obvious if accidentally used
    materials[static_cast<size_t>(MaterialID::Reserved)] = MaterialProperties(
        glm::vec3(1.0f, 0.0f, 1.0f),  // Magenta - debug color
        0.5f, 0.0f, 0.0f, 1000.0f, 0.5f, "Reserved/Debug"
    );
}

std::array<MaterialTable::GPUMaterialData, 16> MaterialTable::getGPUData() const {
    std::array<GPUMaterialData, 16> gpuData;
    
    for (size_t i = 0; i < 16; ++i) {
        const auto& mat = materials[i];
        
        // Pack color and roughness
        gpuData[i].colorAndRoughness = glm::vec4(
            mat.color.r,
            mat.color.g,
            mat.color.b,
            mat.roughness
        );
        
        // Pack other properties
        // Note: density is divided by 1000 to fit in reasonable range
        gpuData[i].properties = glm::vec4(
            mat.metallic,
            mat.emissive,
            mat.density / 1000.0f,  // Convert to g/cm³ for smaller numbers
            mat.hardness
        );
    }
    
    return gpuData;
}

} // namespace core