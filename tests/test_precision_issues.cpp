// Test to identify the actual precision issues causing flickering
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>

void testPrecisionAtPlanetScale() {
    std::cout << "============================================\n";
    std::cout << "PRECISION ISSUES AT PLANET SCALE\n";
    std::cout << "============================================\n\n";
    
    const double PLANET_RADIUS = 6371000.0;
    
    // Issue 1: Vertex positions are planet-scale but UV coords are 0-1
    std::cout << "ISSUE 1: Scale Mismatch\n";
    std::cout << "------------------------\n";
    
    // Vertex on planet surface
    double vertexX = PLANET_RADIUS;
    float vertexXFloat = (float)vertexX;
    
    std::cout << "Planet radius (double): " << std::fixed << std::setprecision(10) << vertexX << "\n";
    std::cout << "Planet radius (float):  " << vertexXFloat << "\n";
    std::cout << "Precision loss: " << (vertexX - vertexXFloat) << " meters\n\n";
    
    // Issue 2: When we modify UV by small amounts, it creates large world jumps
    std::cout << "ISSUE 2: UV to World Scaling\n";
    std::cout << "-----------------------------\n";
    
    float uv1 = 0.25f;
    float uv2 = 0.25f + 0.001f;  // Tiny UV change
    
    // These UVs map to world positions
    double world1 = uv1 * PLANET_RADIUS * 2.0;  // Patch spans diameter
    double world2 = uv2 * PLANET_RADIUS * 2.0;
    
    std::cout << "UV change: " << (uv2 - uv1) << "\n";
    std::cout << "World change: " << (world2 - world1) << " meters\n";
    std::cout << "That's a " << (world2 - world1) << " meter jump from tiny UV change!\n\n";
    
    // Issue 3: T-junction snapping at different scales
    std::cout << "ISSUE 3: T-Junction Snapping Distance\n";
    std::cout << "--------------------------------------\n";
    
    for (int levelDiff = 1; levelDiff <= 3; levelDiff++) {
        std::cout << "Level difference: " << levelDiff << "\n";
        
        // Test snapping of vertex at 0.3
        float vertex = 0.3f;
        float snapped;
        
        if (levelDiff >= 2) {
            // Our fix: snap to 0 or 1
            snapped = (vertex < 0.5f) ? 0.0f : 1.0f;
        } else {
            // Level diff 1: snap to 0, 0.5, or 1
            snapped = round(vertex * 2.0f) / 2.0f;
        }
        
        float snapDistance = std::abs(vertex - snapped);
        double worldSnapDistance = snapDistance * PLANET_RADIUS * 2.0;
        
        std::cout << "  UV " << vertex << " -> " << snapped << "\n";
        std::cout << "  Snap distance in UV: " << snapDistance << "\n";
        std::cout << "  Snap distance in world: " << worldSnapDistance << " meters\n";
        
        // Check visibility at different altitudes
        double altitudes[] = {100.0, 1000.0, 10000.0, 100000.0};
        for (double alt : altitudes) {
            double pixelsOnScreen = (worldSnapDistance / alt) * 720.0;  // Simplified
            if (pixelsOnScreen > 1.0) {
                std::cout << "  ⚠️ VISIBLE at " << alt << "m altitude (" 
                          << pixelsOnScreen << " pixels)\n";
            }
        }
        std::cout << "\n";
    }
    
    // Issue 4: Float vs Double in GPU
    std::cout << "ISSUE 4: GPU Float Limits\n";
    std::cout << "-------------------------\n";
    
    // Camera at altitude
    double cameraPos = PLANET_RADIUS + 1000.0;  // 1km altitude
    float cameraPosFloat = (float)cameraPos;
    
    // Vertex on surface
    double vertexPos = PLANET_RADIUS + 10.0;  // 10m above surface
    float vertexPosFloat = (float)vertexPos;
    
    // View space calculation
    double viewSpaceDouble = vertexPos - cameraPos;  // Should be -990
    float viewSpaceFloat = vertexPosFloat - cameraPosFloat;
    
    std::cout << "Camera position (double): " << cameraPos << "\n";
    std::cout << "Camera position (float):  " << cameraPosFloat << "\n";
    std::cout << "Vertex position (double): " << vertexPos << "\n";
    std::cout << "Vertex position (float):  " << vertexPosFloat << "\n";
    std::cout << "View space (double): " << viewSpaceDouble << " (correct)\n";
    std::cout << "View space (float):  " << viewSpaceFloat << "\n";
    std::cout << "ERROR: " << (viewSpaceDouble - viewSpaceFloat) << " meters\n\n";
    
    // Issue 5: Matrix multiplication accumulation
    std::cout << "ISSUE 5: Matrix Precision\n";
    std::cout << "-------------------------\n";
    
    // Simplified projection matrix element
    double m00 = 1.0 / tan(60.0 * M_PI / 180.0 / 2.0);  // FOV = 60 degrees
    double m11 = m00 * (1280.0 / 720.0);  // Aspect ratio
    double m22 = -(1000.0 + 10.0) / (1000.0 - 10.0);  // Near=10, Far=1000
    
    float m00f = (float)m00;
    float m11f = (float)m11;
    float m22f = (float)m22;
    
    std::cout << "Projection matrix elements:\n";
    std::cout << "  M[0][0] double: " << m00 << "\n";
    std::cout << "  M[0][0] float:  " << m00f << "\n";
    std::cout << "  M[2][2] double: " << m22 << "\n";
    std::cout << "  M[2][2] float:  " << m22f << "\n";
    
    // Apply to large coordinate
    double result = PLANET_RADIUS * m00;
    float resultf = (float)PLANET_RADIUS * m00f;
    
    std::cout << "After multiplying by planet radius:\n";
    std::cout << "  Double result: " << result << "\n";
    std::cout << "  Float result:  " << resultf << "\n";
    std::cout << "  Error: " << (result - resultf) << "\n";
}

