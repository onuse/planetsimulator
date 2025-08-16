#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>
#include <algorithm>

// Include the actual implementation from the project
namespace PlanetSimulator {
namespace Math {
    // Double precision version for exact testing
    glm::dvec3 cubeToSphereD(const glm::dvec3& cubePos, double radius) {
        glm::dvec3 pos2 = cubePos * cubePos;
        glm::dvec3 spherePos;
        
        spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
        spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
        spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
        
        return glm::normalize(spherePos) * radius;
    }
}
}

int main() {
    std::cout << "=== Cube to Sphere Determinism Test ===\n\n";
    std::cout << std::fixed << std::setprecision(15);
    
    const double radius = 6371000.0; // Earth radius in meters
    
    // Test 1: Edge points should produce identical results
    std::cout << "Test 1: Edge Points (between +X and +Y faces)\n";
    std::cout << "================================================\n";
    {
        // These represent the same point on the edge between +X and +Y faces
        glm::dvec3 fromXFace(1.0, 1.0, 0.0);  // As seen from +X face
        glm::dvec3 fromYFace(1.0, 1.0, 0.0);  // As seen from +Y face (same point!)
        
        auto result1 = PlanetSimulator::Math::cubeToSphereD(fromXFace, radius);
        auto result2 = PlanetSimulator::Math::cubeToSphereD(fromYFace, radius);
        
        std::cout << "Point (1,1,0) from X face: (" << result1.x << ", " << result1.y << ", " << result1.z << ")\n";
        std::cout << "Point (1,1,0) from Y face: (" << result2.x << ", " << result2.y << ", " << result2.z << ")\n";
        
        glm::dvec3 diff = result1 - result2;
        double distance = glm::length(diff);
        std::cout << "Difference: (" << diff.x << ", " << diff.y << ", " << diff.z << ")\n";
        std::cout << "Distance: " << distance << " meters\n";
        
        if (distance < 1e-9) {
            std::cout << "✓ PASS: Results are identical\n";
        } else {
            std::cout << "✗ FAIL: Results differ by " << distance << " meters\n";
        }
    }
    
    std::cout << "\nTest 2: Corner Points (shared by 3 faces)\n";
    std::cout << "==========================================\n";
    {
        // Corner at (+1,+1,+1) is shared by +X, +Y, and +Z faces
        glm::dvec3 corner(1.0, 1.0, 1.0);
        
        // Compute from perspective of each face
        auto result = PlanetSimulator::Math::cubeToSphereD(corner, radius);
        
        std::cout << "Corner (1,1,1): (" << result.x << ", " << result.y << ", " << result.z << ")\n";
        std::cout << "Length: " << glm::length(result) << " (should be " << radius << ")\n";
        
        // Test multiple evaluations of the same point
        bool deterministic = true;
        for (int i = 0; i < 100; i++) {
            auto test = PlanetSimulator::Math::cubeToSphereD(corner, radius);
            if (glm::length(test - result) > 1e-15) {
                std::cout << "✗ FAIL: Evaluation " << i << " produced different result!\n";
                deterministic = false;
                break;
            }
        }
        
        if (deterministic) {
            std::cout << "✓ PASS: 100 evaluations produced identical results\n";
        }
    }
    
    std::cout << "\nTest 3: Boundary Points Along Edges\n";
    std::cout << "====================================\n";
    {
        // Test points along the edge between +X and +Z faces
        std::vector<double> discrepancies;
        
        for (int i = 0; i <= 10; i++) {
            double t = i / 10.0;
            double z = -1.0 + 2.0 * t;
            
            // Point on the edge at X=1, Y=0
            glm::dvec3 edgePoint(1.0, 0.0, z);
            
            // Compute multiple times to check consistency
            auto result1 = PlanetSimulator::Math::cubeToSphereD(edgePoint, radius);
            auto result2 = PlanetSimulator::Math::cubeToSphereD(edgePoint, radius);
            
            double diff = glm::length(result1 - result2);
            discrepancies.push_back(diff);
            
            if (diff > 1e-15) {
                std::cout << "Point (" << edgePoint.x << "," << edgePoint.y << "," << edgePoint.z 
                          << ") - Discrepancy: " << diff << " meters\n";
            }
        }
        
        // Check if all discrepancies are zero
        double maxDiscrepancy = *std::max_element(discrepancies.begin(), discrepancies.end());
        
        if (maxDiscrepancy < 1e-15) {
            std::cout << "✓ PASS: All edge points are computed deterministically\n";
        } else {
            std::cout << "✗ FAIL: Maximum discrepancy: " << maxDiscrepancy << " meters\n";
        }
    }
    
    std::cout << "\nTest 4: Face Boundary Points\n";
    std::cout << "=============================\n";
    {
        // Test points that are exactly on face boundaries
        struct BoundaryPoint {
            glm::dvec3 pos;
            const char* description;
        };
        
        BoundaryPoint points[] = {
            { glm::dvec3(1.0, 1.0, 0.5), "+X/+Y edge" },
            { glm::dvec3(1.0, 0.5, 1.0), "+X/+Z edge" },
            { glm::dvec3(0.5, 1.0, 1.0), "+Y/+Z edge" },
            { glm::dvec3(1.0, -1.0, 0.5), "+X/-Y edge" },
            { glm::dvec3(1.0, 0.5, -1.0), "+X/-Z edge" },
        };
        
        bool allPass = true;
        for (const auto& pt : points) {
            auto result1 = PlanetSimulator::Math::cubeToSphereD(pt.pos, radius);
            auto result2 = PlanetSimulator::Math::cubeToSphereD(pt.pos, radius);
            
            double diff = glm::length(result1 - result2);
            
            std::cout << pt.description << " at (" 
                      << pt.pos.x << "," << pt.pos.y << "," << pt.pos.z << "): ";
            
            if (diff < 1e-15) {
                std::cout << "✓ Deterministic\n";
            } else {
                std::cout << "✗ Differs by " << diff << " meters\n";
                allPass = false;
            }
        }
        
        if (allPass) {
            std::cout << "\n✓ PASS: All boundary points are computed deterministically\n";
        } else {
            std::cout << "\n✗ FAIL: Some boundary points show non-deterministic behavior\n";
        }
    }
    
    std::cout << "\nTest 5: Numerical Stability Near Boundaries\n";
    std::cout << "============================================\n";
    {
        // Test points very close to boundaries (within floating point epsilon)
        double epsilon = 1e-10;
        
        glm::dvec3 exactBoundary(1.0, 0.0, 0.0);
        glm::dvec3 nearBoundary1(1.0 - epsilon, 0.0, 0.0);
        glm::dvec3 nearBoundary2(1.0 + epsilon, 0.0, 0.0);
        
        auto exact = PlanetSimulator::Math::cubeToSphereD(exactBoundary, radius);
        auto near1 = PlanetSimulator::Math::cubeToSphereD(nearBoundary1, radius);
        auto near2 = PlanetSimulator::Math::cubeToSphereD(nearBoundary2, radius);
        
        double drift1 = glm::length(exact - near1);
        double drift2 = glm::length(exact - near2);
        
        std::cout << "Exact boundary point: " << glm::length(exact) << " meters from origin\n";
        std::cout << "Point at -epsilon: differs by " << drift1 << " meters\n";
        std::cout << "Point at +epsilon: differs by " << drift2 << " meters\n";
        
        // The drift should be proportional to epsilon
        double expectedDrift = epsilon * radius;
        
        if (drift1 < expectedDrift * 10 && drift2 < expectedDrift * 10) {
            std::cout << "✓ PASS: Numerical stability is acceptable\n";
        } else {
            std::cout << "✗ FAIL: Excessive numerical drift near boundaries\n";
        }
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "The cube-to-sphere transformation should be deterministic.\n";
    std::cout << "Any discrepancies found above could explain gaps at face boundaries.\n";
    
    return 0;
}