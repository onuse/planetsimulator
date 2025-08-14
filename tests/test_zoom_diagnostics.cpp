// Diagnostic test to identify math failures during zoom
// This simulates camera movement and tracks how values change

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream>

struct CameraState {
    double altitude;  // Distance from surface
    double nearPlane;
    double farPlane;
    double fov;
};

struct PatchState {
    int level;
    double size;
    double screenSpaceError;
    double distance;
    bool shouldSubdivide;
};

// Simulate the math at different zoom levels
void diagnoseZoomMath() {
    std::cout << "==============================================\n";
    std::cout << "ZOOM DIAGNOSTICS - Finding Where Math Breaks\n";
    std::cout << "==============================================\n\n";
    
    const double PLANET_RADIUS = 6371000.0;
    
    // Test camera positions from far to near
    std::vector<double> altitudes = {
        10000000.0,  // 10,000 km - far view
        5000000.0,   // 5,000 km
        1000000.0,   // 1,000 km
        500000.0,    // 500 km
        100000.0,    // 100 km
        50000.0,     // 50 km
        10000.0,     // 10 km
        5000.0,      // 5 km
        1000.0,      // 1 km - very close
        100.0        // 100 m - extremely close
    };
    
    std::ofstream csv("zoom_diagnostics.csv");
    csv << "Altitude,Distance,NearPlane,FarPlane,NearFarRatio,DepthPrecisionBits,PatchSize,ScreenError,ShouldSubdivide\n";
    
    for (double altitude : altitudes) {
        std::cout << "===============================\n";
        std::cout << "Altitude: " << altitude/1000.0 << " km\n";
        std::cout << "-------------------------------\n";
        
        double distance = PLANET_RADIUS + altitude;
        
        // 1. CHECK: Clipping plane calculation
        double nearPlane = altitude * 0.001;  // This is what the code does
        double farPlane = altitude * 2.0;
        double nearFarRatio = farPlane / nearPlane;
        
        std::cout << "Clipping Planes:\n";
        std::cout << "  Near: " << nearPlane << " m\n";
        std::cout << "  Far: " << farPlane << " m\n";
        std::cout << "  Ratio: " << nearFarRatio << "\n";
        
        // Check depth buffer precision
        double depthPrecisionBits = log2(nearFarRatio);
        std::cout << "  Depth precision loss: " << depthPrecisionBits << " bits\n";
        
        if (nearFarRatio > 10000) {
            std::cout << "  ⚠️ WARNING: Extreme near/far ratio causes Z-fighting!\n";
        }
        
        // 2. CHECK: Double to float conversion in matrices
        double viewProjMatrix[16];
        // Simulate view-projection matrix computation
        for (int i = 0; i < 16; i++) {
            viewProjMatrix[i] = distance * (i == 0 ? 1.0 : 0.1);  // Simplified
        }
        
        // Convert to float and back
        float floatMatrix[16];
        for (int i = 0; i < 16; i++) {
            floatMatrix[i] = (float)viewProjMatrix[i];
        }
        
        double maxError = 0.0;
        for (int i = 0; i < 16; i++) {
            double error = std::abs(viewProjMatrix[i] - (double)floatMatrix[i]);
            maxError = std::max(maxError, error);
        }
        
        std::cout << "Matrix Precision:\n";
        std::cout << "  Max float conversion error: " << maxError << "\n";
        if (maxError > 1.0) {
            std::cout << "  ⚠️ WARNING: Significant precision loss in matrix!\n";
        }
        
        // 3. CHECK: Screen space error calculation
        double patchSize = PLANET_RADIUS * 2.0;  // Root patch
        double geometricError = patchSize * 0.1;  // Simplified
        double screenHeight = 720;
        
        // This is the critical calculation
        double angularSize = 2.0 * atan(geometricError / (2.0 * distance));
        double screenSpaceError = (angularSize / (M_PI/3.0)) * screenHeight;
        
        std::cout << "LOD Calculation:\n";
        std::cout << "  Patch size: " << patchSize << " m\n";
        std::cout << "  Angular size: " << angularSize << " rad\n";
        std::cout << "  Screen error: " << screenSpaceError << " pixels\n";
        
        // 4. CHECK: Subdivision threshold
        double threshold = 100.0;  // Fixed threshold
        bool shouldSubdivide = screenSpaceError > threshold;
        
        std::cout << "  Should subdivide: " << (shouldSubdivide ? "YES" : "NO") << "\n";
        
        // 5. CHECK: Vertex transformation pipeline
        double vertexX = PLANET_RADIUS;  // A vertex on the surface
        double vertexY = 0;
        double vertexZ = 0;
        
        // Transform to view space (simplified)
        double viewX = vertexX - distance;
        double viewY = vertexY;
        double viewZ = vertexZ;
        
        // Project (simplified perspective division)
        double w = -viewZ / nearPlane;
        double ndcX = viewX / w;
        double ndcY = viewY / w;
        double ndcZ = (viewZ - nearPlane) / (farPlane - nearPlane);
        
        std::cout << "Vertex Transform:\n";
        std::cout << "  World: (" << vertexX << ", " << vertexY << ", " << vertexZ << ")\n";
        std::cout << "  View: (" << viewX << ", " << viewY << ", " << viewZ << ")\n";
        std::cout << "  NDC: (" << ndcX << ", " << ndcY << ", " << ndcZ << ")\n";
        
        // Check for numerical issues
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
            std::cout << "  ⚠️ ERROR: Non-finite values in NDC!\n";
        }
        if (std::abs(ndcX) > 1000 || std::abs(ndcY) > 1000) {
            std::cout << "  ⚠️ WARNING: NDC coordinates extremely large!\n";
        }
        
        // 6. CHECK: T-junction snapping at this scale
        float uvCoord = 0.25f;
        float levelDiff = 2.0f;
        
        // Simulate snapping
        float snapped = (uvCoord < 0.5f) ? 0.0f : 1.0f;
        float snapDistance = std::abs(uvCoord - snapped) * patchSize;
        
        std::cout << "T-Junction Fix:\n";
        std::cout << "  UV " << uvCoord << " -> " << snapped << "\n";
        std::cout << "  Snap distance: " << snapDistance << " m\n";
        
        if (snapDistance > altitude * 0.01) {
            std::cout << "  ⚠️ WARNING: Snap distance visible at this altitude!\n";
        }
        
        // Write to CSV for analysis
        csv << altitude << ","
            << distance << ","
            << nearPlane << ","
            << farPlane << ","
            << nearFarRatio << ","
            << depthPrecisionBits << ","
            << patchSize << ","
            << screenSpaceError << ","
            << shouldSubdivide << "\n";
    }
    
    csv.close();
    std::cout << "\n==============================================\n";
    std::cout << "Diagnostic data written to zoom_diagnostics.csv\n";
}

