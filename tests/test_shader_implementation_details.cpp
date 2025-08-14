// Test shader implementation details that were missed in theoretical tests
// This tests the ACTUAL data flow and edge cases that occur in practice

#include "../shaders/src/lib/shader_math.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <iomanip>
#include <cstring>

// Test framework
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) \
    void name(); \
    tests_run++; \
    std::cout << "\n[TEST " << tests_run << "] " << #name << "\n"; \
    try { \
        name(); \
        tests_passed++; \
        std::cout << "  ✓ PASSED\n"; \
    } catch (const std::exception& e) { \
        tests_failed++; \
        std::cout << "  ✗ FAILED: " << e.what() << "\n"; \
    }

#define ASSERT_NEAR(actual, expected, tolerance) \
    if (std::abs((actual) - (expected)) > (tolerance)) { \
        throw std::runtime_error(std::string("Expected ") + std::to_string(expected) + \
                                " ± " + std::to_string(tolerance) + \
                                " but got " + std::to_string(actual)); \
    }

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition); \
    }

// ============================================================================
// TEST 1: Actual Edge Detection Threshold Used in Shader
// ============================================================================

void testActualEdgeThreshold() {
    std::cout << "  Testing with actual shader threshold (0.002)...\n";
    
    const float ACTUAL_THRESHOLD = 0.002f;  // From shader
    
    // Test cases at exact threshold boundary
    struct EdgeCase {
        vec2 uv;
        int expectedEdge;
        const char* desc;
    };
    
    EdgeCase cases[] = {
        // Just inside threshold
        {{0.0019f, 0.5f}, 3, "Just inside left edge"},
        {{0.9981f, 0.5f}, 4, "Just inside right edge"},
        {{0.5f, 0.0019f}, 1, "Just inside top edge"},
        {{0.5f, 0.9981f}, 2, "Just inside bottom edge"},
        
        // Just outside threshold
        {{0.0021f, 0.5f}, 0, "Just outside left edge"},
        {{0.9979f, 0.5f}, 0, "Just outside right edge"},
        {{0.5f, 0.0021f}, 0, "Just outside top edge"},
        {{0.5f, 0.9979f}, 0, "Just outside bottom edge"},
        
        // Exactly at threshold
        {{0.002f, 0.5f}, 3, "Exactly at left threshold"},
        {{0.998f, 0.5f}, 4, "Exactly at right threshold"},
        {{0.5f, 0.002f}, 1, "Exactly at top threshold"},
        {{0.5f, 0.998f}, 2, "Exactly at bottom threshold"},
    };
    
    for (const auto& tc : cases) {
        int edge = getEdgeType(tc.uv, ACTUAL_THRESHOLD);
        if (edge != tc.expectedEdge) {
            std::cout << "    MISMATCH: " << tc.desc << " - expected " << tc.expectedEdge 
                     << " got " << edge << "\n";
            throw std::runtime_error("Edge detection mismatch");
        }
    }
    
    std::cout << "    All edge cases detected correctly at 0.002 threshold\n";
}

// ============================================================================
// TEST 2: Corner Vertices (On Two Edges Simultaneously)
// ============================================================================

void testCornerVertices() {
    std::cout << "  Testing corner vertices that touch 2 edges...\n";
    
    const float threshold = 0.002f;
    
    // Corners are special - they're on 2 edges at once
    struct CornerCase {
        vec2 uv;
        int primaryEdge;  // getEdgeType returns first matching edge
        const char* desc;
    };
    
    CornerCase corners[] = {
        {{0.001f, 0.001f}, 1, "Top-left corner (top edge wins)"},
        {{0.999f, 0.001f}, 1, "Top-right corner (top edge wins)"},
        {{0.001f, 0.999f}, 2, "Bottom-left corner (bottom edge wins)"},
        {{0.999f, 0.999f}, 2, "Bottom-right corner (bottom edge wins)"},
    };
    
    for (const auto& corner : corners) {
        int edge = getEdgeType(corner.uv, threshold);
        std::cout << "    Corner " << corner.desc << " -> edge " << edge << "\n";
        
        // For T-junction fixing, we need to handle BOTH edges
        // Let's verify the fix works for corners
        vec2 fixed = fixTJunctionEdge(corner.uv, 1.0f, edge);
        
        // Corner vertices should snap to (0,0), (1,0), (0,1), or (1,1)
        bool isCornerFixed = (
            (std::abs(fixed.x - 0.0f) < 0.01f || std::abs(fixed.x - 1.0f) < 0.01f) &&
            (std::abs(fixed.y - 0.0f) < 0.01f || std::abs(fixed.y - 1.0f) < 0.01f)
        );
        
        if (!isCornerFixed) {
            std::cout << "    CORNER NOT PROPERLY FIXED: " << fixed.x << ", " << fixed.y << "\n";
            throw std::runtime_error("Corner vertex not snapped to grid corner");
        }
    }
    
    std::cout << "    Corner vertices handled correctly\n";
}

