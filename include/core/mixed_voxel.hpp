#pragma once

#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>

namespace octree {

// Feature types for LOD preservation
enum class FeatureType : uint8_t {
    GENERIC = 0,
    MOUNTAIN_PEAK = 1,
    MOUNTAIN_SLOPE = 2,
    VALLEY = 3,
    OCEAN_DEEP = 4,
    OCEAN_SHALLOW = 5,
    COAST = 6,
    RIVER = 7,
    PLATEAU = 8
};

// Compact mixed material representation (8 bytes)
struct MixedVoxel {
    // Material composition (4 bytes)
    // Values are normalized: actual = (stored / 255.0)
    uint8_t rock;      // Igneous, metamorphic, sedimentary
    uint8_t water;     // Liquid water
    uint8_t air;       // Atmosphere/empty
    uint8_t sediment;  // Loose material (sand, soil)
    
    // Physical properties (2 bytes)
    uint8_t temperature;  // 0-255 mapped to real temperature
    uint8_t pressure;     // 0-255 mapped to pressure/depth
    
    // Metadata (2 bytes)
    uint8_t age;         // Geological age
    uint8_t flags;       // Bit flags for special properties
    
    // Constructors
    MixedVoxel() : rock(0), water(0), air(255), sediment(0),
                   temperature(128), pressure(128), age(0), flags(0) {}
    
    // Create pure material
    static MixedVoxel createPure(uint8_t rockAmt, uint8_t waterAmt, uint8_t airAmt) {
        MixedVoxel v;
        v.rock = rockAmt;
        v.water = waterAmt;
        v.air = airAmt;
        v.sediment = 0;
        return v;
    }
    
    // Get dominant material
    uint8_t getDominantMaterial() const {
        uint8_t maxVal = std::max({rock, water, air, sediment});
        if (rock == maxVal) return 1;  // Rock
        if (water == maxVal) return 2; // Water
        if (air == maxVal) return 0;   // Air
        return 3; // Sediment
    }
    
    // Get material percentages
    float getRockPercent() const { return rock / 255.0f; }
    float getWaterPercent() const { return water / 255.0f; }
    float getAirPercent() const { return air / 255.0f; }
    float getSedimentPercent() const { return sediment / 255.0f; }
    
    // Blend two voxels
    static MixedVoxel blend(const MixedVoxel& a, const MixedVoxel& b, float t) {
        MixedVoxel result;
        
        auto lerp = [](uint8_t v1, uint8_t v2, float t) -> uint8_t {
            return static_cast<uint8_t>(v1 * (1.0f - t) + v2 * t);
        };
        
        result.rock = lerp(a.rock, b.rock, t);
        result.water = lerp(a.water, b.water, t);
        result.air = lerp(a.air, b.air, t);
        result.sediment = lerp(a.sediment, b.sediment, t);
        result.temperature = lerp(a.temperature, b.temperature, t);
        result.pressure = lerp(a.pressure, b.pressure, t);
        result.age = std::max(a.age, b.age);  // Keep older
        result.flags = a.flags | b.flags;     // Combine flags
        
        return result;
    }
    
