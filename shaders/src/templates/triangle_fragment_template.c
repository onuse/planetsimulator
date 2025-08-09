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

void main() {
    // Simple shading to see 3D structure
    vec3 lightDir = normalize(vec3(0.5, -0.7, -0.5));
    float lighting = max(dot(normalize(fragNormal), -lightDir), 0.0);
    
    // Use vertex color with simple lighting
    vec3 finalColor = fragColor * (0.3 + 0.7 * lighting);
    
    outColor = vec4(finalColor, 1.0);
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