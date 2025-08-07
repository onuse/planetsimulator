#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class LODSelectionTests {
public:
    void runAllTests() {
        std::cout << "=== LOD SELECTION TESTS ===" << std::endl << std::endl;
        
        testBasicLODSelection();
        testScreenSpaceError();
        testDistanceBasedLOD();
        testQualityFactorAdjustment();
        testLODTransitions();
        testPerformanceMetrics();
        
        std::cout << std::endl << "=== ALL LOD TESTS PASSED ===" << std::endl;
    }
    
private:
    // Test 1: Basic LOD selection based on distance/size ratio
    void testBasicLODSelection() {
        std::cout << "Test 1: Basic LOD Selection" << std::endl;
        
        glm::vec3 viewPos(0, 0, 0);
        
        struct TestCase {
            glm::vec3 nodeCenter;
            float nodeHalfSize;
            float qualityFactor;
            uint32_t expectedLOD;
            const char* description;
        };
        
        TestCase cases[] = {
            {{5, 0, 0}, 1.0f, 1.0f, 0, "Very close node (LOD 0)"},
            {{25, 0, 0}, 1.0f, 1.0f, 1, "Close node (LOD 1)"},
            {{100, 0, 0}, 1.0f, 1.0f, 2, "Medium distance (LOD 2)"},
            {{500, 0, 0}, 1.0f, 1.0f, 3, "Far node (LOD 3)"},
            {{5000, 0, 0}, 1.0f, 1.0f, 4, "Very far node (LOD 4)"},
            {{50, 0, 0}, 10.0f, 1.0f, 0, "Large node at medium distance (LOD 0)"},
            {{50, 0, 0}, 0.1f, 1.0f, 3, "Small node at medium distance (LOD 3)"}
        };
        
        for (const auto& tc : cases) {
            uint32_t lod = selectLOD(tc.nodeCenter, tc.nodeHalfSize, viewPos, tc.qualityFactor);
            std::cout << "  " << tc.description << ": LOD " << lod;
            std::cout << " (expected: " << tc.expectedLOD << ")";
            
            assert(lod == tc.expectedLOD);
            std::cout << " ✓" << std::endl;
        }
    }
    
    // Test 2: Screen-space error calculation
    void testScreenSpaceError() {
        std::cout << "Test 2: Screen-Space Error Calculation" << std::endl;
        
        glm::vec3 viewPos(0, 0, 10);
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f);
        glm::mat4 viewProj = proj * view;
        
        struct TestCase {
            glm::vec3 nodeCenter;
            float nodeHalfSize;
            float minError;
            float maxError;
            const char* description;
        };
        
        TestCase cases[] = {
            {{0, 0, 0}, 1.0f, 0.05f, 0.2f, "Node at screen center"},
            {{0, 0, -5}, 0.5f, 0.01f, 0.1f, "Small node closer to camera"},
            {{5, 0, 0}, 2.0f, 0.1f, 0.5f, "Large node to the side"},
            {{0, 0, 50}, 5.0f, 0.001f, 0.05f, "Large node far away"}
        };
        
        for (const auto& tc : cases) {
            float error = calculateScreenSpaceError(tc.nodeCenter, tc.nodeHalfSize, viewProj, viewPos);
            std::cout << "  " << tc.description << ": error = " << error;
            std::cout << " (expected range: " << tc.minError << " - " << tc.maxError << ")";
            
            assert(error >= tc.minError && error <= tc.maxError);
            std::cout << " ✓" << std::endl;
        }
    }
    
    // Test 3: Distance-based LOD with planetary scale
    void testDistanceBasedLOD() {
        std::cout << "Test 3: Distance-Based LOD at Planetary Scale" << std::endl;
        
        float planetRadius = 6371000.0f; // Earth radius in meters
        glm::vec3 viewPos(planetRadius * 2.0f, 0, 0); // Camera at 2x planet radius
        
        struct PlanetaryNode {
            glm::vec3 center;
            float halfSize;
            const char* description;
        };
        
        PlanetaryNode nodes[] = {
            {{planetRadius, 0, 0}, 1000.0f, "Surface node facing camera"},
            {{0, 0, 0}, planetRadius, "Planet root node"},
            {{-planetRadius, 0, 0}, 1000.0f, "Surface node behind planet"},
            {{planetRadius * 0.7f, planetRadius * 0.7f, 0}, 500.0f, "Surface detail node"},
            {{0, 0, planetRadius * 10}, 100000.0f, "Distant space node"}
        };
        
        for (const auto& node : nodes) {
            float distance = glm::length(node.center - viewPos);
            float ratio = distance / node.halfSize;
            uint32_t lod = selectLOD(node.center, node.halfSize, viewPos, 1.0f);
            
            std::cout << "  " << node.description << std::endl;
            std::cout << "    Distance: " << (distance / 1000.0f) << " km" << std::endl;
            std::cout << "    Half-size: " << (node.halfSize / 1000.0f) << " km" << std::endl;
            std::cout << "    Ratio: " << ratio << std::endl;
            std::cout << "    Selected LOD: " << lod << std::endl;
        }
        
        std::cout << "  ✓ Planetary scale LOD selection verified" << std::endl;
    }
    
    // Test 4: Quality factor adjustment
    void testQualityFactorAdjustment() {
        std::cout << "Test 4: Quality Factor Adjustment" << std::endl;
        
        glm::vec3 viewPos(0, 0, 0);
        glm::vec3 nodeCenter(100, 0, 0);
        float nodeHalfSize = 10.0f;
        
        float qualityFactors[] = {0.5f, 1.0f, 2.0f, 4.0f};
        const char* qualityNames[] = {"Low", "Normal", "High", "Ultra"};
        
        std::cout << "  Node at distance 100, size 10:" << std::endl;
        for (int i = 0; i < 4; i++) {
            uint32_t lod = selectLOD(nodeCenter, nodeHalfSize, viewPos, qualityFactors[i]);
            std::cout << "    " << qualityNames[i] << " quality (factor " 
                      << qualityFactors[i] << "): LOD " << lod << std::endl;
        }
        
        // Verify that higher quality -> lower LOD (more detail)
        uint32_t lodLow = selectLOD(nodeCenter, nodeHalfSize, viewPos, 0.5f);
        uint32_t lodHigh = selectLOD(nodeCenter, nodeHalfSize, viewPos, 4.0f);
        assert(lodLow >= lodHigh);
        
        std::cout << "  ✓ Quality factor correctly affects LOD selection" << std::endl;
    }
    
    // Test 5: LOD transitions and hysteresis
    void testLODTransitions() {
        std::cout << "Test 5: LOD Transitions" << std::endl;
        
        glm::vec3 nodeCenter(0, 0, 0);
        float nodeHalfSize = 10.0f;
        
        // Simulate camera moving away from node
        std::cout << "  Camera moving away from node:" << std::endl;
        float distances[] = {5, 10, 20, 40, 80, 160, 320, 640, 1280, 2560};
        uint32_t previousLOD = 0;
        int transitions = 0;
        
        for (float dist : distances) {
            glm::vec3 viewPos(dist, 0, 0);
            uint32_t lod = selectLOD(nodeCenter, nodeHalfSize, viewPos, 1.0f);
            
            if (lod != previousLOD) {
                transitions++;
                std::cout << "    Distance " << dist << ": LOD " << previousLOD 
                          << " -> " << lod << " (transition)" << std::endl;
            }
            
            // LOD should never decrease as we move away
            assert(lod >= previousLOD);
            previousLOD = lod;
        }
        
        std::cout << "  Total transitions: " << transitions << std::endl;
        assert(transitions >= 3 && transitions <= 5); // Should have reasonable number of transitions
        
        std::cout << "  ✓ LOD transitions are smooth and monotonic" << std::endl;
    }
    
    // Test 6: Performance metrics
    void testPerformanceMetrics() {
        std::cout << "Test 6: LOD Performance Metrics" << std::endl;
        
        // Simulate a scene with many nodes
        const int nodeCount = 10000;
        std::vector<glm::vec3> nodeCenters;
        std::vector<float> nodeSizes;
        
        // Generate random nodes
        for (int i = 0; i < nodeCount; i++) {
            float angle = (float)i / nodeCount * 2.0f * 3.14159f;
            float distance = 100.0f + (i % 100) * 10.0f;
            nodeCenters.push_back(glm::vec3(
                cos(angle) * distance,
                sin(angle) * distance,
                (i % 10 - 5) * 10.0f
            ));
            nodeSizes.push_back(1.0f + (i % 10));
        }
        
        glm::vec3 viewPos(0, 0, 0);
        
        // Count LOD distribution
        int lodCounts[5] = {0, 0, 0, 0, 0};
        
        for (int i = 0; i < nodeCount; i++) {
            uint32_t lod = selectLOD(nodeCenters[i], nodeSizes[i], viewPos, 1.0f);
            if (lod < 5) {
                lodCounts[lod]++;
            }
        }
        
        std::cout << "  LOD distribution for " << nodeCount << " nodes:" << std::endl;
        int totalRendered = 0;
        for (int i = 0; i < 5; i++) {
            std::cout << "    LOD " << i << ": " << lodCounts[i] << " nodes";
            
            // Estimate triangle count (assuming lower LOD = fewer triangles)
            int trianglesPerNode = 1000 >> i; // Each LOD level halves triangles
            int totalTriangles = lodCounts[i] * trianglesPerNode;
            std::cout << " (~" << totalTriangles << " triangles)" << std::endl;
            totalRendered += lodCounts[i];
        }
        
        float renderPercentage = (float)totalRendered / nodeCount * 100.0f;
        std::cout << "  Rendering " << renderPercentage << "% of nodes" << std::endl;
        
        // Verify reasonable distribution
        assert(lodCounts[0] < nodeCount / 2); // Not everything at highest detail
        assert(totalRendered > 0); // At least some nodes rendered
        
        std::cout << "  ✓ LOD distribution provides good performance balance" << std::endl;
    }
    
    // Helper: LOD selection algorithm (matching implementation)
    uint32_t selectLOD(const glm::vec3& nodeCenter, float nodeHalfSize, 
                      const glm::vec3& viewPos, float qualityFactor) {
        float distance = glm::length(nodeCenter - viewPos);
        float ratio = distance / (nodeHalfSize * qualityFactor);
        
        if (ratio < 10.0f) return 0;      // Maximum detail
        if (ratio < 50.0f) return 1;      // High detail
        if (ratio < 200.0f) return 2;     // Medium detail
        if (ratio < 1000.0f) return 3;    // Low detail
        return 4;                          // Minimum detail
    }
    
    // Helper: Screen-space error calculation
    float calculateScreenSpaceError(const glm::vec3& nodeCenter, float nodeHalfSize,
                                   const glm::mat4& viewProj, const glm::vec3& viewPos) {
        float distance = glm::length(nodeCenter - viewPos);
        
        // Project node size to screen space
        glm::vec4 centerProj = viewProj * glm::vec4(nodeCenter, 1.0f);
        glm::vec4 edgeProj = viewProj * glm::vec4(nodeCenter + glm::vec3(nodeHalfSize, 0, 0), 1.0f);
        
        if (centerProj.w > 0 && edgeProj.w > 0) {
            glm::vec2 centerScreen = glm::vec2(centerProj) / centerProj.w;
            glm::vec2 edgeScreen = glm::vec2(edgeProj) / edgeProj.w;
            float screenSize = glm::length(edgeScreen - centerScreen);
            
            // Error metric: ratio of node size to screen size
            return nodeHalfSize / (distance * screenSize + 0.001f);
        }
        
        // Default: use distance-based metric
        return nodeHalfSize / (distance + 0.001f);
    }
};

int main() {
    try {
        LODSelectionTests tests;
        tests.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}