# Fix for Depth Precision (Back Face Bleed-Through)

## Problem Confirmed
- Dots are depth buffer precision issues, NOT mesh gaps
- Worse at distance, better up close
- Classic z-fighting between front and back faces

## Solution: Logarithmic Depth Buffer

### Option 1: Quick Fix - Depth Bias (Temporary)
Push back faces slightly farther in depth:

```cpp
// In vertex shader
if (gl_FrontFacing == false) {
    gl_Position.z += 0.0001; // Push back faces farther
}
```

### Option 2: Proper Fix - Logarithmic Depth Buffer

In vertex shader:
```glsl
// After calculating clip space position
vec4 clipPos = viewProj * vec4(cameraRelativePos, 1.0);

// Logarithmic depth
float C = 1.0; // Tweak for best precision distribution
float FC = 1.0 / log(farPlane * C + 1.0);
clipPos.z = log(clipPos.w * C + 1.0) * FC;
clipPos.z *= clipPos.w;

gl_Position = clipPos;
```

In fragment shader:
```glsl
// Fix depth value for logarithmic buffer
gl_FragDepth = log(gl_FragCoord.w * C + 1.0) * FC;
```

### Option 3: Reversed-Z Buffer
Use 1.0 for near, 0.0 for far - better precision distribution:

```cpp
// In projection matrix setup
// Swap near/far in projection matrix
projectionMatrix[2][2] = near / (far - near);  // Instead of -far/(far-near)
projectionMatrix[2][3] = -1.0f;
projectionMatrix[3][2] = -(far * near) / (far - near);
```

## Recommended Approach

1. **First**: Try depth bias (quick test)
2. **Then**: Implement logarithmic depth (proper solution)
3. **Finally**: Re-enable backface culling once depth is fixed