// Test specific problem: flickering during zoom
void testFlickeringCause() {
    std::cout << "\n==============================================\n";
    std::cout << "FLICKERING ROOT CAUSE ANALYSIS\n";
    std::cout << "==============================================\n\n";
    
    // Flickering usually caused by:
    // 1. Vertices jumping position (snapping)
    // 2. Depth buffer precision loss
    // 3. Float/double conversion errors
    // 4. Matrix instability
    
    const double PLANET_RADIUS = 6371000.0;
    
    // Simulate zooming from 100km to 99km
    double altitude1 = 100000.0;
    double altitude2 = 99000.0;
    
    for (int i = 0; i < 2; i++) {
        double altitude = (i == 0) ? altitude1 : altitude2;
        std::cout << "Frame " << i << " - Altitude: " << altitude << " m\n";
        
        // Calculate vertex position with double precision
        double vertexWorldX = PLANET_RADIUS + 100.0;  // 100m above surface
        
        // Camera position
        double cameraX = PLANET_RADIUS + altitude;
        
        // View space position
        double viewX = vertexWorldX - cameraX;
        
        // Perspective projection
        double near = altitude * 0.001;
        double projected = viewX / near;
        
        std::cout << "  Vertex world X: " << std::fixed << std::setprecision(10) << vertexWorldX << "\n";
        std::cout << "  Camera X: " << cameraX << "\n";
        std::cout << "  View X: " << viewX << "\n";
        std::cout << "  Projected X: " << projected << "\n";
        
        // Convert to float (GPU precision)
        float gpuProjected = (float)projected;
        std::cout << "  GPU (float) X: " << gpuProjected << "\n";
    }
    
    // Check if the projection changes significantly
    double near1 = altitude1 * 0.001;
    double near2 = altitude2 * 0.001;
    double proj1 = (PLANET_RADIUS + 100.0 - (PLANET_RADIUS + altitude1)) / near1;
    double proj2 = (PLANET_RADIUS + 100.0 - (PLANET_RADIUS + altitude2)) / near2;
    
    double changePct = std::abs(proj2 - proj1) / std::abs(proj1) * 100.0;
    std::cout << "\nProjection change: " << changePct << "%\n";
    
    if (changePct > 1.0) {
        std::cout << "⚠️ WARNING: Large projection change causes flickering!\n";
    }
}

