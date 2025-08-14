#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <cmath>
#include <map>
#include <sstream>

// ============================================================================
// MODULAR PATCH DIAGNOSTIC SYSTEM
// ============================================================================
// This tool captures "production" patch data and tests each component
// in isolation to identify exactly where discontinuities occur

const double PLANET_RADIUS = 6371000.0;

// ============================================================================
// MODULE 1: Data Capture & Storage
// ============================================================================

struct PatchData {
    int faceIndex;
    int level;
    glm::dvec3 center;
    double size;
    glm::dmat4 transform;
    std::vector<glm::dvec3> boundaryVertices;
    std::vector<double> terrainHeights;
};

class PatchDataCapture {
public:
    void capturePatch(const PatchData& patch) {
        patches.push_back(patch);
    }
    
    void saveToFile(const std::string& filename) {
        std::ofstream file(filename);
        file << std::fixed << std::setprecision(12);
        file << patches.size() << "\n";
        
        for (const auto& p : patches) {
            file << p.faceIndex << " " << p.level << "\n";
            file << p.center.x << " " << p.center.y << " " << p.center.z << "\n";
            file << p.size << "\n";
            
            // Save transform matrix
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    file << p.transform[i][j] << " ";
                }
                file << "\n";
            }
            
            // Save boundary vertices
            file << p.boundaryVertices.size() << "\n";
            for (const auto& v : p.boundaryVertices) {
                file << v.x << " " << v.y << " " << v.z << "\n";
            }
            
            // Save terrain heights
            file << p.terrainHeights.size() << "\n";
            for (double h : p.terrainHeights) {
                file << h << "\n";
            }
        }
        file.close();
        std::cout << "Saved " << patches.size() << " patches to " << filename << "\n";
    }
    
    void loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Could not open " << filename << "\n";
            return;
        }
        
        patches.clear();
        size_t count;
        file >> count;
        
        for (size_t i = 0; i < count; i++) {
            PatchData p;
            file >> p.faceIndex >> p.level;
            file >> p.center.x >> p.center.y >> p.center.z;
            file >> p.size;
            
            // Load transform
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 4; col++) {
                    file >> p.transform[row][col];
                }
            }
            
            // Load boundary vertices
            size_t vertCount;
            file >> vertCount;
            p.boundaryVertices.resize(vertCount);
            for (auto& v : p.boundaryVertices) {
                file >> v.x >> v.y >> v.z;
            }
            
            // Load terrain heights
            size_t heightCount;
            file >> heightCount;
            p.terrainHeights.resize(heightCount);
            for (auto& h : p.terrainHeights) {
                file >> h;
            }
            
            patches.push_back(p);
        }
        file.close();
        std::cout << "Loaded " << patches.size() << " patches from " << filename << "\n";
    }
    
    std::vector<PatchData> patches;
};

// ============================================================================
// MODULE 2: Vertex Transformation Pipeline (CPU Emulation)
// ============================================================================

class VertexTransformEmulator {
public:
    // Emulate the exact shader transformation
    glm::dvec3 transformVertex(glm::dvec2 uv, const glm::dmat4& patchTransform) {
        // Step 1: UV to local space (matching shader)
        glm::dvec4 localPos(uv.x, uv.y, 0.0, 1.0);
        
        // Step 2: Transform to cube face
        glm::dvec3 cubePos = glm::dvec3(patchTransform * localPos);
        
        // Step 3: Snap to boundaries (emulating shader EPSILON logic)
        snapToBoundary(cubePos);
        
        // Step 4: Cube to sphere projection
        glm::dvec3 spherePos = cubeToSphere(cubePos);
        
        // Step 5: Scale by radius
        return spherePos * PLANET_RADIUS;
    }
    
    // Test if two vertices should be identical
    bool shouldBeIdentical(const glm::dvec3& v1, const glm::dvec3& v2) {
        double dist = glm::length(v1 - v2);
        return dist < 0.01; // Less than 1cm difference
    }
    
private:
    void snapToBoundary(glm::dvec3& cubePos) {
        const double BOUNDARY = 1.0;
        const double EPSILON = 0.001; // Current shader value
        
        if (std::abs(std::abs(cubePos.x) - BOUNDARY) < EPSILON) {
            cubePos.x = (cubePos.x > 0.0) ? BOUNDARY : -BOUNDARY;
        }
        if (std::abs(std::abs(cubePos.y) - BOUNDARY) < EPSILON) {
            cubePos.y = (cubePos.y > 0.0) ? BOUNDARY : -BOUNDARY;
        }
        if (std::abs(std::abs(cubePos.z) - BOUNDARY) < EPSILON) {
            cubePos.z = (cubePos.z > 0.0) ? BOUNDARY : -BOUNDARY;
        }
    }
    
