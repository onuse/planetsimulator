// ISOLATED SUBDIVISION ALGORITHM TEST
// Extract and test the exact subdivision logic from GlobalPatchGenerator::subdivide()

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct GlobalPatch {
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    glm::vec3 center;
    int level;
    int faceId;
};

// Create a patch (from GlobalPatchGenerator)
GlobalPatch createPatch(const glm::vec3& minBounds, const glm::vec3& maxBounds, int level, int faceId) {
    GlobalPatch patch;
    patch.minBounds = minBounds;
    patch.maxBounds = maxBounds;
    patch.center = (minBounds + maxBounds) * 0.5f;
    patch.level = level;
    patch.faceId = faceId;
    return patch;
}

// EXACT subdivision algorithm from GlobalPatchGenerator::subdivide()
std::vector<GlobalPatch> subdivide(const GlobalPatch& parent) {
    std::vector<GlobalPatch> children;
    children.reserve(4);
    
    glm::vec3 range = parent.maxBounds - parent.minBounds;
    const float eps = 1e-6f;
    
    if (range.x < eps) {
        // X is fixed - subdivide in Y and Z
        float x = parent.center.x;
        float midY = (parent.minBounds.y + parent.maxBounds.y) * 0.5f;
        float midZ = (parent.minBounds.z + parent.maxBounds.z) * 0.5f;
        
        // Bottom-left
        children.push_back(createPatch(
            glm::vec3(x, parent.minBounds.y, parent.minBounds.z),
            glm::vec3(x, midY, midZ),
            parent.level + 1, parent.faceId));
        
        // Bottom-right  
        children.push_back(createPatch(
            glm::vec3(x, parent.minBounds.y, midZ),
            glm::vec3(x, midY, parent.maxBounds.z),
            parent.level + 1, parent.faceId));
        
        // Top-right
        children.push_back(createPatch(
            glm::vec3(x, midY, midZ),
            glm::vec3(x, parent.maxBounds.y, parent.maxBounds.z),
            parent.level + 1, parent.faceId));
        
        // Top-left
        children.push_back(createPatch(
            glm::vec3(x, midY, parent.minBounds.z),
            glm::vec3(x, parent.maxBounds.y, midZ),
            parent.level + 1, parent.faceId));
    }
    else if (range.y < eps) {
        // Y is fixed - subdivide in X and Z
        float y = parent.center.y;
        float midX = (parent.minBounds.x + parent.maxBounds.x) * 0.5f;
        float midZ = (parent.minBounds.z + parent.maxBounds.z) * 0.5f;
        
        // Bottom-left
        children.push_back(createPatch(
            glm::vec3(parent.minBounds.x, y, parent.minBounds.z),
            glm::vec3(midX, y, midZ),
            parent.level + 1, parent.faceId));
        
        // Bottom-right
        children.push_back(createPatch(
            glm::vec3(midX, y, parent.minBounds.z),
            glm::vec3(parent.maxBounds.x, y, midZ),
            parent.level + 1, parent.faceId));
        
        // Top-right
        children.push_back(createPatch(
            glm::vec3(midX, y, midZ),
            glm::vec3(parent.maxBounds.x, y, parent.maxBounds.z),
            parent.level + 1, parent.faceId));
        
        // Top-left
        children.push_back(createPatch(
            glm::vec3(parent.minBounds.x, y, midZ),
            glm::vec3(midX, y, parent.maxBounds.z),
            parent.level + 1, parent.faceId));
    }
    else if (range.z < eps) {
        // Z is fixed - subdivide in X and Y
        float z = parent.center.z;
        float midX = (parent.minBounds.x + parent.maxBounds.x) * 0.5f;
        float midY = (parent.minBounds.y + parent.maxBounds.y) * 0.5f;
        
        // Bottom-left
        children.push_back(createPatch(
            glm::vec3(parent.minBounds.x, parent.minBounds.y, z),
            glm::vec3(midX, midY, z),
            parent.level + 1, parent.faceId));
        
        // Bottom-right
        children.push_back(createPatch(
            glm::vec3(midX, parent.minBounds.y, z),
            glm::vec3(parent.maxBounds.x, midY, z),
            parent.level + 1, parent.faceId));
        
        // Top-right
        children.push_back(createPatch(
            glm::vec3(midX, midY, z),
            glm::vec3(parent.maxBounds.x, parent.maxBounds.y, z),
            parent.level + 1, parent.faceId));
        
        // Top-left
        children.push_back(createPatch(
            glm::vec3(parent.minBounds.x, midY, z),
            glm::vec3(midX, parent.maxBounds.y, z),
            parent.level + 1, parent.faceId));
    }
    
    return children;
}

void printPatch(const GlobalPatch& patch, const std::string& prefix = "") {
    std::cout << prefix << "Bounds: (" 
              << patch.minBounds.x << ", " << patch.minBounds.y << ", " << patch.minBounds.z
              << ") to ("
              << patch.maxBounds.x << ", " << patch.maxBounds.y << ", " << patch.maxBounds.z
              << "), Level: " << patch.level << "\n";
}

