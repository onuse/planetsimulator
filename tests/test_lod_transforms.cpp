// Test LOD system transforms and rendering calculations
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cassert>

// Simulate the cube-to-sphere mapping from the vertex shader
glm::vec3 cubeToSphere(const glm::vec3& cubePos) {
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos = cubePos * glm::vec3(
        sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f),
        sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f),
        sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f)
    );
    return glm::normalize(spherePos);
}

// Test terrain height function
float getTerrainHeight(const glm::vec3& sphereNormal) {
    // Create continents using low-frequency noise
    float continents = sin(sphereNormal.x * 2.0f) * cos(sphereNormal.y * 1.5f) * 3000.0f;
    continents += sin(sphereNormal.z * 1.8f + 2.3f) * cos(sphereNormal.x * 2.2f) * 2500.0f;
    
    // Add mountain ranges
    float mountains = sin(sphereNormal.x * 8.0f) * sin(sphereNormal.y * 7.0f) * 1500.0f;
    mountains *= std::max(0.0f, continents / 3000.0f); // Mountains only on land
    
    // Add smaller details
    float detail = sin(sphereNormal.x * 20.0f) * cos(sphereNormal.y * 25.0f) * 200.0f;
    
    // Combine to create terrain with ~70% ocean coverage
    float height = continents + mountains * 0.5f + detail * 0.2f;
    
    // Create ocean floor variation
    if (height < 0.0f) {
        height = height * 0.5f - 2000.0f; // Ocean depth
    }
    
    return height;
}

// Test color mapping based on altitude
glm::vec3 getTerrainColor(float altitude) {
    glm::vec3 color;
    
    if (altitude < -1000.0f) {
        // Deep ocean - rich blue
        color = glm::vec3(0.0f, 0.2f, 0.6f);
    } else if (altitude < 500.0f) {
        // Shallow ocean to coast - lighter blue
        float t = (altitude + 1000.0f) / 1500.0f;
        glm::vec3 deepOcean(0.0f, 0.2f, 0.6f);
        glm::vec3 shallowOcean(0.2f, 0.5f, 0.7f);
        color = deepOcean * (1.0f - t) + shallowOcean * t;
    } else if (altitude < 700.0f) {
        // Beach/sand
        float t = (altitude - 500.0f) / 200.0f;
        glm::vec3 coast(0.2f, 0.5f, 0.6f);
        glm::vec3 sand(0.76f, 0.70f, 0.50f);
        color = coast * (1.0f - t) + sand * t;
    } else if (altitude < 2000.0f) {
        // Grassland/forest
        float t = (altitude - 700.0f) / 1300.0f;
        glm::vec3 lowGrass(0.3f, 0.6f, 0.2f);
        glm::vec3 forest(0.1f, 0.4f, 0.1f);
        color = lowGrass * (1.0f - t) + forest * t;
    } else {
        // Snow/rock
        color = glm::vec3(0.9f, 0.9f, 0.95f);
    }
    
    return color;
}

void testCubeToSphereMapping() {
    std::cout << "Testing cube-to-sphere mapping..." << std::endl;
    
    // Test corner points
    glm::vec3 corner(1.0f, 1.0f, 1.0f);
    glm::vec3 spherePoint = cubeToSphere(corner);
    float length = glm::length(spherePoint);
    
    assert(std::abs(length - 1.0f) < 0.001f);
    std::cout << "  Corner mapping: " << corner.x << "," << corner.y << "," << corner.z 
              << " -> " << spherePoint.x << "," << spherePoint.y << "," << spherePoint.z 
              << " (length=" << length << ")" << std::endl;
    
    // Test face centers - should remain unchanged
    glm::vec3 faceCenter(0.0f, 0.0f, 1.0f);
    glm::vec3 sphereFace = cubeToSphere(faceCenter);
    assert(std::abs(sphereFace.z - 1.0f) < 0.001f);
    std::cout << "  Face center mapping correct" << std::endl;
    
    std::cout << "  PASSED" << std::endl;
}

