// Test patch coverage to understand why only lower-right is visible
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iomanip>
#include <cmath>

// Simulate the cube-to-sphere mapping
glm::vec3 cubeToSphere(const glm::vec3& cubePos) {
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
    spherePos.y = cubePos.y * sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
    spherePos.z = cubePos.z * sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
    return glm::normalize(spherePos);
}

struct Patch {
    glm::vec3 center;
    glm::vec3 corners[4];
    std::string name;
};

void analyzePatchCoverage() {
    std::cout << "\n=== Analyzing Patch Coverage ===" << std::endl;
    
    // Create the 3 visible patches from the camera position
    std::vector<Patch> patches;
    
    // Patch 0: +X face (center at 1,0,0)
    Patch patchX;
    patchX.name = "+X";
    patchX.center = glm::vec3(1, 0, 0);
    patchX.corners[0] = glm::vec3(1, -1, -1);
    patchX.corners[1] = glm::vec3(1, -1, 1);
    patchX.corners[2] = glm::vec3(1, 1, 1);
    patchX.corners[3] = glm::vec3(1, 1, -1);
    patches.push_back(patchX);
    
    // Patch 1: +Y face (center at 0,1,0)
    Patch patchY;
    patchY.name = "+Y";
    patchY.center = glm::vec3(0, 1, 0);
    patchY.corners[0] = glm::vec3(-1, 1, -1);
    patchY.corners[1] = glm::vec3(1, 1, -1);
    patchY.corners[2] = glm::vec3(1, 1, 1);
    patchY.corners[3] = glm::vec3(-1, 1, 1);
    patches.push_back(patchY);
    
    // Patch 2: +Z face (center at 0,0,1)
    Patch patchZ;
    patchZ.name = "+Z";
    patchZ.center = glm::vec3(0, 0, 1);
    patchZ.corners[0] = glm::vec3(1, -1, 1);
    patchZ.corners[1] = glm::vec3(-1, -1, 1);
    patchZ.corners[2] = glm::vec3(-1, 1, 1);
    patchZ.corners[3] = glm::vec3(1, 1, 1);
    patches.push_back(patchZ);
    
    // Analyze each patch
    for (const auto& patch : patches) {
        std::cout << "\nPatch " << patch.name << ":" << std::endl;
        std::cout << "  Center (cube): " << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << std::endl;
        
        glm::vec3 sphereCenter = cubeToSphere(patch.center);
        std::cout << "  Center (sphere): " << sphereCenter.x << ", " << sphereCenter.y << ", " << sphereCenter.z << std::endl;
        
        std::cout << "  Corners (cube -> sphere):" << std::endl;
        for (int i = 0; i < 4; i++) {
            glm::vec3 sphereCorner = cubeToSphere(patch.corners[i]);
            std::cout << "    [" << i << "] (" << patch.corners[i].x << "," << patch.corners[i].y << "," << patch.corners[i].z 
                      << ") -> (" << sphereCorner.x << "," << sphereCorner.y << "," << sphereCorner.z << ")" << std::endl;
        }
        
        // Check coverage in UV space (what portion of a unit quad would this cover?)
        std::cout << "  Coverage analysis:" << std::endl;
        
        // For each patch, check which quadrant it covers when viewed from camera
        glm::vec3 cameraPos(7.136f, 3.058f, 6.116f);
        glm::vec3 cameraDir = glm::normalize(-cameraPos);
        
        // Project corners to screen space (simplified)
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
        glm::mat4 viewProj = proj * view;
        
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (int i = 0; i < 4; i++) {
            glm::vec3 worldPos = cubeToSphere(patch.corners[i]) * 6.371f; // Scale to planet size (simplified)
            glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
            if (clipPos.w > 0) {
                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                minX = std::min(minX, ndc.x);
                maxX = std::max(maxX, ndc.x);
                minY = std::min(minY, ndc.y);
                maxY = std::max(maxY, ndc.y);
            }
        }
        
        std::cout << "    Screen bounds (NDC): X[" << minX << " to " << maxX 
                  << "] Y[" << minY << " to " << maxY << "]" << std::endl;
    }
}

