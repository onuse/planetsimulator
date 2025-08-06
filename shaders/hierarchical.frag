#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in flat uint fragMaterialType;

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

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(-ubo.lightDir);
    
    // Basic diffuse lighting
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 0.95); // Slightly warm light
    
    // Ambient light
    vec3 ambient = vec3(0.15, 0.15, 0.2); // Slightly blue ambient
    
    // Material-specific adjustments
    vec3 materialColor = fragColor;
    if (fragMaterialType == 2) { // Water
        // Add some specular for water
        vec3 viewDir = normalize(ubo.viewPos - fragWorldPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        materialColor += vec3(0.3) * spec;
    } else if (fragMaterialType == 3) { // Magma
        // Make magma emissive
        ambient = vec3(0.5, 0.2, 0.1);
        materialColor *= 1.5;
    }
    
    // Combine lighting
    vec3 result = (ambient + diffuse) * materialColor;
    
    // Simple distance fog (scaled for planetary view)
    float distance = length(ubo.viewPos - fragWorldPos);
    float fogStart = 10000000.0;  // 10,000 km
    float fogEnd = 100000000.0;   // 100,000 km
    float fogFactor = clamp((fogEnd - distance) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 fogColor = vec3(0.7, 0.8, 0.9);
    result = mix(fogColor, result, fogFactor);
    
    // Gamma correction
    result = pow(result, vec3(1.0/2.2));
    
    outColor = vec4(result, 1.0);
}