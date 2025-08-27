#!/usr/bin/env python3
import math

# Test the terrain function logic
def test_terrain(x, y, z):
    # Normalize to sphere normal
    length = math.sqrt(x*x + y*y + z*z)
    if length == 0:
        return 0
    nx, ny, nz = x/length, y/length, z/length
    
    longitude = math.atan2(nx, nz)
    latitude = math.asin(ny)
    
    seed = 42
    seedOffset = seed * 0.0123
    
    # Layer 1: Major continents (very low frequency, MUCH larger amplitude)
    continents = 0.0
    continents += math.sin((longitude + seedOffset) * 1.5) * 0.7
    continents += math.cos((latitude * 2.0 + seedOffset * 2.0) * 1.2) * 0.6
    continents += math.sin((longitude * 0.8 - latitude * 1.1 + seedOffset * 3.0)) * 0.5
    
    # Layer 2: Regional variations (medium frequency)
    regional = 0.0
    regional += math.sin((longitude * 3.2 + seedOffset * 4.0)) * math.cos(latitude * 2.8) * 0.35
    regional += math.cos((longitude * 4.1 - seedOffset * 2.0)) * math.sin(latitude * 3.5) * 0.3
    
    # Layer 3: Mountain ridges using absolute value for ridge effect
    ridgeX = math.sin(longitude * 5.0 + latitude * 3.0 + seedOffset * 5.0)
    ridgeY = math.cos(longitude * 4.0 - latitude * 6.0 + seedOffset * 6.0)
    ridges = (1.0 - abs(ridgeX)) * 0.25 + (1.0 - abs(ridgeY)) * 0.25
    
    # Layer 4: Small-scale turbulence
    detail = 0.0
    detail += math.sin(longitude * 12.0 + latitude * 8.0 + seedOffset * 7.0) * 0.15
    detail += math.cos(longitude * 15.0 - latitude * 11.0 + seedOffset * 8.0) * 0.12
    
    # Combine all layers
    terrain = continents + regional + ridges + detail
    
    # Add polar ice tendency
    polarBias = abs(ny)
    if polarBias > 0.8:
        terrain += (polarBias - 0.8) * 1.5
    
    # Add some asymmetry
    terrain += math.sin(nx * 2.1 + nz * 1.7 + seedOffset * 9.0) * 0.1
    
    print(f"Before normalization: terrain = {terrain:.3f}")
    
    # Normalize to 0-1 range
    # The range is now roughly -3.5 to +3.5, so we scale appropriately
    terrain = (terrain + 3.5) / 7.0  # Convert from -3.5..3.5 to 0..1
    
    print(f"After normalization: terrain = {terrain:.3f}")
    
    # Add variation
    variation = math.sin(nx * 7.0) * math.cos(ny * 9.0) * math.sin(nz * 8.0) * 0.12
    terrain += variation
    
    print(f"After variation: terrain = {terrain:.3f}")
    
    # Apply a power curve to create more ocean and sharper coastlines
    terrain = max(0, min(1, terrain))  # clamp
    terrain = terrain ** 1.8
    
    print(f"After power curve: terrain = {terrain:.3f}")
    
    return terrain

# Test several points
print("Testing terrain generation at various points:")
print("\nEquator point (1, 0, 0):")
test_terrain(1, 0, 0)

print("\nPole point (0, 1, 0):")
test_terrain(0, 1, 0)

print("\nRandom point (0.6, 0.6, 0.6):")
test_terrain(0.6, 0.6, 0.6)

print("\nAnother point (-0.5, 0.3, 0.8):")
test_terrain(-0.5, 0.3, 0.8)

# Test 100 random points to get distribution
import random
values = []
for i in range(100):
    # Random point on sphere
    theta = random.random() * 2 * math.pi
    phi = math.acos(2 * random.random() - 1)
    x = math.sin(phi) * math.cos(theta)
    y = math.sin(phi) * math.sin(theta) 
    z = math.cos(phi)
    
    val = test_terrain(x, y, z)
    values.append(val)

print(f"\n\nDistribution of 100 random points:")
print(f"Min: {min(values):.3f}")
print(f"Max: {max(values):.3f}")
print(f"Average: {sum(values)/len(values):.3f}")

# Count how many are ocean vs land (threshold 0.4)
ocean = sum(1 for v in values if v < 0.4)
print(f"Ocean (<0.4): {ocean}%")
print(f"Land (>=0.4): {100-ocean}%")