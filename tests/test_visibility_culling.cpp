// Test visibility and culling algorithms
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>

// Test data structure for a patch
struct TestPatch {
    glm::vec3 center;
    glm::vec3 corners[4];
    int faceIndex; // Which cube face (0-5)
    bool shouldBeVisible;
};

// Create view-projection matrix matching the game camera
glm::mat4 createViewProjMatrix(const glm::vec3& cameraPos, const glm::vec3& target, float fov, float aspect) {
    glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 100.0f, 10000000.0f);
    return proj * view;
}

// Check if a point is in the frustum
bool isPointInFrustum(const glm::vec3& point, const glm::mat4& viewProj) {
    glm::vec4 clipSpace = viewProj * glm::vec4(point, 1.0f);
    
    // Perspective divide
    if (clipSpace.w <= 0) return false; // Behind camera
    
    glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
    
    // Check NDC bounds (-1 to 1)
    return ndc.x >= -1.0f && ndc.x <= 1.0f &&
           ndc.y >= -1.0f && ndc.y <= 1.0f &&
           ndc.z >= -1.0f && ndc.z <= 1.0f;
}

// Check if a patch is visible
bool isPatchVisible(const TestPatch& patch, const glm::vec3& cameraPos, const glm::mat4& viewProj) {
    // Check 1: Is the patch facing the camera? (backface culling)
    glm::vec3 toCamera = glm::normalize(cameraPos - patch.center);
    float dot = glm::dot(glm::normalize(patch.center), toCamera);
    
    // For a sphere, if dot < 0, the patch is on the far side
    if (dot < -0.1f) {
        return false; // Backfacing
    }
    
    // Check 2: Is any corner in the frustum?
    bool anyInFrustum = false;
    for (int i = 0; i < 4; i++) {
        if (isPointInFrustum(patch.corners[i], viewProj)) {
            anyInFrustum = true;
            break;
        }
    }
    
    // Check 3: Is the center in frustum?
    if (!anyInFrustum) {
        anyInFrustum = isPointInFrustum(patch.center, viewProj);
    }
    
    return anyInFrustum;
}

// Generate the 6 cube face patches
std::vector<TestPatch> generateCubeFaces(float planetRadius) {
    std::vector<TestPatch> patches;
    
    // Face normals and up vectors for each cube face
    struct FaceInfo {
        glm::vec3 normal;
        glm::vec3 up;
        glm::vec3 right;
    };
    
    FaceInfo faces[6] = {
        { glm::vec3(0, 0, 1),  glm::vec3(0, 1, 0),  glm::vec3(1, 0, 0) },  // +Z (front)
        { glm::vec3(0, 0, -1), glm::vec3(0, 1, 0),  glm::vec3(-1, 0, 0) }, // -Z (back)
        { glm::vec3(1, 0, 0),  glm::vec3(0, 1, 0),  glm::vec3(0, 0, -1) },  // +X (right)
        { glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),  glm::vec3(0, 0, 1) },   // -X (left)
        { glm::vec3(0, 1, 0),  glm::vec3(0, 0, -1), glm::vec3(1, 0, 0) },   // +Y (top)
        { glm::vec3(0, -1, 0), glm::vec3(0, 0, 1),  glm::vec3(1, 0, 0) }    // -Y (bottom)
    };
    
    for (int i = 0; i < 6; i++) {
        TestPatch patch;
        patch.faceIndex = i;
        
        // Center of face on unit cube
        patch.center = faces[i].normal * planetRadius;
        
        // Generate corners (on the sphere)
        float size = planetRadius * 0.9f; // Slightly smaller to see boundaries
        glm::vec3 up = faces[i].up * size;
        glm::vec3 right = faces[i].right * size;
        
        patch.corners[0] = glm::normalize(patch.center - up - right) * planetRadius;
        patch.corners[1] = glm::normalize(patch.center - up + right) * planetRadius;
        patch.corners[2] = glm::normalize(patch.center + up - right) * planetRadius;
        patch.corners[3] = glm::normalize(patch.center + up + right) * planetRadius;
        
        patches.push_back(patch);
    }
    
    return patches;
}

