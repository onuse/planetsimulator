#include <iostream>
#include <cmath>
#include <cassert>
#include <chrono>
#include "math/cube_sphere_mapping.hpp"

using namespace PlanetSimulator::Math;

void testFloatDoubleConsistency() {
    std::cout << "Testing float/double consistency..." << std::endl;
    
    const float radius = 6371000.0f;
    float maxError = 0.0f;
    
    for (int face = 0; face < 6; ++face) {
        for (float u = 0.0f; u <= 1.0f; u += 0.1f) {
            for (float v = 0.0f; v <= 1.0f; v += 0.1f) {
                glm::vec3 spherePosF = faceUVToSphereF(face, u, v, radius);
                glm::dvec3 spherePosD = faceUVToSphereD(face, 
                    static_cast<double>(u), 
                    static_cast<double>(v), 
                    static_cast<double>(radius));
                
                glm::vec3 spherePosDAsFloat(
                    static_cast<float>(spherePosD.x),
                    static_cast<float>(spherePosD.y),
                    static_cast<float>(spherePosD.z)
                );
                
                float distance = glm::length(spherePosF - spherePosDAsFloat);
                maxError = std::max(maxError, distance);
            }
        }
    }
    
    std::cout << "  Max error between float and double: " << maxError << " meters" << std::endl;
    assert(maxError < 1.0f);
    std::cout << "  PASSED" << std::endl;
}

void testBoundaryVertexSharing() {
    std::cout << "Testing boundary vertex sharing..." << std::endl;
    
    const float radius = 6371000.0f;
    
    // Test +X right edge == +Z left edge
    glm::vec3 xEdge = faceUVToSphereF(0, 1.0f, 0.5f, radius);
    glm::vec3 zEdge = faceUVToSphereF(4, 0.0f, 0.5f, radius);
    float distance = glm::length(xEdge - zEdge);
    
    std::cout << "  +X/+Z edge distance: " << distance << " meters" << std::endl;
    assert(distance < 0.001f);
    
    // Test corner: +X, +Y, +Z
    glm::vec3 corner1 = faceUVToSphereF(0, 1.0f, 1.0f, radius);
    glm::vec3 corner2 = faceUVToSphereF(2, 1.0f, 1.0f, radius);
    glm::vec3 corner3 = faceUVToSphereF(4, 0.0f, 1.0f, radius);
    
    float dist12 = glm::length(corner1 - corner2);
    float dist13 = glm::length(corner1 - corner3);
    
    std::cout << "  Corner distances: " << dist12 << ", " << dist13 << " meters" << std::endl;
    assert(dist12 < 0.001f && dist13 < 0.001f);
    
    std::cout << "  PASSED" << std::endl;
}

void testAllPointsOnSphere() {
    std::cout << "Testing all points lie on sphere..." << std::endl;
    
    const float radius = 6371000.0f;
    float maxError = 0.0f;
    
    for (int face = 0; face < 6; ++face) {
        for (float u = 0.0f; u <= 1.0f; u += 0.2f) {
            for (float v = 0.0f; v <= 1.0f; v += 0.2f) {
                glm::vec3 spherePos = faceUVToSphereF(face, u, v, radius);
                float distance = glm::length(spherePos);
                float error = std::abs(distance - radius);
                maxError = std::max(maxError, error);
            }
        }
    }
    
    std::cout << "  Max distance error: " << maxError << " meters" << std::endl;
    assert(maxError < 2.0f);  // Within 2 meters for planet-scale coordinates
    std::cout << "  PASSED" << std::endl;
}

void testPerformance() {
    std::cout << "Testing performance..." << std::endl;
    
    const float radius = 6371000.0f;
    const int iterations = 1000000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        float u = static_cast<float>(i % 100) / 100.0f;
        float v = static_cast<float>((i / 100) % 100) / 100.0f;
        int face = i % 6;
        volatile glm::vec3 pos = faceUVToSphereF(face, u, v, radius);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double opsPerSecond = iterations * 1000000.0 / duration.count();
    std::cout << "  Operations per second: " << opsPerSecond << std::endl;
    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== Cube-to-Sphere Mapping Tests ===" << std::endl;
    
    testFloatDoubleConsistency();
    testBoundaryVertexSharing();
    testAllPointsOnSphere();
    testPerformance();
    
    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}