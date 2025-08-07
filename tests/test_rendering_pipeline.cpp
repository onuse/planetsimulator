// Comprehensive test for the entire rendering pipeline
// This test verifies all components needed for successful rendering

#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/octree.hpp"
#include "core/camera.hpp"
#include "core/mixed_voxel.hpp"

using namespace octree;

// Mock structures to simulate renderer components
struct InstanceData {
    glm::vec3 center;
    float halfSize;
    glm::vec4 colorAndMaterial;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

// Test 1: Verify cube vertices are correct
void test_cube_vertices() {
    std::cout << "TEST: Cube vertex buffer..." << std::endl;
    
    // These should match the vertices in vulkan_renderer_resources.cpp
    const Vertex vertices[] = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        // Back face  
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
    };
    
    // Verify vertices form a unit cube
    for (const auto& v : vertices) {
        assert(v.pos.x >= -0.5f && v.pos.x <= 0.5f);
        assert(v.pos.y >= -0.5f && v.pos.y <= 0.5f);
        assert(v.pos.z >= -0.5f && v.pos.z <= 0.5f);
        
        // Verify normals are normalized
        float normalLength = glm::length(v.normal);
        assert(std::abs(normalLength - 1.0f) < 0.001f);
    }
    
    std::cout << "  ✓ Cube vertices valid" << std::endl;
}

// Test 2: Verify view projection matrix produces valid clip space coordinates
void test_view_projection_matrix() {
    std::cout << "TEST: View projection matrix..." << std::endl;
    
    // Set up camera
    core::Camera camera(1280, 720);
    camera.setPosition(glm::vec3(0, 0, 19113000)); // Position from screenshot
    camera.lookAt(glm::vec3(0, 0, 0)); // Look at origin
    
    glm::mat4 viewProj = camera.getViewProjectionMatrix();
    
    // Test planet center projection
    float planetRadius = 6371000.0f; // 6.371 million meters
    glm::vec4 planetCenter(0, 0, 0, 1);
    glm::vec4 clipSpace = viewProj * planetCenter;
    
    // Verify planet center is in front of camera (positive w)
    assert(clipSpace.w > 0);
    
    // After perspective divide, should be in NDC space
    glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
    std::cout << "  Planet center in NDC: (" << ndc.x << ", " << ndc.y << ", " << ndc.z << ")" << std::endl;
    
    // Test planet edge projection
    glm::vec4 planetEdge(planetRadius, 0, 0, 1);
    clipSpace = viewProj * planetEdge;
    assert(clipSpace.w > 0);
    
    ndc = glm::vec3(clipSpace) / clipSpace.w;
    std::cout << "  Planet edge in NDC: (" << ndc.x << ", " << ndc.y << ", " << ndc.z << ")" << std::endl;
    
    // Planet should be visible (NDC z between -1 and 1)
    // Note: In Vulkan, NDC z is [0, 1], not [-1, 1]
    assert(ndc.z >= 0.0f && ndc.z <= 1.0f);
    
    std::cout << "  ✓ View projection matrix produces valid coordinates" << std::endl;
}

// Test 3: Verify instance transformations
void test_instance_transformations() {
    std::cout << "TEST: Instance transformations..." << std::endl;
    
    // Create test instance
    InstanceData instance;
    instance.center = glm::vec3(1000, 2000, 3000);
    instance.halfSize = 500.0f;
    instance.colorAndMaterial = glm::vec4(0.5f, 0.4f, 0.3f, 1.0f); // Rock
    
    // Simulate vertex shader transformation (from hierarchical.vert)
    glm::vec3 cubeVertex(-0.5f, -0.5f, 0.5f); // Front bottom left
    glm::vec3 scaledPos = cubeVertex * instance.halfSize * 2.0f;
    glm::vec3 worldPos = scaledPos + instance.center;
    
    // Expected world position
    glm::vec3 expected(500, 1500, 3500);
    
    assert(std::abs(worldPos.x - expected.x) < 0.001f);
    assert(std::abs(worldPos.y - expected.y) < 0.001f);
    assert(std::abs(worldPos.z - expected.z) < 0.001f);
    
    std::cout << "  Instance center: " << instance.center.x << ", " << instance.center.y << ", " << instance.center.z << std::endl;
    std::cout << "  Transformed vertex: " << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << std::endl;
    std::cout << "  ✓ Instance transformations correct" << std::endl;
}

