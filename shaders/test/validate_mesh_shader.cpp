// CPU validation harness for mesh_generator_simple.comp shader logic
// This lets us test the algorithm before running on GPU

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdint>

// Simulate GLSL structures
struct OctreeNode {
    float center[3];
    float halfSize;
    uint32_t childrenAndFlags[4];
};

struct Vertex {
    float position[3];
    float color[3];
    float normal[3];
    float texCoord[2];
};

// Constants from shader
const uint32_t MAX_VERTICES = 1000000;
const uint32_t MAX_INDICES = 3000000;
const float PLANET_RADIUS = 1000.0f;

// Helper functions matching shader
bool isLeaf(const OctreeNode& node) {
    return (node.childrenAndFlags[2] & 1u) != 0u;
}

uint32_t getMaterial(const OctreeNode& node) {
    return (node.childrenAndFlags[2] >> 8u) & 0xFFu;
}

void getMaterialColor(uint32_t mat, float color[3]) {
    if (mat == 1u) { color[0] = 0.7f; color[1] = 0.9f; color[2] = 1.0f; }  // Air - light blue
    else if (mat == 2u) { color[0] = 0.5f; color[1] = 0.4f; color[2] = 0.3f; }  // Rock - brown
    else if (mat == 3u) { color[0] = 0.0f; color[1] = 0.3f; color[2] = 0.7f; }  // Water - blue
    else { color[0] = 1.0f; color[1] = 0.0f; color[2] = 1.0f; }  // Unknown - magenta
}

