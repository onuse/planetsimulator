#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <functional>
#include <iostream>

// ============================================================================
// Vertex Identity System
// 
// This system ensures that vertices at the same 3D position on the planet
// receive the same ID, regardless of which patch or face requests them.
// This is the foundation for eliminating gaps at face boundaries.
// ============================================================================

namespace PlanetRenderer {

// ============================================================================
// VertexID - Unique identifier for any vertex on the planet
// ============================================================================
class VertexID {
public:
    // Default constructor
    VertexID() : id_(0) {}
    
    // Create from raw ID (for deserialization)
    explicit VertexID(uint64_t id) : id_(id) {}
    
    // Create from cube-space position (primary constructor)
    static VertexID fromCubePosition(const glm::dvec3& cubePos) {
        // Quantize position to avoid floating point issues
        // Using 10000 gives us 0.1mm precision at planet scale
        const double QUANTIZATION = 10000.0;
        
        int32_t qx = static_cast<int32_t>(std::round(cubePos.x * QUANTIZATION));
        int32_t qy = static_cast<int32_t>(std::round(cubePos.y * QUANTIZATION));
        int32_t qz = static_cast<int32_t>(std::round(cubePos.z * QUANTIZATION));
        
        // Pack into 64-bit ID
        // Layout: [unused:4][x:20][y:20][z:20]
        uint64_t id = 0;
        id |= (static_cast<uint64_t>(qx + 0x80000) & 0xFFFFF) << 40;  // 20 bits for X
        id |= (static_cast<uint64_t>(qy + 0x80000) & 0xFFFFF) << 20;  // 20 bits for Y
        id |= (static_cast<uint64_t>(qz + 0x80000) & 0xFFFFF);         // 20 bits for Z
        
        return VertexID(id);
    }
    
    // Create from face/UV (convenience, converts to cube position first)
    static VertexID fromFaceUV(int face, double u, double v, double size = 2.0) {
        glm::dvec3 cubePos = faceUVToCube(face, u, v, size);
        return fromCubePosition(cubePos);
    }
    
    // Decode back to cube position (for debugging)
    glm::dvec3 toCubePosition() const {
        const double INV_QUANTIZATION = 1.0 / 10000.0;
        
        int32_t qx = ((id_ >> 40) & 0xFFFFF) - 0x80000;
        int32_t qy = ((id_ >> 20) & 0xFFFFF) - 0x80000;
        int32_t qz = (id_ & 0xFFFFF) - 0x80000;
        
        return glm::dvec3(
            qx * INV_QUANTIZATION,
            qy * INV_QUANTIZATION,
            qz * INV_QUANTIZATION
        );
    }
    
    // Get the raw ID
    uint64_t raw() const { return id_; }
    
    // Comparison operators
    bool operator==(const VertexID& other) const { return id_ == other.id_; }
    bool operator!=(const VertexID& other) const { return id_ != other.id_; }
    bool operator<(const VertexID& other) const { return id_ < other.id_; }
    
    // Check if this vertex is on a face boundary
    bool isOnFaceBoundary() const {
        glm::dvec3 pos = toCubePosition();
        const double BOUNDARY = 1.0;
        const double EPSILON = 1e-4;
        
        return (std::abs(std::abs(pos.x) - BOUNDARY) < EPSILON ||
                std::abs(std::abs(pos.y) - BOUNDARY) < EPSILON ||
                std::abs(std::abs(pos.z) - BOUNDARY) < EPSILON);
    }
    
    // Check if this vertex is on an edge (2 face boundaries)
    bool isOnEdge() const {
        glm::dvec3 pos = toCubePosition();
        const double BOUNDARY = 1.0;
        const double EPSILON = 1e-4;
        
        int boundaryCount = 0;
        if (std::abs(std::abs(pos.x) - BOUNDARY) < EPSILON) boundaryCount++;
        if (std::abs(std::abs(pos.y) - BOUNDARY) < EPSILON) boundaryCount++;
        if (std::abs(std::abs(pos.z) - BOUNDARY) < EPSILON) boundaryCount++;
        
        return boundaryCount >= 2;
    }
    
    // Check if this vertex is on a corner (3 face boundaries)
    bool isOnCorner() const {
        glm::dvec3 pos = toCubePosition();
        const double BOUNDARY = 1.0;
        const double EPSILON = 1e-4;
        
        return (std::abs(std::abs(pos.x) - BOUNDARY) < EPSILON &&
                std::abs(std::abs(pos.y) - BOUNDARY) < EPSILON &&
                std::abs(std::abs(pos.z) - BOUNDARY) < EPSILON);
    }
    
    // Stream operator for debugging
    friend std::ostream& operator<<(std::ostream& os, const VertexID& id) {
        return os << "VertexID(" << std::hex << id.id_ << std::dec << ")";
    }
    
private:
    uint64_t id_;
    
    // Helper: Convert face/UV to cube position
    static glm::dvec3 faceUVToCube(int face, double u, double v, double size) {
        // Map UV [0,1] to [-1,1]
        double s = (u - 0.5) * size;
        double t = (v - 0.5) * size;
        
        switch(face) {
            case 0: return glm::dvec3( 1.0,    t,    s);  // +X
            case 1: return glm::dvec3(-1.0,    t,   -s);  // -X
            case 2: return glm::dvec3(   s,  1.0,    t);  // +Y
            case 3: return glm::dvec3(   s, -1.0,   -t);  // -Y
            case 4: return glm::dvec3(   s,    t,  1.0);  // +Z
            case 5: return glm::dvec3(  -s,    t, -1.0);  // -Z
            default: return glm::dvec3(0.0);
        }
    }
};

// ============================================================================
// EdgeID - Identifies an edge between two vertices
// ============================================================================
class EdgeID {
public:
    EdgeID(VertexID v1, VertexID v2) {
        // Always store in sorted order for consistency
        if (v1 < v2) {
            vertex1_ = v1;
            vertex2_ = v2;
        } else {
            vertex1_ = v2;
            vertex2_ = v1;
        }
    }
    
    VertexID vertex1() const { return vertex1_; }
    VertexID vertex2() const { return vertex2_; }
    
    bool operator==(const EdgeID& other) const {
        return vertex1_ == other.vertex1_ && vertex2_ == other.vertex2_;
    }
    
    bool operator!=(const EdgeID& other) const {
        return !(*this == other);
    }
    
    bool operator<(const EdgeID& other) const {
        if (vertex1_ != other.vertex1_) return vertex1_ < other.vertex1_;
        return vertex2_ < other.vertex2_;
    }
    
    // Stream operator for debugging
    friend std::ostream& operator<<(std::ostream& os, const EdgeID& id) {
        return os << "EdgeID(" << id.vertex1_ << ", " << id.vertex2_ << ")";
    }
    
private:
    VertexID vertex1_, vertex2_;
};

} // namespace PlanetRenderer

// ============================================================================
// Hash specializations for use in unordered containers
// ============================================================================
namespace std {
    template<>
    struct hash<PlanetRenderer::VertexID> {
        size_t operator()(const PlanetRenderer::VertexID& id) const {
            return std::hash<uint64_t>{}(id.raw());
        }
    };
    
    template<>
    struct hash<PlanetRenderer::EdgeID> {
        size_t operator()(const PlanetRenderer::EdgeID& id) const {
            size_t h1 = std::hash<PlanetRenderer::VertexID>{}(id.vertex1());
            size_t h2 = std::hash<PlanetRenderer::VertexID>{}(id.vertex2());
            return h1 ^ (h2 << 1);  // Combine hashes
        }
    };
}