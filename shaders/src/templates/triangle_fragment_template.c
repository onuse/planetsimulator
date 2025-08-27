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
layout(location = 3) in float fragAltitude;
layout(location = 4) in vec3 fragViewDir;

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

// Output color
layout(location = 0) out vec4 outColor;

// Altitude-based coloring thresholds (scaled for 1000m radius test planet)
const float OCEAN_DEPTH = -40.0;
const float SEA_LEVEL = 0.0;
const float BEACH_HEIGHT = 5.0;
const float GRASS_HEIGHT = 20.0;
const float ROCK_HEIGHT = 35.0;
const float SNOW_HEIGHT = 50.0;

// Material colors
const vec3 DEEP_OCEAN = vec3(0.02, 0.15, 0.35);
const vec3 SHALLOW_OCEAN = vec3(0.05, 0.25, 0.45);
const vec3 BEACH_SAND = vec3(0.9, 0.85, 0.65);
const vec3 GRASS_GREEN = vec3(0.2, 0.5, 0.15);
const vec3 FOREST_GREEN = vec3(0.1, 0.35, 0.08);
const vec3 ROCK_BROWN = vec3(0.4, 0.3, 0.2);
const vec3 MOUNTAIN_GRAY = vec3(0.5, 0.45, 0.4);
const vec3 SNOW_WHITE = vec3(0.95, 0.95, 0.98);

vec3 getTerrainColor(float altitude) {
    vec3 color;
    
    if (altitude < OCEAN_DEPTH) {
        color = DEEP_OCEAN;
    } else if (altitude < SEA_LEVEL) {
        // Interpolate ocean depth
        float t = (altitude - OCEAN_DEPTH) / (SEA_LEVEL - OCEAN_DEPTH);
        color = mix(DEEP_OCEAN, SHALLOW_OCEAN, t);
    } else if (altitude < BEACH_HEIGHT) {
        // Beach transition
        float t = altitude / BEACH_HEIGHT;
        color = mix(SHALLOW_OCEAN, BEACH_SAND, smoothstep(0.0, 1.0, t));
    } else if (altitude < GRASS_HEIGHT) {
        // Grassland/forest
        float t = (altitude - BEACH_HEIGHT) / (GRASS_HEIGHT - BEACH_HEIGHT);
        color = mix(GRASS_GREEN, FOREST_GREEN, t);
    } else if (altitude < ROCK_HEIGHT) {
        // Rocky terrain
        float t = (altitude - GRASS_HEIGHT) / (ROCK_HEIGHT - GRASS_HEIGHT);
        color = mix(FOREST_GREEN, ROCK_BROWN, smoothstep(0.0, 1.0, t));
    } else if (altitude < SNOW_HEIGHT) {
        // Mountain slopes
        float t = (altitude - ROCK_HEIGHT) / (SNOW_HEIGHT - ROCK_HEIGHT);
        color = mix(ROCK_BROWN, MOUNTAIN_GRAY, t);
    } else {
        // Snow caps
        float t = min((altitude - SNOW_HEIGHT) / 1000.0, 1.0);
        color = mix(MOUNTAIN_GRAY, SNOW_WHITE, smoothstep(0.0, 1.0, t));
    }
    
    // Mix in a small amount of vertex color for variation
    color = mix(color, fragColor, 0.1); // 10% vertex color
    
    return color;
}

vec3 atmosphericScattering(vec3 color, float distance) {
    // Simple atmospheric scattering
    const vec3 atmosphereColor = vec3(0.5, 0.7, 1.0);
    const float atmosphereDensity = 0.0000002; // Reduced 10x for clearer colors at 1000km scale
    
    float scatterAmount = 1.0 - exp(-distance * atmosphereDensity);
    scatterAmount = pow(scatterAmount, 1.5); // Adjust falloff
    
    return mix(color, atmosphereColor, scatterAmount * 0.4);
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(fragViewDir);
    
    // Primary light source (sun)
    vec3 sunDir = normalize(vec3(0.5, 0.8, 0.3));
    vec3 sunColor = vec3(1.0, 0.95, 0.8);
    
    // Diffuse lighting
    float NdotL = max(dot(normal, sunDir), 0.0);
    vec3 diffuse = sunColor * NdotL;
    
    // Specular lighting for water
    vec3 specular = vec3(0.0);
    if (fragAltitude < SEA_LEVEL) {
        vec3 halfDir = normalize(sunDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), 32.0);
        specular = sunColor * spec * 0.5;
    }
    
    // Ambient lighting with sky color
    vec3 skyColor = vec3(0.4, 0.6, 0.9);
    vec3 groundColor = vec3(0.2, 0.15, 0.1);
    float skyFactor = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(groundColor, skyColor, skyFactor) * 0.3;
    
    // Use vertex color directly (from voxel materials) instead of altitude-based coloring
    vec3 terrainColor = fragColor; // getTerrainColor(fragAltitude);
    
    // Combine lighting
    vec3 color = terrainColor * (ambient + diffuse * 0.8) + specular;
    
    // Rim lighting for atmosphere effect
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, 2.0);
    color += skyColor * rim * 0.05; // Reduced rim lighting
    
    // Apply atmospheric scattering
    float distance = length(fragWorldPos - ubo.viewPos);
    color = atmosphericScattering(color, distance);
    
    // Tone mapping and gamma correction
    color = color / (color + vec3(1.0)); // Reinhard tone mapping
    color = pow(color, vec3(1.0/2.2));   // Gamma correction
    
    outColor = vec4(color, 1.0);
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