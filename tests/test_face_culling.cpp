// Isolated test for face visibility and culling logic
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iomanip>
#include <cmath>

// Test configuration
struct TestConfig {
    glm::vec3 cameraPos;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
};

// Patch data structure
struct Patch {
    int faceIndex;
    std::string name;
    glm::vec3 center;
    glm::vec3 normal;
    glm::vec3 corners[4];
    glm::mat4 transform;
};

// Simulate cube to sphere projection
glm::vec3 cubeToSphere(const glm::vec3& cubePos) {
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
    spherePos.y = cubePos.y * sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
    spherePos.z = cubePos.z * sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
    return glm::normalize(spherePos);
}

// Initialize test configuration matching actual camera
TestConfig initTestConfig() {
    TestConfig config;
    config.cameraPos = glm::vec3(7.136e6f, 3.058e6f, 6.116e6f);
    config.view = glm::lookAt(config.cameraPos, glm::vec3(0,0,0), glm::vec3(0,1,0));
    config.proj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 3512.0f, 7.024e6f);
    config.viewProj = config.proj * config.view;
    return config;
}

// Initialize all 6 cube faces
std::vector<Patch> initAllPatches() {
    std::vector<Patch> patches;
    
    // Face normals and centers for all 6 faces
    struct FaceData {
        int index;
        std::string name;
        glm::vec3 center;
        glm::vec3 normal;
        glm::vec3 right;
        glm::vec3 up;
    } faces[] = {
        {0, "+X", glm::vec3(1,0,0), glm::vec3(1,0,0), glm::vec3(0,0,1), glm::vec3(0,1,0)},
        {1, "-X", glm::vec3(-1,0,0), glm::vec3(-1,0,0), glm::vec3(0,0,-1), glm::vec3(0,1,0)},
        {2, "+Y", glm::vec3(0,1,0), glm::vec3(0,1,0), glm::vec3(1,0,0), glm::vec3(0,0,1)},
        {3, "-Y", glm::vec3(0,-1,0), glm::vec3(0,-1,0), glm::vec3(1,0,0), glm::vec3(0,0,-1)},
        {4, "+Z", glm::vec3(0,0,1), glm::vec3(0,0,1), glm::vec3(1,0,0), glm::vec3(0,1,0)},
        {5, "-Z", glm::vec3(0,0,-1), glm::vec3(0,0,-1), glm::vec3(-1,0,0), glm::vec3(0,1,0)}
    };
    
    for (const auto& face : faces) {
        Patch patch;
        patch.faceIndex = face.index;
        patch.name = face.name;
        patch.center = face.center;
        patch.normal = face.normal;
        
        // Calculate corners
        float halfSize = 1.0f;
        patch.corners[0] = face.center + (-face.right - face.up) * halfSize; // Bottom-left
        patch.corners[1] = face.center + (face.right - face.up) * halfSize;  // Bottom-right
        patch.corners[2] = face.center + (face.right + face.up) * halfSize;  // Top-right
        patch.corners[3] = face.center + (-face.right + face.up) * halfSize; // Top-left
        
        // Build transform matrix
        glm::vec3 bottomLeft = patch.corners[0];
        glm::vec3 bottomRight = patch.corners[1];
        glm::vec3 topLeft = patch.corners[3];
        
        glm::vec3 right = bottomRight - bottomLeft;
        glm::vec3 up = topLeft - bottomLeft;
        
        patch.transform = glm::mat4(1.0f);
        patch.transform[0] = glm::vec4(right, 0.0f);
        patch.transform[1] = glm::vec4(up, 0.0f);
        patch.transform[2] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        patch.transform[3] = glm::vec4(bottomLeft, 1.0f);
        
        patches.push_back(patch);
    }
    
    return patches;
}

// Test 1: Face visibility culling
void testFaceVisibilityCulling(const TestConfig& config, const std::vector<Patch>& patches) {
    std::cout << "\n=== TEST 1: Face Visibility Culling ===" << std::endl;
    std::cout << "Camera position: (" << config.cameraPos.x/1e6 << ", " 
              << config.cameraPos.y/1e6 << ", " << config.cameraPos.z/1e6 << ") million meters" << std::endl;
    
    glm::vec3 toCamera = glm::normalize(config.cameraPos);
    std::cout << "Camera direction (normalized): (" << toCamera.x << ", " 
              << toCamera.y << ", " << toCamera.z << ")" << std::endl;
    
    std::cout << "\nFace visibility test (dot product with camera):" << std::endl;
    for (const auto& patch : patches) {
        float dot = glm::dot(patch.normal, toCamera);
        bool shouldBeVisible = dot > -0.1f;
        
        std::cout << "  " << patch.name << " face: dot=" << std::fixed << std::setprecision(3) << dot;
        std::cout << " -> " << (shouldBeVisible ? "VISIBLE" : "CULLED");
        
        // Check if this matches what we expect
        if (patch.name == "+X" || patch.name == "+Y" || patch.name == "+Z") {
            std::cout << " [Expected: VISIBLE]";
            if (!shouldBeVisible) std::cout << " *** MISMATCH ***";
        }
        std::cout << std::endl;
    }
}

