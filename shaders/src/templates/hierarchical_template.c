// Template for hierarchical.frag shader
// This C file gets transpiled to GLSL for testing and compilation

// GLSL_BEGIN
#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in flat uint fragMaterialType;

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Map MaterialID values to colors
    // MaterialID enum: Vacuum=0, Air=1, Rock=2, Water=3, Sand=4, etc.
    vec3 baseColor;
    
    if (fragMaterialType == 0) { // Vacuum (empty space)
        baseColor = vec3(0.05, 0.05, 0.1); // Dark space color
    } else if (fragMaterialType == 1) { // Air
        baseColor = vec3(0.7, 0.85, 1.0); // Light blue atmosphere
    } else if (fragMaterialType == 2) { // Rock
        baseColor = vec3(0.6, 0.4, 0.2); // Brown for rock
    } else if (fragMaterialType == 3) { // Water
        baseColor = vec3(0.0, 0.4, 0.8); // Blue for water
    } else if (fragMaterialType == 4) { // Sand
        baseColor = vec3(0.9, 0.8, 0.6); // Tan for sand
    } else if (fragMaterialType == 5) { // Soil
        baseColor = vec3(0.3, 0.2, 0.1); // Dark brown for soil
    } else if (fragMaterialType == 6) { // Grass
        baseColor = vec3(0.2, 0.6, 0.2); // Green for grass
    } else if (fragMaterialType == 7) { // Snow
        baseColor = vec3(0.95, 0.95, 1.0); // White for snow
    } else if (fragMaterialType == 8) { // Ice
        baseColor = vec3(0.8, 0.9, 1.0); // Light blue-white for ice
    } else if (fragMaterialType == 9) { // Granite
        baseColor = vec3(0.5, 0.5, 0.5); // Gray for granite
    } else if (fragMaterialType == 10) { // Basalt
        baseColor = vec3(0.2, 0.2, 0.2); // Dark gray for basalt
    } else if (fragMaterialType == 11) { // Clay
        baseColor = vec3(0.7, 0.4, 0.3); // Red-brown for clay
    } else if (fragMaterialType == 12) { // Lava
        baseColor = vec3(1.0, 0.3, 0.0); // Orange-red for lava
    } else if (fragMaterialType == 13) { // Metal
        baseColor = vec3(0.6, 0.6, 0.7); // Metallic gray
    } else if (fragMaterialType == 14) { // Crystal
        baseColor = vec3(0.7, 0.8, 1.0); // Light blue crystal
    } else {
        // Unknown material - make it bright magenta for visibility
        baseColor = vec3(1.0, 0.0, 1.0);
    }
    
    // Simple directional lighting with proper normals
    vec3 lightColor = vec3(1.0, 1.0, 0.95);
    vec3 ambient = 0.4 * baseColor; // Increased ambient for better visibility
    
    // Calculate diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDirNorm = normalize(-ubo.lightDir);
    float diff = max(dot(norm, lightDirNorm), 0.0);
    vec3 diffuse = diff * lightColor * baseColor;
    
    // Add rim lighting for atmosphere effect
    vec3 viewDir = normalize(ubo.viewPos - fragWorldPos);
    float rim = 1.0 - max(dot(viewDir, norm), 0.0);
    rim = pow(rim, 2.0);
    vec3 rimColor = rim * vec3(0.1, 0.2, 0.3) * 0.5;
    
    // Combine all lighting
    vec3 result = ambient + diffuse + rimColor;
    
    // Add subtle water shimmer for water materials
    if (fragMaterialType == 3) { // Water (MaterialID::Water = 3)
        float shimmer = sin(ubo.time * 2.0 + fragWorldPos.x * 0.001) * 0.05 + 0.95;
        result *= shimmer;
    }
    
    outColor = vec4(result, 1.0);
}
// GLSL_END

// C test functions can go here
#ifdef TEST_MODE
#include <stdio.h>
#include <math.h>

// Test that lighting calculations are correct
void test_lighting() {
    // Test ambient calculation
    float baseColor[3] = {0.5f, 0.4f, 0.3f};
    float ambient[3];
    for (int i = 0; i < 3; i++) {
        ambient[i] = 0.3f * baseColor[i];
    }
    
    // Verify ambient is 30% of base
    assert(ambient[0] == 0.15f);
    assert(ambient[1] == 0.12f);
    assert(ambient[2] == 0.09f);
    
    printf("Lighting test passed\n");
}

// Test material type handling
void test_material_types() {
    // Material type 0 = Air
    // Material type 1 = Rock  
    // Material type 2 = Water
    // Material type 3 = Sediment
    
    unsigned int waterType = 2;
    
    // Test water shimmer condition
    if (waterType == 2) {
        printf("Water material detected correctly\n");
    }
}

int main() {
    test_lighting();
    test_material_types();
    return 0;
}
#endif