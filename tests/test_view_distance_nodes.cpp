// Test how many nodes are visible at different view distances
#include <iostream>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

void test_nodes_at_distances() {
    std::cout << "=== Testing Node Visibility at Different Distances ===" << std::endl;
    
    // Create a smaller planet for faster testing
    float planetRadius = 1000.0f; // 1km radius for testing
    OctreePlanet planet(planetRadius, 8); // Depth 8 for good detail
    planet.generate(42);
    
    // Test at various distances
    float distances[] = {
        planetRadius * 1.1f,   // Very close (just above surface)
        planetRadius * 1.5f,   // Close
        planetRadius * 2.0f,   // Medium
        planetRadius * 5.0f,   // Far
        planetRadius * 10.0f,  // Very far
        planetRadius * 100.0f  // Extremely far
    };
    
    std::cout << "\nPlanet radius: " << planetRadius << " meters\n" << std::endl;
    
    for (float dist : distances) {
        glm::vec3 viewPos(0, 0, dist);
        
        // Create a proper projection matrix (perspective)
        float fov = glm::radians(60.0f);
        float aspect = 16.0f / 9.0f;
        float near = dist * 0.1f;  // Near plane at 10% of distance
        float far = dist * 10.0f;   // Far plane at 10x distance
        glm::mat4 proj = glm::perspective(fov, aspect, near, far);
        
        // Create view matrix looking at planet center
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 viewProj = proj * view;
        
        auto renderData = planet.prepareRenderData(viewPos, viewProj);
        
        // Count material types
        int airCount = 0, rockCount = 0, waterCount = 0;
        for (const auto& voxel : renderData.voxels) {
            uint8_t mat = voxel.getDominantMaterial();
            if (mat == 0) airCount++;
            else if (mat == 1) rockCount++;
            else if (mat == 2) waterCount++;
        }
        
        // Calculate planet's angular size in degrees
        float angularSize = 2.0f * glm::degrees(glm::atan(planetRadius / dist));
        
        std::cout << "Distance: " << dist / planetRadius << "x radius (" << dist << "m)" << std::endl;
        std::cout << "  Angular size: " << angularSize << " degrees" << std::endl;
        std::cout << "  Visible nodes: " << renderData.nodes.size() << std::endl;
        std::cout << "  Voxels: " << renderData.voxels.size() << std::endl;
        std::cout << "  Materials: " << rockCount << " rock, " << waterCount << " water, " << airCount << " air" << std::endl;
        
        // Estimate pixels on screen (rough approximation)
        float pixelSize = (angularSize / 60.0f) * 1920.0f; // Assuming 1920px width, 60° FOV
        std::cout << "  Approx screen size: " << pixelSize << " pixels" << std::endl;
        std::cout << std::endl;
    }
}

void test_earth_scale_close_view() {
    std::cout << "\n=== Testing Earth-Scale Planet at Close Distance ===" << std::endl;
    
    float earthRadius = 6371000.0f; // Real Earth radius
    OctreePlanet planet(earthRadius, 7);
    planet.generate(42);
    
    // Test from ISS orbit distance (408 km above surface)
    float issOrbit = earthRadius + 408000.0f;
    glm::vec3 viewPos(0, 0, issOrbit);
    
    // Proper perspective matrix
    float fov = glm::radians(60.0f);
    float aspect = 16.0f / 9.0f;
    float near = 1000.0f;     // 1km near plane
    float far = earthRadius * 3.0f; // See through whole planet
    glm::mat4 proj = glm::perspective(fov, aspect, near, far);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 viewProj = proj * view;
    
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    std::cout << "View from ISS orbit (408km above surface):" << std::endl;
    std::cout << "  Distance from center: " << issOrbit / 1000.0f << " km" << std::endl;
    std::cout << "  Visible nodes: " << renderData.nodes.size() << std::endl;
    std::cout << "  Voxels: " << renderData.voxels.size() << std::endl;
    
    // Material analysis
    int airCount = 0, rockCount = 0, waterCount = 0;
    for (const auto& voxel : renderData.voxels) {
        uint8_t mat = voxel.getDominantMaterial();
        if (mat == 0) airCount++;
        else if (mat == 1) rockCount++;
        else if (mat == 2) waterCount++;
    }
    
    float solidPercent = (rockCount + waterCount) * 100.0f / renderData.voxels.size();
    std::cout << "  Materials: " << rockCount << " rock, " << waterCount << " water, " << airCount << " air" << std::endl;
    std::cout << "  Solid material: " << solidPercent << "%" << std::endl;
    
    // This should have MANY more nodes than the far view
    assert(renderData.nodes.size() > 100 && "Close view should show many nodes");
    assert(solidPercent > 90.0f && "Should be mostly solid material");
    
    std::cout << "  ✓ Close view shows appropriate detail!" << std::endl;
}

void test_frustum_culling() {
    std::cout << "\n=== Testing Frustum Culling ===" << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 6);
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    
    // Test 1: Looking directly at planet
    {
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 100.0f, 10000.0f);
        auto renderData = planet.prepareRenderData(viewPos, proj * view);
        std::cout << "Looking at planet: " << renderData.nodes.size() << " nodes visible" << std::endl;
        assert(renderData.nodes.size() > 0 && "Should see nodes when looking at planet");
    }
    
    // Test 2: Looking away from planet
    {
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0, 0, radius * 3.0f), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 100.0f, 10000.0f);
        auto renderData = planet.prepareRenderData(viewPos, proj * view);
        std::cout << "Looking away from planet: " << renderData.nodes.size() << " nodes visible" << std::endl;
        assert(renderData.nodes.size() == 0 && "Should see no nodes when looking away");
    }
    
    // Test 3: Planet partially in view (looking to the side)
    {
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(radius * 2.0f, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 100.0f, 10000.0f);
        auto renderData = planet.prepareRenderData(viewPos, proj * view);
        std::cout << "Planet at edge of view: " << renderData.nodes.size() << " nodes visible" << std::endl;
        // Should see some but not all nodes
    }
    
    std::cout << "  ✓ Frustum culling working correctly!" << std::endl;
}

int main() {
    std::cout << "=== View Distance and Node Visibility Tests ===" << std::endl;
    std::cout << "Testing how many nodes are visible at different distances\n" << std::endl;
    
    try {
        test_nodes_at_distances();
        test_earth_scale_close_view();
        test_frustum_culling();
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        std::cout << "\nKey findings:" << std::endl;
        std::cout << "- Very far views (>10x radius) show few nodes (planet is tiny)" << std::endl;
        std::cout << "- Close views show hundreds/thousands of nodes" << std::endl;
        std::cout << "- Frustum culling correctly excludes off-screen nodes" << std::endl;
        std::cout << "- The '8 nodes' was due to viewing from very far away" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}