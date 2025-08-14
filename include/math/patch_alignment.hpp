#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace math {

// Snap coordinates to ensure exact vertex sharing at patch boundaries
inline glm::vec3 snapToPatchGrid(const glm::vec3& pos, int level) {
    // At each level, patches are divided into a grid
    // Level 0: 1x1 per face
    // Level 1: 2x2 per face
    // Level 2: 4x4 per face
    // Level n: (2^n)x(2^n) per face
    
    float gridSize = 1.0f / (1 << level);  // 2^level divisions
    float snapPrecision = gridSize * 0.001f; // Snap within 0.1% of grid size
    
    glm::vec3 snapped;
    
    // Snap each coordinate to the nearest grid point
    snapped.x = std::round(pos.x / snapPrecision) * snapPrecision;
    snapped.y = std::round(pos.y / snapPrecision) * snapPrecision;
    snapped.z = std::round(pos.z / snapPrecision) * snapPrecision;
    
    return snapped;
}

// Ensure face boundary vertices are exactly on the face plane
inline glm::vec3 snapToFaceBoundary(const glm::vec3& pos, int faceId) {
    glm::vec3 snapped = pos;
    const float FACE_COORD = 1.0f;  // Cube faces are at ±1
    const float EPSILON = 1e-7f;  // Was 0.001f, which caused 12km gaps!
    
    switch (faceId) {
        case 0: // +X face
            if (std::abs(pos.x - FACE_COORD) < EPSILON) {
                snapped.x = FACE_COORD;
            }
            break;
        case 1: // -X face
            if (std::abs(pos.x + FACE_COORD) < EPSILON) {
                snapped.x = -FACE_COORD;
            }
            break;
        case 2: // +Y face
            if (std::abs(pos.y - FACE_COORD) < EPSILON) {
                snapped.y = FACE_COORD;
            }
            break;
        case 3: // -Y face
            if (std::abs(pos.y + FACE_COORD) < EPSILON) {
                snapped.y = -FACE_COORD;
            }
            break;
        case 4: // +Z face
            if (std::abs(pos.z - FACE_COORD) < EPSILON) {
                snapped.z = FACE_COORD;
            }
            break;
        case 5: // -Z face
            if (std::abs(pos.z + FACE_COORD) < EPSILON) {
                snapped.z = -FACE_COORD;
            }
            break;
    }
    
    // Also snap edges where two coordinates are at boundaries
    // This ensures corner vertices are exactly shared
    if (std::abs(std::abs(snapped.x) - FACE_COORD) < EPSILON) {
        snapped.x = (snapped.x > 0) ? FACE_COORD : -FACE_COORD;
    }
    if (std::abs(std::abs(snapped.y) - FACE_COORD) < EPSILON) {
        snapped.y = (snapped.y > 0) ? FACE_COORD : -FACE_COORD;
    }
    if (std::abs(std::abs(snapped.z) - FACE_COORD) < EPSILON) {
        snapped.z = (snapped.z > 0) ? FACE_COORD : -FACE_COORD;
    }
    
    return snapped;
}

// Check if a point is on a face boundary
inline bool isOnFaceBoundary(const glm::vec3& pos, float epsilon = 1e-7f) {
    const float FACE_COORD = 1.0f;
    
    // Check if any coordinate is at ±1 (cube face boundary)
    return (std::abs(std::abs(pos.x) - FACE_COORD) < epsilon ||
            std::abs(std::abs(pos.y) - FACE_COORD) < epsilon ||
            std::abs(std::abs(pos.z) - FACE_COORD) < epsilon);
}

// Get which faces a boundary point belongs to
inline int getFacesAtPoint(const glm::vec3& pos, int faces[3]) {
    const float FACE_COORD = 1.0f;
    const float EPSILON = 1e-7f;  // Was 0.001f, which caused 12km gaps!
    int count = 0;
    
    if (std::abs(pos.x - FACE_COORD) < EPSILON) {
        faces[count++] = 0; // +X
    }
    if (std::abs(pos.x + FACE_COORD) < EPSILON) {
        faces[count++] = 1; // -X
    }
    if (std::abs(pos.y - FACE_COORD) < EPSILON) {
        faces[count++] = 2; // +Y
    }
    if (std::abs(pos.y + FACE_COORD) < EPSILON) {
        faces[count++] = 3; // -Y
    }
    if (std::abs(pos.z - FACE_COORD) < EPSILON) {
        faces[count++] = 4; // +Z
    }
    if (std::abs(pos.z + FACE_COORD) < EPSILON) {
        faces[count++] = 5; // -Z
    }
    
    return count;
}

} // namespace math