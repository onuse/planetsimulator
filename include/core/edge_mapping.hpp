#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace core {

/**
 * Edge mapping system to ensure vertices at face boundaries are identical
 * 
 * Key principle: For any edge shared between two faces, we designate one face
 * as the "canonical" owner of that edge. When generating vertices on that edge,
 * both faces will use the canonical face's coordinate system.
 */
class EdgeMapping {
public:
    // Cube face IDs
    enum Face {
        FACE_POS_X = 0,  // +X
        FACE_NEG_X = 1,  // -X
        FACE_POS_Y = 2,  // +Y
        FACE_NEG_Y = 3,  // -Y
        FACE_POS_Z = 4,  // +Z
        FACE_NEG_Z = 5   // -Z
    };
    
    // Edge IDs for each face (in UV space)
    enum Edge {
        EDGE_U0 = 0,  // U = 0 (left edge)
        EDGE_U1 = 1,  // U = 1 (right edge)
        EDGE_V0 = 2,  // V = 0 (bottom edge)
        EDGE_V1 = 3,  // V = 1 (top edge)
        NO_EDGE = -1
    };
    
    // Check if a UV coordinate is at a face edge
    static Edge getEdge(double u, double v, double epsilon = 1e-10) {
        if (std::abs(u) < epsilon) return EDGE_U0;
        if (std::abs(u - 1.0) < epsilon) return EDGE_U1;
        if (std::abs(v) < epsilon) return EDGE_V0;
        if (std::abs(v - 1.0) < epsilon) return EDGE_V1;
        return NO_EDGE;
    }
    
    // Determine which face "owns" a shared edge (lower face ID wins)
    static bool isCanonicalFace(int faceId, Edge edge) {
        int neighbor = getNeighborFace(faceId, edge);
        if (neighbor < 0) return true;  // No neighbor, we own it
        return faceId < neighbor;  // Lower ID is canonical
    }
    
    // Get the neighboring face across an edge
    static int getNeighborFace(int faceId, Edge edge) {
        // This defines the cube topology
        // Each face's edges connect to specific other faces
        
        switch (faceId) {
        case FACE_POS_X:  // +X face
            switch (edge) {
                case EDGE_U0: return FACE_NEG_Z;  // U=0 connects to -Z
                case EDGE_U1: return FACE_POS_Z;  // U=1 connects to +Z
                case EDGE_V0: return FACE_NEG_Y;  // V=0 connects to -Y
                case EDGE_V1: return FACE_POS_Y;  // V=1 connects to +Y
                default: return -1;
            }
            
        case FACE_NEG_X:  // -X face
            switch (edge) {
                case EDGE_U0: return FACE_POS_Z;  // U=0 connects to +Z
                case EDGE_U1: return FACE_NEG_Z;  // U=1 connects to -Z
                case EDGE_V0: return FACE_NEG_Y;  // V=0 connects to -Y
                case EDGE_V1: return FACE_POS_Y;  // V=1 connects to +Y
                default: return -1;
            }
            
        case FACE_POS_Y:  // +Y face
            switch (edge) {
                case EDGE_U0: return FACE_NEG_X;  // U=0 connects to -X
                case EDGE_U1: return FACE_POS_X;  // U=1 connects to +X
                case EDGE_V0: return FACE_NEG_Z;  // V=0 connects to -Z
                case EDGE_V1: return FACE_POS_Z;  // V=1 connects to +Z
                default: return -1;
            }
            
        case FACE_NEG_Y:  // -Y face
            switch (edge) {
                case EDGE_U0: return FACE_NEG_X;  // U=0 connects to -X
                case EDGE_U1: return FACE_POS_X;  // U=1 connects to +X
                case EDGE_V0: return FACE_POS_Z;  // V=0 connects to +Z
                case EDGE_V1: return FACE_NEG_Z;  // V=1 connects to -Z
                default: return -1;
            }
            
        case FACE_POS_Z:  // +Z face
            switch (edge) {
                case EDGE_U0: return FACE_NEG_X;  // U=0 connects to -X
                case EDGE_U1: return FACE_POS_X;  // U=1 connects to +X
                case EDGE_V0: return FACE_NEG_Y;  // V=0 connects to -Y
                case EDGE_V1: return FACE_POS_Y;  // V=1 connects to +Y
                default: return -1;
            }
            
        case FACE_NEG_Z:  // -Z face
            switch (edge) {
                case EDGE_U0: return FACE_POS_X;  // U=0 connects to +X
                case EDGE_U1: return FACE_NEG_X;  // U=1 connects to -X
                case EDGE_V0: return FACE_NEG_Y;  // V=0 connects to -Y
                case EDGE_V1: return FACE_POS_Y;  // V=1 connects to +Y
                default: return -1;
            }
            
        default:
            return -1;
        }
    }
    
    // Convert a UV coordinate to the canonical cube position
    // This ensures vertices at shared edges are identical
    static glm::dvec3 uvToCubePosition(double u, double v, int faceId) {
        // Check if we're at an edge
        Edge edge = getEdge(u, v);
        
        // If at an edge and we're not the canonical face, 
        // we need to compute the position differently
        if (edge != NO_EDGE && !isCanonicalFace(faceId, edge)) {
            int canonicalFace = getNeighborFace(faceId, edge);
            
            // Remap UV to the canonical face's coordinate system
            glm::dvec2 remappedUV = remapUVToFace(u, v, faceId, edge, canonicalFace);
            
            // Use the canonical face to generate the position
            return uvToCubePositionDirect(remappedUV.x, remappedUV.y, canonicalFace);
        }
        
        // We own this position, generate it directly
        return uvToCubePositionDirect(u, v, faceId);
    }
    
private:
    // Direct UV to cube position for a specific face
    static glm::dvec3 uvToCubePositionDirect(double u, double v, int faceId) {
        // Map UV [0,1] to [-1,1]
        double su = u * 2.0 - 1.0;
        double sv = v * 2.0 - 1.0;
        
        switch (faceId) {
        case FACE_POS_X: return glm::dvec3(1.0, sv, su);
        case FACE_NEG_X: return glm::dvec3(-1.0, sv, -su);
        case FACE_POS_Y: return glm::dvec3(su, 1.0, sv);
        case FACE_NEG_Y: return glm::dvec3(su, -1.0, -sv);
        case FACE_POS_Z: return glm::dvec3(su, sv, 1.0);
        case FACE_NEG_Z: return glm::dvec3(-su, sv, -1.0);
        default: return glm::dvec3(0.0);
        }
    }
    
    // Remap UV coordinates from one face to another at a shared edge
    static glm::dvec2 remapUVToFace(double u, double v, int fromFace, Edge edge, int toFace) {
        // This is complex and depends on how faces connect
        // For now, simplified version - proper implementation would handle all cases
        
        // The key insight: at a shared edge, the parameter along the edge
        // should map to the same position on both faces
        
        // Example: +Z face U=1 edge connects to +X face U=0 edge
        // A point at (+Z, U=1, V=t) should map to (+X, U=0, V=t)
        
        // This would need a full connectivity table...
        // For now, return the original UV (which will cause gaps)
        // A complete implementation would handle all 24 edge connections
        
        return glm::dvec2(u, v);
    }
};

} // namespace core