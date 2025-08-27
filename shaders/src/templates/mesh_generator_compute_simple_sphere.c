// Template for GPU mesh generation compute shader - SIMPLE SPHERE VERSION
// For high-altitude viewing, just render a smooth sphere

#version 450

// Local workgroup size for compute dispatch
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// GPU Octree structures (must match C++ layout exactly)
struct GPUOctreeNode {
    vec4 centerAndSize;      // xyz = center, w = halfSize
    uvec4 childrenAndFlags;  // x = children offset, y = voxel offset, z = flags, w = reserved
};

struct GPUVoxelData {
    vec4 colorAndDensity;    // rgb = color, a = density
    vec4 tempAndVelocity;    // x = temperature, yzw = velocity
};

// Input: Octree data buffers (not used for simple sphere)
layout(std430, binding = 0) readonly buffer OctreeNodes {
    GPUOctreeNode nodes[];
} octreeNodes;

layout(std430, binding = 1) readonly buffer VoxelData {
    GPUVoxelData voxels[];
} voxelData;

// Input: Patch parameters via push constants
layout(push_constant) uniform PatchParams {
    mat4 patchTransform;     // Transform from patch UV space to cube space
    vec4 patchInfo;          // x=level, y=size, z=gridResolution, w=bufferOffset
    vec4 viewPos;            // Camera position in world space (xyz), planetRadius (w)
} params;

// Output: Generated vertices matching PatchVertex structure in C++
struct PatchVertex {
    vec3 position;      // Camera-relative position (scaled)
    vec3 normal;        // Surface normal
    vec2 texCoord;      // UV coordinates
    float height;       // Terrain height for coloring
    uint faceId;        // Face ID for debug visualization
};

layout(std430, binding = 2) writeonly buffer VertexBuffer {
    PatchVertex vertices[];
} vertexBuffer;

// Output: Generated indices for triangle mesh
layout(std430, binding = 3) writeonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

// Output: Index count for draw call
layout(std430, binding = 4) coherent buffer IndexCount {
    uint count;
} indexCount;

// Constants for coordinate transformation
const float WORLD_SCALE = 1.0 / 1000000.0; // Convert meters to scaled units

// Cube to sphere transformation - use simple normalization to match CPU
vec3 cubeToSphere(vec3 cubePos) {
    // CRITICAL FIX: Use simple normalization like the CPU code
    // The complex area-preserving mapping was causing flat patches
    return normalize(cubePos);
}

// Sample terrain height from octree voxel data
float sampleOctreeHeight(vec3 worldPos) {
    // Find the octree node containing this position
    uint nodeIndex = 0;
    GPUOctreeNode node = octreeNodes.nodes[nodeIndex];
    
    // Traverse octree to find leaf node containing this position
    const uint MAX_DEPTH = 10;
    for (uint depth = 0; depth < MAX_DEPTH; depth++) {
        // Check if this is a leaf node
        if (node.childrenAndFlags.x == 0) {
            break;  // Leaf node found
        }
        
        // Determine which child contains the position
        vec3 center = node.centerAndSize.xyz;
        uint childIndex = 0;
        if (worldPos.x > center.x) childIndex |= 1;
        if (worldPos.y > center.y) childIndex |= 2;
        if (worldPos.z > center.z) childIndex |= 4;
        
        // Move to child node
        nodeIndex = node.childrenAndFlags.x + childIndex;
        if (nodeIndex >= 100000) break;  // Safety check
        node = octreeNodes.nodes[nodeIndex];
    }
    
    // Sample voxel data at this node
    uint voxelIndex = node.childrenAndFlags.y;  // voxel offset
    if (voxelIndex > 0 && voxelIndex < 1000000) {  // Valid voxel
        GPUVoxelData voxel = voxelData.voxels[voxelIndex];
        
        // Use density to determine if inside/outside terrain
        float density = voxel.colorAndDensity.a;
        
        // Convert density to height displacement
        // Positive density = inside terrain, negative = outside
        if (density > 0.5) {
            // Inside terrain - add some height variation based on density
            return density * 100000.0;  // Scale to reasonable terrain height
        }
    }
    
    return 0.0;  // Default to sphere surface
}

