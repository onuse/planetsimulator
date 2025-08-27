// Octree verification compute shader template
// This file will be transpiled to GLSL

// GLSL_BEGIN
#version 450

// Simple compute shader to verify we can read the octree on GPU
// Just counts solid voxels to verify octree access works

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// Octree data structures (must match GPUOctreeNode in C++)
struct OctreeNode {
    vec4 centerAndSize;      // xyz = center, w = halfSize
    uvec4 childrenAndFlags;  // x = children offset, y = voxel offset, z = flags, w = reserved
};

// Input: Octree nodes
layout(binding = 0, std430) readonly buffer OctreeBuffer {
    OctreeNode nodes[];
} octree;

// Output: Simple counter
layout(binding = 1, std430) buffer CounterBuffer {
    uint solidVoxelCount;
    uint airVoxelCount;
    uint totalNodesVisited;
    uint maxDepthReached;
} counter;

// Stack entry for iterative traversal
struct StackEntry {
    uint nodeIndex;
    uint depth;
};

// Traverse octree iteratively (no recursion in GLSL)
void traverseOctree() {
    // Manual stack for traversal (max depth 20 should be plenty)
    StackEntry stack[160];  // 20 levels * 8 children
    int stackTop = 0;
    
    // Push root node
    stack[0].nodeIndex = 0;
    stack[0].depth = 0;
    stackTop = 1;
    
    while (stackTop > 0) {
        // Pop from stack
        stackTop--;
        uint nodeIndex = stack[stackTop].nodeIndex;
        uint depth = stack[stackTop].depth;
        
        if (nodeIndex == 0xFFFFFFFF) continue;
        
        counter.totalNodesVisited++;
        counter.maxDepthReached = max(counter.maxDepthReached, depth);
        
        OctreeNode node = octree.nodes[nodeIndex];
        
        // Check if leaf node (bit 0 of flags)
        bool isLeaf = (node.childrenAndFlags.z & 1u) != 0u;
        
        if (isLeaf) {
            // Extract material ID from flags (bits 8-15)
            uint materialId = (node.childrenAndFlags.z >> 8u) & 0xFFu;
            
            // Count based on material (0 = air/vacuum, anything else = solid)
            if (materialId == 0u) {
                counter.airVoxelCount++;
            } else {
                counter.solidVoxelCount++;
            }
        } else {
            // Push children to stack
            uint childrenOffset = node.childrenAndFlags.x;
            if (childrenOffset != 0xFFFFFFFF && stackTop < 160 - 8) {
                for (uint i = 0u; i < 8u; i++) {
                    stack[stackTop].nodeIndex = childrenOffset + i;
                    stack[stackTop].depth = depth + 1u;
                    stackTop++;
                }
            }
        }
    }
}

void main() {
    // Only thread 0 does the work (single-threaded traversal for now)
    if (gl_GlobalInvocationID.x == 0u) {
        // Initialize counters
        counter.solidVoxelCount = 0u;
        counter.airVoxelCount = 0u;
        counter.totalNodesVisited = 0u;
        counter.maxDepthReached = 0u;
        
        // Start traversal
        traverseOctree();
    }
}
// GLSL_END