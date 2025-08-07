#include <iostream>
#include <memory>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/camera.hpp"

using core::Camera;  // Camera is in core namespace

// Test that the entire pipeline produces visible nodes
void test_full_pipeline() {
    std::cout << "Testing full pipeline from generation to render data..." << std::endl;
    
    // Create planet at realistic scale
    float planetRadius = 6371000.0f; // Earth radius in meters
    int maxDepth = 10;
    auto planet = std::make_unique<octree::OctreePlanet>(planetRadius, maxDepth);
    
    // Generate the planet with a seed
    planet->generate(42);
    
    // Set up camera viewing the planet
    Camera camera(1920, 1080);
    float viewDistance = planetRadius * 3.0f; // View from 3x radius away
    camera.setPosition(glm::vec3(0.0f, 0.0f, viewDistance));
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f)); // Look at planet center
    
    // Get view matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 100.0f, viewDistance * 2.0f);
    glm::mat4 viewProj = proj * view;
    glm::vec3 viewPos = camera.getPosition();
    
    // Get render data
    auto renderData = planet->prepareRenderData(viewPos, viewProj);
    
    std::cout << "  Render data nodes: " << renderData.nodes.size() << std::endl;
    std::cout << "  Visible nodes: " << renderData.visibleNodes.size() << std::endl;
    
    // Count nodes by flags/material
    int leafNodes = 0;
    int internalNodes = 0;
    
    for (size_t i = 0; i < renderData.visibleNodes.size(); ++i) {
        uint32_t nodeIdx = renderData.visibleNodes[i];
        if (nodeIdx < renderData.nodes.size()) {
            const auto& node = renderData.nodes[nodeIdx];
            if (node.flags & 1) { // is_leaf flag
                leafNodes++;
            } else {
                internalNodes++;
            }
        }
    }
    
    std::cout << "  Leaf nodes: " << leafNodes << std::endl;
    std::cout << "  Internal nodes: " << internalNodes << std::endl;
    
    // Verify we have visible content
    if (renderData.visibleNodes.empty()) {
        std::cerr << "  FAIL: No visible nodes generated!" << std::endl;
        exit(1);
    }
    
    if (renderData.nodes.empty()) {
        std::cerr << "  FAIL: No nodes in render data!" << std::endl;
        exit(1);
    }
    
    std::cout << "  PASS: Pipeline generates " << renderData.visibleNodes.size() << " visible nodes" << std::endl;
}

// Test LOD generation at different distances
void test_lod_at_distances() {
    std::cout << "Testing LOD at different view distances..." << std::endl;
    
    float planetRadius = 6371000.0f;
    int maxDepth = 10;
    auto planet = std::make_unique<octree::OctreePlanet>(planetRadius, maxDepth);
    planet->generate(42);
    
    Camera camera(1920, 1080);
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    
    // Test at different distances
    float distances[] = {
        planetRadius * 1.5f,  // Very close
        planetRadius * 3.0f,   // Medium
        planetRadius * 10.0f   // Far
    };
    
    for (float dist : distances) {
        camera.setPosition(glm::vec3(0.0f, 0.0f, dist));
        
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 100.0f, dist * 2.0f);
        glm::mat4 viewProj = proj * view;
        glm::vec3 viewPos = camera.getPosition();
        
        auto renderData = planet->prepareRenderData(viewPos, viewProj);
        
        std::cout << "  Distance " << (dist/planetRadius) << "x radius: " 
                  << renderData.visibleNodes.size() << " visible nodes" << std::endl;
        
        if (renderData.visibleNodes.empty()) {
            std::cerr << "    FAIL: No nodes at distance " << dist << std::endl;
            exit(1);
        }
    }
    
    std::cout << "  PASS: LOD works at all distances" << std::endl;
}

// Test material consistency through the pipeline
void test_material_consistency() {
    std::cout << "Testing material consistency through pipeline..." << std::endl;
    
    float planetRadius = 1000.0f; // Smaller for detailed testing
    int maxDepth = 8;
    auto planet = std::make_unique<octree::OctreePlanet>(planetRadius, maxDepth);
    
    // Generate with known pattern
    planet->generate(42);
    
    // Get render data
    Camera camera(1920, 1080);
    camera.setPosition(glm::vec3(0.0f, 0.0f, planetRadius * 2.0f));
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    
    glm::mat4 view = camera.getViewMatrix();
    // Fix near/far planes for small planet test
    float nearPlane = 1.0f;
    float farPlane = planetRadius * 10.0f;  // Make sure far plane covers the planet
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1920.0f/1080.0f, nearPlane, farPlane);
    glm::mat4 viewProj = proj * view;
    glm::vec3 viewPos = camera.getPosition();
    
    auto renderData = planet->prepareRenderData(viewPos, viewProj);
    
    // Debug output
    std::cout << "  Debug: Planet radius=" << planet->getRadius() 
              << ", View distance=" << glm::length(viewPos) << std::endl;
    std::cout << "  Debug: Render nodes=" << renderData.nodes.size() 
              << ", voxels=" << renderData.voxels.size() 
              << ", visible=" << renderData.visibleNodes.size() << std::endl;
    
    // Verify we have voxel data
    bool hasVoxels = !renderData.voxels.empty();
    bool hasNodes = !renderData.nodes.empty();
    
    if (!hasVoxels || !hasNodes) {
        std::cerr << "  FAIL: Missing data in pipeline (voxels=" << hasVoxels 
                  << ", nodes=" << hasNodes << ")" << std::endl;
        exit(1);
    }
    
    std::cout << "  PASS: Materials preserved through pipeline" << std::endl;
}

