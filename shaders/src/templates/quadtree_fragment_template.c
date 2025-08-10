// Quadtree LOD fragment shader template
// This file is transpiled to GLSL for the quadtree patch rendering

// GLSL_BEGIN
#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in float fragMorphFactor;
layout(location = 5) in float fragAltitude;
layout(location = 6) in vec3 fragViewDir;

// Output
layout(location = 0) out vec4 outColor;

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

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(-fragViewDir);
    
    // Light direction (sun)
    vec3 lightDir = normalize(vec3(0.5, 0.8, 0.3));
    
    // Basic diffuse lighting
    float NdotL = max(dot(normal, lightDir), 0.0);
    float diffuse = NdotL * 0.8 + 0.2; // Ambient + diffuse
    
    // Rim lighting for atmosphere effect (reduced intensity)
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = pow(rim, 3.0) * 0.1; // Reduced from 0.5 to 0.1
    
    // Color based on altitude
    vec3 color;
    float altitude = fragAltitude;
    
    if (altitude < -1000.0) {
        // Deep ocean - rich blue
        color = vec3(0.0, 0.2, 0.6);
    } else if (altitude < -100.0) {
        // Shallow ocean - lighter blue
        color = vec3(0.1, 0.4, 0.7);
    } else if (altitude < 0.0) {
        // Very shallow water/coastline
        color = vec3(0.2, 0.5, 0.7);
    } else if (altitude < 50.0) {
        // Beach/sand
        color = vec3(0.8, 0.7, 0.5);
    } else if (altitude < 200.0) {
        // Lowland grass
        color = vec3(0.3, 0.6, 0.2);
    } else if (altitude < 500.0) {
        // Forest/hills
        color = vec3(0.2, 0.5, 0.1);
    } else if (altitude < 1000.0) {
        // Mountains
        color = vec3(0.5, 0.4, 0.3);
    } else if (altitude < 1500.0) {
        // High mountains
        color = vec3(0.6, 0.6, 0.6);
    } else {
        // Snow peaks
        color = vec3(0.9, 0.9, 0.95);
    }
    
    // Apply lighting
    color = color * diffuse;
    
    // Add rim lighting (atmospheric glow)
    color += vec3(0.5, 0.6, 0.8) * rim;
    
    // Ensure colors are in valid range
    color = clamp(color, 0.0, 1.0);
    
    outColor = vec4(color, 1.0);
}
// GLSL_END