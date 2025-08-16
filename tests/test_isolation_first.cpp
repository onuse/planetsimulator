// STEP 1: Create minimal isolation test BEFORE touching any production code
// This test will help us understand EXACTLY what's happening

#include <iostream>
#include <glm/glm.hpp>
#include <vector>
#include <fstream>

// Question: Why do we still see rendering artifacts?
// Hypothesis 1: Face boundaries have gaps
// Hypothesis 2: Transform matrices are wrong
// Hypothesis 3: Vertex generation has issues
// Hypothesis 4: Something else entirely

// Let's test ONLY the most basic case: 
// Generate 2 adjacent patches and see if their edges match EXACTLY

int main() {
    std::cout << "=== ISOLATION TEST: What's Actually Wrong? ===\n\n";
    
    // SUPER SIMPLE TEST: Two squares that should share an edge
    // No transforms, no sphere mapping, no terrain, NOTHING else
    
    struct SimpleVertex {
        glm::vec3 pos;
    };
    
    // Patch 1: Square from (0,0,0) to (1,1,0)
    std::vector<SimpleVertex> patch1;
    for (int y = 0; y <= 2; y++) {
        for (int x = 0; x <= 2; x++) {
            patch1.push_back({glm::vec3(x * 0.5f, y * 0.5f, 0)});
        }
    }
    
    // Patch 2: Square from (1,0,0) to (2,1,0) - shares edge at x=1
    std::vector<SimpleVertex> patch2;
    for (int y = 0; y <= 2; y++) {
        for (int x = 0; x <= 2; x++) {
            patch2.push_back({glm::vec3(1.0f + x * 0.5f, y * 0.5f, 0)});
        }
    }
    
    // Check: Do the shared edge vertices match EXACTLY?
    std::cout << "Patch 1 right edge vertices:\n";
    for (int y = 0; y <= 2; y++) {
        int idx = y * 3 + 2; // Right edge
        std::cout << "  [" << idx << "] = (" << patch1[idx].pos.x 
                  << ", " << patch1[idx].pos.y << ", " << patch1[idx].pos.z << ")\n";
    }
    
    std::cout << "\nPatch 2 left edge vertices:\n";
    for (int y = 0; y <= 2; y++) {
        int idx = y * 3 + 0; // Left edge
        std::cout << "  [" << idx << "] = (" << patch2[idx].pos.x 
                  << ", " << patch2[idx].pos.y << ", " << patch2[idx].pos.z << ")\n";
    }
    
    // Compare
    std::cout << "\nComparison:\n";
    bool perfectMatch = true;
    for (int y = 0; y <= 2; y++) {
        int idx1 = y * 3 + 2;
        int idx2 = y * 3 + 0;
        glm::vec3 diff = patch1[idx1].pos - patch2[idx2].pos;
        float distance = glm::length(diff);
        
        std::cout << "  Y=" << y << ": distance = " << distance;
        if (distance < 0.0001f) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ MISMATCH!\n";
            perfectMatch = false;
        }
    }
    
    if (perfectMatch) {
        std::cout << "\n✓ BASIC TEST PASSES: Simple patches align perfectly\n";
        std::cout << "CONCLUSION: The problem is NOT in basic vertex positioning\n";
        std::cout << "NEXT STEP: Add complexity one step at a time\n";
    } else {
        std::cout << "\n✗ BASIC TEST FAILS: Even simple patches don't align!\n";
        std::cout << "CONCLUSION: Problem is in the most basic vertex generation\n";
        std::cout << "NEXT STEP: Fix this before doing anything else\n";
    }
    
    // Export for visual inspection
    std::ofstream file("isolation_test.obj");
    
    // Write all vertices
    for (const auto& v : patch1) {
        file << "v " << v.pos.x << " " << v.pos.y << " " << v.pos.z << "\n";
    }
    for (const auto& v : patch2) {
        file << "v " << v.pos.x << " " << v.pos.y << " " << v.pos.z << "\n";
    }
    
    // Write faces for patch 1
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            int base = y * 3 + x + 1; // OBJ uses 1-based indexing
            file << "f " << base << " " << (base + 3) << " " << (base + 1) << "\n";
            file << "f " << (base + 1) << " " << (base + 3) << " " << (base + 4) << "\n";
        }
    }
    
    // Write faces for patch 2
    int offset = 9; // 9 vertices in patch 1
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            int base = offset + y * 3 + x + 1;
            file << "f " << base << " " << (base + 3) << " " << (base + 1) << "\n";
            file << "f " << (base + 1) << " " << (base + 3) << " " << (base + 4) << "\n";
        }
    }
    
    file.close();
    std::cout << "\nWrote isolation_test.obj for visual inspection\n";
    
    return 0;
}