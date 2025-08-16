#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <glm/glm.hpp>
#include <iomanip>
#include <sstream>
#include <cmath>

// Test to check if patches are being duplicated in the instance buffer

struct PatchInstance {
    glm::dvec3 minBounds;
    glm::dvec3 maxBounds;
    int level;
    int faceId;
    
    std::string getKey() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << minBounds.x << "," << minBounds.y << "," << minBounds.z << "|";
        ss << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << "|";
        ss << level << "|" << faceId;
        return ss.str();
    }
};

void simulateLODCollection() {
    std::cout << "\n=== SIMULATING LOD PATCH COLLECTION ===\n";
    
    std::vector<PatchInstance> collectedPatches;
    std::map<std::string, int> patchCounts;
    
    // Simulate collecting patches from 6 faces
    for (int face = 0; face < 6; face++) {
        // Root patch for this face
        PatchInstance root;
        root.level = 0;
        root.faceId = face;
        
        switch(face) {
            case 0: // +X
                root.minBounds = glm::dvec3(1, -1, -1);
                root.maxBounds = glm::dvec3(1, 1, 1);
                break;
            case 1: // -X
                root.minBounds = glm::dvec3(-1, -1, -1);
                root.maxBounds = glm::dvec3(-1, 1, 1);
                break;
            case 2: // +Y
                root.minBounds = glm::dvec3(-1, 1, -1);
                root.maxBounds = glm::dvec3(1, 1, 1);
                break;
            case 3: // -Y
                root.minBounds = glm::dvec3(-1, -1, -1);
                root.maxBounds = glm::dvec3(1, -1, 1);
                break;
            case 4: // +Z
                root.minBounds = glm::dvec3(-1, -1, 1);
                root.maxBounds = glm::dvec3(1, 1, 1);
                break;
            case 5: // -Z
                root.minBounds = glm::dvec3(-1, -1, -1);
                root.maxBounds = glm::dvec3(1, 1, -1);
                break;
        }
        
        collectedPatches.push_back(root);
        patchCounts[root.getKey()]++;
        
        // Simulate subdividing some faces (level 1)
        if (face == 0 || face == 2) { // Subdivide +X and +Y for testing
            for (int i = 0; i < 4; i++) {
                PatchInstance child;
                child.level = 1;
                child.faceId = face;
                
                // Create 4 subdivisions (simplified)
                double halfSize = 1.0;
                if (face == 0) { // +X
                    child.minBounds = glm::dvec3(1, -1 + (i/2)*halfSize, -1 + (i%2)*halfSize);
                    child.maxBounds = glm::dvec3(1, -1 + (i/2+1)*halfSize, -1 + (i%2+1)*halfSize);
                } else { // +Y
                    child.minBounds = glm::dvec3(-1 + (i/2)*halfSize, 1, -1 + (i%2)*halfSize);
                    child.maxBounds = glm::dvec3(-1 + (i/2+1)*halfSize, 1, -1 + (i%2+1)*halfSize);
                }
                
                collectedPatches.push_back(child);
                patchCounts[child.getKey()]++;
            }
        }
    }
    
    // Check for duplicates
    std::cout << "Total patches collected: " << collectedPatches.size() << "\n";
    std::cout << "Unique patches: " << patchCounts.size() << "\n";
    
    int duplicates = 0;
    for (const auto& [key, count] : patchCounts) {
        if (count > 1) {
            std::cout << "  DUPLICATE: Patch appears " << count << " times\n";
            std::cout << "    Key: " << key << "\n";
            duplicates++;
        }
    }
    
    if (duplicates == 0) {
        std::cout << "  No duplicates found in instance buffer\n";
    }
}

