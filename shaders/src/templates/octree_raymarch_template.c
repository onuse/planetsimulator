/**
 * Octree Ray Marching Shader - C Template
 * 
 * This file is written in C-like syntax for testing and verification.
 * It can be compiled as C for testing, then converted to GLSL.
 * 
 * Conversion rules:
 * - vec3/vec4 become GLSL types
 * - dot(), length(), normalize() are already GLSL-compatible
 * - Remove all printf/assert statements
 * - Replace 0u with 0u (keep as-is)
 * - Replace true/false with GLSL bools
 */

// GLSL_HEADER_START
#ifdef GLSL
#version 450

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float planetRadius;
    int debugMode;
} pc;

struct OctreeNode {
    vec4 centerAndSize;
    uvec4 childrenAndFlags;
};

layout(std430, binding = 1) readonly buffer NodeBuffer {
    OctreeNode nodes[];
} nodeBuffer;

// Material table - 16 materials with color and properties
struct Material {
    vec4 color;  // RGB color + alpha/reserved
    vec4 properties;  // density, state, reserved, reserved
};

layout(std430, binding = 2) readonly buffer MaterialTable {
    Material materials[16];
} materialTable;
#endif
// GLSL_HEADER_END

// C_HEADER_START
#ifndef GLSL
#include <math.h>
#include <stdbool.h>
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float x, y; } vec2;
typedef struct { unsigned int x, y, z, w; } uvec4;
typedef unsigned int uint;