// Test LOD transition stability
void testLODTransitions() {
    std::cout << "\n==============================================\n";
    std::cout << "LOD TRANSITION STABILITY\n";
    std::cout << "==============================================\n\n";
    
    // Test if LOD levels flip-flop during zoom
    std::vector<double> altitudes;
    for (double alt = 100000; alt >= 90000; alt -= 100) {
        altitudes.push_back(alt);
    }
    
    std::cout << "Altitude(m)\tScreenError\tLODLevel\tTransition\n";
    
    int prevLevel = -1;
    int flipFlops = 0;
    
    for (double altitude : altitudes) {
        // Simplified screen error calculation
        double distance = 6371000.0 + altitude;
        double patchSize = 1000000.0;  // 1000km patch
        double angularSize = 2.0 * atan(patchSize / (2.0 * distance));
        double screenError = (angularSize / (M_PI/3.0)) * 720;
        
        // Determine LOD level
        int level = 0;
        double threshold = 100.0;
        while (screenError > threshold && level < 10) {
            level++;
            screenError /= 2.0;  // Each level halves the error
        }
        
        std::string transition = "";
        if (prevLevel != -1) {
            if (level != prevLevel) {
                transition = (level > prevLevel) ? "SUBDIVIDE" : "MERGE";
                if (std::abs(level - prevLevel) > 1) {
                    transition += " (JUMP!)";
                    flipFlops++;
                }
            }
        }
        
        std::cout << altitude << "\t\t" 
                  << std::fixed << std::setprecision(2) << screenError << "\t\t" 
                  << level << "\t\t" 
                  << transition << "\n";
        
        prevLevel = level;
    }
    
    if (flipFlops > 0) {
        std::cout << "\n⚠️ WARNING: " << flipFlops << " LOD level jumps detected!\n";
        std::cout << "This causes geometry popping and flickering.\n";
    }
}

int main() {
    diagnoseZoomMath();
    testFlickeringCause();
    testLODTransitions();
    
    std::cout << "\n==============================================\n";
    std::cout << "DIAGNOSIS COMPLETE\n";
    std::cout << "==============================================\n";
    std::cout << "\nKey issues to investigate:\n";
    std::cout << "1. Near/far plane ratio at close distances\n";
    std::cout << "2. Float precision loss in projection\n";
    std::cout << "3. LOD transition instability\n";
    std::cout << "4. T-junction snapping at different scales\n";
    
    return 0;
}