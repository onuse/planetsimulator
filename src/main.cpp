#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <csignal>
#include <atomic>

#include "core/octree.hpp"
#include "rendering/vulkan_renderer.hpp"
#include "core/camera.hpp"

// Global flag for clean shutdown
std::atomic<bool> g_shouldExit(false);

// Signal handler for Ctrl+C
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n[INFO] Ctrl+C received, shutting down gracefully..." << std::endl;
        g_shouldExit = true;
    }
}

// Note: InputState is now defined in vulkan_renderer.hpp

// Command line arguments
struct Config {
    float radius = 1000000.0f;   // 1000km radius - larger planet for better scale
    int maxDepth = 10;           // Reduced depth for better performance (still gives ~2m voxels)
    uint32_t seed = 42;          // Random seed
    int width = 1280;            // Window width
    int height = 720;            // Window height
    float autoTerminate = 0;     // Auto-terminate after N seconds (0 = disabled)
    float screenshotInterval = 0; // Screenshot interval in seconds (0 = disabled)
    bool quiet = false;          // Suppress verbose output
    bool vertexDump = false;     // Dump vertex data on exit for debugging
    bool autoZoom = false;       // Enable automatic zoom during screenshots
    bool usePresetView = false;  // Use preset camera position
    bool disableCulling = false; // Disable face culling for debugging
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
        } else if (arg == "-auto-zoom") {
            config.autoZoom = true;
        } else if (arg == "-preset-view") {
            config.usePresetView = true;
        } else if (arg == "-no-culling") {
            config.disableCulling = true;
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
                      << "  -vertex-dump            Dump vertex data on exit for debugging\n"
                      << "  -auto-zoom              Enable automatic zoom during screenshots\n"
                      << "  -preset-view            Use preset camera position (nice continent view)\n";
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
        auto loopStartTime = std::chrono::high_resolution_clock::now();
        auto lastScreenshot = loopStartTime;
        auto lastFrame = loopStartTime;
        
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
            float elapsed = std::chrono::duration<float>(currentTime - loopStartTime).count();
            float deltaTime = std::chrono::duration<float>(currentTime - lastFrame).count();
            lastFrame = currentTime;
            
            // PERFORMANCE: Disabled early frame debug logging
            // if (frameCount < 5) {
            //     std::cout << "DEBUG: Frame " << frameCount << ", elapsed=" << elapsed 
            //               << "s, shouldExit=" << shouldExit() << std::endl;
            // }
            
            // Check auto-terminate
            if (config.autoTerminate > 0 && elapsed >= config.autoTerminate) {
                std::cout << "Auto-terminating after " << elapsed << " seconds\n" << std::flush;
                break;
            }
            
            // Handle input FIRST so zoom changes take effect this frame
            handleInput();
            
            // Update simulation
            simulationTime += deltaTime * timeScale;
            planet.update(deltaTime);
            
            // Update camera (will apply zoom changes from handleInput)
            camera.update(deltaTime);
            
            // Update LOD based on camera position
            planet.updateLOD(camera.getPosition());
            
            // Render
            // std::cout << "=== ABOUT TO CALL renderer.render, frame=" << frameCount 
            //           << ", near=" << camera.getNearPlane() << ", far=" << camera.getFarPlane() << " ===\n";
            renderer.render(&planet, &camera);
            // std::cout << "=== RETURNED FROM renderer.render ===\n";
            
            // Screenshots with automatic zoom sequence
            if (config.screenshotInterval > 0 && frameCount > 0) {
                float screenshotElapsed = std::chrono::duration<float>(currentTime - lastScreenshot).count();
                // PERFORMANCE: Disabled screenshot check debug output
                // if (frameCount % 60 == 0) {
                //     std::cout << "Screenshot check: elapsed=" << screenshotElapsed 
                //               << "s, interval=" << config.screenshotInterval 
                //               << "s, frame=" << frameCount << std::endl << std::flush;
                // }
                if (screenshotElapsed >= config.screenshotInterval) {
                    std::cout << "Taking screenshot at " << elapsed << "s..." << std::endl;
                    takeScreenshot(elapsed, simulationTime);
                    lastScreenshot = currentTime;
                    
                    // Automatic zoom sequence if enabled
                    if (config.autoZoom) {
                        performAutomaticZoom();
                    }
                }
            }
            
            // Frame counter
            frameCount++;
            // PERFORMANCE: Less frequent frame stats (was every 300, now every 3000 frames)
            if (!config.quiet && frameCount % 3000 == 0) {  // Every 3000 frames (~100 seconds at 30fps)
                std::cout << "Frame " << frameCount 
                          << ", Time: " << elapsed << "s"
                          << ", Sim: " << simulationTime / 1000000.0f << " My"
                          << ", FPS: " << (1.0f / deltaTime)
                          << ", Nodes: " << renderer.getNodeCount() << "\n";
            }
            
            // Note: handleInput() moved to before camera.update()
        }
        
        cleanup();
    }
    