// Test 2: Frustum culling
void testFrustumCulling(const TestConfig& config, const std::vector<Patch>& patches) {
    std::cout << "\n=== TEST 2: Frustum Culling ===" << std::endl;
    
    const float planetRadius = 6.371e6f;
    
    for (const auto& patch : patches) {
        // Test if patch corners are in frustum
        int cornersInFrustum = 0;
        std::cout << "\n" << patch.name << " face frustum test:" << std::endl;
        
        for (int i = 0; i < 4; i++) {
            glm::vec3 worldPos = cubeToSphere(patch.corners[i]) * planetRadius;
            glm::vec4 clipPos = config.viewProj * glm::vec4(worldPos, 1.0f);
            
            if (clipPos.w > 0) {
                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                bool inFrustum = (ndc.x >= -1 && ndc.x <= 1 && 
                                 ndc.y >= -1 && ndc.y <= 1 && 
                                 ndc.z >= 0 && ndc.z <= 1);
                
                std::cout << "  Corner " << i << ": NDC(" << ndc.x << ", " << ndc.y << ", " << ndc.z << ")";
                std::cout << " -> " << (inFrustum ? "IN" : "OUT") << std::endl;
                
                if (inFrustum) cornersInFrustum++;
            } else {
                std::cout << "  Corner " << i << ": Behind camera (w=" << clipPos.w << ")" << std::endl;
            }
        }
        
        std::cout << "  Result: " << cornersInFrustum << "/4 corners in frustum";
        if (cornersInFrustum > 0) {
            std::cout << " -> SHOULD RENDER";
        } else {
            std::cout << " -> SHOULD BE CULLED";
        }
        std::cout << std::endl;
    }
}

// Test 3: Winding order
void testWindingOrder(const std::vector<Patch>& patches) {
    std::cout << "\n=== TEST 3: Winding Order ===" << std::endl;
    std::cout << "Testing if patches have correct counter-clockwise winding when viewed from outside:" << std::endl;
    
    for (const auto& patch : patches) {
        std::cout << "\n" << patch.name << " face winding:" << std::endl;
        
        // Calculate face normal from corners using cross product
        glm::vec3 v0 = patch.corners[1] - patch.corners[0]; // Bottom edge
        glm::vec3 v1 = patch.corners[3] - patch.corners[0]; // Left edge
        glm::vec3 calculatedNormal = glm::normalize(glm::cross(v0, v1));
        
        // Check if calculated normal points outward (same direction as face normal)
        float dot = glm::dot(calculatedNormal, patch.normal);
        
        std::cout << "  Calculated normal: (" << calculatedNormal.x << ", " 
                  << calculatedNormal.y << ", " << calculatedNormal.z << ")" << std::endl;
        std::cout << "  Expected normal: (" << patch.normal.x << ", " 
                  << patch.normal.y << ", " << patch.normal.z << ")" << std::endl;
        std::cout << "  Dot product: " << dot;
        
        if (dot > 0.9f) {
            std::cout << " -> CORRECT CCW winding" << std::endl;
        } else {
            std::cout << " -> INCORRECT winding! ***" << std::endl;
        }
    }
}

// Test 4: Instance data verification
void testInstanceData(const std::vector<Patch>& patches) {
    std::cout << "\n=== TEST 4: Instance Data Verification ===" << std::endl;
    std::cout << "Simulating instance buffer data for visible patches:" << std::endl;
    
    // Only test the 3 visible patches
    std::vector<int> visibleIndices = {0, 2, 4}; // +X, +Y, +Z
    
    for (size_t i = 0; i < visibleIndices.size(); i++) {
        const auto& patch = patches[visibleIndices[i]];
        std::cout << "\nInstance " << i << " (" << patch.name << "):" << std::endl;
        
        // Verify transform matrix
        std::cout << "  Transform matrix:" << std::endl;
        std::cout << "    Right: " << patch.transform[0].x << ", " 
                  << patch.transform[0].y << ", " << patch.transform[0].z << std::endl;
        std::cout << "    Up: " << patch.transform[1].x << ", " 
                  << patch.transform[1].y << ", " << patch.transform[1].z << std::endl;
        std::cout << "    Origin: " << patch.transform[3].x << ", " 
                  << patch.transform[3].y << ", " << patch.transform[3].z << std::endl;
        
        // Test UV mapping
        glm::vec2 testUVs[] = {{0,0}, {1,0}, {1,1}, {0,1}};
        for (int j = 0; j < 4; j++) {
            glm::vec4 localPos(testUVs[j].x, testUVs[j].y, 0.0f, 1.0f);
            glm::vec3 cubePos = glm::vec3(patch.transform * localPos);
            
            float dist = glm::length(cubePos - patch.corners[j]);
            if (dist < 0.01f) {
                std::cout << "  UV(" << testUVs[j].x << "," << testUVs[j].y 
                          << ") -> Corner " << j << " ✓" << std::endl;
            } else {
                std::cout << "  UV(" << testUVs[j].x << "," << testUVs[j].y 
                          << ") -> Wrong position! Distance=" << dist << " ***" << std::endl;
            }
        }
    }
}

