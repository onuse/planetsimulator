#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <map>

// Include the actual vertex generation components
#include "rendering/cpu_vertex_generator.hpp"
#include "core/global_patch_generator.hpp"
#include "core/spherical_quadtree.hpp"

bool testBoundaryVertexConsistency() {
    std::cout << "=== Testing Boundary Vertex Generation Consistency ===\n\n";
    
    // Create vertex generator with specific config
    rendering::CPUVertexGenerator::Config config;
    config.planetRadius = 6371000.0;
    config.gridResolution = 65;  // Standard resolution
    config.enableSkirts = false;  // Focus on main vertices
    config.enableVertexCaching = true;
    config.maxCacheSize = 100000;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create two patches that share a boundary
    // Patch 1: On +X face at the +Y boundary
    core::GlobalPatchGenerator::GlobalPatch xPatch;
    xPatch.minBounds = glm::vec3(1.0, 0.5, -0.5);
    xPatch.maxBounds = glm::vec3(1.0, 1.0, 0.5);
    xPatch.center = (xPatch.minBounds + xPatch.maxBounds) * 0.5f;
    xPatch.level = 1;
    xPatch.faceId = 0;
    
    // Patch 2: On +Y face at the +X boundary
    core::GlobalPatchGenerator::GlobalPatch yPatch;
    yPatch.minBounds = glm::vec3(0.5, 1.0, -0.5);
    yPatch.maxBounds = glm::vec3(1.0, 1.0, 0.5);
    yPatch.center = (yPatch.minBounds + yPatch.maxBounds) * 0.5f;
    yPatch.level = 1;
    yPatch.faceId = 2;
    
    // Generate meshes for both patches
    auto xTransform = xPatch.createTransform();
    auto yTransform = yPatch.createTransform();
    
    // Create QuadtreePatch structures
    core::QuadtreePatch xQuadPatch;
    xQuadPatch.center = xPatch.center;
    xQuadPatch.minBounds = xPatch.minBounds;
    xQuadPatch.maxBounds = xPatch.maxBounds;
    xQuadPatch.level = xPatch.level;
    xQuadPatch.faceId = xPatch.faceId;
    xQuadPatch.size = 0.5f;  // Approximate
    xQuadPatch.morphFactor = 0.0f;
    xQuadPatch.screenSpaceError = 0.0f;
    
    core::QuadtreePatch yQuadPatch;
    yQuadPatch.center = yPatch.center;
    yQuadPatch.minBounds = yPatch.minBounds;
    yQuadPatch.maxBounds = yPatch.maxBounds;
    yQuadPatch.level = yPatch.level;
    yQuadPatch.faceId = yPatch.faceId;
    yQuadPatch.size = 0.5f;  // Approximate
    yQuadPatch.morphFactor = 0.0f;
    yQuadPatch.screenSpaceError = 0.0f;
    
    auto xMesh = generator.generatePatchMesh(xQuadPatch, xTransform);
    auto yMesh = generator.generatePatchMesh(yQuadPatch, yTransform);
    
    std::cout << "Generated meshes:\n";
    std::cout << "  X patch: " << xMesh.vertices.size() << " vertices\n";
    std::cout << "  Y patch: " << yMesh.vertices.size() << " vertices\n\n";
    
    // Extract boundary vertices from each mesh
    // For X patch: vertices at v=1 (top edge)
    // For Y patch: vertices at u=1 (right edge)
    std::vector<glm::vec3> xBoundaryVerts;
    std::vector<glm::vec3> yBoundaryVerts;
    
    uint32_t res = config.gridResolution;
    
    // X patch top edge (v=1)
    for (uint32_t x = 0; x < res; x++) {
        uint32_t idx = (res - 1) * res + x;  // Last row
        xBoundaryVerts.push_back(xMesh.vertices[idx].position);
    }
    
    // Y patch right edge (u=1)
    for (uint32_t y = 0; y < res; y++) {
        uint32_t idx = y * res + (res - 1);  // Last column
        yBoundaryVerts.push_back(yMesh.vertices[idx].position);
    }
    
    std::cout << "Comparing " << xBoundaryVerts.size() << " boundary vertices...\n\n";
    
    // Compare corresponding vertices
    double maxGap = 0.0;
    int mismatchCount = 0;
    std::vector<double> gaps;
    
    for (size_t i = 0; i < std::min(xBoundaryVerts.size(), yBoundaryVerts.size()); i++) {
        // Note: vertices might be in different order along the edge
        // For now, assume they correspond directly (both go from -0.5 to 0.5 in Z)
        glm::vec3 diff = xBoundaryVerts[i] - yBoundaryVerts[i];
        double gap = glm::length(diff);
        gaps.push_back(gap);
        
        if (gap > maxGap) {
            maxGap = gap;
        }
        
        if (gap > 1.0) {  // More than 1 meter gap
            mismatchCount++;
            if (mismatchCount <= 5) {  // Show first 5 mismatches
                std::cout << "  Vertex " << i << " gap: " << gap << " meters\n";
                std::cout << "    X patch: (" << xBoundaryVerts[i].x << ", " 
                          << xBoundaryVerts[i].y << ", " << xBoundaryVerts[i].z << ")\n";
                std::cout << "    Y patch: (" << yBoundaryVerts[i].x << ", " 
                          << yBoundaryVerts[i].y << ", " << yBoundaryVerts[i].z << ")\n";
            }
        }
    }
    
    std::cout << "\nResults:\n";
    std::cout << "  Maximum gap: " << maxGap << " meters\n";
    std::cout << "  Vertices with >1m gap: " << mismatchCount << " / " << gaps.size() << "\n";
    
    // Calculate statistics
    double avgGap = 0.0;
    for (double g : gaps) avgGap += g;
    avgGap /= gaps.size();
    std::cout << "  Average gap: " << avgGap << " meters\n";
    
    bool passed = (maxGap < 1.0);  // Less than 1 meter tolerance
    
    if (passed) {
        std::cout << "\n✓ PASS: Boundary vertices are consistent\n";
    } else {
        std::cout << "\n✗ FAIL: Large gaps found at boundaries\n";
    }
    
    return passed;
}