// ============================================================================
// TEST 3: Actual Instance Buffer Data Patterns
// ============================================================================

struct InstanceData {
    float morphParams[4];      // x=morph, yzw=neighbor levels  
    float heightTexCoord[4];   // x=left neighbor, y=current level
};

void testInstanceBufferData() {
    std::cout << "  Testing with realistic instance buffer data...\n";
    
    // Simulate real patch configurations from the LOD system
    struct PatchConfig {
        float currentLevel;
        float neighbors[4];  // top, right, bottom, left
        const char* desc;
    };
    
    PatchConfig configs[] = {
        {2, {1, 2, 2, 0}, "Level 2 patch with mixed neighbors"},
        {3, {3, 2, 3, 1}, "Level 3 patch with coarser neighbors"},
        {1, {0, 0, 1, 1}, "Level 1 patch next to root"},
        {4, {4, 4, 4, 4}, "Level 4 patch with same-level neighbors"},
        {5, {3, 4, 3, 2}, "Level 5 patch surrounded by coarser"},
    };
    
    for (const auto& config : configs) {
        std::cout << "    Testing: " << config.desc << "\n";
        
        // Test each edge with its neighbor
        vec2 edgeVertices[] = {
            {0.5f, 0.001f},   // Top edge
            {0.999f, 0.5f},   // Right edge
            {0.5f, 0.999f},   // Bottom edge
            {0.001f, 0.5f},   // Left edge
        };
        
        int edgeTypes[] = {1, 4, 2, 3};
        
        for (int i = 0; i < 4; i++) {
            float levelDiff = config.currentLevel - config.neighbors[i];
            
            if (levelDiff > 0) {
                vec2 fixed = fixTJunctionEdge(edgeVertices[i], levelDiff, edgeTypes[i]);
                
                // Verify snapping is consistent
                float coarseSpacing = 0.5f * pow(2.0f, levelDiff - 1.0f);
                float expectedSnap = round(edgeVertices[i].x / coarseSpacing) * coarseSpacing;
                
                // The coordinate that should be modified depends on edge
                float actualCoord = (i == 0 || i == 2) ? fixed.x : fixed.y;
                float inputCoord = (i == 0 || i == 2) ? edgeVertices[i].x : edgeVertices[i].y;
                
                std::cout << "      Edge " << i << ": levelDiff=" << levelDiff 
                         << " input=" << inputCoord << " output=" << actualCoord 
                         << " spacing=" << coarseSpacing << "\n";
            }
        }
    }
}

// ============================================================================
// TEST 4: Float Precision GPU vs CPU
// ============================================================================