    glm::dvec3 cubeToSphere(const glm::dvec3& cubePos) {
        glm::dvec3 pos2 = cubePos * cubePos;
        glm::dvec3 spherePos;
        spherePos.x = cubePos.x * sqrt(1.0 - pos2.y * 0.5 - pos2.z * 0.5 + pos2.y * pos2.z / 3.0);
        spherePos.y = cubePos.y * sqrt(1.0 - pos2.x * 0.5 - pos2.z * 0.5 + pos2.x * pos2.z / 3.0);
        spherePos.z = cubePos.z * sqrt(1.0 - pos2.x * 0.5 - pos2.y * 0.5 + pos2.x * pos2.y / 3.0);
        return glm::normalize(spherePos);
    }
};

// ============================================================================
// MODULE 3: Terrain Sampling Consistency Test
// ============================================================================

class TerrainSamplingTester {
public:
    // Simplified terrain function (matching shader)
    double getTerrainHeight(const glm::dvec3& spherePos) {
        // Simple noise function for testing
        double n = sin(spherePos.x * 0.0001) * cos(spherePos.y * 0.0001) * sin(spherePos.z * 0.0001);
        return 100.0 * n; // 100 meter variation
    }
    
    // Test if two patches sample terrain consistently at shared boundary
    void testBoundaryConsistency(const PatchData& patch1, const PatchData& patch2) {
        std::cout << "\n=== Testing Terrain Sampling Consistency ===\n";
        std::cout << "Patch 1: Face " << patch1.faceIndex << " Level " << patch1.level << "\n";
        std::cout << "Patch 2: Face " << patch2.faceIndex << " Level " << patch2.level << "\n";
        
        VertexTransformEmulator emulator;
        
        // Test edge vertices
        std::vector<std::pair<glm::dvec2, glm::dvec2>> testPoints = {
            {{1.0, 0.0}, {0.0, 0.0}},  // Right edge of patch1, left edge of patch2
            {{1.0, 0.5}, {0.0, 0.5}},
            {{1.0, 1.0}, {0.0, 1.0}}
        };
        
        double maxHeightDiff = 0.0;
        
        for (const auto& [uv1, uv2] : testPoints) {
            glm::dvec3 worldPos1 = emulator.transformVertex(uv1, patch1.transform);
            glm::dvec3 worldPos2 = emulator.transformVertex(uv2, patch2.transform);
            
            double height1 = getTerrainHeight(glm::normalize(worldPos1));
            double height2 = getTerrainHeight(glm::normalize(worldPos2));
            
            double posDiff = glm::length(worldPos1 - worldPos2);
            double heightDiff = std::abs(height1 - height2);
            maxHeightDiff = std::max(maxHeightDiff, heightDiff);
            
            std::cout << "  UV(" << uv1.x << "," << uv1.y << ") vs UV(" << uv2.x << "," << uv2.y << ")\n";
            std::cout << "    Position diff: " << posDiff << " meters\n";
            std::cout << "    Height diff: " << heightDiff << " meters ";
            
            if (heightDiff < 0.01) {
                std::cout << "✓\n";
            } else {
                std::cout << "✗ INCONSISTENT!\n";
            }
        }
        
        std::cout << "Maximum height difference: " << maxHeightDiff << " meters\n";
    }
};

// ============================================================================
// MODULE 4: Patch Adjacency Analyzer
// ============================================================================

class PatchAdjacencyAnalyzer {
public:
    struct EdgeVertex {
        glm::dvec3 position;
        int patchId;
        int edge; // 0=left, 1=right, 2=bottom, 3=top
    };
    
