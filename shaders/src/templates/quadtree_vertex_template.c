// Quadtree LOD vertex shader template
// This file is transpiled to GLSL for the quadtree patch rendering

// GLSL_BEGIN
#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_fp64 : require  // Enable double precision

// Vertex attributes
layout(location = 0) in vec2 inPatchUV;        // UV coordinates within patch (0-1)

// Instance data buffer - array of instance data
// Using double precision for transform to handle planet-scale coordinates
struct InstanceData {
    dmat4 patchTransform;     // Double precision transform for accuracy
    vec4 morphParams;         // x=morphFactor, yzw=neighborLODs
    vec4 heightTexCoord;      // UV offset/scale for height texture
    vec4 patchInfo;          // x=level, y=size, z=unused, w=unused
};

layout(binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
} instanceBuffer;

// Uniforms - using double precision for matrices
layout(binding = 0) uniform UniformBufferObject {
    dmat4 view;           // Double precision view matrix
    dmat4 proj;           // Double precision projection matrix
    dmat4 viewProj;       // Double precision combined matrix
    dvec3 viewPos;        // Double precision view position
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out float fragMorphFactor;
layout(location = 5) out float fragAltitude;
layout(location = 6) out vec3 fragViewDir;
layout(location = 7) flat out uint fragFaceId;

// Include the unified cube-sphere mapping
#include "../lib/cube_sphere_mapping.glsl"

// Constants
const double PLANET_RADIUS = 6371000.0lf;  // Double precision planet radius

// Simple hash function for deterministic pseudo-random values
float hash(vec3 p) {
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// Smooth interpolation function
float smoothNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    
    // Smooth the interpolation
    f = f * f * (3.0 - 2.0 * f);
    
    // Sample the 8 corners of the cube
    float a = hash(i);
    float b = hash(i + vec3(1.0, 0.0, 0.0));
    float c = hash(i + vec3(0.0, 1.0, 0.0));
    float d = hash(i + vec3(1.0, 1.0, 0.0));
    float e = hash(i + vec3(0.0, 0.0, 1.0));
    float f1 = hash(i + vec3(1.0, 0.0, 1.0));
    float g = hash(i + vec3(0.0, 1.0, 1.0));
    float h = hash(i + vec3(1.0, 1.0, 1.0));
    
    // Trilinear interpolation
    float x1 = mix(a, b, f.x);
    float x2 = mix(c, d, f.x);
    float x3 = mix(e, f1, f.x);
    float x4 = mix(g, h, f.x);
    
    float y1 = mix(x1, x2, f.y);
    float y2 = mix(x3, x4, f.y);
    
    return mix(y1, y2, f.z);
}

// Multi-octave noise for terrain
float terrainNoise(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;
    
    for (int i = 0; i < octaves; i++) {
        value += smoothNoise(p * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    
    return value / maxValue;
}

// Continuous terrain height function using proper noise
float getTerrainHeight(vec3 sphereNormal) {
    // Use the sphere normal directly as input to ensure continuity
    // Scale it to get appropriate noise frequency
    vec3 noisePos = sphereNormal * 3.0;
    
    // Continental shelf - large scale features
    float continents = terrainNoise(noisePos, 4) * 2.0 - 1.0; // Range -1 to 1
    continents = continents * 2000.0 - 500.0; // Create ocean basins
    
    // Mountain ranges - medium scale
    float mountains = 0.0;
    if (continents > 0.0) {
        vec3 mountainPos = sphereNormal * 8.0;
        mountains = terrainNoise(mountainPos, 3) * 1200.0;
    }
    
    // Small details - high frequency
    vec3 detailPos = sphereNormal * 20.0;
    float details = terrainNoise(detailPos, 2) * 200.0 - 100.0;
    
    // Combine all layers
    float height = continents + mountains * 0.7 + details * 0.3;
    
    // Ocean floor variation
    if (height < 0.0) {
        height = height * 0.8 - 500.0;
        height = max(height, -3000.0);
    }
    
    return height;
}

// Calculate parent position for morphing
dvec3 getParentPosition(vec2 uv, dmat4 transform) {
    // Snap to parent grid (which has half the resolution)
    // Parent has 2x fewer vertices, so we snap to nearest multiple of 0.5
    vec2 parentUV = floor(uv * 2.0) / 2.0;
    dvec4 localPos = dvec4(double(parentUV.x), double(parentUV.y), 0.0lf, 1.0lf);
    dvec3 cubePos = (transform * localPos).xyz;
    dvec3 spherePos = cubeToSphereDouble(cubePos, 1.0lf);
    return spherePos * PLANET_RADIUS;
}

// Fix T-junctions at patch edges by snapping to coarser neighbor's grid
vec2 fixTJunction(vec2 uv) {
    const float edgeThreshold = 0.002; // Very close to edge
    vec2 fixedUV = uv;
    
    // Get neighbor LOD levels and current patch level
    // morphParams.yzw = top, right, bottom neighbor levels  
    // heightTexCoord.x = left neighbor level, heightTexCoord.y = current patch level
    float currentLevel = instanceBuffer.instances[gl_InstanceIndex].heightTexCoord.y;
    float topNeighborLevel = instanceBuffer.instances[gl_InstanceIndex].morphParams.y;
    float rightNeighborLevel = instanceBuffer.instances[gl_InstanceIndex].morphParams.z;
    float bottomNeighborLevel = instanceBuffer.instances[gl_InstanceIndex].morphParams.w;
    float leftNeighborLevel = instanceBuffer.instances[gl_InstanceIndex].heightTexCoord.x;
    
    // If neighbor has lower level (coarser), snap our edge vertices to their grid
    // This prevents T-junctions by ensuring shared edges have identical vertex positions
    
    // Top edge (v close to 0)
    if (uv.y < edgeThreshold && topNeighborLevel < currentLevel) {
        float levelDiff = min(currentLevel - topNeighborLevel, 10.0);
        
        if (levelDiff >= 2.0) {
            // For large level differences, snap to boundary (0 or 1)
            fixedUV.x = (uv.x < 0.5) ? 0.0 : 1.0;
        } else if (levelDiff > 0.0) {
            // Standard case: spacing = 0.5
            float gridIndex = uv.x * 2.0;  // Equivalent to uv.x / 0.5
            fixedUV.x = round(gridIndex) * 0.5;
        }
    }
    
    // Bottom edge (v close to 1)
    else if (uv.y > 1.0 - edgeThreshold && bottomNeighborLevel < currentLevel) {
        float levelDiff = min(currentLevel - bottomNeighborLevel, 10.0);
        
        if (levelDiff >= 2.0) {
            // For large level differences, snap to boundary (0 or 1)
            fixedUV.x = (uv.x < 0.5) ? 0.0 : 1.0;
        } else if (levelDiff > 0.0) {
            // Standard case: spacing = 0.5
            float gridIndex = uv.x * 2.0;
            fixedUV.x = round(gridIndex) * 0.5;
        }
    }
    
    // Left edge (u close to 0)
    if (uv.x < edgeThreshold && leftNeighborLevel < currentLevel) {
        float levelDiff = min(currentLevel - leftNeighborLevel, 10.0);
        
        if (levelDiff >= 2.0) {
            // For large level differences, snap to boundary (0 or 1)
            fixedUV.y = (uv.y < 0.5) ? 0.0 : 1.0;
        } else if (levelDiff > 0.0) {
            // Standard case: spacing = 0.5
            float gridIndex = uv.y * 2.0;
            fixedUV.y = round(gridIndex) * 0.5;
        }
    }
    
    // Right edge (u close to 1)
    else if (uv.x > 1.0 - edgeThreshold && rightNeighborLevel < currentLevel) {
        float levelDiff = min(currentLevel - rightNeighborLevel, 10.0);
        
        if (levelDiff >= 2.0) {
            // For large level differences, snap to boundary (0 or 1)
            fixedUV.y = (uv.y < 0.5) ? 0.0 : 1.0;
        } else if (levelDiff > 0.0) {
            // Standard case: spacing = 0.5
            float gridIndex = uv.y * 2.0;
            fixedUV.y = round(gridIndex) * 0.5;
        }
    }
    
    return fixedUV;
}

void main() {
    // SAFETY: Clamp instance index to prevent out-of-bounds access
    uint safeIndex = min(uint(gl_InstanceIndex), 9999u); // Arbitrary high limit
    
    // Use actual instance data for proper patch placement - double precision
    dmat4 patchTransform = instanceBuffer.instances[safeIndex].patchTransform;
    float morphFactor = instanceBuffer.instances[safeIndex].morphParams.x;
    vec4 neighborLODs = instanceBuffer.instances[safeIndex].morphParams.yzwx;
    uint faceId = uint(instanceBuffer.instances[safeIndex].patchInfo.z);
    
    // Get camera distance for adaptive T-junction fixing
    double cameraDistance = length(ubo.viewPos);
    double altitude = cameraDistance - PLANET_RADIUS;
    
    // Adaptive T-junction fixing: disable at close range to prevent popping
    vec2 fixedUV = inPatchUV;
    if (altitude > 50000.0) {  // Only apply T-junction fix above 50km altitude
        fixedUV = fixTJunction(inPatchUV);
    }
    
    // Transform UV coordinates using double precision
    // The transform expects UV in (0,1) range and maps to cube face positions
    dvec4 localPos = dvec4(double(fixedUV.x), double(fixedUV.y), 0.0lf, 1.0lf);
    
    // Transform to cube face with double precision
    dvec3 cubePos = (patchTransform * localPos).xyz;
    
    // CRITICAL FIX: Ensure consistent terrain sampling across face boundaries
    // The problem: Each face uses different UV orientations (right/up vectors)
    // The solution: Use the actual 3D cube position for terrain, not UV-dependent transforms
    
    // Snap to exact face boundaries to ensure shared vertices are identical
    // CRITICAL: This MUST match the CPU-side snapping tolerance exactly!
    const double BOUNDARY = 1.0;
    const double EPSILON = 1e-5;  // Match CPU-side SNAP_EPSILON
    
    // Snap X coordinate if near ±1
    if (abs(abs(cubePos.x) - BOUNDARY) < EPSILON) {
        cubePos.x = (cubePos.x > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    // Snap Y coordinate if near ±1
    if (abs(abs(cubePos.y) - BOUNDARY) < EPSILON) {
        cubePos.y = (cubePos.y > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    // Snap Z coordinate if near ±1
    if (abs(abs(cubePos.z) - BOUNDARY) < EPSILON) {
        cubePos.z = (cubePos.z > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    
    // Project to sphere with double precision using unified implementation
    dvec3 spherePos = cubeToSphereDouble(cubePos, 1.0lf);  // Unit sphere first
    dvec3 sphereNormal = normalize(spherePos);
    
    // IMPORTANT: Use the normalized cube position for terrain sampling
    // This ensures all faces sample terrain using the same coordinate system
    // NOT the UV-transformed position which differs per face!
    dvec3 consistentSamplePos = normalize(cubePos);
    
    // Re-enable terrain height (keep as float for now)
    // DEBUG: Let's use the cube position directly to ensure consistency
    // The sphere normal SHOULD be consistent, but floating-point errors might differ
    float height = getTerrainHeight(vec3(consistentSamplePos));
    
    // Apply height displacement with double precision
    dvec3 worldPos = spherePos * (PLANET_RADIUS + double(height));
    
    // Handle skirt vertices - push them down below the sphere surface
    // Skirt vertices are identified by UV coordinates outside [0,1] range
    bool isSkirt = (fixedUV.x < 0.0 || fixedUV.x > 1.0 || fixedUV.y < 0.0 || fixedUV.y > 1.0);
    if (isSkirt) {
        // Push skirt vertices down by a fixed amount to hide gaps
        // Use a larger offset at higher altitudes where gaps are more visible
        double skirtDepth = 500.0; // 500 meters below surface
        if (altitude > 100000.0) {
            skirtDepth = 2000.0; // 2km below surface for high altitude views
        }
        worldPos = spherePos * (PLANET_RADIUS + double(height) - skirtDepth);
    }
    
    // DISABLED: Morphing for testing
    // if (morphFactor > 0.0) {
    //     vec3 parentPos = getParentPosition(inPatchUV, patchTransform);
    //     float parentHeight = getTerrainHeight(normalize(parentPos));
    //     vec3 parentWorldPos = normalize(parentPos) * (PLANET_RADIUS + parentHeight);
    //     
    //     // Smooth blend using smoothstep
    //     float blend = smoothstep(0.0, 1.0, morphFactor);
    //     worldPos = mix(worldPos, parentWorldPos, blend);
    // }
    
    // SIMPLIFIED: Just use sphere normal for testing
    vec3 terrainNormal = vec3(sphereNormal);
    
    // Output - convert back to float for fragment shader
    fragWorldPos = vec3(worldPos);  // Keep world pos for lighting calculations
    fragNormal = terrainNormal; // Use calculated terrain normal
    fragColor = vec3(0.5, 0.7, 0.3); // Terrain color
    fragTexCoord = inPatchUV;
    fragMorphFactor = morphFactor;
    fragAltitude = height; // Pass terrain height to fragment shader
    fragViewDir = vec3(worldPos - ubo.viewPos);
    fragFaceId = faceId; // Pass face ID for debug coloring
    
    // Transform to clip space - use standard viewProj for now
    // TODO: Implement proper camera-relative rendering
    gl_Position = vec4(ubo.viewProj * dvec4(worldPos, 1.0lf));
}
// GLSL_END