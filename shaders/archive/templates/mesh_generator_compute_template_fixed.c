// Template for GPU mesh generation compute shader
// This shader generates terrain vertices by sampling the GPU octree

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

// Input: Octree data buffers
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

// Cube to sphere transformation - matches CPU's PlanetSimulator::Math::cubeToSphereD()
vec3 cubeToSphere(vec3 cubePos) {
    float x2 = cubePos.x * cubePos.x;
    float y2 = cubePos.y * cubePos.y;
    float z2 = cubePos.z * cubePos.z;
    
    vec3 spherePos;
    spherePos.x = cubePos.x * sqrt(max(0.0, 1.0 - y2 * 0.5 - z2 * 0.5 + y2 * z2 / 3.0));
    spherePos.y = cubePos.y * sqrt(max(0.0, 1.0 - x2 * 0.5 - z2 * 0.5 + x2 * z2 / 3.0));
    spherePos.z = cubePos.z * sqrt(max(0.0, 1.0 - x2 * 0.5 - y2 * 0.5 + x2 * y2 / 3.0));
    
    return spherePos;
}

// Simple procedural terrain height for fallback
float getProceduralHeight(vec3 sphereNormal) {
    // Simple smooth terrain
    float height = sin(sphereNormal.x * 5.0) * 2000.0 + 
                   sin(sphereNormal.y * 5.0) * 2000.0 + 
                   sin(sphereNormal.z * 5.0) * 2000.0;
    return height;
}

// Sample signed distance from GPU octree at world position
float sampleOctreeSignedDistance(vec3 worldPos) {
    // Start at root node (index 0)
    uint nodeIndex = 0;
    
    // Maximum traversal depth to prevent infinite loops
    const int MAX_DEPTH = 20;
    
    for (int depth = 0; depth < MAX_DEPTH; depth++) {
        GPUOctreeNode node = octreeNodes.nodes[nodeIndex];
        
        // Check if this is a leaf node (no children)
        uint childrenOffset = node.childrenAndFlags.x;
        if (childrenOffset == 0) {
            // Leaf node - check if it has voxel data
            uint voxelOffset = node.childrenAndFlags.y;
            if (voxelOffset != 0) {
                // Sample the voxel data
                GPUVoxelData voxel = voxelData.voxels[voxelOffset];
                // Return the signed distance value
                // This is the distance from the surface (negative inside, positive outside)
                return voxel.colorAndDensity.a;
            }
            // No voxel data - return large positive value (empty space)
            return 10000.0;
        }
        
        // Not a leaf - determine which child octant contains the position
        vec3 center = node.centerAndSize.xyz;
        float halfSize = node.centerAndSize.w;
        
        // Calculate which octant (0-7) the position falls into
        uint octant = 0;
        if (worldPos.x > center.x) octant |= 1;
        if (worldPos.y > center.y) octant |= 2;
        if (worldPos.z > center.z) octant |= 4;
        
        // Move to the child node
        nodeIndex = childrenOffset + octant;
    }
    
    // Max depth reached - return large positive value (empty space)
    return 10000.0;
}

// Find the surface position by searching along a ray from planet center
float findSurfaceHeight(vec3 sphereNormal, float planetRadius) {
    // Search for the surface along the ray from planet center
    // Start at planet radius and search outward
    float searchRadius = planetRadius;
    float searchStep = 100.0; // 100 meter steps
    const int MAX_STEPS = 100;
    
    // Binary search for more precision
    float minRadius = planetRadius - 10000.0;
    float maxRadius = planetRadius + 10000.0;
    
    for (int i = 0; i < 10; i++) {
        float midRadius = (minRadius + maxRadius) * 0.5;
        vec3 samplePos = sphereNormal * midRadius;
        float sd = sampleOctreeSignedDistance(samplePos);
        
        if (sd < 0.0) {
            // Inside solid - move outward
            minRadius = midRadius;
        } else {
            // Outside solid - move inward
            maxRadius = midRadius;
        }
    }
    
    // Return the height difference from planet radius
    float surfaceRadius = (minRadius + maxRadius) * 0.5;
    return surfaceRadius - planetRadius;
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
    
    // Step 4: Find terrain height using octree
    float planetRadius = params.viewPos.w;
    float height = findSurfaceHeight(sphereNormal, planetRadius);
    
    // If octree search failed, use procedural fallback
    if (abs(height) > 9000.0) {
        height = getProceduralHeight(sphereNormal);
    }
    
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