// Point cloud vertex shader template
// Renders points from GPU-generated surface voxels

// GLSL_BEGIN
#version 450

// No vertex attributes - use gl_VertexIndex instead

// Point positions from compute shader
layout(binding = 0, std430) readonly buffer PointBuffer {
    vec4 points[];  // xyz = position, w = material ID
} pointBuffer;

// Uniform buffer
layout(binding = 1) uniform UniformBufferObject {
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
layout(location = 1) out float fragMaterial;

void main() {
    // Get point data using built-in vertex index
    vec4 pointData = pointBuffer.points[gl_VertexIndex];
    vec3 worldPos = pointData.xyz;
    float material = pointData.w;
    
    // Transform to clip space
    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
    
    // Set point size based on distance - make them bigger!
    float dist = length(worldPos - ubo.viewPos);
    gl_PointSize = clamp(5000.0 / dist, 2.0, 50.0);  // Much bigger points
    
    // Match C++ MaterialID enum:
    // Vacuum = 0, Air = 1, Rock = 2, Water = 3
    if (material < 0.5) {
        fragColor = vec3(0.0, 0.0, 0.0);  // BLACK = Vacuum (material 0)
    } else if (material < 1.5) {
        fragColor = vec3(0.7, 0.9, 1.0);  // LIGHT BLUE = Air (material 1)
    } else if (material < 2.5) {
        fragColor = vec3(0.5, 0.4, 0.3);  // BROWN = Rock (material 2)
    } else if (material < 3.5) {
        fragColor = vec3(0.0, 0.3, 0.7);  // BLUE = Water (material 3)
    } else {
        fragColor = vec3(1.0, 0.8, 0.5);  // TAN = Sand/other (material 4+)
    }
    
    // Add some variation based on position for visual interest
    fragColor += vec3(sin(worldPos.x * 0.01), cos(worldPos.y * 0.01), sin(worldPos.z * 0.01)) * 0.1;
    
    fragMaterial = material;
}
// GLSL_END