bool testVertexSnapping() {
    std::cout << "\n=== Testing Vertex Snapping Logic ===\n\n";
    
    // Test the snapping behavior directly
    struct SnapTest {
        glm::dvec3 input;
        glm::dvec3 expected;
        const char* description;
    };
    
    SnapTest tests[] = {
        // Exact boundary values should stay exact
        { glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0), "Exact +X boundary" },
        { glm::dvec3(-1.0, 0.0, 0.0), glm::dvec3(-1.0, 0.0, 0.0), "Exact -X boundary" },
        
        // Very close to boundary should snap
        { glm::dvec3(0.999999999, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0), "Near +X boundary" },
        { glm::dvec3(1.000000001, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0), "Just past +X boundary" },
        
        // Edge cases - two boundaries
        { glm::dvec3(1.0, 1.0, 0.0), glm::dvec3(1.0, 1.0, 0.0), "Edge between +X and +Y" },
        { glm::dvec3(0.99999, 0.99999, 0.0), glm::dvec3(1.0, 1.0, 0.0), "Near edge" },
        
        // Corner cases - three boundaries
        { glm::dvec3(1.0, 1.0, 1.0), glm::dvec3(1.0, 1.0, 1.0), "Corner" },
        { glm::dvec3(0.999, 0.999, 0.999), glm::dvec3(1.0, 1.0, 1.0), "Near corner" },
        
        // Interior points should not be snapped
        { glm::dvec3(0.5, 0.5, 0.5), glm::dvec3(0.5, 0.5, 0.5), "Interior point" },
        { glm::dvec3(0.9, 0.0, 0.0), glm::dvec3(0.9, 0.0, 0.0), "Near but not at boundary" },
    };
    
    bool allPassed = true;
    
    for (const auto& test : tests) {
        glm::dvec3 cubePos = test.input;
        
        // Apply the snapping logic from cpu_vertex_generator.cpp
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
        
        // Second snapping pass (the problematic one with std::round)
        int boundaryCount = 0;
        if (std::abs(std::abs(cubePos.x) - BOUNDARY) < 0.01) boundaryCount++;
        if (std::abs(std::abs(cubePos.y) - BOUNDARY) < 0.01) boundaryCount++;
        if (std::abs(std::abs(cubePos.z) - BOUNDARY) < 0.01) boundaryCount++;
        
        if (boundaryCount >= 2) {
            if (std::abs(std::abs(cubePos.x) - BOUNDARY) < 0.01) {
                cubePos.x = std::round(cubePos.x);
            }
            if (std::abs(std::abs(cubePos.y) - BOUNDARY) < 0.01) {
                cubePos.y = std::round(cubePos.y);
            }
            if (std::abs(std::abs(cubePos.z) - BOUNDARY) < 0.01) {
                cubePos.z = std::round(cubePos.z);
            }
        }
        
        bool passed = (glm::length(cubePos - test.expected) < 1e-6);
        
        std::cout << test.description << ": ";
        if (passed) {
            std::cout << "✓ PASS\n";
        } else {
            std::cout << "✗ FAIL\n";
            std::cout << "    Input:    (" << test.input.x << ", " << test.input.y << ", " << test.input.z << ")\n";
            std::cout << "    Expected: (" << test.expected.x << ", " << test.expected.y << ", " << test.expected.z << ")\n";
            std::cout << "    Got:      (" << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")\n";
            allPassed = false;
        }
    }
    
    return allPassed;
}