private:
    Config config;
    octree::OctreePlanet planet;
    rendering::VulkanRenderer renderer;
    core::Camera camera;
    
    void init() {
        std::cout << "=== INIT FUNCTION CALLED ===\n" << std::flush;
        
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
        
        // Set initial camera position
        if (config.usePresetView) {
            // Set to the nice continent view from the screenshot
            camera.setPosition(glm::vec3(7.188e6f, 4.266e6f, 9.374e6f));
            camera.lookAt(glm::vec3(0, 0, 0));
            camera.setFieldOfView(60.0f); // Match FOV from screenshot
            std::cout << "Camera set to preset view (nice continent/beach view)\n";
        } else {
            // Default view - far enough to see the planet properly
            // Use 3x radius to see the whole sphere
            float initialDistance = config.radius * 3.0f;
            std::cout << "=== SETTING CAMERA POSITION ===\n";
            std::cout << "Initial distance: " << initialDistance << " (altitude: " << (initialDistance - config.radius) << ")\n";
            // Position camera to look at land/coast instead of open ocean
            // Rotate to see interesting terrain features
            float angle = glm::radians(45.0f);  // Different angle to find land
            camera.setPosition(glm::vec3(
                initialDistance * sin(angle) * 0.9f,
                initialDistance * 0.3f,
                initialDistance * cos(angle) * 0.9f
            ));
            camera.lookAt(glm::vec3(0, 0, 0));
        }
        
        // Adjust near/far planes based on initial altitude
        float altitude = camera.getAltitude(glm::vec3(0, 0, 0), config.radius);
        std::cout << "=== CALLING autoAdjustClipPlanes with altitude=" << altitude << " ===\n";
        camera.autoAdjustClipPlanes(altitude);
        std::cout << "=== DONE CALLING autoAdjustClipPlanes ===\n";
        
        if (!config.quiet) {
            glm::vec3 pos = camera.getPosition();
            std::cout << "Camera positioned at: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
            std::cout << "Near plane: " << camera.getNearPlane() << ", Far plane: " << camera.getFarPlane() << "\n" << std::flush;
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
        return renderer.shouldClose() || g_shouldExit;
    }
    
    void handleInput() {
        // Update input state from previous frame FIRST (before polling new events)
        renderer.updateInput();
        
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
        
        // Auto-adjust near/far planes based on altitude
        camera.autoAdjustClipPlanes(altitude);
        
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
        
        // Sphere rotation with mouse (left-click drag) - orbital camera mode
        if (!imguiWantsMouse && input.mouseButtons[GLFW_MOUSE_BUTTON_LEFT]) {
            float sensitivity = 0.002f; // Radians per pixel
            // Orbit the camera around the sphere (effectively rotating the view)
            // Note: Y-axis inverted for natural mouse control (drag up = rotate up)
            camera.orbit(-input.mouseDelta.x * sensitivity, -input.mouseDelta.y * sensitivity);
        }
        
        // Camera rotation with mouse (right-click drag) - only if ImGui doesn't want mouse
        if (!imguiWantsMouse && input.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT]) {
            float sensitivity = 0.002f; // Radians per pixel
            camera.rotate(-input.mouseDelta.x * sensitivity, -input.mouseDelta.y * sensitivity);
        }
        
        // Zoom with scroll wheel - only if ImGui doesn't want mouse
        if (!imguiWantsMouse && input.scrollDelta.y != 0) {
            // Debug output - commented to reduce spam
            // std::cout << "[SCROLL] Delta: " << input.scrollDelta.y 
            //           << ", Altitude: " << altitude 
            //           << ", Radius: " << config.radius << std::endl;
            
            // Scale zoom speed based on altitude - MUCH slower when closer
            // altitude is already calculated above
            float altitudeRatio = altitude / config.radius;
            
            // Use exponential scaling for smooth speed reduction
            // This gives us very fine control near the surface
            float scaleFactor = 0.5f * std::pow(altitudeRatio * 10.0f, 2.0f);
            scaleFactor = std::max(0.001f, std::min(0.5f, scaleFactor));  // Clamp between 0.001 and 0.5
            
            // Additional safety: prevent zooming below a minimum altitude
            if (altitude < config.radius * 0.0001f && input.scrollDelta.y > 0) {
                // At very low altitude, only allow zooming out
                scaleFactor = 0.0f;
            }
            
            float zoomDelta = input.scrollDelta.y * scaleFactor;
            // Commented to reduce spam
            // std::cout << "[ZOOM] Calling camera.zoom(" << zoomDelta << ")" << std::endl;
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
        
        // Switch mesh generation pipeline with G key
        if (input.keys[GLFW_KEY_G] && !input.prevKeys[GLFW_KEY_G]) {
            // Cycle through pipeline modes
            auto& pipeline = rendering::VulkanRenderer::meshPipeline;
            if (pipeline == rendering::VulkanRenderer::MeshPipeline::CPU_ADAPTIVE) {
                pipeline = rendering::VulkanRenderer::MeshPipeline::GPU_COMPUTE;
                std::cout << "[G KEY] Switched to GPU_COMPUTE pipeline\n";
            } else if (pipeline == rendering::VulkanRenderer::MeshPipeline::GPU_COMPUTE) {
                pipeline = rendering::VulkanRenderer::MeshPipeline::GPU_WITH_CPU_VERIFY;
                std::cout << "[G KEY] Switched to GPU_WITH_CPU_VERIFY pipeline (debug mode)\n";
            } else {
                pipeline = rendering::VulkanRenderer::MeshPipeline::CPU_ADAPTIVE;
                std::cout << "[G KEY] Switched to CPU_ADAPTIVE pipeline (safe mode)\n";
            }
            std::cout << "NOTE: Move camera or zoom to trigger mesh regeneration with new pipeline\n";
        }
        
        // Flip front/back detail assignment for testing dual-detail LOD
        if (input.keys[GLFW_KEY_F] && !input.prevKeys[GLFW_KEY_F]) {
            // Toggle the flip flag
            rendering::VulkanRenderer::adaptiveSphereFlipFrontBack = !rendering::VulkanRenderer::adaptiveSphereFlipFrontBack;
            
            // Force mesh regeneration by slightly changing LOD 
            // (commented out - user needs to move camera slightly to trigger regeneration)
            
            if (!config.quiet) {
                if (rendering::VulkanRenderer::adaptiveSphereFlipFrontBack) {
                    std::cout << "[F KEY] Dual-detail FLIPPED: Back hemisphere now gets HIGH detail\n";
                } else {
                    std::cout << "[F KEY] Dual-detail NORMAL: Front hemisphere gets HIGH detail\n";
                }
                std::cout << "NOTE: Move camera slightly or zoom to trigger mesh regeneration\n";
            }
        }
        
        // Toggle face culling with C key - DISABLED FOR NOW
        // TODO: Add getLODManager() method to VulkanRenderer
        /*
        if (input.keys[GLFW_KEY_C] && !input.prevKeys[GLFW_KEY_C]) {
            if (renderer.getLODManager()) {
                auto* quadtree = renderer.getLODManager()->getQuadtree();
                if (quadtree) {
                    auto& config = quadtree->getConfig();
                    config.enableFaceCulling = !config.enableFaceCulling;
                    if (!config.quiet) {
                        std::cout << "Face culling: " << (config.enableFaceCulling ? "ENABLED" : "DISABLED") << "\n";
                    }
                }
            }
        }
        */
        
        // Exit with ESC
        if (input.keys[GLFW_KEY_ESCAPE]) {
            glfwSetWindowShouldClose(renderer.getWindow(), GLFW_TRUE);
        }
        
        // Note: updateInput() is now called at the beginning of handleInput()
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
    
    void performAutomaticZoom() {
        // Get current camera position
        glm::vec3 currentPos = camera.getPosition();
        glm::vec3 planetCenter(0, 0, 0);
        
        // Calculate direction from camera to planet center
        glm::vec3 dirToCenter = glm::normalize(planetCenter - currentPos);
        
        // Calculate current distance from planet center
        float currentDistance = glm::length(currentPos);
        
        // Move 25% closer to the planet (zoom in)
        float zoomFactor = 0.75f; // Keep 75% of distance = move 25% closer
        float newDistance = currentDistance * zoomFactor;
        
        // Don't get too close to the surface (now using double precision to fix rendering bug)
        float minDistance = config.radius + 10000.0f; // Stay at least 10km above surface
        if (newDistance < minDistance) {
            newDistance = minDistance;
            std::cout << "Reached minimum zoom distance (10km above surface)\n";
        }
        
        // Calculate new position
        glm::vec3 newPos = -dirToCenter * newDistance; // Negative because we're moving from center outward
        
        // Apply the new position
        camera.setPosition(newPos);
        camera.lookAt(planetCenter);
        
        // Update clip planes for new altitude
        float altitude = newDistance - config.radius;
        camera.autoAdjustClipPlanes(altitude);
        
        if (!config.quiet) {
            std::cout << "Auto-zoom: Distance " << currentDistance/1000.0f << "km -> " 
                      << newDistance/1000.0f << "km (altitude: " << altitude/1000.0f << "km)\n";
        }
    }
    
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

int main(int argc, char** argv) {
    std::cout << "Starting Octree Planet Simulator..." << std::endl;
    
    // Install signal handler for clean shutdown
    std::signal(SIGINT, signalHandler);
    
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