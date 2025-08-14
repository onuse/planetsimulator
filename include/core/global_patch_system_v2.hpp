#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace core {

/**
 * Global Patch System V2 - A truly global coordinate approach
 * 
 * Key Innovation:
 * - Patches are defined by their 3D bounding box in cube space
 * - UV coordinates ALWAYS map consistently:
 *   UV(0,0) -> min corner in 3D space
 *   UV(1,1) -> max corner in 3D space
 * - No face-specific logic in coordinate mapping
 */
class GlobalPatchSystemV2 {
public:
    struct Patch {
        // The actual 3D bounding box of the patch
        glm::dvec3 minBounds;
        glm::dvec3 maxBounds;
        glm::dvec3 center;
        
        // Which face this patch primarily belongs to (for culling only)
        int primaryFace;
        
        // Hierarchy info
        uint32_t level;
        uint32_t id;
        uint32_t parentId;
        uint32_t childIds[4];
        
        // LOD info
        float screenSpaceError;
        bool needsSubdivision;
        bool isLeaf;
    };
    
    /**
     * Create a patch from its 3D bounding box
     * This is the ONLY way patches are created - no face-specific logic
     */
    static Patch createPatch(const glm::dvec3& minBounds, 
                            const glm::dvec3& maxBounds,
                            uint32_t level) {
        Patch patch;
        patch.minBounds = minBounds;
        patch.maxBounds = maxBounds;
        patch.center = (minBounds + maxBounds) * 0.5;
        patch.level = level;
        
        // Determine primary face for culling purposes only
        glm::dvec3 absCenter = glm::abs(patch.center);
        if (absCenter.x > absCenter.y && absCenter.x > absCenter.z) {
            patch.primaryFace = patch.center.x > 0 ? 0 : 1; // +X or -X
        } else if (absCenter.y > absCenter.z) {
            patch.primaryFace = patch.center.y > 0 ? 2 : 3; // +Y or -Y
        } else {
            patch.primaryFace = patch.center.z > 0 ? 4 : 5; // +Z or -Z
        }
        
        patch.isLeaf = true;
        patch.needsSubdivision = false;
        patch.screenSpaceError = 0.0f;
        
        return patch;
    }
    
    /**
     * Create the transform matrix that maps UV [0,1]² to the patch's 3D bounds
     * This is the CRITICAL function that ensures global consistency
     */
    static glm::dmat4 createPatchTransform(const Patch& patch) {
        // Patches are 2D surfaces on cube faces, not 3D volumes
        // We need to map 2D UV coordinates to the 3D patch surface
        
        glm::dvec3 scale = patch.maxBounds - patch.minBounds;
        double eps = 0.001;
        
        glm::dmat4 transform(1.0);
        
        // Determine which face this patch is on and set up the transform accordingly
        if (std::abs(scale.x) < eps) {
            // X is fixed (on +X or -X face)
            // UV u maps to Y, UV v maps to Z
            double fixedX = patch.center.x;
            transform[0] = glm::dvec4(0.0, scale.y, 0.0, 0.0);  // u -> Y
            transform[1] = glm::dvec4(0.0, 0.0, scale.z, 0.0);  // v -> Z
            transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
            transform[3] = glm::dvec4(fixedX, patch.minBounds.y, patch.minBounds.z, 1.0);
        }
        else if (std::abs(scale.y) < eps) {
            // Y is fixed (on +Y or -Y face)
            // UV u maps to X, UV v maps to Z
            double fixedY = patch.center.y;
            transform[0] = glm::dvec4(scale.x, 0.0, 0.0, 0.0);  // u -> X
            transform[1] = glm::dvec4(0.0, 0.0, scale.z, 0.0);  // v -> Z
            transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
            transform[3] = glm::dvec4(patch.minBounds.x, fixedY, patch.minBounds.z, 1.0);
        }
        else if (std::abs(scale.z) < eps) {
            // Z is fixed (on +Z or -Z face)
            // UV u maps to X, UV v maps to Y
            double fixedZ = patch.center.z;
            transform[0] = glm::dvec4(scale.x, 0.0, 0.0, 0.0);  // u -> X
            transform[1] = glm::dvec4(0.0, scale.y, 0.0, 0.0);  // v -> Y
            transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
            transform[3] = glm::dvec4(patch.minBounds.x, patch.minBounds.y, fixedZ, 1.0);
        }
        else {
            // General 3D patch (shouldn't happen for cube faces, but handle it)
            transform[0][0] = scale.x;
            transform[1][1] = scale.y;
            transform[2][2] = scale.z;
            transform[3] = glm::dvec4(patch.minBounds, 1.0);
        }
        
        return transform;
    }
    