    // Get interpolated color for rendering
    glm::vec3 getColor() const {
        // Define material colors
        const glm::vec3 ROCK_COLOR(0.5f, 0.4f, 0.3f);      // Brown
        const glm::vec3 WATER_COLOR(0.0f, 0.3f, 0.7f);     // Blue
        const glm::vec3 AIR_COLOR(0.8f, 0.9f, 1.0f);       // Light blue
        const glm::vec3 SEDIMENT_COLOR(0.8f, 0.7f, 0.5f);  // Sandy
        
        // Calculate total for normalization
        float total = rock + water + air + sediment;
        if (total == 0) return AIR_COLOR;  // Empty voxel
        
        // Blend colors based on composition
        glm::vec3 color(0.0f);
        color += ROCK_COLOR * (rock / total);
        color += WATER_COLOR * (water / total);
        color += AIR_COLOR * (air / total);
        color += SEDIMENT_COLOR * (sediment / total);
        
        // Apply temperature tint (cold = blue, hot = red)
        float temp_norm = temperature / 255.0f;
        if (temp_norm < 0.5f) {
            // Cold: add blue
            color = glm::mix(color, glm::vec3(0.5f, 0.7f, 1.0f), 
                           (0.5f - temp_norm) * 0.3f);
        } else {
            // Hot: add red
            color = glm::mix(color, glm::vec3(1.0f, 0.5f, 0.3f), 
                           (temp_norm - 0.5f) * 0.3f);
        }
        
        return color;
    }
};

// Hierarchical averaging for LOD
class VoxelAverager {
public:
    // Average 8 child voxels into parent, preserving features
    static MixedVoxel average(const MixedVoxel children[8]) {
        // First, detect the dominant feature
        FeatureType feature = detectFeature(children);
        
        // Average based on feature type
        switch(feature) {
            case FeatureType::MOUNTAIN_PEAK:
                return averageMountain(children);
            case FeatureType::OCEAN_DEEP:
                return averageOcean(children);
            case FeatureType::COAST:
                return averageCoast(children);
            default:
                return averageGeneric(children);
        }
    }
    
private:
    // Detect what kind of feature these voxels represent
    static FeatureType detectFeature(const MixedVoxel children[8]) {
        int rockDominant = 0;
        int waterDominant = 0;
        int mixed = 0;
        
        for(int i = 0; i < 8; i++) {
            if(children[i].rock > 200) rockDominant++;
            else if(children[i].water > 200) waterDominant++;
            else if(children[i].rock > 100 && children[i].water > 100) mixed++;
        }
        
        if(rockDominant >= 6) return FeatureType::MOUNTAIN_PEAK;
        if(waterDominant >= 6) return FeatureType::OCEAN_DEEP;
        if(mixed >= 3) return FeatureType::COAST;
        
        return FeatureType::GENERIC;
    }
    
    // Preserve mountain peaks (don't average away heights)
    static MixedVoxel averageMountain(const MixedVoxel children[8]) {
        MixedVoxel result;
        
        // For mountains, preserve maximum rock
        uint16_t maxRock = 0;
        uint16_t totalWater = 0;
        uint16_t totalAir = 0;
        
        for(int i = 0; i < 8; i++) {
            maxRock = std::max(maxRock, (uint16_t)children[i].rock);
            totalWater += children[i].water;
            totalAir += children[i].air;
        }
        
        result.rock = std::min(255, (int)maxRock);
        result.water = totalWater / 8;
        result.air = totalAir / 8;
        
        // Average other properties normally
        averageProperties(result, children);
        
        return result;
    }
    
    // Preserve ocean depths
    static MixedVoxel averageOcean(const MixedVoxel children[8]) {
        MixedVoxel result;
        
        // For oceans, preserve maximum water
        uint16_t maxWater = 0;
        uint16_t totalRock = 0;
        uint16_t totalAir = 0;
        
        for(int i = 0; i < 8; i++) {
            maxWater = std::max(maxWater, (uint16_t)children[i].water);
            totalRock += children[i].rock;
            totalAir += children[i].air;
        }
        
        result.water = std::min(255, (int)maxWater);
        result.rock = totalRock / 8;
        result.air = totalAir / 8;
        
        averageProperties(result, children);
        
        return result;
    }
    
    // Blend coastlines naturally
    static MixedVoxel averageCoast(const MixedVoxel children[8]) {
        // For coasts, do standard averaging to create smooth transitions
        return averageGeneric(children);
    }
    
    // Standard averaging
    static MixedVoxel averageGeneric(const MixedVoxel children[8]) {
        MixedVoxel result;
        
        uint16_t totalRock = 0, totalWater = 0, totalAir = 0, totalSediment = 0;
        
        for(int i = 0; i < 8; i++) {
            totalRock += children[i].rock;
            totalWater += children[i].water;
            totalAir += children[i].air;
            totalSediment += children[i].sediment;
        }
        
        result.rock = totalRock / 8;
        result.water = totalWater / 8;
        result.air = totalAir / 8;
        result.sediment = totalSediment / 8;
        
        averageProperties(result, children);
        
        return result;
    }
    
    // Average physical properties
    static void averageProperties(MixedVoxel& result, const MixedVoxel children[8]) {
        uint16_t totalTemp = 0, totalPressure = 0;
        uint8_t maxAge = 0;
        uint8_t combinedFlags = 0;
        
        for(int i = 0; i < 8; i++) {
            totalTemp += children[i].temperature;
            totalPressure += children[i].pressure;
            maxAge = std::max(maxAge, children[i].age);
            combinedFlags |= children[i].flags;
        }
        
        result.temperature = totalTemp / 8;
        result.pressure = totalPressure / 8;
        result.age = maxAge;  // Keep oldest
        result.flags = combinedFlags;  // Combine all flags
    }
};

} // namespace octree