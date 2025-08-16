#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Reproduce the transform creation logic
glm::dmat4 createTransform(const glm::vec3& minBounds, const glm::vec3& maxBounds) {
    glm::dvec3 rawRange = glm::dvec3(maxBounds - minBounds);
    
    glm::dvec3 range;
    range.x = std::abs(rawRange.x);
    range.y = std::abs(rawRange.y);
    range.z = std::abs(rawRange.z);
    
    // CRITICAL: This is the problem!
    const double MIN_RANGE = 1e-5;
    if (range.x > 0 && range.x < MIN_RANGE) range.x = MIN_RANGE;
    if (range.y > 0 && range.y < MIN_RANGE) range.y = MIN_RANGE;
    if (range.z > 0 && range.z < MIN_RANGE) range.z = MIN_RANGE;
    
    const double eps = 1e-6;
    
    glm::dmat4 transform(1.0);
    
    if (range.x < eps) {
        // X is fixed - patch is on +X or -X face
        double x = (minBounds.x + maxBounds.x) * 0.5;
        
        // Problem: If range.y or range.z is very small (but > eps), 
        // we're scaling UV coordinates by that tiny amount!
        transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
    }
    else if (range.y < eps) {
        double y = (minBounds.y + maxBounds.y) * 0.5;
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // V -> Z
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(minBounds.x, y, minBounds.z, 1.0);
    }
    else if (range.z < eps) {
        double z = (minBounds.z + maxBounds.z) * 0.5;
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(minBounds.x, minBounds.y, z, 1.0);
    }
    
    return transform;
}

int main() {
    std::cout << "Testing Transform Issue\n";
    std::cout << "=======================\n\n";
    
    // Test a normal root face patch (+X face)
    {
        glm::vec3 minBounds(1.0f, -1.0f, -1.0f);
        glm::vec3 maxBounds(1.0f, 1.0f, 1.0f);
        
        std::cout << "Root +X face patch:\n";
        std::cout << "  Bounds: (" << minBounds.x << "," << minBounds.y << "," << minBounds.z 
                  << ") to (" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")\n";
        
        glm::dmat4 transform = createTransform(minBounds, maxBounds);
        
        // Test UV corners
        glm::dvec4 uv00(0, 0, 0, 1);
        glm::dvec4 uv10(1, 0, 0, 1);
        glm::dvec4 uv11(1, 1, 0, 1);
        glm::dvec4 uv01(0, 1, 0, 1);
        
        glm::dvec3 p00 = glm::dvec3(transform * uv00);
        glm::dvec3 p10 = glm::dvec3(transform * uv10);
        glm::dvec3 p11 = glm::dvec3(transform * uv11);
        glm::dvec3 p01 = glm::dvec3(transform * uv01);
        
        std::cout << "  UV(0,0) -> (" << p00.x << "," << p00.y << "," << p00.z << ")\n";
        std::cout << "  UV(1,0) -> (" << p10.x << "," << p10.y << "," << p10.z << ")\n";
        std::cout << "  UV(1,1) -> (" << p11.x << "," << p11.y << "," << p11.z << ")\n";
        std::cout << "  UV(0,1) -> (" << p01.x << "," << p01.y << "," << p01.z << ")\n";
    }
    
    // Test what happens when bounds are incorrectly initialized
    // This could happen if the patch initialization is wrong
    {
        std::cout << "\nDEGENERATE CASE - Incorrect bounds:\n";
        
        // What if minBounds == maxBounds? (shouldn't happen but let's test)
        glm::vec3 minBounds(1.0f, 0.0f, 0.0f);
        glm::vec3 maxBounds(1.0f, 0.0f, 0.0f);  // Same as minBounds!
        
        std::cout << "  Bounds: (" << minBounds.x << "," << minBounds.y << "," << minBounds.z 
                  << ") to (" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")\n";
        
        glm::dmat4 transform = createTransform(minBounds, maxBounds);
        
        // Test UV corners
        glm::dvec4 uv00(0, 0, 0, 1);
        glm::dvec4 uv11(1, 1, 0, 1);
        
        glm::dvec3 p00 = glm::dvec3(transform * uv00);
        glm::dvec3 p11 = glm::dvec3(transform * uv11);
        
        std::cout << "  UV(0,0) -> (" << p00.x << "," << p00.y << "," << p00.z << ")\n";
        std::cout << "  UV(1,1) -> (" << p11.x << "," << p11.y << "," << p11.z << ")\n";
        
        // Check the distance between corners
        double dist = glm::length(p11 - p00);
        std::cout << "  Distance between corners: " << dist << " (should be ~2.83 for full face)\n";
        
        if (dist < 0.001) {
            std::cout << "  ERROR: Vertices are collapsed to nearly the same point!\n";
        }
    }
    
    // Test with MIN_RANGE applied
    {
        std::cout << "\nMIN_RANGE CASE:\n";
        
        // Bounds that would trigger MIN_RANGE
        glm::vec3 minBounds(1.0f, -0.000001f, -0.000001f);
        glm::vec3 maxBounds(1.0f, 0.000001f, 0.000001f);
        
        std::cout << "  Bounds: (" << minBounds.x << "," << minBounds.y << "," << minBounds.z 
                  << ") to (" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")\n";
        
        glm::dmat4 transform = createTransform(minBounds, maxBounds);
        
        // Test UV corners
        glm::dvec4 uv00(0, 0, 0, 1);
        glm::dvec4 uv11(1, 1, 0, 1);
        
        glm::dvec3 p00 = glm::dvec3(transform * uv00);
        glm::dvec3 p11 = glm::dvec3(transform * uv11);
        
        std::cout << "  UV(0,0) -> (" << p00.x << "," << p00.y << "," << p00.z << ")\n";
        std::cout << "  UV(1,1) -> (" << p11.x << "," << p11.y << "," << p11.z << ")\n";
        
        double dist = glm::length(p11 - p00);
        std::cout << "  Distance between corners: " << dist << " meters\n";
        std::cout << "  This is only " << (dist / 2.83) * 100 << "% of expected size!\n";
    }
    
    return 0;
}