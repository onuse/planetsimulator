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
    // Use the instance color directly for now to test material colors
    vec3 materialColor = fragColor;
    
    // Simple ambient + directional lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(-ubo.lightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    
    vec3 ambient = vec3(0.4, 0.4, 0.4);
    vec3 diffuse = vec3(0.6, 0.6, 0.6) * diff;
    
    vec3 result = (ambient + diffuse) * materialColor;
    
    // Output with full opacity
    outColor = vec4(result, 1.0);
}