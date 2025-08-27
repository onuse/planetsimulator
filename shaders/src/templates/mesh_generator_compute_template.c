// Mesh generator compute shader template
// Implements Transvoxel algorithm on GPU
// This file will be transpiled to GLSL

// GLSL_BEGIN
#version 450

// Compute shader to generate mesh vertices from voxels using Transvoxel
// This is Step 4 of our GPU-only pipeline

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

// Octree data structures (must match GPUOctreeNode in C++)
struct OctreeNode {
    vec4 centerAndSize;      // xyz = center, w = halfSize
    uvec4 childrenAndFlags;  // x = children offset, y = voxel offset, z = flags, w = reserved
};

// CRITICAL: This must match the C++ Vertex struct in transvoxel_renderer.hpp!
// C++ struct order: position, normal, color, texCoord
// Memory layout must match exactly for proper GPU buffer interpretation
struct Vertex {
    vec3 position;   // offset 0 - location 0
    vec3 normal;     // offset 12 - location 2 (but MUST BE SECOND in struct!)
    vec3 color;      // offset 24 - location 1 (but MUST BE THIRD in struct!)
    vec2 texCoord;   // offset 36 - location 3
};

// Input: Octree nodes
layout(binding = 0, std430) readonly buffer OctreeBuffer {
    OctreeNode nodes[];
} octree;

// Input: Voxel density values (8 per leaf node)
layout(binding = 1, std430) readonly buffer VoxelBuffer {
    vec4 voxels[];  // xyz = position offset, w = density
} voxelBuffer;

// Output: Mesh vertices
layout(binding = 2, std430) writeonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

// Output: Triangle indices
layout(binding = 3, std430) writeonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

// Output: Counters for vertices and indices
layout(binding = 4, std430) buffer CounterBuffer {
    uint vertexCount;
    uint indexCount;
} counter;

// Constants
const uint MAX_VERTICES = 3000000;  // 3M vertices max
const uint MAX_INDICES = 9000000;   // 9M indices max (3M triangles)
const float PLANET_RADIUS = 1500.0;
const float ISO_LEVEL = 0.0;  // Surface at density = 0

// Transvoxel edge table (simplified for now)
// Each cell has 12 edges, we need to check which edges cross the surface
const ivec2 EDGE_VERTICES[12] = ivec2[12](
    ivec2(0, 1), ivec2(1, 2), ivec2(2, 3), ivec2(3, 0),  // Bottom face
    ivec2(4, 5), ivec2(5, 6), ivec2(6, 7), ivec2(7, 4),  // Top face
    ivec2(0, 4), ivec2(1, 5), ivec2(2, 6), ivec2(3, 7)   // Vertical edges
);

// Get corner position for a voxel
vec3 getCornerPosition(vec3 center, float halfSize, uint corner) {
    vec3 offset = vec3(
        (corner & 1u) != 0u ? halfSize : -halfSize,
        (corner & 2u) != 0u ? halfSize : -halfSize,
        (corner & 4u) != 0u ? halfSize : -halfSize
    );
    return center + offset;
}

// Sample density at a world position (simplified)
float sampleDensity(vec3 pos) {
    // Simple sphere SDF for now
    return length(pos) - PLANET_RADIUS * 0.8;
}

// Check if a node is a leaf
bool isLeaf(OctreeNode node) {
    return (node.childrenAndFlags.z & 1u) != 0u;
}

// Get material ID from node flags
uint getMaterial(OctreeNode node) {
    return (node.childrenAndFlags.z >> 8u) & 0xFFu;
}

// Linear interpolation to find surface crossing point
vec3 interpolateEdge(vec3 p1, vec3 p2, float d1, float d2) {
    float t = (ISO_LEVEL - d1) / (d2 - d1);
    return mix(p1, p2, t);
}

// Calculate normal using central differences
vec3 calculateNormal(vec3 pos) {
    float h = 0.1;
    float dx = sampleDensity(pos + vec3(h, 0, 0)) - sampleDensity(pos - vec3(h, 0, 0));
    float dy = sampleDensity(pos + vec3(0, h, 0)) - sampleDensity(pos - vec3(0, h, 0));
    float dz = sampleDensity(pos + vec3(0, 0, h)) - sampleDensity(pos - vec3(0, 0, h));
    return normalize(vec3(dx, dy, dz));
}

