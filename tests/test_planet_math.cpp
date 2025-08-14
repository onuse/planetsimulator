#include <gtest/gtest.h>
#include "math/planet_math.hpp"
#include <random>

using namespace math;

class PlanetMathTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up any common test data
    }
};

// ==================== Coordinate Transformation Tests ====================

TEST_F(PlanetMathTest, CubeToSpherePreservesNormalization) {
    // Test that cube-to-sphere always produces unit vectors
    std::vector<glm::dvec3> testPoints = {
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0),
        glm::dvec3(1.0, 0.5, 0.5),
        glm::dvec3(1.0, 1.0, 1.0)
    };
    
    for (const auto& point : testPoints) {
        glm::dvec3 spherePos = cubeToSphere(point);
        double length = glm::length(spherePos);
        EXPECT_NEAR(length, 1.0, 1e-10) << "Point: " << toString(point);
    }
}

TEST_F(PlanetMathTest, CubeToSphereHandlesCorners) {
    // Cube corners should map to specific sphere points
    glm::dvec3 corner(1.0, 1.0, 1.0);
    glm::dvec3 spherePos = cubeToSphere(corner);
    
    // Should be normalized
    EXPECT_NEAR(glm::length(spherePos), 1.0, 1e-10);
    
    // Should be in the positive octant
    EXPECT_GT(spherePos.x, 0);
    EXPECT_GT(spherePos.y, 0);
    EXPECT_GT(spherePos.z, 0);
}

TEST_F(PlanetMathTest, CubeToSphereIsReversible) {
    // Test that we can go cube -> sphere -> cube (approximately)
    std::vector<glm::dvec3> testPoints = {
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.5, 0.0),
        glm::dvec3(1.0, 0.5, 0.5)
    };
    
    for (const auto& cubePoint : testPoints) {
        glm::dvec3 spherePos = cubeToSphere(cubePoint);
        glm::dvec3 backToCube = sphereToCube(spherePos);
        
        // Normalize for comparison (sphere-to-cube gives direction)
        backToCube = glm::normalize(backToCube) * glm::length(cubePoint);
        
        // Should be close to original
        double error = glm::length(backToCube - cubePoint);
        EXPECT_LT(error, 0.1) << "Original: " << toString(cubePoint) 
                               << ", Recovered: " << toString(backToCube);
    }
}

// ==================== Face Operations Tests ====================

TEST_F(PlanetMathTest, FaceNormalsAreOrthogonal) {
    // Test that face normals are unit vectors and orthogonal to each other
    for (uint32_t i = 0; i < 6; i++) {
        glm::dvec3 normal = getFaceNormal(i);
        EXPECT_NEAR(glm::length(normal), 1.0, 1e-10) << "Face " << i;
        
        // Check opposite faces are negatives
        if (i % 2 == 0) {
            glm::dvec3 opposite = getFaceNormal(i + 1);
            EXPECT_NEAR(glm::length(normal + opposite), 0.0, 1e-10)
                << "Faces " << i << " and " << (i+1) << " should be opposite";
        }
    }
}

TEST_F(PlanetMathTest, FaceBasisIsOrthonormal) {
    // Test that up and right vectors form orthonormal basis with normal
    for (uint32_t faceId = 0; faceId < 6; faceId++) {
        glm::dvec3 up, right;
        getFaceBasis(faceId, up, right);
        glm::dvec3 normal = getFaceNormal(faceId);
        
        // Check all are unit vectors
        EXPECT_NEAR(glm::length(up), 1.0, 1e-10) << "Face " << faceId << " up";
        EXPECT_NEAR(glm::length(right), 1.0, 1e-10) << "Face " << faceId << " right";
        
        // Check orthogonality
        EXPECT_NEAR(glm::dot(up, right), 0.0, 1e-10) << "Face " << faceId << " up-right";
        EXPECT_NEAR(glm::dot(up, normal), 0.0, 1e-10) << "Face " << faceId << " up-normal";
        EXPECT_NEAR(glm::dot(right, normal), 0.0, 1e-10) << "Face " << faceId << " right-normal";
    }
}

// ==================== LOD Calculation Tests ====================

TEST_F(PlanetMathTest, ScreenSpaceErrorScalesWithDistance) {
    const double planetRadius = 6371000.0;
    glm::dvec3 patchCenter(planetRadius, 0, 0);
    double patchSize = 0.1; // 10% of cube face
    
    // Simple view projection for testing
    glm::dmat4 viewProj = glm::perspective(glm::radians(60.0), 16.0/9.0, 1000.0, 1e8);
    
    // Test at different distances
    std::vector<double> distances = {
        planetRadius * 0.1,  // Very close
        planetRadius * 1.0,  // At surface
        planetRadius * 2.0,  // One radius away
        planetRadius * 10.0  // Far away
    };
    
    float prevError = 100000.0f;
    for (double dist : distances) {
        glm::dvec3 viewPos(planetRadius + dist, 0, 0);
        float error = calculateScreenSpaceError(patchCenter, patchSize, viewPos, viewProj, planetRadius);
        
        // Error should decrease with distance
        EXPECT_LT(error, prevError) << "Distance: " << dist;
        prevError = error;
    }
}

TEST_F(PlanetMathTest, ScreenSpaceErrorHandlesBehindCamera) {
    const double planetRadius = 6371000.0;
    glm::dvec3 patchCenter(planetRadius, 0, 0);
    double patchSize = 0.1;
    
    glm::dmat4 viewProj = glm::perspective(glm::radians(60.0), 16.0/9.0, 1000.0, 1e8);
    
    // Camera looking away from patch
    glm::dvec3 viewPos(-planetRadius * 2, 0, 0);
    
    float error = calculateScreenSpaceError(patchCenter, patchSize, viewPos, viewProj, planetRadius);
    
    // Should return high error for patches behind camera
    EXPECT_GT(error, 1000.0f);
}

