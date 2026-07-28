// Triangle vertex shader template - Transvoxel / adaptive sphere mesh rendering
// This file is transpiled to GLSL (shaders/src/vertex/triangle.vert) by
// shaders/tools/extract_simple_glsl.py, then compiled to triangle.vert.spv.
//
// Pairs with triangle_fragment_template.c. The vertex layout below must match
// createTrianglePipeline() in src/rendering/vulkan_renderer_transvoxel.cpp:
//     stride 44 = vec3 position + vec3 color + vec3 normal + vec2 texCoord
// and the mesh produced by generateAdaptiveSphere() in
// src/rendering/generate_adaptive_sphere.cpp, which writes those 11 floats
// per vertex with positions in absolute planet-centred world space (metres).

// GLSL_BEGIN
#version 450

// Vertex attributes - CPU-generated mesh (see createTrianglePipeline)
layout(location = 0) in vec3 inPosition;   // absolute world space, metres
layout(location = 1) in vec3 inColor;      // material colour from the octree
layout(location = 2) in vec3 inNormal;     // smooth normal, world space
layout(location = 3) in vec2 inTexCoord;

// Uniforms - must match struct UniformBufferObject in vulkan_renderer.hpp.
// NOTE: view/viewProj are camera-RELATIVE. updateUniformBuffer() strips the
// translation out of the view matrix, so the vertex shader has to subtract
// viewPos from the world position itself. Doing the subtraction here (rather
// than baking huge coordinates into the matrices) is what keeps float
// precision usable at planetary scale.
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Outputs - locations must match triangle_fragment_template.c
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragAltitude;
layout(location = 4) out vec3 fragViewDir;

void main() {
    // Camera-relative position: the only value small enough to survive
    // single-precision transformation at planet scale.
    vec3 relPos = inPosition - ubo.viewPos;

    gl_Position = ubo.viewProj * vec4(relPos, 1.0);

    fragColor = inColor;
    fragNormal = inNormal;
    fragWorldPos = inPosition;

    // Direction from the surface point back towards the eye.
    fragViewDir = normalize(-relPos);

    // The planet radius is not in the UBO, so true altitude cannot be
    // computed here. The active fragment path shades from vertex colour and
    // ignores this; it is passed through only to satisfy the interface.
    fragAltitude = 0.0;
}
// GLSL_END
