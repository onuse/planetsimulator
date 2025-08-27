// Analyze what the octree data should look like for the compute shader
#include <iostream>
#include <cstdint>
#include <cmath>

struct OctreeNode {
    float center[3];
    float halfSize;
    uint32_t childrenAndFlags[4];
};

bool isLeaf(uint32_t flags) {
    return (flags & 1u) != 0u;
}

uint32_t getMaterial(uint32_t flags) {
    return (flags >> 8u) & 0xFFu;
}

int main() {
    std::cout << "=== Octree Data Analysis ===\n\n";
    
    // From the runtime output: "Got 5935112 nodes"
    const int totalNodes = 5935112;
    const float planetRadius = 1000.0f;
    
    std::cout << "Expected octree structure:\n";
    std::cout << "  Total nodes: " << totalNodes << "\n";
    std::cout << "  Planet radius: " << planetRadius << " meters\n";
    std::cout << "  Root half-size: 1500 meters\n";
    std::cout << "  Max depth: 10\n\n";
    
    // Calculate expected voxel sizes at each depth
    std::cout << "Voxel sizes by depth:\n";
    float halfSize = 1500.0f;
    for (int depth = 0; depth <= 10; depth++) {
        std::cout << "  Depth " << depth << ": " << halfSize << " meters\n";
        halfSize /= 2.0f;
    }
    
    std::cout << "\nSurface voxel criteria:\n";
    std::cout << "  - Must be a leaf node (flags & 1)\n";
    std::cout << "  - Must have non-air material (material != 0)\n";
    std::cout << "  - Must be near surface: |distance - radius| <= halfSize * 2\n";
    
    // Estimate how many voxels might generate quads
    std::cout << "\nEstimated surface voxels:\n";
    
    // Surface area of sphere
    float surfaceArea = 4.0f * M_PI * planetRadius * planetRadius;
    
    // At different depths, voxel face area changes
    for (int depth = 8; depth <= 10; depth++) {
        float voxelSize = 1500.0f / std::pow(2.0f, depth);
        float voxelFaceArea = voxelSize * voxelSize;
        int approxVoxels = (int)(surfaceArea / voxelFaceArea);
        std::cout << "  Depth " << depth << " (" << voxelSize << "m): ~" 
                  << approxVoxels << " surface voxels\n";
    }
    
    std::cout << "\nShader dispatch:\n";
    const int workgroupSize = 64;
    int workgroups = (totalNodes + workgroupSize - 1) / workgroupSize;
    std::cout << "  Workgroup size: " << workgroupSize << " threads\n";
    std::cout << "  Total workgroups: " << workgroups << "\n";
    std::cout << "  Total threads: " << workgroups * workgroupSize << "\n";
    
    std::cout << "\nMemory requirements:\n";
    size_t nodeSize = sizeof(OctreeNode);
    size_t vertexSize = sizeof(float) * 11; // pos(3) + color(3) + normal(3) + texcoord(2)
    
    std::cout << "  Node buffer: " << (totalNodes * nodeSize) / (1024*1024) << " MB\n";
    std::cout << "  Vertex buffer (1M max): " << (1000000 * vertexSize) / (1024*1024) << " MB\n";
    std::cout << "  Index buffer (3M max): " << (3000000 * sizeof(uint32_t)) / (1024*1024) << " MB\n";
    
    // Analyze the flags format
    std::cout << "\nFlag format analysis:\n";
    uint32_t testFlags = 0x0201; // From output logs
    std::cout << "  Example flags: 0x" << std::hex << testFlags << std::dec << "\n";
    std::cout << "    Is leaf: " << (isLeaf(testFlags) ? "YES" : "NO") << "\n";
    std::cout << "    Material: " << getMaterial(testFlags) << " (Rock)\n";
    
    return 0;
}