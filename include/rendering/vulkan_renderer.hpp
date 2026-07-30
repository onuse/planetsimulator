#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <array>
#include <optional>
#include <chrono>

#include <unordered_map>
#include <unordered_set>

#include "core/octree.hpp"
#include "core/camera.hpp"
#include "rendering/imgui_manager.hpp"
#include "rendering/patch_builder.hpp"
#include "rendering/patch_tree.hpp"
// REMOVED: CPU-based renderers - using GPU mesh generation only

namespace rendering {

// Input state structure
struct InputState {
    bool keys[512] = {false};
    bool prevKeys[512] = {false};
    bool mouseButtons[8] = {false};
    bool prevMouseButtons[8] = {false};
    glm::vec2 mousePos = {0, 0};
    glm::vec2 lastMousePos = {0, 0};
    glm::vec2 mouseDelta = {0, 0};
    glm::vec2 scrollDelta = {0, 0};
    bool firstMouse = true;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    
    bool isComplete() const {
        return graphicsFamily.has_value() && 
               presentFamily.has_value() && 
               computeFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;      // Single precision view matrix
    alignas(16) glm::mat4 proj;      // Single precision projection matrix
    alignas(16) glm::mat4 viewProj;  // Single precision combined matrix
    alignas(16) glm::vec3 viewPos;   // Single precision view position (camera-relative origin)
    float time;
    alignas(16) glm::vec3 lightDir;

    // World size of one pixel at one metre from the eye, so a fragment can
    // work out how much ground it covers and stop adding detail finer than
    // that. Was an unused padding float.
    float pixelWorldScale;

    // The planet's own scale, so shading can be written in terms of the world
    // rather than in tuned constants. Height above sea level decides water
    // from land and how much air a ray has come through; without the radius
    // here, a fragment only knows an absolute position of order 10^6 and
    // cannot recover either.
    //   x: planet radius        z: how visible the weather should be, 0 to 1
    //   y: sea level            w: atmosphere scale height
    alignas(16) glm::vec4 planetParams;
};

// Instance data must match shader expectations exactly
struct InstanceData {
    glm::vec3 center;        // offset 0, size 12
    float halfSize;          // offset 12, size 4
    glm::vec4 colorAndMaterial; // offset 16, size 16 - xyz=color, w=material
    // Total size: 32 bytes
};

class VulkanRenderer {
public:
    VulkanRenderer(uint32_t width, uint32_t height);
    ~VulkanRenderer();
    
    // Main interface
    bool initialize();
    void render(octree::OctreePlanet* planet, core::Camera* camera);
    void cleanup();
    
    // Window management
    void resize(uint32_t width, uint32_t height);
    bool shouldClose() const;
    void pollEvents();
    
    // Input handling
    const InputState& getInputState() const { return inputState; }
    void updateInput();
    GLFWwindow* getWindow() const { return window; }
    
    // Settings
    // What the surface is coloured by. Terrain is what the planet looks like;
    // the rest show what the simulation is doing, which is the only way to
    // tell a docking terrane from a rendering artefact.
    enum class SurfaceView {
        Terrain = 0,   // altitude and biome, the normal view
        Plates,        // one colour per plate, so boundaries are visible
        CrustAge,      // young at ridges, old at trenches
        RockType,      // basalt, granite, andesite, sediment
        Thickness,     // thin ocean floor against thick orogens
        Count
    };
    void setSurfaceView(SurfaceView view) { surfaceView = view; }

    // Rebuild the mesh on the next frame regardless of the usual throttle, so
    // switching view is immediate rather than waiting a second.
    void forceMeshRebuild() { meshRebuildRequested = true; }
    SurfaceView getSurfaceView() const { return surfaceView; }
    static const char* surfaceViewName(SurfaceView view) {
        switch (view) {
            case SurfaceView::Terrain:   return "Terrain";
            case SurfaceView::Plates:    return "Plates";
            case SurfaceView::CrustAge:  return "Crust age";
            case SurfaceView::RockType:  return "Rock type";
            case SurfaceView::Thickness: return "Crust thickness";
            default:                     return "?";
        }
    }

    void setRenderMode(int mode) { renderMode = mode; }
    void setWireframe(bool enabled) { wireframeEnabled = enabled; }
    void setVSync(bool enabled);
    // Hierarchical GPU octree is always enabled - no option to disable
    
    // Removed parallel paths - only transvoxel rendering now
    
    // Screenshot support
    bool captureScreenshot(const std::string& filename);
    
    // Debug support
    void dumpVertexData();  // Dump vertex data for debugging
    