void testTerrainGeneration() {
    std::cout << "Testing terrain generation..." << std::endl;
    
    int oceanCount = 0;
    int landCount = 0;
    float minHeight = 1e9f;
    float maxHeight = -1e9f;
    
    // Sample many points on sphere
    const int samples = 1000;
    for (int i = 0; i < samples; i++) {
        float theta = (float)i / samples * 2.0f * M_PI;
        for (int j = 0; j < samples/10; j++) {
            float phi = (float)j / (samples/10) * M_PI - M_PI/2.0f;
            
            glm::vec3 sphereNormal(
                cos(phi) * cos(theta),
                sin(phi),
                cos(phi) * sin(theta)
            );
            
            float height = getTerrainHeight(sphereNormal);
            minHeight = std::min(minHeight, height);
            maxHeight = std::max(maxHeight, height);
            
            if (height < 0.0f) oceanCount++;
            else landCount++;
        }
    }
    
    float oceanPercent = (float)oceanCount / (oceanCount + landCount) * 100.0f;
    std::cout << "  Ocean coverage: " << oceanPercent << "%" << std::endl;
    std::cout << "  Height range: " << minHeight << " to " << maxHeight << " meters" << std::endl;
    
    // We expect significant ocean coverage
    assert(oceanPercent > 30.0f && oceanPercent < 90.0f);
    assert(minHeight < -1000.0f); // Should have deep oceans
    assert(maxHeight > 1000.0f);  // Should have mountains
    
    std::cout << "  PASSED" << std::endl;
}

void testColorMapping() {
    std::cout << "Testing color mapping..." << std::endl;
    
    // Test ocean color
    glm::vec3 deepOcean = getTerrainColor(-3000.0f);
    assert(deepOcean.b > deepOcean.r && deepOcean.b > deepOcean.g); // Should be blue
    std::cout << "  Deep ocean color: RGB(" << deepOcean.r << "," << deepOcean.g << "," << deepOcean.b << ")" << std::endl;
    
    // Test land color
    glm::vec3 grassland = getTerrainColor(1000.0f);
    assert(grassland.g > grassland.r && grassland.g > grassland.b); // Should be green
    std::cout << "  Grassland color: RGB(" << grassland.r << "," << grassland.g << "," << grassland.b << ")" << std::endl;
    
    // Test beach color
    glm::vec3 beach = getTerrainColor(600.0f);
    std::cout << "  Beach color: RGB(" << beach.r << "," << beach.g << "," << beach.b << ")" << std::endl;
    
    std::cout << "  PASSED" << std::endl;
}

void testPatchTransforms() {
    std::cout << "Testing patch transforms..." << std::endl;
    
    // Test that all 6 cube faces are being transformed correctly
    const int FACE_COUNT = 6;
    glm::vec3 faceNormals[FACE_COUNT] = {
        glm::vec3(0, 0, 1),   // +Z
        glm::vec3(0, 0, -1),  // -Z
        glm::vec3(1, 0, 0),   // +X
        glm::vec3(-1, 0, 0),  // -X
        glm::vec3(0, 1, 0),   // +Y
        glm::vec3(0, -1, 0)   // -Y
    };
    
    for (int i = 0; i < FACE_COUNT; i++) {
        std::cout << "  Face " << i << " normal: " 
                  << faceNormals[i].x << "," << faceNormals[i].y << "," << faceNormals[i].z << std::endl;
        
        // Each face should map to a different part of the sphere
        glm::vec3 spherePos = cubeToSphere(faceNormals[i]);
        float dot = glm::dot(spherePos, faceNormals[i]);
        assert(dot > 0.9f); // Should be mostly aligned with original face
    }
    
    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== LOD Transform and Rendering Tests ===" << std::endl;
    
    try {
        testCubeToSphereMapping();
        testTerrainGeneration();
        testColorMapping();
        testPatchTransforms();
        
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}