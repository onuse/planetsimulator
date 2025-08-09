#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <string>
#include <filesystem>

#include "core/octree.hpp"
#include "rendering/vulkan_renderer.hpp"
#include "core/camera.hpp"

// Note: InputState is now defined in vulkan_renderer.hpp

// Command line arguments
struct Config {
    float radius = 6371000.0f;  // Earth radius in meters
    int maxDepth = 10;           // Octree depth (10 for good balance of quality/performance)
    uint32_t seed = 42;          // Random seed
    int width = 1280;            // Window width
    int height = 720;            // Window height
    float autoTerminate = 0;     // Auto-terminate after N seconds (0 = disabled)
    float screenshotInterval = 0; // Screenshot interval in seconds (0 = disabled)
    bool quiet = false;          // Suppress verbose output
    bool vertexDump = false;      // Dump vertex data on exit for debugging
    // Hierarchical GPU octree is always enabled - no config option
};

Config parseArgs(int argc, char** argv) {
    Config config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-radius" && i + 1 < argc) {
            config.radius = std::stof(argv[++i]);
        } else if (arg == "-max-depth" && i + 1 < argc) {
            config.maxDepth = std::stoi(argv[++i]);
        } else if (arg == "-seed" && i + 1 < argc) {
            config.seed = std::stoul(argv[++i]);
        } else if (arg == "-width" && i + 1 < argc) {
            config.width = std::stoi(argv[++i]);
        } else if (arg == "-height" && i + 1 < argc) {
            config.height = std::stoi(argv[++i]);
        } else if (arg == "-auto-terminate" && i + 1 < argc) {
            config.autoTerminate = std::stof(argv[++i]);
        } else if (arg == "-screenshot-interval" && i + 1 < argc) {
            config.screenshotInterval = std::stof(argv[++i]);
        } else if (arg == "-quiet") {
            config.quiet = true;
        } else if (arg == "-vertex-dump") {
            config.vertexDump = true;
        } else if (arg == "-gpu-octree" || arg == "-no-gpu-octree") {
            std::cout << "Warning: GPU octree flags are deprecated. Hierarchical GPU octree is always enabled.\n";
        } else if (arg == "-help" || arg == "-h") {
            std::cout << "Usage: octree_planet [options]\n"
                      << "Options:\n"
                      << "  -radius <meters>        Planet radius (default: 6371000)\n"
                      << "  -max-depth <depth>      Octree max depth (default: 10)\n"
                      << "  -seed <seed>            Random seed (default: 42)\n"
                      << "  -width <pixels>         Window width (default: 1280)\n"
                      << "  -height <pixels>        Window height (default: 720)\n"
                      << "  -auto-terminate <sec>   Exit after N seconds (default: 0 = disabled)\n"
                      << "  -screenshot-interval <sec> Screenshot interval (default: 0 = disabled)\n"
                      << "  -quiet                  Suppress verbose output\n"
                      << "  -vertex-dump            Dump vertex data on exit for debugging\n";
            std::exit(0);
        }
    }
    
    return config;
}

class OctreePlanetApp {
public:
    OctreePlanetApp(const Config& config) 
        : config(config),
          planet(config.radius, config.maxDepth),
          renderer(config.width, config.height),
          camera(config.width, config.height) {
        
        if (!config.quiet) {
            std::cout << "Octree Planet Simulator (C++ Version)\n"
                      << "=====================================\n"
                      << "Radius: " << config.radius << " meters\n"
                      << "Max Depth: " << config.maxDepth << "\n"
                      << "Seed: " << config.seed << "\n"
                      << "Resolution: " << config.width << "x" << config.height << "\n";
                      
            if (config.autoTerminate > 0) {
                std::cout << "Auto-terminate: " << config.autoTerminate << " seconds\n";
            }
            if (config.screenshotInterval > 0) {
                std::cout << "Screenshots: every " << config.screenshotInterval << " seconds\n";
            }
        }
    }
    