    // Stats
    float getFrameTime() const { return frameTime; }
    uint32_t getNodeCount() const { return visibleNodeCount; }
    uint32_t getChunkCount() const { return 0; } // GPU mesh only
    uint32_t getTriangleCount() const { 
        return meshIndexCount / 3; // From GPU mesh
    }

    // What the surface renderer is actually doing this frame.
    //
    // The panel used to report a global level of detail and the triangle count
    // a table said that level implied. There has been no global level since
    // the surface became a quadtree - each patch decides for itself - so the
    // number was of something that no longer exists, and the triangle count
    // was a prediction rather than a measurement. Reporting a plausible
    // invention is worse than reporting nothing: it is what you check a change
    // against.
    struct PatchStats {
        uint32_t drawn = 0;          // patches actually submitted
        uint32_t selected = 0;       // patches selection asked for
        uint32_t culledHorizon = 0;  // behind the planet's own curve
        uint32_t culledFrustum = 0;  // outside the view
        uint32_t cached = 0;
        uint32_t inFlight = 0;       // being built right now
        uint32_t poolSlots = 0;
        uint32_t workers = 0;
        uint32_t triangles = 0;
        uint32_t finestLevel = 0;    // deepest subdivision on screen
        float metresPerVertex = 0.0f;
    };
    PatchStats getPatchStats() const { return patchStats; }

    // The planet being drawn, so the debug panel can drive the simulation.
    // Not const: changing how fast geological time runs is the one control
    // that makes it possible to look at anything closely - at a million years
    // a second the continents move visibly while you are trying to focus on a
    // hillside.
    octree::OctreePlanet* getPlanet() const { return currentPlanet; }
    float getPlanetRadius() const { return patchCullPlanetRadius; }
    
    
    // MASTER PIPELINE SWITCH - THE ONE BOOL TO RULE THEM ALL
private:
    // Window
    GLFWwindow* window = nullptr;
    uint32_t windowWidth;
    uint32_t windowHeight;
    bool framebufferResized = false;
    
    // Core Vulkan objects
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkSurfaceKHR surface;
    
    // Swap chain
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    // Render pass
    VkRenderPass renderPass;
    
    // Depth buffer
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    
    // Pipeline
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkPipeline wireframePipeline;
    
    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    
    // Buffers
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceBufferMemory;
    void* instanceBufferMapped;
    
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
    
    // GPU octree data
    VkBuffer octreeNodeBuffer;
    VkDeviceMemory octreeNodeBufferMemory;
    VkBuffer voxelDataBuffer;
    VkDeviceMemory voxelDataBufferMemory;
    
    // Material table buffer
    VkBuffer materialTableBuffer;
    VkDeviceMemory materialTableBufferMemory;
    
    // Command buffers
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandBuffer currentCommandBuffer = VK_NULL_HANDLE;  // Current command buffer being recorded
    
    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    uint32_t lastRenderedImageIndex = 0; // Track which swap chain image was last rendered
    
    // Rendering state
    int renderMode = 0;
    SurfaceView surfaceView = SurfaceView::Terrain;
    bool meshRebuildRequested = false;

    // ------------------------------------------------------------------
    // Surface patches
    // ------------------------------------------------------------------
    //
    // One quadtree over the planet, and a cache of the patches that have been
    // built. Patches persist across frames: moving the camera changes which
    // are drawn, not what has to be rebuilt.
    // Every patch has the same vertex and index count, because every patch is
    // the same fixed grid. That makes them interchangeable slots in one big
    // buffer rather than a buffer each.
    //
    // This is not a micro-optimisation. A buffer pair per patch meant nearly
    // four thousand device memory allocations at a couple of thousand patches,
    // which is up against the limit most drivers expose, and forty more
    // allocations every frame as patches were built. Pooling also drops the
    // per-patch buffer binds: the pool is bound once for the whole frame and
    // each patch is a draw with an offset into it.
    static constexpr uint32_t PATCH_VERTEX_COUNT =
        PatchTree::VERTS * PatchTree::VERTS + 4 * PatchTree::VERTS;
    static constexpr uint32_t PATCH_INDEX_COUNT =
        PatchTree::GRID * PatchTree::GRID * 6 + 4 * PatchTree::GRID * 12;
    // Sized for the whole horizon disc at low altitude, not for what is on
    // screen. Only frustum culling knows about where the camera points, and
    // that is applied at draw time rather than at selection time - so turning
    // on the spot never has to rebuild anything. About 220 MB, which buys the
    // camera the freedom to spin without the surface going coarse.
    static constexpr uint32_t MAX_PATCHES = 3072;

