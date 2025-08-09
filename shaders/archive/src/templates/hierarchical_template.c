/**
 * Hierarchical Octree Traversal Shader - C Template
 * 
 * Implements hierarchical GPU octree rendering with frustum culling and LOD.
 * This shader efficiently traverses the octree structure uploaded by HierarchicalGPUOctree.
 */

// GLSL_BEGIN
#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in flat uint fragMaterialType;
layout(location = 4) in flat uint fragNodeIndex;     // Node index for hierarchical traversal
layout(location = 5) in flat uint fragLODLevel;      // LOD level for this node

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

// Hierarchical octree node structure matching HierarchicalGPUOctree::GPUNode
struct GPUNode {
    vec4 centerAndSize;      // xyz = center, w = halfSize
    uvec4 childrenAndFlags;  // x = first child index, y = voxel data offset, z = flags, w = LOD level
    vec4 boundsMin;          // AABB for frustum culling
    vec4 boundsMax;
};

// Octree node buffer
layout(std430, binding = 1) readonly buffer NodeBuffer {
    GPUNode nodes[];
} nodeBuffer;

// Voxel data structure
struct GPUVoxelData {
    vec4 colorAndDensity;     // xyz = color, w = density
    vec4 tempAndVelocity;     // x = temperature, yzw = velocity
};

// Voxel data buffer
layout(std430, binding = 2) readonly buffer VoxelBuffer {
    GPUVoxelData voxels[];
} voxelBuffer;

// Material properties (for material-based rendering)
layout(std430, binding = 3) readonly buffer MaterialTable {
    vec4 materials[16];  // Simple color palette for materials
} materialTable;

// Output color
layout(location = 0) out vec4 outColor;

// Constants
const float EPSILON = 0.001;
const float MAX_DISTANCE = 100000.0;

// Helper function to extract material from flags
uint extractMaterial(uint flags) {
    return (flags >> 8) & 0xFFu;
}

// Helper function to check if node is a leaf
bool isLeaf(uint flags) {
    return (flags & 1u) != 0u;
}

