#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <set>
#include <cmath>
#include <iomanip>

// Test to diagnose the "double planet" rendering issue
// Theory: The patches might be getting duplicated or mirrored

struct Patch {
    glm::dvec3 minBounds;
    glm::dvec3 maxBounds;
    glm::dvec3 center;
    int faceId;
    int level;
};

glm::dmat4 createTransform(const Patch& patch) {
    glm::dvec3 range = patch.maxBounds - patch.minBounds;
    const double eps = 1e-6;
    
    glm::dmat4 transform(1.0);
    
    if (range.x < eps) {
        // X is fixed
        double x = patch.minBounds.x;
        if (std::abs(std::abs(x) - 1.0) < 1e-5) {
            x = (x > 0) ? 1.0 : -1.0;
        }
        
        transform[0] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // U -> Z
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[3] = glm::dvec4(x, patch.minBounds.y, patch.minBounds.z, 1.0);
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    }
    else if (range.y < eps) {
        // Y is fixed
        double y = patch.minBounds.y;
        if (std::abs(std::abs(y) - 1.0) < 1e-5) {
            y = (y > 0) ? 1.0 : -1.0;
        }
        
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, 0.0, range.z, 0.0);    // V -> Z
        transform[3] = glm::dvec4(patch.minBounds.x, y, patch.minBounds.z, 1.0);
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    }
    else if (range.z < eps) {
        // Z is fixed
        double z = patch.minBounds.z;
        if (std::abs(std::abs(z) - 1.0) < 1e-5) {
            z = (z > 0) ? 1.0 : -1.0;
        }
        
        transform[0] = glm::dvec4(range.x, 0.0, 0.0, 0.0);    // U -> X
        transform[1] = glm::dvec4(0.0, range.y, 0.0, 0.0);    // V -> Y
        transform[3] = glm::dvec4(patch.minBounds.x, patch.minBounds.y, z, 1.0);
        transform[2] = glm::dvec4(0.0, 0.0, 0.0, 1.0);
    }
    
    return transform;
}

void analyzeRootPatches() {
    std::cout << "\n=== ROOT PATCH ANALYSIS ===\n";
    
    std::vector<Patch> roots;
    
    // Create 6 root patches
    roots.push_back({glm::dvec3(1, -1, -1), glm::dvec3(1, 1, 1), glm::dvec3(1, 0, 0), 0, 0});   // +X
    roots.push_back({glm::dvec3(-1, -1, -1), glm::dvec3(-1, 1, 1), glm::dvec3(-1, 0, 0), 1, 0}); // -X
    roots.push_back({glm::dvec3(-1, 1, -1), glm::dvec3(1, 1, 1), glm::dvec3(0, 1, 0), 2, 0});   // +Y
    roots.push_back({glm::dvec3(-1, -1, -1), glm::dvec3(1, -1, 1), glm::dvec3(0, -1, 0), 3, 0}); // -Y
    roots.push_back({glm::dvec3(-1, -1, 1), glm::dvec3(1, 1, 1), glm::dvec3(0, 0, 1), 4, 0});   // +Z
    roots.push_back({glm::dvec3(-1, -1, -1), glm::dvec3(1, 1, -1), glm::dvec3(0, 0, -1), 5, 0}); // -Z
    
    // Test each root patch transform
    for (size_t i = 0; i < roots.size(); i++) {
        const char* faceNames[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
        std::cout << "\nFace " << faceNames[i] << ":\n";
        
        glm::dmat4 transform = createTransform(roots[i]);
        
        // Test UV corners
        glm::dvec2 uvCorners[] = {
            {0, 0}, {1, 0}, {1, 1}, {0, 1}
        };
        
        std::cout << "  UV -> Cube transformations:\n";
        for (int j = 0; j < 4; j++) {
            glm::dvec4 uv(uvCorners[j].x, uvCorners[j].y, 0, 1);
            glm::dvec3 cube = glm::dvec3(transform * uv);
            std::cout << "    UV(" << uvCorners[j].x << "," << uvCorners[j].y << ") -> "
                     << "Cube(" << std::fixed << std::setprecision(3) 
                     << cube.x << ", " << cube.y << ", " << cube.z << ")\n";
        }
        
        // Check for overlaps
        std::cout << "  Bounds: min(" << roots[i].minBounds.x << ", " << roots[i].minBounds.y << ", " << roots[i].minBounds.z 
                 << ") max(" << roots[i].maxBounds.x << ", " << roots[i].maxBounds.y << ", " << roots[i].maxBounds.z << ")\n";
    }
}

void checkForDuplicateRegions() {
    std::cout << "\n=== CHECKING FOR DUPLICATE/OVERLAPPING REGIONS ===\n";
    
    // Simulate subdivision and check if any patches overlap
    struct Region {
        glm::dvec3 min, max;
        int faceId;
        std::string desc;
    };
    
    std::vector<Region> regions;
    
    // Add root faces
    regions.push_back({glm::dvec3(0.999, -1, -1), glm::dvec3(1.001, 1, 1), 0, "+X face"});
    regions.push_back({glm::dvec3(-1.001, -1, -1), glm::dvec3(-0.999, 1, 1), 1, "-X face"});
    regions.push_back({glm::dvec3(-1, 0.999, -1), glm::dvec3(1, 1.001, 1), 2, "+Y face"});
    regions.push_back({glm::dvec3(-1, -1.001, -1), glm::dvec3(1, -0.999, 1), 3, "-Y face"});
    regions.push_back({glm::dvec3(-1, -1, 0.999), glm::dvec3(1, 1, 1.001), 4, "+Z face"});
    regions.push_back({glm::dvec3(-1, -1, -1.001), glm::dvec3(1, 1, -0.999), 5, "-Z face"});
    
    // Check for overlaps
    for (size_t i = 0; i < regions.size(); i++) {
        for (size_t j = i + 1; j < regions.size(); j++) {
            // Check if regions overlap
            bool overlapX = !(regions[i].max.x < regions[j].min.x || regions[j].max.x < regions[i].min.x);
            bool overlapY = !(regions[i].max.y < regions[j].min.y || regions[j].max.y < regions[i].min.y);
            bool overlapZ = !(regions[i].max.z < regions[j].min.z || regions[j].max.z < regions[i].min.z);
            
            if (overlapX && overlapY && overlapZ) {
                std::cout << "  WARNING: " << regions[i].desc << " overlaps with " << regions[j].desc << "\n";
                
                // Calculate overlap region
                glm::dvec3 overlapMin = glm::max(regions[i].min, regions[j].min);
                glm::dvec3 overlapMax = glm::min(regions[i].max, regions[j].max);
                std::cout << "    Overlap region: (" << overlapMin.x << ", " << overlapMin.y << ", " << overlapMin.z 
                         << ") to (" << overlapMax.x << ", " << overlapMax.y << ", " << overlapMax.z << ")\n";
            }
        }
    }
}

void simulateVertexGeneration() {
    std::cout << "\n=== SIMULATING VERTEX GENERATION ===\n";
    
    // Simulate what happens when we generate vertices for patches
    // Theory: Maybe vertices are being duplicated or mirrored
    
    std::set<std::string> generatedVertices;
    
    // Generate vertices for each face at a coarse level
    std::vector<Patch> faces;
    faces.push_back({glm::dvec3(1, -1, -1), glm::dvec3(1, 1, 1), glm::dvec3(1, 0, 0), 0, 0});   // +X
    faces.push_back({glm::dvec3(-1, -1, -1), glm::dvec3(-1, 1, 1), glm::dvec3(-1, 0, 0), 1, 0}); // -X
    
    for (const auto& face : faces) {
        glm::dmat4 transform = createTransform(face);
        
        // Generate a 3x3 grid of vertices
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                double u = x / 2.0;
                double v = y / 2.0;
                
                glm::dvec4 uv(u, v, 0, 1);
                glm::dvec3 cube = glm::dvec3(transform * uv);
                
                // Create a string key for this vertex
                char key[100];
                snprintf(key, sizeof(key), "%.6f,%.6f,%.6f", cube.x, cube.y, cube.z);
                
                if (generatedVertices.count(key) > 0) {
                    std::cout << "  DUPLICATE VERTEX at cube pos (" << cube.x << ", " << cube.y << ", " << cube.z << ")\n";
                    std::cout << "    Face " << face.faceId << " UV(" << u << ", " << v << ")\n";
                }
                generatedVertices.insert(key);
            }
        }
    }
    
    std::cout << "  Total unique vertices: " << generatedVertices.size() << "\n";
}

