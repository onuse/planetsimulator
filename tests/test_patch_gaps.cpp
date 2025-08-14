#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

const double PLANET_RADIUS = 6371000.0;

// Cube to sphere projection (from shader)
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

// Simulate the full vertex transformation pipeline
glm::dvec3 transformVertex(glm::dvec2 uv, const glm::dmat4& patchTransform) {
    // Step 1: UV to local space
    glm::dvec4 localPos(uv.x, uv.y, 0.0, 1.0);
    
    // Step 2: Transform to cube face
    glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
    
    // Step 3: Project to sphere
    glm::dvec3 spherePos = cubeToSphere(cubePos);
    
    // Step 4: Scale by planet radius
    return spherePos * PLANET_RADIUS;
}

void testAdjacentPatchSeam() {
    std::cout << "=== Testing Adjacent Patch Seam ===\n\n";
    
    // Two adjacent patches on +Z face at same LOD level
    // Left patch: x in [-0.5, 0], y in [-0.5, 0.5], z = 1
    glm::dvec3 leftBL(-0.5, -0.5, 1.0);
    glm::dvec3 leftBR(0.0, -0.5, 1.0);
    glm::dvec3 leftTL(-0.5, 0.5, 1.0);
    
    // Right patch: x in [0, 0.5], y in [-0.5, 0.5], z = 1
    glm::dvec3 rightBL(0.0, -0.5, 1.0);
    glm::dvec3 rightBR(0.5, -0.5, 1.0);
    glm::dvec3 rightTL(0.0, 0.5, 1.0);
    
    // Build transforms
    glm::dmat4 leftTransform(1.0);
    leftTransform[0] = glm::dvec4(leftBR - leftBL, 0.0);  // right vector
    leftTransform[1] = glm::dvec4(leftTL - leftBL, 0.0);  // up vector
    leftTransform[2] = glm::dvec4(0, 0, 1, 0);            // face normal
    leftTransform[3] = glm::dvec4(leftBL, 1.0);           // origin
    
    glm::dmat4 rightTransform(1.0);
    rightTransform[0] = glm::dvec4(rightBR - rightBL, 0.0);
    rightTransform[1] = glm::dvec4(rightTL - rightBL, 0.0);
    rightTransform[2] = glm::dvec4(0, 0, 1, 0);
    rightTransform[3] = glm::dvec4(rightBL, 1.0);
    
    std::cout << "Testing shared edge vertices after full transformation:\n";
    std::cout << std::fixed << std::setprecision(2);
    
    double maxGap = 0.0;
    
    // Test vertices along the shared edge
    for (int i = 0; i <= 10; i++) {
        double v = i / 10.0;
        
        // Right edge of left patch (u=1, v)
        glm::dvec3 leftVertex = transformVertex(glm::dvec2(1.0, v), leftTransform);
        
        // Left edge of right patch (u=0, v)
        glm::dvec3 rightVertex = transformVertex(glm::dvec2(0.0, v), rightTransform);
        
        double gap = glm::length(leftVertex - rightVertex);
        maxGap = std::max(maxGap, gap);
        
        std::cout << "  v=" << std::setw(3) << v << ": Gap = " << std::setw(8) << gap << " meters";
        
        if (gap < 0.01) {
            std::cout << " ✓\n";
        } else if (gap < 1.0) {
            std::cout << " ⚠ (small gap)\n";
        } else {
            std::cout << " ✗ (LARGE GAP!)\n";
        }
    }
    
    std::cout << "\nMaximum gap: " << maxGap << " meters\n";
}

void testDifferentLODSeam() {
    std::cout << "\n=== Testing Different LOD Level Seam ===\n\n";
    
    // Coarse patch (LOD 1): covers x in [-0.5, 0.5], y in [-0.5, 0.5]
    glm::dvec3 coarseBL(-0.5, -0.5, 1.0);
    glm::dvec3 coarseBR(0.5, -0.5, 1.0);
    glm::dvec3 coarseTL(-0.5, 0.5, 1.0);
    
    // Fine patch (LOD 2): covers x in [0.5, 1.0], y in [-0.5, 0.5] (adjacent to coarse)
    glm::dvec3 fineBL(0.5, -0.5, 1.0);
    glm::dvec3 fineBR(1.0, -0.5, 1.0);
    glm::dvec3 fineTL(0.5, 0.5, 1.0);
    
    glm::dmat4 coarseTransform(1.0);
    coarseTransform[0] = glm::dvec4(coarseBR - coarseBL, 0.0);
    coarseTransform[1] = glm::dvec4(coarseTL - coarseBL, 0.0);
    coarseTransform[2] = glm::dvec4(0, 0, 1, 0);
    coarseTransform[3] = glm::dvec4(coarseBL, 1.0);
    
    glm::dmat4 fineTransform(1.0);
    fineTransform[0] = glm::dvec4(fineBR - fineBL, 0.0);
    fineTransform[1] = glm::dvec4(fineTL - fineBL, 0.0);
    fineTransform[2] = glm::dvec4(0, 0, 1, 0);
    fineTransform[3] = glm::dvec4(fineBL, 1.0);
    
    std::cout << "Coarse patch (5 vertices) vs Fine patch (11 vertices) on shared edge:\n";
    
    // Coarse patch has vertices at v = 0, 0.25, 0.5, 0.75, 1.0
    // Fine patch has vertices at v = 0, 0.1, 0.2, ..., 1.0
    
    std::cout << "\nCoarse patch edge vertices:\n";
    for (int i = 0; i <= 4; i++) {
        double v = i / 4.0;
        glm::dvec3 vertex = transformVertex(glm::dvec2(1.0, v), coarseTransform);
        std::cout << "  v=" << std::setw(4) << v << ": World pos = ("
                  << vertex.x/1000.0 << ", " << vertex.y/1000.0 << ", " 
                  << vertex.z/1000.0 << ") km\n";
    }
    
    std::cout << "\nFine patch edge vertices (checking alignment):\n";
    for (int i = 0; i <= 10; i++) {
        double v = i / 10.0;
        glm::dvec3 fineVertex = transformVertex(glm::dvec2(0.0, v), fineTransform);
        
        // Check if this should align with a coarse vertex
        bool shouldAlign = (i % 2 == 0) && (i % 5 == 0); // v = 0, 0.5, 1.0
        if (i == 5) shouldAlign = true; // v = 0.5
        
        if (shouldAlign) {
            // Find corresponding coarse vertex
            double coarseV = v;
            glm::dvec3 coarseVertex = transformVertex(glm::dvec2(1.0, coarseV), coarseTransform);
            double gap = glm::length(fineVertex - coarseVertex);
            
            std::cout << "  v=" << std::setw(4) << v << ": Gap = " << std::setw(8) << gap 
                      << " meters (should align)";
            if (gap < 0.01) {
                std::cout << " ✓\n";
            } else {
                std::cout << " ✗ T-JUNCTION!\n";
            }
        } else {
            std::cout << "  v=" << std::setw(4) << v << ": (no coarse vertex here)\n";
        }
    }
}

