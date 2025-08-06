#version 450

layout(location = 0) out vec4 outColor;

// Uniforms
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Push constants for runtime parameters
layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float planetRadius;
    int debugMode; // 0=normal, 1=iterations, 2=depth, 3=nodes
} pc;

// GPU Octree node structure (must match C++)
struct OctreeNode {
    vec4 centerAndSize;     // xyz = center, w = halfSize
    uvec4 childrenAndFlags; // x = children offset, y = voxel offset, z = flags, w = reserved
};

// GPU Voxel data
struct VoxelData {
    vec4 colorAndDensity;   // rgb = color, a = density
    vec4 tempAndVelocity;   // x = temperature, yzw = velocity
};

// Storage buffers for octree
layout(std430, binding = 1) readonly buffer NodeBuffer {
    OctreeNode nodes[];
} nodeBuffer;

layout(std430, binding = 2) readonly buffer VoxelBuffer {
    VoxelData voxels[];
} voxelBuffer;

// Ray-sphere intersection
vec2 raySphere(vec3 origin, vec3 dir, vec3 center, float radius) {
    vec3 oc = origin - center;
    float b = dot(oc, dir);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(-1.0);
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

// Ray-box intersection
bool rayBox(vec3 origin, vec3 invDir, vec3 center, float halfSize, out float tNear, out float tFar) {
    vec3 tMin = (center - halfSize - origin) * invDir;
    vec3 tMax = (center + halfSize - origin) * invDir;
    
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    
    tNear = max(max(t1.x, t1.y), t1.z);
    tFar = min(min(t2.x, t2.y), t2.z);
    
    return tNear <= tFar && tFar >= 0.0;
}

// Get child index based on position relative to parent center
uint getChildIndex(vec3 pos, vec3 center) {
    uint idx = 0;
    if (pos.x > center.x) idx |= 1;
    if (pos.y > center.y) idx |= 2;
    if (pos.z > center.z) idx |= 4;
    return idx;
}

// Hierarchical octree traversal with proper stack
vec4 traceOctree(vec3 origin, vec3 dir) {
    // Ray-sphere intersection for planet boundary
    vec2 planetHit = raySphere(origin, dir, vec3(0.0), pc.planetRadius);
    if (planetHit.x < 0.0) {
        return vec4(0.0); // Miss planet entirely
    }
    
    // Stack-based octree traversal
    const int MAX_STACK = 64;
    uint nodeStack[MAX_STACK];
    int stackPtr = 0;
    
    // Start with root node
    nodeStack[0] = 0u;
    
    vec3 closestColor = vec3(0.0);
    float closestT = planetHit.y; // Far plane of planet sphere
    bool foundSurface = false;
    
    int iterations = 0;
    int maxIterations = 1000;
    
    while (stackPtr >= 0 && iterations < maxIterations) {
        iterations++;
        
        // Pop node from stack
        uint nodeIdx = nodeStack[stackPtr];
        stackPtr--;
        
        // Safety check
        if (nodeIdx >= 200000u) continue;
        
        // Get node data
        OctreeNode node = nodeBuffer.nodes[nodeIdx];
        
        // Check if ray intersects this node's bounding sphere
        float nodeRadius = node.centerAndSize.w * 1.732; // sqrt(3) for cube diagonal
        vec2 nodeHit = raySphere(origin, dir, node.centerAndSize.xyz, nodeRadius);
        
        // Skip if miss or behind closest hit
        if (nodeHit.x < 0.0 || nodeHit.x > closestT) {
            continue;
        }
        
        // Check if this is a leaf node
        bool isLeaf = (node.childrenAndFlags.z & 1u) != 0u;
        
        if (isLeaf) {
            // Extract material type
            uint materialType = (node.childrenAndFlags.z >> 8) & 0xFFu;
            
            // Only process non-air materials
            if (materialType != 0u) {
                // We found a solid surface!
                closestT = nodeHit.x;
                foundSurface = true;
                
                // Set color based on material
                if (materialType == 1u) {
                    closestColor = vec3(0.2, 0.5, 0.1); // Green land/continents
                } else if (materialType == 2u) {
                    closestColor = vec3(0.0, 0.3, 0.7); // Deep blue ocean
                } else if (materialType == 3u) {
                    closestColor = vec3(1.0, 0.3, 0.0); // Orange/red magma
                } else {
                    closestColor = vec3(0.5, 0.5, 0.5); // Gray unknown
                }
            }
        } else {
            // Internal node - add children to stack
            uint childrenOffset = node.childrenAndFlags.x;
            
            // Check for valid children offset
            if (childrenOffset != 0xFFFFFFFFu && childrenOffset < 200000u && stackPtr < MAX_STACK - 9) {
                // Add all 8 children to stack (in reverse order for depth-first)
                for (int i = 7; i >= 0; i--) {
                    stackPtr++;
                    nodeStack[stackPtr] = childrenOffset + uint(i);
                }
            }
        }
    }
    
    if (foundSurface) {
        // Calculate simple lighting
        vec3 hitPoint = origin + dir * closestT;
        vec3 normal = normalize(hitPoint); // Sphere normal
        
        // Simple directional light
        vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
        float NdotL = max(dot(normal, lightDir), 0.0);
        
        // Ambient + diffuse lighting
        vec3 ambient = closestColor * 0.3;
        vec3 diffuse = closestColor * NdotL * 0.7;
        
        return vec4(ambient + diffuse, 1.0);
    }
    
    // No surface found - return black space
    return vec4(0.0, 0.0, 0.0, 1.0);
}

void main() {
    // Get screen coordinates (-1 to 1)
    vec2 uv = (gl_FragCoord.xy / pc.resolution) * 2.0 - 1.0;
    uv.y = -uv.y; // Flip Y for Vulkan
    
    // Calculate ray from camera
    vec3 rayOrigin = ubo.viewPos;
    
    // Unproject screen coordinates to world space
    mat4 invViewProj = inverse(ubo.viewProj);
    
    // Create near and far points in clip space
    vec4 nearPoint = vec4(uv, 0.0, 1.0);
    vec4 farPoint = vec4(uv, 1.0, 1.0);
    
    // Transform to world space
    nearPoint = invViewProj * nearPoint;
    farPoint = invViewProj * farPoint;
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    
    // Ray direction from camera position through the pixel
    vec3 rayDir = normalize(farPoint.xyz - rayOrigin);
    
    // Trace through octree
    vec4 color = traceOctree(rayOrigin, rayDir);
    
    // Debug modes
    if (pc.debugMode == 1) {
        // Show planet sphere in red if we would hit it
        vec2 testHit = raySphere(rayOrigin, rayDir, vec3(0.0), pc.planetRadius);
        if (testHit.x > 0.0) {
            outColor = vec4(1.0, 0.0, 0.0, 1.0); // Red for planet hit
            return;
        }
    } else if (pc.debugMode == 2) {
        // Show ray direction as color
        outColor = vec4(abs(rayDir), 1.0);
        return;
    } else if (pc.debugMode == 3) {
        // Debug: Count how many leaves we traverse
        vec2 planetHit = raySphere(rayOrigin, rayDir, vec3(0.0), pc.planetRadius);
        if (planetHit.x > 0.0) {
            int leafCount = 0;
            int nodeCount = 0;
            
            // Do a simple traversal to count nodes
            const int MAX_STACK = 64;
            uint nodeStack[MAX_STACK];
            int stackPtr = 0;
            nodeStack[0] = 0u;
            
            int iterations = 0;
            while (stackPtr >= 0 && iterations < 100) {
                iterations++;
                uint nodeIdx = nodeStack[stackPtr--];
                
                if (nodeIdx >= 200000u) continue;
                
                OctreeNode node = nodeBuffer.nodes[nodeIdx];
                nodeCount++;
                
                // Check if we hit this node
                float nodeRadius = node.centerAndSize.w * 1.732;
                vec2 nodeHit = raySphere(rayOrigin, rayDir, node.centerAndSize.xyz, nodeRadius);
                
                if (nodeHit.x < 0.0) continue;
                
                bool isLeaf = (node.childrenAndFlags.z & 1u) != 0u;
                
                if (isLeaf) {
                    leafCount++;
                    // Stop at first non-air leaf
                    uint materialType = (node.childrenAndFlags.z >> 8) & 0xFFu;
                    if (materialType != 0u) {
                        // Found a solid leaf! Color based on material
                        if (materialType == 1u) {
                            outColor = vec4(0.0, 1.0, 0.0, 1.0); // Green = rock
                        } else if (materialType == 2u) {
                            outColor = vec4(0.0, 0.0, 1.0, 1.0); // Blue = water
                        } else {
                            outColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow = other
                        }
                        return;
                    }
                } else {
                    // Add children
                    uint childrenOffset = node.childrenAndFlags.x;
                    if (childrenOffset != 0xFFFFFFFFu && childrenOffset < 200000u && stackPtr < MAX_STACK - 8) {
                        for (uint i = 0u; i < 8u; i++) {
                            stackPtr++;
                            nodeStack[stackPtr] = childrenOffset + i;
                        }
                    }
                }
            }
            
            // Color based on how many leaves we found
            if (leafCount == 0) {
                outColor = vec4(1.0, 0.0, 0.0, 1.0); // Red = no leaves
            } else {
                // Gradient from dark to bright based on leaf count
                float intensity = float(leafCount) / 10.0;
                outColor = vec4(intensity, intensity * 0.5, intensity * 0.2, 1.0);
            }
        } else {
            outColor = vec4(0.5, 0.0, 0.0, 1.0); // Dark red = missed planet
        }
        return;
    }
    
    // If we didn't hit anything, show black space
    if (color.a < 0.5) {
        // Black space background
        color = vec4(0.0, 0.0, 0.0, 1.0);
    }
    
    outColor = color;
}