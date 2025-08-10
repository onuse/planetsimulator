// Test to debug why only part of hemisphere renders
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iomanip>

// Simulate the entire vertex transformation pipeline
glm::vec3 cubeToSphere(const glm::vec3& cubePos) {
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
    spherePos.y = cubePos.y * sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
    spherePos.z = cubePos.z * sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
    return glm::normalize(spherePos);
}

void testPatchTransform() {
    std::cout << "\n=== Testing Full Transform Pipeline ===\n" << std::endl;
    
    // Camera setup
    glm::vec3 cameraPos(7.136e6f, 3.058e6f, 6.116e6f);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 3512.0f, 7.024e6f);
    glm::mat4 viewProj = proj * view;
    
    // The 3 patches as reported in debug output
    struct Patch {
        std::string name;
        glm::vec3 center;
        glm::vec3 corners[4];
    };
    
    std::vector<Patch> patches = {
        {"+X", glm::vec3(1,0,0), {glm::vec3(1,-1,-1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(1,1,-1)}},
        {"+Y", glm::vec3(0,1,0), {glm::vec3(-1,1,-1), glm::vec3(1,1,-1), glm::vec3(1,1,1), glm::vec3(-1,1,1)}},
        {"+Z", glm::vec3(0,0,1), {glm::vec3(-1,-1,1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(-1,1,1)}}
    };
    
    for (size_t i = 0; i < patches.size(); i++) {
        const auto& patch = patches[i];
        std::cout << "\nPatch " << i << " (" << patch.name << "):" << std::endl;
        
        // Build transform as in LOD manager
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
        
        std::cout << "  Transform matrix:" << std::endl;
        std::cout << "    Right: " << right.x << ", " << right.y << ", " << right.z << std::endl;
        std::cout << "    Up: " << up.x << ", " << up.y << ", " << up.z << std::endl;
        std::cout << "    Origin: " << bottomLeft.x << ", " << bottomLeft.y << ", " << bottomLeft.z << std::endl;
        
        // Test sample UV points across the patch
        std::vector<glm::vec2> testUVs = {
            {0.0f, 0.0f}, {0.5f, 0.0f}, {1.0f, 0.0f},
            {0.0f, 0.5f}, {0.5f, 0.5f}, {1.0f, 0.5f},
            {0.0f, 1.0f}, {0.5f, 1.0f}, {1.0f, 1.0f}
        };
        
        std::cout << "  Screen coverage:" << std::endl;
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        int visibleCount = 0;
        
        for (const auto& uv : testUVs) {
            // Apply transform as in vertex shader
            glm::vec4 localPos(uv.x, uv.y, 0.0f, 1.0f);
            glm::vec3 cubePos = glm::vec3(transform * localPos);
            glm::vec3 spherePos = cubeToSphere(cubePos);
            glm::vec3 worldPos = spherePos * 6.371e6f;
            
            // Project to screen
            glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
            if (clipPos.w > 0) {
                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                minX = std::min(minX, ndc.x);
                maxX = std::max(maxX, ndc.x);
                minY = std::min(minY, ndc.y);
                maxY = std::max(maxY, ndc.y);
                
                if (ndc.x >= -1 && ndc.x <= 1 && ndc.y >= -1 && ndc.y <= 1) {
                    visibleCount++;
                }
            }
        }
        
        std::cout << "    NDC bounds: X[" << minX << " to " << maxX 
                  << "] Y[" << minY << " to " << maxY << "]" << std::endl;
        std::cout << "    Visible points: " << visibleCount << "/9" << std::endl;
        
        // Check which screen quadrant this patch covers
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        std::cout << "    Center at NDC: (" << centerX << ", " << centerY << ")" << std::endl;
        
        if (centerX > 0 && centerY > 0) {
            std::cout << "    -> Upper-right quadrant" << std::endl;
        } else if (centerX < 0 && centerY > 0) {
            std::cout << "    -> Upper-left quadrant" << std::endl;
        } else if (centerX > 0 && centerY < 0) {
            std::cout << "    -> Lower-right quadrant" << std::endl;
        } else if (centerX < 0 && centerY < 0) {
            std::cout << "    -> Lower-left quadrant" << std::endl;
        }
    }
}

void testMeshCoverage() {
    std::cout << "\n=== Testing Base Mesh Coverage ===\n" << std::endl;
    
    // The base mesh is 64x64
    const int resolution = 64;
    int cornerCount = 0;
    int edgeCount = 0;
    int interiorCount = 0;
    
    for (int y = 0; y < resolution; y++) {
        for (int x = 0; x < resolution; x++) {
            float u = float(x) / (resolution - 1);
            float v = float(y) / (resolution - 1);
            
            // Check if this is a corner, edge, or interior vertex
            bool isCorner = (x == 0 || x == resolution-1) && (y == 0 || y == resolution-1);
            bool isEdge = !isCorner && (x == 0 || x == resolution-1 || y == 0 || y == resolution-1);
            
            if (isCorner) cornerCount++;
            else if (isEdge) edgeCount++;
            else interiorCount++;
        }
    }
    
    std::cout << "Base mesh (64x64):" << std::endl;
    std::cout << "  Total vertices: " << resolution * resolution << std::endl;
    std::cout << "  Corner vertices: " << cornerCount << std::endl;
    std::cout << "  Edge vertices: " << edgeCount << std::endl;
    std::cout << "  Interior vertices: " << interiorCount << std::endl;
    std::cout << "  UV range: [0,0] to [1,1]" << std::endl;
}

int main() {
    std::cout << "=== Patch Rendering Debug ===" << std::endl;
    
    testPatchTransform();
    testMeshCoverage();
    
    std::cout << "\n=== Analysis ===" << std::endl;
    std::cout << "If only lower-right renders, possible causes:" << std::endl;
    std::cout << "1. Clipping: Parts may be outside frustum" << std::endl;
    std::cout << "2. Culling: Backface culling removing patches" << std::endl;
    std::cout << "3. Transform: Instance transforms not applied correctly" << std::endl;
    std::cout << "4. Indexing: Instance indexing might be wrong" << std::endl;
    
    return 0;
}