void testCameraPositions() {
    std::cout << "\n=== Testing Different Camera Positions ===" << std::endl;
    
    float planetRadius = 6371000.0f;
    float altitude = 3512000.0f; // Same as in the game
    float distance = planetRadius + altitude;
    
    // Generate cube face patches
    auto patches = generateCubeFaces(planetRadius);
    
    // Test different camera positions
    struct CameraTest {
        glm::vec3 position;
        std::string description;
        int expectedVisible; // How many faces should be visible
    };
    
    std::vector<CameraTest> cameraTests = {
        { glm::vec3(distance, 0, 0), "Looking from +X", 3 },
        { glm::vec3(0, 0, distance), "Looking from +Z", 3 },
        { glm::vec3(distance * 0.707f, 0, distance * 0.707f), "Looking from diagonal XZ", 3 },
        { glm::vec3(distance * 0.577f, distance * 0.577f, distance * 0.577f), "Looking from corner", 3 }
    };
    
    // Camera from the game
    glm::vec3 gameCamera(7.13552e6f, 3.05808e6f, 6.11616e6f);
    cameraTests.push_back({ gameCamera, "Game camera position", 3 });
    
    for (const auto& test : cameraTests) {
        std::cout << "\nCamera: " << test.description << std::endl;
        std::cout << "  Position: (" << test.position.x/1e6 << ", " 
                  << test.position.y/1e6 << ", " << test.position.z/1e6 << ") Mm" << std::endl;
        
        glm::mat4 viewProj = createViewProjMatrix(test.position, glm::vec3(0, 0, 0), 60.0f, 1.77f);
        
        int visibleCount = 0;
        std::cout << "  Visible faces: ";
        for (int i = 0; i < patches.size(); i++) {
            if (isPatchVisible(patches[i], test.position, viewProj)) {
                visibleCount++;
                std::cout << i << " ";
            }
        }
        std::cout << "  (Total: " << visibleCount << ")" << std::endl;
        
        if (visibleCount != test.expectedVisible) {
            std::cout << "  WARNING: Expected " << test.expectedVisible 
                      << " visible faces, got " << visibleCount << std::endl;
        }
    }
}

void testFrustumCulling() {
    std::cout << "\n=== Testing Frustum Culling ===" << std::endl;
    
    // Test with known camera setup from the game
    glm::vec3 cameraPos(7.13552e6f, 3.05808e6f, 6.11616e6f);
    glm::mat4 viewProj = createViewProjMatrix(cameraPos, glm::vec3(0, 0, 0), 60.0f, 1280.0f/720.0f);
    
    // Test points that should definitely be visible/invisible
    struct TestPoint {
        glm::vec3 position;
        std::string description;
        bool shouldBeVisible;
    };
    
    std::vector<TestPoint> testPoints = {
        { glm::vec3(0, 0, 0), "Planet center", true },
        { glm::vec3(6371000, 0, 0), "Planet surface +X", true },
        { glm::vec3(0, 6371000, 0), "Planet surface +Y", true },
        { glm::vec3(0, 0, 6371000), "Planet surface +Z", true },
        { glm::vec3(-6371000, 0, 0), "Planet surface -X", false }, // Behind planet
        { glm::vec3(0, -6371000, 0), "Planet surface -Y", false }, // Behind planet
        { cameraPos + glm::vec3(1000, 0, 0), "Near camera", true },
        { cameraPos * 2.0f, "Far behind camera", false }
    };
    
    std::cout << "Testing frustum culling:" << std::endl;
    for (const auto& test : testPoints) {
        bool inFrustum = isPointInFrustum(test.position, viewProj);
        std::cout << "  " << test.description << ": " 
                  << (inFrustum ? "IN" : "OUT") << " frustum";
        
        if (inFrustum != test.shouldBeVisible) {
            std::cout << " [UNEXPECTED!]";
        }
        std::cout << std::endl;
    }
}

