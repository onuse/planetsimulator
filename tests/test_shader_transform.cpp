#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

const double PLANET_RADIUS = 6371000.0;

// Exact copy of shader's cube-to-sphere
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

void testSinglePatchVertices() {
    std::cout << "=== Testing Single Patch Transformation ===\n\n";
    
    // Simulate what LODManager sends to GPU for a patch on +Z face
    // This should be a small patch near center of face
    glm::dvec3 bottomLeft(-0.25, -0.25, 1.0);
    glm::dvec3 bottomRight(0.25, -0.25, 1.0);
    glm::dvec3 topLeft(-0.25, 0.25, 1.0);
    glm::dvec3 topRight(0.25, 0.25, 1.0);
    
    // Build transform as LODManager does
    glm::dvec3 right = bottomRight - bottomLeft;
    glm::dvec3 up = topLeft - bottomLeft;
    glm::dvec3 faceNormal(0, 0, 1);
    
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(right, 0.0);
    transform[1] = glm::dvec4(up, 0.0);
    transform[2] = glm::dvec4(faceNormal, 0.0);
    transform[3] = glm::dvec4(bottomLeft, 1.0);
    
    std::cout << "Patch transform matrix:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  [";
        for (int j = 0; j < 4; j++) {
            std::cout << std::setw(8) << transform[j][i] << " ";
        }
        std::cout << "]\n";
    }
    
    // Test the 4 corners and center
    struct TestPoint {
        glm::dvec2 uv;
        const char* name;
    };
    
    TestPoint points[] = {
        {{0.0, 0.0}, "Bottom-left"},
        {{1.0, 0.0}, "Bottom-right"},
        {{0.0, 1.0}, "Top-left"},
        {{1.0, 1.0}, "Top-right"},
        {{0.5, 0.5}, "Center"}
    };
    
    std::cout << "\nVertex transformations:\n";
    std::cout << std::fixed << std::setprecision(2);
    
    for (const auto& pt : points) {
        // Step 1: UV to local
        glm::dvec4 localPos(pt.uv.x, pt.uv.y, 0.0, 1.0);
        
        // Step 2: Transform to cube
        glm::dvec3 cubePos = glm::dvec3(transform * localPos);
        
        // Step 3: Project to sphere
        glm::dvec3 spherePos = cubeToSphere(cubePos);
        
        // Step 4: Scale to planet
        glm::dvec3 worldPos = spherePos * PLANET_RADIUS;
        
        std::cout << pt.name << " (UV " << pt.uv.x << "," << pt.uv.y << "):\n";
        std::cout << "  -> Cube: (" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")\n";
        std::cout << "  -> Sphere: (" << spherePos.x << ", " << spherePos.y << ", " << spherePos.z << ")\n";
        std::cout << "  -> World: (" << worldPos.x/1000 << ", " << worldPos.y/1000 << ", " << worldPos.z/1000 << ") km\n";
        std::cout << "  Distance from origin: " << glm::length(worldPos)/1000 << " km\n\n";
    }
}

void testDegenerateCases() {
    std::cout << "=== Testing Degenerate Cases ===\n\n";
    
    // Test 1: What if patch corners are identical (collapsed patch)?
    std::cout << "Test 1: Collapsed patch (all corners at same point)\n";
    glm::dvec3 point(0.5, 0.5, 1.0);
    
    glm::dmat4 badTransform(1.0);
    badTransform[0] = glm::dvec4(0, 0, 0, 0);  // No right vector!
    badTransform[1] = glm::dvec4(0, 0, 0, 0);  // No up vector!
    badTransform[2] = glm::dvec4(0, 0, 1, 0);
    badTransform[3] = glm::dvec4(point, 1.0);
    
    glm::dvec4 testUV(0.5, 0.5, 0.0, 1.0);
    glm::dvec3 result = glm::dvec3(badTransform * testUV);
    std::cout << "  Result: (" << result.x << ", " << result.y << ", " << result.z << ")\n";
    std::cout << "  This would create a single point patch!\n\n";
    
    // Test 2: Inverted patch (negative area)
    std::cout << "Test 2: Inverted patch (clockwise winding)\n";
    glm::dvec3 bl(0, 0, 1);
    glm::dvec3 br(1, 0, 1);
    glm::dvec3 tl(0, 1, 1);
    
    // Intentionally build transform with inverted axes
    glm::dmat4 invertedTransform(1.0);
    invertedTransform[0] = glm::dvec4(bl - br, 0.0);  // Negative right!
    invertedTransform[1] = glm::dvec4(tl - bl, 0.0);
    invertedTransform[2] = glm::dvec4(0, 0, 1, 0);
    invertedTransform[3] = glm::dvec4(bl, 1.0);
    
    glm::dvec3 cubePos = glm::dvec3(invertedTransform * testUV);
    std::cout << "  Cube pos: (" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")\n";
    std::cout << "  This creates inside-out geometry!\n\n";
    
    // Test 3: Extreme scale difference
    std::cout << "Test 3: Extreme scale (tiny patch on huge sphere)\n";
    glm::dvec3 tinyBL(0.0001, 0.0001, 1.0);
    glm::dvec3 tinyBR(0.0002, 0.0001, 1.0);
    glm::dvec3 tinyTL(0.0001, 0.0002, 1.0);
    
    glm::dmat4 tinyTransform(1.0);
    tinyTransform[0] = glm::dvec4(tinyBR - tinyBL, 0.0);
    tinyTransform[1] = glm::dvec4(tinyTL - tinyBL, 0.0);
    tinyTransform[2] = glm::dvec4(0, 0, 1, 0);
    tinyTransform[3] = glm::dvec4(tinyBL, 1.0);
    
    glm::dvec3 tinyResult = glm::dvec3(tinyTransform * testUV);
    glm::dvec3 tinySphere = cubeToSphere(tinyResult) * PLANET_RADIUS;
    std::cout << "  Tiny patch size in cube space: 0.0001\n";
    std::cout << "  Result in world space: " << glm::length(tinySphere)/1000 << " km from origin\n\n";
}

