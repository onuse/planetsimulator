// Triangle fragment shader template
// This file is transpiled to GLSL for the Transvoxel mesh rendering

#include <math.h>

// GLSL type definitions for C compatibility
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { 
    float m[16]; // mat4 stored as array
} mat4;

// Helper functions
static inline vec3 vec3_create(float x, float y, float z) {
    vec3 v = {x, y, z};
    return v;
}

static inline vec4 vec4_create(float x, float y, float z, float w) {
    vec4 v = {x, y, z, w};
    return v;
}

static inline float dot3(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float length3(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline vec3 normalize3(vec3 v) {
    float len = length3(v);
    if (len > 0.0f) {
        v.x /= len;
        v.y /= len;
        v.z /= len;
    }
    return v;
}

static inline vec3 vec3_scale(vec3 v, float s) {
    v.x *= s;
    v.y *= s;
    v.z *= s;
    return v;
}

static inline vec3 vec3_add(vec3 a, vec3 b) {
    vec3 v = {a.x + b.x, a.y + b.y, a.z + b.z};
    return v;
}

static inline vec3 vec3_sub(vec3 a, vec3 b) {
    vec3 v = {a.x - b.x, a.y - b.y, a.z - b.z};
    return v;
}

static inline vec3 vec3_neg(vec3 v) {
    vec3 r = {-v.x, -v.y, -v.z};
    return r;
}

static inline vec3 vec3_mix(vec3 a, vec3 b, float t) {
    vec3 v;
    v.x = a.x * (1.0f - t) + b.x * t;
    v.y = a.y * (1.0f - t) + b.y * t;
    v.z = a.z * (1.0f - t) + b.z * t;
    return v;
}

static inline float max_float(float a, float b) {
    return a > b ? a : b;
}

// Uniform buffer structure
struct UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
};

// Main fragment shader function (C version for testing)
vec4 fragment_main(
    vec3 fragColor,
    vec3 fragNormal, 
    vec3 fragWorldPos,
    const struct UniformBufferObject* ubo)
{
    // Simple shading
    vec3 lightDir = normalize3(vec3_create(0.5f, -0.7f, -0.5f));
    float lighting = max_float(dot3(normalize3(fragNormal), vec3_neg(lightDir)), 0.0f);
    
    // Use vertex color with simple lighting
    vec3 finalColor = vec3_scale(fragColor, 0.3f + 0.7f * lighting);
    
    return vec4_create(finalColor.x, finalColor.y, finalColor.z, 1.0f);
}

// GLSL fragment shader code
// This section is extracted by the transpiler
// GLSL_BEGIN
#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragEyeDistance;
layout(location = 4) in vec3 fragViewDir;

// Uniform buffer - must match struct UniformBufferObject in vulkan_renderer.hpp
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
    vec4 planetParams;   // radius, sea level, highest land, atmosphere scale height
} ubo;

layout(location = 0) out vec4 outColor;

// Rayleigh scattering goes as the inverse fourth power of wavelength, which is
// why the sky is blue and why a distant mountain is bluer than a near one.
// These are that ratio for red, green and blue, normalised so green is one.
const vec3 RAYLEIGH = vec3(0.36, 1.00, 2.62);

const vec3 SUN_COLOUR = vec3(1.00, 0.97, 0.92);

