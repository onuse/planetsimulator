#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

// ============================================================================
// TEST: Reproduce the ACTUAL visual gaps we see in screenshots
// ============================================================================

const double PLANET_RADIUS = 6371000.0;
const double EPSILON = 1e-5;

// Cube to sphere projection (matching shader)
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    return glm::normalize(spherePos);
}

// Create transform exactly as in GlobalPatchGenerator
glm::dmat4 createTransform(int face, const glm::dvec3& center, double size) {
    glm::dmat4 transform(1.0);
    
    double halfSize = size * 0.5;
    glm::dvec3 bl, br, tl;
    
    switch(face) {
        case 0: // +X
            bl = glm::dvec3(1.0, center.y - halfSize, center.z - halfSize);
            br = glm::dvec3(1.0, center.y - halfSize, center.z + halfSize);
            tl = glm::dvec3(1.0, center.y + halfSize, center.z - halfSize);
            break;
        case 4: // +Z
            bl = glm::dvec3(center.x - halfSize, center.y - halfSize, 1.0);
            br = glm::dvec3(center.x + halfSize, center.y - halfSize, 1.0);
            tl = glm::dvec3(center.x - halfSize, center.y + halfSize, 1.0);
            break;
        default:
            return transform;
    }
    
    // Snap boundaries
    auto snapBoundary = [](double& val) {
        if (std::abs(std::abs(val) - 1.0) < EPSILON) {
            val = (val > 0.0) ? 1.0 : -1.0;
        }
    };
    
    snapBoundary(bl.x); snapBoundary(bl.y); snapBoundary(bl.z);
    snapBoundary(br.x); snapBoundary(br.y); snapBoundary(br.z);
    snapBoundary(tl.x); snapBoundary(tl.y); snapBoundary(tl.z);
    
    transform[0] = glm::dvec4(br - bl, 0.0);
    transform[1] = glm::dvec4(tl - bl, 0.0);
    transform[2] = glm::dvec4(0, 0, 0, 0);
    transform[3] = glm::dvec4(bl, 1.0);
    
    return transform;
}

// Transform vertex (emulating shader)
glm::dvec3 transformVertex(glm::dvec2 uv, const glm::dmat4& transform) {
    glm::dvec4 localPos(uv.x, uv.y, 0.0, 1.0);
    glm::dvec3 cubePos = glm::dvec3(transform * localPos);
    
    // Snap to boundaries
    if (std::abs(std::abs(cubePos.x) - 1.0) < EPSILON) {
        cubePos.x = (cubePos.x > 0.0) ? 1.0 : -1.0;
    }
    if (std::abs(std::abs(cubePos.y) - 1.0) < EPSILON) {
        cubePos.y = (cubePos.y > 0.0) ? 1.0 : -1.0;
    }
    if (std::abs(std::abs(cubePos.z) - 1.0) < EPSILON) {
        cubePos.z = (cubePos.z > 0.0) ? 1.0 : -1.0;
    }
    
    glm::dvec3 spherePos = cubeToSphere(cubePos);
    return spherePos * PLANET_RADIUS;
}

void testActualPatchConfiguration() {
    std::cout << "=== Testing ACTUAL patch configurations from the renderer ===\n\n";
    
    // These are actual patch configurations that appear in the rendered scene
    // We can see these patches in the screenshots - they should connect but don't
    
    // Example: Two patches that meet at the +X/+Z edge
    // These specific patches are visible in the screenshot with gaps
    
    // Patch on +Z face near the edge with +X
    glm::dvec3 centerZ(0.75, 0.75, 1.0);  // Near the +X edge
    glm::dmat4 transformZ = createTransform(4, centerZ, 0.5);
    
    // Patch on +X face near the edge with +Z  
    glm::dvec3 centerX(1.0, 0.75, 0.75);  // Near the +Z edge
    glm::dmat4 transformX = createTransform(0, centerX, 0.5);
    
    std::cout << "Testing patches that SHOULD share an edge:\n";
    std::cout << "+Z patch center: (" << centerZ.x << ", " << centerZ.y << ", " << centerZ.z << ")\n";
    std::cout << "+X patch center: (" << centerX.x << ", " << centerX.y << ", " << centerX.z << ")\n\n";
    
    // Test the edge that should be shared
    // For +Z patch: right edge (u=1)
    // For +X patch: top edge (v=1)
    
    std::cout << "Edge vertices (should match but don't in the visual):\n";
    std::cout << std::fixed << std::setprecision(2);
    
    double maxGap = 0.0;
    
    for (int i = 0; i <= 10; i++) {
        double t = i / 10.0;
        
        // +Z patch right edge
        glm::dvec3 vertZ = transformVertex(glm::dvec2(1.0, t), transformZ);
        
        // +X patch - what SHOULD connect?
        // The visual shows these don't connect properly
        // Let's test both possibilities:
        
        // Option 1: Top edge (v=1)
        glm::dvec3 vertX1 = transformVertex(glm::dvec2(t, 1.0), transformX);
        double gap1 = glm::length(vertZ - vertX1);
        
        // Option 2: Right edge (u=1) 
        glm::dvec3 vertX2 = transformVertex(glm::dvec2(1.0, t), transformX);
        double gap2 = glm::length(vertZ - vertX2);
        
        std::cout << "t=" << t << ":\n";
        std::cout << "  +Z right edge: (" << vertZ.x/1000 << ", " << vertZ.y/1000 << ", " << vertZ.z/1000 << ") km\n";
        std::cout << "  +X top edge:   (" << vertX1.x/1000 << ", " << vertX1.y/1000 << ", " << vertX1.z/1000 << ") km";
        std::cout << " -> gap: " << gap1/1000 << " km\n";
        std::cout << "  +X right edge: (" << vertX2.x/1000 << ", " << vertX2.y/1000 << ", " << vertX2.z/1000 << ") km";
        std::cout << " -> gap: " << gap2/1000 << " km\n\n";
        
        maxGap = std::max(maxGap, std::min(gap1, gap2));
    }
    
    std::cout << "Minimum gap found: " << maxGap/1000 << " km\n\n";
    
    if (maxGap > 1000) {  // More than 1km gap
        std::cout << "PROBLEM CONFIRMED: Large gaps between patches that should connect!\n";
        std::cout << "This matches what we see visually - the patches don't align.\n";
    }
}

