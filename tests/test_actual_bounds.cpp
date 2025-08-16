#include <iostream>
#include <glm/glm.hpp>
#include <vector>

// Test what bounds are actually being generated

struct GlobalPatch {
    glm::vec3 center;
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    int level;
    int faceId;
};

GlobalPatch createPatch(const glm::vec3& min, const glm::vec3& max, int level, int faceId) {
    GlobalPatch p;
    p.minBounds = min;
    p.maxBounds = max;
    p.center = (min + max) * 0.5f;
    p.level = level;
    p.faceId = faceId;
    return p;
}

std::vector<GlobalPatch> subdivide(const GlobalPatch& parent) {
    std::vector<GlobalPatch> children;
    
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
            
        // And so on for other children...
    }
    
    return children;
}

int main() {
    std::cout << "=== ACTUAL PATCH BOUNDS TEST ===\n\n";
    
    const double INSET = 0.9995;
    
    // Create Face 0 root patch (+X face)
    GlobalPatch root = createPatch(
        glm::vec3(1, -INSET, -INSET), 
        glm::vec3(1, INSET, INSET), 
        0, 0);
    
    std::cout << "Face 0 ROOT patch:\n";
    std::cout << "  Bounds: (" << root.minBounds.x << ", " << root.minBounds.y << ", " << root.minBounds.z 
              << ") to (" << root.maxBounds.x << ", " << root.maxBounds.y << ", " << root.maxBounds.z << ")\n";
    std::cout << "  Y range: " << root.minBounds.y << " to " << root.maxBounds.y << "\n";
    std::cout << "  Z range: " << root.minBounds.z << " to " << root.maxBounds.z << "\n\n";
    
    // Subdivide to level 1
    auto level1 = subdivide(root);
    if (!level1.empty()) {
        std::cout << "Level 1 corner patch (should be bottom-left):\n";
        auto& corner = level1[0];
        std::cout << "  Bounds: (" << corner.minBounds.x << ", " << corner.minBounds.y << ", " << corner.minBounds.z 
                  << ") to (" << corner.maxBounds.x << ", " << corner.maxBounds.y << ", " << corner.maxBounds.z << ")\n";
        std::cout << "  Y range: " << corner.minBounds.y << " to " << corner.maxBounds.y << "\n";
        std::cout << "  Z range: " << corner.minBounds.z << " to " << corner.maxBounds.z << "\n\n";
        
        // Subdivide to level 2
        auto level2 = subdivide(corner);
        if (!level2.empty()) {
            std::cout << "Level 2 corner patch:\n";
            auto& corner2 = level2[0];
            std::cout << "  Bounds: (" << corner2.minBounds.x << ", " << corner2.minBounds.y << ", " << corner2.minBounds.z 
                      << ") to (" << corner2.maxBounds.x << ", " << corner2.maxBounds.y << ", " << corner2.maxBounds.z << ")\n";
            std::cout << "  Y range: " << corner2.minBounds.y << " to " << corner2.maxBounds.y << "\n";
            std::cout << "  Z range: " << corner2.minBounds.z << " to " << corner2.maxBounds.z << "\n\n";
        }
    }
    
    std::cout << "ANALYSIS:\n";
    std::cout << "=========\n";
    std::cout << "If the debug output shows 'UV(0,0) -> Cube(1, -1, -1)' for a Level 2 patch,\n";
    std::cout << "but our root started at -0.9995, then either:\n";
    std::cout << "1. The patch being debugged is NOT from the subdivided root (different patch)\n";
    std::cout << "2. The transform is computing bounds incorrectly\n";
    std::cout << "3. The debug output is from a different face or patch than expected\n\n";
    
    std::cout << "The debug shows Patch Center: (1, -0.75, -0.75), Level: 2\n";
    std::cout << "This would come from bounds (1, -1, -1) to (1, -0.5, -0.5)\n";
    std::cout << "But with INSET, it should be (1, -0.9995, -0.9995) to (1, -0.49975, -0.49975)\n\n";
    
    std::cout << "CONCLUSION: The patches are NOT respecting the INSET!\n";
    
    return 0;
}