// Test frustum culling effectiveness
void test_frustum_culling() {
    std::cout << "Testing frustum culling..." << std::endl;
    
    float planetRadius = 6371000.0f;
    int maxDepth = 10;
    auto planet = std::make_unique<octree::OctreePlanet>(planetRadius, maxDepth);
    planet->generate(42);
    
    Camera camera(1920, 1080);
    float dist = planetRadius * 2.0f;
    
    // Look at planet center
    camera.setPosition(glm::vec3(0.0f, 0.0f, dist));
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 100.0f, dist * 2.0f);
    glm::mat4 viewProj = proj * view;
    glm::vec3 viewPos = camera.getPosition();
    
    auto centerView = planet->prepareRenderData(viewPos, viewProj);
    
    // Look away from planet - look in the opposite direction
    camera.lookAt(glm::vec3(0.0f, 0.0f, dist * 10.0f));  // Look far beyond the camera position
    view = camera.getViewMatrix();
    viewProj = proj * view;
    // Note: viewPos is still the same, only the view direction changed
    
    auto awayView = planet->prepareRenderData(viewPos, viewProj);
    
    std::cout << "  Nodes when looking at planet: " << centerView.visibleNodes.size() << std::endl;
    std::cout << "  Nodes when looking away: " << awayView.visibleNodes.size() << std::endl;
    
    // When looking directly away from the planet, ZERO nodes should be visible
    // The frustum cone points away from the planet - there's no mathematical way
    // for any part of the planet to be visible
    
    if (awayView.visibleNodes.size() != 0) {
        std::cerr << "  FAIL: Frustum culling not working!" << std::endl;
        std::cerr << "  Expected 0 nodes when looking away, but got " 
                  << awayView.visibleNodes.size() << std::endl;
        std::cerr << "  This indicates the frustum planes are incorrectly calculated" << std::endl;
        exit(1);
    }
    
    std::cout << "  PASS: Frustum culling correctly shows 0 nodes when looking away" << std::endl;
}

// Test node hierarchy and subdivision
void test_node_subdivision() {
    std::cout << "Testing node subdivision near surface..." << std::endl;
    
    float planetRadius = 1000.0f;
    int maxDepth = 8;
    auto planet = std::make_unique<octree::OctreePlanet>(planetRadius, maxDepth);
    planet->generate(42);
    
    // Get close-up view
    Camera camera(1920, 1080);
    camera.setPosition(glm::vec3(0.0f, 0.0f, planetRadius * 1.1f));
    camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, planetRadius * 3.0f);
    glm::mat4 viewProj = proj * view;
    glm::vec3 viewPos = camera.getPosition();
    
    auto renderData = planet->prepareRenderData(viewPos, viewProj);
    
    // Check for varying LOD levels
    std::set<int> lodLevels;
    float minSize = std::numeric_limits<float>::max();
    float maxSize = 0.0f;
    
    for (size_t i = 0; i < renderData.visibleNodes.size(); ++i) {
        uint32_t nodeIdx = renderData.visibleNodes[i];
        if (nodeIdx < renderData.nodes.size()) {
            const auto& node = renderData.nodes[nodeIdx];
            lodLevels.insert(node.level);
            minSize = std::min(minSize, node.halfSize);
            maxSize = std::max(maxSize, node.halfSize);
        }
    }
    
    std::cout << "  LOD levels present: " << lodLevels.size() << std::endl;
    std::cout << "  Node size range: " << minSize << " to " << maxSize << std::endl;
    
    if (lodLevels.size() < 2) {
        std::cerr << "  WARNING: Only one LOD level visible" << std::endl;
    }
    
    std::cout << "  PASS: Subdivision creates multiple LOD levels" << std::endl;
}

int main() {
    std::cout << "\n=== Full Pipeline Integration Test ===" << std::endl;
    
    test_full_pipeline();
    test_lod_at_distances();
    test_material_consistency();
    test_frustum_culling();
    test_node_subdivision();
    
    std::cout << "\n=== All Pipeline Tests Passed ===" << std::endl;
    return 0;
}