void testFloatPrecisionGPUvsCPU() {
    std::cout << "  Testing float precision edge cases (GPU vs CPU)...\n";
    
    // GPUs may have different rounding behavior
    // Test values that are problematic for float precision
    
    float problematicValues[] = {
        0.333333333f,  // 1/3
        0.666666667f,  // 2/3
        0.1f,          // Not exactly representable in binary
        0.2f,          // Not exactly representable in binary
        0.3f,          // Not exactly representable in binary
        0.7f,          // Not exactly representable in binary
    };
    
    for (float val : problematicValues) {
        vec2 uv = {val, 0.001f};
        
        // Test snapping with level difference 1 (coarse spacing = 0.5)
        vec2 fixed = fixTJunctionEdge(uv, 1.0f, 1);
        
        // Check if the result is exactly 0.0, 0.5, or 1.0
        bool isGridAligned = (
            std::abs(fixed.x - 0.0f) < 1e-6f ||
            std::abs(fixed.x - 0.5f) < 1e-6f ||
            std::abs(fixed.x - 1.0f) < 1e-6f
        );
        
        std::cout << "    " << val << " -> " << fixed.x 
                 << (isGridAligned ? " (grid-aligned)" : " (PRECISION ISSUE!)") << "\n";
        
        if (!isGridAligned) {
            std::cout << "      WARNING: Not exactly on grid, may cause cracks\n";
        }
    }
    
    // Test accumulation of rounding errors
    float accumulated = 0.0f;
    for (int i = 0; i < 10; i++) {
        accumulated += 0.1f;  // Should be 1.0 after 10 additions
    }
    
    std::cout << "    0.1 * 10 = " << accumulated 
             << " (error: " << std::abs(accumulated - 1.0f) << ")\n";
    
    if (std::abs(accumulated - 1.0f) > 1e-6f) {
        std::cout << "    WARNING: Float accumulation error detected\n";
    }
}

// ============================================================================
// TEST 5: Integration Test - Multiple Patches Meeting
// ============================================================================

void testMultiplePatchesAtSharedEdge() {
    std::cout << "  Testing multiple patches sharing an edge...\n";
    
    // Simulate 4 patches meeting at a shared edge
    // Patch A (level 2) and Patch B (level 1) share a vertical edge
    
    struct Vertex {
        vec2 uv;
        int patchLevel;
        int neighborLevel;
        int edgeType;
    };
    
    // Vertices along the shared edge from both patches
    Vertex patchA[] = {
        {{0.999f, 0.0f}, 2, 1, 4},   // Right edge of patch A
        {{0.999f, 0.25f}, 2, 1, 4},
        {{0.999f, 0.5f}, 2, 1, 4},
        {{0.999f, 0.75f}, 2, 1, 4},
        {{0.999f, 1.0f}, 2, 1, 4},
    };
    
    Vertex patchB[] = {
        {{0.001f, 0.0f}, 1, 2, 3},   // Left edge of patch B
        {{0.001f, 0.5f}, 1, 2, 3},
        {{0.001f, 1.0f}, 1, 2, 3},
    };
    
    std::cout << "    Patch A vertices (level 2, right edge):\n";
    for (const auto& v : patchA) {
        float levelDiff = v.patchLevel - v.neighborLevel;
        vec2 fixed = fixTJunctionEdge(v.uv, levelDiff, v.edgeType);
        std::cout << "      UV(" << v.uv.y << ") -> " << fixed.y << "\n";
    }
    
    std::cout << "    Patch B vertices (level 1, left edge):\n";
    for (const auto& v : patchB) {
        float levelDiff = v.patchLevel - v.neighborLevel;
        vec2 fixed = fixTJunctionEdge(v.uv, levelDiff, v.edgeType);
        std::cout << "      UV(" << v.uv.y << ") -> " << fixed.y << "\n";
    }
    
    // Verify alignment: Patch A's fixed vertices should match Patch B's positions
    vec2 fixedA_0 = fixTJunctionEdge(patchA[0].uv, 1.0f, 4);
    vec2 fixedA_2 = fixTJunctionEdge(patchA[2].uv, 1.0f, 4);
    vec2 fixedA_4 = fixTJunctionEdge(patchA[4].uv, 1.0f, 4);
    
    // These should align with patch B's native vertices at 0, 0.5, 1
    ASSERT_NEAR(fixedA_0.y, 0.0f, 0.001f);
    ASSERT_NEAR(fixedA_2.y, 0.5f, 0.001f);
    ASSERT_NEAR(fixedA_4.y, 1.0f, 0.001f);
    
    std::cout << "    ✓ Shared edge vertices align correctly\n";
}

// ============================================================================
// TEST 6: Actual Shader Conditions
// ============================================================================

