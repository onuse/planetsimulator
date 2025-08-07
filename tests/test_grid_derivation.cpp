#include <iostream>
#include <cmath>
#include "../include/core/octree.hpp"

// Test to derive proper grid size from planet properties
int main() {
    std::cout << "=== DERIVING PROPER GRID SIZE FROM PLANET PROPERTIES ===" << std::endl;
    
    // What is the grid size really representing?
    std::cout << "\n1. Understanding the Grid Purpose:" << std::endl;
    std::cout << "   - It defines continent/ocean feature size" << std::endl;
    std::cout << "   - It's essentially a 'continent resolution'" << std::endl;
    std::cout << "   - Should relate to the smallest resolvable feature" << std::endl;
    
    // Option 1: Derive from octree depth and node size
    std::cout << "\n2. Option 1: Derive from Octree Properties" << std::endl;
    auto deriveFromOctree = [](float radius, int maxDepth) {
        // Root node size
        float rootSize = radius * 1.5f;  // From OctreePlanet constructor
        
        // Leaf node size (smallest feature we can represent)
        float leafSize = rootSize / std::pow(2.0f, maxDepth);
        
        // Continent features should be larger than individual nodes
        // Maybe 10-100 leaf nodes per continent feature?
        float continentFeatureSize = leafSize * 50.0f;
        
        std::cout << "   Radius: " << radius/1e6 << " Mm" << std::endl;
        std::cout << "   Max depth: " << maxDepth << std::endl;
        std::cout << "   Root node size: " << rootSize/1e6 << " Mm" << std::endl;
        std::cout << "   Leaf node size: " << leafSize/1e3 << " km" << std::endl;
        std::cout << "   Suggested grid size: " << continentFeatureSize/1e3 << " km" << std::endl;
        
        return continentFeatureSize;
    };
    
    // Test at different scales
    deriveFromOctree(1000.0f, 3);
    deriveFromOctree(1000.0f, 7);
    deriveFromOctree(6.371e6f, 7);
    deriveFromOctree(6.371e6f, 10);
    
    // Option 2: Derive from planet circumference
    std::cout << "\n3. Option 2: Derive from Planet Circumference" << std::endl;
    auto deriveFromCircumference = [](float radius, int targetFeatures) {
        float circumference = 2.0f * 3.14159f * radius;
        float featureSize = circumference / targetFeatures;
        
        std::cout << "   Radius: " << radius/1e6 << " Mm" << std::endl;
        std::cout << "   Circumference: " << circumference/1e6 << " Mm" << std::endl;
        std::cout << "   Target features: " << targetFeatures << std::endl;
        std::cout << "   Feature size: " << featureSize/1e3 << " km" << std::endl;
        
        return featureSize;
    };
    
    // Earth-like: ~7 major continents + oceans = ~20-30 major features
    deriveFromCircumference(6.371e6f, 20);
    deriveFromCircumference(6.371e6f, 30);
    deriveFromCircumference(1000.0f, 20);
    
    // Option 3: Based on voxel resolution at surface
    std::cout << "\n4. Option 3: Based on Surface Voxel Resolution" << std::endl;
    auto deriveFromVoxelSize = [](float radius, int maxDepth) {
        // Each leaf node has 8 voxels (2x2x2)
        float rootSize = radius * 1.5f;
        float leafNodeSize = rootSize / std::pow(2.0f, maxDepth);
        float voxelSize = leafNodeSize / 2.0f;  // 2 voxels per node dimension
        
        // Continents should span multiple voxels
        // Maybe 20-50 voxels per continent feature
        float continentSize = voxelSize * 30.0f;
        
        std::cout << "   Radius: " << radius/1e6 << " Mm" << std::endl;
        std::cout << "   Voxel size: " << voxelSize/1e3 << " km" << std::endl;
        std::cout << "   Continent feature: " << continentSize/1e3 << " km" << std::endl;
        
        return continentSize;
    };
    
    deriveFromVoxelSize(6.371e6f, 7);
    deriveFromVoxelSize(6.371e6f, 10);
    
    // Actual Earth reference
    std::cout << "\n5. Real Earth Reference:" << std::endl;
    std::cout << "   Earth radius: 6371 km" << std::endl;
    std::cout << "   Typical continent width: 3000-7000 km" << std::endl;
    std::cout << "   Typical ocean width: 5000-15000 km" << std::endl;
    std::cout << "   Suggested feature size: ~2000-5000 km" << std::endl;
    
    // Recommendation
    std::cout << "\n6. RECOMMENDATION:" << std::endl;
    std::cout << "   gridSize = radius / 3.0f;  // Approximately 3 features across radius" << std::endl;
    std::cout << "   This gives:" << std::endl;
    std::cout << "   - Earth (6371km): " << 6371/3.0f << " km grid" << std::endl;
    std::cout << "   - Small (1km): " << 1.0f/3.0f << " km grid" << std::endl;
    std::cout << "\n   OR better yet, tie to octree structure:" << std::endl;
    std::cout << "   gridSize = (radius * 1.5f) / pow(2, maxDepth - 3);" << std::endl;
    std::cout << "   This ensures grid aligns with octree nodes!" << std::endl;
    
    return 0;
}