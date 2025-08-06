#version 450
#extension GL_ARB_separate_shader_objects : enable

// Vertex attributes (cube vertices)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// Instance attributes
layout(location = 2) in vec3 instanceCenter;
layout(location = 3) in float instanceHalfSize;
layout(location = 4) in vec3 instanceColor;
layout(location = 5) in uint instanceMaterialType;

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

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out flat uint fragMaterialType;

void main() {
    // Transform cube vertex by instance transform
    vec3 worldPos = instanceCenter + inPosition * instanceHalfSize;
    fragWorldPos = worldPos;
    
    // Pass through normal (no rotation for axis-aligned cubes)
    fragNormal = inNormal;
    
    // Pass through instance data
    fragColor = instanceColor;
    fragMaterialType = instanceMaterialType;
    
    // Transform to clip space
    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}