void testWrongFaceNormal() {
    std::cout << "=== Testing Wrong Face Normal ===\n\n";
    
    // What if we use wrong face normal in transform?
    glm::dvec3 bl(-0.5, -0.5, 1.0);  // +Z face corners
    glm::dvec3 br(0.5, -0.5, 1.0);
    glm::dvec3 tl(-0.5, 0.5, 1.0);
    
    // Build transform with WRONG face normal
    glm::dmat4 wrongTransform(1.0);
    wrongTransform[0] = glm::dvec4(br - bl, 0.0);
    wrongTransform[1] = glm::dvec4(tl - bl, 0.0);
    wrongTransform[2] = glm::dvec4(1, 0, 0, 0);  // Wrong! Using +X normal for +Z face!
    wrongTransform[3] = glm::dvec4(bl, 1.0);
    
    std::cout << "Using +X face normal (1,0,0) for +Z face patch:\n";
    
    glm::dvec4 centerUV(0.5, 0.5, 0.0, 1.0);
    glm::dvec3 wrongCube = glm::dvec3(wrongTransform * centerUV);
    glm::dvec3 wrongSphere = cubeToSphere(wrongCube);
    
    std::cout << "  Cube pos: (" << wrongCube.x << ", " << wrongCube.y << ", " << wrongCube.z << ")\n";
    std::cout << "  Sphere pos: (" << wrongSphere.x << ", " << wrongSphere.y << ", " << wrongSphere.z << ")\n";
    
    // Compare with correct
    wrongTransform[2] = glm::dvec4(0, 0, 1, 0);  // Correct normal
    glm::dvec3 correctCube = glm::dvec3(wrongTransform * centerUV);
    glm::dvec3 correctSphere = cubeToSphere(correctCube);
    
    std::cout << "\nWith correct +Z normal (0,0,1):\n";
    std::cout << "  Cube pos: (" << correctCube.x << ", " << correctCube.y << ", " << correctCube.z << ")\n";
    std::cout << "  Sphere pos: (" << correctSphere.x << ", " << correctSphere.y << ", " << correctSphere.z << ")\n";
    
    double error = glm::length(wrongSphere - correctSphere) * PLANET_RADIUS;
    std::cout << "\nError from wrong normal: " << error/1000 << " km!\n";
}

void testMatrixDeterminant() {
    std::cout << "\n=== Testing Matrix Determinants ===\n\n";
    
    // Check if any transforms are degenerate (det = 0)
    std::vector<glm::dmat4> transforms;
    std::vector<const char*> names;
    
    // Normal patch
    glm::dmat4 normal(1.0);
    normal[0] = glm::dvec4(0.5, 0, 0, 0);
    normal[1] = glm::dvec4(0, 0.5, 0, 0);
    normal[2] = glm::dvec4(0, 0, 1, 0);
    normal[3] = glm::dvec4(-0.25, -0.25, 1, 1);
    transforms.push_back(normal);
    names.push_back("Normal patch");
    
    // Degenerate (no area)
    glm::dmat4 degenerate(1.0);
    degenerate[0] = glm::dvec4(0, 0, 0, 0);
    degenerate[1] = glm::dvec4(0, 0, 0, 0);
    degenerate[2] = glm::dvec4(0, 0, 1, 0);
    degenerate[3] = glm::dvec4(0, 0, 1, 1);
    transforms.push_back(degenerate);
    names.push_back("Degenerate patch");
    
    // Inverted
    glm::dmat4 inverted(1.0);
    inverted[0] = glm::dvec4(-0.5, 0, 0, 0);  // Negative right
    inverted[1] = glm::dvec4(0, 0.5, 0, 0);
    inverted[2] = glm::dvec4(0, 0, 1, 0);
    inverted[3] = glm::dvec4(0, 0, 1, 1);
    transforms.push_back(inverted);
    names.push_back("Inverted patch");
    
    for (size_t i = 0; i < transforms.size(); i++) {
        double det = glm::determinant(transforms[i]);
        std::cout << names[i] << ": determinant = " << det;
        
        if (std::abs(det) < 1e-10) {
            std::cout << " ✗ DEGENERATE!\n";
        } else if (det < 0) {
            std::cout << " ✗ INVERTED!\n";
        } else {
            std::cout << " ✓\n";
        }
    }
}

int main() {
    testSinglePatchVertices();
    testDegenerateCases();
    testWrongFaceNormal();
    testMatrixDeterminant();
    
    std::cout << "\n=== HYPOTHESIS ===\n";
    std::cout << "The broken geometry could be caused by:\n";
    std::cout << "1. Degenerate transforms (determinant = 0)\n";
    std::cout << "2. Wrong face normals in transform matrix\n";
    std::cout << "3. Inverted patches (negative determinant)\n";
    std::cout << "4. Extreme scale differences\n";
    std::cout << "5. Transform matrix column 2 (face normal) being used incorrectly\n";
    
    return 0;
}