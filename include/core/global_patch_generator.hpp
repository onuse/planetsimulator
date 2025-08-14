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
            
            const double eps = 1e-6;  // Much smaller epsilon for better precision
            
            glm::dmat4 transform(1.0);
            
            if (range.x < eps) {
                // X is fixed - patch is on +X or -X face
                // CRITICAL: Use exact boundary value if patch is at face boundary
                double x = minBounds.x;  // Use exact value, not average
                if (std::abs(std::abs(x) - 1.0) < 1e-5) {
                    x = (x > 0) ? 1.0 : -1.0;  // Snap to exact boundary
                }
                
                // FIX: U maps to Z (horizontal in child layout), V maps to Y (vertical in child layout)
                // This ensures children 0,1 (which share a vertical edge) have matching positions
                transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
                transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
                transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);        // Dummy for 4x4
                transform[3] = glm::dvec4(x, minBounds.y, minBounds.z, 1.0);
            }
            else if (range.y < eps) {
                // Y is fixed - patch is on +Y or -Y face
                // CRITICAL: Use exact boundary value if patch is at face boundary
                double y = minBounds.y;  // Use exact value, not average
                if (std::abs(std::abs(y) - 1.0) < 1e-5) {
                    y = (y > 0) ? 1.0 : -1.0;  // Snap to exact boundary
                }
                
                // U maps to X, V maps to Z 
                transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
                transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // V -> Z
                transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);        // Dummy for 4x4
                transform[3] = glm::dvec4(minBounds.x, y, minBounds.z, 1.0);
            }
            else if (range.z < eps) {
                // Z is fixed - patch is on +Z or -Z face
                // CRITICAL: Use exact boundary value if patch is at face boundary
                double z = minBounds.z;  // Use exact value, not average
                if (std::abs(std::abs(z) - 1.0) < 1e-5) {
                    z = (z > 0) ? 1.0 : -1.0;  // Snap to exact boundary
                }
                
                // U maps to X, V maps to Y
                transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
                transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
                transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);        // Dummy for 4x4
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
        
        // CRITICAL FIX: Ensure face boundaries are EXACTLY at ±1
        // This prevents gaps between patches from different faces
        const float BOUNDARY = 1.0f;
        const float SNAP_EPSILON = 1e-5f;  // Snap if within this distance of boundary
        
        // Snap min bounds to exact boundaries if close
        for (int i = 0; i < 3; i++) {
            if (std::abs(std::abs(patch.minBounds[i]) - BOUNDARY) < SNAP_EPSILON) {
                patch.minBounds[i] = (patch.minBounds[i] > 0) ? BOUNDARY : -BOUNDARY;
            }
        }
        
        // Snap max bounds to exact boundaries if close
        for (int i = 0; i < 3; i++) {
            if (std::abs(std::abs(patch.maxBounds[i]) - BOUNDARY) < SNAP_EPSILON) {
                patch.maxBounds[i] = (patch.maxBounds[i] > 0) ? BOUNDARY : -BOUNDARY;
            }
        }
        
        patch.center = (patch.minBounds + patch.maxBounds) * 0.5f;
        patch.level = level;
        patch.faceId = faceId;
        return patch;
    }
};

} // namespace core