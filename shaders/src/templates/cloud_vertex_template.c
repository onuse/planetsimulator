// Cloud layer vertex shader.
//
// Transpiled to shaders/src/vertex/cloud.vert by
// shaders/tools/extract_simple_glsl.py, then compiled to cloud.vert.spv.
//
// Takes the surface geometry and lifts it onto a sphere at cloud altitude.
// The patches are already selected, culled and pooled for the ground, so the
// sky costs no geometry of its own - only a second draw of the same buffers
// with a different pipeline.

// GLSL_BEGIN
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in float inCloudCover;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float pixelWorldScale;
    vec4 planetParams;   // radius, sea level, highest land, atmosphere scale height
} ubo;

layout(push_constant) uniform PatchConstants {
    vec3 patchOffset;
} patchData;

layout(location = 0) out float fragCover;
layout(location = 1) out vec3 fragUp;
layout(location = 2) out vec3 fragViewDir;
layout(location = 3) out float fragEyeDistance;

void main() {
    // Where this vertex is, relative to the eye.
    vec3 relPos = patchData.patchOffset + inPosition;

    // Its direction from the planet's centre. The absolute position is of
    // order ten million metres, but only its direction is wanted and that is
    // order one, so normalising immediately keeps the precision.
    vec3 up = normalize(relPos + ubo.viewPos);

    // Onto the shell. Terrain height is discarded outright: cloud sits at a
    // level set by where air cools to its dew point, not by what the ground
    // beneath it happens to be doing. Following the terrain instead would drag
    // the cloud deck up over every mountain, which is the one place cloud
    // visibly does not go.
    float cloudAltitude = ubo.planetParams.x * 0.006;   // ~6 km on Earth's scale
    vec3 cloudWorld = up * (ubo.planetParams.x + ubo.planetParams.y + cloudAltitude);

    vec3 cloudRel = cloudWorld - ubo.viewPos;
    gl_Position = ubo.viewProj * vec4(cloudRel, 1.0);

    fragCover = inCloudCover;
    fragUp = up;
    fragViewDir = normalize(-cloudRel);
    fragEyeDistance = length(cloudRel);
}
// GLSL_END
