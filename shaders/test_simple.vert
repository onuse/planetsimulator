#version 450

// Per-vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

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

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out flat uint fragMaterialType;

void main() {
    // TEST: Just render a single cube at a fixed position with fixed size
    vec3 scaledPos = inPosition * 1000000.0; // 1000km cube
    vec3 worldPos = scaledPos + vec3(0.0, 0.0, 5000000.0); // 5000km from origin
    
    // Transform to clip space
    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
    
    // Pass data to fragment shader
    fragColor = vec3(1.0, 0.0, 0.0); // Red
    fragNormal = normalize(inNormal);
    fragWorldPos = worldPos;
    fragMaterialType = 1; // Rock
}