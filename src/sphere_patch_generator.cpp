#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>
#include <iostream>
#include "rendering/transvoxel_renderer.hpp"
#include "core/octree.hpp"

namespace sphere_patches {

// Cube face enumeration
enum CubeFace {
    FACE_POS_X = 0,  // +X (right)
    FACE_NEG_X = 1,  // -X (left)  
    FACE_POS_Y = 2,  // +Y (top)
    FACE_NEG_Y = 3,  // -Y (bottom)
    FACE_POS_Z = 4,  // +Z (front)
    FACE_NEG_Z = 5   // -Z (back)
};

// Convert cube coordinates to sphere coordinates
glm::vec3 cubeToSphere(glm::vec3 cubePos, float radius) {
    // Normalize the cube position to get direction
    glm::vec3 dir = glm::normalize(cubePos);
    // Scale by radius
    return dir * radius;
}

// Generate vertices for a patch on a cube face
std::vector<glm::vec3> generateCubeFacePatch(
    CubeFace face, 
    float radius,
    int patchX, int patchY,  // Which patch on this face (0 to patchesPerSide-1)
    int patchesPerSide,       // How many patches per cube face side
    int verticesPerPatch      // Resolution of each patch (e.g., 33 for 32x32 quads)
) {
    std::vector<glm::vec3> vertices;
    
    // Calculate patch bounds in UV space (-1 to 1 on the cube face)
    float patchSize = 2.0f / patchesPerSide;
    float minU = -1.0f + patchX * patchSize;
    float maxU = minU + patchSize;
    float minV = -1.0f + patchY * patchSize;
    float maxV = minV + patchSize;
    
    // Generate vertices
    for (int v = 0; v < verticesPerPatch; v++) {
        for (int u = 0; u < verticesPerPatch; u++) {
            // UV coordinates within this patch
            float tu = minU + (maxU - minU) * (float)u / (verticesPerPatch - 1);
            float tv = minV + (maxV - minV) * (float)v / (verticesPerPatch - 1);
            
            // Convert UV to 3D cube position based on face
            glm::vec3 cubePos(0.0f);  // Initialize to avoid warning
            switch(face) {
                case FACE_POS_X: cubePos = glm::vec3( 1.0f,    tv,    tu); break;
                case FACE_NEG_X: cubePos = glm::vec3(-1.0f,    tv,   -tu); break;
                case FACE_POS_Y: cubePos = glm::vec3(   tu,  1.0f,    tv); break;
                case FACE_NEG_Y: cubePos = glm::vec3(   tu, -1.0f,   -tv); break;
                case FACE_POS_Z: cubePos = glm::vec3(  -tu,    tv,  1.0f); break;
                case FACE_NEG_Z: cubePos = glm::vec3(   tu,    tv, -1.0f); break;
                default: cubePos = glm::vec3(1.0f, 0.0f, 0.0f); break;  // Should never happen
            }
            
            // Project cube position onto sphere
            glm::vec3 spherePos = cubeToSphere(cubePos, radius);
            vertices.push_back(spherePos);
        }
    }
    
    return vertices;
}

// Generate a complete sphere patch mesh
rendering::TransvoxelChunk generateSpherePatch(
    CubeFace face,
    float radius,
    int patchX, int patchY,
    int patchesPerSide,
    int verticesPerPatch = 33,  // 33x33 vertices = 32x32 quads
    octree::OctreePlanet* planet = nullptr  // Optional: sample colors from planet
) {
    rendering::TransvoxelChunk chunk;
    
    // Generate vertices
    auto verts = generateCubeFacePatch(face, radius, patchX, patchY, patchesPerSide, verticesPerPatch);
    
    // Debug: Print patch info at start (disabled)
    // std::cout << "Generating patch face=" << face << " pos=(" << patchX << "," << patchY << ")\n";
    
    // Debug: Sample a few vertices to understand material distribution (disabled)
    // static int debugPatchCount = 0;
    // bool debugThisPatch = (debugPatchCount++ < 3);  // Debug first 3 patches
    // int debugVertexCount = 0;
    
    // Convert to chunk vertices with colors and normals
    for (const auto& pos : verts) {
        rendering::Vertex vertex;
        vertex.position = pos;
        vertex.normal = glm::normalize(pos);  // Normal points outward from center
        
        // Sample color from planet if available
        if (planet && planet->getRoot()) {
            // No scaling needed - sphere is already at planet scale
            glm::vec3 samplePos = pos;
            
            // Query the octree for the voxel at this position
            // If we hit air/vacuum, sample deeper to find the surface
            const octree::MixedVoxel* voxel = planet->getVoxel(samplePos);
            
            // If we hit air/vacuum, step inward to find the surface
            glm::vec3 currentPos = samplePos;
            const int maxSteps = 10;
            float stepSize = 1000.0f; // 1km steps inward
            
            for (int step = 0; step < maxSteps; step++) {
                voxel = planet->getVoxel(currentPos);
                
                if (voxel && !voxel->isEmpty()) {
                    // Found a non-empty voxel (water, land, etc.)
                    break;
                }
                
                // Step inward toward planet center
                glm::vec3 inwardDir = -glm::normalize(currentPos);
                currentPos += inwardDir * stepSize;
            }
            
            if (voxel && !voxel->isEmpty()) {
                // Get the blended color from the voxel's materials
                vertex.color = voxel->getColor();
                
                
                
                // Debug output for first few vertices (disabled for now)
                // if (debugThisPatch && debugVertexCount++ < 10) {
                //     core::MaterialID dominant = voxel->getDominantMaterialID();
                //     std::cout << "Patch " << (debugPatchCount-1) << " vertex " << (debugVertexCount-1) 
                //               << ": material=" << static_cast<int>(dominant)
                //               << " color=(" << vertex.color.r << "," << vertex.color.g << "," << vertex.color.b << ")\n";
                // }
                
                // Noise disabled to debug moiré pattern
                // float noise = sin(samplePos.x * 0.00001f) * cos(samplePos.z * 0.00001f);
                // vertex.color += noise * 0.05f;  // Subtle variation
                // vertex.color = glm::clamp(vertex.color, 0.0f, 1.0f);
            } else {
                // Fallback - should rarely happen now that we step inward
                // Use a distinct color to make it obvious if we hit this case
                vertex.color = glm::vec3(1.0f, 0.0f, 1.0f);  // Magenta for debugging
            }
        } else {
            // Make each face VERY different colors for debugging
            switch(face) {
                case FACE_POS_X: vertex.color = glm::vec3(1.0f, 0.0f, 0.0f); break; // RED
                case FACE_NEG_X: vertex.color = glm::vec3(0.0f, 1.0f, 0.0f); break; // GREEN  
                case FACE_POS_Y: vertex.color = glm::vec3(0.0f, 0.0f, 1.0f); break; // BLUE
                case FACE_NEG_Y: vertex.color = glm::vec3(1.0f, 1.0f, 0.0f); break; // YELLOW
                case FACE_POS_Z: vertex.color = glm::vec3(1.0f, 0.0f, 1.0f); break; // MAGENTA
                case FACE_NEG_Z: vertex.color = glm::vec3(0.0f, 1.0f, 1.0f); break; // CYAN
            }
            
            // Add patch variation within face
            vertex.color *= (0.5f + 0.5f * ((patchX + patchY) % 2));
            
            // Debug first vertex of each patch (disabled)
            // if (chunk.vertices.empty()) {  // First vertex of this patch
            //     std::cout << "  First vertex color=(" << vertex.color.r << "," << vertex.color.g 
            //               << "," << vertex.color.b << ")\n";
            // }
        }
        
        vertex.texCoord = glm::vec2(0, 0);  // TODO: Proper UV mapping
        chunk.vertices.push_back(vertex);
    }
    
    // Generate indices for triangle mesh (assuming row-major vertex ordering)
    int quadsPerSide = verticesPerPatch - 1;
    for (int v = 0; v < quadsPerSide; v++) {
        for (int u = 0; u < quadsPerSide; u++) {
            // Indices of the four corners of this quad
            uint32_t i0 = v * verticesPerPatch + u;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (v + 1) * verticesPerPatch + u;
            uint32_t i3 = i2 + 1;
            
            // Two triangles per quad
            chunk.indices.push_back(i0);
            chunk.indices.push_back(i2);
            chunk.indices.push_back(i1);
            
            chunk.indices.push_back(i1);
            chunk.indices.push_back(i2);
            chunk.indices.push_back(i3);
        }
    }
    
    // Set chunk metadata
    chunk.position = glm::vec3(0, 0, 0);  // Patches are already in world space
    chunk.voxelSize = radius * 2.0f / (patchesPerSide * verticesPerPatch);
    chunk.lodLevel = 0;
    
    return chunk;
}

// Generate all patches for a complete sphere at given resolution
std::vector<rendering::TransvoxelChunk> generateSphere(
    float radius,
    int resolution,  // 0 = 6 patches, 1 = 24 patches, 2 = 96 patches, etc.
    octree::OctreePlanet* planet = nullptr  // Optional: sample from planet data
) {
    std::vector<rendering::TransvoxelChunk> chunks;
    
    // Calculate patches per cube face side (1, 2, 4, 8...)
    int patchesPerSide = 1 << resolution;  // 2^resolution
    
    // Generate patches for all 6 cube faces
    for (int face = 0; face < 6; face++) {
        for (int y = 0; y < patchesPerSide; y++) {
            for (int x = 0; x < patchesPerSide; x++) {
                auto patch = generateSpherePatch(
                    (CubeFace)face, 
                    radius, 
                    x, y, 
                    patchesPerSide,
                    33,  // vertices per patch
                    planet
                );
                chunks.push_back(patch);
            }
        }
    }
    
    return chunks;
}

} // namespace sphere_patches