void checkMirroringIssue() {
    std::cout << "\n=== CHECKING FOR MIRRORING ISSUE ===\n";
    
    // Theory: Maybe transforms are creating mirrored geometry
    // Test UV to cube transformations for opposite faces
    
    struct Transform {
        glm::dmat4 matrix;
        std::string face;
    };
    
    // Create transforms for opposite faces
    std::vector<Transform> transforms;
    
    // +X face transform (X fixed at 1)
    glm::dmat4 posX(1.0);
    posX[0] = glm::dvec4(0, 0, 2, 0);    // U -> Z (range -1 to 1)
    posX[1] = glm::dvec4(0, 2, 0, 0);    // V -> Y (range -1 to 1)
    posX[3] = glm::dvec4(1, -1, -1, 1);  // Origin at (1, -1, -1)
    transforms.push_back({posX, "+X"});
    
    // -X face transform (X fixed at -1)
    glm::dmat4 negX(1.0);
    negX[0] = glm::dvec4(0, 0, 2, 0);    // U -> Z
    negX[1] = glm::dvec4(0, 2, 0, 0);    // V -> Y
    negX[3] = glm::dvec4(-1, -1, -1, 1); // Origin at (-1, -1, -1)
    transforms.push_back({negX, "-X"});
    
    // Test center point (0.5, 0.5) for each face
    glm::dvec4 centerUV(0.5, 0.5, 0, 1);
    
    std::cout << "Testing UV(0.5, 0.5) transformation:\n";
    for (const auto& t : transforms) {
        glm::dvec3 cube = glm::dvec3(t.matrix * centerUV);
        std::cout << "  " << t.face << " -> Cube(" 
                  << cube.x << ", " << cube.y << ", " << cube.z << ")\n";
        
        // Check if this creates a mirrored position
        glm::dvec3 mirrored(-cube.x, cube.y, cube.z);
        std::cout << "    Mirrored would be: (" 
                  << mirrored.x << ", " << mirrored.y << ", " << mirrored.z << ")\n";
    }
    
    // Check if they map to mirrored positions
    glm::dvec3 posXCenter = glm::dvec3(posX * centerUV);
    glm::dvec3 negXCenter = glm::dvec3(negX * centerUV);
    
    if (std::abs(posXCenter.x + negXCenter.x) < 0.001 &&
        std::abs(posXCenter.y - negXCenter.y) < 0.001 &&
        std::abs(posXCenter.z - negXCenter.z) < 0.001) {
        std::cout << "\n  MIRRORING DETECTED: +X and -X faces create mirrored geometry!\n";
    }
}

void analyzeRenderingPath() {
    std::cout << "\n=== ANALYZING RENDERING PATH ===\n";
    
    std::cout << "Based on the code analysis:\n";
    std::cout << "1. LODManager collects patches from all 6 faces\n";
    std::cout << "2. Each patch gets a transform matrix in the instance buffer\n";
    std::cout << "3. Vertex shader applies transform to generate world positions\n";
    std::cout << "4. Problem might occur if:\n";
    std::cout << "   - Patches are collected twice (once per update?)\n";
    std::cout << "   - Transform matrices are incorrect\n";
    std::cout << "   - Instance buffer is not cleared between frames\n";
    std::cout << "   - Draw call uses wrong instance count\n";
}

void suggestDebugging() {
    std::cout << "\n=== DEBUGGING SUGGESTIONS ===\n";
    
    std::cout << "1. Add logging to LODManager::updateQuadtreeData():\n";
    std::cout << "   - Log number of patches before and after update\n";
    std::cout << "   - Check if patches vector has duplicates\n";
    std::cout << "   - Verify instance count matches patches.size()\n\n";
    
    std::cout << "2. Check vertex shader:\n";
    std::cout << "   - Log gl_InstanceIndex to ensure it's in valid range\n";
    std::cout << "   - Verify patchTransform matrix values\n";
    std::cout << "   - Check if same patch is rendered multiple times\n\n";
    
    std::cout << "3. Verify draw call:\n";
    std::cout << "   - Ensure instanceCount is correct\n";
    std::cout << "   - Check if draw is called multiple times per frame\n";
    std::cout << "   - Verify vertex/index buffers are correct\n\n";
    
    std::cout << "4. Add unique patch ID to instance data:\n";
    std::cout << "   - Assign unique ID to each patch\n";
    std::cout << "   - Pass ID through instance buffer\n";
    std::cout << "   - Color patches based on ID to visualize duplicates\n";
}

int main() {
    std::cout << "=== INVESTIGATING INSTANCE DUPLICATION ===\n";
    std::cout << "Hypothesis: The 'double planet' might be caused by:\n";
    std::cout << "- Patches being added to instance buffer multiple times\n";
    std::cout << "- Incorrect instance count in draw call\n";
    std::cout << "- Transform matrices creating mirrored geometry\n";
    
    simulateLODCollection();
    checkMirroringIssue();
    analyzeRenderingPath();
    suggestDebugging();
    
    std::cout << "\n=== MOST LIKELY CAUSE ===\n";
    std::cout << "Based on the symptoms (double planet, black hole):\n";
    std::cout << "1. Instance buffer contains duplicate patches\n";
    std::cout << "2. Or patches from different LOD levels overlap\n";
    std::cout << "3. Or transform matrices are creating inverted geometry\n";
    std::cout << "\nRecommended: Add logging to track instance count and patch IDs\n";
    
    return 0;
}