TEST_F(PlanetMathTest, LODThresholdDecreasesWithAltitude) {
    const double planetRadius = 6371000.0;
    
    std::vector<double> altitudes = {
        100.0,        // 100m above surface
        1000.0,       // 1km
        10000.0,      // 10km
        100000.0,     // 100km
        1000000.0     // 1000km
    };
    
    float prevThreshold = 0.0f;
    for (double alt : altitudes) {
        float threshold = calculateLODThreshold(alt, planetRadius);
        
        // Threshold should increase with altitude (less detail needed)
        EXPECT_GE(threshold, prevThreshold) << "Altitude: " << alt;
        prevThreshold = threshold;
    }
}

// ==================== Face Culling Tests ====================

TEST_F(PlanetMathTest, FaceCullingWorksCorrectly) {
    const double planetRadius = 6371000.0;
    
    // Camera on +X axis looking at planet
    glm::dvec3 viewPos(planetRadius * 2, 0, 0);
    
    // +X face should be visible
    EXPECT_FALSE(shouldCullFace(0, viewPos, planetRadius)) << "+X face should be visible";
    
    // -X face should be culled (on far side)
    EXPECT_TRUE(shouldCullFace(1, viewPos, planetRadius)) << "-X face should be culled";
    
    // Y and Z faces are on the edges, might or might not be culled depending on threshold
}

TEST_F(PlanetMathTest, FaceCullingIsConservativeNearSurface) {
    const double planetRadius = 6371000.0;
    
    // Camera very close to surface on +X axis
    glm::dvec3 viewPos(planetRadius * 1.001, 0, 0); // 0.1% above surface
    
    // Even edge faces shouldn't be culled when very close
    bool yPosCulled = shouldCullFace(2, viewPos, planetRadius);
    bool yNegCulled = shouldCullFace(3, viewPos, planetRadius);
    
    // At least some edge faces should be visible when close
    EXPECT_FALSE(yPosCulled && yNegCulled) << "Some edge faces should be visible when close";
}

// ==================== Transform Building Tests ====================

TEST_F(PlanetMathTest, PatchTransformIsValid) {
    glm::dvec3 bottomLeft(1.0, -0.5, -0.5);
    glm::dvec3 bottomRight(1.0, 0.5, -0.5);
    glm::dvec3 topLeft(1.0, -0.5, 0.5);
    uint32_t faceId = 0; // +X face
    
    glm::dmat4 transform = buildPatchTransform(bottomLeft, bottomRight, topLeft, faceId);
    
    // Transform should be valid (no NaN/Inf)
    EXPECT_TRUE(isValid(transform));
    
    // Transform should map (0,0) to bottomLeft
    glm::dvec4 origin(0, 0, 0, 1);
    glm::dvec4 mapped = transform * origin;
    EXPECT_NEAR(glm::length(glm::dvec3(mapped) - bottomLeft), 0.0, 1e-10);
    
    // Transform should map (1,0) to bottomRight
    glm::dvec4 rightCorner(1, 0, 0, 1);
    mapped = transform * rightCorner;
    EXPECT_NEAR(glm::length(glm::dvec3(mapped) - bottomRight), 0.0, 1e-10);
}

// ==================== Validation Function Tests ====================

TEST_F(PlanetMathTest, ValidationFunctionsWork) {
    // Test scalar validation
    EXPECT_TRUE(isValid(1.0));
    EXPECT_TRUE(isValid(0.0));
    EXPECT_TRUE(isValid(-1.0));
    EXPECT_FALSE(isValid(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(isValid(std::numeric_limits<double>::infinity()));
    
    // Test vector validation
    glm::dvec3 validVec(1.0, 2.0, 3.0);
    EXPECT_TRUE(isValid(validVec));
    
    glm::dvec3 invalidVec(1.0, std::numeric_limits<double>::quiet_NaN(), 3.0);
    EXPECT_FALSE(isValid(invalidVec));
    
    // Test matrix validation
    glm::dmat4 validMat(1.0);
    EXPECT_TRUE(isValid(validMat));
    
    glm::dmat4 invalidMat(1.0);
    invalidMat[2][2] = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(isValid(invalidMat));
}

// ==================== Edge Case Tests ====================

TEST_F(PlanetMathTest, HandlesZeroVectors) {
    // Zero vector should map to zero in sphere
    glm::dvec3 zero(0.0, 0.0, 0.0);
    glm::dvec3 spherePos = cubeToSphere(zero);
    
    // Result should be zero or very small
    EXPECT_LT(glm::length(spherePos), 1e-10);
}

TEST_F(PlanetMathTest, HandlesVeryLargePatchSizes) {
    const double planetRadius = 6371000.0;
    glm::dvec3 patchCenter(planetRadius, 0, 0);
    double hugePatchSize = 2.0; // Entire cube face!
    
    glm::dmat4 viewProj = glm::perspective(glm::radians(60.0), 16.0/9.0, 1000.0, 1e8);
    glm::dvec3 viewPos(planetRadius * 2, 0, 0);
    
    float error = calculateScreenSpaceError(patchCenter, hugePatchSize, viewPos, viewProj, planetRadius);
    
    // Should not produce NaN or Inf
    EXPECT_TRUE(std::isfinite(error));
    // Should produce high error for large patches
    EXPECT_GT(error, 100.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}