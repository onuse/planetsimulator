#version 450
#extension GL_ARB_separate_shader_objects : enable

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in flat uint fragMaterialType;

// Uniforms
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Output
layout(location = 0) out vec4 outColor;

// Material types (matching C++ enum)
const uint MATERIAL_AIR = 0;
const uint MATERIAL_WATER = 1;
const uint MATERIAL_ROCK = 2;
const uint MATERIAL_MAGMA = 3;
const uint MATERIAL_ICE = 4;
const uint MATERIAL_SAND = 5;

vec3 getMaterialColor(uint materialType) {
    switch(materialType) {
        case MATERIAL_AIR:   return vec3(0.7, 0.85, 1.0);  // Light blue
        case MATERIAL_WATER: return vec3(0.0, 0.3, 0.7);   // Ocean blue
        case MATERIAL_ROCK:  return vec3(0.5, 0.4, 0.3);   // Brown/gray
        case MATERIAL_MAGMA: return vec3(1.0, 0.3, 0.0);   // Orange/red
        case MATERIAL_ICE:   return vec3(0.9, 0.95, 1.0);  // White/blue
        case MATERIAL_SAND:  return vec3(0.9, 0.8, 0.6);   // Sandy yellow
        default:             return vec3(1.0, 0.0, 1.0);   // Magenta for unknown
    }
}

void main() {
    // Get base color from material type or instance color
    vec3 baseColor = fragColor;
    if (length(baseColor) < 0.01) {
        // If no color specified, use material type
        baseColor = getMaterialColor(fragMaterialType);
    }
    
    // Simple lighting
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-ubo.lightDir);
    
    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;
    
    // Ambient
    vec3 ambient = 0.3 * baseColor;
    
    // Distance fog for atmosphere effect
    float distance = length(ubo.viewPos - fragWorldPos);
    float fogStart = 100000.0;  // 100km
    float fogEnd = 10000000.0;   // 10,000km
    float fogFactor = clamp((distance - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 fogColor = vec3(0.7, 0.85, 1.0);
    
    // Combine lighting
    vec3 result = ambient + diffuse;
    
    // Apply fog
    result = mix(result, fogColor, fogFactor);
    
    // Edge highlight for better visibility
    vec3 viewDir = normalize(ubo.viewPos - fragWorldPos);
    float edge = 1.0 - abs(dot(viewDir, normal));
    edge = pow(edge, 2.0) * 0.2;
    result += vec3(edge);
    
    outColor = vec4(result, 1.0);
}