// ULTRA SIMPLE vertex shader template for debugging
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

// Helper function
vec4 mat4_mul_vec4(const mat4* m, vec4 v) {
    vec4 result;
    result.x = m->m[0][0] * v.x + m->m[1][0] * v.y + m->m[2][0] * v.z + m->m[3][0] * v.w;
    result.y = m->m[0][1] * v.x + m->m[1][1] * v.y + m->m[2][1] * v.z + m->m[3][1] * v.w;
    result.z = m->m[0][2] * v.x + m->m[1][2] * v.y + m->m[2][2] * v.z + m->m[3][2] * v.w;
    result.w = m->m[0][3] * v.x + m->m[1][3] * v.y + m->m[2][3] * v.z + m->m[3][3] * v.w;
    return result;
}

// SIMPLIFIED vertex shader main function
void vertex_main(
    // Inputs
    vec3 inPosition,
    vec3 inColor,
    vec3 inNormal,
    vec2 inTexCoord,
    // Uniforms
    struct UniformBufferObject* ubo,
    // Outputs
    vec4* gl_Position,
    vec3* fragColor,
    vec3* fragNormal,
    vec3* fragWorldPos,
    float* fragAltitude,
    vec3* fragViewDir)
{
    // SIMPLE: Just transform position and pass color through
    vec4 pos;
    pos.x = inPosition.x;
    pos.y = inPosition.y;
    pos.z = inPosition.z;
    pos.w = 1.0f;
    
    // Apply view-projection matrix
    *gl_Position = mat4_mul_vec4(&ubo->viewProj, pos);
    
    // Pass color directly through
    *fragColor = inColor;
    
    // Dummy values for other outputs (not used in simple fragment shader)
    *fragNormal = inNormal;
    *fragWorldPos = inPosition;
    *fragAltitude = 0.0f;
    *fragViewDir = (vec3){0.0f, 0.0f, 1.0f};
}