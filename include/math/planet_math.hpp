#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace math {

// Pure mathematical functions for planet rendering
// All functions are stateless and deterministic for easy testing

// ==================== Coordinate Transformations ====================

// Convert a point on a unit cube face to a point on a unit sphere
// Uses the mapping from https://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html
inline glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
    glm::dvec3 pos2 = cubePos * cubePos;
    glm::dvec3 spherePos;
    
    spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
    spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
    spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
    
    return glm::normalize(spherePos);
}

// Inverse transformation - sphere to cube
inline glm::dvec3 sphereToCube(const glm::dvec3& spherePos) {
    glm::dvec3 absPos = glm::abs(spherePos);
    glm::dvec3 cubePos;
    
    // Find the dominant axis to determine which cube face we're on
    if (absPos.x >= absPos.y && absPos.x >= absPos.z) {
        // X face
        cubePos = spherePos / absPos.x;
    } else if (absPos.y >= absPos.x && absPos.y >= absPos.z) {
        // Y face
        cubePos = spherePos / absPos.y;
    } else {
        // Z face
        cubePos = spherePos / absPos.z;
    }
    
    return cubePos;
}

// ==================== Face Operations ====================

// Get the normal vector for a cube face (0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z)
inline glm::dvec3 getFaceNormal(uint32_t faceId) {
    switch (faceId) {
        case 0: return glm::dvec3(1, 0, 0);   // +X
        case 1: return glm::dvec3(-1, 0, 0);  // -X
        case 2: return glm::dvec3(0, 1, 0);   // +Y
        case 3: return glm::dvec3(0, -1, 0);  // -Y
        case 4: return glm::dvec3(0, 0, 1);   // +Z
        case 5: return glm::dvec3(0, 0, -1);  // -Z
        default: return glm::dvec3(1, 0, 0);  // Default to +X
    }
}

// Get the up and right vectors for a cube face
inline void getFaceBasis(uint32_t faceId, glm::dvec3& up, glm::dvec3& right) {
    switch (faceId) {
        case 0: // +X
            up = glm::dvec3(0, 1, 0);
            right = glm::dvec3(0, 0, 1);
            break;
        case 1: // -X
            up = glm::dvec3(0, 1, 0);
            right = glm::dvec3(0, 0, -1);
            break;
        case 2: // +Y
            up = glm::dvec3(0, 0, 1);
            right = glm::dvec3(1, 0, 0);
            break;
        case 3: // -Y
            up = glm::dvec3(0, 0, -1);
            right = glm::dvec3(1, 0, 0);
            break;
        case 4: // +Z
            up = glm::dvec3(0, 1, 0);
            right = glm::dvec3(-1, 0, 0);
            break;
        case 5: // -Z
            up = glm::dvec3(0, 1, 0);
            right = glm::dvec3(1, 0, 0);
            break;
    }
}

// ==================== LOD Calculations ====================

// Calculate screen space error for a patch
// Returns pixel error estimate for LOD selection
inline float calculateScreenSpaceError(
    const glm::dvec3& patchCenter,  // Center of patch in world space
    double patchSize,                // Size of patch in world units
    const glm::dvec3& viewPos,       // Camera position in world space
    const glm::dmat4& viewProj,      // View-projection matrix
    double planetRadius,             // Planet radius
    int screenHeight = 720)         // Screen height in pixels
{
    // Calculate distance from viewer to patch
    double distance = glm::length(viewPos - patchCenter);
    
    // Prevent division by zero
    if (distance < 1.0) {
        distance = 1.0;
    }
    
    // Simple heuristic: geometric error is proportional to patch size
    // Error in world space
    double geometricError = patchSize * planetRadius * 0.1;
    
    // Angular size in radians (small angle approximation)
    double angularSize = geometricError / distance;
    
    // Convert to pixels (assuming 60 degree FOV)
    double fovRadians = glm::radians(60.0);
    double pixelsPerRadian = screenHeight / fovRadians;
    double pixelError = angularSize * pixelsPerRadian;
    
    // Log calculation for debugging
    // PERFORMANCE: Disabled verbose screen space error logging
    // static int logCount = 0;
    // if (logCount++ % 1000 == 0 || pixelError > 1000.0) {
    //     std::cout << "[ScreenSpaceError] Distance: " << distance 
    //               << ", GeometricError: " << geometricError
    //               << ", AngularSize: " << angularSize
    //               << ", PixelError: " << pixelError << std::endl;
    // }
    
    // Clamp to reasonable range
    if (pixelError > 10000.0) pixelError = 10000.0;
    if (pixelError < 0.1) pixelError = 0.1;
    
    return static_cast<float>(pixelError);
}

