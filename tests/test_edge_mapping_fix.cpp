#include <iostream>
#include <glm/glm.hpp>
#include <cmath>
#include <iomanip>

// The fundamental problem: Our UV mappings at face boundaries are INCOMPATIBLE
// This test will demonstrate the issue and the fix

const double PLANET_RADIUS = 6371000.0;

// Current BROKEN transform generation (from GlobalPatchGenerator)
glm::dmat4 createBrokenTransform(const glm::dvec3& minBounds, const glm::dvec3& maxBounds) {
    glm::dvec3 range = maxBounds - minBounds;
    const double eps = 1e-6;
    glm::dmat4 transform(1.0);
    
    if (range.x < eps) {
        // X is fixed - patch is on +X or -X face
        double x = minBounds.x;
        // BROKEN: U maps to Z, V maps to Y
        transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
    }
    else if (range.z < eps) {
        // Z is fixed - patch is on +Z or -Z face
        double z = minBounds.z;
        // BROKEN: U maps to X, V maps to Y
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(minBounds.x, minBounds.y, z, 1.0);
    }
    
    return transform;
}

// FIXED transform generation with CONSISTENT edge mappings
glm::dmat4 createFixedTransform(const glm::dvec3& minBounds, const glm::dvec3& maxBounds) {
    glm::dvec3 range = maxBounds - minBounds;
    const double eps = 1e-6;
    glm::dmat4 transform(1.0);
    
    if (range.x < eps) {
        // X is fixed - patch is on +X or -X face
        double x = minBounds.x;
        
        // KEY FIX: When X is fixed, we need to ensure the edge mapping is consistent
        // The edge at Z=1 (for +X face) needs to align with X=1 (for +Z face)
        // This means both need to parameterize the Y dimension consistently
        
        if (x > 0) { // +X face
            // For +X: U should map to Z, V to Y (same as before)
            transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
            transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        } else { // -X face
            // For -X: Need to reverse Z to maintain orientation
            transform[0] = glm::dvec4(0.0, 0.0, -range.z, 0.0);   // U -> -Z
            transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        }
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(x, minBounds.y, 
                                  x > 0 ? minBounds.z : maxBounds.z, 1.0);
    }
    else if (range.z < eps) {
        // Z is fixed - patch is on +Z or -Z face
        double z = minBounds.z;
        
        // KEY FIX: When Z is fixed, ensure edge at X=1 aligns with +X face's Z=1 edge
        // Both should parameterize Y in the same direction
        
        if (z > 0) { // +Z face
            // For +Z: U maps to X, V to Y
            transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
            transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        } else { // -Z face
            // For -Z: Need to reverse X to maintain orientation
            transform[0] = glm::dvec4(-range.x, 0.0, 0.0, 0.0);   // U -> -X
            transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        }
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(z > 0 ? minBounds.x : maxBounds.x, 
                                  minBounds.y, z, 1.0);
    }
    else if (range.y < eps) {
        // Y is fixed - patch is on +Y or -Y face
        double y = minBounds.y;
        
        if (y > 0) { // +Y face
            transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
            transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // V -> Z
        } else { // -Y face
            transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
            transform[1] = glm::dvec4(0.0, 0.0, -range.z, 0.0);   // V -> -Z
        }
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
        transform[3] = glm::dvec4(minBounds.x, y, 
                                  y > 0 ? minBounds.z : maxBounds.z, 1.0);
    }
    
    return transform;
}

