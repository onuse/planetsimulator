#include "rendering/transvoxel_renderer.hpp"
#include "algorithms/mesh_generation.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>

namespace rendering {

// Transvoxel Regular Cell lookup tables
// These tables define the 256 possible cube configurations for Transvoxel mesh generation
// Based on the original Transvoxel algorithm by Eric Lengyel

// Regular cell class table - maps each of the 256 cube configurations to a class
const uint8_t TransvoxelRenderer::regularCellClass[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x02, 0x01, 0x03, 0x02, 0x05, 0x01, 0x02, 0x03, 0x05, 0x02, 0x05, 0x05, 0x08,
    0x00, 0x01, 0x01, 0x02, 0x01, 0x03, 0x02, 0x05, 0x01, 0x02, 0x03, 0x05, 0x02, 0x05, 0x05, 0x08,
    0x01, 0x04, 0x02, 0x06, 0x03, 0x07, 0x05, 0x09, 0x02, 0x06, 0x05, 0x09, 0x05, 0x09, 0x08, 0x0C,
    0x00, 0x01, 0x01, 0x02, 0x01, 0x03, 0x02, 0x05, 0x01, 0x02, 0x03, 0x05, 0x02, 0x05, 0x05, 0x08,
    0x01, 0x04, 0x02, 0x06, 0x03, 0x07, 0x05, 0x09, 0x02, 0x06, 0x05, 0x09, 0x05, 0x09, 0x08, 0x0C,
    0x01, 0x02, 0x03, 0x05, 0x04, 0x06, 0x06, 0x09, 0x02, 0x05, 0x07, 0x09, 0x06, 0x09, 0x09, 0x0C,
    0x02, 0x06, 0x05, 0x09, 0x06, 0x09, 0x09, 0x0C, 0x05, 0x09, 0x09, 0x0C, 0x09, 0x0C, 0x0C, 0x0F,
    0x00, 0x01, 0x01, 0x02, 0x01, 0x03, 0x02, 0x05, 0x01, 0x02, 0x03, 0x05, 0x02, 0x05, 0x05, 0x08,
    0x01, 0x04, 0x02, 0x06, 0x03, 0x07, 0x05, 0x09, 0x02, 0x06, 0x05, 0x09, 0x05, 0x09, 0x08, 0x0C,
    0x01, 0x02, 0x03, 0x05, 0x04, 0x06, 0x06, 0x09, 0x02, 0x05, 0x07, 0x09, 0x06, 0x09, 0x09, 0x0C,
    0x02, 0x06, 0x05, 0x09, 0x06, 0x09, 0x09, 0x0C, 0x05, 0x09, 0x09, 0x0C, 0x09, 0x0C, 0x0C, 0x0F,
    0x01, 0x02, 0x03, 0x05, 0x04, 0x06, 0x06, 0x09, 0x02, 0x05, 0x07, 0x09, 0x06, 0x09, 0x09, 0x0C,
    0x02, 0x06, 0x05, 0x09, 0x06, 0x09, 0x09, 0x0C, 0x05, 0x09, 0x09, 0x0C, 0x09, 0x0C, 0x0C, 0x0F,
    0x02, 0x05, 0x07, 0x09, 0x06, 0x09, 0x09, 0x0C, 0x05, 0x09, 0x09, 0x0C, 0x09, 0x0C, 0x0C, 0x0F,
    0x05, 0x09, 0x09, 0x0C, 0x09, 0x0C, 0x0C, 0x0F, 0x08, 0x0C, 0x0C, 0x0F, 0x0C, 0x0F, 0x0F, 0x0F
};

// Regular cell data - defines triangle configurations for each cell class
// Each row contains up to 12 values (4 triangles * 3 vertices each)
// 0xFF terminates the list for configurations with fewer than 4 triangles
const uint8_t TransvoxelRenderer::regularCellData[16][12] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 0: no triangles
    {0x00, 0x08, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 1: 1 triangle
    {0x00, 0x01, 0x08, 0x01, 0x03, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 2: 2 triangles
    {0x01, 0x08, 0x03, 0x09, 0x08, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 3: 2 triangles
    {0x01, 0x02, 0x0A, 0x08, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 4: 2 triangles
    {0x00, 0x08, 0x03, 0x01, 0x02, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 5: 2 triangles
    {0x02, 0x03, 0x08, 0x02, 0x08, 0x0A, 0x0A, 0x08, 0x01, 0xFF, 0xFF, 0xFF}, // Class 6: 3 triangles
    {0x02, 0x0A, 0x01, 0x02, 0x01, 0x03, 0x03, 0x01, 0x08, 0xFF, 0xFF, 0xFF}, // Class 7: 3 triangles
    {0x03, 0x0B, 0x02, 0x00, 0x08, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 8: 2 triangles
    {0x08, 0x01, 0x00, 0x02, 0x03, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 9: 2 triangles
    {0x01, 0x02, 0x0A, 0x03, 0x0B, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 10: 2 triangles
    {0x02, 0x0A, 0x01, 0x0B, 0x08, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Class 11: 2 triangles
    {0x0A, 0x01, 0x08, 0x0A, 0x08, 0x02, 0x02, 0x08, 0x0B, 0x02, 0x0B, 0x03}, // Class 12: 4 triangles
    {0x08, 0x01, 0x0A, 0x08, 0x0A, 0x02, 0x08, 0x02, 0x03, 0x03, 0x02, 0x0B}, // Class 13: 4 triangles
    {0x01, 0x08, 0x0A, 0x08, 0x02, 0x0A, 0x08, 0x03, 0x02, 0x03, 0x0B, 0x02}, // Class 14: 4 triangles
    {0x0A, 0x01, 0x08, 0x02, 0x0A, 0x08, 0x03, 0x02, 0x08, 0x0B, 0x03, 0x08}  // Class 15: 4 triangles
};

// Note: regularVertexData table removed - edge detection is done algorithmically
// in generateRegularCell() based on the cube configuration

TransvoxelRenderer::TransvoxelRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue)
    : device(device), physicalDevice(physicalDevice), commandPool(commandPool), graphicsQueue(graphicsQueue) {
}

TransvoxelRenderer::~TransvoxelRenderer() {
    // The TransvoxelRenderer doesn't own the chunks directly - they are managed by VulkanRenderer
    // Individual chunk buffer cleanup is handled by destroyChunkBuffers() method
}

void TransvoxelRenderer::generateMesh(TransvoxelChunk& chunk, const octree::OctreePlanet& planet) {
    // Validate planet has been initialized
    if (!planet.getRoot()) {
        std::cerr << "ERROR: TransvoxelRenderer::generateMesh called with uninitialized planet. "
                  << "Ensure planet.generate() has been called!\n";
        chunk.hasValidMesh = false;
        return;
    }
    
    // Clean up existing GPU buffers before regenerating
    destroyChunkBuffers(chunk);
    
    // Clear existing mesh data
    chunk.vertices.clear();
    chunk.indices.clear();
    chunk.vertexColors.clear();
    
    // Use the algorithm to generate mesh
    const int chunkSize = 32;
    algorithms::MeshGenParams params(
        chunk.position - glm::vec3(chunkSize/2.0f * chunk.voxelSize),
        chunk.voxelSize,
        glm::ivec3(chunkSize, chunkSize, chunkSize),
        chunk.lodLevel
    );
    
    algorithms::MeshData meshData = algorithms::generateTransvoxelMesh(params, planet);
    
    // Convert algorithm mesh data to chunk format
    for (const auto& vertex : meshData.vertices) {
        Vertex v;
        v.position = vertex.position;
        v.normal = vertex.normal;
        v.color = vertex.color;
        v.texCoord = glm::vec2(0.0f); // Default texture coords
        chunk.vertices.push_back(v);
    }
    chunk.indices = meshData.indices;
    
    // Create Vulkan buffers for the mesh
    if (!chunk.vertices.empty()) {
        try {
            createVertexBuffer(chunk);
            createIndexBuffer(chunk);
            
            // Only set hasValidMesh if both buffers were successfully created
            if (chunk.vertexBuffer != VK_NULL_HANDLE && chunk.indexBuffer != VK_NULL_HANDLE) {
                chunk.hasValidMesh = true;
            } else {
                std::cout << "Warning: GPU buffers creation failed - buffers are VK_NULL_HANDLE\n";
                chunk.hasValidMesh = false;
            }
        } catch (const std::exception& e) {
            std::cout << "Error creating GPU buffers: " << e.what() << "\n";
            chunk.hasValidMesh = false;
        }
        
        // Only update statistics if mesh creation was successful
        if (chunk.hasValidMesh) {
            totalTriangles.fetch_add(static_cast<uint32_t>(chunk.indices.size() / 3));
            activeChunks.fetch_add(1);
            
            // Debug output removed to reduce console spam
            // std::cout << "Successfully created Transvoxel mesh with " << chunk.vertices.size() 
            //           << " vertices and " << chunk.indices.size()/3 << " triangles\n";
        }
    } else {
        // Debug output removed to reduce console spam
        // std::cout << "Transvoxel produced no geometry for this chunk - this is expected for chunks with uniform density\n";
        chunk.hasValidMesh = false;
    }
    
    chunk.isDirty = false;
}

void TransvoxelRenderer::dumpMeshDataToFile(const std::vector<TransvoxelChunk>& chunks) {
    std::cout << "Dumping mesh data to debug files...\n";
    
    // Dump comprehensive mesh data
    std::ofstream meshDump("mesh_debug_dump.txt");
    std::ofstream objFile("mesh_debug.obj");  // OBJ format for visualization
    
    if (!meshDump.is_open() || !objFile.is_open()) {
        std::cerr << "Failed to open debug files!\n";
        return;
    }
    
    meshDump << "=== TRANSVOXEL MESH DEBUG DUMP ===\n";
    meshDump << "Total chunks: " << chunks.size() << "\n\n";
    
    int validChunks = 0;
    int totalVerts = 0;  // Renamed to avoid shadowing
    int totalTris = 0;    // Renamed to avoid shadowing
    int vertexOffset = 1; // OBJ format uses 1-based indexing
    
    // Analyze each chunk
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        
        if (!chunk.hasValidMesh || chunk.vertices.empty()) {
            meshDump << "Chunk " << i << ": INVALID/EMPTY\n";
            continue;
        }
        
        validChunks++;
        int triangleCount = static_cast<int>(chunk.indices.size() / 3);
        totalVerts += static_cast<int>(chunk.vertices.size());
        totalTris += triangleCount;
        
        meshDump << "Chunk " << i << ":\n";
        meshDump << "  Position: (" << chunk.position.x << ", " << chunk.position.y << ", " << chunk.position.z << ")\n";
        meshDump << "  Voxel Size: " << chunk.voxelSize << "\n";
        meshDump << "  LOD Level: " << chunk.lodLevel << "\n";
        meshDump << "  Vertices: " << chunk.vertices.size() << "\n";
        meshDump << "  Triangles: " << triangleCount << "\n";
        
        // Analyze vertex positions for sanity
        glm::vec3 minPos(1e10f), maxPos(-1e10f);
        float avgDist = 0;
        for (const auto& v : chunk.vertices) {
            minPos = glm::min(minPos, v.position);
            maxPos = glm::max(maxPos, v.position);
            avgDist += glm::length(v.position);
            
            // Write to OBJ file
            objFile << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
        }
        avgDist /= chunk.vertices.size();
        
        meshDump << "  Vertex bounds: min(" << minPos.x << "," << minPos.y << "," << minPos.z 
                 << ") max(" << maxPos.x << "," << maxPos.y << "," << maxPos.z << ")\n";
        meshDump << "  Bounding box size: (" << (maxPos.x - minPos.x) << "," 
                 << (maxPos.y - minPos.y) << "," << (maxPos.z - minPos.z) << ")\n";
        meshDump << "  Average distance from origin: " << avgDist << "\n";
        
        // Write vertex normals to OBJ
        for (const auto& v : chunk.vertices) {
            objFile << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
        }
        
        // Sample first few triangles
        meshDump << "  First 3 triangles (indices):\n";
        for (size_t t = 0; t < std::min(size_t(3), chunk.indices.size()/3); ++t) {
            uint32_t i0 = chunk.indices[t*3];
            uint32_t i1 = chunk.indices[t*3+1];
            uint32_t i2 = chunk.indices[t*3+2];
            
            meshDump << "    Triangle " << t << ": [" << i0 << "," << i1 << "," << i2 << "]\n";
            
            if (i0 < chunk.vertices.size() && i1 < chunk.vertices.size() && i2 < chunk.vertices.size()) {
                glm::vec3 v0 = chunk.vertices[i0].position;
                glm::vec3 v1 = chunk.vertices[i1].position;
                glm::vec3 v2 = chunk.vertices[i2].position;
                
                meshDump << "      v0: (" << v0.x << "," << v0.y << "," << v0.z << ")\n";
                meshDump << "      v1: (" << v1.x << "," << v1.y << "," << v1.z << ")\n";
                meshDump << "      v2: (" << v2.x << "," << v2.y << "," << v2.z << ")\n";
                
                // Calculate triangle size
                float edge1 = glm::length(v1 - v0);
                float edge2 = glm::length(v2 - v1);
                float edge3 = glm::length(v0 - v2);
                meshDump << "      Edge lengths: " << edge1 << ", " << edge2 << ", " << edge3 << "\n";
                
                // Calculate triangle area
                glm::vec3 cross = glm::cross(v1 - v0, v2 - v0);
                float area = glm::length(cross) * 0.5f;
                meshDump << "      Triangle area: " << area << "\n";
            }
            
            // Write to OBJ (adjusting for 1-based indexing)
            objFile << "f " << (i0 + vertexOffset) << "//" << (i0 + vertexOffset) 
                    << " " << (i1 + vertexOffset) << "//" << (i1 + vertexOffset)
                    << " " << (i2 + vertexOffset) << "//" << (i2 + vertexOffset) << "\n";
        }
        
        // Check for degenerate triangles
        int degenerateCount = 0;
        for (size_t t = 0; t < chunk.indices.size()/3; ++t) {
            uint32_t i0 = chunk.indices[t*3];
            uint32_t i1 = chunk.indices[t*3+1];
            uint32_t i2 = chunk.indices[t*3+2];
            
            if (i0 == i1 || i1 == i2 || i0 == i2) {
                degenerateCount++;
            } else if (i0 < chunk.vertices.size() && i1 < chunk.vertices.size() && i2 < chunk.vertices.size()) {
                glm::vec3 v0 = chunk.vertices[i0].position;
                glm::vec3 v1 = chunk.vertices[i1].position;
                glm::vec3 v2 = chunk.vertices[i2].position;
                
                float area = glm::length(glm::cross(v1 - v0, v2 - v0)) * 0.5f;
                if (area < 0.0001f) {
                    degenerateCount++;
                }
            }
        }
        
        if (degenerateCount > 0) {
            meshDump << "  WARNING: " << degenerateCount << " degenerate triangles found!\n";
        }
        
        vertexOffset += static_cast<int>(chunk.vertices.size());
        meshDump << "\n";
    }
    
    meshDump << "\n=== SUMMARY ===\n";
    meshDump << "Valid chunks: " << validChunks << " / " << chunks.size() << "\n";
    meshDump << "Total vertices: " << totalVerts << "\n";
    meshDump << "Total triangles: " << totalTris << "\n";
    
    meshDump.close();
    objFile.close();
    
    std::cout << "Mesh debug data written to mesh_debug_dump.txt and mesh_debug.obj\n";
    std::cout << "Summary: " << validChunks << " valid chunks, " 
              << totalVerts << " vertices, " << totalTris << " triangles\n";
}

void TransvoxelRenderer::render(
    const std::vector<TransvoxelChunk>& chunks,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout /*pipelineLayout*/) {  // Unused parameter
    
    static int renderCallCount = 0;
    bool debugThis = (renderCallCount++ % 60 == 0); // Debug every 60 calls
    
    // Dump mesh data on first render
    static bool firstDump = true;
    if (firstDump && !chunks.empty()) {
        firstDump = false;
        dumpMeshDataToFile(chunks);
    }
    
    if (debugThis) {
        // TransvoxelRenderer::render called with chunks
    }
    
    int validMeshCount = 0;
    int totalTriangleCount = 0;  // Renamed to avoid shadowing
    
    for (const auto& chunk : chunks) {
        if (debugThis) {
            // Debug: Chunk vertices and triangles
        }
        
        if (!chunk.hasValidMesh || chunk.vertices.empty()) continue;
        
        validMeshCount++;
        totalTriangleCount += static_cast<int>(chunk.indices.size() / 3);
        
        // Bind vertex and index buffers
        VkBuffer vertexBuffers[] = {chunk.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, chunk.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        
        // Draw the mesh
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(chunk.indices.size()), 1, 0, 0, 0);
        
        if (debugThis) {
            // Debug: Rendered chunk
        }
    }
    
    if (debugThis) {
        // std::cout << "[TRANSVOXEL RENDER] Rendered " << validMeshCount 
        //           << " chunks with " << totalTriangleCount << " triangles" << std::endl;
    }
}

void TransvoxelRenderer::invalidateChunk(const glm::vec3& /*position*/) {  // Unused parameter
    // TODO: Mark chunks at the given position as dirty for regeneration
    // This will be called when the planet data changes
}

void TransvoxelRenderer::clearCache() {
    // Clear all cached mesh data (thread-safe)
    totalTriangles.store(0);
    activeChunks.store(0);
}

void TransvoxelRenderer::createVertexBuffer(TransvoxelChunk& chunk) {
    if (chunk.vertices.empty()) {
        // Debug output removed to reduce console spam
        // std::cout << "createVertexBuffer: No vertices to upload\n";
        return;
    }
    
    // Debug output removed to reduce console spam
    // std::cout << "createVertexBuffer: Creating buffer for " << chunk.vertices.size() << " vertices\n";
    VkDeviceSize bufferSize = sizeof(Vertex) * chunk.vertices.size();
    
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    // Copy vertex data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, chunk.vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Create vertex buffer
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, chunk.vertexBuffer, chunk.vertexBufferMemory);
    
    // Copy from staging to vertex buffer
    copyBuffer(stagingBuffer, chunk.vertexBuffer, bufferSize);
    
    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    // Debug output removed to reduce console spam
    // std::cout << "createVertexBuffer: Successfully created vertex buffer\n";
}

void TransvoxelRenderer::createIndexBuffer(TransvoxelChunk& chunk) {
    if (chunk.indices.empty()) {
        // Debug output removed to reduce console spam
        // std::cout << "createIndexBuffer: No indices to upload\n";
        return;
    }
    
    // Debug output removed to reduce console spam
    // std::cout << "createIndexBuffer: Creating buffer for " << chunk.indices.size() << " indices\n";
    VkDeviceSize bufferSize = sizeof(uint32_t) * chunk.indices.size();
    
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    // Copy index data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, chunk.indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Create index buffer
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, chunk.indexBuffer, chunk.indexBufferMemory);
    
    // Copy from staging to index buffer
    copyBuffer(stagingBuffer, chunk.indexBuffer, bufferSize);
    
    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    // Debug output removed to reduce console spam
    // std::cout << "createIndexBuffer: Successfully created index buffer\n";
}

void TransvoxelRenderer::destroyChunkBuffers(TransvoxelChunk& chunk) {
    // Update statistics if chunk had valid mesh
    if (chunk.hasValidMesh && !chunk.indices.empty()) {
        totalTriangles.fetch_sub(static_cast<uint32_t>(chunk.indices.size() / 3));
        activeChunks.fetch_sub(1);
    }
    
    // Don't wait here - let the caller handle synchronization if needed
    // The updateChunks function should wait before destroying all chunks
    
    if (chunk.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, chunk.vertexBuffer, nullptr);
        chunk.vertexBuffer = VK_NULL_HANDLE;
    }
    if (chunk.vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, chunk.vertexBufferMemory, nullptr);
        chunk.vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (chunk.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, chunk.indexBuffer, nullptr);
        chunk.indexBuffer = VK_NULL_HANDLE;
    }
    if (chunk.indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, chunk.indexBufferMemory, nullptr);
        chunk.indexBufferMemory = VK_NULL_HANDLE;
    }
    // Reset mesh validity flag since buffers are destroyed
    chunk.hasValidMesh = false;
}

void TransvoxelRenderer::createBuffer(
    VkDeviceSize size, 
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, 
    VkBuffer& buffer, 
    VkDeviceMemory& bufferMemory) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }
    
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void TransvoxelRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t TransvoxelRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace rendering