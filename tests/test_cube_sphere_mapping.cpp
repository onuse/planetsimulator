#include <gtest/gtest.h>
#include <math/cube_sphere_mapping.hpp>
#include <glm/gtc/epsilon.hpp>
#include <vector>
#include <array>
#include <chrono>

using namespace PlanetSimulator::Math;

class CubeSphereTest : public ::testing::Test {
protected:
    static constexpr float FLOAT_EPSILON = 1e-5f;
    static constexpr double DOUBLE_EPSILON = 1e-10;
    static constexpr float TEST_RADIUS = 6371000.0f;
};

// Test 1: Verify float and double versions produce consistent results
TEST_F(CubeSphereTest, FloatDoubleConsistency) {
    const int numSamples = 100;
    
    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i <= numSamples; ++i) {
            for (int j = 0; j <= numSamples; ++j) {
                float u = static_cast<float>(i) / numSamples;
                float v = static_cast<float>(j) / numSamples;
                
                // Compute with float
                glm::vec3 spherePosF = faceUVToSphereF(face, u, v, TEST_RADIUS);
                
                // Compute with double
                glm::dvec3 spherePosD = faceUVToSphereD(face, 
                    static_cast<double>(u), 
                    static_cast<double>(v), 
                    static_cast<double>(TEST_RADIUS));
                
                // Convert double to float for comparison
                glm::vec3 spherePosDAsFloat(
                    static_cast<float>(spherePosD.x),
                    static_cast<float>(spherePosD.y),
                    static_cast<float>(spherePosD.z)
                );
                
                // They should be very close (within float precision)
                float distance = glm::length(spherePosF - spherePosDAsFloat);
                EXPECT_LT(distance, 1.0f) << "Face " << face << " UV(" << u << ", " << v << ")";
            }
        }
    }
}

// Test 2: Verify boundary vertices are shared exactly
TEST_F(CubeSphereTest, BoundaryVertexSharing) {
    struct EdgeVertex {
        int face;
        float u, v;
    };
    
    // Test edge sharing between faces
    std::vector<std::pair<std::vector<EdgeVertex>, std::string>> edgeGroups = {
        // +X right edge == +Z left edge
        {{{0, 1.0f, 0.5f}, {4, 0.0f, 0.5f}}, "+X/+Z edge"},
        
        // +X top edge == +Y right edge  
        {{{0, 0.5f, 1.0f}, {2, 1.0f, 0.5f}}, "+X/+Y edge"},
        
        // +Y front edge == +Z top edge
        {{{2, 0.5f, 1.0f}, {4, 0.5f, 1.0f}}, "+Y/+Z edge"},
        
        // Corner: +X, +Y, +Z
        {{{0, 1.0f, 1.0f}, {2, 1.0f, 1.0f}, {4, 0.0f, 1.0f}}, "+X/+Y/+Z corner"},
    };
    
    for (const auto& [vertices, description] : edgeGroups) {
        glm::vec3 firstPos = faceUVToSphereF(vertices[0].face, vertices[0].u, vertices[0].v, TEST_RADIUS);
        
        for (size_t i = 1; i < vertices.size(); ++i) {
            glm::vec3 pos = faceUVToSphereF(vertices[i].face, vertices[i].u, vertices[i].v, TEST_RADIUS);
            
            float distance = glm::length(pos - firstPos);
            EXPECT_LT(distance, FLOAT_EPSILON) 
                << description << " vertex " << i << " doesn't match vertex 0"
                << "\nFirst: " << firstPos.x << ", " << firstPos.y << ", " << firstPos.z
                << "\nCurrent: " << pos.x << ", " << pos.y << ", " << pos.z;
        }
    }
}