void testBackfaceCulling() {
    std::cout << "\n=== Testing Backface Culling ===" << std::endl;
    
    glm::vec3 cameraPos(7.13552e6f, 3.05808e6f, 6.11616e6f);
    float planetRadius = 6371000.0f;
    
    // Test faces at different positions
    struct FaceTest {
        glm::vec3 faceCenter;
        std::string description;
        bool shouldBeVisible;
    };
    
    std::vector<FaceTest> faceTests = {
        { glm::normalize(cameraPos) * planetRadius, "Face pointing at camera", true },
        { glm::normalize(-cameraPos) * planetRadius, "Face pointing away", false },
        { glm::vec3(planetRadius, 0, 0), "Face on +X", true },
        { glm::vec3(-planetRadius, 0, 0), "Face on -X", false },
        { glm::vec3(0, planetRadius, 0), "Face on +Y", true },
        { glm::vec3(0, -planetRadius, 0), "Face on -Y", false }
    };
    
    std::cout << "Camera position: (" << cameraPos.x/1e6 << ", " 
              << cameraPos.y/1e6 << ", " << cameraPos.z/1e6 << ") Mm" << std::endl;
    
    for (const auto& test : faceTests) {
        glm::vec3 toCamera = glm::normalize(cameraPos - test.faceCenter);
        glm::vec3 faceNormal = glm::normalize(test.faceCenter);
        float dot = glm::dot(faceNormal, toCamera);
        
        bool facing = dot > 0;
        std::cout << "  " << test.description << ": dot=" << std::fixed 
                  << std::setprecision(3) << dot << " -> " 
                  << (facing ? "VISIBLE" : "CULLED");
        
        if (facing != test.shouldBeVisible) {
            std::cout << " [UNEXPECTED!]";
        }
        std::cout << std::endl;
    }
}

void analyzeGameCamera() {
    std::cout << "\n=== Analyzing Game Camera Setup ===" << std::endl;
    
    // Values from the debug output
    glm::vec3 cameraPos(7.13552e6f, 3.05808e6f, 6.11616e6f);
    float planetRadius = 6371000.0f;
    
    float distance = glm::length(cameraPos);
    float altitude = distance - planetRadius;
    
    std::cout << "Camera analysis:" << std::endl;
    std::cout << "  Position: (" << cameraPos.x/1e6 << ", " 
              << cameraPos.y/1e6 << ", " << cameraPos.z/1e6 << ") Mm" << std::endl;
    std::cout << "  Distance from origin: " << distance/1e6 << " Mm" << std::endl;
    std::cout << "  Altitude: " << altitude/1e6 << " Mm" << std::endl;
    std::cout << "  Direction: " << glm::normalize(cameraPos).x << ", "
              << glm::normalize(cameraPos).y << ", "
              << glm::normalize(cameraPos).z << std::endl;
    
    // Which cube faces should be visible?
    std::cout << "\nExpected visible cube faces from this position:" << std::endl;
    std::string faceNames[6] = {"+Z", "-Z", "+X", "-X", "+Y", "-Y"};
    glm::vec3 faceNormals[6] = {
        glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
        glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
        glm::vec3(0, 1, 0), glm::vec3(0, -1, 0)
    };
    
    glm::vec3 viewDir = glm::normalize(-cameraPos);
    for (int i = 0; i < 6; i++) {
        float dot = glm::dot(faceNormals[i], viewDir);
        std::cout << "  Face " << i << " (" << faceNames[i] << "): dot=" 
                  << dot << " -> " << (dot > -0.3f ? "VISIBLE" : "HIDDEN") << std::endl;
    }
}

int main() {
    std::cout << "=== Visibility and Culling Tests ===" << std::endl;
    
    try {
        analyzeGameCamera();
        testBackfaceCulling();
        testFrustumCulling();
        testCameraPositions();
        
        std::cout << "\n=== TESTS COMPLETE ===" << std::endl;
        std::cout << "\nSummary: The game camera at (7.1, 3.1, 6.1) Mm should see faces:" << std::endl;
        std::cout << "  - Face 0 (+Z): Likely visible" << std::endl;
        std::cout << "  - Face 2 (+X): Likely visible" << std::endl;
        std::cout << "  - Face 4 (+Y): Partially visible" << std::endl;
        std::cout << "  - Faces 1,3,5 (-Z,-X,-Y): Should be culled" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}