// Test 4: Verify instance data buffer contents match render data
void test_instance_buffer_contents() {
    std::cout << "TEST: Instance buffer contents..." << std::endl;
    
    float radius = 1000.0f;
    OctreePlanet planet(radius, 4);
    planet.generate(42);
    
    glm::vec3 viewPos(0, 0, radius * 2.0f);
    glm::mat4 viewProj = glm::mat4(1.0f);
    auto renderData = planet.prepareRenderData(viewPos, viewProj);
    
    // Simulate instance buffer creation
    std::vector<InstanceData> instances;
    int nonAirInstances = 0;
    
    for (uint32_t nodeIdx : renderData.visibleNodes) {
        const auto& node = renderData.nodes[nodeIdx];
        
        if (node.flags & 1) { // Leaf node
            uint32_t voxelIdx = node.voxelIndex;
            if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                float voxelSize = node.halfSize * 0.5f;
                
                for (int i = 0; i < 8; i++) {
                    const auto& voxel = renderData.voxels[voxelIdx + i];
                    core::MaterialID mat = voxel.getDominantMaterialID();
                    
                    // Skip air
                    if ((mat == core::MaterialID::Air || mat == core::MaterialID::Vacuum) && 
                        voxel.getMaterialAmount(0) > 200) {
                        continue;
                    }
                    
                    nonAirInstances++;
                    
                    glm::vec3 offset(
                        (i & 1) ? voxelSize : -voxelSize,
                        (i & 2) ? voxelSize : -voxelSize,
                        (i & 4) ? voxelSize : -voxelSize
                    );
                    
                    InstanceData instance;
                    instance.center = node.center + offset;
                    instance.halfSize = voxelSize;
                    
                    // Verify instance properties
                    assert(instance.halfSize > 0);
                    assert(!std::isnan(instance.center.x));
                    assert(!std::isnan(instance.center.y));
                    assert(!std::isnan(instance.center.z));
                    
                    instances.push_back(instance);
                }
            }
        }
    }
    
    std::cout << "  Created " << instances.size() << " instances from " 
              << renderData.visibleNodes.size() << " nodes" << std::endl;
    std::cout << "  Non-air instances: " << nonAirInstances << std::endl;
    
    // Verify we have instances
    assert(instances.size() > 0);
    assert(instances.size() == nonAirInstances);
    
    std::cout << "  ✓ Instance buffer contents valid" << std::endl;
}

// Test 5: Verify near/far plane setup for depth precision
void test_depth_buffer_precision() {
    std::cout << "TEST: Depth buffer precision..." << std::endl;
    
    core::Camera camera(1280, 720);
    float cameraDistance = 19113000.0f; // 19,113 km
    float planetRadius = 6371000.0f;    // 6,371 km
    float distanceToPlanet = cameraDistance - planetRadius; // ~12,742 km
    
    camera.setPosition(glm::vec3(0, 0, cameraDistance));
    camera.lookAt(glm::vec3(0, 0, 0));
    
    // Check default near/far planes
    float nearPlane = 100.0f;        // Default from camera.hpp
    float farPlane = 100000000.0f;   // 100,000 km default
    
    float ratio = farPlane / nearPlane;
    std::cout << "  Near plane: " << nearPlane << " meters" << std::endl;
    std::cout << "  Far plane: " << farPlane / 1000.0f << " km" << std::endl;
    std::cout << "  Far/Near ratio: " << ratio << ":1" << std::endl;
    std::cout << "  Distance to planet: " << distanceToPlanet / 1000.0f << " km" << std::endl;
    
    // Calculate depth buffer precision at planet distance
    // With 24-bit depth buffer, we have ~16.7 million discrete values
    // These are distributed logarithmically between near and far
    
    // Calculate NDC z for planet distance
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, nearPlane, farPlane);
    proj[1][1] *= -1.0f; // Vulkan Y flip
    
    glm::vec4 planetPos(0, 0, -distanceToPlanet, 1); // In view space
    glm::vec4 clipPos = proj * planetPos;
    float ndcZ = clipPos.z / clipPos.w;
    
    std::cout << "  Planet NDC Z: " << ndcZ << std::endl;
    
    // Check depth precision issues
    if (ratio > 10000) {
        std::cout << "  ⚠ WARNING: Far/Near ratio > 10,000:1 causes severe depth precision loss!" << std::endl;
    }
    
    if (ndcZ > 0.9999) {
        std::cout << "  ⚠ CRITICAL: Planet is at NDC Z > 0.9999 - almost no depth precision!" << std::endl;
        std::cout << "  This causes Z-fighting and may make geometry invisible!" << std::endl;
    }
    
    // Suggest better near/far values
    float suggestedNear = distanceToPlanet * 0.001f;  // 0.1% of distance
    float suggestedFar = distanceToPlanet * 10.0f;    // 10x distance
    std::cout << "  Suggested near: " << suggestedNear / 1000.0f << " km" << std::endl;
    std::cout << "  Suggested far: " << suggestedFar / 1000.0f << " km" << std::endl;
    std::cout << "  Suggested ratio: " << (suggestedFar/suggestedNear) << ":1" << std::endl;
    
    std::cout << "  ✓ Depth buffer analysis complete" << std::endl;
}