    struct GpuPatch {
        uint32_t slot = UINT32_MAX;
        uint32_t indexCount = 0;
        glm::dvec3 centre{0.0};
        float boundingRadius = 0.0f;
        uint64_t lastUsedFrame = 0;

        // What the surface looked like when this was built. When the
        // simulation moves on, the patch is stale but still perfectly drawable
        // - so it keeps being drawn until its replacement is ready.
        uint64_t builtAtCrustVersion = 0;
        uint32_t builtAtStyle = 0;
    };

    // What the vertex shader needs to place a patch: its centre relative to
    // the camera, worked out in double on this side.
    struct PatchPushConstants {
        glm::vec3 patchOffset;
        float padding = 0.0f;
    };

    PatchTree patchTree;
    std::unordered_map<uint64_t, GpuPatch> patchCache;
    std::vector<PatchTree::PatchKey> visiblePatches;
    uint64_t patchFrameCounter = 0;
    uint32_t patchStyleVersion = 0;

    VkBuffer patchVertexPool = VK_NULL_HANDLE;
    VkDeviceMemory patchVertexPoolMemory = VK_NULL_HANDLE;
    void* patchVertexPoolMapped = nullptr;
    VkBuffer patchIndexPool = VK_NULL_HANDLE;
    VkDeviceMemory patchIndexPoolMemory = VK_NULL_HANDLE;
    void* patchIndexPoolMapped = nullptr;
    std::vector<uint32_t> freePatchSlots;
    std::vector<PatchTree::PatchKey> wantedPatches;
    std::vector<PatchTree::PatchKey> staleVisible;

    // Geometry is built on worker threads. The render thread only decides
    // what to ask for, hands out slots, and files what comes back.
    PatchBuilder patchBuilder;
    std::vector<PatchBuilder::Result> builtPatches;
    std::unordered_set<uint64_t> inFlightPatches;
    uint64_t sourceCrustVersion = UINT64_MAX;

    // A slot cannot be handed to a different patch the moment its old owner is
    // dropped: a command buffer still in flight may be drawing that slot, and
    // it was recorded with the old patch's position. Overwriting the geometry
    // underneath it draws one patch's terrain at another patch's location.
    struct PendingSlot {
        uint32_t slot = 0;
        uint64_t freedOnFrame = 0;
    };
    std::vector<PendingSlot> pendingPatchSlots;

    // The camera-relative view-projection the uniform buffer was last written
    // with. Culling has to test against exactly the matrix the vertex shader
    // will use, or patches get rejected while still on screen.
    glm::mat4 patchCullMatrix{1.0f};
    float patchCullPlanetRadius = 0.0f;
    PatchStats patchStats;

    static uint64_t packPatchKey(const PatchTree::PatchKey& key);
    void createPatchPools();
    uint32_t acquirePatchSlot();
    void releasePatchSlot(uint32_t slot);
    void reclaimPendingSlots();
    void updatePatches(octree::OctreePlanet* planet, core::Camera* camera);
    void evictUnusedPatches();
    void renderPatches(const glm::dvec3& cameraPosition);

    // The cloud layer, drawn from the same patches after the ground.
    void renderClouds(const glm::dvec3& cameraPosition);
    void destroyAllPatches();
    bool wireframeEnabled = false;
    // Removed parallel rendering paths
    uint32_t visibleNodeCount = 0;
    std::vector<InstanceData> instances;
    
    // Performance tracking
    std::chrono::steady_clock::time_point lastFrameTime;
    float frameTime = 0.0f;
    
    // Input state
    InputState inputState;
    
    // ImGui manager
    ImGuiManager imguiManager;
    
    // Current camera (for debug display)
    core::Camera* currentCamera = nullptr;
    octree::OctreePlanet* currentPlanet = nullptr;
    
    // REMOVED: CPU-based renderers - using GPU mesh generation only
    
    // GPU octree for GPU-only pipeline
    
    // REMOVED: GPU point cloud rendering - using only triangle mesh rendering
    
    // GPU mesh generation
    VkBuffer meshVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory meshVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer meshIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory meshIndexBufferMemory = VK_NULL_HANDLE;
    uint32_t meshVertexCount = 0;
    uint32_t meshIndexCount = 0;
    
    // GPU mesh generation
    
