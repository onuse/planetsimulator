// Canonical Cube-to-Sphere Mapping (GLSL)
// This must match the CPU implementation EXACTLY

#ifndef CUBE_SPHERE_MAPPING_GLSL
#define CUBE_SPHERE_MAPPING_GLSL

// Epsilon for boundary snapping - ensures exact vertex sharing
const float BOUNDARY_EPSILON = 1e-6;

/**
 * Convert UV coordinates on a cube face to 3D position on unit cube
 * face: Face index (0-5)
 * uv: UV coordinates [0, 1]
 * Returns: 3D position on unit cube surface
 */
vec3 uvToUnitCube(int face, vec2 uv) {
    // Snap to boundaries for exact vertex sharing
    float u = uv.x;
    float v = uv.y;
    
    if (u < BOUNDARY_EPSILON) u = 0.0;
    if (u > 1.0 - BOUNDARY_EPSILON) u = 1.0;
    if (v < BOUNDARY_EPSILON) v = 0.0;
    if (v > 1.0 - BOUNDARY_EPSILON) v = 1.0;
    
    // Map [0,1] to [-1,1]
    float x = 2.0 * u - 1.0;
    float y = 2.0 * v - 1.0;
    
    // Generate 3D position based on face
    vec3 result;
    switch(face) {
        case 0: result = vec3( 1.0,  y,    x);    break; // +X
        case 1: result = vec3(-1.0,  y,   -x);    break; // -X
        case 2: result = vec3( x,    1.0,  y);    break; // +Y
        case 3: result = vec3( x,   -1.0, -y);    break; // -Y
        case 4: result = vec3(-x,    y,    1.0);  break; // +Z
        case 5: result = vec3( x,    y,   -1.0);  break; // -Z
        default: result = vec3(0.0, 0.0, 0.0);    break;
    }
    return result;
}

/**
 * Core cube-to-sphere mapping function
 * cubePos: Position on unit cube surface
 * radius: Sphere radius
 * Returns: Position on sphere surface
 */
vec3 cubeToSphere(vec3 cubePos, float radius) {
    // Normalize the cube position to get sphere direction
    vec3 normalized = normalize(cubePos);
    
    // Scale by radius
    return normalized * radius;
}

/**
 * Convert face UV to sphere position (convenience function)
 * face: Face index (0-5)
 * uv: UV coordinates [0, 1]
 * radius: Sphere radius
 * Returns: Position on sphere surface
 */
vec3 faceUVToSphere(int face, vec2 uv, float radius) {
    vec3 cubePos = uvToUnitCube(face, uv);
    return cubeToSphere(cubePos, radius);
}

// Double precision versions (if GL_ARB_gpu_shader_fp64 is available)
#ifdef GL_ARB_gpu_shader_fp64

dvec3 uvToUnitCubeDouble(int face, dvec2 uv) {
    const double BOUNDARY_EPSILON_D = 1e-12;
    
    double u = uv.x;
    double v = uv.y;
    
    if (u < BOUNDARY_EPSILON_D) u = 0.0lf;
    if (u > 1.0lf - BOUNDARY_EPSILON_D) u = 1.0lf;
    if (v < BOUNDARY_EPSILON_D) v = 0.0lf;
    if (v > 1.0lf - BOUNDARY_EPSILON_D) v = 1.0lf;
    
    double x = 2.0lf * u - 1.0lf;
    double y = 2.0lf * v - 1.0lf;
    
    dvec3 result;
    switch(face) {
        case 0: result = dvec3( 1.0lf,  y,     x);     break; // +X
        case 1: result = dvec3(-1.0lf,  y,    -x);     break; // -X
        case 2: result = dvec3( x,      1.0lf, y);     break; // +Y
        case 3: result = dvec3( x,     -1.0lf,-y);     break; // -Y
        case 4: result = dvec3(-x,      y,     1.0lf); break; // +Z
        case 5: result = dvec3( x,      y,    -1.0lf); break; // -Z
        default: result = dvec3(0.0lf, 0.0lf, 0.0lf);  break;
    }
    return result;
}

dvec3 cubeToSphereDouble(dvec3 cubePos, double radius) {
    dvec3 normalized = normalize(cubePos);
    return normalized * radius;
}

dvec3 faceUVToSphereDouble(int face, dvec2 uv, double radius) {
    dvec3 cubePos = uvToUnitCubeDouble(face, uv);
    return cubeToSphereDouble(cubePos, radius);
}

#endif // GL_ARB_gpu_shader_fp64

#endif // CUBE_SPHERE_MAPPING_GLSL