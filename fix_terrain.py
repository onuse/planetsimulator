#!/usr/bin/env python3
import sys

# Read the file
with open('src/core/octree.cpp', 'r') as f:
    content = f.read()

# Fix 1: Update the sampleImprovedTerrain function to have better amplitude
old_terrain = """    // Layer 1: Major continents (very low frequency)
    continents += sin((longitude + seedOffset) * 1.5f) * 0.3f;
    continents += cos((latitude * 2.0f + seedOffset * 2.0f) * 1.2f) * 0.3f;
    continents += sin((longitude * 0.8f - latitude * 1.1f + seedOffset * 3.0f)) * 0.2f;
    
    // Layer 2: Regional variations (medium frequency)
    float regional = 0.0f;
    regional += sin((longitude * 3.2f + seedOffset * 4.0f)) * cos(latitude * 2.8f) * 0.15f;
    regional += cos((longitude * 4.1f - seedOffset * 2.0f)) * sin(latitude * 3.5f) * 0.15f;
    
    // Layer 3: Mountain ridges using absolute value for ridge effect
    float ridges = 0.0f;
    float ridgeX = sin(longitude * 5.0f + latitude * 3.0f + seedOffset * 5.0f);
    float ridgeY = cos(longitude * 4.0f - latitude * 6.0f + seedOffset * 6.0f);
    ridges = (1.0f - abs(ridgeX)) * 0.1f + (1.0f - abs(ridgeY)) * 0.1f;
    
    // Layer 4: Small-scale turbulence
    float detail = 0.0f;
    detail += sin(longitude * 12.0f + latitude * 8.0f + seedOffset * 7.0f) * 0.05f;
    detail += cos(longitude * 15.0f - latitude * 11.0f + seedOffset * 8.0f) * 0.05f;"""

new_terrain = """    // Layer 1: Major continents (very low frequency, MUCH larger amplitude)
    continents += sin((longitude + seedOffset) * 1.5f) * 0.7f;
    continents += cos((latitude * 2.0f + seedOffset * 2.0f) * 1.2f) * 0.6f;
    continents += sin((longitude * 0.8f - latitude * 1.1f + seedOffset * 3.0f)) * 0.5f;
    
    // Layer 2: Regional variations (medium frequency)
    float regional = 0.0f;
    regional += sin((longitude * 3.2f + seedOffset * 4.0f)) * cos(latitude * 2.8f) * 0.35f;
    regional += cos((longitude * 4.1f - seedOffset * 2.0f)) * sin(latitude * 3.5f) * 0.3f;
    
    // Layer 3: Mountain ridges using absolute value for ridge effect
    float ridges = 0.0f;
    float ridgeX = sin(longitude * 5.0f + latitude * 3.0f + seedOffset * 5.0f);
    float ridgeY = cos(longitude * 4.0f - latitude * 6.0f + seedOffset * 6.0f);
    ridges = (1.0f - abs(ridgeX)) * 0.25f + (1.0f - abs(ridgeY)) * 0.25f;
    
    // Layer 4: Small-scale turbulence
    float detail = 0.0f;
    detail += sin(longitude * 12.0f + latitude * 8.0f + seedOffset * 7.0f) * 0.15f;
    detail += cos(longitude * 15.0f - latitude * 11.0f + seedOffset * 8.0f) * 0.12f;"""

content = content.replace(old_terrain, new_terrain)

# Fix 2: Update the normalization range
old_normalize = """    // Normalize to 0-1 range with bias toward ocean
    terrain = (terrain + 1.0f) * 0.5f; // Convert from -1..1 to 0..1"""

new_normalize = """    // Normalize to 0-1 range
    // The range is now roughly -3.5 to +3.5, so we scale appropriately
    terrain = (terrain + 3.5f) / 7.0f; // Convert from -3.5..3.5 to 0..1"""

content = content.replace(old_normalize, new_normalize)

# Fix 3: Add power curve for ocean bias
old_variation = """    // Add some variation based on 3D position for less predictable patterns
    float variation = sin(sphereNormal.x * 7.0f) * cos(sphereNormal.y * 9.0f) * sin(sphereNormal.z * 8.0f) * 0.08f;
    terrain += variation;
    
    return glm::clamp(terrain, 0.0f, 1.0f);"""

new_variation = """    // Add some variation based on 3D position for less predictable patterns
    float variation = sin(sphereNormal.x * 7.0f) * cos(sphereNormal.y * 9.0f) * sin(sphereNormal.z * 8.0f) * 0.12f;
    terrain += variation;
    
    // Apply a power curve to create more ocean and sharper coastlines
    terrain = pow(glm::clamp(terrain, 0.0f, 1.0f), 0.65f);
    
    return glm::clamp(terrain, 0.0f, 1.0f);"""

content = content.replace(old_variation, new_variation)

# Fix 4: Remove the remapping that compresses values
old_remap = """                // Apply ocean/land ratio bias (40% land, 60% ocean)
                // Remap noise value to get desired distribution
                continentValue = continentValue * 0.7f + 0.15f; // Range 0.15-0.85"""

new_remap = """                // Don't remap - let the terrain function output determine ocean/land
                // The sampleImprovedTerrain already includes ocean bias via power curve
                // continentValue is now in range 0-1 with natural distribution"""

content = content.replace(old_remap, new_remap)

# Fix 5: Update the material thresholds to match the new distribution
old_thresholds = """                // 40% land, 60% ocean based on actual continent value range (0.29-0.55)
                // Use mixed materials for more realistic boundaries
                if (continentValue > 0.5f) {"""

new_thresholds = """                // Use natural distribution from terrain function
                // Ocean below 0.4, land above 0.4 (roughly 60/40 split)
                if (continentValue > 0.7f) {"""

content = content.replace(old_thresholds, new_thresholds)

# Fix the rest of the thresholds
content = content.replace("else if (continentValue > 0.45f) {", "else if (continentValue > 0.6f) {")
content = content.replace("else if (continentValue > 0.40f) {", "else if (continentValue > 0.5f) {")
content = content.replace("else if (continentValue > 0.37f) {", "else if (continentValue > 0.45f) {")
content = content.replace("else if (continentValue > 0.35f) {", "else if (continentValue > 0.42f) {")
content = content.replace("else if (continentValue > 0.33f) {", "else if (continentValue > 0.4f) {")

# Write back
with open('src/core/octree.cpp', 'w') as f:
    f.write(content)

print("Fixed terrain generation in octree.cpp")