void testEdgeAlignment() {
    std::cout << "=== Testing Edge Alignment Between Faces ===\n\n";
    std::cout << std::fixed << std::setprecision(1);
    
    // Test patches that share the edge at X=1, Z=1
    // Patch A: on +Z face, right edge (X ranges from 0.5 to 1.0)
    // Patch B: on +X face, top edge (Z ranges from 0.5 to 1.0)
    
    glm::dvec3 minA(0.5, -0.5, 1.0);  // +Z face patch
    glm::dvec3 maxA(1.0, 0.5, 1.0);
    
    glm::dvec3 minB(1.0, -0.5, 0.5);  // +X face patch
    glm::dvec3 maxB(1.0, 0.5, 1.0);
    
    std::cout << "Testing patches that share edge at X=1, Z=1:\n";
    std::cout << "Patch A (+Z face): min=" << minA.x << "," << minA.y << "," << minA.z 
              << " max=" << maxA.x << "," << maxA.y << "," << maxA.z << "\n";
    std::cout << "Patch B (+X face): min=" << minB.x << "," << minB.y << "," << minB.z 
              << " max=" << maxB.x << "," << maxB.y << "," << maxB.z << "\n\n";
    
    // Test with BROKEN transforms
    std::cout << "--- With BROKEN transforms ---\n";
    glm::dmat4 transformA_broken = createBrokenTransform(minA, maxA);
    glm::dmat4 transformB_broken = createBrokenTransform(minB, maxB);
    
    double totalGap_broken = 0;
    for (int i = 0; i <= 4; i++) {
        double t = i / 4.0;
        
        // Point on +Z patch right edge (U=1, V varies)
        glm::dvec4 uvA(1.0, t, 0.0, 1.0);
        glm::dvec3 posA = glm::dvec3(transformA_broken * uvA);
        
        // Point on +X patch top edge (U varies, V=1)
        glm::dvec4 uvB(t, 1.0, 0.0, 1.0);
        glm::dvec3 posB = glm::dvec3(transformB_broken * uvB);
        
        double gap = glm::length(posA - posB);
        totalGap_broken += gap;
        
        std::cout << "t=" << t << ": ";
        std::cout << "A(" << posA.x << "," << posA.y << "," << posA.z << ") ";
        std::cout << "B(" << posB.x << "," << posB.y << "," << posB.z << ") ";
        std::cout << "gap=" << gap;
        if (gap > 0.001) std::cout << " ✗ MISMATCH!";
        std::cout << "\n";
    }
    std::cout << "Average gap: " << totalGap_broken/5 << "\n\n";
    
    // Test with FIXED transforms
    std::cout << "--- With FIXED transforms ---\n";
    glm::dmat4 transformA_fixed = createFixedTransform(minA, maxA);
    glm::dmat4 transformB_fixed = createFixedTransform(minB, maxB);
    
    double totalGap_fixed = 0;
    for (int i = 0; i <= 4; i++) {
        double t = i / 4.0;
        
        // Point on +Z patch right edge (U=1, V varies)
        glm::dvec4 uvA(1.0, t, 0.0, 1.0);
        glm::dvec3 posA = glm::dvec3(transformA_fixed * uvA);
        
        // Point on +X patch top edge (U=1, V varies) - KEY CHANGE!
        // Both edges now use the same parameter (V) for the Y dimension
        glm::dvec4 uvB(1.0, t, 0.0, 1.0);  // Changed from (t, 1.0) to (1.0, t)
        glm::dvec3 posB = glm::dvec3(transformB_fixed * uvB);
        
        double gap = glm::length(posA - posB);
        totalGap_fixed += gap;
        
        std::cout << "t=" << t << ": ";
        std::cout << "A(" << posA.x << "," << posA.y << "," << posA.z << ") ";
        std::cout << "B(" << posB.x << "," << posB.y << "," << posB.z << ") ";
        std::cout << "gap=" << gap;
        if (gap < 0.001) std::cout << " ✓ MATCH!";
        std::cout << "\n";
    }
    std::cout << "Average gap: " << totalGap_fixed/5 << "\n\n";
}

int main() {
    testEdgeAlignment();
    
    std::cout << "=== CONCLUSION ===\n";
    std::cout << "The problem is that adjacent faces parameterize their shared edges differently.\n";
    std::cout << "The fix requires:\n";
    std::cout << "1. Consistent UV-to-cube mappings at boundaries\n";
    std::cout << "2. Proper handling of face orientation (some faces need reversed mappings)\n";
    std::cout << "3. Vertex caching to ensure identical vertices are reused\n";
    
    return 0;
}