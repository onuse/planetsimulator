#include <iostream>
#include <cassert>
#include "../include/core/octree.hpp"

// Test each stage of material pipeline in isolation
class MaterialIsolationTest {
public:
    void runAll() {
        std::cout << "=== MATERIAL PIPELINE ISOLATION TESTS ===" << std::endl;
        
        test1_VoxelStorage();
        test2_NodeVoxelPersistence();
        test3_TreeTraversal();
        test4_RenderDataExtraction();
        
        std::cout << "\n=== ALL ISOLATION TESTS COMPLETE ===" << std::endl;
    }
    
private:
    void test1_VoxelStorage() {
        std::cout << "\nTest 1: Direct Voxel Storage" << std::endl;
        
        octree::OctreeNode node(glm::vec3(0, 0, 0), 100.0f, 0);
        
        // Set voxels directly
        node.voxels[0].material = octree::MaterialType::Water;
        node.voxels[1].material = octree::MaterialType::Rock;
        node.voxels[2].material = octree::MaterialType::Water;
        node.voxels[3].material = octree::MaterialType::Rock;
        
        // Verify they're stored
        assert(node.voxels[0].material == octree::MaterialType::Water);
        assert(node.voxels[1].material == octree::MaterialType::Rock);
        assert(node.voxels[2].material == octree::MaterialType::Water);
        assert(node.voxels[3].material == octree::MaterialType::Rock);
        
        // Verify via getter
        const auto& voxels = node.getVoxels();
        assert(voxels[0].material == octree::MaterialType::Water);
        assert(voxels[1].material == octree::MaterialType::Rock);
        
        std::cout << "  ✓ Voxels store and retrieve correctly" << std::endl;
    }
    
    void test2_NodeVoxelPersistence() {
        std::cout << "\nTest 2: Node Voxel Persistence After Creation" << std::endl;
        
        // Create a small planet
        octree::OctreePlanet planet(1000.0f, 2);
        
        // Manually create a node and set its voxels
        auto testNode = std::make_unique<octree::OctreeNode>(glm::vec3(900, 0, 0), 50.0f, 0);
        testNode->isLeaf = true;
        
        // Set test pattern
        for (int i = 0; i < 8; i++) {
            testNode->voxels[i].material = (i % 2 == 0) ? 
                octree::MaterialType::Water : octree::MaterialType::Rock;
        }
        
        // Verify before adding to tree
        int waterCount = 0;
        for (const auto& voxel : testNode->voxels) {
            if (voxel.material == octree::MaterialType::Water) waterCount++;
        }
        assert(waterCount == 4);
        std::cout << "  ✓ Node has 4 water voxels before tree insertion" << std::endl;
        
        // Now check if the pattern persists after generation
        planet.generate(42);
        
        // The generate() call might overwrite our test - this is what we're testing!
        // We need to check if materials persist or get reset
    }
    
    void test3_TreeTraversal() {
        std::cout << "\nTest 3: Tree Traversal Material Preservation" << std::endl;
        
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(42);
        
        // Count materials directly by traversing tree
        int directWaterCount = 0;
        int directRockCount = 0;
        
        // Manual traversal
        std::function<void(octree::OctreeNode*)> traverse = [&](octree::OctreeNode* node) {
            if (!node) return;
            
            if (node->isLeaf) {
                for (const auto& voxel : node->voxels) {
                    if (voxel.material == octree::MaterialType::Water) directWaterCount++;
                    else if (voxel.material == octree::MaterialType::Rock) directRockCount++;
                }
            } else {
                for (auto& child : node->children) {
                    if (child) traverse(child.get());
                }
            }
        };
        
        // Start traversal from root (assuming we can access it)
        // This is where we need to check the actual tree structure
        
        std::cout << "  Direct traversal: " << directWaterCount << " water, " 
                  << directRockCount << " rock" << std::endl;
    }
    
    void test4_RenderDataExtraction() {
        std::cout << "\nTest 4: RenderData Extraction" << std::endl;
        
        octree::OctreePlanet planet(1000.0f, 3);
        planet.generate(42);
        
        // Get render data
        auto renderData = planet.prepareRenderData(
            glm::vec3(2000, 2000, 2000),
            glm::mat4(1.0f)
        );
        
        // Count materials in render data
        int waterInRenderData = 0;
        int rockInRenderData = 0;
        
        for (const auto& voxel : renderData.voxels) {
            if (voxel.material == octree::MaterialType::Water) waterInRenderData++;
            else if (voxel.material == octree::MaterialType::Rock) rockInRenderData++;
        }
        
        std::cout << "  RenderData: " << waterInRenderData << " water, " 
                  << rockInRenderData << " rock out of " << renderData.voxels.size() << " voxels" << std::endl;
        
        if (waterInRenderData == 0 && rockInRenderData > 0) {
            std::cout << "  ❌ FOUND THE BUG: RenderData has no water!" << std::endl;
            
            // Now check the nodes directly
            std::cout << "  Checking nodes in renderData.nodes:" << std::endl;
            int nodeWaterCount = 0;
            for (size_t i = 0; i < renderData.nodes.size(); i++) {
                const auto& node = renderData.nodes[i];
                if (node.flags & 1) { // Is leaf
                    // The node should have voxel data
                    std::cout << "    Node " << i << " is leaf at voxelIndex " << node.voxelIndex << std::endl;
                    if (node.voxelIndex != 0xFFFFFFFF && node.voxelIndex < renderData.voxels.size()) {
                        // Check the 8 voxels for this node
                        for (int v = 0; v < 8 && node.voxelIndex + v < renderData.voxels.size(); v++) {
                            auto mat = renderData.voxels[node.voxelIndex + v].material;
                            if (mat == octree::MaterialType::Water) {
                                nodeWaterCount++;
                                std::cout << "      Found water at voxel " << (node.voxelIndex + v) << std::endl;
                            }
                        }
                    }
                }
            }
            std::cout << "  Total water voxels found via nodes: " << nodeWaterCount << std::endl;
        }
    }
};

int main() {
    MaterialIsolationTest test;
    test.runAll();
    return 0;
}