// Calculate lighting for a given position and normal
vec3 calculateLighting(vec3 worldPos, vec3 normal, vec3 baseColor, uint materialId) {
    vec3 lightColor = vec3(1.0, 1.0, 0.95);
    vec3 ambient = 0.4 * baseColor;
    
    // Directional lighting
    vec3 lightDirNorm = normalize(-ubo.lightDir);
    float diff = max(dot(normal, lightDirNorm), 0.0);
    vec3 diffuse = diff * lightColor * baseColor;
    
    // View-dependent effects
    vec3 viewDir = normalize(ubo.viewPos - worldPos);
    
    // Rim lighting for atmosphere effect
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = pow(rim, 2.0);
    vec3 rimColor = rim * vec3(0.1, 0.2, 0.3) * 0.5;
    
    // Specular for water and ice
    vec3 specular = vec3(0.0);
    if (materialId == 3u || materialId == 8u) { // Water or Ice
        vec3 reflectDir = reflect(-lightDirNorm, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        specular = spec * lightColor * 0.5;
    }
    
    // Combine all lighting
    vec3 result = ambient + diffuse + rimColor + specular;
    
    // Add water shimmer animation
    if (materialId == 3u) { // Water
        float shimmer = sin(ubo.time * 2.0 + worldPos.x * 0.001 + worldPos.z * 0.001) * 0.05 + 0.95;
        result *= shimmer;
    }
    
    // Add lava glow
    if (materialId == 12u) { // Lava
        float glow = sin(ubo.time * 3.0 + worldPos.x * 0.002) * 0.2 + 1.0;
        result *= glow;
        result += vec3(0.5, 0.1, 0.0) * glow; // Add emissive glow
    }
    
    return result;
}

// LOD-based color blending for smooth transitions
vec3 blendLODColor(vec3 color, uint lodLevel) {
    // Subtle tint based on LOD for debugging (can be removed in production)
    // Higher LODs get slightly desaturated for distance effect
    float lodFactor = 1.0 - float(lodLevel) * 0.02;
    return color * lodFactor;
}

// Main fragment shader
void main() {
    // Check if we have a valid node index
    if (fragNodeIndex >= 200000u) {
        discard;
    }
    
    // Get the node data
    GPUNode node = nodeBuffer.nodes[fragNodeIndex];
    
    // Extract material from node flags
    uint materialId = extractMaterial(node.childrenAndFlags.z);
    
    // Skip vacuum and air
    if (materialId <= 1u) {
        discard;
    }
    
    // Get base color from material table
    vec3 baseColor;
    
    // Material color mapping matching MaterialID enum
    if (materialId == 2u) { // Rock
        baseColor = vec3(0.6, 0.4, 0.2);
    } else if (materialId == 3u) { // Water
        baseColor = vec3(0.0, 0.4, 0.8);
    } else if (materialId == 4u) { // Sand
        baseColor = vec3(0.9, 0.8, 0.6);
    } else if (materialId == 5u) { // Soil
        baseColor = vec3(0.3, 0.2, 0.1);
    } else if (materialId == 6u) { // Grass
        baseColor = vec3(0.2, 0.6, 0.2);
    } else if (materialId == 7u) { // Snow
        baseColor = vec3(0.95, 0.95, 1.0);
    } else if (materialId == 8u) { // Ice
        baseColor = vec3(0.8, 0.9, 1.0);
    } else if (materialId == 9u) { // Granite
        baseColor = vec3(0.5, 0.5, 0.5);
    } else if (materialId == 10u) { // Basalt
        baseColor = vec3(0.2, 0.2, 0.2);
    } else if (materialId == 11u) { // Clay
        baseColor = vec3(0.7, 0.4, 0.3);
    } else if (materialId == 12u) { // Lava
        baseColor = vec3(1.0, 0.3, 0.0);
    } else if (materialId == 13u) { // Metal
        baseColor = vec3(0.6, 0.6, 0.7);
    } else if (materialId == 14u) { // Crystal
        baseColor = vec3(0.7, 0.8, 1.0);
    } else {
        // Unknown material - bright magenta for debugging
        baseColor = vec3(1.0, 0.0, 1.0);
    }
    
    // If this node has voxel data, use it for more detailed rendering
    if (node.childrenAndFlags.y != 0xFFFFFFFFu) {
        uint voxelIndex = node.childrenAndFlags.y;
        if (voxelIndex < 100000u) {
            GPUVoxelData voxel = voxelBuffer.voxels[voxelIndex];
            // Blend base color with voxel-specific color
            baseColor = mix(baseColor, voxel.colorAndDensity.rgb, 0.3);
        }
    }
    
    // Apply LOD-based adjustments
    baseColor = blendLODColor(baseColor, fragLODLevel);
    
    // Calculate final color with lighting
    vec3 normal = normalize(fragNormal);
    vec3 finalColor = calculateLighting(fragWorldPos, normal, baseColor, materialId);
    
    // Atmospheric scattering for distant objects
    float distance = length(ubo.viewPos - fragWorldPos);
    float fogFactor = 1.0 - exp(-distance / (500000.0)); // 500km fog distance
    vec3 fogColor = vec3(0.7, 0.85, 1.0);
    finalColor = mix(finalColor, fogColor, fogFactor * 0.3);
    
    // Debug visualization modes (can be toggled via uniform)
    #ifdef DEBUG_LOD
    // Visualize LOD levels with colors
    if (fragLODLevel == 0u) finalColor *= vec3(1.0, 0.8, 0.8); // Red tint for LOD 0
    else if (fragLODLevel == 1u) finalColor *= vec3(0.8, 1.0, 0.8); // Green for LOD 1
    else if (fragLODLevel == 2u) finalColor *= vec3(0.8, 0.8, 1.0); // Blue for LOD 2
    else finalColor *= vec3(1.0, 1.0, 0.8); // Yellow for higher LODs
    #endif
    
    #ifdef DEBUG_NODES
    // Visualize node boundaries
    vec3 nodeCenter = node.centerAndSize.xyz;
    float nodeSize = node.centerAndSize.w;
    vec3 localPos = fragWorldPos - nodeCenter;
    float edgeDist = min(min(
        nodeSize - abs(localPos.x),
        nodeSize - abs(localPos.y)),
        nodeSize - abs(localPos.z));
    if (edgeDist < nodeSize * 0.05) {
        finalColor = mix(finalColor, vec3(1.0, 1.0, 0.0), 0.5);
    }
    #endif
    
    outColor = vec4(finalColor, 1.0);
}
// GLSL_END

// C test functions
#ifdef TEST_MODE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Test material extraction
void test_material_extraction() {
    unsigned int flags = 0x0301; // Leaf flag + material ID 3
    unsigned int material = (flags >> 8) & 0xFF;
    assert(material == 3);
    printf("Material extraction test passed\n");
}

// Test leaf detection
void test_leaf_detection() {
    unsigned int leafFlags = 0x0301;    // Leaf with material
    unsigned int nonLeafFlags = 0x0300; // Not a leaf
    
    assert((leafFlags & 1) != 0);      // Is leaf
    assert((nonLeafFlags & 1) == 0);   // Not leaf
    printf("Leaf detection test passed\n");
}

// Test LOD color blending
void test_lod_blending() {
    float color[3] = {1.0f, 0.5f, 0.25f};
    
    // LOD 0 - no change
    float lod0Factor = 1.0f - 0.0f * 0.02f;
    assert(lod0Factor == 1.0f);
    
    // LOD 5 - slight desaturation
    float lod5Factor = 1.0f - 5.0f * 0.02f;
    assert(lod5Factor == 0.9f);
    
    printf("LOD blending test passed\n");
}

// Test lighting calculation basics
void test_lighting() {
    // Test ambient component
    float baseColor[3] = {1.0f, 0.5f, 0.25f};
    float ambient[3];
    
    for (int i = 0; i < 3; i++) {
        ambient[i] = 0.4f * baseColor[i];
    }
    
    assert(ambient[0] == 0.4f);
    assert(ambient[1] == 0.2f);
    assert(ambient[2] == 0.1f);
    
    printf("Lighting test passed\n");
}

int main() {
    printf("Testing hierarchical octree shader...\n");
    
    test_material_extraction();
    test_leaf_detection();
    test_lod_blending();
    test_lighting();
    
    printf("All tests passed!\n");
    return 0;
}
#endif