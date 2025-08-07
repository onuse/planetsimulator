# Float Workaround for UINT Attribute Issue

## Problem
Research shows that `VK_FORMAT_R32_UINT` can have driver/hardware compatibility issues, especially:
- At higher location indices (like location 5)
- With instance attributes
- On certain GPUs/drivers

## Workaround Options

### Option 1: Use SINT instead of UINT
Change to `VK_FORMAT_R32_SINT` and use `int` in shader instead of `uint`.

### Option 2: Pack material into existing float
Pack the material type as a float and unpack in shader.

### Option 3: Use float directly
Store material as float (0.0, 1.0, 2.0, 3.0) and cast to int in shader.

## Implementation Steps

1. Change InstanceData:
```cpp
struct InstanceData {
    glm::vec3 center;        // offset 0
    float halfSize;          // offset 12
    glm::vec3 color;         // offset 16
    float materialType;      // offset 28 - CHANGED TO FLOAT
};
```

2. Change vertex attribute:
```cpp
attributeDescriptions[5].format = VK_FORMAT_R32_SFLOAT;
```

3. Change shader:
```glsl
layout(location = 5) in float instanceMaterialType;
// Then in main():
uint materialType = uint(instanceMaterialType + 0.5); // Round to nearest int
```

This workaround avoids the UINT attribute issue entirely.