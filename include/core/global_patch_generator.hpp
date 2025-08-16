#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>  // For std::abs with doubles

namespace core {

/**
 * Global Patch Generator - Creates patches with truly global coordinates from the start
 * 
 * Key principle: A patch is defined by its 3D bounding box in cube space.
 * UV coordinates ALWAYS map the same way regardless of face:
 * - UV(0,0) maps to the minimum corner (smallest X, then Y, then Z)  
 * - UV(1,1) maps to the maximum corner (largest X, then Y, then Z)
 */
class GlobalPatchGenerator {
public:
    struct GlobalPatch {
        glm::vec3 minBounds;
        glm::vec3 maxBounds;
        glm::vec3 center;
        uint32_t level;
        int faceId;  // Which face this patch primarily belongs to (for culling only)
        
        // Get the 4 corners in canonical order
        void getCorners(glm::vec3 corners[4]) const {
            // Determine which dimension is fixed (which face we're on)
            glm::vec3 range = maxBounds - minBounds;
            const float eps = 1e-6f;  // Consistent with subdivide epsilon
            
            if (range.x < eps) {
                // X is fixed - patch is on +X or -X face
                // FIX: Corners now match U->Z, V->Y mapping
                // UV(0,0) -> (x, minY, minZ)
                // UV(1,0) -> (x, minY, maxZ) 
                // UV(1,1) -> (x, maxY, maxZ)
                // UV(0,1) -> (x, maxY, minZ)
                float x = center.x;
                corners[0] = glm::vec3(x, minBounds.y, minBounds.z);
                corners[1] = glm::vec3(x, minBounds.y, maxBounds.z);
                corners[2] = glm::vec3(x, maxBounds.y, maxBounds.z);
                corners[3] = glm::vec3(x, maxBounds.y, minBounds.z);
            }
            else if (range.y < eps) {
                // Y is fixed - patch is on +Y or -Y face
                // Corners ordered as: (min_x, fixed_y, min_z), (max_x, fixed_y, min_z),
                //                     (max_x, fixed_y, max_z), (min_x, fixed_y, max_z)
                float y = center.y;
                corners[0] = glm::vec3(minBounds.x, y, minBounds.z);
                corners[1] = glm::vec3(maxBounds.x, y, minBounds.z);
                corners[2] = glm::vec3(maxBounds.x, y, maxBounds.z);
                corners[3] = glm::vec3(minBounds.x, y, maxBounds.z);
            }
            else if (range.z < eps) {
                // Z is fixed - patch is on +Z or -Z face
                // Corners ordered as: (min_x, min_y, fixed_z), (max_x, min_y, fixed_z),
                //                     (max_x, max_y, fixed_z), (min_x, max_y, fixed_z)
                float z = center.z;
                corners[0] = glm::vec3(minBounds.x, minBounds.y, z);
                corners[1] = glm::vec3(maxBounds.x, minBounds.y, z);
                corners[2] = glm::vec3(maxBounds.x, maxBounds.y, z);
                corners[3] = glm::vec3(minBounds.x, maxBounds.y, z);
            }
            else {
                // Edge or corner patch - shouldn't happen for face patches
                // Use the 4 corners of the bounding box
                corners[0] = minBounds;
                corners[1] = glm::vec3(maxBounds.x, minBounds.y, minBounds.z);
                corners[2] = maxBounds;
                corners[3] = glm::vec3(minBounds.x, maxBounds.y, maxBounds.z);
            }
        }
        
