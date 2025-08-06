#version 450

// Per-vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// Per-instance attributes
layout(location = 2) in vec3 inInstanceCenter;
layout(location = 3) in float inInstanceHalfSize;
layout(location = 4) in vec3 inInstanceColor;

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
    // Transform vertex by instance data
    vec3 scaledPos = inPosition * inInstanceHalfSize * 2.0;
    vec3 worldPos = scaledPos + inInstanceCenter;
    
    // Transform to clip space
    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
    
    // Pass data to fragment shader
    fragColor = inInstanceColor; // Use actual instance color!
    fragNormal = normalize(inNormal);
    fragWorldPos = worldPos;
    fragMaterialType = 1u; // We'll restore material type later
}