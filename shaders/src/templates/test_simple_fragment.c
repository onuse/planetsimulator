// ULTRA SIMPLE fragment shader template for debugging
// This gets transpiled to GLSL

#include <math.h>

// GLSL built-in types (defined for C testing)
#ifndef GLSL_TYPES
#define GLSL_TYPES
typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float m[4][4]; } mat4;
#endif

// Uniform buffer structure (must match vertex shader)
struct UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
};

// SIMPLIFIED fragment shader main function
void fragment_main(
    // Inputs from vertex shader
    vec3 fragColor,
    vec3 fragNormal,
    vec3 fragWorldPos,
    float fragAltitude,
    vec3 fragViewDir,
    // Uniforms
    struct UniformBufferObject* ubo,
    // Output
    vec4* outColor)
{
    // ULTRA SIMPLE: Just output the vertex color directly
    // No lighting, no atmospheric effects, no tone mapping
    outColor->x = fragColor.x;
    outColor->y = fragColor.y;
    outColor->z = fragColor.z;
    outColor->w = 1.0f;
    
    // Alternative test patterns (uncomment one to test):
    
    // 1. Bright red (should be clearly visible)
    // outColor->x = 1.0f;
    // outColor->y = 0.0f;
    // outColor->z = 0.0f;
    // outColor->w = 1.0f;
    
    // 2. Gradient based on screen position (if geometry is rendering)
    // outColor->x = fragWorldPos.x * 0.0001f + 0.5f;
    // outColor->y = fragWorldPos.y * 0.0001f + 0.5f;
    // outColor->z = 0.5f;
    // outColor->w = 1.0f;
    
    // 3. Normal visualization (helps debug geometry)
    // outColor->x = fragNormal.x * 0.5f + 0.5f;
    // outColor->y = fragNormal.y * 0.5f + 0.5f;
    // outColor->z = fragNormal.z * 0.5f + 0.5f;
    // outColor->w = 1.0f;
}