        // Create the transform matrix for this patch
        glm::dmat4 createTransform() const {
            // Calculate raw range
            glm::dvec3 rawRange = glm::dvec3(maxBounds - minBounds);
            
            // Ensure we always have positive ranges
            // This handles cases where bounds might be reversed
            glm::dvec3 range;
            range.x = std::abs(rawRange.x);
            range.y = std::abs(rawRange.y);
            range.z = std::abs(rawRange.z);
            
            // FIXED: Only apply MIN_RANGE to non-fixed dimensions!
            // For face patches, one dimension is fixed (range ~0) and should stay that way
            // The other two dimensions should use their actual range, not MIN_RANGE
            const double eps = 1e-6;  // Threshold for "is this dimension fixed?"
            const double MIN_RANGE = 1e-5;  // Minimum range for degenerate cases
            
            glm::dmat4 transform(1.0);
            
            if (range.x < eps) {
                // X is fixed - patch is on +X or -X face
                double x = (minBounds.x + maxBounds.x) * 0.5;  // Use center for fixed dimension
                
                // CRITICAL FIX: Do NOT apply MIN_RANGE to y and z here!
                // These should be the actual patch size (e.g., 2.0 for root faces)
                // Use the actual range directly - it's already been abs()'d
                double actualRangeY = range.y;
                double actualRangeZ = range.z;
                
                // Only apply MIN_RANGE if the range is truly degenerate (shouldn't happen)
                if (actualRangeY < eps) actualRangeY = MIN_RANGE;  // Only for truly degenerate cases
                if (actualRangeZ < eps) actualRangeZ = MIN_RANGE;
                
                // FIX: U maps to Z (horizontal in child layout), V maps to Y (vertical in child layout)
                // This ensures children 0,1 (which share a vertical edge) have matching positions
                transform[0] = glm::dvec4(0.0, 0.0, actualRangeZ, 0.0);    // U -> Z
                transform[1] = glm::dvec4(0.0, actualRangeY, 0.0, 0.0);    // V -> Y
                transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);            // Dummy for 4x4
                transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
            }
            else if (range.y < eps) {
                // Y is fixed - patch is on +Y or -Y face
                double y = (minBounds.y + maxBounds.y) * 0.5;  // Use center for fixed dimension
                
                // CRITICAL FIX: Do NOT apply MIN_RANGE to x and z here!
                double actualRangeX = range.x;
                double actualRangeZ = range.z;
                
                if (actualRangeX < eps) actualRangeX = MIN_RANGE;
                if (actualRangeZ < eps) actualRangeZ = MIN_RANGE;
                
                // U maps to X, V maps to Z 
                transform[0] = glm::dvec4(actualRangeX, 0.0, 0.0, 0.0);    // U -> X
                transform[1] = glm::dvec4(0.0, 0.0, actualRangeZ, 0.0);    // V -> Z
                transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);            // Dummy for 4x4
                transform[3] = glm::dvec4(minBounds.x, y, minBounds.z, 1.0);
            }
            else if (range.z < eps) {
                // Z is fixed - patch is on +Z or -Z face
                double z = (minBounds.z + maxBounds.z) * 0.5;  // Use center for fixed dimension
                
                // CRITICAL FIX: Do NOT apply MIN_RANGE to x and y here!
                double actualRangeX = range.x;
                double actualRangeY = range.y;
                
                if (actualRangeX < eps) actualRangeX = MIN_RANGE;
                if (actualRangeY < eps) actualRangeY = MIN_RANGE;
                
                // U maps to X, V maps to Y
                transform[0] = glm::dvec4(actualRangeX, 0.0, 0.0, 0.0);    // U -> X
                transform[1] = glm::dvec4(0.0, actualRangeY, 0.0, 0.0);    // V -> Y
                transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);            // Dummy for 4x4
                transform[3] = glm::dvec4(minBounds.x, minBounds.y, z, 1.0);
            }
            
            return transform;
        }
    };
    
    /**
     * Create the 6 root patches for a cube
     */
    static std::vector<GlobalPatch> createRootPatches() {
        std::vector<GlobalPatch> roots;
        roots.reserve(6);
        
        // +X face
        roots.push_back(createPatch(glm::vec3(1, -1, -1), glm::vec3(1, 1, 1), 0, 0));
        // -X face  
        roots.push_back(createPatch(glm::vec3(-1, -1, -1), glm::vec3(-1, 1, 1), 0, 1));
        // +Y face
        roots.push_back(createPatch(glm::vec3(-1, 1, -1), glm::vec3(1, 1, 1), 0, 2));
        // -Y face
        roots.push_back(createPatch(glm::vec3(-1, -1, -1), glm::vec3(1, -1, 1), 0, 3));
        // +Z face
        roots.push_back(createPatch(glm::vec3(-1, -1, 1), glm::vec3(1, 1, 1), 0, 4));
        // -Z face
        roots.push_back(createPatch(glm::vec3(-1, -1, -1), glm::vec3(1, 1, -1), 0, 5));
        
        return roots;
    }
    
    /**
     * Subdivide a patch into 4 children using global coordinates
     */
    static std::vector<GlobalPatch> subdivide(const GlobalPatch& parent) {
        std::vector<GlobalPatch> children;
        children.reserve(4);
        
        glm::vec3 range = parent.maxBounds - parent.minBounds;
        const float eps = 1e-6f;  // Much smaller epsilon for better precision
        
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
    
private:
    static GlobalPatch createPatch(const glm::vec3& minBounds, const glm::vec3& maxBounds, 
                                   uint32_t level, int faceId) {
        GlobalPatch patch;
        patch.minBounds = minBounds;
        patch.maxBounds = maxBounds;
        
        // DO NOT snap to exact boundaries - we want to preserve the INSET
        // to prevent z-fighting between adjacent cube faces
        
        patch.center = (patch.minBounds + patch.maxBounds) * 0.5f;
        patch.level = level;
        patch.faceId = faceId;
        return patch;
    }
};

} // namespace core