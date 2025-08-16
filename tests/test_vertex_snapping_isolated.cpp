// ISOLATED TEST: Just the vertex snapping logic
#include <iostream>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

// Extract JUST the snapping logic from cpu_vertex_generator.cpp
glm::dvec3 applySnapping(glm::dvec3 cubePos) {
    const double BOUNDARY = 1.0;
    const double EPSILON = 1e-8;
    
    // First snapping pass
    if (std::abs(std::abs(cubePos.x) - BOUNDARY) < EPSILON) {
        cubePos.x = (cubePos.x > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(cubePos.y) - BOUNDARY) < EPSILON) {
        cubePos.y = (cubePos.y > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(cubePos.z) - BOUNDARY) < EPSILON) {
        cubePos.z = (cubePos.z > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    
    // Second snapping pass - THE PROBLEMATIC ONE
    int boundaryCount = 0;
    if (std::abs(std::abs(cubePos.x) - BOUNDARY) < 0.01) boundaryCount++;
    if (std::abs(std::abs(cubePos.y) - BOUNDARY) < 0.01) boundaryCount++;
    if (std::abs(std::abs(cubePos.z) - BOUNDARY) < 0.01) boundaryCount++;
    
    if (boundaryCount >= 2) {
        if (std::abs(std::abs(cubePos.x) - BOUNDARY) < 0.01) {
            cubePos.x = std::round(cubePos.x);  // THIS IS WRONG!
        }
        if (std::abs(std::abs(cubePos.y) - BOUNDARY) < 0.01) {
            cubePos.y = std::round(cubePos.y);  // THIS IS WRONG!
        }
        if (std::abs(std::abs(cubePos.z) - BOUNDARY) < 0.01) {
            cubePos.z = std::round(cubePos.z);  // THIS IS WRONG!
        }
    }
    
    return cubePos;
}

// Better snapping logic
glm::dvec3 applySnappingFixed(glm::dvec3 cubePos) {
    const double BOUNDARY = 1.0;
    const double EPSILON = 1e-8;
    
    // Only snap if VERY close to boundary
    if (std::abs(std::abs(cubePos.x) - BOUNDARY) < EPSILON) {
        cubePos.x = (cubePos.x > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(cubePos.y) - BOUNDARY) < EPSILON) {
        cubePos.y = (cubePos.y > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(cubePos.z) - BOUNDARY) < EPSILON) {
        cubePos.z = (cubePos.z > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    
    // NO SECOND PASS - std::round() is completely wrong here!
    
    return cubePos;
}

int main() {
    std::cout << "=== VERTEX SNAPPING ISOLATION TEST ===\n\n";
    
    struct TestCase {
        glm::dvec3 input;
        const char* description;
    };
    
    TestCase cases[] = {
        // Normal interior points
        { glm::dvec3(0.5, 0.0, 0.0), "Interior point" },
        { glm::dvec3(0.9, 0.0, 0.0), "Near boundary but not at it" },
        
        // Points that should snap
        { glm::dvec3(0.999999999, 0.0, 0.0), "Very close to +X boundary" },
        { glm::dvec3(1.000000001, 0.0, 0.0), "Just past +X boundary" },
        
        // Edge cases that break with std::round
        { glm::dvec3(0.995, 0.995, 0.0), "Near edge - PROBLEM CASE" },
        { glm::dvec3(1.0, 0.995, 0.0), "On X boundary, near Y - PROBLEM CASE" },
        { glm::dvec3(0.995, 0.995, 0.995), "Near corner - PROBLEM CASE" },
        
        // Exact boundaries
        { glm::dvec3(1.0, 0.0, 0.0), "Exact +X boundary" },
        { glm::dvec3(1.0, 1.0, 0.0), "Exact edge" },
        { glm::dvec3(1.0, 1.0, 1.0), "Exact corner" },
    };
    
    for (const auto& test : cases) {
        glm::dvec3 original = applySnapping(test.input);
        glm::dvec3 fixed = applySnappingFixed(test.input);
        
        std::cout << test.description << ":\n";
        std::cout << "  Input:    (" << test.input.x << ", " << test.input.y << ", " << test.input.z << ")\n";
        std::cout << "  Original: (" << original.x << ", " << original.y << ", " << original.z << ")";
        
        // Check if std::round messed it up
        bool roundProblem = false;
        if (std::abs(original.x - test.input.x) > 0.01 && std::abs(test.input.x) < 0.99) {
            std::cout << " <- X ROUNDED TO " << original.x << "!";
            roundProblem = true;
        }
        if (std::abs(original.y - test.input.y) > 0.01 && std::abs(test.input.y) < 0.99) {
            std::cout << " <- Y ROUNDED TO " << original.y << "!";
            roundProblem = true;
        }
        if (std::abs(original.z - test.input.z) > 0.01 && std::abs(test.input.z) < 0.99) {
            std::cout << " <- Z ROUNDED TO " << original.z << "!";
            roundProblem = true;
        }
        
        std::cout << "\n";
        std::cout << "  Fixed:    (" << fixed.x << ", " << fixed.y << ", " << fixed.z << ")\n";
        
        if (roundProblem) {
            std::cout << "  *** PROBLEM: std::round() incorrectly modified coordinates! ***\n";
        }
        
        std::cout << "\n";
    }
    
    std::cout << "=== CONCLUSION ===\n";
    std::cout << "The std::round() in the second snapping pass is WRONG!\n";
    std::cout << "It rounds coordinates to 0 or ±1, destroying vertex positions.\n";
    std::cout << "This creates degenerate triangles when multiple vertices collapse to the same point.\n";
    
    return 0;
}