void main() {
    const float planetRadius = ubo.planetParams.x;
    const float seaLevel = ubo.planetParams.y;
    const float scaleHeight = ubo.planetParams.w;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(fragViewDir);

    // Up is away from the planet's centre, not along any world axis. Every
    // term below that has an up in it means this one.
    vec3 up = normalize(fragWorldPos);
    float altitude = length(fragWorldPos) - (planetRadius + seaLevel);

    // ubo.lightDir points the way the light travels, so the direction towards
    // the sun is its negation.
    vec3 sunDir = normalize(-ubo.lightDir);

    // Water is not a material flag: the ocean is built as a flat surface at
    // sea level, so anything at that height is water and anything above it is
    // ground. The transition is over a couple of metres, which puts a soft
    // edge on the shoreline instead of a hard one.
    float water = 1.0 - smoothstep(0.0, 2.0, altitude);

    // Sunlight, with a terminator softened over the angle the sun actually
    // subtends. A hard cutoff at ninety degrees reads as a drawn line around
    // the planet rather than as a curved body turning away from its star.
    float NdotL = dot(normal, sunDir);
    float sunlight = smoothstep(-0.05, 0.15, NdotL);

    // Sky light. The ground is lit from the whole dome, not from a point, so
    // surfaces facing up pick up blue and those facing down pick up bounce
    // from the terrain. Without this the unlit side is flat black and the
    // shadowed slopes lose all their shape.
    float dome = dot(normal, up) * 0.5 + 0.5;
    vec3 skyLight = mix(vec3(0.12, 0.11, 0.10), vec3(0.22, 0.32, 0.48), dome);

    // How much of the sky is actually lit here - the ambient has to go out
    // with the sun, or the night side glows.
    float dayness = smoothstep(-0.25, 0.25, dot(up, sunDir));
    skyLight *= 0.25 + 0.75 * dayness;

    // Into linear light before anything is done to it.
    //
    // The colours the surface is built with are the ones intended to appear on
    // screen, which is a gamma-encoded space. Multiplying those by a light
    // level and then encoding for display a second time brightens everything
    // and washes the colour out of it - green hills come back nearly white.
    // Lighting is a physical sum and only means anything in linear light.
    vec3 albedo = pow(fragColor, vec3(2.2));

    // Land.
    vec3 lit = albedo * (SUN_COLOUR * sunlight + skyLight);

    // Water, over the top of it.
    //
    // Two things make water read as water rather than as blue ground: it gets
    // brighter as you look along it, and the sun leaves a glint. The first is
    // Fresnel - reflectance climbs to almost one at grazing angles, which is
    // why a lake is a mirror from the shore and transparent from above.
    float fresnel = 0.02 + 0.98 * pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);

    // The lobe has to be very tight. Open ocean is smooth at this scale and
    // the surface curves away slowly, so a broad exponent spreads the
    // highlight over hundreds of kilometres and burns out as a white disc
    // rather than reading as the sun on water.
    vec3 halfway = normalize(sunDir + viewDir);
    float glint = pow(max(dot(normal, halfway), 0.0), 6000.0);

    vec3 skyReflection = mix(vec3(0.18, 0.28, 0.45), vec3(0.35, 0.50, 0.75), dayness);
    vec3 waterLit = albedo * (SUN_COLOUR * sunlight * 0.5 + skyLight);
    waterLit = mix(waterLit, skyReflection * (0.3 + 0.7 * dayness), fresnel * 0.75);
    // Weighted by Fresnel like any other reflection - the sun's image is
    // strongest where the water is behaving most like a mirror.
    waterLit += SUN_COLOUR * glint * sunlight * fresnel * 12.0;

    vec3 colour = mix(lit, waterLit, water);

    // Air between here and the eye.
    //
    // What matters is how much air the ray passed through, which is not the
    // same as how far it travelled. From orbit almost the entire distance is
    // vacuum - using it directly extinguished blue completely and left the
    // planet olive under a wash of haze.
    //
    // Air thins exponentially with height, so a ray leaving the surface at
    // some angle from vertical passes through a column of about 1/cos(angle)
    // times the vertical one, however far it then continues through nothing.
    // Close up that overestimates, because the ray stops before clearing the
    // atmosphere, so the shorter of the two is the one to take.
    float density = exp(-max(altitude, 0.0) / scaleHeight);
    float viewZenith = max(dot(up, viewDir), 0.02);
    float slantColumn = density / viewZenith;
    float travelled = fragEyeDistance * density / scaleHeight;
    float airMass = min(slantColumn, travelled);

    // Optical depth of one vertical column at sea level, for green. The other
    // two follow from the wavelength ratio.
    const float ZENITH_DEPTH = 0.10;
    vec3 transmittance = exp(-RAYLEIGH * airMass * ZENITH_DEPTH);

    // Air scatters sunlight towards the eye, and more of it when looking
    // along the sun's direction than across it.
    float cosSun = dot(viewDir, sunDir);
    float phase = 0.75 * (1.0 + cosSun * cosSun);
    vec3 skyTint = vec3(0.16, 0.36, 0.78);

    colour = colour * transmittance +
             skyTint * SUN_COLOUR * phase * dayness * (1.0 - transmittance);

    // The limb. Looking at the edge of the planet the line of sight grazes the
    // surface and travels through far more air than it does looking straight
    // down, so the edge glows and softens against space.
    float grazing = 1.0 - max(dot(normal, viewDir), 0.0);
    colour += skyTint * pow(grazing, 5.0) * dayness * 0.5;

    // Exposure and tone mapping, so the glint and the lit limb roll off
    // instead of clipping to white.
    colour *= 1.35;
    colour = colour / (colour + vec3(1.0));

    // Left in linear light on purpose. The swap chain is an sRGB format, so
    // the hardware encodes for display on write; doing it here as well was
    // the second of two gamma encodes and is what bleached the terrain.
    outColor = vec4(colour, 1.0);
}
// GLSL_END

// Test function for C compilation
#ifdef TEST_COMPILE
#include <stdio.h>

void test_fragment_shader() {
    // Test inputs
    vec3 fragColor = vec3_create(0.5f, 0.45f, 0.4f);  // Rock color
    vec3 fragNormal = vec3_create(0.0f, 1.0f, 0.0f);  // Up normal
    vec3 fragWorldPos = vec3_create(6371000.0f, 0.0f, 0.0f);  // On planet surface
    
    struct UniformBufferObject ubo;
    ubo.viewPos = vec3_create(10000000.0f, 0.0f, 0.0f);  // Camera 10Mm away
    ubo.lightDir = vec3_create(-0.5f, -1.0f, -0.3f);
    ubo.time = 0.0f;
    
    vec4 color = fragment_main(fragColor, fragNormal, fragWorldPos, &ubo);
    
    printf("Fragment shader test:\n");
    printf("  Input color: (%.2f, %.2f, %.2f)\n", fragColor.x, fragColor.y, fragColor.z);
    printf("  Output color: (%.2f, %.2f, %.2f, %.2f)\n", color.x, color.y, color.z, color.w);
}

int main() {
    test_fragment_shader();
    return 0;
}
#endif