// Test 5: Specific +Z face investigation
void testPlusZFace(const TestConfig& config, const std::vector<Patch>& patches) {
    std::cout << "\n=== TEST 5: +Z Face Deep Investigation ===" << std::endl;
    
    const Patch& pzPatch = patches[4]; // +Z face
    std::cout << "Analyzing why +Z face (front-left) might not be rendering:" << std::endl;
    
    // 1. Check basic properties
    std::cout << "\n1. Basic properties:" << std::endl;
    std::cout << "  Center: (" << pzPatch.center.x << ", " << pzPatch.center.y << ", " << pzPatch.center.z << ")" << std::endl;
    std::cout << "  Corners:" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "    [" << i << "]: (" << pzPatch.corners[i].x << ", " 
                  << pzPatch.corners[i].y << ", " << pzPatch.corners[i].z << ")" << std::endl;
    }
    
    // 2. Check visibility from camera
    glm::vec3 toCamera = glm::normalize(config.cameraPos);
    float dot = glm::dot(pzPatch.normal, toCamera);
    std::cout << "\n2. Visibility check:" << std::endl;
    std::cout << "  Dot with camera: " << dot << " (should be > -0.1 for visible)" << std::endl;
    std::cout << "  Result: " << (dot > -0.1f ? "VISIBLE" : "CULLED") << std::endl;
    
    // 3. Check screen space coverage
    std::cout << "\n3. Screen space coverage:" << std::endl;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    const float planetRadius = 6.371e6f;
    
    for (int i = 0; i < 4; i++) {
        glm::vec3 worldPos = cubeToSphere(pzPatch.corners[i]) * planetRadius;
        glm::vec4 clipPos = config.viewProj * glm::vec4(worldPos, 1.0f);
        
        if (clipPos.w > 0) {
            glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
            minX = std::min(minX, ndc.x);
            maxX = std::max(maxX, ndc.x);
            minY = std::min(minY, ndc.y);
            maxY = std::max(maxY, ndc.y);
            
            std::cout << "  Corner " << i << " -> NDC(" << ndc.x << ", " << ndc.y << ")" << std::endl;
        }
    }
    
    std::cout << "  Screen bounds: X[" << minX << " to " << maxX 
              << "] Y[" << minY << " to " << maxY << "]" << std::endl;
    
    // 4. Check if it overlaps with other patches
    std::cout << "\n4. Potential issues:" << std::endl;
    if (minX > 1.0f || maxX < -1.0f || minY > 1.0f || maxY < -1.0f) {
        std::cout << "  - Patch is completely outside screen!" << std::endl;
    }
    if (minX < -1.0f && maxX > -1.0f) {
        std::cout << "  - Patch extends beyond left edge of screen" << std::endl;
    }
    if (minY < -1.0f && maxY > -1.0f) {
        std::cout << "  - Patch extends beyond bottom edge of screen" << std::endl;
    }
}

int main() {
    std::cout << "=== Face Culling Diagnostic Tests ===" << std::endl;
    std::cout << "Testing why front-left (+Z) face is not rendering\n" << std::endl;
    
    TestConfig config = initTestConfig();
    std::vector<Patch> patches = initAllPatches();
    
    testFaceVisibilityCulling(config, patches);
    testFrustumCulling(config, patches);
    testWindingOrder(patches);
    testInstanceData(patches);
    testPlusZFace(config, patches);
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Expected visible faces: +X (right), +Y (top), +Z (front-left)" << std::endl;
    std::cout << "Currently rendering: +X ✓, +Y ✓, +Z ✗" << std::endl;
    std::cout << "\nCheck test results above for potential issues with +Z face." << std::endl;
    
    return 0;
}