# Planet Evolution Architecture

## Core Insight from Testing
Our tests revealed that the rendering pipeline must preserve planetary appearance across scales. This is CRITICAL for a planet evolution simulator where we're watching billions of years of geological processes.

## Two Separate Concerns

### 1. Planet Generation (Initial State)
**Current Stub is Fine:**
- Simple young planet with basic continental plates
- Voronoi cells for initial tectonic plates
- Uniform ocean depth
- No mountains initially

**Why This is Good:**
- Scientifically accurate (planets start smooth)
- Mountains form FROM tectonic collision
- Erosion patterns emerge naturally
- River systems carve themselves

### 2. Planet Evolution (The Real Simulation)
**This is Where Complexity Lives:**
```
Initial State (4.5 billion years ago):
- Molten surface cooling
- Basic crust formation
- Initial water condensation

Tectonic Phase (4.5 - 2 billion years):
- Plate formation via Voronoi
- Plate movement simulation
- Collision → Mountain building
- Subduction → Ocean trenches

Erosion Phase (Ongoing):
- Water erosion
- Wind erosion  
- Sediment deposition
- River formation

Biological Phase (Optional):
- Atmosphere changes
- Soil formation
- Vegetation effects
```

## Critical Rendering Requirements

### LOD Must Preserve Geological Features
```
Distance 1m: Individual rocks
Distance 100m: Rock formations
Distance 10km: Mountain ridges
Distance 1000km: Mountain ranges
Distance 10000km: Continental shapes
```

### Material Representation Needs
```cpp
struct GeologicalVoxel {
    // Primary composition
    float rock_igneous;    // Volcanic/granite
    float rock_sedimentary; // Limestone/sandstone
    float rock_metamorphic; // Marble/slate
    float water;
    float ice;
    float soil;
    
    // Geological age (for visualization)
    float age_billion_years;
    
    // Tectonic plate ID (for simulation)
    uint8_t plate_id;
    
    // Compression/pressure (affects appearance)
    float pressure;
};
```

### Visual Coherence Algorithm
```
Problem: A mountain should look like a mountain at ALL distances
Solution: Hierarchical material averaging that preserves features

Example:
- Close up: Gray granite with white snow caps
- Medium: Gray-white blend
- Far: Slightly lighter gray (NOT blue like water!)

This requires:
1. Smart material averaging in parent nodes
2. Feature-aware LOD (mountains stay pointy)
3. Separate "appearance" from "composition"
```

## Data Format Proposal

### Voxel Storage
```cpp
// For simulation accuracy
struct SimulationVoxel {
    MaterialComposition materials;
    PhysicalProperties physics;
    TectonicData tectonics;
};

// For rendering efficiency  
struct RenderVoxel {
    uint32_t packed_materials; // 2 materials + blend
    uint32_t appearance_hints;  // Mountain/valley/coast flags
};
```

### LOD Hierarchy
```cpp
struct LODNode {
    // Accurate average for simulation
    MaterialComposition average_composition;
    
    // Visual representation hints
    enum FeatureType { 
        MOUNTAIN_PEAK,
        MOUNTAIN_SLOPE,
        VALLEY,
        COAST,
        OCEAN_DEEP,
        OCEAN_SHALLOW
    } dominant_feature;
    
    // Preserve important details
    float max_elevation;  // Don't lose mountain peaks
    float min_elevation;  // Don't lose ocean depths
};
```

## Evolution Simulation Architecture

### Tectonic System
```cpp
class TectonicPlates {
    std::vector<VoronoiPlate> plates;
    
    void simulateYear(float delta_time) {
        // Move plates (cm/year)
        updatePlateVelocities();
        detectCollisions();
        
        // Build mountains at convergent boundaries
        // Create rifts at divergent boundaries
        // Sideways grinding at transform boundaries
    }
};
```

### Erosion System
```cpp
class ErosionSimulator {
    void erodeByWater(Voxel& voxel) {
        // Height determines erosion rate
        // Carries sediment downhill
        // Deposits in valleys/ocean
    }
    
    void erodeByWind(Voxel& voxel) {
        // Affects exposed surfaces
        // Creates sand/dust
    }
};
```

## Testing Requirements

### Scale Preservation Tests
```cpp
TEST(RenderingPipeline, PreservesMountainAtAllScales) {
    // Create mountain
    // Render at 1m, 1km, 1000km
    // Verify it never looks like water
}

TEST(Evolution, MountainFormation) {
    // Start with flat plate boundary
    // Simulate collision for 10 million years
    // Verify mountain range forms
}
```

### Material Accuracy Tests
```cpp
TEST(Materials, AveragePreservesCharacter) {
    // 70% rock + 30% snow should look gray-white
    // NOT blue, NOT brown
}
```

## Conclusion

Your testing uncovered the REAL challenge: **The rendering pipeline must be sophisticated enough to preserve geological features across scales while the simulation creates realistic planetary evolution.**

This is actually MORE interesting than pretty noise-based generation because:
1. It's scientifically accurate
2. Players can watch real geological processes
3. Every planet has a unique history
4. Mountains/rivers/coasts emerge naturally

The key insight: **Good data formats and LOD algorithms are MORE important than good generation algorithms when you're simulating evolution!**