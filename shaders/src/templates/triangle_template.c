// Triangle vertex shader template - testable C code
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

// Helper functions for C testing
vec4 mat4_mul_vec4(const mat4* m, vec4 v) {
    vec4 result;
    result.x = m->m[0][0] * v.x + m->m[1][0] * v.y + m->m[2][0] * v.z + m->m[3][0] * v.w;
    result.y = m->m[0][1] * v.x + m->m[1][1] * v.y + m->m[2][1] * v.z + m->m[3][1] * v.w;
    result.z = m->m[0][2] * v.x + m->m[1][2] * v.y + m->m[2][2] * v.z + m->m[3][2] * v.w;
    result.w = m->m[0][3] * v.x + m->m[1][3] * v.y + m->m[2][3] * v.z + m->m[3][3] * v.w;
    return result;
}

vec3 normalize_vec3(vec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0f) {
        v.x /= len;
        v.y /= len;
        v.z /= len;
    }
    return v;
}

// Vertex shader main function
void vertex_main(
    // Inputs
    vec3 inPosition,
    vec3 inColor,
    vec3 inNormal,
    vec2 inTexCoord,
    const struct UniformBufferObject* ubo,
    // Outputs
    vec4* gl_Position,
    vec3* fragColor,
    vec3* fragNormal,
    vec3* fragWorldPos)
{
    vec3 worldPos = inPosition;
    
    // IMPORTANT: Handle planet-scale coordinates properly
    // The issue is that worldPos is in meters (millions of meters from origin)
    // We need to transform this correctly to clip space
    
    // Method 1: Direct transformation (current approach - causes issues)
    // vec4 pos = {worldPos.x, worldPos.y, worldPos.z, 1.0f};
    // *gl_Position = mat4_mul_vec4(&ubo->viewProj, pos);
    
    // Method 2: Relative to camera transformation (better for large worlds)
    // Transform relative to camera position to avoid precision issues
    vec3 relativePos;
    relativePos.x = worldPos.x - ubo->viewPos.x;
    relativePos.y = worldPos.y - ubo->viewPos.y;
    relativePos.z = worldPos.z - ubo->viewPos.z;
    
    // Now transform the relative position
    vec4 pos = {relativePos.x, relativePos.y, relativePos.z, 1.0f};
    *gl_Position = mat4_mul_vec4(&ubo->viewProj, pos);
    
    // Pass data to fragment shader
    *fragColor = inColor;
    *fragNormal = normalize_vec3(inNormal);
    *fragWorldPos = worldPos;
}

// Test function for debugging
#ifdef TEST_MODE
#include <stdio.h>

void test_vertex_transform() {
    // Test with planet-scale coordinates
    vec3 inPosition = {2356650.0f, -1618290.0f, 5693590.0f}; // From mesh dump
    vec3 inColor = {0.5f, 0.5f, 0.5f};
    vec3 inNormal = {0.577f, -0.577f, 0.577f};
    vec2 inTexCoord = {0.0f, 0.0f};
    
    // Set up a test view-projection matrix
    struct UniformBufferObject ubo = {0};
    // Camera at similar distance from planet
    ubo.viewPos.x = 7135520.0f;
    ubo.viewPos.y = 3058080.0f;
    ubo.viewPos.z = 6116160.0f;
    
    // Simple identity for testing
    for (int i = 0; i < 4; i++) {
        ubo.viewProj.m[i][i] = 1.0f;
    }
    
    vec4 gl_Position;
    vec3 fragColor, fragNormal, fragWorldPos;
    
    vertex_main(inPosition, inColor, inNormal, inTexCoord, &ubo,
                &gl_Position, &fragColor, &fragNormal, &fragWorldPos);
    
    printf("Input position: (%.0f, %.0f, %.0f)\n", 
           inPosition.x, inPosition.y, inPosition.z);
    printf("Camera position: (%.0f, %.0f, %.0f)\n",
           ubo.viewPos.x, ubo.viewPos.y, ubo.viewPos.z);
    printf("Relative position: (%.0f, %.0f, %.0f)\n",
           inPosition.x - ubo.viewPos.x,
           inPosition.y - ubo.viewPos.y,
           inPosition.z - ubo.viewPos.z);
    printf("Output clip position: (%.3f, %.3f, %.3f, %.3f)\n",
           gl_Position.x, gl_Position.y, gl_Position.z, gl_Position.w);
    
    // Check if in NDC range after perspective divide
    float ndcX = gl_Position.x / gl_Position.w;
    float ndcY = gl_Position.y / gl_Position.w;
    float ndcZ = gl_Position.z / gl_Position.w;
    printf("NDC position: (%.3f, %.3f, %.3f)\n", ndcX, ndcY, ndcZ);
    
    if (fabs(ndcX) > 1.0f || fabs(ndcY) > 1.0f || ndcZ < -1.0f || ndcZ > 1.0f) {
        printf("WARNING: Vertex outside NDC range!\n");
    }
}

int main() {
    printf("Testing triangle vertex shader transformations:\n");
    printf("================================================\n");
    test_vertex_transform();
    return 0;
}
#endif