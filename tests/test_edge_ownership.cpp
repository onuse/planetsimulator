#include <iostream>
#include <vector>
#include <set>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

// Test edge ownership strategy for preventing face overlap

struct EdgeKey {
    glm::ivec3 v1, v2;  // Quantized positions
    
    EdgeKey(glm::vec3 p1, glm::vec3 p2) {
        // Quantize to avoid float precision issues
        v1 = glm::ivec3(p1 * 1000.0f);
        v2 = glm::ivec3(p2 * 1000.0f);
        // Ensure consistent ordering
        if (v1.x > v2.x || (v1.x == v2.x && v1.y > v2.y) || 
            (v1.x == v2.x && v1.y == v2.y && v1.z > v2.z)) {
            std::swap(v1, v2);
        }
    }
    
    bool operator<(const EdgeKey& other) const {
        if (v1.x != other.v1.x) return v1.x < other.v1.x;
        if (v1.y != other.v1.y) return v1.y < other.v1.y;
        if (v1.z != other.v1.z) return v1.z < other.v1.z;
        if (v2.x != other.v2.x) return v2.x < other.v2.x;
        if (v2.y != other.v2.y) return v2.y < other.v2.y;
        return v2.z < other.v2.z;
    }
};

int main() {
    std::cout << "==========================================\n";
    std::cout << "    EDGE OWNERSHIP STRATEGY ANALYSIS\n";
    std::cout << "==========================================\n\n";
    
    std::cout << "OPTION 1: Edge Ownership\n";
    std::cout << "------------------------\n";
    std::cout << "Performance Impact:\n";
    std::cout << "  - CPU: Moderate - need to check edge ownership during generation\n";
    std::cout << "  - Memory: Same - same number of vertices total\n";
    std::cout << "  - GPU: Best - no z-fighting, no overdraw\n";
    std::cout << "  - Complexity: High - need edge detection and ownership rules\n\n";
    
    std::cout << "Implementation approach:\n";
    std::cout << "  1. Each edge is owned by the face with lowest ID that contains it\n";
    std::cout << "  2. Faces generate vertices up to but not including non-owned edges\n\n";
    
    // Define cube edges and which faces share them
    struct Edge {
        const char* name;
        glm::vec3 p1, p2;
        std::vector<int> faces;  // Faces that share this edge
    };
    
    Edge edges[] = {
        // X-parallel edges
        {"Bottom-front X", {-1,-1,-1}, {1,-1,-1}, {3,5}},
        {"Bottom-back X",  {-1,-1, 1}, {1,-1, 1}, {3,4}},
        {"Top-front X",    {-1, 1,-1}, {1, 1,-1}, {2,5}},
        {"Top-back X",     {-1, 1, 1}, {1, 1, 1}, {2,4}},
        
        // Y-parallel edges  
        {"Left-front Y",   {-1,-1,-1}, {-1, 1,-1}, {1,5}},
        {"Right-front Y",  { 1,-1,-1}, { 1, 1,-1}, {0,5}},
        {"Left-back Y",    {-1,-1, 1}, {-1, 1, 1}, {1,4}},
        {"Right-back Y",   { 1,-1, 1}, { 1, 1, 1}, {0,4}},
        
        // Z-parallel edges
        {"Bottom-left Z",  {-1,-1,-1}, {-1,-1, 1}, {1,3}},
        {"Bottom-right Z", { 1,-1,-1}, { 1,-1, 1}, {0,3}},
        {"Top-left Z",     {-1, 1,-1}, {-1, 1, 1}, {1,2}},
        {"Top-right Z",    { 1, 1,-1}, { 1, 1, 1}, {0,2}},
    };
    
    std::cout << "Edge ownership assignments:\n";
    for (const auto& edge : edges) {
        int owner = *std::min_element(edge.faces.begin(), edge.faces.end());
        std::cout << "  " << edge.name << ": Face " << owner << " owns (shared by faces";
        for (int f : edge.faces) std::cout << " " << f;
        std::cout << ")\n";
    }
    
    std::cout << "\nVertex generation rules per face:\n";
    const char* faceNames[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    
    for (int face = 0; face < 6; face++) {
        std::cout << "Face " << face << " (" << faceNames[face] << "):\n";
        
        // Count owned edges
        int ownedEdges = 0;
        int sharedEdges = 0;
        for (const auto& edge : edges) {
            bool onFace = std::find(edge.faces.begin(), edge.faces.end(), face) != edge.faces.end();
            if (onFace) {
                int owner = *std::min_element(edge.faces.begin(), edge.faces.end());
                if (owner == face) ownedEdges++;
                else sharedEdges++;
            }
        }
        
        std::cout << "  - Owns " << ownedEdges << " edges\n";
        std::cout << "  - Shares but doesn't own " << sharedEdges << " edges\n";
        
        // Determine extent adjustments
        if (face == 0) { // +X
            std::cout << "  - Generate Y: [-1, 1], Z: [-1, 1] (owns all +X edges)\n";
        } else if (face == 1) { // -X  
            std::cout << "  - Generate Y: (-1, 1), Z: (-1, 1) (exclude all edges)\n";
        } else if (face == 2) { // +Y
            std::cout << "  - Generate X: (-1, 1], Z: [-1, 1] (owns some edges)\n";
        } else if (face == 3) { // -Y
            std::cout << "  - Generate X: (-1, 1), Z: (-1, 1) (exclude most edges)\n";
        } else if (face == 4) { // +Z
            std::cout << "  - Generate X: (-1, 1], Y: (-1, 1] (owns some edges)\n";
        } else { // -Z
            std::cout << "  - Generate X: (-1, 1), Y: (-1, 1) (no owned edges)\n";
        }
    }
    
    std::cout << "\nPERFORMANCE COMPARISON:\n";
    std::cout << "=======================\n\n";
    
    std::cout << "Option 1 - Edge Ownership (current analysis):\n";
    std::cout << "  CPU Cost: O(n) edge checks per patch\n";
    std::cout << "  GPU Cost: Optimal - no overdraw\n";
    std::cout << "  Quality: Perfect - no z-fighting, no gaps\n";
    std::cout << "  Complexity: High\n\n";
    
    std::cout << "Option 2 - Larger Inset (0.995):\n";
    std::cout << "  CPU Cost: None - simple multiplication\n";
    std::cout << "  GPU Cost: Optimal - no overdraw\n";
    std::cout << "  Quality: Good - no z-fighting, tiny gaps (0.01 units)\n";
    std::cout << "  Complexity: Trivial\n\n";
    
    std::cout << "Option 3 - Depth Bias:\n";
    std::cout << "  CPU Cost: None\n";
    std::cout << "  GPU Cost: Suboptimal - all faces render, z-fighting suppressed\n";
    std::cout << "  Quality: Variable - can cause incorrect face ordering\n";
    std::cout << "  Complexity: Low\n\n";
    
    std::cout << "Option 4 - Separate Passes:\n";
    std::cout << "  CPU Cost: 6x draw calls\n";
    std::cout << "  GPU Cost: High - 6 passes, state changes\n";
    std::cout << "  Quality: Perfect - no z-fighting possible\n";
    std::cout << "  Complexity: Moderate\n\n";
    
    std::cout << "RECOMMENDATION:\n";
    std::cout << "===============\n";
    std::cout << "Start with Option 2 (Larger Inset):\n";
    std::cout << "  - Simplest to implement (change one constant)\n";
    std::cout << "  - Good performance\n";
    std::cout << "  - If gaps are visible, try 0.9995 or 0.999\n";
    std::cout << "  - Can always upgrade to Option 1 later if needed\n";
    
    return 0;
}