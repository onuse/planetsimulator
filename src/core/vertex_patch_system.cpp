#include "core/vertex_patch_system.hpp"
#include <iostream>
#include <unordered_set>

namespace PlanetRenderer {

// ============================================================================
// VertexPatchSystem Implementation
// ============================================================================

VertexPatchSystem::VertexPatchSystem() {
    // Initialize with Earth radius by default
    generator_.setPlanetRadius(6371000.0);
}

VertexIDPatch VertexPatchSystem::generatePatch(int faceId, glm::dvec2 center, 
                                               double size, int resolution) {
    VertexIDPatch patch;
    patch.faceId = faceId;
    patch.center = center;
    patch.size = size;
    patch.resolution = resolution;
    patch.level = 0;  // Can be set later based on LOD
    
    // Generate the vertex grid
    generateVertexGrid(patch);
    
    // Generate triangle indices
    generateTriangleIndices(patch);
    
    // Update statistics
    stats_.totalPatches++;
    
    return patch;
}

void VertexPatchSystem::generateVertexGrid(VertexIDPatch& patch) {
    const int gridSize = patch.resolution + 1;
    patch.vertexIDs.clear();
    patch.vertexIDs.reserve(gridSize * gridSize);
    
    // Generate vertex IDs for the grid
    for (int y = 0; y <= patch.resolution; y++) {
        for (int x = 0; x <= patch.resolution; x++) {
            // Calculate UV coordinates for this vertex
            double u = (patch.center.x - patch.size/2.0) + 
                      (x / double(patch.resolution)) * patch.size;
            double v = (patch.center.y - patch.size/2.0) + 
                      (y / double(patch.resolution)) * patch.size;
            
            // Clamp to valid range [0,1]
            u = std::max(0.0, std::min(1.0, u));
            v = std::max(0.0, std::min(1.0, v));
            
            // Create vertex ID from face UV
            VertexID vid = VertexID::fromFaceUV(patch.faceId, u, v);
            patch.vertexIDs.push_back(vid);
        }
    }
}

void VertexPatchSystem::generateTriangleIndices(VertexIDPatch& patch) {
    patch.indices.clear();
    const int gridSize = patch.resolution + 1;
    
    // Generate two triangles per quad
    for (int y = 0; y < patch.resolution; y++) {
        for (int x = 0; x < patch.resolution; x++) {
            int idx = y * gridSize + x;
            
            // First triangle (CCW winding)
            patch.indices.push_back(idx);
            patch.indices.push_back(idx + 1);
            patch.indices.push_back(idx + gridSize);
            
            // Second triangle
            patch.indices.push_back(idx + 1);
            patch.indices.push_back(idx + gridSize + 1);
            patch.indices.push_back(idx + gridSize);
        }
    }
}

VertexPatchSystem::RenderData VertexPatchSystem::convertToRenderData(const VertexIDPatch& patch) {
    RenderData data;
    
    // Reserve space
    data.positions.reserve(patch.vertexIDs.size());
    data.normals.reserve(patch.vertexIDs.size());
    data.texCoords.reserve(patch.vertexIDs.size());
    data.indices = patch.indices;  // Indices stay the same
    
    // Convert each vertex ID to actual vertex data
    for (const VertexID& vid : patch.vertexIDs) {
        CachedVertex vertex = generator_.getVertex(vid);
        data.positions.push_back(vertex.position);
        data.normals.push_back(vertex.normal);
        data.texCoords.push_back(vertex.texCoord);
    }
    
    return data;
}

void VertexPatchSystem::convertPatchesToGlobalBuffer(
    const std::vector<VertexIDPatch>& patches,
    std::vector<CachedVertex>& globalVertexBuffer,
    std::vector<uint32_t>& globalIndexBuffer) {
    
    // Clear buffers
    bufferManager_.clear();
    globalVertexBuffer.clear();
    globalIndexBuffer.clear();
    
    // Track unique vertices
    std::unordered_set<VertexID> uniqueVertices;
    
    // Process each patch
    for (const auto& patch : patches) {
        // Map local indices to global indices
        std::vector<uint32_t> localToGlobal;
        localToGlobal.reserve(patch.vertexIDs.size());
        
        for (const VertexID& vid : patch.vertexIDs) {
            uint32_t globalIdx = bufferManager_.getOrCreateIndex(vid, generator_);
            localToGlobal.push_back(globalIdx);
            uniqueVertices.insert(vid);
        }
        
        // Add indices to global index buffer
        for (uint32_t localIdx : patch.indices) {
            globalIndexBuffer.push_back(localToGlobal[localIdx]);
        }
    }
    
    // Copy vertex buffer
    globalVertexBuffer = bufferManager_.getBuffer();
    
    // Update statistics
    stats_.totalVertices = patches.size() * (patches[0].resolution + 1) * (patches[0].resolution + 1);
    stats_.sharedVertices = stats_.totalVertices - uniqueVertices.size();
    stats_.sharingRatio = float(stats_.sharedVertices) / float(stats_.totalVertices);
    
    std::cout << "Vertex sharing statistics:\n";
    std::cout << "  Total vertices (without sharing): " << stats_.totalVertices << "\n";
    std::cout << "  Unique vertices (with sharing): " << uniqueVertices.size() << "\n";
    std::cout << "  Shared vertices: " << stats_.sharedVertices << "\n";
    std::cout << "  Sharing ratio: " << (stats_.sharingRatio * 100.0f) << "%\n";
}

// ============================================================================
// QuadtreePatchAdapter Implementation
// ============================================================================

QuadtreePatchAdapter::QuadtreePatchAdapter() {
}

VertexIDPatch QuadtreePatchAdapter::convertFromSimplePatch(const SimplePatch& oldPatch) {
    // Calculate UV bounds from the simple patch
    glm::dvec2 center(0.5, 0.5);  // Default to face center
    double size = 1.0 / (1 << oldPatch.level);  // Size based on LOD level
    
    VertexIDPatch newPatch = patchSystem_.generatePatch(
        oldPatch.faceId, center, size, 32);  // 32x32 resolution
    
    newPatch.level = oldPatch.level;
    newPatch.isVisible = oldPatch.isVisible;
    
    return newPatch;
}

std::vector<VertexIDPatch> QuadtreePatchAdapter::generateTestPatches(int numPatches) {
    std::vector<VertexIDPatch> patches;
    
    // Generate test patches on different faces
    for (int i = 0; i < numPatches; i++) {
        int face = i % 6;
        double u = 0.5 + 0.25 * (i % 2);
        double v = 0.5 + 0.25 * (i / 2 % 2);
        
        patches.push_back(patchSystem_.generatePatch(face, glm::dvec2(u, v), 0.5, 16));
    }
    
    return patches;
}

glm::dvec3 QuadtreePatchAdapter::faceUVToCube(int face, double u, double v) {
    // Map UV [0,1] to [-1,1]
    double s = (u - 0.5) * 2.0;
    double t = (v - 0.5) * 2.0;
    
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

} // namespace PlanetRenderer