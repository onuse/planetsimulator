#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include "rendering/transvoxel_renderer.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace test {

// Generate a simple UV sphere mesh for testing
rendering::TransvoxelChunk generateTestSphere(float radius = 10.0f, glm::vec3 center = glm::vec3(0,0,-50)) {
    rendering::TransvoxelChunk chunk;
    chunk.position = center;
    chunk.voxelSize = 1.0f;
    chunk.lodLevel = 0;
    chunk.isDirty = false;
    chunk.hasValidMesh = false;
    
    const int latSegments = 16;
    const int lonSegments = 32;
    
    // Generate vertices
    for (int lat = 0; lat <= latSegments; lat++) {
        float theta = lat * static_cast<float>(M_PI) / latSegments;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);
        
        for (int lon = 0; lon <= lonSegments; lon++) {
            float phi = lon * 2 * static_cast<float>(M_PI) / lonSegments;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);
            
            rendering::Vertex v;
            v.normal = glm::vec3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
            v.position = center + v.normal * radius;
            v.color = glm::vec3(0.5f, 0.7f, 1.0f); // Light blue
            v.texCoord = glm::vec2((float)lon / lonSegments, (float)lat / latSegments);
            
            chunk.vertices.push_back(v);
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < latSegments; lat++) {
        for (int lon = 0; lon < lonSegments; lon++) {
            int current = lat * (lonSegments + 1) + lon;
            int next = current + lonSegments + 1;
            
            // Triangle 1
            chunk.indices.push_back(current);
            chunk.indices.push_back(next);
            chunk.indices.push_back(current + 1);
            
            // Triangle 2
            chunk.indices.push_back(current + 1);
            chunk.indices.push_back(next);
            chunk.indices.push_back(next + 1);
        }
    }
    
    return chunk;
}

// Generate a simple cube mesh for testing
rendering::TransvoxelChunk generateTestCube(float size = 10.0f, glm::vec3 center = glm::vec3(0,0,-50)) {
    rendering::TransvoxelChunk chunk;
    chunk.position = center;
    chunk.voxelSize = 1.0f;
    chunk.lodLevel = 0;
    chunk.isDirty = false;
    chunk.hasValidMesh = false;
    
    float h = size / 2.0f;
    
    // Define cube vertices
    glm::vec3 positions[8] = {
        center + glm::vec3(-h, -h, -h), // 0
        center + glm::vec3( h, -h, -h), // 1
        center + glm::vec3( h,  h, -h), // 2
        center + glm::vec3(-h,  h, -h), // 3
        center + glm::vec3(-h, -h,  h), // 4
        center + glm::vec3( h, -h,  h), // 5
        center + glm::vec3( h,  h,  h), // 6
        center + glm::vec3(-h,  h,  h), // 7
    };
    
    // Define faces (each face has 4 vertices, we'll duplicate for proper normals)
    struct Face {
        int indices[4];
        glm::vec3 normal;
    };
    
    Face faces[6] = {
        {{0,1,2,3}, glm::vec3(0,0,-1)}, // Front
        {{5,4,7,6}, glm::vec3(0,0,1)},  // Back
        {{4,0,3,7}, glm::vec3(-1,0,0)}, // Left
        {{1,5,6,2}, glm::vec3(1,0,0)},  // Right
        {{3,2,6,7}, glm::vec3(0,1,0)},  // Top
        {{4,5,1,0}, glm::vec3(0,-1,0)}, // Bottom
    };
    
    // Create vertices with proper normals for each face
    for (int f = 0; f < 6; f++) {
        Face& face = faces[f];
        int baseIdx = static_cast<int>(chunk.vertices.size());
        
        // Add 4 vertices for this face
        for (int i = 0; i < 4; i++) {
            rendering::Vertex v;
            v.position = positions[face.indices[i]];
            v.normal = face.normal;
            v.color = glm::vec3(0.8f, 0.3f, 0.3f); // Reddish
            v.texCoord = glm::vec2((i%2), (i/2));
            chunk.vertices.push_back(v);
        }
        
        // Add 2 triangles for this face
        chunk.indices.push_back(baseIdx + 0);
        chunk.indices.push_back(baseIdx + 1);
        chunk.indices.push_back(baseIdx + 2);
        
        chunk.indices.push_back(baseIdx + 0);
        chunk.indices.push_back(baseIdx + 2);
        chunk.indices.push_back(baseIdx + 3);
    }
    
    return chunk;
}

} // namespace test