    void run() {
        // Initialize
        if (!config.quiet) {
            std::cout << "Starting initialization...\n" << std::flush;
        }
        init();
        if (!config.quiet) {
            std::cout << "Initialization complete, entering main loop...\n" << std::flush;
        }
        
        // Main loop
        auto startTime = std::chrono::high_resolution_clock::now();
        auto lastScreenshot = startTime;
        auto lastFrame = startTime;
        
        int frameCount = 0;
        float simulationTime = 0.0f; // In million years
        const float timeScale = 1000000.0f; // 1 second = 1 million years
        
        if (!config.quiet) {
            std::cout << "Entering render loop..." << std::endl;
        }
        
        std::cout << "DEBUG: Starting main loop, auto-terminate=" << config.autoTerminate 
                  << ", screenshot-interval=" << config.screenshotInterval << std::endl;
        
        while (!shouldExit()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float elapsed = std::chrono::duration<float>(currentTime - startTime).count();
            float deltaTime = std::chrono::duration<float>(currentTime - lastFrame).count();
            lastFrame = currentTime;
            
            if (frameCount < 5) {
                std::cout << "DEBUG: Frame " << frameCount << ", elapsed=" << elapsed 
                          << "s, shouldExit=" << shouldExit() << std::endl;
            }
            
            // Check auto-terminate
            if (config.autoTerminate > 0 && elapsed >= config.autoTerminate) {
                std::cout << "Auto-terminating after " << elapsed << " seconds\n" << std::flush;
                break;
            }
            
            // Update simulation
            simulationTime += deltaTime * timeScale;
            planet.update(deltaTime);
            
            // Update camera
            camera.update(deltaTime);
            
            // Update LOD based on camera position
            planet.updateLOD(camera.getPosition());
            
            // Render
            if (frameCount < 3) {
                // std::cout << "Main loop: calling renderer.render (frame " << frameCount << ")" << std::endl;
            }
            renderer.render(&planet, &camera);
            if (frameCount < 3) {
                // std::cout << "Main loop: renderer.render returned" << std::endl;
            }
            
            // Screenshots (only after first frame to ensure something is rendered)
            if (config.screenshotInterval > 0 && frameCount > 0) {
                float screenshotElapsed = std::chrono::duration<float>(currentTime - lastScreenshot).count();
                // Debug output
                if (frameCount % 60 == 0) {
                    std::cout << "Screenshot check: elapsed=" << screenshotElapsed 
                              << "s, interval=" << config.screenshotInterval 
                              << "s, frame=" << frameCount << std::endl << std::flush;
                }
                if (screenshotElapsed >= config.screenshotInterval) {
                    std::cout << "Taking screenshot at " << elapsed << "s..." << std::endl;
                    takeScreenshot(elapsed, simulationTime);
                    lastScreenshot = currentTime;
                }
            }
            
            // Frame counter
            frameCount++;
            if (!config.quiet && frameCount % 30 == 0) {  // More frequent output
                std::cout << "Frame " << frameCount 
                          << ", Time: " << elapsed << "s"
                          << ", Sim: " << simulationTime / 1000000.0f << " My"
                          << ", FPS: " << (1.0f / deltaTime)
                          << ", Nodes: " << renderer.getNodeCount() << "\n";
            }
            
            // Handle input
            handleInput();
        }
        
        cleanup();
    }
    
private:
    Config config;
    octree::OctreePlanet planet;
    rendering::VulkanRenderer renderer;
    core::Camera camera;
    
