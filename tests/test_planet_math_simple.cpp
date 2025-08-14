#include "math/planet_math.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

using namespace math;

// Simple test framework
int tests_run = 0;
int tests_passed = 0;

#define TEST(name) void name(); tests_run++; std::cout << "Running " << #name << "... "; name(); tests_passed++; std::cout << "PASSED\n";
#define ASSERT_NEAR(actual, expected, tolerance) \
    if (std::abs((actual) - (expected)) > (tolerance)) { \
        std::cerr << "\nAssertion failed: " << #actual << " = " << (actual) \
                  << " expected " << (expected) << " +/- " << (tolerance) << std::endl; \
        exit(1); \
    }
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "\nAssertion failed: " << #condition << std::endl; \
        exit(1); \
    }
#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

void testCubeToSphereNormalization() {
    // Test that cube-to-sphere always produces unit vectors
    glm::dvec3 testPoints[] = {
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0),
        glm::dvec3(1.0, 0.5, 0.5),
        glm::dvec3(1.0, 1.0, 1.0)
    };
    
    for (const auto& point : testPoints) {
        glm::dvec3 spherePos = cubeToSphere(point);
        double length = glm::length(spherePos);
        ASSERT_NEAR(length, 1.0, 1e-10);
    }
}

void testFaceNormals() {
    // Test that face normals are unit vectors
    for (uint32_t i = 0; i < 6; i++) {
        glm::dvec3 normal = getFaceNormal(i);
        ASSERT_NEAR(glm::length(normal), 1.0, 1e-10);
    }
    
    // Check opposite faces are negatives
    glm::dvec3 xPos = getFaceNormal(0);
    glm::dvec3 xNeg = getFaceNormal(1);
    ASSERT_NEAR(glm::length(xPos + xNeg), 0.0, 1e-10);
}

void testScreenSpaceError() {
    const double planetRadius = 6371000.0;
    glm::dvec3 patchCenter(planetRadius, 0, 0);
    double patchSize = 0.1;
    
    // Simple view projection
    glm::dmat4 viewProj = glm::perspective(glm::radians(60.0), 16.0/9.0, 1000.0, 1e8);
    
    // Test at close distance
    glm::dvec3 viewPosClose(planetRadius * 1.1, 0, 0);
    float errorClose = calculateScreenSpaceError(patchCenter, patchSize, viewPosClose, viewProj, planetRadius);
    
    // Test at far distance
    glm::dvec3 viewPosFar(planetRadius * 10.0, 0, 0);
    float errorFar = calculateScreenSpaceError(patchCenter, patchSize, viewPosFar, viewProj, planetRadius);
    
    std::cout << "Error close: " << errorClose << ", Error far: " << errorFar << std::endl;
    
    // Error should be higher when close
    ASSERT_TRUE(errorClose > errorFar);
    
    // Should not be NaN or Inf
    ASSERT_TRUE(std::isfinite(errorClose));
    ASSERT_TRUE(std::isfinite(errorFar));
}

void testLODThreshold() {
    const double planetRadius = 6371000.0;
    
    // Test at different altitudes
    float threshold100m = calculateLODThreshold(100.0, planetRadius);
    float threshold10km = calculateLODThreshold(10000.0, planetRadius);
    float threshold1000km = calculateLODThreshold(1000000.0, planetRadius);
    
    std::cout << "Threshold at 100m: " << threshold100m 
              << ", 10km: " << threshold10km 
              << ", 1000km: " << threshold1000km << std::endl;
    
    // Threshold should increase with altitude
    ASSERT_TRUE(threshold100m < threshold10km);
    ASSERT_TRUE(threshold10km < threshold1000km);
    
    // Should be in reasonable range - updated for new threshold values
    ASSERT_TRUE(threshold100m >= 0.5f && threshold100m <= 2.0f);    // Now expecting 1.0f for very close
    ASSERT_TRUE(threshold1000km >= 3.0f && threshold1000km <= 10.0f); // Now expecting 5.0f for altitude ratio 0.15
}

void testFaceCulling() {
    const double planetRadius = 6371000.0;
    
    // Camera on +X axis looking at planet
    glm::dvec3 viewPos(planetRadius * 2, 0, 0);
    
    // +X face should be visible
    bool xPosCulled = shouldCullFace(0, viewPos, planetRadius);
    ASSERT_FALSE(xPosCulled);
    
    // -X face should be culled
    bool xNegCulled = shouldCullFace(1, viewPos, planetRadius);
    ASSERT_TRUE(xNegCulled);
}

void testPatchTransform() {
    glm::dvec3 bottomLeft(1.0, -0.5, -0.5);
    glm::dvec3 bottomRight(1.0, 0.5, -0.5);
    glm::dvec3 topLeft(1.0, -0.5, 0.5);
    uint32_t faceId = 0;
    
    glm::dmat4 transform = buildPatchTransform(bottomLeft, bottomRight, topLeft, faceId);
    
    // Transform should be valid
    ASSERT_TRUE(isValid(transform));
    
    // Transform should map (0,0) to bottomLeft
    glm::dvec4 origin(0, 0, 0, 1);
    glm::dvec4 mapped = transform * origin;
    glm::dvec3 mappedPos(mapped);
    
    ASSERT_NEAR(glm::length(mappedPos - bottomLeft), 0.0, 1e-10);
}

void testValidation() {
    // Test scalar validation
    ASSERT_TRUE(isValid(1.0));
    ASSERT_TRUE(isValid(0.0));
    ASSERT_FALSE(isValid(std::numeric_limits<double>::quiet_NaN()));
    ASSERT_FALSE(isValid(std::numeric_limits<double>::infinity()));
    
    // Test vector validation
    glm::dvec3 validVec(1.0, 2.0, 3.0);
    ASSERT_TRUE(isValid(validVec));
    
    glm::dvec3 invalidVec(1.0, std::numeric_limits<double>::quiet_NaN(), 3.0);
    ASSERT_FALSE(isValid(invalidVec));
}

int main() {
    std::cout << "Running Planet Math Tests\n";
    std::cout << "=========================\n\n";
    
    TEST(testCubeToSphereNormalization)
    TEST(testFaceNormals)
    TEST(testScreenSpaceError)
    TEST(testLODThreshold)
    TEST(testFaceCulling)
    TEST(testPatchTransform)
    TEST(testValidation)
    
    std::cout << "\n=========================\n";
    std::cout << "Tests run: " << tests_run << "\n";
    std::cout << "Tests passed: " << tests_passed << "\n";
    
    if (tests_passed == tests_run) {
        std::cout << "ALL TESTS PASSED!\n";
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED!\n";
        return 1;
    }
}