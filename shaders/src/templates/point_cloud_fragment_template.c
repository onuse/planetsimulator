// Point cloud fragment shader template
// Simple fragment shader for point cloud rendering

// GLSL_BEGIN
#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in float fragMaterial;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Output the color with full opacity
    outColor = vec4(fragColor, 1.0);
}
// GLSL_END