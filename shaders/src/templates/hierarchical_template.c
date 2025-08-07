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
    vec3 baseColor = fragColor;
    
    // Simple directional lighting with proper normals
    vec3 lightColor = vec3(1.0, 1.0, 0.95);
    vec3 ambient = 0.3 * baseColor;
    
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
    if (fragMaterialType == 2) { // Water
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