// Get terrain height at a given position on the sphere
float getSimpleTerrainHeight(vec3 sphereNormal) {
    // Calculate world position on sphere surface
    float planetRadius = params.viewPos.w;
    vec3 worldPos = sphereNormal * planetRadius;
    
    // Sample height from octree
    return sampleOctreeHeight(worldPos);
}

void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    
    uint gridRes = uint(params.patchInfo.z);
    if (x >= gridRes || y >= gridRes) return;
    
    // Step 1: UV coordinates in patch space (0 to 1)
    vec2 uv = vec2(x, y) / float(gridRes - 1);
    
    // Step 2: Transform UV to cube face position using patch transform
    vec4 localPos = vec4(uv.x, uv.y, 0.0, 1.0);
    vec3 cubePos = (params.patchTransform * localPos).xyz;
    
    // Step 3: Convert cube position to sphere
    vec3 spherePos = cubeToSphere(cubePos);
    vec3 sphereNormal = normalize(spherePos);
    
    // Step 4: Simple procedural height for high-altitude viewing
    float planetRadius = params.viewPos.w;
    float height = getSimpleTerrainHeight(sphereNormal);
    
    // Step 5: Apply height displacement to get world position
    vec3 worldPos = sphereNormal * (planetRadius + height);
    
    // Step 6: Transform to camera-relative coordinates
    vec3 cameraRelativePos = worldPos - params.viewPos.xyz;
    
    // Step 7: Scale to reasonable units
    vec3 finalPos = cameraRelativePos * WORLD_SCALE;
    
    // Calculate normal (for now just use sphere normal)
    vec3 normal = sphereNormal;
    
    // Store vertex in buffer with proper offset for multi-patch generation
    uint bufferOffset = uint(params.patchInfo.w);
    uint vertexIndex = bufferOffset + (y * gridRes + x);
    vertexBuffer.vertices[vertexIndex].position = finalPos;
    vertexBuffer.vertices[vertexIndex].normal = normal;
    vertexBuffer.vertices[vertexIndex].texCoord = uv;
    vertexBuffer.vertices[vertexIndex].height = height;
    vertexBuffer.vertices[vertexIndex].faceId = uint(params.patchTransform[3][3]);
    
    // Generate indices for triangle mesh (two triangles per quad)
    // Only thread (0,0) generates indices to avoid race conditions
    if (x == 0 && y == 0) {
        uint bufferOffset = uint(params.patchInfo.w);
        uint indexOffset = bufferOffset / (gridRes * gridRes) * ((gridRes - 1) * (gridRes - 1) * 6);
        uint idx = indexOffset;
        
        for (uint row = 0; row < gridRes - 1; row++) {
            for (uint col = 0; col < gridRes - 1; col++) {
                uint topLeft = bufferOffset + (row * gridRes + col);
                uint topRight = topLeft + 1;
                uint bottomLeft = bufferOffset + ((row + 1) * gridRes + col);
                uint bottomRight = bottomLeft + 1;
                
                // First triangle (counter-clockwise winding)
                indexBuffer.indices[idx++] = topLeft;
                indexBuffer.indices[idx++] = bottomLeft;
                indexBuffer.indices[idx++] = topRight;
                
                // Second triangle (counter-clockwise winding)
                indexBuffer.indices[idx++] = topRight;
                indexBuffer.indices[idx++] = bottomLeft;
                indexBuffer.indices[idx++] = bottomRight;
            }
        }
        
        // Use atomicAdd to safely update the total index count across patches
        atomicAdd(indexCount.count, (gridRes - 1) * (gridRes - 1) * 6);
    }
}