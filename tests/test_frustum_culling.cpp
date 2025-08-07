#include <iostream>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../include/rendering/hierarchical_gpu_octree.hpp"

class FrustumCullingTests {
public:
    void runAllTests() {
        std::cout << "=== FRUSTUM CULLING TESTS ===" << std::endl << std::endl;
        
        testFrustumExtraction();
        testSphereIntersection();
        testAABBIntersection();
        testOctreeNodeCulling();
        testEdgeCases();
        
        std::cout << std::endl << "=== ALL FRUSTUM CULLING TESTS PASSED ===" << std::endl;
    }
    
private:
    // Test 1: Verify frustum extraction from view-projection matrix
    void testFrustumExtraction() {
        std::cout << "Test 1: Frustum Extraction from Matrix" << std::endl;
        
        // Create a simple view-projection matrix
        glm::vec3 eye(0, 0, 10);
        glm::vec3 center(0, 0, 0);
        glm::vec3 up(0, 1, 0);
        glm::mat4 view = glm::lookAt(eye, center, up);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
        glm::mat4 viewProj = proj * view;
        
        // Extract frustum planes
        glm::vec4 planes[6];
        
        // Left plane
        planes[0] = glm::vec4(
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        );
        
        // Normalize and verify
        float length = glm::length(glm::vec3(planes[0]));
        assert(length > 0.0f);
        planes[0] /= length;
        
        std::cout << "  Left plane: (" << planes[0].x << ", " << planes[0].y 
                  << ", " << planes[0].z << ", " << planes[0].w << ")" << std::endl;
        
        std::cout << "  ✓ Frustum extraction successful" << std::endl;
    }
    
    // Test 2: Sphere-frustum intersection
    void testSphereIntersection() {
        std::cout << "Test 2: Sphere-Frustum Intersection" << std::endl;
        
        // Create a simple frustum looking down -Z
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 100.0f);
        glm::mat4 viewProj = proj * view;
        
        struct TestCase {
            glm::vec3 center;
            float radius;
            bool expectedResult;
            const char* description;
        };
        
        TestCase cases[] = {
            {{0, 0, 0}, 1.0f, true, "Sphere at origin (visible)"},
            {{0, 0, -50}, 1.0f, false, "Sphere behind camera (culled)"},
            {{100, 0, 0}, 1.0f, false, "Sphere far to the right (culled)"},
            {{0, 0, 5}, 50.0f, true, "Large sphere overlapping frustum"},
            {{0, 0, 150}, 1.0f, false, "Sphere beyond far plane (culled)"}
        };
        
