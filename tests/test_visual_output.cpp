#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/spherical_quadtree.hpp"
#include "core/density_field.hpp"
#include "rendering/cpu_vertex_generator.hpp"

void generateASCIIVisualization() {
    std::cout << "\n=== ASCII PLANET VISUALIZATION ===\n\n";
    
    // Create quadtree
    core::SphericalQuadtree::Config config;
    config.planetRadius = 6371000.0f;
    config.maxLevel = 5;  // Lower for ASCII viz
    config.enableFaceCulling = false;
    
    auto densityField = std::make_shared<core::DensityField>(config.planetRadius);
    core::SphericalQuadtree quadtree(config, densityField);
    
    // Update with a camera position
    glm::vec3 viewPos(config.planetRadius * 1.5f, config.planetRadius * 0.5f, config.planetRadius * 0.5f);
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 1000.0f, 100000000.0f);
    glm::mat4 viewProj = proj * view;
    
    quadtree.update(viewPos, viewProj, 0.016f);
    
    auto patches = quadtree.getVisiblePatches();
    std::cout << "Rendering " << patches.size() << " patches\n\n";
    
    // Generate vertices for each patch
    rendering::CPUVertexGenerator::Config genConfig;
    genConfig.gridResolution = 17;  // Lower resolution for ASCII
    genConfig.planetRadius = config.planetRadius;
    
    rendering::CPUVertexGenerator generator(genConfig);
    
    // Create ASCII grid (80x40)
    const int WIDTH = 80;
    const int HEIGHT = 40;
    std::vector<std::vector<char>> screen(HEIGHT, std::vector<char>(WIDTH, ' '));
    std::vector<std::vector<float>> zBuffer(HEIGHT, std::vector<float>(WIDTH, -1e9f));
    
    // Collect all vertices
    std::vector<glm::vec4> projectedVerts;
    
    for (const auto& patch : patches) {
        auto mesh = generator.generatePatchMesh(patch, patch.patchTransform);
        
        for (const auto& vertex : mesh.vertices) {
            // Project vertex to screen space
            glm::vec4 worldPos(vertex.position, 1.0f);
            glm::vec4 clipPos = viewProj * worldPos;
            
            if (clipPos.w > 0) {
                // Perspective divide
                glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
                
                // Convert to screen coordinates
                int x = (ndcPos.x + 1.0f) * 0.5f * WIDTH;
                int y = (1.0f - ndcPos.y) * 0.5f * HEIGHT;
                
                if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                    if (ndcPos.z > zBuffer[y][x]) {
                        zBuffer[y][x] = ndcPos.z;
                        
                        // Choose character based on face ID and depth
                        char c = '.';
                        if (patch.faceId == 0) c = '+';
                        else if (patch.faceId == 1) c = '-';
                        else if (patch.faceId == 2) c = '^';
                        else if (patch.faceId == 3) c = 'v';
                        else if (patch.faceId == 4) c = '>';
                        else if (patch.faceId == 5) c = '<';
                        
                        // Vary intensity by depth
                        if (ndcPos.z > 0.9f) c = '#';
                        else if (ndcPos.z > 0.7f) c = '*';
                        else if (ndcPos.z > 0.5f && c == '.') c = 'o';
                        
                        screen[y][x] = c;
                    }
                }
            }
        }
    }
    
    // Print the ASCII visualization
    std::cout << "+" << std::string(WIDTH, '-') << "+\n";
    for (int y = 0; y < HEIGHT; y++) {
        std::cout << "|";
        for (int x = 0; x < WIDTH; x++) {
            std::cout << screen[y][x];
        }
        std::cout << "|\n";
    }
    std::cout << "+" << std::string(WIDTH, '-') << "+\n";
    
    // Legend
    std::cout << "\nLegend:\n";
    std::cout << "  + = Face +X    - = Face -X\n";
    std::cout << "  ^ = Face +Y    v = Face -Y\n";
    std::cout << "  > = Face +Z    < = Face -Z\n";
    std::cout << "  # = Near       * = Mid      o = Far\n";
    
    // Stats
    int filledPixels = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (screen[y][x] != ' ') filledPixels++;
        }
    }
    
    float coverage = (float)filledPixels / (WIDTH * HEIGHT) * 100.0f;
    std::cout << "\nScreen coverage: " << std::fixed << std::setprecision(1) 
              << coverage << "%\n";
    
    if (coverage < 10.0f) {
        std::cout << "WARNING: Very low coverage - planet may be too small or off-screen\n";
    } else if (coverage > 80.0f) {
        std::cout << "WARNING: Very high coverage - planet may be too close\n";
    } else {
        std::cout << "Good coverage - planet is visible!\n";
    }
}

int main() {
    try {
        generateASCIIVisualization();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}