void checkTransformConsistency() {
    std::cout << "\n=== CHECKING TRANSFORM CONSISTENCY ===\n";
    
    // Test if transforms are consistent when patches share edges
    Patch patchPosX = {glm::dvec3(1, 0, 0), glm::dvec3(1, 0.5, 0.5), glm::dvec3(1, 0.25, 0.25), 0, 1};
    Patch patchPosY = {glm::dvec3(0, 1, 0), glm::dvec3(0.5, 1, 0.5), glm::dvec3(0.25, 1, 0.25), 2, 1};
    
    glm::dmat4 transformX = createTransform(patchPosX);
    glm::dmat4 transformY = createTransform(patchPosY);
    
    // Test edge at X=1, Y=1 (should be shared by both patches at their boundaries)
    // For +X face: U=1 maps to Z=0.5, V=1 maps to Y=0.5
    // For +Y face: U=1 maps to X=0.5, V=1 maps to Z=0.5
    
    glm::dvec4 uvX(1, 1, 0, 1);  // Top-right corner of +X patch
    glm::dvec3 cubeX = glm::dvec3(transformX * uvX);
    
    glm::dvec4 uvY(1, 1, 0, 1);  // Top-right corner of +Y patch  
    glm::dvec3 cubeY = glm::dvec3(transformY * uvY);
    
    std::cout << "  +X patch corner (1,1): " << cubeX.x << ", " << cubeX.y << ", " << cubeX.z << "\n";
    std::cout << "  +Y patch corner (1,1): " << cubeY.x << ", " << cubeY.y << ", " << cubeY.z << "\n";
    
    // Check if edge/corner patches would create duplicates
    // Edge patches occur where two faces meet
    glm::dvec3 distance = cubeX - cubeY;
    double dist = glm::length(distance);
    if (dist > 0.1) {
        std::cout << "  WARNING: Large distance between supposedly adjacent patches: " << dist << "\n";
    }
}

int main() {
    std::cout << "=== DIAGNOSING DOUBLE PLANET RENDERING ISSUE ===\n";
    std::cout << "Theory: The issue might be caused by:\n";
    std::cout << "1. Duplicate patch generation\n";
    std::cout << "2. Incorrect transforms causing mirroring\n";
    std::cout << "3. Overlapping face regions\n";
    std::cout << "4. Instance buffer containing duplicates\n\n";
    
    analyzeRootPatches();
    checkForDuplicateRegions();
    simulateVertexGeneration();
    checkTransformConsistency();
    
    std::cout << "\n=== DIAGNOSIS SUMMARY ===\n";
    std::cout << "Based on the analysis above, the most likely cause is:\n";
    std::cout << "- If overlaps detected: Face patches are overlapping at boundaries\n";
    std::cout << "- If duplicates detected: Vertices are being generated multiple times\n";
    std::cout << "- If transform issues: The transform matrices are creating mirrored geometry\n";
    
    return 0;
}
