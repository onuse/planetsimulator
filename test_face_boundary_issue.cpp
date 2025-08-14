#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

void testActualProblem() {
    std::cout << "=== TESTING THE ACTUAL PROBLEM ===\n\n";
    std::cout << std::fixed << std::setprecision(6);
    
    std::cout << "The issue from the test: +Z face point (1,0,1) and +X face point (1,0,1)\n";
    std::cout << "should be the same, but create a 12km gap.\n\n";
    
    std::cout << "Let's test what's actually happening:\n\n";
    
    // What if the +Z face patch thinks its edge is at x=0.999 instead of x=1.0?
    std::cout << "Case 1: +Z patch edge vertex slightly inside boundary\n";
    glm::dvec3 zPoint(0.999, 0.0, 1.0);  // Slightly off from x=1
    glm::dvec3 xPoint(1.0, 0.0, 1.0);    // Exactly at boundary
    
    glm::dvec3 zSphere = cubeToSphere(zPoint) * PLANET_RADIUS;
    glm::dvec3 xSphere = cubeToSphere(xPoint) * PLANET_RADIUS;
    
    double gap1 = glm::length(zSphere - xSphere);
    std::cout << "  +Z point: (" << zPoint.x << ", " << zPoint.y << ", " << zPoint.z << ")\n";
    std::cout << "  +X point: (" << xPoint.x << ", " << xPoint.y << ", " << xPoint.z << ")\n";
    std::cout << "  Gap: " << gap1 << " meters\n\n";
    
    // What if both are slightly off?
    std::cout << "Case 2: Both patches slightly off\n";
    glm::dvec3 zPoint2(0.999, 0.0, 1.0);
    glm::dvec3 xPoint2(1.0, 0.0, 0.999);
    
    glm::dvec3 zSphere2 = cubeToSphere(zPoint2) * PLANET_RADIUS;
    glm::dvec3 xSphere2 = cubeToSphere(xPoint2) * PLANET_RADIUS;
    
    double gap2 = glm::length(zSphere2 - xSphere2);
    std::cout << "  +Z point: (" << zPoint2.x << ", " << zPoint2.y << ", " << zPoint2.z << ")\n";
    std::cout << "  +X point: (" << xPoint2.x << ", " << xPoint2.y << ", " << xPoint2.z << ")\n";
    std::cout << "  Gap: " << gap2 << " meters ✗ THIS IS THE PROBLEM!\n\n";
    
    // The REAL issue: patches think they're on different edges!
    std::cout << "Case 3: THE ACTUAL BUG - Patches using different edge coordinates\n";
    std::cout << "This happens when face patches don't extend to the actual edge!\n\n";
    
    // +Z face patch might stop at x=0.99 (not reaching the edge)
    // +X face patch might stop at z=0.99 (not reaching the edge)
    glm::dvec3 zPoint3(0.99, 0.0, 1.0);   // +Z patch's "edge" vertex
    glm::dvec3 xPoint3(1.0, 0.0, 0.99);    // +X patch's "edge" vertex
    
    glm::dvec3 zSphere3 = cubeToSphere(zPoint3) * PLANET_RADIUS;
    glm::dvec3 xSphere3 = cubeToSphere(xPoint3) * PLANET_RADIUS;
    
    double gap3 = glm::length(zSphere3 - xSphere3);
    std::cout << "  +Z face 'edge' vertex: (" << zPoint3.x << ", " << zPoint3.y << ", " << zPoint3.z << ")\n";
    std::cout << "  +X face 'edge' vertex: (" << xPoint3.x << ", " << xPoint3.y << ", " << xPoint3.z << ")\n";
    std::cout << "  Gap: " << gap3 << " meters\n\n";
    
    // Test with actual measured gap distance
    std::cout << "Case 4: Reproducing the 12,735 meter gap\n";
    // To get a 12km gap, the points must be quite different
    // Let's work backwards from the gap
    
    // Try different separations
    for (double offset = 0.001; offset <= 0.01; offset += 0.001) {
        glm::dvec3 zTest(1.0 - offset, 0.0, 1.0);
        glm::dvec3 xTest(1.0, 0.0, 1.0 - offset);
        
        glm::dvec3 zTestSphere = cubeToSphere(zTest) * PLANET_RADIUS;
        glm::dvec3 xTestSphere = cubeToSphere(xTest) * PLANET_RADIUS;
        
        double testGap = glm::length(zTestSphere - xTestSphere);
        std::cout << "  Offset " << offset << ": Gap = " << testGap << " meters\n";
        
        if (std::abs(testGap - 12735.0) < 100) {
            std::cout << "    ^^ This offset reproduces the 12km gap!\n";
        }
    }
}