    /**
     * Get the 4 corners of a patch face for rendering
     * Note: This is only used for generating mesh vertices
     */
    static void getPatchCorners(const Patch& patch, glm::dvec3 corners[4]) {
        // Determine which face this patch is on
        double eps = 0.001;
        
        if (std::abs(patch.maxBounds.x - 1.0) < eps) {
            // +X face: X is fixed at max, vary Y and Z
            corners[0] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[1] = glm::dvec3(patch.maxBounds.x, patch.maxBounds.y, patch.minBounds.z);
            corners[2] = glm::dvec3(patch.maxBounds.x, patch.maxBounds.y, patch.maxBounds.z);
            corners[3] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.maxBounds.z);
        }
        else if (std::abs(patch.minBounds.x + 1.0) < eps) {
            // -X face: X is fixed at min, vary Y and Z
            corners[0] = glm::dvec3(patch.minBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[1] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.minBounds.z);
            corners[2] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.maxBounds.z);
            corners[3] = glm::dvec3(patch.minBounds.x, patch.minBounds.y, patch.maxBounds.z);
        }
        else if (std::abs(patch.maxBounds.y - 1.0) < eps) {
            // +Y face: Y is fixed at max, vary X and Z
            corners[0] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.minBounds.z);
            corners[1] = glm::dvec3(patch.maxBounds.x, patch.maxBounds.y, patch.minBounds.z);
            corners[2] = glm::dvec3(patch.maxBounds.x, patch.maxBounds.y, patch.maxBounds.z);
            corners[3] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.maxBounds.z);
        }
        else if (std::abs(patch.minBounds.y + 1.0) < eps) {
            // -Y face: Y is fixed at min, vary X and Z
            corners[0] = glm::dvec3(patch.minBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[1] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[2] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.maxBounds.z);
            corners[3] = glm::dvec3(patch.minBounds.x, patch.minBounds.y, patch.maxBounds.z);
        }
        else if (std::abs(patch.maxBounds.z - 1.0) < eps) {
            // +Z face: Z is fixed at max, vary X and Y
            corners[0] = glm::dvec3(patch.minBounds.x, patch.minBounds.y, patch.maxBounds.z);
            corners[1] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.maxBounds.z);
            corners[2] = glm::dvec3(patch.maxBounds.x, patch.maxBounds.y, patch.maxBounds.z);
            corners[3] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.maxBounds.z);
        }
        else if (std::abs(patch.minBounds.z + 1.0) < eps) {
            // -Z face: Z is fixed at min, vary X and Y
            corners[0] = glm::dvec3(patch.minBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[1] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[2] = glm::dvec3(patch.maxBounds.x, patch.maxBounds.y, patch.minBounds.z);
            corners[3] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.minBounds.z);
        }
        else {
            // Edge or corner patch - use the 4 most relevant corners
            // This is a simplified approach - may need refinement
            corners[0] = patch.minBounds;
            corners[1] = glm::dvec3(patch.maxBounds.x, patch.minBounds.y, patch.minBounds.z);
            corners[2] = patch.maxBounds;
            corners[3] = glm::dvec3(patch.minBounds.x, patch.maxBounds.y, patch.maxBounds.z);
        }
    }
    
    /**
     * Subdivide a patch into 4 children
     * Uses consistent 3D space subdivision
     */
    static std::vector<Patch> subdivide(const Patch& parent) {
        std::vector<Patch> children(4);
        
        glm::dvec3 mid = parent.center;
        
        // Subdivide in the two varying dimensions
        // This depends on which face the patch is on
        double eps = 0.001;
        
        if (std::abs(parent.maxBounds.x - parent.minBounds.x) < eps) {
            // X is fixed (on +X or -X face), subdivide in Y and Z
            double fixedX = parent.center.x;
            
            children[0] = createPatch(
                glm::dvec3(fixedX, parent.minBounds.y, parent.minBounds.z),
                glm::dvec3(fixedX, mid.y, mid.z),
                parent.level + 1
            );
            children[1] = createPatch(
                glm::dvec3(fixedX, mid.y, parent.minBounds.z),
                glm::dvec3(fixedX, parent.maxBounds.y, mid.z),
                parent.level + 1
            );
            children[2] = createPatch(
                glm::dvec3(fixedX, mid.y, mid.z),
                glm::dvec3(fixedX, parent.maxBounds.y, parent.maxBounds.z),
                parent.level + 1
            );
            children[3] = createPatch(
                glm::dvec3(fixedX, parent.minBounds.y, mid.z),
                glm::dvec3(fixedX, mid.y, parent.maxBounds.z),
                parent.level + 1
            );
        }
        else if (std::abs(parent.maxBounds.y - parent.minBounds.y) < eps) {
            // Y is fixed (on +Y or -Y face), subdivide in X and Z
            double fixedY = parent.center.y;
            
            children[0] = createPatch(
                glm::dvec3(parent.minBounds.x, fixedY, parent.minBounds.z),
                glm::dvec3(mid.x, fixedY, mid.z),
                parent.level + 1
            );
            children[1] = createPatch(
                glm::dvec3(mid.x, fixedY, parent.minBounds.z),
                glm::dvec3(parent.maxBounds.x, fixedY, mid.z),
                parent.level + 1
            );
            children[2] = createPatch(
                glm::dvec3(mid.x, fixedY, mid.z),
                glm::dvec3(parent.maxBounds.x, fixedY, parent.maxBounds.z),
                parent.level + 1
            );
            children[3] = createPatch(
                glm::dvec3(parent.minBounds.x, fixedY, mid.z),
                glm::dvec3(mid.x, fixedY, parent.maxBounds.z),
                parent.level + 1
            );
        }
        else if (std::abs(parent.maxBounds.z - parent.minBounds.z) < eps) {
            // Z is fixed (on +Z or -Z face), subdivide in X and Y
            double fixedZ = parent.center.z;
            
            children[0] = createPatch(
                glm::dvec3(parent.minBounds.x, parent.minBounds.y, fixedZ),
                glm::dvec3(mid.x, mid.y, fixedZ),
                parent.level + 1
            );
            children[1] = createPatch(
                glm::dvec3(mid.x, parent.minBounds.y, fixedZ),
                glm::dvec3(parent.maxBounds.x, mid.y, fixedZ),
                parent.level + 1
            );
            children[2] = createPatch(
                glm::dvec3(mid.x, mid.y, fixedZ),
                glm::dvec3(parent.maxBounds.x, parent.maxBounds.y, fixedZ),
                parent.level + 1
            );
            children[3] = createPatch(
                glm::dvec3(parent.minBounds.x, mid.y, fixedZ),
                glm::dvec3(mid.x, parent.maxBounds.y, fixedZ),
                parent.level + 1
            );
        }
        else {
            // General case - shouldn't happen for cube face patches
            // But handle it anyway for robustness
            children[0] = createPatch(parent.minBounds, mid, parent.level + 1);
            children[1] = createPatch(
                glm::dvec3(mid.x, parent.minBounds.y, parent.minBounds.z),
                glm::dvec3(parent.maxBounds.x, mid.y, mid.z),
                parent.level + 1
            );
            children[2] = createPatch(mid, parent.maxBounds, parent.level + 1);
            children[3] = createPatch(
                glm::dvec3(parent.minBounds.x, mid.y, mid.z),
                glm::dvec3(mid.x, parent.maxBounds.y, parent.maxBounds.z),
                parent.level + 1
            );
        }
        
        // Set parent-child relationships
        for (int i = 0; i < 4; i++) {
            children[i].parentId = parent.id;
        }
        
        return children;
    }
};

} // namespace core