float length(const float v[3]) {
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

void normalize(float v[3]) {
    float len = length(v);
    if (len > 0.0f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

void cross(const float a[3], const float b[3], float result[3]) {
    result[0] = a[1]*b[2] - a[2]*b[1];
    result[1] = a[2]*b[0] - a[0]*b[2];
    result[2] = a[0]*b[1] - a[1]*b[0];
}

// Test cases
void test_leaf_detection() {
    std::cout << "TEST: Leaf Detection\n";
    
    OctreeNode node;
    
    // Test non-leaf
    node.childrenAndFlags[2] = 0x0000;  // No flags set
    assert(!isLeaf(node));
    std::cout << "  ✓ Non-leaf detected correctly\n";
    
    // Test leaf
    node.childrenAndFlags[2] = 0x0001;  // Leaf flag set
    assert(isLeaf(node));
    std::cout << "  ✓ Leaf detected correctly\n";
    
    // Test leaf with other flags
    node.childrenAndFlags[2] = 0x0201;  // Leaf flag + material in upper bits
    assert(isLeaf(node));
    assert(getMaterial(node) == 2);
    std::cout << "  ✓ Leaf with material detected correctly\n";
}

void test_distance_filtering() {
    std::cout << "\nTEST: Distance Filtering\n";
    
    OctreeNode node;
    node.halfSize = 10.0f;
    
    // Test voxel at surface
    node.center[0] = PLANET_RADIUS;
    node.center[1] = 0.0f;
    node.center[2] = 0.0f;
    float dist = length(node.center);
    bool nearSurface = std::abs(dist - PLANET_RADIUS) <= node.halfSize * 2.0f;
    assert(nearSurface);
    std::cout << "  ✓ Surface voxel passes filter\n";
    
    // Test voxel far inside
    node.center[0] = 500.0f;
    node.center[1] = 0.0f;
    node.center[2] = 0.0f;
    dist = length(node.center);
    nearSurface = std::abs(dist - PLANET_RADIUS) <= node.halfSize * 2.0f;
    assert(!nearSurface);
    std::cout << "  ✓ Deep voxel filtered out\n";
    
    // Test voxel far outside
    node.center[0] = 1500.0f;
    node.center[1] = 0.0f;
    node.center[2] = 0.0f;
    dist = length(node.center);
    nearSurface = std::abs(dist - PLANET_RADIUS) <= node.halfSize * 2.0f;
    assert(!nearSurface);
    std::cout << "  ✓ Outer voxel filtered out\n";
}

void test_normal_generation() {
    std::cout << "\nTEST: Normal Generation\n";
    
    float center[3] = {1000.0f, 0.0f, 0.0f};
    float normal[3];
    
    // Copy center to normal and normalize
    normal[0] = center[0];
    normal[1] = center[1];
    normal[2] = center[2];
    normalize(normal);
    
    // Should point outward from origin
    assert(std::abs(normal[0] - 1.0f) < 0.001f);
    assert(std::abs(normal[1]) < 0.001f);
    assert(std::abs(normal[2]) < 0.001f);
    std::cout << "  ✓ Normal points outward correctly\n";
    
    // Test diagonal position
    center[0] = 707.1f;
    center[1] = 707.1f;
    center[2] = 0.0f;
    normal[0] = center[0];
    normal[1] = center[1];
    normal[2] = center[2];
    normalize(normal);
    
    assert(std::abs(normal[0] - 0.7071f) < 0.01f);
    assert(std::abs(normal[1] - 0.7071f) < 0.01f);
    std::cout << "  ✓ Diagonal normal correct\n";
}

void test_tangent_generation() {
    std::cout << "\nTEST: Tangent Generation\n";
    
    float normal[3] = {1.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};  // abs(normal.y) < 0.9
    float tangent[3];
    float bitangent[3];
    
    // tangent = cross(up, normal)
    cross(up, normal, tangent);
    normalize(tangent);
    
    // bitangent = cross(normal, tangent)
    cross(normal, tangent, bitangent);
    
    // Check orthogonality
    float dot_nt = normal[0]*tangent[0] + normal[1]*tangent[1] + normal[2]*tangent[2];
    float dot_nb = normal[0]*bitangent[0] + normal[1]*bitangent[1] + normal[2]*bitangent[2];
    float dot_tb = tangent[0]*bitangent[0] + tangent[1]*bitangent[1] + tangent[2]*bitangent[2];
    
    assert(std::abs(dot_nt) < 0.001f);
    assert(std::abs(dot_nb) < 0.001f);
    assert(std::abs(dot_tb) < 0.001f);
    std::cout << "  ✓ Tangent frame is orthogonal\n";
}

void test_quad_generation() {
    std::cout << "\nTEST: Quad Vertex Generation\n";
    
    float center[3] = {1000.0f, 0.0f, 0.0f};
    float halfSize = 10.0f;
    float normal[3] = {1.0f, 0.0f, 0.0f};
    float tangent[3] = {0.0f, 0.0f, -1.0f};
    float bitangent[3] = {0.0f, 1.0f, 0.0f};
    
    Vertex vertices[4];
    float size = halfSize * 0.8f;
    
    // Generate 4 vertices
    for (int i = 0; i < 4; i++) {
        float t = (i == 0 || i == 1) ? 1.0f : -1.0f;
        float b = (i == 0 || i == 3) ? -1.0f : 1.0f;
        
        vertices[i].position[0] = center[0] + (tangent[0] * t + bitangent[0] * b) * size;
        vertices[i].position[1] = center[1] + (tangent[1] * t + bitangent[1] * b) * size;
        vertices[i].position[2] = center[2] + (tangent[2] * t + bitangent[2] * b) * size;
    }
    
    // Check vertices form a square
    float edge_len[4];
    for (int i = 0; i < 4; i++) {
        int next = (i + 1) % 4;
        float dx = vertices[next].position[0] - vertices[i].position[0];
        float dy = vertices[next].position[1] - vertices[i].position[1];
        float dz = vertices[next].position[2] - vertices[i].position[2];
        edge_len[i] = std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    // All edges should be equal
    for (int i = 1; i < 4; i++) {
        assert(std::abs(edge_len[i] - edge_len[0]) < 0.001f);
    }
    std::cout << "  ✓ Quad vertices form a square\n";
    
    // Check distance from center
    for (int i = 0; i < 4; i++) {
        float dx = vertices[i].position[0] - center[0];
        float dy = vertices[i].position[1] - center[1];
        float dz = vertices[i].position[2] - center[2];
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        assert(std::abs(dist - size * std::sqrt(2.0f)) < 0.001f);
    }
    std::cout << "  ✓ Vertices at correct distance from center\n";
}

void simulate_full_shader() {
    std::cout << "\nSIMULATION: Full Shader Execution\n";
    
    // Create test octree nodes
    std::vector<OctreeNode> nodes;
    
    // Add a leaf node at the surface
    OctreeNode surfaceNode;
    surfaceNode.center[0] = 999.0f;
    surfaceNode.center[1] = 0.0f;
    surfaceNode.center[2] = 0.0f;
    surfaceNode.halfSize = 5.0f;
    surfaceNode.childrenAndFlags[2] = 0x0201;  // Leaf + Rock material
    nodes.push_back(surfaceNode);
    
    // Add a non-leaf node (should be skipped)
    OctreeNode innerNode;
    innerNode.center[0] = 500.0f;
    innerNode.center[1] = 0.0f;
    innerNode.center[2] = 0.0f;
    innerNode.halfSize = 50.0f;
    innerNode.childrenAndFlags[2] = 0x0200;  // Not a leaf
    nodes.push_back(innerNode);
    
    // Add an air node (should be skipped)
    OctreeNode airNode;
    airNode.center[0] = 1005.0f;
    airNode.center[1] = 0.0f;
    airNode.center[2] = 0.0f;
    airNode.halfSize = 5.0f;
    airNode.childrenAndFlags[2] = 0x0001;  // Leaf + Air material (0)
    nodes.push_back(airNode);
    
    // Simulate shader execution
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    
    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++) {
        const OctreeNode& node = nodes[nodeIndex];
        
        // Check if leaf
        if (!isLeaf(node)) {
            std::cout << "  Node " << nodeIndex << ": Skipped (not leaf)\n";
            continue;
        }
        
        // Check material
        uint32_t material = getMaterial(node);
        if (material == 0u) {
            std::cout << "  Node " << nodeIndex << ": Skipped (air material)\n";
            continue;
        }
        
        // Check distance from surface
        float distFromCenter = length(node.center);
        if (std::abs(distFromCenter - PLANET_RADIUS) > node.halfSize * 2.0f) {
            std::cout << "  Node " << nodeIndex << ": Skipped (too far from surface)\n";
            continue;
        }
        
        // Would generate quad here
        vertexCount += 4;
        indexCount += 6;
        std::cout << "  Node " << nodeIndex << ": Generated quad (material=" << material << ")\n";
    }
    
    std::cout << "\nFinal counts: " << vertexCount << " vertices, " << indexCount << " indices\n";
    assert(vertexCount == 4);  // Only surface node should generate geometry
    assert(indexCount == 6);
    std::cout << "  ✓ Correct number of primitives generated\n";
}

int main() {
    std::cout << "=== Mesh Shader Validation Harness ===\n\n";
    
    test_leaf_detection();
    test_distance_filtering();
    test_normal_generation();
    test_tangent_generation();
    test_quad_generation();
    simulate_full_shader();
    
    std::cout << "\n=== All Tests Passed! ===\n";
    return 0;
}