    void init() {
        // Clean up old screenshots if we're taking new ones
        if (config.screenshotInterval > 0) {
            cleanupOldScreenshots();
        }
        
        // Generate planet
        if (!config.quiet) {
            std::cout << "Generating planet...\n" << std::flush;
        }
        try {
            planet.generate(config.seed);
            if (!config.quiet) {
                std::cout << "Planet generated successfully\n" << std::flush;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to generate planet: " << e.what() << std::endl;
            throw;
        }
        
        // Initialize renderer
        if (!config.quiet) {
            std::cout << "Initializing Vulkan renderer...\n" << std::flush;
        }
        try {
            // Hierarchical GPU octree is always enabled
            if (!config.quiet) {
                std::cout << "Hierarchical GPU octree rendering enabled\n" << std::flush;
            }
            
            if (!renderer.initialize()) {
                throw std::runtime_error("Failed to initialize renderer");
            }
            if (!config.quiet) {
                std::cout << "Vulkan renderer initialized successfully\n" << std::flush;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize renderer: " << e.what() << std::endl;
            throw;
        }
        
        // Set initial camera position - back away from planet to see whole sphere
        // 1.6x radius gives a good close view of the planet globe
        float initialDistance = config.radius * 1.6f;
        camera.setPosition(glm::vec3(initialDistance * 0.7f, initialDistance * 0.3f, initialDistance * 0.6f));
        camera.lookAt(glm::vec3(0, 0, 0));
        if (!config.quiet) {
            glm::vec3 pos = camera.getPosition();
            std::cout << "Camera positioned at: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n" << std::flush;
        }
    }
    
    void cleanup() {
        // Dump vertex data if requested
        if (config.vertexDump) {
            std::cout << "Dumping vertex data before exit...\n";
            renderer.dumpVertexData();
        }
        // cleanup handled by destructor
    }
    
    bool shouldExit() {
        return renderer.shouldClose();
    }
    
    void handleInput() {
        renderer.pollEvents();
        
        // Get input state from renderer
        const auto& input = renderer.getInputState();
        
        // Calculate frame time for speed adjustment
        static auto lastTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Base movement speed - adjust based on camera altitude
        float moveSpeed = 1000.0f; // meters per second
        float altitude = camera.getAltitude(glm::vec3(0, 0, 0), config.radius);
        
        // Scale speed based on altitude (move faster when far away)
        if (altitude > config.radius * 0.1f) {
            moveSpeed = altitude * 0.1f;
        } else if (altitude > 1000.0f) {
            moveSpeed = altitude * 0.01f;
        }
        
        // Speed boost with shift
        if (input.keys[GLFW_KEY_LEFT_SHIFT] || input.keys[GLFW_KEY_RIGHT_SHIFT]) {
            moveSpeed *= 10.0f;
        }
        
        // Apply deltaTime to make movement frame-rate independent
        float frameMoveSpeed = moveSpeed * deltaTime;
        
        // Only process camera input if ImGui doesn't want it
        bool imguiWantsMouse = false; // TODO: Get from ImGui manager
        bool imguiWantsKeyboard = false; // TODO: Get from ImGui manager
        
        // Camera movement (WASD + QE) - only if ImGui doesn't want keyboard
        if (!imguiWantsKeyboard) {
            if (input.keys[GLFW_KEY_W]) camera.moveForward(frameMoveSpeed);
            if (input.keys[GLFW_KEY_S]) camera.moveForward(-frameMoveSpeed);
            if (input.keys[GLFW_KEY_A]) camera.moveRight(-frameMoveSpeed);
            if (input.keys[GLFW_KEY_D]) camera.moveRight(frameMoveSpeed);
            if (input.keys[GLFW_KEY_Q]) camera.moveUp(-frameMoveSpeed);
            if (input.keys[GLFW_KEY_E]) camera.moveUp(frameMoveSpeed);
        }
        
        // Camera rotation with mouse (right-click drag) - only if ImGui doesn't want mouse
        if (!imguiWantsMouse && input.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT]) {
            float sensitivity = 0.002f; // Radians per pixel
            camera.rotate(-input.mouseDelta.x * sensitivity, -input.mouseDelta.y * sensitivity);
        }
        
        // Zoom with scroll wheel - only if ImGui doesn't want mouse
        if (!imguiWantsMouse && input.scrollDelta.y != 0) {
            // Pass scroll delta directly to zoom function
            // Positive scroll (up) should zoom in (negative delta for zoom function)
            // Negative scroll (down) should zoom out (positive delta for zoom function)
            float zoomDelta = -input.scrollDelta.y * 0.5f; // Invert and scale
            camera.zoom(zoomDelta);
        }
        
        // Reset camera with R key - position to see whole planet
        if (input.keys[GLFW_KEY_R] && !input.prevKeys[GLFW_KEY_R]) {
            float resetDistance = config.radius * 2.5f;
            camera.setPosition(glm::vec3(resetDistance * 0.7f, resetDistance * 0.3f, resetDistance * 0.6f));
            camera.lookAt(glm::vec3(0, 0, 0));
            if (!config.quiet) {
                std::cout << "Camera reset\n";
            }
        }
        
        // Screenshot with P key
        if (input.keys[GLFW_KEY_P] && !input.prevKeys[GLFW_KEY_P]) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - startTime).count();
            takeScreenshot(elapsed, 0);
        }
        
