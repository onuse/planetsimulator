#define GLM_ENABLE_EXPERIMENTAL
#include "rendering/cpu_vertex_generator.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rendering {

CPUVertexGenerator::CPUVertexGenerator(const Config& config) 
    : config(config) {
}

CPUVertexGenerator::PatchMesh CPUVertexGenerator::generatePatchMesh(
    const core::QuadtreePatch& patch, const glm::dmat4& patchTransform) {
    
    PatchMesh mesh;
    const uint32_t res = config.gridResolution;
    
    // Calculate edge resolution multipliers based on neighbor levels
    // When a neighbor has a higher level, we need more vertices on that edge
    uint32_t topEdgeMultiplier = 1;
    uint32_t rightEdgeMultiplier = 1;
    uint32_t bottomEdgeMultiplier = 1;
    uint32_t leftEdgeMultiplier = 1;
    
    // Debug: Track T-junction handling
    static int tjunctionDebugCount = 0;
    bool hasTJunction = false;
    
    if (patch.neighborLevels[0] > patch.level) {
        topEdgeMultiplier = 1 << (patch.neighborLevels[0] - patch.level);
        hasTJunction = true;
    }
    if (patch.neighborLevels[1] > patch.level) {
        rightEdgeMultiplier = 1 << (patch.neighborLevels[1] - patch.level);
        hasTJunction = true;
    }
    if (patch.neighborLevels[2] > patch.level) {
        bottomEdgeMultiplier = 1 << (patch.neighborLevels[2] - patch.level);
        hasTJunction = true;
    }
    if (patch.neighborLevels[3] > patch.level) {
        leftEdgeMultiplier = 1 << (patch.neighborLevels[3] - patch.level);
        hasTJunction = true;
    }
    
    // T-junction debug disabled for testing
    // if (hasTJunction && tjunctionDebugCount++ < 10) {
    //     std::cout << "[T-JUNCTION] ..." << std::endl;
    // }
    
    // Reserve space for vertices and indices (accounting for extra edge vertices)
    const uint32_t mainVertexCount = res * res;
    const uint32_t extraEdgeVertices = 
        (res - 1) * (topEdgeMultiplier - 1) +
        (res - 1) * (rightEdgeMultiplier - 1) +
        (res - 1) * (bottomEdgeMultiplier - 1) +
        (res - 1) * (leftEdgeMultiplier - 1);
    const uint32_t skirtVertexCount = config.enableSkirts ? (res * 4) : 0;
    const uint32_t totalVertexCount = mainVertexCount + extraEdgeVertices + skirtVertexCount;
    
    mesh.vertices.reserve(totalVertexCount);
    mesh.mainVertexStart = 0;
    mesh.mainVertexCount = mainVertexCount + extraEdgeVertices;
    mesh.skirtVertexStart = mainVertexCount + extraEdgeVertices;
    mesh.skirtVertexCount = skirtVertexCount;
    
    // Generate main patch vertices
    for (uint32_t y = 0; y < res; y++) {
        for (uint32_t x = 0; x < res; x++) {
            // UV coordinates in patch space (0 to 1)
            double u = static_cast<double>(x) / (res - 1);
            double v = static_cast<double>(y) / (res - 1);
            
            // Transform to cube face position
            glm::dvec4 localPos(u, v, 0.0, 1.0);
            glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
            
            // Safety check: if we get exactly (0,0,0), something is wrong
            // This shouldn't happen with proper patches, but add fallback
            if (cubePos.x == 0.0 && cubePos.y == 0.0 && cubePos.z == 0.0) {
                static int zeroWarningCount = 0;
                if (zeroWarningCount++ < 5) {
                    std::cout << "WARNING: Transform produced (0,0,0) at UV(" << u << "," << v << ")" << std::endl;
                    std::cout << "  Patch level=" << patch.level << " face=" << patch.faceId << std::endl;
                }
                // Use patch center as fallback to avoid NaN
                cubePos = patch.center;
            }
            
            // DO NOT SNAP TO ±1.0! We're using INSET (0.9995) to prevent z-fighting.
            // The patch bounds already have INSET applied, and snapping to ±1.0 here
            // would override that fix and cause faces to overlap again.
            
            // REMOVED: The std::round() logic was WRONG!
            // It was rounding coordinates to 0 or ±1, destroying vertex positions
            // The first snapping pass with EPSILON = 1e-8 is sufficient
            
            // Check cache for existing vertex
            if (config.enableVertexCaching) {
                // Don't include faceId in the key for boundary vertices!
                // This allows vertices to be shared across faces
                VertexKey key = createVertexKey(cubePos, 0);  // Use 0 for all faces to enable sharing
                auto it = vertexCache.find(key);
                
                if (it != vertexCache.end()) {
                    // Cache hit - but we need to use the correct faceId for this face!
                    PatchVertex vertex = it->second;
                    vertex.faceId = patch.faceId;  // Override with the requesting face's ID
                    mesh.vertices.push_back(vertex);
                    stats.cacheHits++;
                } else {
                    // Cache miss - generate new vertex
                    PatchVertex vertex = generateVertex(cubePos, false, patch.faceId);
                    mesh.vertices.push_back(vertex);
                    
                    // Add to cache
                    if (vertexCache.size() < config.maxCacheSize) {
                        vertexCache[key] = vertex;
                    }
                    stats.cacheMisses++;
                }
            } else {
                // No caching - generate vertex directly
                mesh.vertices.push_back(generateVertex(cubePos, false, patch.faceId));
            }
        }
    }
    
    // Generate extra edge vertices for T-junction resolution
    // These are additional vertices placed between regular grid vertices on edges
    // where we have a neighbor with a higher subdivision level
    
    // Top edge (v=1, u varies)
    if (topEdgeMultiplier > 1) {
        uint32_t y = res - 1;  // Top row
        for (uint32_t x = 0; x < res - 1; x++) {
            // Generate extra vertices between x and x+1
            for (uint32_t sub = 1; sub < topEdgeMultiplier; sub++) {
                double u = (x + static_cast<double>(sub) / topEdgeMultiplier) / (res - 1);
                double v = 1.0;
                
                glm::dvec4 localPos(u, v, 0.0, 1.0);
                glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
                
                if (cubePos.x == 0.0 && cubePos.y == 0.0 && cubePos.z == 0.0) {
                    cubePos = patch.center;
                }
                
                if (config.enableVertexCaching) {
                    VertexKey key = createVertexKey(cubePos, 0);
                    auto it = vertexCache.find(key);
                    
                    if (it != vertexCache.end()) {
                        PatchVertex vertex = it->second;
                        vertex.faceId = patch.faceId;
                        mesh.vertices.push_back(vertex);
                        stats.cacheHits++;
                    } else {
                        PatchVertex vertex = generateVertex(cubePos, false, patch.faceId);
                        mesh.vertices.push_back(vertex);
                        if (vertexCache.size() < config.maxCacheSize) {
                            vertexCache[key] = vertex;
                        }
                        stats.cacheMisses++;
                    }
                } else {
                    mesh.vertices.push_back(generateVertex(cubePos, false, patch.faceId));
                }
            }
        }
    }
    
    // Right edge (u=1, v varies)
    if (rightEdgeMultiplier > 1) {
        uint32_t x = res - 1;  // Right column
        for (uint32_t y = 0; y < res - 1; y++) {
            // Generate extra vertices between y and y+1
            for (uint32_t sub = 1; sub < rightEdgeMultiplier; sub++) {
                double u = 1.0;
                double v = (y + static_cast<double>(sub) / rightEdgeMultiplier) / (res - 1);
                
                glm::dvec4 localPos(u, v, 0.0, 1.0);
                glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
                
                if (cubePos.x == 0.0 && cubePos.y == 0.0 && cubePos.z == 0.0) {
                    cubePos = patch.center;
                }
                
                if (config.enableVertexCaching) {
                    VertexKey key = createVertexKey(cubePos, 0);
                    auto it = vertexCache.find(key);
                    
                    if (it != vertexCache.end()) {
                        PatchVertex vertex = it->second;
                        vertex.faceId = patch.faceId;
                        mesh.vertices.push_back(vertex);
                        stats.cacheHits++;
                    } else {
                        PatchVertex vertex = generateVertex(cubePos, false, patch.faceId);
                        mesh.vertices.push_back(vertex);
                        if (vertexCache.size() < config.maxCacheSize) {
                            vertexCache[key] = vertex;
                        }
                        stats.cacheMisses++;
                    }
                } else {
                    mesh.vertices.push_back(generateVertex(cubePos, false, patch.faceId));
                }
            }
        }
    }
    
    // Bottom edge (v=0, u varies)
    if (bottomEdgeMultiplier > 1) {
        uint32_t y = 0;  // Bottom row
        for (uint32_t x = 0; x < res - 1; x++) {
            // Generate extra vertices between x and x+1
            for (uint32_t sub = 1; sub < bottomEdgeMultiplier; sub++) {
                double u = (x + static_cast<double>(sub) / bottomEdgeMultiplier) / (res - 1);
                double v = 0.0;
                
                glm::dvec4 localPos(u, v, 0.0, 1.0);
                glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
                
                if (cubePos.x == 0.0 && cubePos.y == 0.0 && cubePos.z == 0.0) {
                    cubePos = patch.center;
                }
                
                if (config.enableVertexCaching) {
                    VertexKey key = createVertexKey(cubePos, 0);
                    auto it = vertexCache.find(key);
                    
                    if (it != vertexCache.end()) {
                        PatchVertex vertex = it->second;
                        vertex.faceId = patch.faceId;
                        mesh.vertices.push_back(vertex);
                        stats.cacheHits++;
                    } else {
                        PatchVertex vertex = generateVertex(cubePos, false, patch.faceId);
                        mesh.vertices.push_back(vertex);
                        if (vertexCache.size() < config.maxCacheSize) {
                            vertexCache[key] = vertex;
                        }
                        stats.cacheMisses++;
                    }
                } else {
                    mesh.vertices.push_back(generateVertex(cubePos, false, patch.faceId));
                }
            }
        }
    }
    
    // Left edge (u=0, v varies)
    if (leftEdgeMultiplier > 1) {
        uint32_t x = 0;  // Left column
        for (uint32_t y = 0; y < res - 1; y++) {
            // Generate extra vertices between y and y+1
            for (uint32_t sub = 1; sub < leftEdgeMultiplier; sub++) {
                double u = 0.0;
                double v = (y + static_cast<double>(sub) / leftEdgeMultiplier) / (res - 1);
                
                glm::dvec4 localPos(u, v, 0.0, 1.0);
                glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
                
                if (cubePos.x == 0.0 && cubePos.y == 0.0 && cubePos.z == 0.0) {
                    cubePos = patch.center;
                }
                
                if (config.enableVertexCaching) {
                    VertexKey key = createVertexKey(cubePos, 0);
                    auto it = vertexCache.find(key);
                    
                    if (it != vertexCache.end()) {
                        PatchVertex vertex = it->second;
                        vertex.faceId = patch.faceId;
                        mesh.vertices.push_back(vertex);
                        stats.cacheHits++;
                    } else {
                        PatchVertex vertex = generateVertex(cubePos, false, patch.faceId);
                        mesh.vertices.push_back(vertex);
                        if (vertexCache.size() < config.maxCacheSize) {
                            vertexCache[key] = vertex;
                        }
                        stats.cacheMisses++;
                    }
                } else {
                    mesh.vertices.push_back(generateVertex(cubePos, false, patch.faceId));
                }
            }
        }
    }
    
    // Generate skirt vertices if enabled
    if (config.enableSkirts) {
        const float skirtOffset = -0.05f;
        
        // Top edge skirt
        for (uint32_t x = 0; x < res; x++) {
            double u = static_cast<double>(x) / (res - 1);
            double v = skirtOffset;
            
            glm::dvec4 localPos(u, v, 0.0, 1.0);
            glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
            mesh.vertices.push_back(generateVertex(cubePos, true, patch.faceId));
        }
        
        // Bottom edge skirt
        for (uint32_t x = 0; x < res; x++) {
            double u = static_cast<double>(x) / (res - 1);
            double v = 1.0 - skirtOffset;
            
            glm::dvec4 localPos(u, v, 0.0, 1.0);
            glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
            mesh.vertices.push_back(generateVertex(cubePos, true, patch.faceId));
        }
        
        // Left edge skirt
        for (uint32_t y = 0; y < res; y++) {
            double u = skirtOffset;
            double v = static_cast<double>(y) / (res - 1);
            
            glm::dvec4 localPos(u, v, 0.0, 1.0);
            glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
            mesh.vertices.push_back(generateVertex(cubePos, true, patch.faceId));
        }
        
        // Right edge skirt
        for (uint32_t y = 0; y < res; y++) {
            double u = 1.0 - skirtOffset;
            double v = static_cast<double>(y) / (res - 1);
            
            glm::dvec4 localPos(u, v, 0.0, 1.0);
            glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
            mesh.vertices.push_back(generateVertex(cubePos, true, patch.faceId));
        }
    }
    
    // Generate indices for main patch with T-junction support
    const uint32_t mainIndexCount = (res - 1) * (res - 1) * 6;
    const uint32_t skirtIndexCount = config.enableSkirts ? ((res - 1) * 4 * 6) : 0;
    // Reserve extra space for T-junction triangles
    mesh.indices.reserve(mainIndexCount * 2 + skirtIndexCount);
    
    // Track where extra vertices are located  
    uint32_t nextExtraVertex = res * res;  // Extra vertices start after main grid
    
    // Calculate starting indices for each edge's extra vertices
    uint32_t topEdgeExtraStart = nextExtraVertex;
    uint32_t rightEdgeExtraStart = topEdgeExtraStart + ((res - 1) * (topEdgeMultiplier - 1));
    uint32_t bottomEdgeExtraStart = rightEdgeExtraStart + ((res - 1) * (rightEdgeMultiplier - 1));
    uint32_t leftEdgeExtraStart = bottomEdgeExtraStart + ((res - 1) * (bottomEdgeMultiplier - 1));
    
    // Generate main patch triangles with T-junction handling
    for (uint32_t y = 0; y < res - 1; y++) {
        for (uint32_t x = 0; x < res - 1; x++) {
            // Get the four corner vertices of this quad
            uint32_t topLeft = y * res + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * res + x;
            uint32_t bottomRight = bottomLeft + 1;
            
            // Check if we're on an edge that needs T-junction handling
            bool onTopEdge = (y == res - 2);
            bool onBottomEdge = (y == 0);
            bool onLeftEdge = (x == 0);
            bool onRightEdge = (x == res - 2);
            
            // Handle corner cases first (where two edges meet)
            if ((onTopEdge && onLeftEdge) && (topEdgeMultiplier > 1 || leftEdgeMultiplier > 1)) {
                // Top-left corner with T-junctions
                // This needs special triangulation to connect both edge's extra vertices
                // For simplicity, we'll use a fan pattern from the bottom-right vertex
                
                // Handle the quad with special corner logic
                // This is complex - for now use standard triangulation
                // TODO: Implement proper corner T-junction handling
                mesh.indices.push_back(topLeft);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(topRight);
                
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(bottomRight);
            }
            else if ((onTopEdge && onRightEdge) && (topEdgeMultiplier > 1 || rightEdgeMultiplier > 1)) {
                // Top-right corner
                mesh.indices.push_back(topLeft);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(topRight);
                
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(bottomRight);
            }
            else if ((onBottomEdge && onLeftEdge) && (bottomEdgeMultiplier > 1 || leftEdgeMultiplier > 1)) {
                // Bottom-left corner
                mesh.indices.push_back(topLeft);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(topRight);
                
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(bottomRight);
            }
            else if ((onBottomEdge && onRightEdge) && (bottomEdgeMultiplier > 1 || rightEdgeMultiplier > 1)) {
                // Bottom-right corner
                mesh.indices.push_back(topLeft);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(topRight);
                
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(bottomRight);
            }
            // Handle single edge cases
            else if (onTopEdge && topEdgeMultiplier > 1) {
                // Special handling for top edge with T-junction
                // We have extra vertices between topLeft and topRight
                // Create triangles connecting to these intermediate vertices
                
                // Calculate how many extra vertices are between topLeft and topRight
                uint32_t numExtras = topEdgeMultiplier - 1;
                
                // Start with the leftmost vertex
                uint32_t currentTop = topLeft;
                
                // Create triangles for each subdivision
                for (uint32_t i = 0; i < topEdgeMultiplier; i++) {
                    uint32_t nextTop;
                    
                    if (i == topEdgeMultiplier - 1) {
                        // Last subdivision connects to topRight
                        nextTop = topRight;
                    } else {
                        // Use the next extra vertex
                        // Extra vertices for this edge segment start at:
                        // topEdgeExtraStart + (x * numExtras) + i
                        nextTop = topEdgeExtraStart + (x * numExtras) + i;
                    }
                    
                    // Create triangle(s) for this subdivision
                    if (i == 0) {
                        // First triangle connects to bottomLeft
                        mesh.indices.push_back(currentTop);
                        mesh.indices.push_back(bottomLeft);
                        mesh.indices.push_back(nextTop);
                    } else if (i == topEdgeMultiplier - 1) {
                        // Last triangle connects to bottomRight
                        mesh.indices.push_back(currentTop);
                        mesh.indices.push_back(bottomLeft);
                        mesh.indices.push_back(bottomRight);
                        
                        mesh.indices.push_back(currentTop);
                        mesh.indices.push_back(bottomRight);
                        mesh.indices.push_back(nextTop);
                    } else {
                        // Middle triangles fan from bottomLeft/bottomRight
                        mesh.indices.push_back(currentTop);
                        mesh.indices.push_back(bottomLeft);
                        mesh.indices.push_back(nextTop);
                    }
                    
                    currentTop = nextTop;
                }
            }
            else if (onBottomEdge && bottomEdgeMultiplier > 1) {
                // Special handling for bottom edge with T-junction
                // Extra vertices between bottomLeft and bottomRight
                uint32_t numExtras = bottomEdgeMultiplier - 1;
                uint32_t currentBottom = bottomLeft;
                
                for (uint32_t i = 0; i < bottomEdgeMultiplier; i++) {
                    uint32_t nextBottom;
                    
                    if (i == bottomEdgeMultiplier - 1) {
                        nextBottom = bottomRight;
                    } else {
                        nextBottom = bottomEdgeExtraStart + (x * numExtras) + i;
                    }
                    
                    if (i == 0) {
                        mesh.indices.push_back(topLeft);
                        mesh.indices.push_back(currentBottom);
                        mesh.indices.push_back(nextBottom);
                    } else if (i == bottomEdgeMultiplier - 1) {
                        mesh.indices.push_back(topLeft);
                        mesh.indices.push_back(currentBottom);
                        mesh.indices.push_back(topRight);
                        
                        mesh.indices.push_back(topRight);
                        mesh.indices.push_back(currentBottom);
                        mesh.indices.push_back(nextBottom);
                    } else {
                        mesh.indices.push_back(topLeft);
                        mesh.indices.push_back(currentBottom);
                        mesh.indices.push_back(nextBottom);
                    }
                    
                    currentBottom = nextBottom;
                }
            }
            else if (onLeftEdge && leftEdgeMultiplier > 1) {
                // Special handling for left edge with T-junction
                // Extra vertices between topLeft and bottomLeft
                uint32_t numExtras = leftEdgeMultiplier - 1;
                uint32_t currentLeft = topLeft;
                
                for (uint32_t i = 0; i < leftEdgeMultiplier; i++) {
                    uint32_t nextLeft;
                    
                    if (i == leftEdgeMultiplier - 1) {
                        nextLeft = bottomLeft;
                    } else {
                        nextLeft = leftEdgeExtraStart + (y * numExtras) + i;
                    }
                    
                    if (i == 0) {
                        mesh.indices.push_back(currentLeft);
                        mesh.indices.push_back(nextLeft);
                        mesh.indices.push_back(topRight);
                    } else if (i == leftEdgeMultiplier - 1) {
                        mesh.indices.push_back(currentLeft);
                        mesh.indices.push_back(nextLeft);
                        mesh.indices.push_back(bottomRight);
                        
                        mesh.indices.push_back(currentLeft);
                        mesh.indices.push_back(bottomRight);
                        mesh.indices.push_back(topRight);
                    } else {
                        mesh.indices.push_back(currentLeft);
                        mesh.indices.push_back(nextLeft);
                        mesh.indices.push_back(topRight);
                    }
                    
                    currentLeft = nextLeft;
                }
            }
            else if (onRightEdge && rightEdgeMultiplier > 1) {
                // Special handling for right edge with T-junction
                // Extra vertices between topRight and bottomRight
                uint32_t numExtras = rightEdgeMultiplier - 1;
                uint32_t currentRight = topRight;
                
                for (uint32_t i = 0; i < rightEdgeMultiplier; i++) {
                    uint32_t nextRight;
                    
                    if (i == rightEdgeMultiplier - 1) {
                        nextRight = bottomRight;
                    } else {
                        nextRight = rightEdgeExtraStart + (y * numExtras) + i;
                    }
                    
                    if (i == 0) {
                        mesh.indices.push_back(topLeft);
                        mesh.indices.push_back(currentRight);
                        mesh.indices.push_back(nextRight);
                    } else if (i == rightEdgeMultiplier - 1) {
                        mesh.indices.push_back(topLeft);
                        mesh.indices.push_back(bottomLeft);
                        mesh.indices.push_back(currentRight);
                        
                        mesh.indices.push_back(bottomLeft);
                        mesh.indices.push_back(nextRight);
                        mesh.indices.push_back(currentRight);
                    } else {
                        mesh.indices.push_back(topLeft);
                        mesh.indices.push_back(currentRight);
                        mesh.indices.push_back(nextRight);
                    }
                    
                    currentRight = nextRight;
                }
            }
            else {
                // Standard triangles for interior
                mesh.indices.push_back(topLeft);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(topRight);
                
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(bottomRight);
            }
        }
    }
    
    // Generate skirt indices if enabled
    if (config.enableSkirts) {
        uint32_t skirtStart = mainVertexCount;
        
        // Top edge skirt
        for (uint32_t x = 0; x < res - 1; x++) {
            uint32_t mainTop = x;
            uint32_t mainTopRight = mainTop + 1;
            uint32_t skirtTop = skirtStart + x;
            uint32_t skirtTopRight = skirtTop + 1;
            
            mesh.indices.push_back(mainTop);
            mesh.indices.push_back(skirtTop);
            mesh.indices.push_back(mainTopRight);
            
            mesh.indices.push_back(mainTopRight);
            mesh.indices.push_back(skirtTop);
            mesh.indices.push_back(skirtTopRight);
        }
        
        // Similar for other edges...
        // (Implementation abbreviated for brevity)
    }
    
    mesh.vertexCount = mesh.vertices.size();
    mesh.indexCount = mesh.indices.size();
    
    // Debug disabled for testing
    // if (hasTJunction && tjunctionDebugCount < 10) { ... }
    
    // Debug: Look for issues that could cause grey dots
    static int debugCount = 0;
    if (debugCount++ < 5) {
        std::cout << "\n[DEBUG] Patch " << debugCount << " detailed analysis:" << std::endl;
        std::cout << "  Center: (" << patch.center.x << ", " << patch.center.y << ", " << patch.center.z << ")" << std::endl;
        std::cout << "  Level: " << patch.level << ", FaceId: " << patch.faceId << std::endl;
        
        // Special debug for X-faces (Face 0 and Face 1)
        if (patch.faceId == 0 || patch.faceId == 1) {
            std::cout << "  [X-FACE DEBUG] Face " << patch.faceId << " transform analysis:" << std::endl;
            // Print transform matrix
            std::cout << "  Transform matrix:" << std::endl;
            for (int row = 0; row < 4; row++) {
                std::cout << "    [";
                for (int col = 0; col < 4; col++) {
                    std::cout << std::setw(12) << patchTransform[col][row];
                    if (col < 3) std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }
            
            // Test transform of corner vertices
            glm::dvec4 testCorners[4] = {
                glm::dvec4(0.0, 0.0, 0.0, 1.0),  // Bottom-left
                glm::dvec4(1.0, 0.0, 0.0, 1.0),  // Bottom-right
                glm::dvec4(0.0, 1.0, 0.0, 1.0),  // Top-left
                glm::dvec4(1.0, 1.0, 0.0, 1.0)   // Top-right
            };
            
            std::cout << "  Transformed corners:" << std::endl;
            for (int i = 0; i < 4; i++) {
                glm::dvec3 transformed = glm::dvec3(patchTransform * testCorners[i]);
                std::cout << "    UV(" << testCorners[i].x << "," << testCorners[i].y 
                          << ") -> Cube(" << transformed.x << ", " << transformed.y 
                          << ", " << transformed.z << ")" << std::endl;
            }
        }
        std::cout << "  Total vertices: " << mesh.vertices.size() 
                  << " (main: " << mesh.mainVertexCount << ", skirt: " << mesh.skirtVertexCount << ")" << std::endl;
        
        // Check for vertices with extreme positions that might render as dots
        int outliers = 0;
        int nanVertices = 0;
        int extremeVertices = 0;
        float avgDistance = 0;
        float minDist = 1e10f;
        float maxDist = 0;
        
        for (size_t i = 0; i < mesh.vertices.size(); i++) {
            const auto& v = mesh.vertices[i];
            
            // Check for NaN or infinity
            if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z)) {
                nanVertices++;
                if (nanVertices <= 3) {
                    std::cout << "  NAN/INF VERTEX " << i << ": pos(" << v.position.x << ", " 
                              << v.position.y << ", " << v.position.z << ")" << std::endl;
                }
                continue;
            }
            
            float dist = glm::length(v.position);
            avgDistance += dist;
            minDist = std::min(minDist, dist);
            maxDist = std::max(maxDist, dist);
            
            // Check if vertex is way off the planet surface
            if (std::abs(dist - config.planetRadius) > 50000) { // More than 50km from expected radius
                outliers++;
                if (outliers <= 3) {
                    std::cout << "  OUTLIER VERTEX " << i << ": distance=" << dist 
                              << " (expected ~" << config.planetRadius << ")" << std::endl;
                    std::cout << "    Position: (" << v.position.x << ", " 
                              << v.position.y << ", " << v.position.z << ")" << std::endl;
                    std::cout << "    Height: " << v.height << std::endl;
                    std::cout << "    TexCoord: (" << v.texCoord.x << ", " << v.texCoord.y << ")" << std::endl;
                }
            }
            
            // Check for vertices extremely far from origin (possible escape)
            if (dist > config.planetRadius * 2) { // More than 2x planet radius
                extremeVertices++;
                if (extremeVertices <= 3) {
                    std::cout << "  EXTREME VERTEX " << i << ": distance=" << dist 
                              << " (" << (dist/config.planetRadius) << "x planet radius)" << std::endl;
                    std::cout << "    Position: (" << v.position.x << ", " 
                              << v.position.y << ", " << v.position.z << ")" << std::endl;
                }
            }
        }
        if (mesh.vertices.size() > 0) {
            avgDistance /= mesh.vertices.size();
        }
        
        std::cout << "  Vertex distance stats:" << std::endl;
        std::cout << "    Min: " << minDist << ", Max: " << maxDist << ", Avg: " << avgDistance << std::endl;
        std::cout << "    Expected: ~" << config.planetRadius << std::endl;
        
        if (nanVertices > 0) {
            std::cout << "  WARNING: " << nanVertices << " NaN/Inf vertices!" << std::endl;
        }
        if (outliers > 0) {
            std::cout << "  Total outlier vertices (>50km off): " << outliers << std::endl;
        }
        if (extremeVertices > 0) {
            std::cout << "  CRITICAL: " << extremeVertices << " vertices escaped to extreme distances!" << std::endl;
        }
        
        // Check for degenerate triangles that might render as dots
        int degenerateTris = 0;
        int tinyTris = 0;
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            if (i+2 >= mesh.indices.size()) break;
            
            uint32_t i0 = mesh.indices[i];
            uint32_t i1 = mesh.indices[i+1];
            uint32_t i2 = mesh.indices[i+2];
            
            if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
                std::cout << "  ERROR: Index out of bounds at triangle " << i/3 << std::endl;
                continue;
            }
            
            glm::vec3 v0 = mesh.vertices[i0].position;
            glm::vec3 v1 = mesh.vertices[i1].position;
            glm::vec3 v2 = mesh.vertices[i2].position;
            
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            float area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
            
            if (area < 0.001f) {
                degenerateTris++;
            } else if (area < 1.0f) { // Very small triangles (< 1 square meter)
                tinyTris++;
                if (tinyTris <= 3) {
                    std::cout << "  TINY TRIANGLE " << i/3 << ": area=" << area << " m²" << std::endl;
                }
            }
        }
        if (degenerateTris > 0 || tinyTris > 0) {
            std::cout << "  Degenerate triangles: " << degenerateTris 
                      << ", Tiny triangles (<1m²): " << tinyTris << std::endl;
        }
    }
    
    stats.totalVerticesGenerated += mesh.vertexCount;
    stats.currentCacheSize = vertexCache.size();
    
    return mesh;
}

