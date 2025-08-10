// Test UV mapping after fixing the shader
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

void testCorrectedUVMapping() {
    std::cout << "\n=== Testing Corrected UV Mapping ===\n" << std::endl;
    
    // For each of the 3 visible patches, test the UV mapping
    struct Patch {
        std::string name;
        glm::vec3 corners[4];
    };
    
    std::vector<Patch> patches = {
        {"+X", {glm::vec3(1,-1,-1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(1,1,-1)}},
        {"+Y", {glm::vec3(-1,1,-1), glm::vec3(1,1,-1), glm::vec3(1,1,1), glm::vec3(-1,1,1)}},
        {"+Z", {glm::vec3(-1,-1,1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(-1,1,1)}}
    };
    
    for (const auto& patch : patches) {
        std::cout << "\nPatch " << patch.name << ":" << std::endl;
        
        // Build transform as in LOD manager (corrected version)
        glm::vec3 bottomLeft = patch.corners[0];
        glm::vec3 bottomRight = patch.corners[1];
        glm::vec3 topLeft = patch.corners[3];
        
        glm::vec3 right = bottomRight - bottomLeft;
        glm::vec3 up = topLeft - bottomLeft;
        
        glm::mat4 transform(1.0f);
        transform[0] = glm::vec4(right, 0.0f);
        transform[1] = glm::vec4(up, 0.0f);
        transform[2] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        transform[3] = glm::vec4(bottomLeft, 1.0f);
        
        std::cout << "  Transform basis:" << std::endl;
        std::cout << "    Right: " << right.x << ", " << right.y << ", " << right.z << std::endl;
        std::cout << "    Up: " << up.x << ", " << up.y << ", " << up.z << std::endl;
        std::cout << "    Origin: " << bottomLeft.x << ", " << bottomLeft.y << ", " << bottomLeft.z << std::endl;
        
        // Test UV corners with CORRECTED shader logic
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
            // NEW shader logic: UV stays in 0-1 range
            glm::vec4 localPos(test.uv.x, test.uv.y, 0.0f, 1.0f);
            glm::vec3 cubePos = glm::vec3(transform * localPos);
            glm::vec3 spherePos = cubeToSphere(cubePos);
            
            std::cout << "  UV(" << test.uv.x << "," << test.uv.y << ") [" << test.name << "]" << std::endl;
            std::cout << "    -> Cube(" << cubePos.x << "," << cubePos.y << "," << cubePos.z << ")";
            
            // Check if this matches expected corner
            bool matchesExpected = false;
            if (test.name == "Bottom-left" && glm::length(cubePos - patch.corners[0]) < 0.01f) matchesExpected = true;
            if (test.name == "Bottom-right" && glm::length(cubePos - patch.corners[1]) < 0.01f) matchesExpected = true;
            if (test.name == "Top-right" && glm::length(cubePos - patch.corners[2]) < 0.01f) matchesExpected = true;
            if (test.name == "Top-left" && glm::length(cubePos - patch.corners[3]) < 0.01f) matchesExpected = true;
            
            if (matchesExpected) {
                std::cout << " ✓ CORRECT";
            }
            std::cout << std::endl;
            
            std::cout << "    -> Sphere(" << spherePos.x << "," << spherePos.y << "," << spherePos.z << ")" << std::endl;
        }
    }
}

void testPatchCoverage() {
    std::cout << "\n=== Testing Patch Coverage ===\n" << std::endl;
    
    // Test that the 3 patches cover the visible hemisphere properly
    glm::vec3 cameraPos(7.136f, 3.058f, 6.116f);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
    glm::mat4 viewProj = proj * view;
    
    // Test each patch's screen coverage
    struct Patch {
        std::string name;
        glm::vec3 corners[4];
    };
    
    std::vector<Patch> patches = {
        {"+X", {glm::vec3(1,-1,-1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(1,1,-1)}},
        {"+Y", {glm::vec3(-1,1,-1), glm::vec3(1,1,-1), glm::vec3(1,1,1), glm::vec3(-1,1,1)}},
        {"+Z", {glm::vec3(-1,-1,1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(-1,1,1)}}
    };
    
    for (const auto& patch : patches) {
        std::cout << "\nPatch " << patch.name << " screen coverage:" << std::endl;
        
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (int i = 0; i < 4; i++) {
            glm::vec3 spherePos = cubeToSphere(patch.corners[i]);
            glm::vec3 worldPos = spherePos * 6.371f; // Simplified planet scale
            glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
            
            if (clipPos.w > 0) {
                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                minX = std::min(minX, ndc.x);
                maxX = std::max(maxX, ndc.x);
                minY = std::min(minY, ndc.y);
                maxY = std::max(maxY, ndc.y);
                
                std::cout << "  Corner " << i << ": NDC(" << ndc.x << ", " << ndc.y << ")" << std::endl;
            }
        }
        
        std::cout << "  Screen bounds: X[" << minX << " to " << maxX 
                  << "] Y[" << minY << " to " << maxY << "]" << std::endl;
        
        // Check which quadrant this covers
        if (minX >= 0 && minY >= 0) {
            std::cout << "  -> Top-right quadrant" << std::endl;
        } else if (maxX <= 0 && minY >= 0) {
            std::cout << "  -> Top-left quadrant" << std::endl;
        } else if (minX >= 0 && maxY <= 0) {
            std::cout << "  -> Bottom-right quadrant" << std::endl;
        } else if (maxX <= 0 && maxY <= 0) {
            std::cout << "  -> Bottom-left quadrant" << std::endl;
        } else {
            std::cout << "  -> Spans multiple quadrants" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== UV Mapping Fix Verification ===" << std::endl;
    
    testCorrectedUVMapping();
    testPatchCoverage();
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "The UV mapping has been fixed to:" << std::endl;
    std::cout << "1. Keep UV coordinates in 0-1 range (not remapping to -1,1)" << std::endl;
    std::cout << "2. Transform directly maps UV(0,0) to bottom-left corner" << std::endl;
    std::cout << "3. Transform directly maps UV(1,1) to top-right corner" << std::endl;
    std::cout << "4. All 3 patches should now render in their correct positions" << std::endl;
    
    return 0;
}