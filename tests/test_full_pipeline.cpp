/**
 * Full Pipeline Test
 * 
 * This test verifies the entire octree material pipeline:
 * 1. Voxel generation
 * 2. Material counting
 * 3. GPU upload
 * 4. Shader traversal
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cmath>

// Simulate the entire material pipeline
class PipelineTest {
public:
    // Material types matching the enum
    enum MaterialType {
        Air = 0,
        Rock = 1,
        Water = 2,
        Magma = 3
    };
    
    struct Voxel {
        MaterialType material;
        float density;
    };
    
    struct OctreeNode {
        Voxel voxels[8];
        float centerX, centerY, centerZ;
        float halfSize;
        bool isLeaf;
    };
    
    void runTests() {
        std::cout << "=== OCTREE MATERIAL PIPELINE TEST ===\n\n";
        
        // Test 1: Voxel material generation
        std::cout << "TEST 1: Voxel Material Generation\n";
        std::cout << "---------------------------------\n";
        testVoxelGeneration();
        
        // Test 2: Material counting logic
        std::cout << "\nTEST 2: Material Counting\n";
        std::cout << "-------------------------\n";
        testMaterialCounting();
        
        // Test 3: GPU encoding/decoding
        std::cout << "\nTEST 3: GPU Encoding\n";
        std::cout << "--------------------\n";
        testGPUEncoding();
        
        // Test 4: Common failure scenarios
        std::cout << "\nTEST 4: Failure Scenarios\n";
        std::cout << "-------------------------\n";
        testFailureScenarios();
    }
    
private:
    void testVoxelGeneration() {
        float planetRadius = 6371000.0f;
        
        // Test surface node
        OctreeNode surfaceNode;
        surfaceNode.centerX = 6300000.0f;
        surfaceNode.centerY = 0;
        surfaceNode.centerZ = 0;
        surfaceNode.halfSize = 100000.0f;
        
        std::cout << "Surface node at distance " << sqrt(surfaceNode.centerX * surfaceNode.centerX) << "m\n";
        
        // Simulate voxel generation
        for (int i = 0; i < 8; i++) {
            float voxelX = surfaceNode.centerX + (i & 1 ? 1 : -1) * surfaceNode.halfSize * 0.5f;
            float voxelY = surfaceNode.centerY + (i & 2 ? 1 : -1) * surfaceNode.halfSize * 0.5f;
            float voxelZ = surfaceNode.centerZ + (i & 4 ? 1 : -1) * surfaceNode.halfSize * 0.5f;
            float dist = sqrt(voxelX * voxelX + voxelY * voxelY + voxelZ * voxelZ);
            
            if (dist > planetRadius * 1.01f) {
                surfaceNode.voxels[i].material = Air;
            } else if (dist > planetRadius * 0.99f) {
                // Surface - mix of rock and water
                float noise = sin(voxelX * 0.00001f) * cos(voxelZ * 0.00001f);
                surfaceNode.voxels[i].material = (noise > 0.0f) ? Rock : Water;
            } else {
                surfaceNode.voxels[i].material = Rock; // Underground
            }
            
            std::cout << "  Voxel " << i << ": dist=" << dist 
                      << " -> " << getMaterialName(surfaceNode.voxels[i].material) << "\n";
        }
    }
    
    void testMaterialCounting() {
        OctreeNode testNode;
        
        // Scenario 1: Mixed surface materials
        std::cout << "Scenario 1: Mixed surface (3 Rock, 4 Water, 1 Air)\n";
        testNode.voxels[0].material = Rock;
        testNode.voxels[1].material = Rock;
        testNode.voxels[2].material = Rock;
        testNode.voxels[3].material = Water;
        testNode.voxels[4].material = Water;
        testNode.voxels[5].material = Water;
        testNode.voxels[6].material = Water;
        testNode.voxels[7].material = Air;
        
        MaterialType dominant = countDominantMaterial(testNode);
        std::cout << "  Dominant: " << getMaterialName(dominant) << " (expected: Water)\n";
        
        // Scenario 2: All air (empty space)
        std::cout << "\nScenario 2: All air\n";
        for (int i = 0; i < 8; i++) {
            testNode.voxels[i].material = Air;
        }
        dominant = countDominantMaterial(testNode);
        std::cout << "  Dominant: " << getMaterialName(dominant) << " (expected: Air)\n";
        
        // Scenario 3: Tie between materials
        std::cout << "\nScenario 3: Tie (4 Rock, 4 Water)\n";
        for (int i = 0; i < 4; i++) {
            testNode.voxels[i].material = Rock;
            testNode.voxels[i+4].material = Water;
        }
        dominant = countDominantMaterial(testNode);
        std::cout << "  Dominant: " << getMaterialName(dominant) << "\n";
    }
    
    void testGPUEncoding() {
        // Test the bit encoding used in GPU
        uint32_t flags = 1; // isLeaf
        
        // Test each material type
        for (int mat = 0; mat < 4; mat++) {
            uint32_t encoded = flags | (mat << 8);
            uint32_t decodedMat = (encoded >> 8) & 0xFF;
            bool decodedLeaf = (encoded & 1) != 0;
            
            std::cout << "Material " << getMaterialName((MaterialType)mat) 
                      << ": encoded=0x" << std::hex << encoded << std::dec
                      << " -> decoded mat=" << decodedMat
                      << ", isLeaf=" << decodedLeaf << "\n";
        }
    }
    
    void testFailureScenarios() {
        std::cout << "1. BLACK PLANET CAUSES:\n";
        std::cout << "   a) All nodes have Air material (0)\n";
        std::cout << "   b) Shader exits early without finding leaves\n";
        std::cout << "   c) Incorrect node traversal (wrong child indices)\n";
        std::cout << "   d) Materials not properly encoded in flags\n\n";
        
        std::cout << "2. DEBUGGING STRATEGY:\n";
        std::cout << "   a) Add debug output in GPU upload to verify materials\n";
        std::cout << "   b) Check first few leaf nodes for non-Air materials\n";
        std::cout << "   c) Verify shader receives correct data\n";
        std::cout << "   d) Test with hardcoded materials to isolate issue\n\n";
        
        std::cout << "3. LIKELY ISSUE:\n";
        std::cout << "   The voxels array might be uninitialized or all Air.\n";
        std::cout << "   The material counting might return 0 for all materials.\n";
        std::cout << "   The shader might not be finding the correct leaves.\n";
    }
    
    MaterialType countDominantMaterial(const OctreeNode& node) {
        uint32_t counts[4] = {0, 0, 0, 0};
        
        for (int i = 0; i < 8; i++) {
            uint32_t mat = static_cast<uint32_t>(node.voxels[i].material);
            if (mat < 4) {
                counts[mat]++;
            }
        }
        
        uint32_t maxCount = 0;
        MaterialType dominant = Air;
        for (int m = 0; m < 4; m++) {
            if (counts[m] > maxCount) {
                maxCount = counts[m];
                dominant = static_cast<MaterialType>(m);
            }
        }
        
        std::cout << "  Counts: Air=" << counts[0] 
                  << ", Rock=" << counts[1]
                  << ", Water=" << counts[2]
                  << ", Magma=" << counts[3] << "\n";
        
        return dominant;
    }
    
    const char* getMaterialName(MaterialType mat) {
        switch(mat) {
            case Air: return "Air";
            case Rock: return "Rock";
            case Water: return "Water";
            case Magma: return "Magma";
            default: return "Unknown";
        }
    }
};

int main() {
    PipelineTest test;
    test.runTests();
    
    std::cout << "\n=== RECOMMENDED NEXT STEPS ===\n";
    std::cout << "1. Add console output in gpu_octree.cpp after line 190:\n";
    std::cout << "   std::cout << \"Voxel materials: \";\n";
    std::cout << "   for(int i = 0; i < 8; i++) {\n";
    std::cout << "       std::cout << (int)voxels[i].material << \" \";\n";
    std::cout << "   }\n";
    std::cout << "   std::cout << \"-> dominant: \" << (int)dominantMaterial << std::endl;\n\n";
    
    std::cout << "2. Check if voxels are being initialized in octree.cpp\n";
    std::cout << "3. Verify the shader is receiving non-zero materials\n";
    std::cout << "4. Test with hardcoded non-Air material to isolate the issue\n";
    
    return 0;
}