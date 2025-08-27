// Surface points compute shader template
// This file will be transpiled to GLSL

// GLSL_BEGIN
#version 450

// Compute shader to find surface voxels and generate points for rendering
// This is Step 3 of our GPU-only pipeline

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Octree data structures (must match GPUOctreeNode in C++)
struct OctreeNode {
    vec4 centerAndSize;      // xyz = center, w = halfSize
    uvec4 childrenAndFlags;  // x = children offset, y = voxel offset, z = flags, w = reserved
};

// Input: Octree nodes
layout(binding = 0, std430) readonly buffer OctreeBuffer {
    OctreeNode nodes[];
} octree;

// Output: Point positions for surface voxels
layout(binding = 1, std430) writeonly buffer PointBuffer {
    vec4 points[];  // xyz = position, w = material ID
} pointBuffer;

// Output: Counter for number of points found
layout(binding = 2, std430) buffer CounterBuffer {
    uint pointCount;
} counter;

// Constants
const uint MAX_POINTS = 1000000;  // Maximum points we can generate
const uint MATERIAL_AIR = 0u;
const uint MATERIAL_ROCK = 1u;
const uint MATERIAL_WATER = 2u;
const float PLANET_RADIUS = 1500.0;  // Hardcoded for now, works with -radius 1000

// Check if a node is a leaf
bool isLeaf(OctreeNode node) {
    return (node.childrenAndFlags.z & 1u) != 0u;
}

// Get material ID from node flags
uint getMaterial(OctreeNode node) {
    return (node.childrenAndFlags.z >> 8u) & 0xFFu;
}

// Generate 8 corner positions for a voxel
vec3 getVoxelCorner(vec3 center, float halfSize, uint cornerIndex) {
    vec3 offset = vec3(
        (cornerIndex & 1u) != 0u ? halfSize : -halfSize,
        (cornerIndex & 2u) != 0u ? halfSize : -halfSize,
        (cornerIndex & 4u) != 0u ? halfSize : -halfSize
    );
    return center + offset;
}

void main() {
    uint nodeIndex = gl_GlobalInvocationID.x;
    
    // Check bounds
    if (nodeIndex >= octree.nodes.length()) {
        return;
    }
    
    OctreeNode node = octree.nodes[nodeIndex];
    
    // Only process leaf nodes
    if (!isLeaf(node)) {
        return;
    }
    
    // Get material - skip air voxels
    uint material = getMaterial(node);
    if (material == MATERIAL_AIR) {
        return;
    }
    
    // This is a solid surface voxel! Add it as a point
    vec3 center = node.centerAndSize.xyz;
    float halfSize = node.centerAndSize.w;
    
    // IMPORTANT: Only add points that are inside the planet sphere!
    float distFromOrigin = length(center);
    if (distFromOrigin > PLANET_RADIUS) {
        return;  // Skip voxels outside the planet
    }
    
    // For now, just add the center point
    // Later we can add all 8 corners or do edge detection
    uint pointIndex = atomicAdd(counter.pointCount, 1u);
    
    if (pointIndex < MAX_POINTS) {
        pointBuffer.points[pointIndex] = vec4(center, float(material));
        
        // Optional: Add corner points for better coverage
        // This gives us a denser point cloud
        if (halfSize < 10.0) {  // Only for small voxels
            for (uint corner = 1u; corner < 8u; corner++) {
                uint extraIndex = atomicAdd(counter.pointCount, 1u);
                if (extraIndex < MAX_POINTS) {
                    vec3 cornerPos = getVoxelCorner(center, halfSize * 0.8, corner);
                    // Also check corner is inside sphere
                    if (length(cornerPos) <= PLANET_RADIUS) {
                        pointBuffer.points[extraIndex] = vec4(cornerPos, float(material));
                    }
                }
            }
        }
    }
}
// GLSL_END