// Test 3: Verify no gaps at face boundaries
TEST_F(CubeSphereTest, NoGapsAtBoundaries) {
    const int numSamples = 50;
    const float step = 1.0f / numSamples;
    
    // Test pairs of adjacent faces
    struct FaceEdge {
        int face1, face2;
        std::function<std::pair<glm::vec2, glm::vec2>(float)> getUV;
        std::string description;
    };
    
    std::vector<FaceEdge> edges = {
        // +X right edge matches +Z left edge
        {0, 4, [](float t) { return std::make_pair(glm::vec2(1.0f, t), glm::vec2(0.0f, t)); }, "+X/+Z edge"},
        
        // +X top edge matches +Y right edge
        {0, 2, [](float t) { return std::make_pair(glm::vec2(t, 1.0f), glm::vec2(1.0f, t)); }, "+X/+Y edge"},
        
        // +Y front edge matches +Z top edge
        {2, 4, [](float t) { return std::make_pair(glm::vec2(1.0f - t, 1.0f), glm::vec2(t, 1.0f)); }, "+Y/+Z edge"},
    };
    
    for (const auto& edge : edges) {
        for (int i = 0; i <= numSamples; ++i) {
            float t = i * step;
            auto [uv1, uv2] = edge.getUV(t);
            
            glm::vec3 pos1 = faceUVToSphereF(edge.face1, uv1.x, uv1.y, TEST_RADIUS);
            glm::vec3 pos2 = faceUVToSphereF(edge.face2, uv2.x, uv2.y, TEST_RADIUS);
            
            float distance = glm::length(pos1 - pos2);
            EXPECT_LT(distance, FLOAT_EPSILON) 
                << edge.description << " at t=" << t
                << "\nFace " << edge.face1 << " UV(" << uv1.x << ", " << uv1.y << ")"
                << "\nFace " << edge.face2 << " UV(" << uv2.x << ", " << uv2.y << ")";
        }
    }
}

// Test 4: Verify all points lie on sphere surface
TEST_F(CubeSphereTest, AllPointsOnSphere) {
    const int numSamples = 20;
    
    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i <= numSamples; ++i) {
            for (int j = 0; j <= numSamples; ++j) {
                float u = static_cast<float>(i) / numSamples;
                float v = static_cast<float>(j) / numSamples;
                
                glm::vec3 spherePos = faceUVToSphereF(face, u, v, TEST_RADIUS);
                float distance = glm::length(spherePos);
                
                EXPECT_NEAR(distance, TEST_RADIUS, FLOAT_EPSILON)
                    << "Face " << face << " UV(" << u << ", " << v << ")";
            }
        }
    }
}

// Test 5: Test angular distortion characteristics
TEST_F(CubeSphereTest, AngularDistortion) {
    // Test distortion at various points
    struct TestPoint {
        int face;
        float u, v;
        float maxDistortion;  // Maximum acceptable distortion ratio
        std::string description;
    };
    
    std::vector<TestPoint> testPoints = {
        {0, 0.5f, 0.5f, 1.1f, "Face center"},      // Minimal distortion at center
        {0, 0.0f, 0.0f, 1.5f, "Face corner"},      // More distortion at corners
        {0, 0.5f, 0.0f, 1.3f, "Face edge center"},  // Moderate distortion at edge centers
    };
    
    for (const auto& point : testPoints) {
        float distortion = computeAngularDistortion(point.face, point.u, point.v, TEST_RADIUS);
        EXPECT_LT(distortion, point.maxDistortion)
            << point.description << " has excessive distortion: " << distortion;
    }
}

// Test 6: Test vertex cache functionality
TEST_F(CubeSphereTest, VertexCache) {
    CubeSphereCache<float> cache;
    
    // First access should compute and cache
    glm::vec3 pos1 = cache.get(0, 0.5f, 0.5f, TEST_RADIUS);
    EXPECT_EQ(cache.size(), 1);
    
    // Second access should retrieve from cache
    glm::vec3 pos2 = cache.get(0, 0.5f, 0.5f, TEST_RADIUS);
    EXPECT_EQ(cache.size(), 1);
    
    // Results should be identical
    EXPECT_EQ(pos1, pos2);
    
    // Different UV should create new cache entry
    glm::vec3 pos3 = cache.get(0, 0.6f, 0.5f, TEST_RADIUS);
    EXPECT_EQ(cache.size(), 2);
    
    // Clear should empty cache
    cache.clear();
    EXPECT_EQ(cache.size(), 0);
}