    // Sphere mesh generation with proper cube-to-sphere mapping
    bool generateSphereMesh(octree::OctreePlanet* planet);
    bool generateSphereMeshHighRes(octree::OctreePlanet* planet);  // High resolution version
    bool generateSeamlessSphere(octree::OctreePlanet* planet);  // Seamless version with vertex deduplication
    bool generateAdaptiveSphere(octree::OctreePlanet* planet, core::Camera* camera);  // Dual-detail adaptive sphere generation (Phase 1)
    bool generateGPUAdaptiveSphere(octree::OctreePlanet* planet, core::Camera* camera);  // GPU compute version
    bool generateGPUAdaptiveSphereWithOctree(octree::OctreePlanet* planet, core::Camera* camera);  // GPU with octree traversal
    bool generateGPUTransvoxelMesh(octree::OctreePlanet* planet, core::Camera* camera);  // GPU Transvoxel implementation
    bool createTransvoxelComputePipeline();  // Create Transvoxel compute pipeline
    void cleanupTransvoxelPipeline();  // Cleanup Transvoxel pipeline
    // Upload CPU-generated mesh to GPU (used by adaptive sphere generation)
    bool uploadCPUReferenceMesh(const void* vertexData, size_t vertexDataSize,
                                const void* indexData, size_t indexDataSize,
                                uint32_t vertexCount, uint32_t indexCount);
    
    // GPU compute pipeline functions for adaptive sphere
    bool createAdaptiveSphereComputePipeline();
    bool allocateGPUMeshBuffers(size_t maxVertices, size_t maxIndices);
    bool dispatchAdaptiveSphereCompute(const glm::vec3& cameraPos, float planetRadius, 
                                       int highDetailLevel, int lowDetailLevel, bool flipFrontBack);

    
#ifdef DEBUG_CPU_REFERENCE
    // TEMPORARY: CPU reference implementation for debugging GPU mesh generation
    // This code should be REMOVED once GPU mesh generation is verified working
    bool generateCPUReferenceMesh(octree::OctreePlanet* planet);
    void collectSurfaceLeaves(octree::OctreeNode* node, std::vector<octree::OctreeNode*>& leaves);
    
#endif // DEBUG_CPU_REFERENCE
    
    // Hierarchical pipeline (single rendering path)
    VkPipeline hierarchicalPipeline = VK_NULL_HANDLE;
    VkPipelineLayout hierarchicalPipelineLayout = VK_NULL_HANDLE;
    
    // Triangle mesh pipeline for Transvoxel rendering
    VkPipeline trianglePipeline = VK_NULL_HANDLE;

    // Same geometry, same layout - it differs only in lifting each vertex to
    // the cloud shell, blending instead of overwriting, and not writing depth.
    VkPipeline cloudPipeline = VK_NULL_HANDLE;
    // REMOVED: Test NDC pipeline - using only triangle mesh rendering
    VkDescriptorSetLayout hierarchicalDescriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> hierarchicalDescriptorSets;
    
    
    // Initialization functions
    void createWindow();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createDepthResources();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    
    // Helper functions
    void cleanupSwapChain();
    void recreateSwapChain();
    void updateUniformBuffer(uint32_t currentImage, core::Camera* camera);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame(octree::OctreePlanet* planet, core::Camera* camera);
    void createCubeGeometry();
    void updateInstanceBuffer(const octree::OctreePlanet::RenderData& renderData);
    void createMaterialTableBuffer();
    void updateMaterialTableBuffer();
    
    // Device selection helpers
    bool isDeviceSuitable(VkPhysicalDevice physDevice);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physDevice);
    bool checkDeviceExtensionSupport(VkPhysicalDevice physDevice);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physDevice);
    
    // Swap chain helpers
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
    // Buffer helpers
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Image helpers
    void createImage(uint32_t width, uint32_t height, VkFormat format, 
                    VkImageTiling tiling, VkImageUsageFlags usage, 
                    VkMemoryPropertyFlags properties, VkImage& image, 
                    VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, 
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    
    // Shader loading
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
    
    // Transvoxel rendering
    void createTransvoxelPipeline();
    void createTransvoxelDescriptorSets();
    void createTrianglePipeline();
    
    void updateChunks(octree::OctreePlanet* planet, core::Camera* camera);
    void generateChunkMeshes(octree::OctreePlanet* planet);
    
    // Removed parallel rendering paths
    
    // Legacy hierarchical functions (still used by Transvoxel for MVP)
    void createHierarchicalPipeline();
    void createHierarchicalDescriptorSets();
    
    // Callbacks
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    
    // Validation layers
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
    
    // Required extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    // Validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif
};

} // namespace rendering