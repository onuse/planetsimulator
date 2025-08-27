#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <unordered_map>
#include <functional>

namespace PlanetSimulator {
namespace Math {

/**
 * Canonical Cube-to-Sphere Mapping
 * 
 * This implementation provides the standard mapping from cube coordinates to sphere coordinates
 * used throughout the planet simulator. The mapping minimizes distortion while ensuring
 * continuous boundaries between cube faces.
 * 
 * Mathematical Formula:
 * Given a point (x, y, z) on the unit cube surface, the mapping to sphere is:
 * 
 * 1. Normalize to get direction: d = normalize(x, y, z)
 * 2. Scale by radius: p = d * radius
 * 
 * This simple normalization approach ensures:
 * - Continuous mapping across face boundaries
 * - Minimal angular distortion near cube vertices
 * - Exact vertex sharing at edges and corners
 * 
 * Face Convention:
 * - Face 0: +X (right)
 * - Face 1: -X (left)  
 * - Face 2: +Y (top)
 * - Face 3: -Y (bottom)
 * - Face 4: +Z (front)
 * - Face 5: -Z (back)
 * 
 * UV Convention:
 * - UV coordinates range from [0, 1] within each face
 * - (0, 0) is at the minimum corner of each face
 * - (1, 1) is at the maximum corner of each face
 */

// Epsilon for boundary snapping - ensures exact vertex sharing
constexpr float BOUNDARY_EPSILON_F = 1e-6f;
constexpr double BOUNDARY_EPSILON_D = 1e-12;

/**
 * Convert UV coordinates on a cube face to 3D position on unit cube
 * @param face Face index (0-5)
 * @param u U coordinate [0, 1]
 * @param v V coordinate [0, 1]
 * @return 3D position on unit cube surface
 */
template<typename T>
inline glm::vec<3, T> uvToUnitCube(int face, T u, T v) {
    // Snap to boundaries for exact vertex sharing
    const T epsilon = std::is_same<T, float>::value ? 
        static_cast<T>(BOUNDARY_EPSILON_F) : 
        static_cast<T>(BOUNDARY_EPSILON_D);
    
    if (u < epsilon) u = T(0);
    if (u > T(1) - epsilon) u = T(1);
    if (v < epsilon) v = T(0);
    if (v > T(1) - epsilon) v = T(1);
    
    // Map [0,1] to [-1,1]
    T x = T(2) * u - T(1);
    T y = T(2) * v - T(1);
    
    // Generate 3D position based on face
    // FIXED: Correct UV mapping for each face
    // u varies horizontally, v varies vertically in patch space
    switch(face) {
        case 0: return glm::vec<3, T>( T(1),  x,     y);     // +X: u->Y, v->Z
        case 1: return glm::vec<3, T>(-T(1), -x,     y);     // -X: u->-Y, v->Z
        case 2: return glm::vec<3, T>( x,     T(1),  y);     // +Y: u->X, v->Z
        case 3: return glm::vec<3, T>( x,    -T(1), -y);     // -Y: u->X, v->-Z
        case 4: return glm::vec<3, T>( x,     y,     T(1));  // +Z: u->X, v->Y
        case 5: return glm::vec<3, T>( x,     y,    -T(1));  // -Z: u->X, v->Y
        default: return glm::vec<3, T>(T(0), T(0), T(0));
    }
}

/**
 * Core cube-to-sphere mapping function
 * @param cubePos Position on unit cube surface
 * @param radius Sphere radius
 * @return Position on sphere surface
 */
template<typename T>
inline glm::vec<3, T> cubeToSphere(const glm::vec<3, T>& cubePos, T radius) {
    // Normalize the cube position to get sphere direction
    glm::vec<3, T> normalized = glm::normalize(cubePos);
    
    // Scale by radius
    return normalized * radius;
}

/**
 * Convert face UV to sphere position (convenience function)
 * @param face Face index (0-5)
 * @param u U coordinate [0, 1]
 * @param v V coordinate [0, 1]
 * @param radius Sphere radius
 * @return Position on sphere surface
 */
template<typename T>
inline glm::vec<3, T> faceUVToSphere(int face, T u, T v, T radius) {
    glm::vec<3, T> cubePos = uvToUnitCube(face, u, v);
    return cubeToSphere(cubePos, radius);
}

// Float precision versions
inline glm::vec3 cubeToSphereF(const glm::vec3& cubePos, float radius) {
    return cubeToSphere<float>(cubePos, radius);
}

inline glm::vec3 faceUVToSphereF(int face, float u, float v, float radius) {
    return faceUVToSphere<float>(face, u, v, radius);
}

// Double precision versions
inline glm::dvec3 cubeToSphereD(const glm::dvec3& cubePos, double radius) {
    return cubeToSphere<double>(cubePos, radius);
}

inline glm::dvec3 faceUVToSphereD(int face, double u, double v, double radius) {
    return faceUVToSphere<double>(face, u, v, radius);
}

/**
 * Optional vertex cache for performance optimization
 * Caches computed sphere positions for frequently accessed UV coordinates
 */
template<typename T>
class CubeSphereCache {
public:
    struct Key {
        int face;
        T u, v;
        
