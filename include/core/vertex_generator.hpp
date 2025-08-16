#pragma once

#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>
#include "vertex_id_system.hpp"

// ============================================================================
// Phase 2: Centralized Vertex Generation System
// 
// This system generates and caches all vertices for the planet, ensuring
// that vertices at the same 3D position are only computed once and shared
// across all patches that reference them.
// 
// Design principles (from TODO.md):
// - Start with simplest correct implementation (std::unordered_map)
// - Clear interfaces to allow implementation evolution
// - Measure then optimize
// ============================================================================

namespace PlanetRenderer {

// ============================================================================
// Vertex data stored in the cache
// ============================================================================
struct CachedVertex {
    glm::vec3 position;       // Position on sphere surface
    glm::vec3 normal;         // Surface normal
    glm::vec2 texCoord;       // Texture coordinates
    uint32_t materialID;      // Material at this vertex
    
    CachedVertex() = default;
    CachedVertex(const glm::vec3& pos, const glm::vec3& norm, 
                 const glm::vec2& uv = glm::vec2(0.0f), 
                 uint32_t mat = 0)
        : position(pos), normal(norm), texCoord(uv), materialID(mat) {}
};

// ============================================================================
// Interface for vertex generation (allows future optimization)
// ============================================================================
class IVertexGenerator {
public:
    virtual ~IVertexGenerator() = default;
    
    // Get or generate a vertex
    virtual CachedVertex getVertex(const VertexID& id) = 0;
    
    // Batch generation for efficiency (can be optimized later)
    virtual void generateBatch(const std::vector<VertexID>& ids, 
                               std::vector<CachedVertex>& vertices) = 0;
    
    // Cache management
    virtual size_t getCacheSize() const = 0;
    virtual void clearCache() = 0;
    virtual float getCacheHitRate() const = 0;
};

// ============================================================================
// Simple vertex generator with basic caching (Phase 2 initial implementation)
// ============================================================================
class SimpleVertexGenerator : public IVertexGenerator {
public:
    SimpleVertexGenerator(double planetRadius = 6371000.0);
    ~SimpleVertexGenerator() = default;
    
    // IVertexGenerator interface
    CachedVertex getVertex(const VertexID& id) override;
    void generateBatch(const std::vector<VertexID>& ids, 
                       std::vector<CachedVertex>& vertices) override;
    size_t getCacheSize() const override { return cache_.size(); }
    void clearCache() override;
    float getCacheHitRate() const override;
    
    // Configuration
    void setPlanetRadius(double radius) { planetRadius_ = radius; }
    double getPlanetRadius() const { return planetRadius_; }
    
    // Statistics (for debugging/profiling)
    struct Stats {
        uint64_t totalRequests = 0;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        uint64_t batchRequests = 0;
    };
    const Stats& getStats() const { return stats_; }
    void resetStats() { stats_ = Stats(); }
    
private:
    // Generate a vertex from its ID (called on cache miss)
    CachedVertex generateVertex(const VertexID& id);
    
    // Cube to sphere mapping (matches our existing algorithm)
    glm::vec3 cubeToSphere(const glm::vec3& cubePos) const;
    
    // Simple cache - just an unordered_map for now
    // Can be replaced with spatial hash, LRU, etc. based on profiling
    std::unordered_map<VertexID, CachedVertex> cache_;
    
    // Configuration
    double planetRadius_;
    
    // Statistics
    mutable Stats stats_;
};

// ============================================================================
// Vertex buffer manager - manages the global vertex buffer
// ============================================================================
class VertexBufferManager {
public:
    VertexBufferManager();
    ~VertexBufferManager() = default;
    
    // Add vertices to the buffer, returns starting index
    uint32_t addVertices(const std::vector<CachedVertex>& vertices);
    
    // Get vertex by global index
    const CachedVertex& getVertex(uint32_t index) const;
    
    // Get raw buffer for GPU upload
    const std::vector<CachedVertex>& getBuffer() const { return buffer_; }
    std::vector<CachedVertex>& getBuffer() { return buffer_; }
    
    // Buffer management
    void clear() { buffer_.clear(); indexMap_.clear(); }
    size_t size() const { return buffer_.size(); }
    
    // Get or create index for a vertex ID
    uint32_t getOrCreateIndex(const VertexID& id, IVertexGenerator& generator);
    
private:
    std::vector<CachedVertex> buffer_;
    std::unordered_map<VertexID, uint32_t> indexMap_;
};

// ============================================================================
// Global vertex generator instance (singleton for now, can evolve)
// ============================================================================
class VertexGeneratorSystem {
public:
    static VertexGeneratorSystem& getInstance();
    
    // Access to generator and buffer
    IVertexGenerator& getGenerator() { return *generator_; }
    VertexBufferManager& getBufferManager() { return bufferManager_; }
    
    // Configuration
    void setPlanetRadius(double radius);
    
    // Reset system (clear all caches)
    void reset();
    
private:
    VertexGeneratorSystem();
    ~VertexGeneratorSystem() = default;
    
    // Delete copy/move constructors
    VertexGeneratorSystem(const VertexGeneratorSystem&) = delete;
    VertexGeneratorSystem& operator=(const VertexGeneratorSystem&) = delete;
    
    std::unique_ptr<IVertexGenerator> generator_;
    VertexBufferManager bufferManager_;
};

} // namespace PlanetRenderer