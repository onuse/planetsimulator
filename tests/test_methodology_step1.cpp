// METHODOLOGY: Start from ZERO, add complexity until we find the EXACT problem
// STEP 1: Can we generate a simple patch mesh at all?

#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <fstream>

// First, let's just make sure we can create a basic mesh grid
struct TestResult {
    bool passed;
    std::string description;
    std::string details;
};

TestResult testBasicGrid() {
    const int GRID_SIZE = 3; // 3x3 grid
    std::vector<glm::vec3> vertices;
    
    // Generate a simple grid
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float u = static_cast<float>(x) / (GRID_SIZE - 1);
            float v = static_cast<float>(y) / (GRID_SIZE - 1);
            vertices.push_back(glm::vec3(u, v, 0));
        }
    }
    
    // Verify we got what we expected
    if (vertices.size() != 9) {
        return {false, "Basic grid generation", "Wrong vertex count: " + std::to_string(vertices.size())};
    }
    
    // Check corners are where we expect
    if (vertices[0] != glm::vec3(0, 0, 0)) {
        return {false, "Basic grid generation", "Bottom-left corner wrong"};
    }
    if (vertices[8] != glm::vec3(1, 1, 0)) {
        return {false, "Basic grid generation", "Top-right corner wrong"};
    }
    
    return {true, "Basic grid generation", "9 vertices generated correctly"};
}

TestResult testTwoGridsAdjacent() {
    const int GRID_SIZE = 3;
    std::vector<glm::vec3> grid1, grid2;
    
    // Grid 1: [0,1] x [0,1]
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float u = static_cast<float>(x) / (GRID_SIZE - 1);
            float v = static_cast<float>(y) / (GRID_SIZE - 1);
            grid1.push_back(glm::vec3(u, v, 0));
        }
    }
    
    // Grid 2: [1,2] x [0,1] - shares edge at x=1
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float u = 1.0f + static_cast<float>(x) / (GRID_SIZE - 1);
            float v = static_cast<float>(y) / (GRID_SIZE - 1);
            grid2.push_back(glm::vec3(u, v, 0));
        }
    }
    
    // Check: Do the shared edges match?
    std::string mismatches;
    int mismatchCount = 0;
    
    for (int y = 0; y < GRID_SIZE; y++) {
        int idx1 = y * GRID_SIZE + (GRID_SIZE - 1); // Right edge of grid1
        int idx2 = y * GRID_SIZE + 0;               // Left edge of grid2
        
        float dist = glm::length(grid1[idx1] - grid2[idx2]);
        if (dist > 0.0001f) {
            mismatchCount++;
            mismatches += "Y=" + std::to_string(y) + " dist=" + std::to_string(dist) + " ";
        }
    }
    
    if (mismatchCount > 0) {
        return {false, "Adjacent grids", "Edges don't match: " + mismatches};
    }
    
    return {true, "Adjacent grids", "All edge vertices match perfectly"};
}

TestResult testFloatingPointPrecision() {
    // Test if floating point math could be causing issues
    float step = 1.0f / 64.0f; // Common grid resolution
    
    float accumulated = 0.0f;
    for (int i = 0; i < 64; i++) {
        accumulated += step;
    }
    
    float direct = 64.0f * step;
    float difference = std::abs(accumulated - direct);
    
    if (difference > 1e-6f) {
        return {false, "Float precision", 
                "Accumulated error: " + std::to_string(difference)};
    }
    
    // Test at planet scale
    float planetRadius = 6371000.0f;
    float scaledStep = planetRadius / 64.0f;
    
    accumulated = 0.0f;
    for (int i = 0; i < 64; i++) {
        accumulated += scaledStep;
    }
    
    direct = 64.0f * scaledStep;
    difference = std::abs(accumulated - direct);
    
    if (difference > 1.0f) { // 1 meter tolerance at planet scale
        return {false, "Float precision at planet scale", 
                "Accumulated error: " + std::to_string(difference) + " meters"};
    }
    
    return {true, "Float precision", "Acceptable error at all scales"};
}

void exportDebugMesh(const std::string& filename, const std::vector<glm::vec3>& vertices) {
    std::ofstream file(filename);
    for (const auto& v : vertices) {
        file << "v " << v.x << " " << v.y << " " << v.z << "\n";
    }
    file.close();
}

int main() {
    std::cout << "=== SYSTEMATIC ISOLATION TESTING ===\n";
    std::cout << "Starting from the absolute basics...\n\n";
    
    std::vector<TestResult> results;
    
    // Run tests in order of increasing complexity
    results.push_back(testBasicGrid());
    results.push_back(testTwoGridsAdjacent());
    results.push_back(testFloatingPointPrecision());
    
    // Print results
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : results) {
        if (result.passed) {
            std::cout << "[✓] " << result.description << "\n";
            std::cout << "    " << result.details << "\n";
            passed++;
        } else {
            std::cout << "[✗] " << result.description << "\n";
            std::cout << "    ERROR: " << result.details << "\n";
            failed++;
        }
        std::cout << "\n";
    }
    
    std::cout << "=== SUMMARY ===\n";
    std::cout << "Passed: " << passed << "/" << results.size() << "\n";
    
    if (failed == 0) {
        std::cout << "\nAll basic tests pass. The fundamentals are correct.\n";
        std::cout << "NEXT STEP: Add the actual production vertex generation.\n";
    } else {
        std::cout << "\nBasic tests failing! Fix these before proceeding.\n";
        std::cout << "DO NOT touch production code until these pass.\n";
    }
    
    return failed;
}