// Test 7: Performance characteristics
TEST_F(CubeSphereTest, Performance) {
    const int numIterations = 1000000;
    
    // Test float version performance
    auto startF = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numIterations; ++i) {
        float u = static_cast<float>(i % 100) / 100.0f;
        float v = static_cast<float>((i / 100) % 100) / 100.0f;
        int face = i % 6;
        volatile glm::vec3 pos = faceUVToSphereF(face, u, v, TEST_RADIUS);
    }
    auto endF = std::chrono::high_resolution_clock::now();
    auto durationF = std::chrono::duration_cast<std::chrono::microseconds>(endF - startF);
    
    // Test double version performance
    auto startD = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numIterations; ++i) {
        double u = static_cast<double>(i % 100) / 100.0;
        double v = static_cast<double>((i / 100) % 100) / 100.0;
        int face = i % 6;
        volatile glm::dvec3 pos = faceUVToSphereD(face, u, v, static_cast<double>(TEST_RADIUS));
    }
    auto endD = std::chrono::high_resolution_clock::now();
    auto durationD = std::chrono::duration_cast<std::chrono::microseconds>(endD - startD);
    
    std::cout << "Float version: " << durationF.count() << " microseconds" << std::endl;
    std::cout << "Double version: " << durationD.count() << " microseconds" << std::endl;
    std::cout << "Operations per second (float): " 
              << (numIterations * 1000000.0 / durationF.count()) << std::endl;
}

// Test 8: Boundary snapping behavior
TEST_F(CubeSphereTest, BoundarySnapping) {
    // Values very close to boundaries should snap
    float almostZero = BOUNDARY_EPSILON_F / 2.0f;
    float almostOne = 1.0f - BOUNDARY_EPSILON_F / 2.0f;
    
    // These should all produce the same result as exact boundary values
    glm::vec3 exact0 = faceUVToSphereF(0, 0.0f, 0.5f, TEST_RADIUS);
    glm::vec3 snapped0 = faceUVToSphereF(0, almostZero, 0.5f, TEST_RADIUS);
    EXPECT_EQ(exact0, snapped0);
    
    glm::vec3 exact1 = faceUVToSphereF(0, 1.0f, 0.5f, TEST_RADIUS);
    glm::vec3 snapped1 = faceUVToSphereF(0, almostOne, 0.5f, TEST_RADIUS);
    EXPECT_EQ(exact1, snapped1);
}

// Test 9: Special cases and edge conditions
TEST_F(CubeSphereTest, SpecialCases) {
    // Test all 8 cube corners
    std::array<glm::vec3, 8> corners = {
        faceUVToSphereF(0, 1.0f, 0.0f, TEST_RADIUS),  // +X, -Y, -Z
        faceUVToSphereF(0, 1.0f, 1.0f, TEST_RADIUS),  // +X, +Y, -Z
        faceUVToSphereF(0, 0.0f, 0.0f, TEST_RADIUS),  // +X, -Y, +Z
        faceUVToSphereF(0, 0.0f, 1.0f, TEST_RADIUS),  // +X, +Y, +Z
        faceUVToSphereF(1, 1.0f, 0.0f, TEST_RADIUS),  // -X, -Y, +Z
        faceUVToSphereF(1, 1.0f, 1.0f, TEST_RADIUS),  // -X, +Y, +Z
        faceUVToSphereF(1, 0.0f, 0.0f, TEST_RADIUS),  // -X, -Y, -Z
        faceUVToSphereF(1, 0.0f, 1.0f, TEST_RADIUS),  // -X, +Y, -Z
    };
    
    // All corners should be at the same distance from origin
    float expectedDistance = TEST_RADIUS;
    for (const auto& corner : corners) {
        float distance = glm::length(corner);
        EXPECT_NEAR(distance, expectedDistance, FLOAT_EPSILON);
    }
    
    // Test face centers
    for (int face = 0; face < 6; ++face) {
        glm::vec3 center = faceUVToSphereF(face, 0.5f, 0.5f, TEST_RADIUS);
        float distance = glm::length(center);
        EXPECT_NEAR(distance, TEST_RADIUS, FLOAT_EPSILON);
    }
}

// Test 10: Continuity across boundaries
TEST_F(CubeSphereTest, BoundaryContinuity) {
    const float delta = 0.001f;
    
    // Test continuity by checking that points just inside boundaries
    // are very close to points just outside (on adjacent face)
    
    // Test +X/+Z boundary
    glm::vec3 xInside = faceUVToSphereF(0, 1.0f - delta, 0.5f, TEST_RADIUS);
    glm::vec3 zInside = faceUVToSphereF(4, 0.0f + delta, 0.5f, TEST_RADIUS);
    
    // Points should be very close but not identical (due to slight offset)
    float distance = glm::length(xInside - zInside);
    EXPECT_LT(distance, TEST_RADIUS * 0.001f);  // Within 0.1% of radius
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}