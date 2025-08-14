// DEBUG SHADER: Visualize UV orientation per face to diagnose "jammed puzzle" effect

// Fragment shader that colors patches based on their UV coordinates
// This will reveal if adjacent patches have inconsistent UV orientations

#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in float fragMorphFactor;
layout(location = 5) in float fragAltitude;
layout(location = 6) in vec3 fragViewDir;

layout(location = 0) out vec4 outColor;

void main() {
    // DIAGNOSTIC: Color based on UV coordinates to reveal orientation
    // Red = U coordinate (0 to 1)
    // Green = V coordinate (0 to 1)
    // Blue = face ID indicator
    
    float u = fragTexCoord.x;
    float v = fragTexCoord.y;
    
    // Create a gradient that shows UV orientation
    vec3 uvColor = vec3(u, v, 0.0);
    
    // Add a grid pattern to make orientation even clearer
    float gridSize = 4.0;
    float uGrid = fract(u * gridSize);
    float vGrid = fract(v * gridSize);
    
    // Create grid lines
    float gridLine = 0.0;
    if (uGrid < 0.1 || uGrid > 0.9 || vGrid < 0.1 || vGrid > 0.9) {
        gridLine = 0.5;
    }
    
    // Final color: UV gradient with grid overlay
    vec3 color = uvColor + vec3(gridLine);
    
    // Make patch boundaries extra visible
    const float EDGE_WIDTH = 0.02;
    if (fragTexCoord.x < EDGE_WIDTH || fragTexCoord.x > 1.0 - EDGE_WIDTH ||
        fragTexCoord.y < EDGE_WIDTH || fragTexCoord.y > 1.0 - EDGE_WIDTH) {
        // White edges
        color = vec3(1.0, 1.0, 1.0);
    }
    
    outColor = vec4(color, 1.0);
}