#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Test patch transform generation
void printMatrix(const glm::dmat4& m, const std::string& name) {
    std::cout << name << ":\n";
    for (int row = 0; row < 4; row++) {
        std::cout << "  [";
        for (int col = 0; col < 4; col++) {
            std::cout << std::setw(10) << std::fixed << std::setprecision(4) << m[col][row];
            if (col < 3) std::cout << ", ";
        }
        std::cout << "]\n";
    }
}

void testUVMapping() {
    std::cout << "=== Testing UV to World Mapping ===\n\n";
    
    // Define a patch on the +Z face
    glm::dvec3 bottomLeft(-0.5, -0.5, 1.0);
    glm::dvec3 bottomRight(0.5, -0.5, 1.0);
    glm::dvec3 topLeft(-0.5, 0.5, 1.0);
    glm::dvec3 topRight(0.5, 0.5, 1.0);
    
    // Build transform matrix (as done in LODManager)
    glm::dvec3 right = bottomRight - bottomLeft;
    glm::dvec3 up = topLeft - bottomLeft;
    glm::dvec3 faceNormal(0, 0, 1); // +Z face
    
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(right, 0.0);
    transform[1] = glm::dvec4(up, 0.0);
    transform[2] = glm::dvec4(faceNormal, 0.0);
    transform[3] = glm::dvec4(bottomLeft, 1.0);
    
    printMatrix(transform, "Patch Transform Matrix");
    
    // Test UV corners
    std::cout << "\nTesting UV corner mapping:\n";
    glm::dvec2 uvCorners[] = {
        glm::dvec2(0, 0),  // Bottom-left
        glm::dvec2(1, 0),  // Bottom-right
        glm::dvec2(0, 1),  // Top-left
        glm::dvec2(1, 1)   // Top-right
    };
    
    glm::dvec3 expectedCorners[] = {
        bottomLeft,
        bottomRight,
        topLeft,
        topRight
    };
    
    const char* cornerNames[] = {
        "Bottom-left (0,0)",
        "Bottom-right (1,0)",
        "Top-left (0,1)",
        "Top-right (1,1)"
    };
    
    for (int i = 0; i < 4; i++) {
        glm::dvec4 uvPos(uvCorners[i].x, uvCorners[i].y, 0.0, 1.0);
        glm::dvec4 worldPos = transform * uvPos;
        glm::dvec3 result(worldPos);
        glm::dvec3 expected = expectedCorners[i];
        glm::dvec3 diff = result - expected;
        double error = glm::length(diff);
        
        std::cout << "  " << cornerNames[i] << ":\n";
        std::cout << "    Expected: (" << expected.x << ", " << expected.y << ", " << expected.z << ")\n";
        std::cout << "    Got:      (" << result.x << ", " << result.y << ", " << result.z << ")\n";
        std::cout << "    Error:    " << error;
        if (error < 1e-10) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗\n";
        }
    }
}

void testSubdividedPatch() {
    std::cout << "\n=== Testing Subdivided Patch ===\n\n";
    
    // Parent patch covers full +Z face
    glm::dvec3 parentBL(-1, -1, 1);
    glm::dvec3 parentBR(1, -1, 1);
    glm::dvec3 parentTL(-1, 1, 1);
    
    // Child patch (bottom-left quadrant)
    glm::dvec3 childBL(-1, -1, 1);
    glm::dvec3 childBR(0, -1, 1);
    glm::dvec3 childTL(-1, 0, 1);
    
    // Build child transform
    glm::dvec3 right = childBR - childBL;
    glm::dvec3 up = childTL - childBL;
    glm::dvec3 faceNormal(0, 0, 1);
    
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(right, 0.0);
    transform[1] = glm::dvec4(up, 0.0);
    transform[2] = glm::dvec4(faceNormal, 0.0);
    transform[3] = glm::dvec4(childBL, 1.0);
    
    std::cout << "Child patch (bottom-left quadrant of parent):\n";
    printMatrix(transform, "Child Transform");
    
    // Test that UV (0.5, 0.5) maps to center of child patch
    glm::dvec4 centerUV(0.5, 0.5, 0.0, 1.0);
    glm::dvec4 centerWorld = transform * centerUV;
    
    std::cout << "\nCenter of child patch (UV 0.5, 0.5):\n";
    std::cout << "  World position: (" << centerWorld.x << ", " << centerWorld.y << ", " << centerWorld.z << ")\n";
    std::cout << "  Expected:       (-0.5, -0.5, 1.0)\n";
    
    glm::dvec3 expected(-0.5, -0.5, 1.0);
    glm::dvec3 result(centerWorld);
    double error = glm::length(result - expected);
    std::cout << "  Error: " << error;
    if (error < 1e-10) {
        std::cout << " ✓\n";
    } else {
        std::cout << " ✗\n";
    }
}

void testAdjacentPatches() {
    std::cout << "\n=== Testing Adjacent Patches ===\n\n";
    
    // Two adjacent patches on +Z face
    // Left patch: x in [-1, 0], y in [-1, 1]
    glm::dvec3 leftBL(-1, -1, 1);
    glm::dvec3 leftBR(0, -1, 1);
    glm::dvec3 leftTL(-1, 1, 1);
    
    // Right patch: x in [0, 1], y in [-1, 1]
    glm::dvec3 rightBL(0, -1, 1);
    glm::dvec3 rightBR(1, -1, 1);
    glm::dvec3 rightTL(0, 1, 1);
    
    // Build transforms
    glm::dmat4 leftTransform(1.0);
    leftTransform[0] = glm::dvec4(leftBR - leftBL, 0.0);
    leftTransform[1] = glm::dvec4(leftTL - leftBL, 0.0);
    leftTransform[2] = glm::dvec4(0, 0, 1, 0);
    leftTransform[3] = glm::dvec4(leftBL, 1.0);
    
    glm::dmat4 rightTransform(1.0);
    rightTransform[0] = glm::dvec4(rightBR - rightBL, 0.0);
    rightTransform[1] = glm::dvec4(rightTL - rightBL, 0.0);
    rightTransform[2] = glm::dvec4(0, 0, 1, 0);
    rightTransform[3] = glm::dvec4(rightBL, 1.0);
    
    // Test shared edge vertices
    std::cout << "Testing shared edge between adjacent patches:\n";
    
    for (int i = 0; i <= 4; i++) {
        double v = i / 4.0;
        
        // Right edge of left patch (UV = 1, v)
        glm::dvec4 leftEdgeUV(1.0, v, 0.0, 1.0);
        glm::dvec4 leftEdgeWorld = leftTransform * leftEdgeUV;
        
        // Left edge of right patch (UV = 0, v)
        glm::dvec4 rightEdgeUV(0.0, v, 0.0, 1.0);
        glm::dvec4 rightEdgeWorld = rightTransform * rightEdgeUV;
        
        glm::dvec3 leftPos(leftEdgeWorld);
        glm::dvec3 rightPos(rightEdgeWorld);
        double error = glm::length(leftPos - rightPos);
        
        std::cout << "  v=" << v << ":\n";
        std::cout << "    Left patch edge:  (" << leftPos.x << ", " << leftPos.y << ", " << leftPos.z << ")\n";
        std::cout << "    Right patch edge: (" << rightPos.x << ", " << rightPos.y << ", " << rightPos.z << ")\n";
        std::cout << "    Error: " << error;
        if (error < 1e-10) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ MISMATCH!\n";
        }
    }
}

int main() {
    testUVMapping();
    testSubdividedPatch();
    testAdjacentPatches();
    
    return 0;
}