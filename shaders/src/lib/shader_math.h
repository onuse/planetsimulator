// Shader Math Library - Pure C functions that can be tested and transpiled to GLSL
// This file contains all mathematical functions used in shaders
// Each function is:
// 1. Pure (no side effects)
// 2. Deterministic (same input = same output)
// 3. Testable in C
// 4. Transpilable to GLSL

#ifndef SHADER_MATH_H
#define SHADER_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type definitions for C testing
// These get replaced by GLSL types during transpilation
// ============================================================================

#ifndef GLSL_TRANSPILE
    #include <math.h>
    
    typedef struct { float x, y; } vec2;
    typedef struct { float x, y, z; } vec3;
    typedef struct { float x, y, z, w; } vec4;
    typedef struct { double x, y, z; } dvec3;
    typedef struct { double x, y, z, w; } dvec4;
    
    // C implementations of GLSL functions
    static inline float glsl_min(float a, float b) { return a < b ? a : b; }
    static inline float glsl_max(float a, float b) { return a > b ? a : b; }
    static inline float glsl_clamp(float x, float minVal, float maxVal) {
        return x < minVal ? minVal : (x > maxVal ? maxVal : x);
    }
    static inline float glsl_mix(float a, float b, float t) {
        return a * (1.0f - t) + b * t;
    }
    static inline float glsl_smoothstep(float edge0, float edge1, float x) {
        float t = glsl_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    
    // Additional math functions for C testing
    #define floor(x) floorf(x)
    #define fabs(x) fabsf(x)
    #define round(x) roundf(x)
    #define pow(x,y) powf(x,y)
    #define sqrt(x) sqrtf(x)
    
#endif

// ============================================================================
// COORDINATE TRANSFORMATIONS
// ============================================================================

// Convert a point on a unit cube face to a point on a unit sphere
// TESTED: test_shader_math.cpp::testCubeToSphere
static inline dvec3 cubeToSphere(dvec3 cubePos) {
    dvec3 pos2;
    pos2.x = cubePos.x * cubePos.x;
    pos2.y = cubePos.y * cubePos.y;
    pos2.z = cubePos.z * cubePos.z;
    
    dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    
    // Normalize
    double len = sqrt(spherePos.x * spherePos.x + spherePos.y * spherePos.y + spherePos.z * spherePos.z);
    spherePos.x /= len;
    spherePos.y /= len;
    spherePos.z /= len;
    
    return spherePos;
}

// ============================================================================
// T-JUNCTION PREVENTION
// ============================================================================

// Fix T-junctions at LOD boundaries by snapping fine vertices to coarse grid
// TESTED: test_shader_math.cpp::testTJunctionFix
static inline vec2 fixTJunctionEdge(vec2 uv, float levelDiff, int isOnEdge) {
    if (!isOnEdge || levelDiff <= 0.0f) {
        return uv;  // Not on edge or no level difference
    }
    
    // Calculate coarse grid vertices based on level difference
    // levelDiff=1: coarse has 3 vertices (0, 0.5, 1), spacing=0.5
    // levelDiff=2: coarse has 2 vertices (0, 1), spacing=1.0
    // levelDiff>=3: coarse has 2 vertices (0, 1) only
    
    vec2 result = uv;
    
    if (levelDiff >= 2.0f) {
        // For large level differences, coarse only has vertices at 0 and 1
        // Snap to nearest boundary
        if (isOnEdge == 1 || isOnEdge == 2) {  // Top or bottom edge
            result.x = (uv.x < 0.5f) ? 0.0f : 1.0f;
        } else if (isOnEdge == 3 || isOnEdge == 4) {  // Left or right edge
            result.y = (uv.y < 0.5f) ? 0.0f : 1.0f;
        }
    } else if (levelDiff > 0.0f) {
        // levelDiff = 1: standard case with spacing = 0.5
        float coarseSpacing = 0.5f;
        
        if (isOnEdge == 1 || isOnEdge == 2) {  // Top or bottom edge
            float gridIndex = uv.x / coarseSpacing;
            result.x = round(gridIndex) * coarseSpacing;
            
            // Clamp to valid range
            result.x = glsl_clamp(result.x, 0.0f, 1.0f);
        } else if (isOnEdge == 3 || isOnEdge == 4) {  // Left or right edge
            float gridIndex = uv.y / coarseSpacing;
            result.y = round(gridIndex) * coarseSpacing;
            
            // Clamp to valid range
            result.y = glsl_clamp(result.y, 0.0f, 1.0f);
        }
    }
    
    // Clamp to ensure we stay within [0,1]
    result.x = glsl_clamp(result.x, 0.0f, 1.0f);
    result.y = glsl_clamp(result.y, 0.0f, 1.0f);
    
    return result;
}

// Determine which edge a UV coordinate is on (if any)
// Returns: 0=not on edge, 1=top, 2=bottom, 3=left, 4=right
static inline int getEdgeType(vec2 uv, float threshold) {
    if (uv.y <= threshold) return 1;      // Top edge (inclusive)
    if (uv.y >= 1.0f - threshold) return 2;  // Bottom edge (inclusive)
    if (uv.x <= threshold) return 3;      // Left edge (inclusive)
    if (uv.x >= 1.0f - threshold) return 4;  // Right edge (inclusive)
    return 0;  // Not on edge
}

// ============================================================================
// TERRAIN GENERATION
// ============================================================================

// Generate terrain height at a given sphere normal
// TESTED: test_shader_math.cpp::testTerrainGeneration
static inline float getTerrainHeight(vec3 sphereNormal) {
    // Create continents using low-frequency noise
    float continents = sin(sphereNormal.x * 2.0f) * cos(sphereNormal.y * 1.5f) * 1500.0f;
    continents += sin(sphereNormal.z * 1.8f + 2.3f) * cos(sphereNormal.x * 2.2f) * 1000.0f;
    
    // Shift down to create more ocean (sea level at 0)
    continents -= 800.0f; // This creates ~70% ocean coverage
    
    // Add mountain ranges only on land
    float mountains = 0.0f;
    if (continents > 0.0f) {
        mountains = sin(sphereNormal.x * 8.0f) * sin(sphereNormal.y * 7.0f) * 800.0f;
        mountains += sin(sphereNormal.x * 15.0f + 1.0f) * cos(sphereNormal.z * 12.0f) * 400.0f;
    }
    
    // Add smaller details
    float detail = sin(sphereNormal.x * 30.0f) * cos(sphereNormal.y * 25.0f) * 100.0f;
    
    // Combine all features
    float height = continents + mountains * 0.7f + detail;
    
    // Create ocean floor variation
    if (height < 0.0f) {
        // Ocean depth between -3000m and 0m
        height = height * 0.8f - 1000.0f;
        height = glsl_max(height, -3000.0f); // Limit ocean depth
    }
    
    return height;
}

// ============================================================================
// LOD MORPHING
// ============================================================================

// Calculate morphing factor for smooth LOD transitions
// TESTED: test_shader_math.cpp::testMorphingFactor
static inline float calculateMorphFactor(float screenSpaceError, float threshold, float morphRegion) {
    float morphStart = threshold * (1.0f - morphRegion);
    float morphEnd = threshold;
    
    if (screenSpaceError <= morphStart) {
        return 0.0f;  // No morphing
    } else if (screenSpaceError >= morphEnd) {
        return 1.0f;  // Full morph
    } else {
        // Smooth transition using smoothstep
        return glsl_smoothstep(morphStart, morphEnd, screenSpaceError);
    }
}

// Calculate parent position for morphing
static inline dvec3 getParentPosition(vec2 uv, double patchSize) {
    // Snap to parent grid (which has half the resolution)
    // Parent has 2x fewer vertices, so we snap to nearest multiple of 0.5
    vec2 parentUV;
    parentUV.x = floor(uv.x * 2.0f) / 2.0f;
    parentUV.y = floor(uv.y * 2.0f) / 2.0f;
    
    // Return as dvec3 for further processing
    dvec3 result;
    result.x = (double)parentUV.x * patchSize;
    result.y = (double)parentUV.y * patchSize;
    result.z = 0.0;
    
    return result;
}

// ============================================================================
// NORMAL CALCULATION
// ============================================================================

// Calculate terrain normal using finite differences
// TESTED: test_shader_math.cpp::testNormalCalculation
static inline vec3 calculateTerrainNormal(vec3 spherePos, float height, float delta) {
    // Sample neighboring points
    vec3 right = spherePos;
    right.x += delta;
    float rightHeight = getTerrainHeight(right);
    
    vec3 up = spherePos;
    up.y += delta;
    float upHeight = getTerrainHeight(up);
    
    // Calculate gradient
    vec3 normal;
    normal.x = (height - rightHeight) / delta;
    normal.y = (height - upHeight) / delta;
    normal.z = 1.0f;
    
    // Normalize
    float len = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    normal.x /= len;
    normal.y /= len;
    normal.z /= len;
    
    return normal;
}

#ifdef __cplusplus
}
#endif

#endif // SHADER_MATH_H