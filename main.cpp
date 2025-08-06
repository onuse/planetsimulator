#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#include "core/octree.hpp"
#include "core/camera.hpp"
#include "rendering/vulkan_renderer.hpp"
#include "utils/screenshot.hpp"

// Program options
struct Options {
    uint32_t windowWidth = 1280;
    uint32_t windowHeight = 720;
    float planetRadius = 6371000.0f; // Earth radius in meters
    int maxOctreeDepth = 10;
    uint32_t seed = 42;
    int autoTerminate = 0;        // Seconds, 0 = disabled
    int screenshotInterval = 0;   // Seconds, 0 = disabled
    bool quiet = false;
};

Options parseArguments(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-width" && i + 1 < argc) {
            opts.windowWidth = std::stoi(argv[++i]);
        } else if (arg == "-height" && i + 1 < argc) {
            opts.windowHeight = std::stoi(argv[++i]);
        } else if (arg == "-radius" && i + 1 < argc) {
            opts.planetRadius = std::stof(argv[++i]);
        } else if (arg == "-max-depth" && i + 1 < argc) {
            opts.maxOctreeDepth = std::stoi(argv[++i]);
        } else if (arg == "-seed" && i + 1 < argc) {
            opts.seed = std::stoi(argv[++i]);
        } else if (arg == "-auto-terminate" && i + 1 < argc) {
            opts.autoTerminate = std::stoi(argv[++i]);
        } else if (arg == "-screenshot-interval" && i + 1 < argc) {
            opts.screenshotInterval = std::stoi(argv[++i]);
        } else if (arg == "-quiet") {
            opts.quiet = true;
        } else if (arg == "-help" || arg == "--help") {
            std::cout << "Octree Planet Simulator\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -width <n>              Window width (default: 1280)\n";
            std::cout << "  -height <n>             Window height (default: 720)\n";
            std::cout << "  -radius <n>             Planet radius in meters (default: 6371000)\n";
            std::cout << "  -max-depth <n>          Maximum octree depth (default: 10)\n";
            std::cout << "  -seed <n>               Random seed (default: 42)\n";
            std::cout << "  -auto-terminate <n>     Exit after n seconds (0 = disabled)\n";
            std::cout << "  -screenshot-interval <n> Take screenshots every n seconds (0 = disabled)\n";
            std::cout << "  -quiet                  Disable verbose output\n";
            std::cout << "  -help                   Show this help message\n";
            std::cout << "\nKeyboard controls:\n";
            std::cout << "  WASD        - Move camera\n";
            std::cout << "  Mouse       - Look around\n";
            std::cout << "  Scroll      - Zoom in/out\n";
            std::cout << "  1-8         - Change visualization mode\n";
            std::cout << "  Space       - Toggle camera mode\n";
            std::cout << "  P           - Pause simulation\n";
            std::cout << "  +/-         - Speed up/slow down simulation\n";
            std::cout << "  F1          - Toggle wireframe\n";
            std::cout << "  F11         - Toggle fullscreen\n";
            std::cout << "  ESC         - Exit\n";
            std::exit(0);
        }
    }
    
    return opts;
}

class Application {
public:
    Application(const Options& opts) : options(opts) {}
    
