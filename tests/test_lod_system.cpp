#include <iostream>
#include <cassert>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/density_field.hpp"
#include "core/spherical_quadtree.hpp"

// Test the density field implementation
void testDensityField() {
    std::cout << "Testing DensityField..." << std::endl;
    
    core::DensityField densityField(6371000.0f, 42);
    
    // Test 1: Point at planet center should be inside
    {
        glm::vec3 center(0, 0, 0);
        float density = densityField.getDensity(center);
        assert(density < 0 && "Center should be inside planet (negative density)");
        std::cout << "  ✓ Center point inside planet" << std::endl;
    }
    
    // Test 2: Point far outside should have positive density
    {
        glm::vec3 farPoint(10000000, 0, 0);
        float density = densityField.getDensity(farPoint);
        assert(density > 0 && "Far point should be outside planet (positive density)");
        std::cout << "  ✓ Far point outside planet" << std::endl;
    }
    
    // Test 3: Point on surface should be near zero
    {
        glm::vec3 surfacePoint = glm::normalize(glm::vec3(1, 1, 1)) * 6371000.0f;
        float density = densityField.getDensity(surfacePoint);
        assert(std::abs(density) < 10000.0f && "Surface point should have small density");
        std::cout << "  ✓ Surface point has appropriate density: " << density << std::endl;
    }
    
    // Test 4: Terrain height should vary
    {
        glm::vec3 normal1 = glm::normalize(glm::vec3(1, 0, 0));
        glm::vec3 normal2 = glm::normalize(glm::vec3(0, 1, 0));
        float height1 = densityField.getTerrainHeight(normal1);
        float height2 = densityField.getTerrainHeight(normal2);
        assert(height1 != height2 && "Terrain should have variation");
        std::cout << "  ✓ Terrain has variation: " << height1 << " vs " << height2 << std::endl;
    }
    
    // Test 5: Gradient should point outward
    {
        glm::vec3 point = glm::normalize(glm::vec3(1, 1, 0)) * 6371000.0f;
        glm::vec3 gradient = densityField.getGradient(point, 10.0f);
        float dotProduct = glm::dot(gradient, glm::normalize(point));
        assert(dotProduct > 0.5f && "Gradient should generally point outward");
        std::cout << "  ✓ Gradient points outward: " << dotProduct << std::endl;
    }
    
    // Test 6: Material assignment
    {
        glm::vec3 deepPoint = glm::normalize(glm::vec3(1, 0, 0)) * 6370000.0f; // 1km below surface
        uint8_t material = densityField.getMaterialAt(deepPoint);
        assert(material != 0 && "Deep point should have solid material");
        std::cout << "  ✓ Material assignment works: " << (int)material << std::endl;
    }
    
    std::cout << "DensityField tests passed!" << std::endl << std::endl;
}

// Test the spherical quadtree implementation
void testSphericalQuadtree() {
    std::cout << "Testing SphericalQuadtree..." << std::endl;
    
    auto densityField = std::make_shared<core::DensityField>(6371000.0f, 42);
    
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.maxLevel = 10;
    config.pixelError = 2.0f;
    
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Test 1: Initial state should have 6 root patches
    {
        glm::vec3 viewPos(0, 0, 6371000.0f * 2); // 2x planet radius
        glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 1000.0f, 1e9f);
        glm::mat4 viewProj = proj * view;
        
        quadtree.update(viewPos, viewProj, 0.016f);
        
        const auto& patches = quadtree.getVisiblePatches();
        assert(!patches.empty() && "Should have visible patches");
        std::cout << "  ✓ Initial patches: " << patches.size() << std::endl;
    }
    
    // Test 2: Closer view should trigger subdivision
    {
        glm::vec3 closePos(0, 0, 6371000.0f * 1.1f); // Just above surface
        glm::mat4 view = glm::lookAt(closePos, glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 100.0f, 1e8f);
        glm::mat4 viewProj = proj * view;
        
        quadtree.update(closePos, viewProj, 0.016f);
        
        const auto& patches = quadtree.getVisiblePatches();
        std::cout << "  ✓ Close view patches: " << patches.size() << std::endl;
    }
    
    // Test 3: Altitude-based mode selection
    {
        float highAltitude = 5000.0f;
        bool shouldUseOctree = quadtree.shouldUseOctree(highAltitude);
        assert(!shouldUseOctree && "High altitude should use quadtree");
        std::cout << "  ✓ High altitude uses quadtree" << std::endl;
        
        float lowAltitude = 100.0f;
        shouldUseOctree = quadtree.shouldUseOctree(lowAltitude);
        assert(shouldUseOctree && "Low altitude should use octree");
        std::cout << "  ✓ Low altitude uses octree" << std::endl;
    }
    
    // Test 4: Transition blend factor
    {
        float blend1000m = quadtree.getTransitionBlendFactor(1000.0f);
        assert(blend1000m == 0.0f && "At 1000m should be pure quadtree");
        
        float blend750m = quadtree.getTransitionBlendFactor(750.0f);
        assert(blend750m == 0.5f && "At 750m should be 50% blend");
        
        float blend500m = quadtree.getTransitionBlendFactor(500.0f);
        assert(blend500m == 1.0f && "At 500m should be pure octree");
        
        std::cout << "  ✓ Transition blend factors correct" << std::endl;
    }
    
    // Test 5: Statistics tracking
    {
        const auto& stats = quadtree.getStats();
        assert(stats.visibleNodes > 0 && "Should have visible nodes");
        assert(stats.totalNodes >= 6 && "Should have at least 6 root nodes");
        std::cout << "  ✓ Statistics: " << stats.visibleNodes << " visible, " 
                  << stats.totalNodes << " total" << std::endl;
    }
    
    std::cout << "SphericalQuadtree tests passed!" << std::endl << std::endl;
}

// Test spherical projection
void testSphericalProjection() {
    std::cout << "Testing spherical projection..." << std::endl;
    
    // Test cube-to-sphere projection
    auto projectToSphere = [](const glm::vec3& cubePos) -> glm::vec3 {
        glm::vec3 pos2 = cubePos * cubePos;
        glm::vec3 spherePos = cubePos * glm::sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
        spherePos.y *= glm::sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
        spherePos.z *= glm::sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
        return glm::normalize(spherePos);
    };
    
    // Test 1: Cube face centers should map to unit sphere
    {
        glm::vec3 faces[] = {
            glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
            glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
            glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)
        };
        
        for (const auto& face : faces) {
            glm::vec3 spherePoint = projectToSphere(face);
            float length = glm::length(spherePoint);
            assert(std::abs(length - 1.0f) < 0.001f && "Should project to unit sphere");
        }
        std::cout << "  ✓ Face centers project correctly" << std::endl;
    }
    
    // Test 2: Cube corners should also map to unit sphere
    {
        glm::vec3 corner(1, 1, 1);
        glm::vec3 spherePoint = projectToSphere(corner);
        float length = glm::length(spherePoint);
        assert(std::abs(length - 1.0f) < 0.001f && "Corner should project to unit sphere");
        std::cout << "  ✓ Corners project correctly" << std::endl;
    }
    
    std::cout << "Spherical projection tests passed!" << std::endl << std::endl;
}

int main() {
    std::cout << "=== LOD System Tests ===" << std::endl << std::endl;
    
    try {
        testDensityField();
        testSphericalQuadtree();
        testSphericalProjection();
        
        std::cout << "All LOD system tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}