        // Visualization modes (1-8 keys)
        for (int i = 0; i < 8; i++) {
            if (input.keys[GLFW_KEY_1 + i] && !input.prevKeys[GLFW_KEY_1 + i]) {
                renderer.setRenderMode(i);
                if (!config.quiet) {
                    const char* modes[] = {
                        "Material", "Temperature", "Velocity", "Age",
                        "Plate IDs", "Stress", "Density", "Elevation"
                    };
                    std::cout << "Visualization mode: " << modes[i] << "\n";
                }
            }
        }
        
        // Toggle orbital/free-fly camera mode with TAB
        if (input.keys[GLFW_KEY_TAB] && !input.prevKeys[GLFW_KEY_TAB]) {
            if (camera.getMode() == core::CameraMode::Orbital) {
                camera.setMode(core::CameraMode::FreeFly);
                if (!config.quiet) {
                    std::cout << "Camera mode: Free-fly\n";
                }
            } else {
                camera.setMode(core::CameraMode::Orbital);
                if (!config.quiet) {
                    std::cout << "Camera mode: Orbital\n";
                }
            }
        }
        
        // Exit with ESC
        if (input.keys[GLFW_KEY_ESCAPE]) {
            glfwSetWindowShouldClose(renderer.getWindow(), GLFW_TRUE);
        }
        
        // Update input state for next frame
        renderer.updateInput();
    }
    
    void cleanupOldScreenshots() {
        std::filesystem::path screenshotDir = std::filesystem::current_path() / "screenshot_dev";
        
        if (std::filesystem::exists(screenshotDir)) {
            int count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(screenshotDir)) {
                if (entry.path().extension() == ".png") {
                    std::filesystem::remove(entry.path());
                    count++;
                }
            }
            if (count > 0 && !config.quiet) {
                std::cout << "Cleaned up " << count << " old screenshots from " << screenshotDir.string() << "\n";
            }
        }
    }
    
    void takeScreenshot(float realTime, float simTime) {
        // Format: screenshot_Xs_YMy.png where X is real time, Y is simulation time in million years
        char filename[256];
        snprintf(filename, sizeof(filename), "screenshot_%.0fs_%.0fMy.png", 
                 realTime, simTime / 1000000.0f);
        
        bool success = renderer.captureScreenshot(filename);
        
        if (!config.quiet) {
            if (success) {
                std::cout << "Screenshot saved: " << filename << "\n";
            } else {
                std::cout << "Screenshot FAILED: " << filename << "\n";
            }
        }
    }
    
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

int main(int argc, char** argv) {
    std::cout << "Starting Octree Planet Simulator..." << std::endl;
    try {
        Config config = parseArgs(argc, argv);
        OctreePlanetApp app(config);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}