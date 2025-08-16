#include <iostream>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>
#include "core/global_patch_generator.hpp"

// Test to verify patches have correct face IDs for their positions

int getFaceFromPosition(const glm::dvec3& center) {
    // Determine which face a position belongs to based on which coordinate is closest to ±1
    double ax = std::abs(center.x);
    double ay = std::abs(center.y);
    double az = std::abs(center.z);
    
    // Find the dominant axis (closest to ±1)
    if (ax >= ay && ax >= az) {
        // X is dominant
        return (center.x > 0) ? 0 : 1;  // +X : -X
    } else if (ay >= ax && ay >= az) {
        // Y is dominant
        return (center.y > 0) ? 2 : 3;  // +Y : -Y
    } else {
        // Z is dominant
        return (center.z > 0) ? 4 : 5;  // +Z : -Z
    }
}

const char* getFaceName(int faceId) {
    switch(faceId) {
        case 0: return "+X";
        case 1: return "-X";
        case 2: return "+Y";
        case 3: return "-Y";
        case 4: return "+Z";
        case 5: return "-Z";
        default: return "???";
    }
}

int main() {
    std::cout << "=== Testing Face ID Assignment ===\n\n";
    
    // Create root patches
    auto roots = core::GlobalPatchGenerator::createRootPatches();
    
    std::cout << "Root patches:\n";
    for (size_t i = 0; i < roots.size(); i++) {
        auto& patch = roots[i];
        int expectedFace = getFaceFromPosition(patch.center);
        
        std::cout << "Patch " << i << ": ";
        std::cout << "Center=(" << patch.center.x << "," << patch.center.y << "," << patch.center.z << ") ";
        std::cout << "FaceId=" << patch.faceId << " (" << getFaceName(patch.faceId) << ") ";
        std::cout << "Expected=" << expectedFace << " (" << getFaceName(expectedFace) << ") ";
        
        if (patch.faceId != expectedFace) {
            std::cout << "✗ MISMATCH!";
        } else {
            std::cout << "✓";
        }
        std::cout << "\n";
    }
    
    std::cout << "\n=== Testing Subdivided Patches ===\n";
    
    // Test subdivision to see if face IDs are preserved correctly
    int mismatches = 0;
    int totalPatches = 0;
    
    for (auto& root : roots) {
        auto children = core::GlobalPatchGenerator::subdivide(root);
        
        for (auto& child : children) {
            int expectedFace = getFaceFromPosition(child.center);
            totalPatches++;
            
            if (child.faceId != expectedFace) {
                mismatches++;
                std::cout << "MISMATCH: Child at (" 
                          << child.center.x << "," << child.center.y << "," << child.center.z 
                          << ") has faceId=" << child.faceId << " (" << getFaceName(child.faceId) 
                          << ") but should be " << expectedFace << " (" << getFaceName(expectedFace) << ")\n";
                
                // Also check the bounds
                std::cout << "  Bounds: min=(" << child.minBounds.x << "," << child.minBounds.y << "," << child.minBounds.z 
                          << ") max=(" << child.maxBounds.x << "," << child.maxBounds.y << "," << child.maxBounds.z << ")\n";
            }
            
            // Go one more level deep for edge/corner patches
            if (mismatches < 5) {  // Limit output
                auto grandchildren = core::GlobalPatchGenerator::subdivide(child);
                for (auto& gc : grandchildren) {
                    int gcExpected = getFaceFromPosition(gc.center);
                    totalPatches++;
                    
                    if (gc.faceId != gcExpected) {
                        mismatches++;
                        std::cout << "  GRANDCHILD MISMATCH at (" 
                                  << gc.center.x << "," << gc.center.y << "," << gc.center.z 
                                  << ") faceId=" << gc.faceId << " expected=" << gcExpected << "\n";
                    }
                }
            }
        }
    }
    
    std::cout << "\n=== RESULTS ===\n";
    std::cout << "Total patches tested: " << totalPatches << "\n";
    std::cout << "Mismatches found: " << mismatches << "\n";
    
    if (mismatches > 0) {
        std::cout << "\n✗ FACE ID ASSIGNMENT IS BROKEN!\n";
        std::cout << "This could cause patches to use wrong transforms, creating the 'double planet' effect.\n";
    } else {
        std::cout << "\n✓ All patches have correct face IDs\n";
    }
    
    // Additional test: Check if face IDs are being preserved through subdivision
    std::cout << "\n=== Testing Face ID Preservation ===\n";
    for (int faceId = 0; faceId < 6; faceId++) {
        auto& root = roots[faceId];
        auto children = core::GlobalPatchGenerator::subdivide(root);
        
        bool allMatch = true;
        for (auto& child : children) {
            if (child.faceId != root.faceId) {
                allMatch = false;
                std::cout << "Face " << faceId << " child has different faceId: " << child.faceId << "\n";
            }
        }
        
        if (allMatch) {
            std::cout << "Face " << faceId << " (" << getFaceName(faceId) << "): All children preserved face ID ✓\n";
        }
    }
    
    return 0;
}