// Mock GLSL functions for C
float dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float length(vec3 v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
vec3 normalize(vec3 v) { 
    float len = length(v);
    vec3 result = {v.x/len, v.y/len, v.z/len};
    return result;
}
float max(float a, float b) { return a > b ? a : b; }
// sqrt is already defined in math.h

struct OctreeNode {
    vec4 centerAndSize;
    uvec4 childrenAndFlags;
};

// Material structure for C testing
struct Material {
    vec4 color;
    vec4 properties;
};

// Test data for C compilation
struct TestGlobals {
    struct { vec2 resolution; float planetRadius; int debugMode; } pc;
    struct { vec3 viewPos; } ubo;
    struct { struct OctreeNode nodes[1000]; } nodeBuffer;
    struct { struct Material materials[16]; } materialTable;
} test_globals;
#define pc test_globals.pc
#define ubo test_globals.ubo
#define nodeBuffer test_globals.nodeBuffer
#define materialTable test_globals.materialTable
#endif
// C_HEADER_END

// Shared implementation that works in both C and GLSL
vec2 raySphere(vec3 origin, vec3 dir, vec3 center, float radius) {
    vec3 oc;
    oc.x = origin.x - center.x;
    oc.y = origin.y - center.y;
    oc.z = origin.z - center.z;
    
    float b = dot(oc, dir);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    
    vec2 result;
    if (h < 0.0) {
        result.x = -1.0;
        result.y = -1.0;
    } else {
        h = sqrt(h);
        result.x = -b - h;
        result.y = -b + h;
    }
    return result;
}

vec4 traverseOctree(vec3 rayOrigin, vec3 rayDir) {
    vec4 blackSpace;
    blackSpace.x = 0.0; blackSpace.y = 0.0; 
    blackSpace.z = 0.0; blackSpace.w = 1.0;
    
    // Debug mode 1: Red sphere test
    if (pc.debugMode == 1) {
        vec3 center; 
        center.x = 0.0; center.y = 0.0; center.z = 0.0;
        vec2 hit = raySphere(rayOrigin, rayDir, center, pc.planetRadius);
        if (hit.x > 0.0) {
            vec4 red;
            red.x = 1.0; red.y = 0.0; red.z = 0.0; red.w = 1.0;
            return red;
        }
        return blackSpace;
    }
    
    // Check planet intersection
    vec3 planetCenter;
    planetCenter.x = 0.0; planetCenter.y = 0.0; planetCenter.z = 0.0;
    vec2 planetHit = raySphere(rayOrigin, rayDir, planetCenter, pc.planetRadius);
    
    if (planetHit.x < 0.0) {
        return blackSpace;
    }
    
    // Calculate ray start position
    float startDist = max(planetHit.x, 0.0);
    vec3 rayStart;
    rayStart.x = rayOrigin.x + rayDir.x * startDist;
    rayStart.y = rayOrigin.y + rayDir.y * startDist;
    rayStart.z = rayOrigin.z + rayDir.z * startDist;
    
    // Traversal constants
    const int MAX_STEPS = 400;
    const float MIN_STEP = 10.0;  // Reduced from 100.0 to avoid gaps
    float MAX_DISTANCE = pc.planetRadius * 2.0;
    
    float t = 0.0;
    vec3 currentPos = rayStart;
    
    // Main traversal loop
    for (int step = 0; step < MAX_STEPS; step++) {
        // Update position
        currentPos.x = rayStart.x + rayDir.x * t;
        currentPos.y = rayStart.y + rayDir.y * t;
        currentPos.z = rayStart.z + rayDir.z * t;
        
        // Check if we've left the planet
        float distFromCenter = length(currentPos);
        if (distFromCenter > pc.planetRadius || t > MAX_DISTANCE) {
            break;
        }
        
        // Traverse octree from root
        uint nodeIndex = 0;
        float currentNodeSize = nodeBuffer.nodes[0].centerAndSize.w;
        
        // Traverse down to leaf (max 15 levels)
        for (int depth = 0; depth < 15; depth++) {
            struct OctreeNode node = nodeBuffer.nodes[nodeIndex];
            
            // Check if this is a leaf
            bool isLeaf = (node.childrenAndFlags.z & 1u) != 0u;
            
            if (isLeaf) {
                // Extract MaterialID directly from flags
                uint materialId = (node.childrenAndFlags.z >> 8) & 0xFFu;
                
                // MaterialTable indices match MaterialID enum values:
                // 0=Vacuum, 1=Air, 2=Rock, 3=Water, 4=Sand, etc.
                // Skip rendering for Vacuum(0) and Air(1)
                
                if (materialId > 1u) {
                    // Direct lookup - no mapping needed!
                    vec4 color = materialTable.materials[materialId].color;
                    
                    // Simple lighting
                    vec3 normal = normalize(currentPos);
                    vec3 lightDir;
                    lightDir.x = 1.0; lightDir.y = 1.0; lightDir.z = 0.5;
                    lightDir = normalize(lightDir);
                    float NdotL = max(dot(normal, lightDir), 0.0);
                    
                    color.x = color.x * (0.3 + 0.7 * NdotL);
                    color.y = color.y * (0.3 + 0.7 * NdotL);
                    color.z = color.z * (0.3 + 0.7 * NdotL);
                    
                    return color;
                }
                break; // Leaf with air, continue marching
            }
            
            // Not a leaf, find child
            uint childrenOffset = node.childrenAndFlags.x;
            if (childrenOffset == 0xFFFFFFFFu || childrenOffset >= 200000u) {
                break; // Invalid children
            }
            
            // Calculate octant
            vec3 nodeCenter;
            nodeCenter.x = node.centerAndSize.x;
            nodeCenter.y = node.centerAndSize.y;
            nodeCenter.z = node.centerAndSize.z;
            
            uint octant = 0u;
            if (currentPos.x > nodeCenter.x) octant |= 1u;
            if (currentPos.y > nodeCenter.y) octant |= 2u;
            if (currentPos.z > nodeCenter.z) octant |= 4u;
            
            // Move to child node
            nodeIndex = childrenOffset + octant;
            currentNodeSize = currentNodeSize * 0.5;
            
            // Bounds check
            if (nodeIndex >= 200000u) {
                break;
            }
        }
        
        // Step forward with smaller steps to avoid gaps
        float stepSize = max(MIN_STEP, currentNodeSize * 0.25);  // Reduced from 0.5 to 0.25
        t = t + stepSize;
    }
    
    return blackSpace;
}

// GLSL_MAIN_START
#ifdef GLSL
void main() {
    vec2 uv = (gl_FragCoord.xy / pc.resolution) * 2.0 - 1.0;
    uv.y = -uv.y;
    
    vec3 rayOrigin = ubo.viewPos;
    
    mat4 invViewProj = inverse(ubo.viewProj);
    vec4 nearPoint = invViewProj * vec4(uv, 0.0, 1.0);
    vec4 farPoint = invViewProj * vec4(uv, 1.0, 1.0);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    
    vec3 rayDir = normalize(farPoint.xyz - rayOrigin);
    
    outColor = traverseOctree(rayOrigin, rayDir);
}
#endif
// GLSL_MAIN_END

// C_TEST_START
#ifndef GLSL
#include <stdio.h>

void initMaterialTable() {
    // Initialize material table matching MaterialID enum from material_table.hpp
    // 0: Vacuum - black/transparent
    materialTable.materials[0].color.x = 0.0f;
    materialTable.materials[0].color.y = 0.0f;
    materialTable.materials[0].color.z = 0.0f;
    materialTable.materials[0].color.w = 0.0f;
    
    // 1: Air - transparent blue
    materialTable.materials[1].color.x = 0.7f;
    materialTable.materials[1].color.y = 0.85f;
    materialTable.materials[1].color.z = 1.0f;
    materialTable.materials[1].color.w = 0.1f;
    
    // 2: Rock - gray
    materialTable.materials[2].color.x = 0.5f;
    materialTable.materials[2].color.y = 0.5f;
    materialTable.materials[2].color.z = 0.5f;
    materialTable.materials[2].color.w = 1.0f;
    
    // 3: Water - blue
    materialTable.materials[3].color.x = 0.1f;
    materialTable.materials[3].color.y = 0.4f;
    materialTable.materials[3].color.z = 0.8f;
    materialTable.materials[3].color.w = 0.9f;
    
    // 4: Sand - tan
    materialTable.materials[4].color.x = 0.9f;
    materialTable.materials[4].color.y = 0.8f;
    materialTable.materials[4].color.z = 0.6f;
    materialTable.materials[4].color.w = 1.0f;
    
    // 5: Soil - brown
    materialTable.materials[5].color.x = 0.4f;
    materialTable.materials[5].color.y = 0.3f;
    materialTable.materials[5].color.z = 0.2f;
    materialTable.materials[5].color.w = 1.0f;
    
    // 6: Grass - green
    materialTable.materials[6].color.x = 0.2f;
    materialTable.materials[6].color.y = 0.6f;
    materialTable.materials[6].color.z = 0.2f;
    materialTable.materials[6].color.w = 1.0f;
    
    // 7: Snow - white
    materialTable.materials[7].color.x = 0.95f;
    materialTable.materials[7].color.y = 0.95f;
    materialTable.materials[7].color.z = 0.95f;
    materialTable.materials[7].color.w = 1.0f;
    
    // 8: Ice - light blue
    materialTable.materials[8].color.x = 0.8f;
    materialTable.materials[8].color.y = 0.9f;
    materialTable.materials[8].color.z = 1.0f;
    materialTable.materials[8].color.w = 0.95f;
    
    // 9: Granite - dark gray
    materialTable.materials[9].color.x = 0.4f;
    materialTable.materials[9].color.y = 0.4f;
    materialTable.materials[9].color.z = 0.4f;
    materialTable.materials[9].color.w = 1.0f;
    
    // 10: Basalt - very dark gray
    materialTable.materials[10].color.x = 0.2f;
    materialTable.materials[10].color.y = 0.2f;
    materialTable.materials[10].color.z = 0.2f;
    materialTable.materials[10].color.w = 1.0f;
    
    // 11: Clay - reddish brown
    materialTable.materials[11].color.x = 0.6f;
    materialTable.materials[11].color.y = 0.4f;
    materialTable.materials[11].color.z = 0.3f;
    materialTable.materials[11].color.w = 1.0f;
    
    // 12: Lava - bright orange/red
    materialTable.materials[12].color.x = 1.0f;
    materialTable.materials[12].color.y = 0.3f;
    materialTable.materials[12].color.z = 0.0f;
    materialTable.materials[12].color.w = 1.0f;
    
    // 13: Metal - silver
    materialTable.materials[13].color.x = 0.7f;
    materialTable.materials[13].color.y = 0.7f;
    materialTable.materials[13].color.z = 0.75f;
    materialTable.materials[13].color.w = 1.0f;
    
    // 14: Crystal - cyan
    materialTable.materials[14].color.x = 0.3f;
    materialTable.materials[14].color.y = 0.8f;
    materialTable.materials[14].color.z = 0.9f;
    materialTable.materials[14].color.w = 0.8f;
    
    // 15: Reserved - purple (for debugging)
    materialTable.materials[15].color.x = 0.8f;
    materialTable.materials[15].color.y = 0.2f;
    materialTable.materials[15].color.z = 0.8f;
    materialTable.materials[15].color.w = 1.0f;
}

int main() {
    printf("Testing octree ray marching with material table...\n");
    
    // Initialize material table
    initMaterialTable();
    
    // Setup test data
    pc.planetRadius = 9556500.0f;
    pc.debugMode = 0;
    ubo.viewPos.x = 20000000.0f;
    ubo.viewPos.y = 0.0f;
    ubo.viewPos.z = 0.0f;
    
    // Setup simple octree
    nodeBuffer.nodes[0].centerAndSize.x = 0.0f;
    nodeBuffer.nodes[0].centerAndSize.y = 0.0f;
    nodeBuffer.nodes[0].centerAndSize.z = 0.0f;
    nodeBuffer.nodes[0].centerAndSize.w = 9556500.0f;
    nodeBuffer.nodes[0].childrenAndFlags.x = 1;
    nodeBuffer.nodes[0].childrenAndFlags.z = 0; // Not a leaf
    
    // Add a child with water material (ID=3)
    nodeBuffer.nodes[1].centerAndSize.x = 5000000.0f;
    nodeBuffer.nodes[1].centerAndSize.y = 0.0f;
    nodeBuffer.nodes[1].centerAndSize.z = 0.0f;
    nodeBuffer.nodes[1].centerAndSize.w = 2500000.0f;
    nodeBuffer.nodes[1].childrenAndFlags.x = 0xFFFFFFFF;
    nodeBuffer.nodes[1].childrenAndFlags.z = 0x0301; // Leaf with water (material ID 3)
    
    // Test ray
    vec3 rayOrigin = ubo.viewPos;
    vec3 rayDir;
    rayDir.x = -1.0f; rayDir.y = 0.0f; rayDir.z = 0.0f;
    rayDir = normalize(rayDir);
    
    vec4 result = traverseOctree(rayOrigin, rayDir);
    
    printf("Result color: (%.2f, %.2f, %.2f, %.2f)\n", 
           result.x, result.y, result.z, result.w);
    
    if (result.z > 0.5f) {
        printf("SUCCESS: Found water (blue)!\n");
    } else {
        printf("FAILURE: Expected water color\n");
    }
    
    return 0;
}
#endif
// C_TEST_END