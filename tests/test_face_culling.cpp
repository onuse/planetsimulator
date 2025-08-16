#include <iostream>
#include <glm/glm.hpp>
#include "math/planet_math.hpp"

int main() {
    std::cout << "=== TESTING FACE CULLING LOGIC ===" << std::endl;
    
    // Camera position from the screenshot
    glm::dvec3 viewPos(-1.115e7, 4.778e6, -9.556e6);
    double planetRadius = 6371000.0;
    
    std::cout << "Camera position: (" << viewPos.x/1000 << "km, " 
              << viewPos.y/1000 << "km, " << viewPos.z/1000 << "km)" << std::endl;
    std::cout << "Distance from origin: " << glm::length(viewPos)/1000 << "km" << std::endl;
    std::cout << "Altitude: " << (glm::length(viewPos) - planetRadius)/1000 << "km" << std::endl;
    
    std::cout << "\nTesting which faces should be culled:" << std::endl;
    for (int face = 0; face < 6; face++) {
        bool culled = math::shouldCullFace(face, viewPos, planetRadius);
        std::cout << "Face " << face << " (";
        switch(face) {
            case 0: std::cout << "+X"; break;
            case 1: std::cout << "-X"; break;
            case 2: std::cout << "+Y"; break;
            case 3: std::cout << "-Y"; break;
            case 4: std::cout << "+Z"; break;
            case 5: std::cout << "-Z"; break;
        }
        std::cout << "): " << (culled ? "CULLED" : "VISIBLE") << std::endl;
    }
    
    // Based on camera position, we expect:
    // Camera is at negative X, positive Y, negative Z
    // So we should see: -X face, +Y face, -Z face, and possibly edges of others
    
    std::cout << "\nExpected visible faces based on camera position:" << std::endl;
    std::cout << "Camera is in -X, +Y, -Z octant" << std::endl;
    std::cout << "Should definitely see: -X, +Y, -Z faces" << std::endl;
    std::cout << "Might see edges of: +X, -Y, +Z faces" << std::endl;
    
    return 0;
}