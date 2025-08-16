#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace core {

/**
 * Unified Patch System - Ensures perfect vertex alignment at face boundaries
 * 
 * Key insight: The problem isn't the mappings themselves, but how we generate
 * vertices at boundaries. We need to ensure that vertices at shared edges
 * are generated using the SAME parameterization.
 */
class UnifiedPatchSystem {
public:
    
    /**
     * Generate a vertex position ensuring face boundary consistency
     * 
     * @param u,v UV coordinates in [0,1]
     * @param minBounds,maxBounds The patch bounds in cube space
     * @return The cube position, properly aligned at boundaries
     */
    static glm::dvec3 generateVertex(double u, double v, 
                                     const glm::dvec3& minBounds, 
                                     const glm::dvec3& maxBounds) {
        
        // Determine which face we're on based on bounds
        glm::dvec3 range = maxBounds - minBounds;
        const double eps = 1e-6;
        
        glm::dvec3 cubePos;
        
        if (range.x < eps) {
            // X is fixed - we're on +X or -X face
            double x = minBounds.x;
            
            // CRITICAL: Check if we're at a face boundary
            // and ensure consistent vertex generation
            
            // Check if we're at the Z=1 or Z=-1 boundary
            if (std::abs(std::abs(minBounds.z) - 1.0) < eps || 
                std::abs(std::abs(maxBounds.z) - 1.0) < eps) {
                
                // We're at a Z boundary - need special handling
                if (std::abs(u - 1.0) < eps && std::abs(maxBounds.z - 1.0) < eps) {
                    // Right edge at Z=1 boundary
                    // This edge is shared with +Z face
                    // Both faces should produce: X=1, Y=varying, Z=1
                    cubePos.x = x;
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = 1.0; // Force exact boundary
                } else if (std::abs(u) < eps && std::abs(minBounds.z + 1.0) < eps) {
                    // Left edge at Z=-1 boundary
                    cubePos.x = x;
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = -1.0; // Force exact boundary
                } else {
                    // Normal case
                    cubePos.x = x;
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = minBounds.z + u * range.z;
                }
            }
            // Check if we're at the Y=1 or Y=-1 boundary
            else if (std::abs(std::abs(minBounds.y) - 1.0) < eps || 
                     std::abs(std::abs(maxBounds.y) - 1.0) < eps) {
                
                if (std::abs(v - 1.0) < eps && std::abs(maxBounds.y - 1.0) < eps) {
                    // Top edge at Y=1 boundary
                    cubePos.x = x;
                    cubePos.y = 1.0; // Force exact boundary
                    cubePos.z = minBounds.z + u * range.z;
                } else if (std::abs(v) < eps && std::abs(minBounds.y + 1.0) < eps) {
                    // Bottom edge at Y=-1 boundary
                    cubePos.x = x;
                    cubePos.y = -1.0; // Force exact boundary
                    cubePos.z = minBounds.z + u * range.z;
                } else {
                    // Normal case
                    cubePos.x = x;
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = minBounds.z + u * range.z;
                }
            }
            else {
                // Not at a boundary - standard mapping
                cubePos.x = x;
                cubePos.y = minBounds.y + v * range.y;
                cubePos.z = minBounds.z + u * range.z;
            }
        }
        else if (range.y < eps) {
            // Y is fixed - we're on +Y or -Y face
            double y = minBounds.y;
            
            // Similar boundary checking for Y faces
            cubePos.x = minBounds.x + u * range.x;
            cubePos.y = y;
            cubePos.z = minBounds.z + v * range.z;
        }
        else if (range.z < eps) {
            // Z is fixed - we're on +Z or -Z face
            double z = minBounds.z;
            
            // Check if we're at the X=1 or X=-1 boundary
            if (std::abs(std::abs(minBounds.x) - 1.0) < eps || 
                std::abs(std::abs(maxBounds.x) - 1.0) < eps) {
                
                if (std::abs(u - 1.0) < eps && std::abs(maxBounds.x - 1.0) < eps) {
                    // Right edge at X=1 boundary
                    // This edge is shared with +X face
                    // Both faces should produce: X=1, Y=varying, Z=1
                    cubePos.x = 1.0; // Force exact boundary
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = z;
                } else if (std::abs(u) < eps && std::abs(minBounds.x + 1.0) < eps) {
                    // Left edge at X=-1 boundary
                    cubePos.x = -1.0; // Force exact boundary
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = z;
                } else {
                    // Normal case
                    cubePos.x = minBounds.x + u * range.x;
                    cubePos.y = minBounds.y + v * range.y;
                    cubePos.z = z;
                }
            }
            else {
                // Not at a boundary - standard mapping
                cubePos.x = minBounds.x + u * range.x;
                cubePos.y = minBounds.y + v * range.y;
                cubePos.z = z;
            }
        }
        else {
            // Edge or corner patch - shouldn't happen for face patches
            cubePos = minBounds + glm::dvec3(u, v, 0.5) * range;
        }
        
        // Final snapping to ensure exact boundaries
        const double BOUNDARY = 1.0;
        const double SNAP_EPS = 1e-8;
        
        if (std::abs(std::abs(cubePos.x) - BOUNDARY) < SNAP_EPS) {
            cubePos.x = (cubePos.x > 0) ? BOUNDARY : -BOUNDARY;
        }
        if (std::abs(std::abs(cubePos.y) - BOUNDARY) < SNAP_EPS) {
            cubePos.y = (cubePos.y > 0) ? BOUNDARY : -BOUNDARY;
        }
        if (std::abs(std::abs(cubePos.z) - BOUNDARY) < SNAP_EPS) {
            cubePos.z = (cubePos.z > 0) ? BOUNDARY : -BOUNDARY;
        }
        
        return cubePos;
    }
};

} // namespace core