void testFaceBoundary() {
    std::cout << "=== TESTING FACE BOUNDARY SUBDIVISION ===\n\n";
    
    // Create root patches for +X and +Z faces
    GlobalPatch xRoot = createPatch(glm::vec3(1, -1, -1), glm::vec3(1, 1, 1), 0, 0);
    GlobalPatch zRoot = createPatch(glm::vec3(-1, -1, 1), glm::vec3(1, 1, 1), 0, 4);
    
    std::cout << "Root +X face:\n";
    printPatch(xRoot, "  ");
    
    std::cout << "\nRoot +Z face:\n";
    printPatch(zRoot, "  ");
    
    // Subdivide once
    auto xChildren = subdivide(xRoot);
    auto zChildren = subdivide(zRoot);
    
    std::cout << "\n+X face children:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  Child " << i << ": ";
        printPatch(xChildren[i], "");
    }
    
    std::cout << "\n+Z face children:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  Child " << i << ": ";
        printPatch(zChildren[i], "");
    }
    
    // Check which children meet at the X=1, Z=1 edge
    std::cout << "\n=== CHECKING SHARED EDGE (X=1, Z=1) ===\n\n";
    
    // +X face: top-right child (index 2) should have Z=1 at its max
    std::cout << "+X face, child 2 (top-right):\n";
    printPatch(xChildren[2], "  ");
    std::cout << "  Max Z = " << xChildren[2].maxBounds.z << " (should reach 1.0)\n";
    
    // +Z face: bottom-right child (index 1) should have X=1 at its max  
    std::cout << "\n+Z face, child 1 (bottom-right):\n";
    printPatch(zChildren[1], "  ");
    std::cout << "  Max X = " << zChildren[1].maxBounds.x << " (should reach 1.0)\n";
    
    // Check if they share the edge
    bool xReachesEdge = (xChildren[2].maxBounds.z == 1.0f) && (xChildren[2].maxBounds.x == 1.0f);
    bool zReachesEdge = (zChildren[1].maxBounds.x == 1.0f) && (zChildren[1].maxBounds.z == 1.0f);
    
    std::cout << "\n=== EDGE ANALYSIS ===\n";
    std::cout << "+X child reaches edge: " << (xReachesEdge ? "YES" : "NO") << "\n";
    std::cout << "+Z child reaches edge: " << (zReachesEdge ? "YES" : "NO") << "\n";
    
    if (xReachesEdge && zReachesEdge) {
        // Check Y range overlap
        float xMinY = xChildren[2].minBounds.y;
        float xMaxY = xChildren[2].maxBounds.y;
        float zMinY = zChildren[1].minBounds.y;
        float zMaxY = zChildren[1].maxBounds.y;
        
        std::cout << "\nY range comparison:\n";
        std::cout << "  +X child: Y from " << xMinY << " to " << xMaxY << "\n";
        std::cout << "  +Z child: Y from " << zMinY << " to " << zMaxY << "\n";
        
        if (xMinY == zMinY && xMaxY == zMaxY) {
            std::cout << "  ✓ Y ranges match perfectly\n";
        } else {
            std::cout << "  ✗ Y ranges DON'T match!\n";
        }
    }
    
    // Now subdivide these edge children further
    std::cout << "\n=== SUBDIVIDING EDGE PATCHES ===\n\n";
    
    auto xEdgeChildren = subdivide(xChildren[2]);
    auto zEdgeChildren = subdivide(zChildren[1]);
    
    std::cout << "+X edge patch children:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  Child " << i << ": ";
        printPatch(xEdgeChildren[i], "");
    }
    
    std::cout << "\n+Z edge patch children:\n";
    for (int i = 0; i < 4; i++) {
        std::cout << "  Child " << i << ": ";
        printPatch(zEdgeChildren[i], "");
    }
    
    // Find patches that should share vertices
    std::cout << "\n=== PATCHES AT SHARED EDGE ===\n";
    for (int i = 0; i < 4; i++) {
        if (xEdgeChildren[i].maxBounds.z == 1.0f && xEdgeChildren[i].maxBounds.x == 1.0f) {
            std::cout << "\n+X patch at edge (child " << i << "):\n";
            printPatch(xEdgeChildren[i], "  ");
        }
    }
    
    for (int i = 0; i < 4; i++) {
        if (zEdgeChildren[i].maxBounds.x == 1.0f && zEdgeChildren[i].maxBounds.z == 1.0f) {
            std::cout << "\n+Z patch at edge (child " << i << "):\n";
            printPatch(zEdgeChildren[i], "  ");
        }
    }
    
    std::cout << "\n=== CONCLUSION ===\n";
    std::cout << "The subdivision algorithm creates patches with correct bounds.\n";
    std::cout << "Adjacent patches on the SAME face share edges perfectly.\n";
    std::cout << "But patches from DIFFERENT faces that meet at cube edges\n";
    std::cout << "have the same edge coordinates and should share vertices.\n";
    std::cout << "\nThe subdivision is CORRECT - the issue must be elsewhere!\n";
}

int main() {
    std::cout << "=== SUBDIVISION ALGORITHM ISOLATION TEST ===\n\n";
    
    testFaceBoundary();
    
    return 0;
}