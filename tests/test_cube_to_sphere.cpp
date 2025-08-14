#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

// Test the cube-to-sphere projection
glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    
    return glm::normalize(spherePos);
}

void testCorners() {
    std::cout << "Testing cube corners (should all have length 1.0):\n";
    std::cout << std::fixed << std::setprecision(6);
    
    // Test all 8 corners of the unit cube
    glm::dvec3 corners[] = {
        glm::dvec3( 1,  1,  1),
        glm::dvec3( 1,  1, -1),
        glm::dvec3( 1, -1,  1),
        glm::dvec3( 1, -1, -1),
        glm::dvec3(-1,  1,  1),
        glm::dvec3(-1,  1, -1),
        glm::dvec3(-1, -1,  1),
        glm::dvec3(-1, -1, -1)
    };
    
    for (const auto& corner : corners) {
        glm::dvec3 sphere = cubeToSphere(corner);
        double length = glm::length(sphere);
        std::cout << "  Cube(" << corner.x << "," << corner.y << "," << corner.z 
                  << ") -> Sphere(" << sphere.x << "," << sphere.y << "," << sphere.z 
                  << ") Length: " << length << "\n";
    }
}

void testFaceCenters() {
    std::cout << "\nTesting face centers (should map to axes):\n";
    
    // Face centers should map to unit vectors along axes
    struct FaceTest {
        glm::dvec3 cube;
        glm::dvec3 expected;
        const char* name;
    };
    
    FaceTest faces[] = {
        { glm::dvec3( 1, 0, 0), glm::dvec3( 1, 0, 0), "+X face" },
        { glm::dvec3(-1, 0, 0), glm::dvec3(-1, 0, 0), "-X face" },
        { glm::dvec3(0,  1, 0), glm::dvec3(0,  1, 0), "+Y face" },
        { glm::dvec3(0, -1, 0), glm::dvec3(0, -1, 0), "-Y face" },
        { glm::dvec3(0, 0,  1), glm::dvec3(0, 0,  1), "+Z face" },
        { glm::dvec3(0, 0, -1), glm::dvec3(0, 0, -1), "-Z face" }
    };
    
    for (const auto& face : faces) {
        glm::dvec3 sphere = cubeToSphere(face.cube);
        glm::dvec3 diff = sphere - face.expected;
        double error = glm::length(diff);
        
        std::cout << "  " << face.name << ": Error = " << error;
        if (error < 1e-10) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ (Got " << sphere.x << "," << sphere.y << "," << sphere.z << ")\n";
        }
    }
}

void testPatchGrid() {
    std::cout << "\nTesting 3x3 grid on +Z face:\n";
    
    // Test a small grid of points on the +Z face
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            glm::dvec3 cubePos(x * 0.5, y * 0.5, 1.0);
            glm::dvec3 spherePos = cubeToSphere(cubePos);
            
            std::cout << "  (" << std::setw(4) << cubePos.x 
                      << "," << std::setw(4) << cubePos.y 
                      << "," << std::setw(4) << cubePos.z << ") -> ";
            std::cout << "(" << std::setw(8) << spherePos.x 
                      << "," << std::setw(8) << spherePos.y 
                      << "," << std::setw(8) << spherePos.z 
                      << ") L=" << glm::length(spherePos) << "\n";
        }
    }
}

void testContinuity() {
    std::cout << "\nTesting continuity across edge (should have smooth transition):\n";
    
    // Test points along an edge between two faces
    for (int i = 0; i <= 10; i++) {
        double t = i / 10.0;
        
        // Points along the edge between +X and +Z faces
        glm::dvec3 cubePos(1.0, 0.0, -1.0 + 2.0 * t);
        glm::dvec3 spherePos = cubeToSphere(cubePos);
        
        std::cout << "  t=" << std::setw(3) << t 
                  << " Cube(" << cubePos.x << "," << cubePos.y << "," << cubePos.z 
                  << ") -> Sphere(" << std::setw(8) << spherePos.x 
                  << "," << std::setw(8) << spherePos.y 
                  << "," << std::setw(8) << spherePos.z << ")\n";
    }
}

int main() {
    std::cout << "=== Cube to Sphere Projection Test ===\n\n";
    
    testCorners();
    testFaceCenters();
    testPatchGrid();
    testContinuity();
    
    return 0;
}