PatchVertex CPUVertexGenerator::generateVertex(const glm::dvec3& cubePos, bool isSkirt, uint32_t faceId) {
    PatchVertex vertex;
    
    // Safety check for invalid input
    if (!std::isfinite(cubePos.x) || !std::isfinite(cubePos.y) || !std::isfinite(cubePos.z)) {
        static int errorCount = 0;
        if (errorCount++ < 5) {
            std::cout << "ERROR: Invalid cubePos in generateVertex: (" 
                      << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
        }
        // Return a default vertex at origin to avoid propagating NaN
        vertex.position = glm::vec3(0, 0, 0);
        vertex.normal = glm::vec3(0, 1, 0);
        vertex.texCoord = glm::vec2(0.0f);
        vertex.height = 0;
        vertex.faceId = faceId;
        return vertex;
    }
    
    // Convert cube position to sphere using unified mapping
    glm::dvec3 spherePos = PlanetSimulator::Math::cubeToSphereD(cubePos, 1.0);
    
    // Check for NaN after transformation
    if (!std::isfinite(spherePos.x) || !std::isfinite(spherePos.y) || !std::isfinite(spherePos.z)) {
        static int transformErrorCount = 0;
        if (transformErrorCount++ < 5) {
            std::cout << "ERROR: cubeToSphereD produced NaN from cubePos: (" 
                      << cubePos.x << ", " << cubePos.y << ", " << cubePos.z << ")" << std::endl;
        }
        // Fallback to simple normalization
        spherePos = glm::normalize(cubePos);
    }
    
    glm::dvec3 sphereNormal = glm::normalize(spherePos);
    
    // Generate terrain height
    float height = getTerrainHeight(glm::vec3(sphereNormal));
    
    // Clamp height to reasonable values
    if (!std::isfinite(height) || std::abs(height) > 100000) {
        static int heightErrorCount = 0;
        if (heightErrorCount++ < 5) {
            std::cout << "ERROR: Invalid height " << height << " at sphereNormal: (" 
                      << sphereNormal.x << ", " << sphereNormal.y << ", " << sphereNormal.z << ")" << std::endl;
        }
        height = 0; // Use sea level as fallback
    }
    
    // Apply height displacement
    double finalRadius = config.planetRadius + height;
    if (isSkirt) {
        finalRadius -= config.skirtDepth;
    }
    
    glm::dvec3 worldPos = sphereNormal * finalRadius;
    
    // Fill vertex data - keep in world space for now
    // Camera-relative transform will be applied later during buffer upload
    vertex.position = glm::vec3(worldPos);
    vertex.normal = calculateNormal(glm::vec3(sphereNormal));
    vertex.texCoord = glm::vec2(0.0f); // UV coords can be computed from position if needed
    vertex.height = height;
    vertex.faceId = faceId;
    
    return vertex;
}