    void analyzePatchSet(const std::vector<PatchData>& patches) {
        std::cout << "\n=== Patch Adjacency Analysis ===\n";
        
        VertexTransformEmulator emulator;
        std::map<std::string, std::vector<EdgeVertex>> spatialHash;
        
        // Collect all edge vertices
        for (size_t i = 0; i < patches.size(); i++) {
            const auto& patch = patches[i];
            
            // Sample edge vertices
            for (int j = 0; j <= 10; j++) {
                double t = j / 10.0;
                
                // Left edge
                addEdgeVertex(spatialHash, emulator.transformVertex({0.0, t}, patch.transform), i, 0);
                // Right edge  
                addEdgeVertex(spatialHash, emulator.transformVertex({1.0, t}, patch.transform), i, 1);
                // Bottom edge
                addEdgeVertex(spatialHash, emulator.transformVertex({t, 0.0}, patch.transform), i, 2);
                // Top edge
                addEdgeVertex(spatialHash, emulator.transformVertex({t, 1.0}, patch.transform), i, 3);
            }
        }
        
        // Find vertices that should be shared
        int sharedCount = 0;
        int gapCount = 0;
        double maxGap = 0.0;
        
        for (const auto& [hash, vertices] : spatialHash) {
            if (vertices.size() > 1) {
                sharedCount++;
                
                // Check if vertices at this location are actually identical
                for (size_t i = 1; i < vertices.size(); i++) {
                    double gap = glm::length(vertices[0].position - vertices[i].position);
                    if (gap > 0.01) {
                        gapCount++;
                        maxGap = std::max(maxGap, gap);
                        
                        if (gap > 1.0) {
                            std::cout << "  GAP FOUND: " << gap << " meters between:\n";
                            std::cout << "    Patch " << vertices[0].patchId 
                                     << " edge " << vertices[0].edge << "\n";
                            std::cout << "    Patch " << vertices[i].patchId 
                                     << " edge " << vertices[i].edge << "\n";
                        }
                    }
                }
            }
        }
        
        std::cout << "Shared vertex locations: " << sharedCount << "\n";
        std::cout << "Locations with gaps: " << gapCount << "\n";
        std::cout << "Maximum gap: " << maxGap << " meters\n";
    }
    
private:
    void addEdgeVertex(std::map<std::string, std::vector<EdgeVertex>>& hash,
                       const glm::dvec3& pos, int patchId, int edge) {
        // Hash position to 10cm grid
        int x = static_cast<int>(pos.x / 0.1);
        int y = static_cast<int>(pos.y / 0.1);
        int z = static_cast<int>(pos.z / 0.1);
        
        std::stringstream ss;
        ss << x << "," << y << "," << z;
        std::string key = ss.str();
        
        hash[key].push_back({pos, patchId, edge});
    }
};

// ============================================================================
// MODULE 5: Visual Debug Output
// ============================================================================