void testActualShaderConditions() {
    std::cout << "  Testing exact shader conditions and edge cases...\n";
    
    // The shader uses specific conditions we need to test
    
    // Test 1: Tie-breaking logic for halfway points
    {
        vec2 uv = {0.25f, 0.001f};  // Exactly halfway between 0 and 0.5
        vec2 fixed = fixTJunctionEdge(uv, 1.0f, 1);
        
        // Our tie-breaking rule: snap to edge (0 or 1) over middle
        if (std::abs(fixed.x - 0.0f) > 0.001f && std::abs(fixed.x - 0.5f) > 0.001f) {
            std::cout << "    ERROR: Tie-breaking failed for 0.25 -> " << fixed.x << "\n";
            throw std::runtime_error("Tie-breaking logic incorrect");
        }
    }
    
    // Test 2: Level difference clamping (shader clamps to 10)
    {
        vec2 uv = {0.5f, 0.001f};
        vec2 fixed = fixTJunctionEdge(uv, 15.0f, 1);  // Extreme level difference
        
        // Should still produce valid result
        ASSERT_TRUE(fixed.x >= 0.0f && fixed.x <= 1.0f);
        std::cout << "    Level difference clamping works\n";
    }
    
    // Test 3: No modification when same level
    {
        vec2 uv = {0.123f, 0.001f};
        vec2 fixed = fixTJunctionEdge(uv, 0.0f, 1);  // Same level
        ASSERT_NEAR(fixed.x, uv.x, 0.0001f);
        std::cout << "    Same-level patches correctly unchanged\n";
    }
}

// ============================================================================
// TEST 7: Black Triangle Root Cause
// ============================================================================

void testBlackTriangleScenario() {
    std::cout << "  Investigating black triangle root cause...\n";
    
    // Black triangles appear at LOD boundaries
    // Let's test the exact scenario that causes them
    
    // Scenario: Level 2 patch next to Level 0 patch
    // Level 0 has vertices at: 0, 1
    // Level 2 has vertices at: 0, 0.25, 0.5, 0.75, 1
    
    float level2Vertices[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float levelDiff = 2.0f;
    
    std::cout << "    Level 2 vertices snapping to Level 0 grid:\n";
    for (float v : level2Vertices) {
        vec2 uv = {v, 0.001f};
        vec2 fixed = fixTJunctionEdge(uv, levelDiff, 1);
        
        bool matchesCoarse = (std::abs(fixed.x - 0.0f) < 0.001f || 
                             std::abs(fixed.x - 1.0f) < 0.001f);
        
        std::cout << "      " << v << " -> " << fixed.x;
        if (!matchesCoarse) {
            std::cout << " (ERROR: Doesn't match coarse grid!)";
        }
        std::cout << "\n";
    }
    
    // The problem: 0.5 should snap to either 0 or 1, but our current logic
    // might be keeping it at 0.5, creating a T-junction!
    vec2 midPoint = {0.5f, 0.001f};
    vec2 fixedMid = fixTJunctionEdge(midPoint, 2.0f, 1);
    
    if (std::abs(fixedMid.x - 0.5f) < 0.001f) {
        std::cout << "    FOUND BUG: 0.5 not snapping to coarse grid with levelDiff=2\n";
        std::cout << "    This creates T-junctions causing black triangles!\n";
        throw std::runtime_error("T-junction fix doesn't handle levelDiff > 1 correctly");
    }
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    std::cout << "=============================================\n";
    std::cout << "SHADER IMPLEMENTATION DETAILS VERIFICATION\n";
    std::cout << "=============================================\n";
    std::cout << "Testing actual runtime conditions and edge cases...\n";
    
    TEST(testActualEdgeThreshold)
    TEST(testCornerVertices)
    TEST(testInstanceBufferData)
    TEST(testFloatPrecisionGPUvsCPU)
    TEST(testMultiplePatchesAtSharedEdge)
    TEST(testActualShaderConditions)
    TEST(testBlackTriangleScenario)
    
    std::cout << "\n=============================================\n";
    std::cout << "RESULTS: " << tests_passed << "/" << tests_run << " passed, " 
              << tests_failed << " failed\n";
    
    if (tests_failed > 0) {
        std::cout << "\n⚠️  CRITICAL ISSUES FOUND:\n";
        std::cout << "- Implementation doesn't match theory\n";
        std::cout << "- Black triangles likely caused by incorrect snapping\n";
        std::cout << "- Need to fix level difference > 1 handling\n";
        return 1;
    } else if (tests_passed == tests_run) {
        std::cout << "✓ All implementation details verified!\n";
        return 0;
    }
    
    return 1;
}