void testUVMapping() {
    std::cout << "\n=== Testing UV to World Mapping ===" << std::endl;
    
    // Test how UV coordinates (0-1) map through the transform
    std::cout << "\nFor +X face patch:" << std::endl;
    
    // Build transform as in the LOD manager
    glm::vec3 bottomLeft(1, -1, -1);
    glm::vec3 bottomRight(1, -1, 1);
    glm::vec3 topLeft(1, 1, -1);
    
    glm::vec3 right = bottomRight - bottomLeft;  // (0, 0, 2)
    glm::vec3 up = topLeft - bottomLeft;         // (0, 2, 0)
    
    std::cout << "  Right vector: " << right.x << ", " << right.y << ", " << right.z << std::endl;
    std::cout << "  Up vector: " << up.x << ", " << up.y << ", " << up.z << std::endl;
    std::cout << "  Origin: " << bottomLeft.x << ", " << bottomLeft.y << ", " << bottomLeft.z << std::endl;
    
    // Test UV corners
    struct UVTest {
        glm::vec2 uv;
        std::string name;
    };
    
    std::vector<UVTest> testPoints = {
        {{0, 0}, "Bottom-left"},
        {{1, 0}, "Bottom-right"},
        {{1, 1}, "Top-right"},
        {{0, 1}, "Top-left"},
        {{0.5f, 0.5f}, "Center"}
    };
    
    for (const auto& test : testPoints) {
        // Transform UV to cube position
        glm::vec3 cubePos = bottomLeft + right * test.uv.x + up * test.uv.y;
        glm::vec3 spherePos = cubeToSphere(cubePos);
        
        std::cout << "  UV(" << test.uv.x << "," << test.uv.y << ") [" << test.name << "]" << std::endl;
        std::cout << "    -> Cube(" << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")" << std::endl;
        std::cout << "    -> Sphere(" << spherePos.x << "," << spherePos.y << "," << spherePos.z << ")" << std::endl;
    }
}

void checkPatchOverlap() {
    std::cout << "\n=== Checking Patch Overlap ===" << std::endl;
    
    // Check if the 3 patches share edges properly
    std::cout << "\n+X face edges:" << std::endl;
    std::cout << "  Top edge: (1,1,-1) to (1,1,1)" << std::endl;
    std::cout << "  Bottom edge: (1,-1,-1) to (1,-1,1)" << std::endl;
    
    std::cout << "\n+Y face edges:" << std::endl;
    std::cout << "  Right edge: (1,1,-1) to (1,1,1)" << std::endl;  // Shared with +X top
    std::cout << "  Front edge: (-1,1,1) to (1,1,1)" << std::endl;
    
    std::cout << "\n+Z face edges:" << std::endl;
    std::cout << "  Top edge: (-1,1,1) to (1,1,1)" << std::endl;   // Shared with +Y front
    std::cout << "  Right edge: (1,-1,1) to (1,1,1)" << std::endl; // Shared with +X right
    
    std::cout << "\nShared corners:" << std::endl;
    std::cout << "  (1,1,1) - shared by all three faces" << std::endl;
    std::cout << "  (1,1,-1) - shared by +X and +Y" << std::endl;
    std::cout << "  (1,-1,1) - shared by +X and +Z" << std::endl;
    std::cout << "  (-1,1,1) - shared by +Y and +Z" << std::endl;
}

int main() {
    std::cout << "=== Patch Coverage Analysis ===" << std::endl;
    
    analyzePatchCoverage();
    testUVMapping();
    checkPatchOverlap();
    
    std::cout << "\n=== Analysis Complete ===" << std::endl;
    std::cout << "\nKey findings:" << std::endl;
    std::cout << "1. The 3 patches (+X, +Y, +Z) should meet at corner (1,1,1)" << std::endl;
    std::cout << "2. From camera at (7.1, 3.1, 6.1), we're looking mostly at the +X/+Y/+Z corner" << std::endl;
    std::cout << "3. If only lower-right is visible, the patches may not be transforming correctly" << std::endl;
    
    return 0;
}