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
    vec4 patchInfo;          // x=level, y=size, z=gridResolution, w=planetRadius
    vec4 viewPos;            // Camera position in world space (xyz), w unused
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
layout(std430, binding = 4) writeonly buffer IndexCount {
    uint count;
} indexCount;

// Constants for coordinate transformation
const float WORLD_SCALE = 1.0 / 1000000.0; // Convert meters to scaled units

// Cube to sphere transformation - matches CPU's PlanetSimulator::Math::cubeToSphereD()
// This is the corrected spherical mapping that avoids distortion
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

// Simple procedural terrain height for testing
// TODO: Replace with octree sampling once vertices are visible
float getTerrainHeight(vec3 sphereNormal) {
    // Simple sine wave pattern for testing
    float height = sin(sphereNormal.x * 10.0) * 
                   sin(sphereNormal.y * 10.0) * 
                   sin(sphereNormal.z * 10.0) * 10000.0;
    return height;
}

// Calculate surface normal using finite differences
vec3 calculateNormal(vec3 spherePos, float currentHeight) {
    // For now, return sphere normal
    // TODO: Implement proper normal calculation with height gradients
    return normalize(spherePos);
}

// Future: Sample height from GPU octree
float sampleOctreeHeight(vec3 worldPos) {
    // TODO: Implement octree traversal to sample voxel data
    // For now, return procedural height
    return getTerrainHeight(normalize(worldPos));
}

void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    
    uint gridRes = uint(params.patchInfo.z);
    if (x >= gridRes || y >= gridRes) return;
    
    // EXACTLY MATCH CPU TRANSFORMATION PIPELINE:
    
    // Step 1: UV coordinates in patch space (0 to 1)
    vec2 uv = vec2(x, y) / float(gridRes - 1);
    
    // Step 2: Transform UV to cube face position using patch transform
    vec4 localPos = vec4(uv.x, uv.y, 0.0, 1.0);
    vec3 cubePos = (params.patchTransform * localPos).xyz;
    
    // Step 3: Convert cube position to sphere (matches CPU's cubeToSphereD)
    vec3 spherePos = cubeToSphere(cubePos);
    vec3 sphereNormal = normalize(spherePos);
    
    // Step 4: Generate terrain height
    float height = getTerrainHeight(sphereNormal);
    
    // Step 5: Apply height displacement to get world position
    float planetRadius = params.patchInfo.w;
    vec3 worldPos = sphereNormal * (planetRadius + height);
    
    // Step 6: Transform to camera-relative coordinates (critical for precision)
    vec3 cameraRelativePos = worldPos - params.viewPos.xyz;
    
    // Step 7: Scale to reasonable units (1 unit = 1 million meters)
    vec3 finalPos = cameraRelativePos * WORLD_SCALE;
    
    // Calculate normal (for now just use sphere normal)
    vec3 normal = sphereNormal;
    
    // Store vertex in buffer
    uint vertexIndex = y * gridRes + x;
    vertexBuffer.vertices[vertexIndex].position = finalPos;
    vertexBuffer.vertices[vertexIndex].normal = normal;
    vertexBuffer.vertices[vertexIndex].texCoord = uv;
    vertexBuffer.vertices[vertexIndex].height = height;
    vertexBuffer.vertices[vertexIndex].faceId = uint(params.patchTransform[3][3]); // Store face ID in unused matrix element
    
    // Generate indices for triangle mesh (two triangles per quad)
    // Only thread (0,0) generates indices to avoid race conditions
    if (x == 0 && y == 0) {
        uint idx = 0;
        for (uint row = 0; row < gridRes - 1; row++) {
            for (uint col = 0; col < gridRes - 1; col++) {
                uint topLeft = row * gridRes + col;
                uint topRight = topLeft + 1;
                uint bottomLeft = (row + 1) * gridRes + col;
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
        
        // Store total index count for draw call
        indexCount.count = idx;
    }
}