    int run() {
        try {
            initialize();
            mainLoop();
            cleanup();
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
        
        return EXIT_SUCCESS;
    }
    
private:
    Options options;
    std::unique_ptr<octree::OctreePlanet> planet;
    std::unique_ptr<core::Camera> camera;
    std::unique_ptr<rendering::VulkanRenderer> renderer;
    
    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastScreenshotTime;
    std::chrono::steady_clock::time_point lastFrameTime;
    float simulationTime = 0.0f;      // Simulation time in seconds
    float simulationSpeed = 1.0f;     // Time multiplier
    bool paused = false;
    
    // Stats
    int frameCount = 0;
    float fpsUpdateTimer = 0.0f;
    float currentFPS = 0.0f;
    
    void initialize() {
        if (!options.quiet) {
            std::cout << "Initializing Octree Planet Simulator...\n";
            std::cout << "  Planet radius: " << options.planetRadius / 1000.0f << " km\n";
            std::cout << "  Max octree depth: " << options.maxOctreeDepth << "\n";
            std::cout << "  Random seed: " << options.seed << "\n";
        }
        
        // Initialize screenshot system (creates/cleans screenshot_dev folder)
        utils::Screenshot::initialize();
        
        // Create planet
        planet = std::make_unique<octree::OctreePlanet>(options.planetRadius, options.maxOctreeDepth);
        planet->generate(options.seed);
        
        // Create camera
        camera = std::make_unique<core::Camera>(options.windowWidth, options.windowHeight);
        
        // Position camera to see the whole planet
        float viewDistance = options.planetRadius * 3.0f; // 3x planet radius
        camera->setPosition(glm::vec3(0.0f, 0.0f, viewDistance));
        camera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        
        // Create renderer
        renderer = std::make_unique<rendering::VulkanRenderer>(options.windowWidth, options.windowHeight);
        if (!renderer->initialize()) {
            throw std::runtime_error("Failed to initialize Vulkan renderer");
        }
        
        // Initialize timing
        startTime = std::chrono::steady_clock::now();
        lastScreenshotTime = startTime;
        lastFrameTime = startTime;
        
        if (!options.quiet) {
            std::cout << "Initialization complete!\n\n";
        }
    }
    
    void mainLoop() {
        while (!renderer->shouldClose()) {
            auto currentTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;
            
            // Update timers
            float elapsedTime = std::chrono::duration<float>(currentTime - startTime).count();
            
            // Check auto-terminate
            if (options.autoTerminate > 0 && elapsedTime >= options.autoTerminate) {
                if (!options.quiet) {
                    std::cout << "\nAuto-terminating after " << options.autoTerminate << " seconds\n";
                }
                break;
            }
            
            // Handle input and events
            renderer->pollEvents();
            handleInput(deltaTime);
            
            // Update simulation
            if (!paused) {
                float simDelta = deltaTime * simulationSpeed;
                simulationTime += simDelta;
                planet->update(simDelta);
            }
            
            // Update camera
            camera->update(deltaTime);
            
            // Update LOD based on camera position
            planet->updateLOD(camera->getPosition());
            
            // Render frame
            renderer->render(planet.get(), camera.get());
            
            // Handle screenshots
            if (options.screenshotInterval > 0) {
                float screenshotElapsed = std::chrono::duration<float>(currentTime - lastScreenshotTime).count();
                if (screenshotElapsed >= options.screenshotInterval) {
                    takeScreenshot(elapsedTime);
                    lastScreenshotTime = currentTime;
                }
            }
            
            // Update stats
            updateStats(deltaTime);
            
            // Frame rate limiting (optional)
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void cleanup() {
        if (!options.quiet) {
            std::cout << "Cleaning up...\n";
        }
        
        renderer->cleanup();
    }
    
    void handleInput(float deltaTime) {
        // This is a placeholder - in a real implementation,
        // we would handle GLFW input callbacks here
        // For now, just rotate the camera slowly
        if (camera->getMode() == core::CameraMode::Orbital) {
            camera->orbit(deltaTime * 0.2f, 0.0f); // Slow rotation
        }
    }
    
    void takeScreenshot(float elapsedTime) {
        // Generate filename: screenshot_Xs_YMy.png
        // where X = elapsed seconds, Y = simulation million years
        float simulationMegaYears = simulationTime / 31536000.0f / 1000000.0f;
        std::string filename = utils::Screenshot::generateFilename(elapsedTime, simulationMegaYears);
        
        renderer->captureScreenshot(filename);
        
        if (!options.quiet) {
            std::cout << "Screenshot saved: screenshot_dev/" << filename << "\n";
        }
    }
    
    void updateStats(float deltaTime) {
        frameCount++;
        fpsUpdateTimer += deltaTime;
        
        // Update FPS every second
        if (fpsUpdateTimer >= 1.0f) {
            currentFPS = frameCount / fpsUpdateTimer;
            frameCount = 0;
            fpsUpdateTimer = 0.0f;
            
            if (!options.quiet) {
                // Print stats to console
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << currentFPS
                          << " | Nodes: " << renderer->getNodeCount()
                          << " | Frame time: " << std::fixed << std::setprecision(2) 
                          << renderer->getFrameTime() * 1000.0f << " ms"
                          << " | Sim time: " << std::fixed << std::setprecision(2)
                          << simulationTime / 31536000.0f << " years"
                          << "     " << std::flush;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    Options options = parseArguments(argc, argv);
    Application app(options);
    return app.run();
}