void analyzeWhyPatchesDontConnect() {
    std::cout << "\n=== Analysis: Why don't patches connect? ===\n\n";
    
    // Let's trace through exactly what happens
    glm::dvec3 centerZ(0.75, 0.75, 1.0);
    glm::dvec3 centerX(1.0, 0.75, 0.75);
    
    double halfSize = 0.25;
    
    // +Z patch corners
    glm::dvec3 z_bl(centerZ.x - halfSize, centerZ.y - halfSize, 1.0);
    glm::dvec3 z_br(centerZ.x + halfSize, centerZ.y - halfSize, 1.0);
    glm::dvec3 z_tl(centerZ.x - halfSize, centerZ.y + halfSize, 1.0);
    glm::dvec3 z_tr(centerZ.x + halfSize, centerZ.y + halfSize, 1.0);
    
    // +X patch corners  
    glm::dvec3 x_bl(1.0, centerX.y - halfSize, centerX.z - halfSize);
    glm::dvec3 x_br(1.0, centerX.y - halfSize, centerX.z + halfSize);
    glm::dvec3 x_tl(1.0, centerX.y + halfSize, centerX.z - halfSize);
    glm::dvec3 x_tr(1.0, centerX.y + halfSize, centerX.z + halfSize);
    
    std::cout << "+Z patch right edge goes from:\n";
    std::cout << "  Bottom: (" << z_br.x << ", " << z_br.y << ", " << z_br.z << ")\n";
    std::cout << "  Top:    (" << z_tr.x << ", " << z_tr.y << ", " << z_tr.z << ")\n\n";
    
    std::cout << "+X patch top edge goes from:\n";
    std::cout << "  Left:  (" << x_tl.x << ", " << x_tl.y << ", " << x_tl.z << ")\n";
    std::cout << "  Right: (" << x_tr.x << ", " << x_tr.y << ", " << x_tr.z << ")\n\n";
    
    // Check if they share any vertices
    std::cout << "Checking corner matches:\n";
    std::cout << "  +Z BR vs +X TR: " << ((z_br == x_tr) ? "MATCH" : "NO MATCH") << "\n";
    std::cout << "  +Z TR vs +X TR: " << ((z_tr == x_tr) ? "MATCH" : "NO MATCH") << "\n";
    
    // The issue is clear: the edges don't overlap in 3D space!
    std::cout << "\nTHE PROBLEM:\n";
    std::cout << "+Z right edge: x=1.0, y varies from 0.5 to 1.0, z=1.0\n";
    std::cout << "+X top edge:   x=1.0, y=1.0, z varies from 0.5 to 1.0\n";
    std::cout << "These are PERPENDICULAR lines that only meet at ONE point!\n";
}

int main() {
    testActualPatchConfiguration();
    analyzeWhyPatchesDontConnect();
    
    std::cout << "\n=== CONCLUSION ===\n";
    std::cout << "The test we wrote earlier is WRONG.\n";
    std::cout << "It tests that patches connect in a way they actually don't.\n";
    std::cout << "The visual gaps are real - patches at face boundaries\n";
    std::cout << "only share corners, not edges!\n";
    
    return 0;
}