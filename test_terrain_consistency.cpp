#include <iostream>
#include <iomanip>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include "core/global_patch_generator.hpp"
#include "math/planet_math.hpp"

// Simplified noise function matching the shader
float hash(glm::vec3 p) {
    p = glm::fract(p * glm::vec3(443.8975f, 397.2973f, 491.1871f));
    glm::vec3 p_yzx = glm::vec3(p.y, p.z, p.x);  // Manual swizzle
    p += glm::dot(p, p_yzx + 19.19f);
    return glm::fract((p.x + p.y) * p.z);
}

float smoothNoise(glm::vec3 p) {
    glm::vec3 i = glm::floor(p);
    glm::vec3 f = glm::fract(p);
    
    // Smooth the interpolation
    f = f * f * (3.0f - 2.0f * f);
    
    // Sample the 8 corners of the cube
    float a = hash(i);
    float b = hash(i + glm::vec3(1.0f, 0.0f, 0.0f));
    float c = hash(i + glm::vec3(0.0f, 1.0f, 0.0f));
    float d = hash(i + glm::vec3(1.0f, 1.0f, 0.0f));
    float e = hash(i + glm::vec3(0.0f, 0.0f, 1.0f));
    float f1 = hash(i + glm::vec3(1.0f, 0.0f, 1.0f));
    float g = hash(i + glm::vec3(0.0f, 1.0f, 1.0f));
    float h = hash(i + glm::vec3(1.0f, 1.0f, 1.0f));
    
    // Trilinear interpolation
    float x1 = glm::mix(a, b, f.x);
    float x2 = glm::mix(c, d, f.x);
    float x3 = glm::mix(e, f1, f.x);
    float x4 = glm::mix(g, h, f.x);
    
    float y1 = glm::mix(x1, x2, f.y);
    float y2 = glm::mix(x3, x4, f.y);
    
    return glm::mix(y1, y2, f.z);
}

float terrainNoise(glm::vec3 p, int octaves) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += smoothNoise(p * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return value / maxValue;
}

float getTerrainHeight(glm::vec3 sphereNormal) {
    // Use the sphere normal directly as input to ensure continuity
    glm::vec3 noisePos = sphereNormal * 3.0f;
    
    // Continental shelf - large scale features
    float continents = terrainNoise(noisePos, 4) * 2.0f - 1.0f;
    continents = continents * 2000.0f - 500.0f;
    
    // Mountain ranges - medium scale
    float mountains = 0.0f;
    if (continents > 0.0f) {
        glm::vec3 mountainPos = sphereNormal * 8.0f;
        mountains = terrainNoise(mountainPos, 3) * 1200.0f;
    }
    
    // Small details - high frequency
    glm::vec3 detailPos = sphereNormal * 20.0f;
    float details = terrainNoise(detailPos, 2) * 200.0f - 100.0f;
    
    // Combine all layers
    float height = continents + mountains * 0.7f + details * 0.3f;
    
    // Ocean floor variation
    if (height < 0.0f) {
        height = height * 0.8f - 500.0f;
        height = std::max(height, -3000.0f);
    }
    
    return height;
}

void testBoundaryConsistency() {
    std::cout << "\n=== Testing Terrain Consistency at Face Boundaries ===\n\n";
    
    // Test edge between +X and +Y faces
    std::cout << "Testing edge between +X and +Y faces:\n";
    std::cout << "--------------------------------------\n";
    
    // Points along the edge X=1, Y=1, varying Z
    for (float z = -1.0f; z <= 1.0f; z += 0.5f) {
        glm::dvec3 cubePos(1.0, 1.0, z);
        
        // Normalize to get consistent sampling position
        glm::dvec3 spherePos = math::cubeToSphere(cubePos);
        glm::vec3 samplePos = glm::normalize(glm::vec3(cubePos));
        
        float height = getTerrainHeight(samplePos);
        
        std::cout << "Cube pos: (" << std::setw(5) << cubePos.x 
                  << ", " << std::setw(5) << cubePos.y 
                  << ", " << std::setw(5) << cubePos.z << ")";
        std::cout << " -> Sample pos: (" << std::setw(7) << std::setprecision(4) << samplePos.x
                  << ", " << std::setw(7) << samplePos.y
                  << ", " << std::setw(7) << samplePos.z << ")";
        std::cout << " -> Height: " << std::setw(8) << height << " m\n";
    }
    
    // Test corner where 3 faces meet
    std::cout << "\nTesting corner at (+1, +1, +1):\n";
    std::cout << "--------------------------------\n";
    
    glm::dvec3 cornerCube(1.0, 1.0, 1.0);
    glm::vec3 cornerSample = glm::normalize(glm::vec3(cornerCube));
    float cornerHeight = getTerrainHeight(cornerSample);
    
    std::cout << "Corner cube pos: " << glm::to_string(cornerCube) << "\n";
    std::cout << "Sample pos: " << glm::to_string(cornerSample) << "\n";
    std::cout << "Height: " << cornerHeight << " m\n";
    
    // Now test from slightly different positions (should give same height)
    std::cout << "\nTesting consistency from nearby positions:\n";
    std::cout << "------------------------------------------\n";
    
    // Test with slight numerical errors (as might happen in floating point)
    float epsilon = 1e-6f;
    for (int i = 0; i < 5; i++) {
        glm::dvec3 testCube = cornerCube + glm::dvec3(
            (i & 1) ? epsilon : -epsilon,
            (i & 2) ? epsilon : -epsilon,
            (i & 4) ? epsilon : -epsilon
        );
        
        glm::vec3 testSample = glm::normalize(glm::vec3(testCube));
        float testHeight = getTerrainHeight(testSample);
        
        float diff = std::abs(testHeight - cornerHeight);
        std::cout << "Offset (" << ((i & 1) ? "+" : "-") << "ε, "
                  << ((i & 2) ? "+" : "-") << "ε, "
                  << ((i & 4) ? "+" : "-") << "ε): "
                  << "Height = " << testHeight 
                  << ", Diff = " << diff;
        
        if (diff > 0.01f) {
            std::cout << " [INCONSISTENT!]";
        }
        std::cout << "\n";
    }
}

