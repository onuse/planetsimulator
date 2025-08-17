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
layout(location = 7) flat in uint fragFaceId;

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
    
    // DEBUG MODE: Choose visualization mode
    vec3 color;
    int debugMode = 0; // 0=terrain, 1=face colors, 2=LOD patches
    
    if (debugMode == 1) {
        // Mode 1: Each FACE gets a distinct color (6 colors total)
        switch(fragFaceId) {
            case 0u: color = vec3(1.0, 0.0, 0.0); break; // +X = RED
            case 1u: color = vec3(0.5, 0.0, 0.0); break; // -X = DARK RED
            case 2u: color = vec3(0.0, 1.0, 0.0); break; // +Y = GREEN
            case 3u: color = vec3(0.0, 0.5, 0.0); break; // -Y = DARK GREEN
            case 4u: color = vec3(0.0, 0.0, 1.0); break; // +Z = BLUE
            case 5u: color = vec3(0.0, 0.0, 0.5); break; // -Z = DARK BLUE
            default: color = vec3(1.0, 1.0, 0.0); break; // YELLOW = ERROR
        }
        color = color * (0.5 + 0.5 * diffuse);
    } else if (debugMode == 2) {
        // Mode 2: Color each PATCH differently to visualize LOD subdivision
        // Create a pseudo-random color based on world position
        float hash1 = fract(sin(dot(fragWorldPos.xy, vec2(12.9898, 78.233))) * 43758.5453);
        float hash2 = fract(sin(dot(fragWorldPos.yz, vec2(93.989, 67.345))) * 24734.3421);
        float hash3 = fract(sin(dot(fragWorldPos.xz, vec2(43.332, 93.532))) * 56472.3785);
        
        // Create vibrant colors that are easy to distinguish
        color = vec3(
            0.3 + 0.7 * hash1,  // R: 0.3-1.0
            0.3 + 0.7 * hash2,  // G: 0.3-1.0  
            0.3 + 0.7 * hash3   // B: 0.3-1.0
        );
        
        // Apply lighting to see 3D shape
        color = color * (0.6 + 0.4 * diffuse);
    }
    
    if (debugMode > 0) {
        // Add grid lines at patch boundaries for both debug modes
        float gridLine = 0.0;
        if (fragTexCoord.x < 0.02 || fragTexCoord.x > 0.98 ||
            fragTexCoord.y < 0.02 || fragTexCoord.y > 0.98) {
            gridLine = 0.3;
        }
        color = mix(color, vec3(1.0), gridLine);
    } else {
        // Original altitude-based coloring
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
    }
    
    // Add rim lighting (atmospheric glow)
    color += vec3(0.5, 0.6, 0.8) * rim;
    
    // Ensure colors are in valid range
    color = clamp(color, 0.0, 1.0);
    
    outColor = vec4(color, 1.0);
}
// GLSL_END