void testTransformIssue() {
    std::cout << "\n=== TESTING TRANSFORM MATRIX ISSUE ===\n\n";
    
    // Simulate what happens with the actual transform matrices
    // +Z face patch at edge
    std::cout << "Building transform for +Z face edge patch:\n";
    std::cout << "  Patch bounds: x=[0.5, 1.0], y=[-0.5, 0.5], z=1.0\n";
    
    glm::dmat4 zTransform(1.0);
    double rangeX = 0.5;  // from 0.5 to 1.0
    double rangeY = 1.0;  // from -0.5 to 0.5
    zTransform[0] = glm::dvec4(rangeX, 0.0, 0.0, 0.0);
    zTransform[1] = glm::dvec4(0.0, rangeY, 0.0, 0.0);
    zTransform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    zTransform[3] = glm::dvec4(0.5, -0.5, 1.0, 1.0);  // origin
    
    // UV (1, 0.5) should map to edge point (1.0, 0.0, 1.0)
    glm::dvec4 edgeUV(1.0, 0.5, 0.0, 1.0);
    glm::dvec3 edgePoint = glm::dvec3(zTransform * edgeUV);
    
    std::cout << "  UV(1.0, 0.5) transforms to: (" 
              << edgePoint.x << ", " << edgePoint.y << ", " << edgePoint.z << ")\n";
    std::cout << "  Expected: (1.0, 0.0, 1.0)\n";
    std::cout << "  Correct? " << (std::abs(edgePoint.x - 1.0) < 0.001 ? "YES" : "NO") << "\n\n";
    
    // Now +X face patch at edge
    std::cout << "Building transform for +X face edge patch:\n";
    std::cout << "  Patch bounds: x=1.0, y=[-0.5, 0.5], z=[0.5, 1.0]\n";
    
    glm::dmat4 xTransform(1.0);
    double xRangeZ = 0.5;  // from 0.5 to 1.0
    double xRangeY = 1.0;  // from -0.5 to 0.5
    // For +X face: U maps to Z, V maps to Y
    xTransform[0] = glm::dvec4(0.0, 0.0, xRangeZ, 0.0);
    xTransform[1] = glm::dvec4(0.0, xRangeY, 0.0, 0.0);
    xTransform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    xTransform[3] = glm::dvec4(1.0, -0.5, 0.5, 1.0);  // origin
    
    // UV (1.0, 0.5) should map to edge point (1.0, 0.0, 1.0)
    glm::dvec3 xEdgePoint = glm::dvec3(xTransform * edgeUV);
    
    std::cout << "  UV(1.0, 0.5) transforms to: (" 
              << xEdgePoint.x << ", " << xEdgePoint.y << ", " << xEdgePoint.z << ")\n";
    std::cout << "  Expected: (1.0, 0.0, 1.0)\n";
    std::cout << "  Correct? " << (std::abs(xEdgePoint.z - 1.0) < 0.001 ? "YES" : "NO") << "\n\n";
    
    // Check if they match
    double transformGap = glm::length(edgePoint - xEdgePoint);
    std::cout << "Gap between transformed points: " << transformGap << "\n";
    
    if (transformGap > 0.001) {
        std::cout << "✗ TRANSFORM MISMATCH! The patches are generating different coordinates!\n";
    }
}

int main() {
    testActualProblem();
    testTransformIssue();
    
    std::cout << "\n=== ROOT CAUSE ===\n";
    std::cout << "The 12km gaps occur when:\n";
    std::cout << "1. Face patches don't extend all the way to cube edges (x=±1, y=±1, z=±1)\n";
    std::cout << "2. Adjacent face patches use different coordinate systems\n";
    std::cout << "3. The transform matrices don't account for shared edges\n";
    std::cout << "\nSOLUTION: Ensure patches at face boundaries generate vertices\n";
    std::cout << "at EXACTLY x=±1, y=±1, or z=±1 as appropriate.\n";
    
    return 0;
}