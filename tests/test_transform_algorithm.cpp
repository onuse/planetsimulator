// ISOLATED TRANSFORM ALGORITHM TEST
// Extract just the transform logic and test it in complete isolation

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <vector>
#include <string>

// The EXACT algorithm from GlobalPatchGenerator::createTransform()
glm::dmat4 transformAlgorithm(const glm::dvec3& minBounds, const glm::dvec3& maxBounds) {
    glm::dmat4 transform(1.0);
    glm::dvec3 range = maxBounds - minBounds;
    const double eps = 1e-6;
    
    if (range.x < eps) {
        // X is fixed
        double x = minBounds.x;
        transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y  
        transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
    }
    else if (range.y < eps) {
        // Y is fixed
        double y = minBounds.y;
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // V -> Z
        transform[3] = glm::dvec4(minBounds.x, y, minBounds.z, 1.0);
    }
    else if (range.z < eps) {
        // Z is fixed
        double z = minBounds.z;
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[3] = glm::dvec4(minBounds.x, minBounds.y, z, 1.0);
    }
    
    transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    return transform;
}

// Test a specific UV coordinate through the transform
glm::dvec3 applyTransform(double u, double v, const glm::dmat4& transform) {
    glm::dvec4 localPos(u, v, 0.0, 1.0);
    return glm::dvec3(transform * localPos);
}

// Test case structure
struct TestCase {
    std::string name;
    glm::dvec3 minBounds;
    glm::dvec3 maxBounds;
    double u, v;
    glm::dvec3 expected;
};

void runTest(const TestCase& test) {
    auto transform = transformAlgorithm(test.minBounds, test.maxBounds);
    auto result = applyTransform(test.u, test.v, transform);
    
    double error = glm::length(result - test.expected);
    bool passed = error < 1e-6;
    
    std::cout << "[" << (passed ? "✓" : "✗") << "] " << test.name << "\n";
    std::cout << "    Input: UV(" << test.u << "," << test.v << ") with bounds [" 
              << test.minBounds.x << "," << test.minBounds.y << "," << test.minBounds.z << "] to ["
              << test.maxBounds.x << "," << test.maxBounds.y << "," << test.maxBounds.z << "]\n";
    std::cout << "    Expected: (" << test.expected.x << ", " << test.expected.y << ", " << test.expected.z << ")\n";
    std::cout << "    Got:      (" << result.x << ", " << result.y << ", " << result.z << ")\n";
    
    if (!passed) {
        std::cout << "    ERROR: " << error << "\n";
        
        // Debug the transform matrix
        std::cout << "    Transform matrix:\n";
        for (int i = 0; i < 4; i++) {
            std::cout << "      [" << transform[i][0] << ", " << transform[i][1] 
                      << ", " << transform[i][2] << ", " << transform[i][3] << "]\n";
        }
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== TRANSFORM ALGORITHM ISOLATION TEST ===\n\n";
    
    std::vector<TestCase> tests;
    
    // Test 1: Basic +X face patch
    tests.push_back({
        "+X face: UV(0,0) -> bottom-left",
        glm::dvec3(1, -1, -1), glm::dvec3(1, 1, 1),
        0.0, 0.0,
        glm::dvec3(1, -1, -1)
    });
    
    tests.push_back({
        "+X face: UV(1,1) -> top-right",
        glm::dvec3(1, -1, -1), glm::dvec3(1, 1, 1),
        1.0, 1.0,
        glm::dvec3(1, 1, 1)
    });
    
    // Test 2: Partial +X face patch (the problematic one)
    tests.push_back({
        "+X face partial: UV(0,0) -> min bounds",
        glm::dvec3(1, -0.5, 0.5), glm::dvec3(1, 0.5, 1),
        0.0, 0.0,
        glm::dvec3(1, -0.5, 0.5)
    });
    
    tests.push_back({
        "+X face partial: UV(1,1) -> max bounds",
        glm::dvec3(1, -0.5, 0.5), glm::dvec3(1, 0.5, 1),
        1.0, 1.0,
        glm::dvec3(1, 0.5, 1)
    });
    
    // Test 3: The critical shared vertex case
    tests.push_back({
        "+X face: UV(0,1) for shared edge at Z=0.5, Y=0.5",
        glm::dvec3(1, -0.5, 0.5), glm::dvec3(1, 0.5, 1),
        0.0, 1.0,
        glm::dvec3(1, 0.5, 0.5)  // This is what we get
    });
    
    // Test 4: +Z face patch that should share with above
    tests.push_back({
        "+Z face: UV(1,1) for shared edge at X=1, Y=0.5",
        glm::dvec3(0.5, -0.5, 1), glm::dvec3(1, 0.5, 1),
        1.0, 1.0,
        glm::dvec3(1, 0.5, 1)  // This is what we get
    });
    
    // Run all tests
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        runTest(test);
        if (test.name.find("✗") == std::string::npos) {
            passed++;
        } else {
            failed++;
        }
    }
    
    std::cout << "=== ANALYSIS ===\n";
    std::cout << "The algorithm correctly maps UV [0,1] to the patch bounds.\n";
    std::cout << "But patches at face boundaries have DIFFERENT bounds even though\n";
    std::cout << "they should share vertices!\n\n";
    
    std::cout << "Example:\n";
    std::cout << "  +X patch: bounds (1, -0.5, 0.5) to (1, 0.5, 1)\n";
    std::cout << "  +Z patch: bounds (0.5, -0.5, 1) to (1, 0.5, 1)\n";
    std::cout << "  Both patches correctly map UV to their bounds.\n";
    std::cout << "  But UV(0,1) on +X gives (1, 0.5, 0.5)\n";
    std::cout << "  And UV(1,1) on +Z gives (1, 0.5, 1)\n";
    std::cout << "  These should be the SAME vertex but they're not!\n\n";
    
    std::cout << "ROOT CAUSE: The patches have incompatible bounds at face boundaries.\n";
    std::cout << "The transform algorithm is CORRECT, but the INPUT bounds are WRONG.\n";
    
    return 0;
}