class VisualDebugger {
public:
    void generateDebugMesh(const std::vector<PatchData>& patches, const std::string& filename) {
        std::ofstream obj(filename);
        obj << "# Patch boundary debug mesh\n";
        obj << std::fixed << std::setprecision(6);
        
        VertexTransformEmulator emulator;
        int vertexIndex = 1;
        
        for (const auto& patch : patches) {
            obj << "# Patch Face=" << patch.faceIndex << " Level=" << patch.level << "\n";
            
            // Generate boundary wireframe
            std::vector<int> indices;
            
            // Sample boundary
            for (int i = 0; i <= 20; i++) {
                double t = i / 20.0;
                
                // Bottom edge
                glm::dvec3 v1 = emulator.transformVertex({t, 0.0}, patch.transform);
                obj << "v " << v1.x << " " << v1.y << " " << v1.z << "\n";
                indices.push_back(vertexIndex++);
                
                // Top edge
                glm::dvec3 v2 = emulator.transformVertex({t, 1.0}, patch.transform);
                obj << "v " << v2.x << " " << v2.y << " " << v2.z << "\n";
                indices.push_back(vertexIndex++);
                
                // Left edge
                glm::dvec3 v3 = emulator.transformVertex({0.0, t}, patch.transform);
                obj << "v " << v3.x << " " << v3.y << " " << v3.z << "\n";
                indices.push_back(vertexIndex++);
                
                // Right edge
                glm::dvec3 v4 = emulator.transformVertex({1.0, t}, patch.transform);
                obj << "v " << v4.x << " " << v4.y << " " << v4.z << "\n";
                indices.push_back(vertexIndex++);
            }
            
            // Create lines for boundary
            obj << "# Boundary lines\n";
            for (size_t i = 0; i < indices.size() - 1; i++) {
                obj << "l " << indices[i] << " " << indices[i + 1] << "\n";
            }
        }
        
        obj.close();
        std::cout << "Generated debug mesh: " << filename << "\n";
    }
};

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== MODULAR PATCH DIAGNOSTIC SYSTEM ===\n\n";
    
    PatchDataCapture capture;
    
    // Option 1: Generate test data
    if (argc > 1 && std::string(argv[1]) == "generate") {
        std::cout << "Generating test patch data...\n";
        
        // Create two adjacent test patches
        PatchData patch1;
        patch1.faceIndex = 2; // +Z face
        patch1.level = 2;
        patch1.center = glm::dvec3(0.75, 0.0, 1.0);
        patch1.size = 0.5;
        
        // Build transform for patch 1
        glm::dvec3 bl1(0.5, -0.5, 1.0);
        glm::dvec3 br1(1.0, -0.5, 1.0);
        glm::dvec3 tl1(0.5, 0.5, 1.0);
        
        patch1.transform = glm::dmat4(1.0);
        patch1.transform[0] = glm::dvec4(br1 - bl1, 0.0);
        patch1.transform[1] = glm::dvec4(tl1 - bl1, 0.0);
        patch1.transform[2] = glm::dvec4(0, 0, 1, 0);
        patch1.transform[3] = glm::dvec4(bl1, 1.0);
        
        // Create adjacent patch
        PatchData patch2;
        patch2.faceIndex = 0; // +X face (adjacent)
        patch2.level = 2;
        patch2.center = glm::dvec3(1.0, 0.0, 0.75);
        patch2.size = 0.5;
        
        // Build transform for patch 2
        glm::dvec3 bl2(1.0, -0.5, 0.5);
        glm::dvec3 br2(1.0, -0.5, 1.0);
        glm::dvec3 tl2(1.0, 0.5, 0.5);
        
        patch2.transform = glm::dmat4(1.0);
        patch2.transform[0] = glm::dvec4(0, 0, 0.5, 0.0); // Along Z
        patch2.transform[1] = glm::dvec4(0, 1, 0, 0.0);   // Along Y
        patch2.transform[2] = glm::dvec4(1, 0, 0, 0);     // Face normal
        patch2.transform[3] = glm::dvec4(bl2, 1.0);
        
        capture.capturePatch(patch1);
        capture.capturePatch(patch2);
        capture.saveToFile("patch_data.txt");
    }
    
    // Option 2: Load and test
    else {
        std::cout << "Loading patch data from file...\n";
        capture.loadFromFile("patch_data.txt");
        
        if (capture.patches.size() >= 2) {
            // Test 1: Vertex transformation
            std::cout << "\n=== TEST 1: Vertex Transformation ===\n";
            VertexTransformEmulator emulator;
            
            for (const auto& patch : capture.patches) {
                std::cout << "Patch Face=" << patch.faceIndex << " Level=" << patch.level << "\n";
                glm::dvec3 corner = emulator.transformVertex({0.0, 0.0}, patch.transform);
                std::cout << "  Bottom-left corner: " << glm::to_string(corner) << "\n";
            }
            
            // Test 2: Terrain sampling
            TerrainSamplingTester terrainTester;
            terrainTester.testBoundaryConsistency(capture.patches[0], capture.patches[1]);
            
            // Test 3: Adjacency analysis
            PatchAdjacencyAnalyzer adjacencyAnalyzer;
            adjacencyAnalyzer.analyzePatchSet(capture.patches);
            
            // Test 4: Visual debug output
            VisualDebugger visualDebugger;
            visualDebugger.generateDebugMesh(capture.patches, "patch_boundaries.obj");
        }
    }
    
    std::cout << "\n=== DIAGNOSTIC COMPLETE ===\n";
    std::cout << "Results saved to:\n";
    std::cout << "  - patch_data.txt (captured data)\n";
    std::cout << "  - patch_boundaries.obj (visual debug mesh)\n";
    
    return 0;
}