void testPatchTransforms() {
    std::cout << "\n=== Testing Patch Transform Consistency ===\n\n";
    
    // Create root patches
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    // Test a point at the edge between +X and +Y faces
    glm::vec2 testUV(1.0f, 0.5f);  // Right edge of a patch
    
    std::cout << "Testing UV(" << testUV.x << ", " << testUV.y << ") on different faces:\n";
    std::cout << "---------------------------------------------------\n";
    
    for (size_t i = 0; i < roots.size(); i++) {
        auto& patch = roots[i];
        glm::dmat4 transform = patch.createTransform();
        
        // Transform UV to cube position
        glm::dvec4 localPos(testUV.x, testUV.y, 0.0, 1.0);
        glm::dvec3 cubePos = glm::dvec3(transform * localPos);
        
        // Get the sample position
        glm::vec3 samplePos = glm::normalize(glm::vec3(cubePos));
        float height = getTerrainHeight(samplePos);
        
        const char* faceNames[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
        std::cout << "Face " << faceNames[i] << ": ";
        std::cout << "Cube(" << std::setw(6) << std::setprecision(3) << cubePos.x
                  << ", " << std::setw(6) << cubePos.y
                  << ", " << std::setw(6) << cubePos.z << ")";
        std::cout << " -> Sample(" << std::setw(6) << samplePos.x
                  << ", " << std::setw(6) << samplePos.y  
                  << ", " << std::setw(6) << samplePos.z << ")";
        std::cout << " -> Height: " << std::setw(8) << height << " m\n";
    }
}

void testSubdivisionConsistency() {
    std::cout << "\n=== Testing Subdivision Boundary Consistency ===\n\n";
    
    // Create a root patch and subdivide it
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    auto& rootPatch = roots[0]; // +X face
    
    auto children = core::GlobalPatchGenerator::subdivide(rootPatch);
    
    std::cout << "Testing shared edge between child patches 0 and 1:\n";
    std::cout << "---------------------------------------------------\n";
    
    // Child 0 and 1 share a vertical edge
    // Test points along this edge
    for (float v = 0.0f; v <= 1.0f; v += 0.25f) {
        // From child 0's perspective (right edge)
        glm::dvec2 uv0(1.0, v);
        glm::dmat4 transform0 = children[0].createTransform();
        glm::dvec4 localPos0(uv0.x, uv0.y, 0.0, 1.0);
        glm::dvec3 cubePos0 = glm::dvec3(transform0 * localPos0);
        glm::vec3 samplePos0 = glm::normalize(glm::vec3(cubePos0));
        float height0 = getTerrainHeight(samplePos0);
        
        // From child 1's perspective (left edge)
        glm::dvec2 uv1(0.0, v);
        glm::dmat4 transform1 = children[1].createTransform();
        glm::dvec4 localPos1(uv1.x, uv1.y, 0.0, 1.0);
        glm::dvec3 cubePos1 = glm::dvec3(transform1 * localPos1);
        glm::vec3 samplePos1 = glm::normalize(glm::vec3(cubePos1));
        float height1 = getTerrainHeight(samplePos1);
        
        float diff = std::abs(height1 - height0);
        
        std::cout << "V=" << std::setw(4) << v << ": ";
        std::cout << "Child0 cube(" << std::setw(6) << std::setprecision(3) << cubePos0.x
                  << ", " << std::setw(6) << cubePos0.y  
                  << ", " << std::setw(6) << cubePos0.z << ")";
        std::cout << " H=" << std::setw(8) << height0 << " | ";
        std::cout << "Child1 cube(" << std::setw(6) << cubePos1.x
                  << ", " << std::setw(6) << cubePos1.y
                  << ", " << std::setw(6) << cubePos1.z << ")";
        std::cout << " H=" << std::setw(8) << height1;
        std::cout << " | Diff=" << std::setw(8) << diff;
        
        if (diff > 0.01f) {
            std::cout << " [MISMATCH!]";
        } else {
            std::cout << " [OK]";
        }
        std::cout << "\n";
    }
}

int main() {
    std::cout << std::fixed << std::setprecision(2);
    
    testBoundaryConsistency();
    testPatchTransforms();
    testSubdivisionConsistency();
    
    return 0;
}