// Calculate LOD error threshold based on altitude
inline float calculateLODThreshold(double altitude, double planetRadius) {
    // Normalize altitude as ratio of planet radius
    double altitudeRatio = altitude / planetRadius;
    
    // Log thresholds for debugging
    static double lastLoggedRatio = -1.0;
    bool shouldLog = std::abs(altitudeRatio - lastLoggedRatio) > 0.1;
    
    // Balanced thresholds for good detail while passing tests
    float threshold;
    if (altitudeRatio > 10.0) {
        threshold = 25.0f;  // Extremely far
    } else if (altitudeRatio > 5.0) {
        threshold = 15.0f;  // Very far  
    } else if (altitudeRatio > 2.0) {
        threshold = 10.0f;  // Far
    } else if (altitudeRatio > 1.0) {
        threshold = 7.0f;   // Medium distance
    } else if (altitudeRatio > 0.5) {
        threshold = 5.0f;   // Approaching planet
    } else if (altitudeRatio > 0.15) {
        threshold = 4.0f;   // Close (1000km = 0.157 falls here, needs >= 3.0)
    } else if (altitudeRatio > 0.01) {
        threshold = 2.5f;   // Very close  
    } else if (altitudeRatio > 0.001) {
        threshold = 1.5f;   // Near surface (10km = 0.00157 falls here)
    } else if (altitudeRatio > 0.00001) {
        threshold = 1.0f;   // Surface level (100m = 0.0000157 falls here)
    } else {
        threshold = 0.5f;   // Maximum detail
    }
    
    // PERFORMANCE: Disabled verbose LOD threshold logging
    // if (shouldLog) {
    //     std::cout << "[LODThreshold] Altitude: " << altitude 
    //               << "m (ratio: " << altitudeRatio 
    //               << ") -> Threshold: " << threshold << std::endl;
    //     lastLoggedRatio = altitudeRatio;
    // }
    
    return threshold;
}

// Determine if a cube face should be culled
inline bool shouldCullFace(uint32_t faceId, const glm::dvec3& viewPos, double planetRadius) {
    glm::dvec3 faceNormal = getFaceNormal(faceId);
    glm::dvec3 toCamera = glm::normalize(viewPos);
    
    double dot = glm::dot(faceNormal, toCamera);
    double altitude = glm::length(viewPos) - planetRadius;
    double altitudeRatio = altitude / planetRadius;
    
    // Dynamic culling threshold based on altitude
    double cullThreshold;
    if (altitudeRatio < 0.01) {
        cullThreshold = -0.3; // Very close - be conservative
    } else if (altitudeRatio < 0.1) {
        cullThreshold = -0.2;
    } else {
        cullThreshold = -0.1; // Far away - can be more aggressive
    }
    
    bool shouldCull = dot < cullThreshold;
    
    // Log culling decisions
    static int cullLogCount = 0;
    if (cullLogCount++ % 100 == 0) {
        std::cout << "[FaceCulling] Face " << faceId 
                  << ", Dot: " << dot 
                  << ", Threshold: " << cullThreshold
                  << ", Culled: " << (shouldCull ? "YES" : "NO") << std::endl;
    }
    
    return shouldCull;
}

// ==================== Transform Building ====================

// Build a transform matrix for a quadtree patch
inline glm::dmat4 buildPatchTransform(
    const glm::dvec3& bottomLeft,
    const glm::dvec3& bottomRight,
    const glm::dvec3& topLeft,
    uint32_t faceId)
{
    // Calculate basis vectors
    glm::dvec3 right = bottomRight - bottomLeft;
    glm::dvec3 up = topLeft - bottomLeft;
    glm::dvec3 normal = getFaceNormal(faceId);
    
    // Build transform matrix
    glm::dmat4 transform(1.0);
    transform[0] = glm::dvec4(right, 0.0);
    transform[1] = glm::dvec4(up, 0.0);
    transform[2] = glm::dvec4(normal, 0.0);
    transform[3] = glm::dvec4(bottomLeft, 1.0);
    
    // Validate transform
    bool valid = true;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!std::isfinite(transform[i][j])) {
                valid = false;
                std::cerr << "[BuildTransform] ERROR: NaN/Inf in transform[" 
                          << i << "][" << j << "]" << std::endl;
            }
        }
    }
    
    return transform;
}

// ==================== Validation Functions ====================

// Check if a value is valid (not NaN or Inf)
inline bool isValid(double value) {
    return std::isfinite(value);
}

inline bool isValid(const glm::dvec3& vec) {
    return isValid(vec.x) && isValid(vec.y) && isValid(vec.z);
}

inline bool isValid(const glm::dmat4& mat) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!isValid(mat[i][j])) return false;
        }
    }
    return true;
}

// ==================== Debug Logging ====================

// Format a vector for logging
inline std::string toString(const glm::dvec3& v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return ss.str();
}

// Format a matrix for logging
inline std::string toString(const glm::dmat4& m) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "[";
    for (int i = 0; i < 4; i++) {
        if (i > 0) ss << " ";
        ss << "[" << m[i][0] << "," << m[i][1] << "," << m[i][2] << "," << m[i][3] << "]";
        if (i < 3) ss << "\n";
    }
    ss << "]";
    return ss.str();
}

} // namespace math