        bool operator==(const Key& other) const {
            const T epsilon = std::is_same<T, float>::value ? 
                static_cast<T>(BOUNDARY_EPSILON_F) : 
                static_cast<T>(BOUNDARY_EPSILON_D);
            return face == other.face &&
                   std::abs(u - other.u) < epsilon &&
                   std::abs(v - other.v) < epsilon;
        }
    };
    
    struct KeyHash {
        std::size_t operator()(const Key& k) const {
            // Quantize UV to grid for stable hashing
            const int gridSize = 10000;
            int uGrid = static_cast<int>(k.u * gridSize);
            int vGrid = static_cast<int>(k.v * gridSize);
            
            std::size_t h1 = std::hash<int>()(k.face);
            std::size_t h2 = std::hash<int>()(uGrid);
            std::size_t h3 = std::hash<int>()(vGrid);
            
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
    
    glm::vec<3, T> get(int face, T u, T v, T radius) {
        Key key{face, u, v};
        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }
        
        glm::vec<3, T> result = faceUVToSphere(face, u, v, radius);
        cache[key] = result;
        return result;
    }
    
    void clear() {
        cache.clear();
    }
    
    size_t size() const {
        return cache.size();
    }
    
private:
    std::unordered_map<Key, glm::vec<3, T>, KeyHash> cache;
};

// Utility functions for testing and validation

/**
 * Check if two sphere positions are approximately equal
 */
template<typename T>
inline bool spherePositionsEqual(const glm::vec<3, T>& a, const glm::vec<3, T>& b, T epsilon) {
    return glm::length(a - b) < epsilon;
}

/**
 * Compute the angular distortion at a given UV coordinate
 * Returns the ratio of actual angle to ideal angle (1.0 = no distortion)
 */
template<typename T>
inline T computeAngularDistortion(int face, T u, T v, T radius, T delta = T(0.01)) {
    // Get center point
    glm::vec<3, T> center = faceUVToSphere(face, u, v, radius);
    
    // Get neighboring points
    glm::vec<3, T> right = faceUVToSphere(face, u + delta, v, radius);
    glm::vec<3, T> up = faceUVToSphere(face, u, v + delta, radius);
    
    // Compute angles
    T angleRight = std::acos(glm::dot(center, right) / (radius * radius));
    T angleUp = std::acos(glm::dot(center, up) / (radius * radius));
    
    // Ideal angle (based on cube distance)
    T idealAngle = delta * T(2); // Approximate
    
    // Return average distortion ratio
    return (angleRight + angleUp) / (T(2) * idealAngle);
}

} // namespace Math
} // namespace PlanetSimulator