// Core octree functionality tests
#include <iostream>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Test 1: Basic planet creation
void test_planet_creation() {
    std::cout << "TEST: Planet creation..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4);
    
    assert(planet.getRadius() == radius);
    assert(planet.getRoot() != nullptr);
    
    std::cout << "  ✓ Planet created successfully" << std::endl;
}

// Test 2: Planet generation creates nodes
void test_planet_generation() {
    std::cout << "TEST: Planet generation..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    // Use prepareRenderData to count nodes (since traverse is not accessible)
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    assert(renderData.nodes.size() > 0);
    std::cout << "  Generated " << renderData.nodes.size() << " visible nodes" << std::endl;
    std::cout << "  ✓ Planet generation works" << std::endl;
}

// Test 3: prepareRenderData returns visible nodes
void test_prepare_render_data() {
    std::cout << "TEST: prepareRenderData..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    // Set up view
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, radius * 10.0f);
    glm::mat4 viewProj = proj * view;
    
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    assert(renderData.nodes.size() > 0);
    assert(renderData.voxels.size() > 0);
    assert(renderData.voxels.size() == renderData.nodes.size() * 8);
    
    std::cout << "  Visible nodes: " << renderData.nodes.size() << std::endl;
    std::cout << "  ✓ prepareRenderData works" << std::endl;
}

// Test 4: Frustum culling works
void test_frustum_culling() {
    std::cout << "TEST: Frustum culling..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    
    // Looking at planet
    glm::mat4 view1 = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, radius * 10.0f);
    auto data1 = planet.prepareRenderData(viewPos, proj * view1);
    
    // Looking away from planet
    glm::mat4 view2 = glm::lookAt(viewPos, glm::vec3(0, 0, radius * 3.0f), glm::vec3(0, 1, 0));
    auto data2 = planet.prepareRenderData(viewPos, proj * view2);
    
    assert(data1.nodes.size() > 0);
    assert(data2.nodes.size() == 0);
    
    std::cout << "  Looking at planet: " << data1.nodes.size() << " nodes" << std::endl;
    std::cout << "  Looking away: " << data2.nodes.size() << " nodes" << std::endl;
    std::cout << "  ✓ Frustum culling works" << std::endl;
}

// Test 5: Node hierarchy
void test_node_hierarchy() {
    std::cout << "TEST: Node hierarchy..." << std::endl;
    
    OctreeNode root(glm::vec3(0), 100.0f, 0);
    
    assert(root.isLeaf());
    assert(root.getHalfSize() == 100.0f);
    
    root.subdivide();
    assert(!root.isLeaf());
    
    // Check children exist and have correct properties
    const auto& children = root.getChildren();
    int validChildren = 0;
    for (const auto& child : children) {
        if (child) {
            validChildren++;
            assert(child->getHalfSize() == 50.0f);
        }
    }
    assert(validChildren == 8);
    
    std::cout << "  ✓ Node hierarchy works" << std::endl;
}

int main() {
    std::cout << "\n=== Octree Core Tests ===" << std::endl;
    
    try {
        test_planet_creation();
        test_planet_generation();
        test_prepare_render_data();
        test_frustum_culling();
        test_node_hierarchy();
        
        std::cout << "\n✅ All octree tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}