void main() {
    // Get voxel coordinates
    uvec3 voxelCoord = gl_GlobalInvocationID;
    
    // Simple test: process one voxel per thread
    // In reality, we'd iterate through octree nodes
    uint nodeIndex = voxelCoord.x + voxelCoord.y * 64u + voxelCoord.z * 4096u;
    
    if (nodeIndex >= octree.nodes.length()) {
        return;
    }
    
    OctreeNode node = octree.nodes[nodeIndex];
    
    // Only process leaf nodes
    if (!isLeaf(node)) {
        return;
    }
    
    vec3 center = node.centerAndSize.xyz;
    float halfSize = node.centerAndSize.w;
    uint material = getMaterial(node);
    
    // Skip air voxels
    if (material == 0u) {
        return;
    }
    
    // Skip voxels outside planet
    if (length(center) > PLANET_RADIUS) {
        return;
    }
    
    // Sample density at 8 corners
    float densities[8];
    vec3 corners[8];
    
    for (uint i = 0u; i < 8u; i++) {
        corners[i] = getCornerPosition(center, halfSize, i);
        densities[i] = sampleDensity(corners[i]);
    }
    
    // Determine cube configuration (which corners are inside/outside)
    uint cubeIndex = 0u;
    for (uint i = 0u; i < 8u; i++) {
        if (densities[i] < ISO_LEVEL) {
            cubeIndex |= (1u << i);
        }
    }
    
    // Skip if all corners are inside or all outside
    if (cubeIndex == 0u || cubeIndex == 255u) {
        return;
    }
    
    // For now, just create a simple triangle for any surface crossing
    // (Full Transvoxel would use lookup tables here)
    
    // Find first edge that crosses the surface
    for (uint e = 0u; e < 12u; e++) {
        uint v1 = uint(EDGE_VERTICES[e].x);
        uint v2 = uint(EDGE_VERTICES[e].y);
        
        // Check if edge crosses surface
        if ((densities[v1] < ISO_LEVEL) != (densities[v2] < ISO_LEVEL)) {
            // Edge crosses surface - create vertex at intersection
            vec3 surfacePos = interpolateEdge(corners[v1], corners[v2], 
                                             densities[v1], densities[v2]);
            vec3 normal = calculateNormal(surfacePos);
            
            // Add vertex
            uint vertIndex = atomicAdd(counter.vertexCount, 1u);
            if (vertIndex < MAX_VERTICES) {
                // Must write fields in struct order for proper memory layout!
                vertexBuffer.vertices[vertIndex].position = surfacePos;
                vertexBuffer.vertices[vertIndex].normal = normal;  // Normal MUST be second field!
                
                // Simple material-based coloring for now
                vec3 color = vec3(0.5, 0.5, 0.5); // default gray
                if (material == 1u) color = vec3(0.2, 0.8, 0.2); // grass green
                else if (material == 2u) color = vec3(0.7, 0.5, 0.3); // dirt brown
                else if (material == 3u) color = vec3(0.5, 0.5, 0.5); // stone gray
                
                vertexBuffer.vertices[vertIndex].color = color;  // Color MUST be third field!
                vertexBuffer.vertices[vertIndex].texCoord = vec2(0.0, 0.0); // UV coords, unused for now
            }
            
            // For testing, just add this vertex 3 times to make a visible point
            // (Real implementation would create proper triangles)
            if (vertIndex < MAX_VERTICES) {
                uint idx = atomicAdd(counter.indexCount, 3u);
                if (idx + 2u < MAX_INDICES) {
                    indexBuffer.indices[idx] = vertIndex;
                    indexBuffer.indices[idx + 1u] = vertIndex;
                    indexBuffer.indices[idx + 2u] = vertIndex;
                }
            }
            
            // Just process first crossing edge for now
            break;
        }
    }
}
// GLSL_END