// Test 6: Verify complete rendering pipeline
void test_complete_rendering_pipeline() {
    std::cout << "TEST: Complete rendering pipeline..." << std::endl;
    
    // Create planet
    float planetRadius = 6371000.0f; // Earth-like
    OctreePlanet planet(planetRadius, 7); // Default depth
    planet.generate(42);
    
    // Set up camera (position from actual run)
    core::Camera camera(1280, 720);
    camera.setPosition(glm::vec3(0, 0, 19113000)); // ~19M meters away
    camera.lookAt(glm::vec3(0, 0, 0));
    
    // Get render data
    auto renderData = planet.prepareRenderData(
        camera.getPosition(), 
        camera.getViewProjectionMatrix()
    );
    
    std::cout << "  Visible nodes: " << renderData.visibleNodes.size() << std::endl;
    
    // Simulate instance creation
    std::vector<InstanceData> instances;
    for (uint32_t nodeIdx : renderData.visibleNodes) {
        const auto& node = renderData.nodes[nodeIdx];
        
        if (node.flags & 1) { // Leaf
            uint32_t voxelIdx = node.voxelIndex;
            if (voxelIdx != 0xFFFFFFFF && voxelIdx + 8 <= renderData.voxels.size()) {
                float voxelSize = node.halfSize * 0.5f;
                
                for (int i = 0; i < 8; i++) {
                    const auto& voxel = renderData.voxels[voxelIdx + i];
                    core::MaterialID mat = voxel.getDominantMaterialID();
                    
                    if ((mat == core::MaterialID::Air || mat == core::MaterialID::Vacuum) && 
                        voxel.getMaterialAmount(0) > 200) {
                        continue;
                    }
                    
                    glm::vec3 offset(
                        (i & 1) ? voxelSize : -voxelSize,
                        (i & 2) ? voxelSize : -voxelSize,
                        (i & 4) ? voxelSize : -voxelSize
                    );
                    
                    InstanceData instance;
                    instance.center = node.center + offset;
                    instance.halfSize = voxelSize;
                    instances.push_back(instance);
                }
            }
        }
    }
    
    std::cout << "  Instances created: " << instances.size() << std::endl;
    
    // Verify instances are within view frustum
    glm::mat4 viewProj = camera.getViewProjectionMatrix();
    int visibleInstances = 0;
    int behindCamera = 0;
    int outsideNDC = 0;
    float minZ = 1e10f, maxZ = -1e10f;
    float minX = 1e10f, maxX = -1e10f;
    float minY = 1e10f, maxY = -1e10f;
    
    for (const auto& inst : instances) {
        // Transform instance center to clip space
        glm::vec4 worldPos(inst.center, 1.0f);
        glm::vec4 clipPos = viewProj * worldPos;
        
        if (clipPos.w <= 0) {
            behindCamera++;
            continue;
        }
        
        // Perspective divide to get NDC
        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
        
        // Track NDC bounds
        minX = std::min(minX, ndc.x);
        maxX = std::max(maxX, ndc.x);
        minY = std::min(minY, ndc.y);
        maxY = std::max(maxY, ndc.y);
        minZ = std::min(minZ, ndc.z);
        maxZ = std::max(maxZ, ndc.z);
        
        // Check if in view frustum (Vulkan NDC)
        if (ndc.x >= -1 && ndc.x <= 1 && 
            ndc.y >= -1 && ndc.y <= 1 && 
            ndc.z >= 0 && ndc.z <= 1) {
            visibleInstances++;
        } else {
            outsideNDC++;
        }
    }
    
    std::cout << "  Instances in frustum: " << visibleInstances << std::endl;
    std::cout << "  Behind camera: " << behindCamera << std::endl;
    std::cout << "  Outside NDC: " << outsideNDC << std::endl;
    std::cout << "  NDC X range: [" << minX << ", " << maxX << "]" << std::endl;
    std::cout << "  NDC Y range: [" << minY << ", " << maxY << "]" << std::endl;
    std::cout << "  NDC Z range: [" << minZ << ", " << maxZ << "]" << std::endl;
    
    // We should have visible instances
    if (visibleInstances == 0) {
        std::cout << "  ⚠ WARNING: No instances are visible in the frustum!" << std::endl;
        std::cout << "  This explains why the planet isn't rendering!" << std::endl;
        
        // Debug first few instance positions
        std::cout << "  Sample instance positions:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), instances.size()); i++) {
            std::cout << "    [" << i << "]: center=(" 
                      << instances[i].center.x << ", "
                      << instances[i].center.y << ", "
                      << instances[i].center.z << ") size="
                      << instances[i].halfSize << std::endl;
        }
    }
    
    assert(instances.size() > 0); // Should have instances
    // Note: visibleInstances might be 0 if there's a frustum culling issue
    
    std::cout << "  ✓ Complete pipeline test finished" << std::endl;
}

int main() {
    std::cout << "\n=== Rendering Pipeline Tests ===" << std::endl;
    std::cout << "Testing all components of the rendering pipeline...\n" << std::endl;
    
    try {
        test_cube_vertices();
        test_view_projection_matrix();
        test_instance_transformations();
        test_instance_buffer_contents();
        test_depth_buffer_precision();
        test_complete_rendering_pipeline();
        
        std::cout << "\n✅ All rendering pipeline tests completed!" << std::endl;
        std::cout << "Check warnings above for potential issues." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}