bool testDegenerateTriangles() {
    std::cout << "\n=== Testing for Degenerate Triangles ===\n\n";
    
    rendering::CPUVertexGenerator::Config config;
    config.planetRadius = 6371000.0;
    config.gridResolution = 65;
    config.enableSkirts = false;
    config.enableVertexCaching = true;
    
    rendering::CPUVertexGenerator generator(config);
    
    // Create a patch at a face boundary where we see issues
    core::GlobalPatchGenerator::GlobalPatch patch;
    patch.minBounds = glm::vec3(1.0, -1.0, -1.0);
    patch.maxBounds = glm::vec3(1.0, 0.0, 0.0);
    patch.center = (patch.minBounds + patch.maxBounds) * 0.5f;
    patch.level = 2;
    patch.faceId = 0;
    
    auto transform = patch.createTransform();
    
    core::QuadtreePatch quadPatch;
    quadPatch.center = patch.center;
    quadPatch.minBounds = patch.minBounds;
    quadPatch.maxBounds = patch.maxBounds;
    quadPatch.level = patch.level;
    quadPatch.faceId = patch.faceId;
    quadPatch.size = 1.0f;
    quadPatch.morphFactor = 0.0f;
    quadPatch.screenSpaceError = 0.0f;
    
    auto mesh = generator.generatePatchMesh(quadPatch, transform);
    
    // Check for degenerate triangles
    int degenerateCount = 0;
    int tinyCount = 0;
    double minArea = 1e10;
    double maxArea = 0.0;
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        if (i + 2 >= mesh.indices.size()) break;
        
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];
        
        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
            continue;
        }
        
        glm::vec3 v0 = mesh.vertices[i0].position;
        glm::vec3 v1 = mesh.vertices[i1].position;
        glm::vec3 v2 = mesh.vertices[i2].position;
        
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        float area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
        
        minArea = std::min(minArea, (double)area);
        maxArea = std::max(maxArea, (double)area);
        
        if (area < 0.001f) {
            degenerateCount++;
        } else if (area < 1.0f) {
            tinyCount++;
        }
    }
    
    std::cout << "Triangle statistics:\n";
    std::cout << "  Total triangles: " << mesh.indices.size() / 3 << "\n";
    std::cout << "  Degenerate (<0.001 m²): " << degenerateCount << "\n";
    std::cout << "  Tiny (<1 m²): " << tinyCount << "\n";
    std::cout << "  Min area: " << minArea << " m²\n";
    std::cout << "  Max area: " << maxArea << " m²\n";
    
    bool passed = (degenerateCount == 0);
    
    if (passed) {
        std::cout << "\n✓ PASS: No degenerate triangles\n";
    } else {
        std::cout << "\n✗ FAIL: Found " << degenerateCount << " degenerate triangles\n";
    }
    
    return passed;
}

int main() {
    std::cout << "=== Vertex Generation Determinism Test ===\n";
    std::cout << "==========================================\n\n";
    
    bool allPassed = true;
    
    // Run all tests
    allPassed &= testVertexSnapping();
    allPassed &= testBoundaryVertexConsistency();
    allPassed &= testDegenerateTriangles();
    
    std::cout << "\n=== OVERALL RESULT ===\n";
    if (allPassed) {
        std::cout << "✓ ALL TESTS PASSED\n";
        return 0;
    } else {
        std::cout << "✗ SOME TESTS FAILED\n";
        return 1;
    }
}