CPUVertexGenerator::VertexKey CPUVertexGenerator::createVertexKey(const glm::dvec3& cubePos, uint32_t faceId) const {
    // CRITICAL: Use higher quantization for better precision at boundaries
    // This ensures vertices that are meant to be identical get the same key
    const double QUANTIZATION = 1000000.0;  // Increased from 10000
    
    // Pre-snap boundaries to ensure consistent keys
    glm::dvec3 snapped = cubePos;
    const double BOUNDARY = 1.0;
    const double EPSILON = 1e-8;
    
    if (std::abs(std::abs(snapped.x) - BOUNDARY) < EPSILON) {
        snapped.x = (snapped.x > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(snapped.y) - BOUNDARY) < EPSILON) {
        snapped.y = (snapped.y > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    if (std::abs(std::abs(snapped.z) - BOUNDARY) < EPSILON) {
        snapped.z = (snapped.z > 0.0) ? BOUNDARY : -BOUNDARY;
    }
    
    VertexKey key;
    key.qx = static_cast<int32_t>(std::round(snapped.x * QUANTIZATION));
    key.qy = static_cast<int32_t>(std::round(snapped.y * QUANTIZATION));
    key.qz = static_cast<int32_t>(std::round(snapped.z * QUANTIZATION));
    key.faceId = 0;  // Don't use faceId - allow vertices to be shared across faces!
    return key;
}

float CPUVertexGenerator::getTerrainHeight(const glm::vec3& sphereNormal) const {
    // Use the sphere normal directly as input to ensure continuity
    glm::vec3 noisePos = sphereNormal * 3.0f;
    
    // Continental shelf - large scale features
    float continents = terrainNoise(noisePos, 4) * 2.0f - 1.0f;
    continents = continents * 2000.0f - 500.0f;
    
    // Mountain ranges - medium scale
    float mountains = 0.0f;
    if (continents > 0.0f) {
        glm::vec3 mountainPos = sphereNormal * 8.0f;
        mountains = terrainNoise(mountainPos, 3) * 1200.0f;
    }
    
    // Small details - high frequency
    glm::vec3 detailPos = sphereNormal * 20.0f;
    float details = terrainNoise(detailPos, 2) * 200.0f - 100.0f;
    
    // Combine all layers
    float height = continents + mountains * 0.7f + details * 0.3f;
    
    // Ocean floor variation
    if (height < 0.0f) {
        height = height * 0.8f - 500.0f;
        height = std::max(height, -3000.0f);
    }
    
    return height;
}

glm::vec3 CPUVertexGenerator::calculateNormal(const glm::vec3& sphereNormal) const {
    // For now, just use sphere normal
    // TODO: Calculate proper terrain normal using finite differences
    return sphereNormal;
}

float CPUVertexGenerator::hash(const glm::vec3& p) const {
    glm::vec3 fp = glm::fract(p * glm::vec3(443.8975f, 397.2973f, 491.1871f));
    fp += glm::dot(fp, glm::vec3(fp.y + 19.19f, fp.z + 19.19f, fp.x + 19.19f));
    return glm::fract((fp.x + fp.y) * fp.z);
}

float CPUVertexGenerator::smoothNoise(const glm::vec3& p) const {
    glm::vec3 i = glm::floor(p);
    glm::vec3 f = glm::fract(p);
    
    // Smooth the interpolation
    f = f * f * (3.0f - 2.0f * f);
    
    // Sample the 8 corners of the cube
    float a = hash(i);
    float b = hash(i + glm::vec3(1.0f, 0.0f, 0.0f));
    float c = hash(i + glm::vec3(0.0f, 1.0f, 0.0f));
    float d = hash(i + glm::vec3(1.0f, 1.0f, 0.0f));
    float e = hash(i + glm::vec3(0.0f, 0.0f, 1.0f));
    float f1 = hash(i + glm::vec3(1.0f, 0.0f, 1.0f));
    float g = hash(i + glm::vec3(0.0f, 1.0f, 1.0f));
    float h = hash(i + glm::vec3(1.0f, 1.0f, 1.0f));
    
    // Trilinear interpolation
    float x1 = glm::mix(a, b, f.x);
    float x2 = glm::mix(c, d, f.x);
    float x3 = glm::mix(e, f1, f.x);
    float x4 = glm::mix(g, h, f.x);
    
    float y1 = glm::mix(x1, x2, f.y);
    float y2 = glm::mix(x3, x4, f.y);
    
    return glm::mix(y1, y2, f.z);
}

float CPUVertexGenerator::terrainNoise(const glm::vec3& p, int octaves) const {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += smoothNoise(p * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return value / maxValue;
}

void CPUVertexGenerator::clearCache() {
    vertexCache.clear();
    stats.cacheHits = 0;
    stats.cacheMisses = 0;
    stats.currentCacheSize = 0;
}

} // namespace rendering