void testPlanetScalePrecision() {
    std::cout << "\n=== Testing Precision at Planet Scale ===\n\n";
    
    // Test if small differences in cube coordinates create gaps at planet scale
    glm::dvec3 pos1(0.5, 0.5, 1.0);
    glm::dvec3 pos2(0.5 + 1e-6, 0.5, 1.0); // Tiny difference
    
    glm::dvec3 sphere1 = cubeToSphere(pos1) * PLANET_RADIUS;
    glm::dvec3 sphere2 = cubeToSphere(pos2) * PLANET_RADIUS;
    
    double gap = glm::length(sphere2 - sphere1);
    
    std::cout << "Cube position difference: " << 1e-6 << "\n";
    std::cout << "Resulting gap at planet scale: " << gap << " meters\n";
    
    if (gap < 1.0) {
        std::cout << "Precision is adequate ✓\n";
    } else {
        std::cout << "Precision issue! Small cube differences create large gaps ✗\n";
    }
    
    // Test accumulated error
    std::cout << "\nTesting accumulated precision error:\n";
    
    glm::dvec3 cubePos(0.123456789, 0.987654321, 1.0);
    
    // Simulate multiple transformations
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(0.1, 0.0, 0.0, 0.0);
    transform[1] = glm::dvec4(0.0, 0.1, 0.0, 0.0);
    transform[2] = glm::dvec4(0.0, 0.0, 1.0, 0.0);
    transform[3] = glm::dvec4(0.45, 0.45, 0.0, 1.0);
    
    glm::dvec2 uv(0.23456789, 0.87654321);
    glm::dvec3 result1 = transformVertex(uv, transform);
    
    // Do it again with slightly different values
    uv.x += 1e-7;
    glm::dvec3 result2 = transformVertex(uv, transform);
    
    double finalGap = glm::length(result2 - result1);
    std::cout << "UV difference: " << 1e-7 << "\n";
    std::cout << "Final gap: " << finalGap << " meters\n";
}

void testCubeFaceEdges() {
    std::cout << "\n=== Testing Cube Face Edge Transitions ===\n\n";
    
    // Test the edge between +Z face and +X face
    // +Z face: x,y in [-1,1], z=1
    // +X face: x=1, y,z in [-1,1]
    
    // Point on +Z face near right edge
    glm::dvec3 zFacePoint(0.999, 0.0, 1.0);
    glm::dvec3 zSphere = cubeToSphere(zFacePoint) * PLANET_RADIUS;
    
    // Corresponding point on +X face near left edge  
    glm::dvec3 xFacePoint(1.0, 0.0, 0.999);
    glm::dvec3 xSphere = cubeToSphere(xFacePoint) * PLANET_RADIUS;
    
    double gap = glm::length(xSphere - zSphere);
    
    std::cout << "+Z face edge point: (" << zFacePoint.x << ", " << zFacePoint.y << ", " << zFacePoint.z << ")\n";
    std::cout << "+X face edge point: (" << xFacePoint.x << ", " << xFacePoint.y << ", " << xFacePoint.z << ")\n";
    std::cout << "Gap between face edges: " << gap << " meters\n";
    
    if (gap > 1000.0) {
        std::cout << "LARGE GAP at face boundary! ✗\n";
    } else {
        std::cout << "Face edges are close ✓\n";
    }
}

int main() {
    testAdjacentPatchSeam();
    testDifferentLODSeam();
    testPlanetScalePrecision();
    testCubeFaceEdges();
    
    std::cout << "\n=== ANALYSIS ===\n";
    std::cout << "Based on these tests, we can determine:\n";
    std::cout << "1. If gaps exist between same-LOD patches\n";
    std::cout << "2. If T-junctions create gaps at different LODs\n";
    std::cout << "3. If precision is adequate at planet scale\n";
    std::cout << "4. If cube face transitions have gaps\n";
    
    return 0;
}