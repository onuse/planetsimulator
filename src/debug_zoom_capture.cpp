#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <glm/glm.hpp>
#include "core/octree.hpp"
#include "rendering/vulkan_renderer.hpp"
#include "core/camera.hpp"

class ZoomDiagnostics {
public:
    ZoomDiagnostics(float planetRadius) : radius(planetRadius) {}
    
    void run() {
        std::cout << "=== ZOOM LEVEL DIAGNOSTICS ===" << std::endl;
        std::cout << "This will capture rendering data at multiple zoom levels\n" << std::endl;
        
        // Initialize components
        octree::OctreePlanet planet(radius, 6);
        std::cout << "Generating planet..." << std::endl;
        planet.generate(42);
        
        rendering::VulkanRenderer renderer(1280, 720);
        if (!renderer.initialize()) {
            std::cerr << "Failed to initialize renderer" << std::endl;
            return;
        }
        
        core::Camera camera;
        
        // Define zoom levels to test
        struct ZoomLevel {
            float distance;  // Multiple of planet radius
            std::string description;
            glm::vec3 position;
        };
        
        ZoomLevel levels[] = {
            {1.2f, "Very close (surface)", glm::vec3(0, 0, 0)},
            {1.5f, "Close (near surface)", glm::vec3(0, 0, 0)},
            {2.0f, "Medium (full planet)", glm::vec3(0, 0, 0)},
            {2.5f, "Default view", glm::vec3(0, 0, 0)},
            {3.0f, "Far", glm::vec3(0, 0, 0)},
            {5.0f, "Very far", glm::vec3(0, 0, 0)},
            {10.0f, "Extreme distance", glm::vec3(0, 0, 0)}
        };
        
        // Calculate positions
        for (auto& level : levels) {
            level.position = glm::vec3(0, 0, radius * level.distance);
        }
        
        // Create diagnostics file
        std::ofstream report("zoom_diagnostics.txt");
        report << "ZOOM LEVEL RENDERING DIAGNOSTICS\n";
        report << "=================================\n\n";
        report << "Planet radius: " << radius << " meters\n\n";
        
        // Test each zoom level
        for (const auto& level : levels) {
            std::cout << "\nTesting: " << level.description 
                      << " (distance: " << level.distance << "R)" << std::endl;
            
            // Set camera position
            camera.setPosition(level.position);
            camera.lookAt(glm::vec3(0, 0, 0));
            
            // Render a few frames to stabilize
            for (int i = 0; i < 3; i++) {
                renderer.render(&planet, &camera);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            
            // Capture metrics
            report << "ZOOM LEVEL: " << level.description << "\n";
            report << "Distance: " << level.distance << " x radius = " 
                   << (level.distance * radius / 1000.0f) << " km\n";
            report << "Camera position: (" << level.position.x << ", " 
                   << level.position.y << ", " << level.position.z << ")\n";
            
            // Get rendering stats
            uint32_t nodeCount = renderer.getNodeCount();
            report << "Visible nodes: " << nodeCount << "\n";
            
            // Analyze what materials are visible at this distance
            analyzeVisibleMaterials(planet, level.position, report);
            
            // Check LOD distribution
            analyzeLODDistribution(planet, level.position, report);
            
            report << "----------------------------------------\n\n";
            
            // Take screenshot if possible
            std::string filename = "zoom_" + std::to_string(static_cast<int>(level.distance * 10)) + ".png";
            if (renderer.captureScreenshot(filename)) {
                std::cout << "  Screenshot saved: " << filename << std::endl;
            }
        }
        
        // Test rapid zoom to catch transition artifacts
        std::cout << "\nTesting rapid zoom transitions..." << std::endl;
        report << "RAPID ZOOM TEST\n";
        report << "===============\n";
        
        for (float dist = 10.0f; dist >= 1.5f; dist -= 0.5f) {
            glm::vec3 pos(0, 0, radius * dist);
            camera.setPosition(pos);
            camera.lookAt(glm::vec3(0, 0, 0));
            
            renderer.render(&planet, &camera);
            
            report << "Distance " << dist << "R: " << renderer.getNodeCount() << " nodes\n";
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        report.close();
        std::cout << "\nDiagnostics complete. Report saved to zoom_diagnostics.txt" << std::endl;
    }
    
private:
    float radius;
    
    void analyzeVisibleMaterials(const octree::OctreePlanet& planet,
                                 const glm::vec3& viewPos,
                                 std::ofstream& report) {
        // Sample rays from camera to planet
        int airCount = 0, rockCount = 0, waterCount = 0, magmaCount = 0;
        
        // Cast sample rays in a grid pattern
        const int samples = 10;
        for (int i = 0; i < samples; i++) {
            for (int j = 0; j < samples; j++) {
                float u = (float)i / samples - 0.5f;
                float v = (float)j / samples - 0.5f;
                
                glm::vec3 rayDir = glm::normalize(glm::vec3(u, v, -1.0f));
                
                // Simple ray-sphere test
                float t = raySphereIntersect(viewPos, rayDir, glm::vec3(0), radius);
                if (t > 0) {
                    glm::vec3 hitPoint = viewPos + rayDir * t;
                    
                    // Find material at hit point (simplified)
                    float dist = glm::length(hitPoint);
                    if (dist < radius * 0.5f) {
                        magmaCount++;
                    } else if (dist < radius * 0.95f) {
                        rockCount++;
                    } else if (dist < radius * 1.05f) {
                        // Surface - should be mix
                        if ((i + j) % 3 == 0) {
                            waterCount++;
                        } else {
                            rockCount++;
                        }
                    } else {
                        airCount++;
                    }
                }
            }
        }
        
        report << "Ray sampling results (100 rays):\n";
        report << "  Air: " << airCount << "\n";
        report << "  Rock: " << rockCount << "\n";
        report << "  Water: " << waterCount << "\n";
        report << "  Magma: " << magmaCount << "\n";
    }
    
    void analyzeLODDistribution(const octree::OctreePlanet& planet,
                                const glm::vec3& viewPos,
                                std::ofstream& report) {
        // Analyze LOD levels that would be selected
        float viewDist = glm::length(viewPos);
        
        report << "LOD Analysis:\n";
        report << "  View distance: " << (viewDist / radius) << "R\n";
        
        // Sample LOD decisions
        float nodeSizes[] = {radius * 0.01f, radius * 0.05f, radius * 0.1f, radius * 0.5f};
        for (float nodeSize : nodeSizes) {
            float ratio = viewDist / nodeSize;
            int lod;
            if (ratio < 10.0f) lod = 0;
            else if (ratio < 50.0f) lod = 1;
            else if (ratio < 200.0f) lod = 2;
            else if (ratio < 1000.0f) lod = 3;
            else lod = 4;
            
            report << "  Node size " << (nodeSize/radius) << "R would use LOD " << lod << "\n";
        }
    }
    
    float raySphereIntersect(const glm::vec3& origin, const glm::vec3& dir,
                             const glm::vec3& center, float radius) {
        glm::vec3 oc = origin - center;
        float a = glm::dot(dir, dir);
        float b = 2.0f * glm::dot(oc, dir);
        float c = glm::dot(oc, oc) - radius * radius;
        float disc = b * b - 4 * a * c;
        
        if (disc < 0) return -1;
        
        float t1 = (-b - sqrt(disc)) / (2 * a);
        float t2 = (-b + sqrt(disc)) / (2 * a);
        
        if (t1 > 0) return t1;
        if (t2 > 0) return t2;
        return -1;
    }
};

int main(int argc, char* argv[]) {
    float radius = 6371000.0f;
    if (argc > 1) {
        radius = std::stof(argv[1]);
    }
    
    try {
        ZoomDiagnostics diag(radius);
        diag.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}