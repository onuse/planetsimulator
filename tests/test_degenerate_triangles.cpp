#include <iostream>
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

// Test to understand why we have degenerate triangles

int main() {
    std::cout << "=== Investigating Degenerate Triangle Issue ===\n\n";
    
    // A degenerate triangle has area < 0.001 according to the code
    // This happens when vertices are:
    // 1. Colinear (on the same line)
    // 2. Very close together
    // 3. Two or more vertices are identical
    
    // Let's simulate what might be happening
    // Theory: At patch boundaries, snapping might be creating duplicate vertices
    
    const double BOUNDARY = 1.0;
    const double EPSILON = 1e-8;
    
    // Simulate vertices near a boundary
    glm::dvec3 v1(0.999999999, 0.5, 0.5);
    glm::dvec3 v2(1.000000001, 0.5, 0.5);
    glm::dvec3 v3(1.0, 0.50001, 0.5);
    
    std::cout << "Before snapping:\n";
    std::cout << "v1: (" << v1.x << ", " << v1.y << ", " << v1.z << ")\n";
    std::cout << "v2: (" << v2.x << ", " << v2.y << ", " << v2.z << ")\n";
    std::cout << "v3: (" << v3.x << ", " << v3.y << ", " << v3.z << ")\n";
    
    // Calculate area before snapping
    glm::dvec3 edge1 = v2 - v1;
    glm::dvec3 edge2 = v3 - v1;
    double area_before = glm::length(glm::cross(edge1, edge2)) * 0.5;
    std::cout << "Triangle area before: " << area_before << "\n\n";
    
    // Apply snapping (like in cpu_vertex_generator.cpp)
    if (std::abs(std::abs(v1.x) - BOUNDARY) < EPSILON) {
        v1.x = (v1.x > 0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(v2.x) - BOUNDARY) < EPSILON) {
        v2.x = (v2.x > 0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(v3.x) - BOUNDARY) < EPSILON) {
        v3.x = (v3.x > 0) ? BOUNDARY : -BOUNDARY;
    }
    
    std::cout << "After snapping:\n";
    std::cout << "v1: (" << v1.x << ", " << v1.y << ", " << v1.z << ")\n";
    std::cout << "v2: (" << v2.x << ", " << v2.y << ", " << v2.z << ")\n";
    std::cout << "v3: (" << v3.x << ", " << v3.y << ", " << v3.z << ")\n";
    
    // Calculate area after snapping
    edge1 = v2 - v1;
    edge2 = v3 - v1;
    double area_after = glm::length(glm::cross(edge1, edge2)) * 0.5;
    std::cout << "Triangle area after: " << area_after << "\n";
    
    if (area_after < 0.001) {
        std::cout << "DEGENERATE TRIANGLE CREATED BY SNAPPING!\n";
    }
    
    std::cout << "\n=== Grid Resolution Issue ===\n";
    std::cout << "With 65x65 grid (4225 vertices), at patch boundaries:\n";
    
    // Simulate grid vertices at boundary
    const int gridRes = 65;
    double spacing = 2.0 / (gridRes - 1);  // Patch goes from -1 to 1
    
    std::cout << "Grid spacing: " << spacing << "\n";
    std::cout << "At boundary X=1.0:\n";
    
    std::vector<glm::dvec3> boundaryVerts;
    for (int i = 0; i < 5; i++) {
        double y = -1.0 + i * spacing;
        boundaryVerts.push_back(glm::dvec3(1.0, y, 0.5));
        std::cout << "  Vertex " << i << ": (1.0, " << y << ", 0.5)\n";
    }
    
    // Check if boundary vertices would create degenerate triangles
    std::cout << "\nTriangles at boundary:\n";
    for (int i = 0; i < 3; i++) {
        glm::dvec3 a = boundaryVerts[i];
        glm::dvec3 b = boundaryVerts[i+1];
        glm::dvec3 c = glm::dvec3(0.99, a.y + spacing/2, 0.5);  // Interior vertex
        
        glm::dvec3 e1 = b - a;
        glm::dvec3 e2 = c - a;
        double area = glm::length(glm::cross(e1, e2)) * 0.5;
        
        std::cout << "  Triangle " << i << " area: " << area;
        if (area < 0.001) {
            std::cout << " - DEGENERATE!";
        }
        std::cout << "\n";
    }
    
    std::cout << "\n=== CONCLUSION ===\n";
    std::cout << "Degenerate triangles likely occur at patch boundaries where:\n";
    std::cout << "1. Vertices are snapped to exact boundary values\n";
    std::cout << "2. Multiple vertices collapse to the same position\n";
    std::cout << "3. Triangles become extremely thin slivers\n";
    std::cout << "\nThis could create a 'hole' if many triangles in a row are degenerate.\n";
    
    return 0;
}