void identifyFlickeringSources() {
    std::cout << "\n============================================\n";
    std::cout << "FLICKERING SOURCE IDENTIFICATION\n";
    std::cout << "============================================\n\n";
    
    std::cout << "Flickering is likely caused by:\n\n";
    
    std::cout << "1. VERTEX SNAPPING:\n";
    std::cout << "   When T-junction fix snaps vertices, they jump by thousands of meters\n";
    std::cout << "   Solution: Disable T-junction fix at close range OR use smoother snapping\n\n";
    
    std::cout << "2. FLOAT PRECISION IN VIEW SPACE:\n";
    std::cout << "   Planet-scale coords (6M meters) lose precision in float\n";
    std::cout << "   Solution: Use camera-relative rendering (origin at camera)\n\n";
    
    std::cout << "3. LOD TRANSITIONS:\n";
    std::cout << "   Patches subdivide/merge at slightly different distances each frame\n";
    std::cout << "   Solution: Add hysteresis to LOD transitions\n\n";
    
    std::cout << "4. Z-FIGHTING:\n";
    std::cout << "   Near/far ratio too large causes depth buffer precision loss\n";
    std::cout << "   Solution: Use logarithmic depth buffer or tighter near/far\n\n";
    
    // Test specific scenario
    const double PLANET_RADIUS = 6371000.0;
    
    std::cout << "SPECIFIC TEST: Zoom from 10km to 9km\n";
    std::cout << "-------------------------------------\n";
    
    for (int frame = 0; frame < 2; frame++) {
        double altitude = 10000.0 - frame * 1000.0;
        std::cout << "\nFrame " << frame << " (altitude " << altitude << "m):\n";
        
        // LOD calculation
        double distance = PLANET_RADIUS + altitude;
        double patchSize = 100000.0;  // 100km patch
        double screenError = (2.0 * atan(patchSize / (2.0 * distance)) / (M_PI/3.0)) * 720.0;
        int lodLevel = (screenError > 100.0) ? 1 : 0;
        
        std::cout << "  Screen error: " << screenError << " pixels\n";
        std::cout << "  LOD level: " << lodLevel << "\n";
        
        // Vertex position with T-junction fix
        float uv = 0.3f;
        float snapped = (lodLevel == 1) ? round(uv * 2.0f) / 2.0f : uv;
        double worldPos = PLANET_RADIUS + snapped * patchSize;
        
        std::cout << "  UV " << uv << " -> " << snapped << "\n";
        std::cout << "  World position: " << std::fixed << std::setprecision(2) << worldPos << "\n";
    }
}

int main() {
    testPrecisionAtPlanetScale();
    identifyFlickeringSources();
    
    std::cout << "\n============================================\n";
    std::cout << "CONCLUSION\n";
    std::cout << "============================================\n\n";
    
    std::cout << "The main problems are:\n";
    std::cout << "1. T-junction snapping causes massive world-space jumps\n";
    std::cout << "2. Float precision insufficient for planet-scale coordinates\n";
    std::cout << "3. LOD transitions happen at unstable thresholds\n";
    std::cout << "\nRecommended fixes:\n";
    std::cout << "1. Use camera-relative coordinates (subtract camera pos on CPU)\n";
    std::cout << "2. Implement LOD hysteresis (different thresholds for split/merge)\n";
    std::cout << "3. Disable or smooth T-junction fix at close range\n";
    std::cout << "4. Use logarithmic depth buffer for better Z precision\n";
    
    return 0;
}