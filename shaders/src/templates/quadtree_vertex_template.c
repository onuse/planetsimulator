// Quadtree LOD vertex shader template
// This file is transpiled to GLSL for the quadtree patch rendering

// GLSL_BEGIN
#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Vertex attributes
layout(location = 0) in vec2 inPatchUV;        // UV coordinates within patch (0-1)

// Instance data buffer - array of instance data
struct InstanceData {
    mat4 patchTransform;      // Transform from patch space to world space
    vec4 morphParams;         // x=morphFactor, yzw=neighborLODs
    vec4 heightTexCoord;      // UV offset/scale for height texture
    vec4 patchInfo;          // x=level, y=size, z=unused, w=unused
};

layout(binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
} instanceBuffer;

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

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out float fragMorphFactor;
layout(location = 5) out float fragAltitude;
layout(location = 6) out vec3 fragViewDir;

// Constants
const float PLANET_RADIUS = 6371000.0;

// Helper functions
vec3 cubeToSphere(vec3 cubePos) {
    vec3 pos2 = cubePos * cubePos;
    vec3 spherePos = cubePos * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y *= sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z *= sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return normalize(spherePos);
}

// Realistic terrain height function with oceans and continents
float getTerrainHeight(vec3 sphereNormal) {
    // Create continents using low-frequency noise
    float continents = sin(sphereNormal.x * 2.0) * cos(sphereNormal.y * 1.5) * 1500.0;
    continents += sin(sphereNormal.z * 1.8 + 2.3) * cos(sphereNormal.x * 2.2) * 1000.0;
    
    // Shift down to create more ocean (sea level at 0)
    continents -= 800.0; // This creates ~70% ocean coverage
    
    // Add mountain ranges only on land
    float mountains = 0.0;
    if (continents > 0.0) {
        mountains = sin(sphereNormal.x * 8.0) * sin(sphereNormal.y * 7.0) * 800.0;
        mountains += sin(sphereNormal.x * 15.0 + 1.0) * cos(sphereNormal.z * 12.0) * 400.0;
    }
    
    // Add smaller details
    float detail = sin(sphereNormal.x * 30.0) * cos(sphereNormal.y * 25.0) * 100.0;
    
    // Combine all features
    float height = continents + mountains * 0.7 + detail;
    
    // Create ocean floor variation
    if (height < 0.0) {
        // Ocean depth between -3000m and 0m
        height = height * 0.8 - 1000.0;
        height = max(height, -3000.0); // Limit ocean depth
    }
    
    return height;
}

// Calculate parent position for morphing
vec3 getParentPosition(vec2 uv, mat4 transform) {
    // Snap to parent grid
    vec2 parentUV = floor(uv * 0.5) / 0.5;
    vec4 localPos = vec4(parentUV.x, parentUV.y, 0.0, 1.0);
    vec3 cubePos = (transform * localPos).xyz;
    return cubeToSphere(cubePos) * PLANET_RADIUS;
}

// Fix T-junctions at patch edges
vec2 fixTJunction(vec2 uv, vec4 neighborLODs) {
    const float edgeThreshold = 0.01;
    vec2 fixedUV = uv;
    
    // Check each edge
    float patchLevel = instanceBuffer.instances[gl_InstanceIndex].patchInfo.x;
    if (uv.y < edgeThreshold && neighborLODs.x < patchLevel) {
        // Top edge - snap to neighbor grid
        fixedUV.x = floor(fixedUV.x * 0.5) / 0.5;
    }
    if (uv.x > 1.0 - edgeThreshold && neighborLODs.y < patchLevel) {
        // Right edge
        fixedUV.y = floor(fixedUV.y * 0.5) / 0.5;
    }
    if (uv.y > 1.0 - edgeThreshold && neighborLODs.z < patchLevel) {
        // Bottom edge
        fixedUV.x = floor(fixedUV.x * 0.5) / 0.5;
    }
    if (uv.x < edgeThreshold && neighborLODs.w < patchLevel) {
        // Left edge
        fixedUV.y = floor(fixedUV.y * 0.5) / 0.5;
    }
    
    return fixedUV;
}

void main() {
    // Use actual instance data for proper patch placement
    mat4 patchTransform = instanceBuffer.instances[gl_InstanceIndex].patchTransform;
    float morphFactor = instanceBuffer.instances[gl_InstanceIndex].morphParams.x;
    vec4 neighborLODs = instanceBuffer.instances[gl_InstanceIndex].morphParams.yzwx;
    
    // Fix T-junctions if needed
    vec2 fixedUV = fixTJunction(inPatchUV, neighborLODs);
    
    // Transform UV coordinates directly through the patch transform
    // The transform expects UV in (0,1) range and maps to cube face positions
    vec4 localPos = vec4(fixedUV.x, fixedUV.y, 0.0, 1.0);
    
    // Transform to cube face
    vec3 cubePos = (patchTransform * localPos).xyz;
    
    // Project to sphere
    vec3 spherePos = cubeToSphere(cubePos);
    vec3 sphereNormal = spherePos;
    
    // Get terrain height
    float height = getTerrainHeight(sphereNormal);
    
    // Apply height displacement
    vec3 worldPos = spherePos * (PLANET_RADIUS + height);
    
    // Morphing for smooth LOD transitions
    if (morphFactor > 0.0) {
        vec3 parentPos = getParentPosition(inPatchUV, patchTransform);
        float parentHeight = getTerrainHeight(normalize(parentPos));
        vec3 parentWorldPos = normalize(parentPos) * (PLANET_RADIUS + parentHeight);
        
        // Smooth blend using smoothstep
        float blend = smoothstep(0.0, 1.0, morphFactor);
        worldPos = mix(worldPos, parentWorldPos, blend);
    }
    
    // Calculate terrain normal using finite differences
    const float delta = 0.001; // Small offset for normal calculation
    
    // Sample neighboring points
    vec3 spherePosX = cubeToSphere((patchTransform * vec4(fixedUV + vec2(delta, 0.0), 0.0, 1.0)).xyz);
    vec3 spherePosY = cubeToSphere((patchTransform * vec4(fixedUV + vec2(0.0, delta), 0.0, 1.0)).xyz);
    
    float heightX = getTerrainHeight(spherePosX);
    float heightY = getTerrainHeight(spherePosY);
    
    vec3 posX = spherePosX * (PLANET_RADIUS + heightX);
    vec3 posY = spherePosY * (PLANET_RADIUS + heightY);
    
    // Calculate tangent vectors
    vec3 tangentX = posX - worldPos;
    vec3 tangentY = posY - worldPos;
    
    // Cross product to get normal
    vec3 terrainNormal = normalize(cross(tangentY, tangentX));
    
    // Output
    fragWorldPos = worldPos;
    fragNormal = terrainNormal; // Use calculated terrain normal
    fragColor = vec3(0.5, 0.7, 0.3); // Terrain color
    fragTexCoord = inPatchUV;
    fragMorphFactor = morphFactor;
    fragAltitude = height; // Pass terrain height to fragment shader
    fragViewDir = worldPos - ubo.viewPos;
    
    // Transform to clip space
    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}
// GLSL_END