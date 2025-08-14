#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace core {

/**
 * Canonical Patch System - Ensures globally consistent UV mapping
 * 
 * The key insight: UV coordinates should map to the same relative position
 * on ANY patch, regardless of which cube face it's on.
 * 
 * UV(0,0) always maps to the "minimum" corner (smallest X, then Y, then Z)
 * UV(1,1) always maps to the "maximum" corner (largest X, then Y, then Z)
 */
class CanonicalPatchSystem {
public:
    /**
     * Create a transform matrix that maps UV to world space with GLOBAL consistency
     * This is the critical function that ensures adjacent patches agree on shared vertices
     */
    static glm::dmat4 createCanonicalTransform(const glm::vec3 corners[4], uint32_t faceId) {
        // Determine the actual min/max bounds of this patch
        glm::dvec3 minBounds(1e9), maxBounds(-1e9);
        for (int i = 0; i < 4; i++) {
            minBounds = glm::min(minBounds, glm::dvec3(corners[i]));
            maxBounds = glm::max(maxBounds, glm::dvec3(corners[i]));
        }
        
        // Determine which dimension is fixed (identifies the cube face)
        glm::dvec3 range = maxBounds - minBounds;
        double eps = 0.001;
        
        glm::dmat4 transform(1.0);
        
        if (range.x < eps) {
            // X is fixed (+X or -X face)
            // Map UV(u,v) -> (fixedX, minY + v*rangeY, minZ + u*rangeZ)
            // This ensures UV mapping is consistent:
            // - u always controls the "first" varying dimension (Z for X-faces)
            // - v always controls the "second" varying dimension (Y for X-faces)
            double fixedX = (minBounds.x + maxBounds.x) * 0.5;
            
            // Standard mapping for X-faces:
            transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);  // u -> Z
            transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);  // v -> Y
            transform[2] = glm::dvec4(1.0, 0.0, 0.0, 0.0);      // Normal along X
            transform[3] = glm::dvec4(fixedX, minBounds.y, minBounds.z, 1.0);
        }
        else if (range.y < eps) {
            // Y is fixed (+Y or -Y face)
            // Map UV(u,v) -> (minX + u*rangeX, fixedY, minZ + v*rangeZ)
            double fixedY = (minBounds.y + maxBounds.y) * 0.5;
            
            // Standard mapping for Y-faces:
            transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);  // u -> X
            transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);  // v -> Z
            transform[2] = glm::dvec4(0.0, 1.0, 0.0, 0.0);      // Normal along Y
            transform[3] = glm::dvec4(minBounds.x, fixedY, minBounds.z, 1.0);
        }
        else if (range.z < eps) {
            // Z is fixed (+Z or -Z face)
            // Map UV(u,v) -> (minX + u*rangeX, minY + v*rangeY, fixedZ)
            double fixedZ = (minBounds.z + maxBounds.z) * 0.5;
            
            // Standard mapping for Z-faces:
            transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);  // u -> X
            transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);  // v -> Y
            transform[2] = glm::dvec4(0.0, 0.0, 1.0, 0.0);      // Normal along Z
            transform[3] = glm::dvec4(minBounds.x, minBounds.y, fixedZ, 1.0);
        }
        else {
            // This shouldn't happen for cube face patches
            // Fall back to corner-based mapping
            glm::dvec3 bottomLeft = glm::dvec3(corners[0]);
            glm::dvec3 bottomRight = glm::dvec3(corners[1]);
            glm::dvec3 topLeft = glm::dvec3(corners[3]);
            
            glm::dvec3 right = bottomRight - bottomLeft;
            glm::dvec3 up = topLeft - bottomLeft;
            glm::dvec3 normal = glm::normalize(glm::cross(right, up));
            
            transform[0] = glm::dvec4(right, 0.0);
            transform[1] = glm::dvec4(up, 0.0);
            transform[2] = glm::dvec4(normal, 0.0);
            transform[3] = glm::dvec4(bottomLeft, 1.0);
        }
        
        return transform;
    }
    
    /**
     * Reorder patch corners to canonical order based on their actual 3D positions
     * This ensures consistent corner ordering regardless of face
     */
    static void canonicalizeCorners(glm::vec3 corners[4], uint32_t faceId) {
        // Find the actual min/max bounds
        glm::vec3 minBounds(1e9), maxBounds(-1e9);
        for (int i = 0; i < 4; i++) {
            minBounds = glm::min(minBounds, corners[i]);
            maxBounds = glm::max(maxBounds, corners[i]);
        }
        
        // Determine which dimension is fixed
        glm::vec3 range = maxBounds - minBounds;
        double eps = 0.001;
        
        glm::vec3 reordered[4];
        
        if (range.x < eps) {
            // X-face: order by (Y,Z)
            // [0] = min Y, min Z
            // [1] = min Y, max Z  
            // [2] = max Y, max Z
            // [3] = max Y, min Z
            
            for (int i = 0; i < 4; i++) {
                bool isMinY = (std::abs(corners[i].y - minBounds.y) < eps);
                bool isMinZ = (std::abs(corners[i].z - minBounds.z) < eps);
                
                if (isMinY && isMinZ) reordered[0] = corners[i];
                else if (isMinY && !isMinZ) reordered[1] = corners[i];
                else if (!isMinY && !isMinZ) reordered[2] = corners[i];
                else reordered[3] = corners[i];
            }
        }
        else if (range.y < eps) {
            // Y-face: order by (X,Z)
            // [0] = min X, min Z
            // [1] = max X, min Z
            // [2] = max X, max Z
            // [3] = min X, max Z
            
            for (int i = 0; i < 4; i++) {
                bool isMinX = (std::abs(corners[i].x - minBounds.x) < eps);
                bool isMinZ = (std::abs(corners[i].z - minBounds.z) < eps);
                
                if (isMinX && isMinZ) reordered[0] = corners[i];
                else if (!isMinX && isMinZ) reordered[1] = corners[i];
                else if (!isMinX && !isMinZ) reordered[2] = corners[i];
                else reordered[3] = corners[i];
            }
        }
        else if (range.z < eps) {
            // Z-face: order by (X,Y)
            // [0] = min X, min Y
            // [1] = max X, min Y
            // [2] = max X, max Y
            // [3] = min X, max Y
            
            for (int i = 0; i < 4; i++) {
                bool isMinX = (std::abs(corners[i].x - minBounds.x) < eps);
                bool isMinY = (std::abs(corners[i].y - minBounds.y) < eps);
                
                if (isMinX && isMinY) reordered[0] = corners[i];
                else if (!isMinX && isMinY) reordered[1] = corners[i];
                else if (!isMinX && !isMinY) reordered[2] = corners[i];
                else reordered[3] = corners[i];
            }
        }
        else {
            // No fixed dimension - keep original order
            return;
        }
        
        // Copy reordered corners back
        for (int i = 0; i < 4; i++) {
            corners[i] = reordered[i];
        }
    }
};

} // namespace core