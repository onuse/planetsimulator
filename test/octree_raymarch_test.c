/**
 * C implementation of octree ray marching algorithm
 * This can be compiled and tested independently before converting to GLSL
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

// Vector math structures (mimicking GLSL)
typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    float x, y, z, w;
} vec4;

typedef struct {
    uint32_t x, y, z, w;
} uvec4;

// GPU node structure (matches shader exactly)
typedef struct {
    vec4 centerAndSize;      // xyz = center, w = halfSize
    uvec4 childrenAndFlags;  // x = children offset, y = voxel offset, z = flags
} OctreeNode;

// Material types
enum MaterialType {
    MATERIAL_AIR = 0,
    MATERIAL_ROCK = 1,
    MATERIAL_WATER = 2,
    MATERIAL_MAGMA = 3
};

// Vector operations
vec3 vec3_make(float x, float y, float z) {
    vec3 v = {x, y, z};
    return v;
}

vec3 vec3_add(vec3 a, vec3 b) {
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec3 vec3_sub(vec3 a, vec3 b) {
    return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3 vec3_scale(vec3 v, float s) {
    return vec3_make(v.x * s, v.y * s, v.z * s);
}

float vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_length(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3 vec3_normalize(vec3 v) {
    float len = vec3_length(v);
    if (len > 0.0001f) {
        return vec3_scale(v, 1.0f / len);
    }
    return v;
}

// Ray-sphere intersection
// Returns: vec2 equivalent (near, far), or (-1, -1) if no hit
typedef struct {
    float near;
    float far;
} vec2;

vec2 raySphere(vec3 origin, vec3 dir, vec3 center, float radius) {
    vec3 oc = vec3_sub(origin, center);
    float b = vec3_dot(oc, dir);
    float c = vec3_dot(oc, oc) - radius * radius;
    float h = b * b - c;
    
    vec2 result = {-1.0f, -1.0f};
    if (h >= 0.0f) {
        h = sqrtf(h);
        result.near = -b - h;
        result.far = -b + h;
    }
    return result;
}

// Core traversal function
typedef struct {
    int material;
    vec3 position;
    vec3 normal;
    int iterations;
    int nodesVisited;
    int maxDepth;
} TraversalResult;

TraversalResult traceOctree(vec3 rayOrigin, vec3 rayDir, 
                            OctreeNode* nodeBuffer, int nodeCount,
                            float planetRadius) {
    TraversalResult result = {
        .material = MATERIAL_AIR,
        .position = {0, 0, 0},
        .normal = {0, 1, 0},
        .iterations = 0,
        .nodesVisited = 0,
        .maxDepth = 0
    };
    
    // First check planet intersection
    vec3 planetCenter = {0, 0, 0};
    vec2 planetHit = raySphere(rayOrigin, rayDir, planetCenter, planetRadius);
    
    if (planetHit.near < 0.0f) {
        printf("  Ray misses planet\n");
        return result; // Miss planet
    }
    
    // Start ray at planet surface
    vec3 rayStart = vec3_add(rayOrigin, vec3_scale(rayDir, fmaxf(planetHit.near, 0.0f)));
    printf("  Ray enters planet at (%.2f, %.2f, %.2f)\n", rayStart.x, rayStart.y, rayStart.z);
    
    // Traversal parameters
    const int MAX_STEPS = 200;
    const float MIN_STEP = 100.0f; // 100 meters minimum
    const float MAX_DISTANCE = planetRadius * 2.0f;
    
    float t = 0.0f;
    vec3 currentPos = rayStart;
    
    // Main traversal loop
    for (int step = 0; step < MAX_STEPS; step++) {
        result.iterations++;
        
        // Check if we've exited the planet
        float distFromCenter = vec3_length(currentPos);
        if (distFromCenter > planetRadius || t > MAX_DISTANCE) {
            printf("  Exited planet at step %d\n", step);
            break;
        }
        
        // Find current node by traversing from root
        int nodeIndex = 0; // Start at root
        float currentNodeSize = nodeBuffer[0].centerAndSize.w;
        int depth = 0;
        
        // Traverse down to leaf
        while (nodeIndex < nodeCount && depth < 15) {
            OctreeNode* node = &nodeBuffer[nodeIndex];
            result.nodesVisited++;
            
            // Check if this is a leaf
            bool isLeaf = (node->childrenAndFlags.z & 1) != 0;
            
            if (isLeaf) {
                // Extract material from flags
                int material = (node->childrenAndFlags.z >> 8) & 0xFF;
                
                if (step == 0) {
                    printf("  First leaf: idx=%d, material=%d, flags=0x%x\n", 
                           nodeIndex, material, node->childrenAndFlags.z);
                }
                
                // If we found a solid material, we're done
                if (material != MATERIAL_AIR) {
                    result.material = material;
                    result.position = currentPos;
                    result.normal = vec3_normalize(currentPos); // Sphere normal
                    result.maxDepth = depth;
                    
                    printf("  HIT! Material %d at (%.2f, %.2f, %.2f) after %d steps\n",
                           material, currentPos.x, currentPos.y, currentPos.z, step);
                    return result;
                }
                break; // Leaf but air, continue marching
            }
            
            // Not a leaf, find child
            uint32_t childrenOffset = node->childrenAndFlags.x;
            if (childrenOffset == 0xFFFFFFFF || childrenOffset >= nodeCount) {
                printf("  Invalid children offset: 0x%x\n", childrenOffset);
                break; // Invalid children
            }
            
            // Calculate octant based on position relative to node center
            vec3 nodeCenter = {node->centerAndSize.x, node->centerAndSize.y, node->centerAndSize.z};
            int octant = 0;
            if (currentPos.x > nodeCenter.x) octant |= 1;
            if (currentPos.y > nodeCenter.y) octant |= 2;
            if (currentPos.z > nodeCenter.z) octant |= 4;
            
            // Move to child
            nodeIndex = childrenOffset + octant;
            currentNodeSize *= 0.5f;
            depth++;
            
            if (nodeIndex >= nodeCount) {
                printf("  Child index %d out of bounds\n", nodeIndex);
                break;
            }
        }
        
        result.maxDepth = fmaxf(result.maxDepth, depth);
        
        // Step forward along ray
        float stepSize = fmaxf(MIN_STEP, currentNodeSize * 0.5f);
        t += stepSize;
        currentPos = vec3_add(currentPos, vec3_scale(rayDir, stepSize));
    }
    
    printf("  No hit after %d iterations, visited %d nodes, max depth %d\n",
           result.iterations, result.nodesVisited, result.maxDepth);
    return result;
}

// Test harness
void test_simple_octree() {
    printf("\n=== Test: Simple Octree Traversal ===\n");
    
    // Create a simple test octree
    OctreeNode nodes[9];
    memset(nodes, 0, sizeof(nodes));
    
    // Root node (planet-sized)
    nodes[0].centerAndSize = (vec4){0, 0, 0, 10000000.0f}; // 10M meter radius
    nodes[0].childrenAndFlags = (uvec4){1, 0xFFFFFFFF, 0, 0}; // Children at index 1
    
    // 8 children - some with materials
    for (int i = 0; i < 8; i++) {
        float offset = 5000000.0f; // Half of parent
        float cx = (i & 1) ? offset : -offset;
        float cy = (i & 2) ? offset : -offset;
        float cz = (i & 4) ? offset : -offset;
        
        nodes[1 + i].centerAndSize = (vec4){cx, cy, cz, 2500000.0f};
        
        // Make children 0, 3, 5 have rock material
        if (i == 0 || i == 3 || i == 5) {
            nodes[1 + i].childrenAndFlags = (uvec4){0xFFFFFFFF, 0, 0x0101, 0}; // Leaf with Rock
            printf("  Child %d at (%.0f, %.0f, %.0f): Rock\n", i, cx, cy, cz);
        } else {
            nodes[1 + i].childrenAndFlags = (uvec4){0xFFFFFFFF, 0, 0x0001, 0}; // Leaf with Air
            printf("  Child %d at (%.0f, %.0f, %.0f): Air\n", i, cx, cy, cz);
        }
    }
    
    // Test ray from outside
    printf("\nTest 1: Ray from outside hitting octant 0 (should find Rock)\n");
    vec3 rayOrigin = {-20000000.0f, -20000000.0f, -20000000.0f};
    vec3 rayDir = vec3_normalize(vec3_make(1, 1, 1));
    
    TraversalResult result = traceOctree(rayOrigin, rayDir, nodes, 9, 6371000.0f);
    assert(result.material == MATERIAL_ROCK);
    printf("  ✓ Found material: %d\n", result.material);
    
    // Test ray hitting air octant
    printf("\nTest 2: Ray hitting octant 1 (should continue and find nothing)\n");
    rayOrigin = (vec3){20000000.0f, -10000000.0f, -10000000.0f};
    rayDir = vec3_normalize(vec3_make(-1, 0.1f, 0.1f));
    
    result = traceOctree(rayOrigin, rayDir, nodes, 9, 6371000.0f);
    printf("  Material found: %d (0=Air)\n", result.material);
}

void test_real_planet_scenario() {
    printf("\n=== Test: Real Planet Scenario ===\n");
    
    // This simulates what the actual shader receives
    // Based on debug output, we have materials set but traversal fails
    
    OctreeNode* nodes = malloc(sizeof(OctreeNode) * 100);
    memset(nodes, 0, sizeof(OctreeNode) * 100);
    
    // Root encompasses planet
    nodes[0].centerAndSize = (vec4){0, 0, 0, 9556500.0f};
    nodes[0].childrenAndFlags = (uvec4){1, 0xFFFFFFFF, 0, 0};
    
    // Add children with Water material (as seen in logs)
    for (int i = 0; i < 8; i++) {
        float offset = 4778250.0f;
        float cx = (i & 1) ? offset : -offset;
        float cy = (i & 2) ? offset : -offset;
        float cz = (i & 4) ? offset : -offset;
        
        nodes[1 + i].centerAndSize = (vec4){cx, cy, cz, 2389125.0f};
        nodes[1 + i].childrenAndFlags = (uvec4){0xFFFFFFFF, 0, 0x0201, 0}; // All water
    }
    
    printf("Setup: Root with 8 water children\n");
    
    // Test from camera position
    vec3 rayOrigin = {20000000.0f, 0, 0};
    vec3 rayDir = vec3_normalize(vec3_make(-1, 0, 0));
    
    // Use the actual planet radius that encompasses the octree
    // The root node has halfSize 9556500, so planet should be at least that
    TraversalResult result = traceOctree(rayOrigin, rayDir, nodes, 9, 9556500.0f);
    
    if (result.material == MATERIAL_WATER) {
        printf("  ✓ SUCCESS: Found water material!\n");
    } else {
        printf("  ✗ FAILURE: Expected water(2), got %d\n", result.material);
    }
    
    free(nodes);
}

int main() {
    printf("========================================\n");
    printf("   OCTREE RAY MARCH C IMPLEMENTATION   \n");
    printf("========================================\n");
    
    test_simple_octree();
    test_real_planet_scenario();
    
    printf("\n========================================\n");
    printf("            TESTS COMPLETE              \n");
    printf("========================================\n");
    
    return 0;
}