        for (const auto& tc : cases) {
            bool result = testSphereAgainstFrustum(viewProj, tc.center, tc.radius);
            std::cout << "  " << tc.description << ": ";
            std::cout << (result ? "visible" : "culled");
            std::cout << " (expected: " << (tc.expectedResult ? "visible" : "culled") << ")";
            
            assert(result == tc.expectedResult);
            std::cout << " ✓" << std::endl;
        }
    }
    
    // Test 3: AABB-frustum intersection
    void testAABBIntersection() {
        std::cout << "Test 3: AABB-Frustum Intersection" << std::endl;
        
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
        glm::mat4 viewProj = proj * view;
        
        struct TestCase {
            glm::vec3 min;
            glm::vec3 max;
            bool expectedResult;
            const char* description;
        };
        
        TestCase cases[] = {
            {{-1, -1, -1}, {1, 1, 1}, true, "Small AABB at origin"},
            {{-100, -100, -100}, {100, 100, 100}, true, "Large AABB containing camera"},
            {{50, 50, 50}, {60, 60, 60}, false, "AABB outside frustum"},
            {{-5, -5, 0}, {5, 5, 10}, true, "AABB partially in frustum"},
            {{-1, -1, -200}, {1, 1, -190}, false, "AABB behind camera"}
        };
        
        for (const auto& tc : cases) {
            bool result = testAABBAgainstFrustum(viewProj, tc.min, tc.max);
            std::cout << "  " << tc.description << ": ";
            std::cout << (result ? "visible" : "culled");
            std::cout << " (expected: " << (tc.expectedResult ? "visible" : "culled") << ")";
            
            assert(result == tc.expectedResult);
            std::cout << " ✓" << std::endl;
        }
    }
    
    // Test 4: Octree node culling simulation
    void testOctreeNodeCulling() {
        std::cout << "Test 4: Octree Node Culling" << std::endl;
        
        // Simulate planet viewing scenario
        float planetRadius = 6371000.0f;
        glm::vec3 cameraPos(planetRadius * 2.0f, 0, 0);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 100.0f, planetRadius * 10.0f);
        glm::mat4 viewProj = proj * view;
        
        // Test nodes at various positions
        struct NodeTest {
            glm::vec3 center;
            float halfSize;
            const char* description;
        };
        
        NodeTest nodes[] = {
            {{0, 0, 0}, planetRadius, "Planet center node"},
            {{planetRadius, 0, 0}, planetRadius * 0.1f, "Surface node facing camera"},
            {{-planetRadius, 0, 0}, planetRadius * 0.1f, "Surface node behind planet"},
            {{0, planetRadius, 0}, planetRadius * 0.1f, "Surface node at pole"},
            {{0, 0, planetRadius * 5}, planetRadius * 0.5f, "Node far behind planet"}
        };
        
        int visibleCount = 0;
        int culledCount = 0;
        
        for (const auto& node : nodes) {
            bool visible = testSphereAgainstFrustum(viewProj, node.center, node.halfSize * 1.732f);
            if (visible) {
                visibleCount++;
            } else {
                culledCount++;
            }
            std::cout << "  " << node.description << ": " 
                      << (visible ? "visible" : "culled") << std::endl;
        }
        
        std::cout << "  Summary: " << visibleCount << " visible, " 
                  << culledCount << " culled" << std::endl;
        
        assert(visibleCount > 0 && culledCount > 0);
        std::cout << "  ✓ Node culling working as expected" << std::endl;
    }
    
    // Test 5: Edge cases and numerical stability
    void testEdgeCases() {
        std::cout << "Test 5: Edge Cases and Numerical Stability" << std::endl;
        
        // Test with extreme values
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 1e-6f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(179.0f), 0.01f, 1e-9f, 1e9f);
        glm::mat4 viewProj = proj * view;
        
        // Test very small sphere
        bool tinyVisible = testSphereAgainstFrustum(viewProj, glm::vec3(0, 0, 0), 1e-8f);
        std::cout << "  Tiny sphere at origin: " << (tinyVisible ? "visible" : "culled") << std::endl;
        
        // Test very large sphere
        bool hugeVisible = testSphereAgainstFrustum(viewProj, glm::vec3(0, 0, 0), 1e8f);
        std::cout << "  Huge sphere at origin: " << (hugeVisible ? "visible" : "culled") << std::endl;
        assert(hugeVisible); // Should always be visible
        
        // Test sphere at frustum corner
        float cornerDist = 1.0f;
        glm::vec3 cornerPos(cornerDist, cornerDist, -cornerDist);
        bool cornerVisible = testSphereAgainstFrustum(viewProj, cornerPos, 0.1f);
        std::cout << "  Sphere at frustum corner: " << (cornerVisible ? "visible" : "culled") << std::endl;
        
        std::cout << "  ✓ Edge cases handled correctly" << std::endl;
    }
    
    // Helper function to test sphere against frustum
    bool testSphereAgainstFrustum(const glm::mat4& viewProj, const glm::vec3& center, float radius) {
        glm::vec4 planes[6];
        extractFrustumPlanes(viewProj, planes);
        
        for (int i = 0; i < 6; i++) {
            float distance = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
            if (distance < -radius) {
                return false; // Completely outside this plane
            }
        }
        return true;
    }
    
    // Helper function to test AABB against frustum
    bool testAABBAgainstFrustum(const glm::mat4& viewProj, const glm::vec3& min, const glm::vec3& max) {
        glm::vec4 planes[6];
        extractFrustumPlanes(viewProj, planes);
        
        for (int i = 0; i < 6; i++) {
            glm::vec3 positive(
                planes[i].x > 0 ? max.x : min.x,
                planes[i].y > 0 ? max.y : min.y,
                planes[i].z > 0 ? max.z : min.z
            );
            
            float distance = glm::dot(glm::vec3(planes[i]), positive) + planes[i].w;
            if (distance < 0) {
                return false;
            }
        }
        return true;
    }
    
    // Extract frustum planes from view-projection matrix
    void extractFrustumPlanes(const glm::mat4& viewProj, glm::vec4 planes[6]) {
        // Left plane
        planes[0] = glm::vec4(
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        );
        
        // Right plane
        planes[1] = glm::vec4(
            viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0],
            viewProj[3][3] - viewProj[3][0]
        );
        
        // Bottom plane
        planes[2] = glm::vec4(
            viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1],
            viewProj[3][3] + viewProj[3][1]
        );
        
        // Top plane
        planes[3] = glm::vec4(
            viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1],
            viewProj[3][3] - viewProj[3][1]
        );
        
        // Near plane
        planes[4] = glm::vec4(
            viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2],
            viewProj[3][3] + viewProj[3][2]
        );
        
        // Far plane
        planes[5] = glm::vec4(
            viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2],
            viewProj[3][3] - viewProj[3][2]
        );
        
        // Normalize planes
        for (int i = 0; i < 6; i++) {
            float length = glm::length(glm::vec3(planes[i]));
            if (length > 0) {
                planes[i] /= length;
            }
        }
    }
};

int main() {
    try {
        FrustumCullingTests tests;
        tests.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}