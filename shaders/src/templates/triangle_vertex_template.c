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

// Uniforms - must match struct UniformBufferObject in vulkan_renderer.hpp.
// NOTE: view/viewProj are camera-RELATIVE. updateUniformBuffer() strips the
// translation out of the view matrix, so the vertex shader works entirely in
// coordinates relative to the eye. Baking planet-scale absolute positions into
// the matrices would lose everything below a few metres.
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Where this patch sits relative to the camera, computed on the CPU in double
// precision. Vertex positions are stored relative to their patch centre, so
// the two together give a camera-relative position without any large number
// ever existing in float: a patch is a few kilometres across, which leaves
// millimetre precision, where absolute planetary coordinates leave metres.
layout(push_constant) uniform PatchConstants {
    vec3 patchOffset;
} patchData;

// Outputs - locations must match triangle_fragment_template.c
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragEyeDistance;
layout(location = 4) out vec3 fragViewDir;

void main() {
    // Patch offset plus the vertex's offset within its patch. Both are small.
    vec3 relPos = patchData.patchOffset + inPosition;

    gl_Position = ubo.viewProj * vec4(relPos, 1.0);

    fragColor = inColor;
    fragNormal = inNormal;
    fragWorldPos = relPos + ubo.viewPos;

    // Direction from the surface point back towards the eye.
    fragViewDir = normalize(-relPos);

    // How far this point is from the eye, in metres.
    //
    // Computed here rather than in the fragment shader, because there it
    // would have to come from subtracting two absolute planetary positions -
    // both of order ten million - and the difference would lose everything
    // below a few metres. Here it is the length of a vector that is already
    // camera-relative and small.
    fragEyeDistance = length(relPos);
}
// GLSL_END
