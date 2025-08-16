#include "core/vertex_generator.hpp"
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PlanetRenderer {

// ============================================================================
// SimpleVertexGenerator Implementation
// ============================================================================

SimpleVertexGenerator::SimpleVertexGenerator(double planetRadius)
    : planetRadius_(planetRadius) {
}

CachedVertex SimpleVertexGenerator::getVertex(const VertexID& id) {
    stats_.totalRequests++;
    
    // Check cache first
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        stats_.cacheHits++;
        return it->second;
    }
    
    // Cache miss - generate vertex
    stats_.cacheMisses++;
    CachedVertex vertex = generateVertex(id);
    cache_[id] = vertex;
    return vertex;
}

void SimpleVertexGenerator::generateBatch(const std::vector<VertexID>& ids, 
                                         std::vector<CachedVertex>& vertices) {
    stats_.batchRequests++;
    vertices.clear();
    vertices.reserve(ids.size());
    
    for (const auto& id : ids) {
        vertices.push_back(getVertex(id));
    }
}

void SimpleVertexGenerator::clearCache() {
    cache_.clear();
    // Keep statistics to analyze performance
}

float SimpleVertexGenerator::getCacheHitRate() const {
    if (stats_.totalRequests == 0) return 0.0f;
    return float(stats_.cacheHits) / float(stats_.totalRequests);
}

CachedVertex SimpleVertexGenerator::generateVertex(const VertexID& id) {
    // Get cube position from vertex ID
    glm::dvec3 cubePosD = id.toCubePosition();
    glm::vec3 cubePos = glm::vec3(cubePosD);  // Convert to float
    
    // Map to sphere
    glm::vec3 spherePos = cubeToSphere(cubePos);
    
    // Normal is just normalized position for a sphere
    glm::vec3 normal = glm::normalize(spherePos);
    
    // Simple UV mapping based on spherical coordinates
    // This can be improved later based on requirements
    float theta = atan2(spherePos.z, spherePos.x);
    float phi = asin(spherePos.y / glm::length(spherePos));
    glm::vec2 texCoord(
        (theta + M_PI) / (2.0f * M_PI),
        (phi + M_PI/2.0f) / M_PI
    );
    
    // Material ID - for now just based on height
    // This will be replaced with proper material sampling
    float height = glm::length(spherePos);
    uint32_t materialID = 0;
    if (height > planetRadius_ * 1.001) {
        materialID = 1; // Mountain
    } else if (height < planetRadius_ * 0.999) {
        materialID = 2; // Ocean
    }
    
    return CachedVertex(spherePos, normal, texCoord, materialID);
}

glm::vec3 SimpleVertexGenerator::cubeToSphere(const glm::vec3& cubePos) const {
    // Use the standard cube-to-sphere mapping that minimizes distortion
    glm::vec3 pos2 = cubePos * cubePos;
    glm::vec3 spherePos;
    
    spherePos.x = cubePos.x * sqrt(1.0f - pos2.y * 0.5f - pos2.z * 0.5f + pos2.y * pos2.z / 3.0f);
    spherePos.y = cubePos.y * sqrt(1.0f - pos2.x * 0.5f - pos2.z * 0.5f + pos2.x * pos2.z / 3.0f);
    spherePos.z = cubePos.z * sqrt(1.0f - pos2.x * 0.5f - pos2.y * 0.5f + pos2.x * pos2.y / 3.0f);
    
    // Scale to planet radius
    return glm::normalize(spherePos) * static_cast<float>(planetRadius_);
}

// ============================================================================
// VertexBufferManager Implementation
// ============================================================================

VertexBufferManager::VertexBufferManager() {
    // Reserve some initial capacity to reduce reallocations
    buffer_.reserve(65536);
}

uint32_t VertexBufferManager::addVertices(const std::vector<CachedVertex>& vertices) {
    uint32_t startIndex = static_cast<uint32_t>(buffer_.size());
    buffer_.insert(buffer_.end(), vertices.begin(), vertices.end());
    return startIndex;
}

const CachedVertex& VertexBufferManager::getVertex(uint32_t index) const {
    if (index >= buffer_.size()) {
        throw std::out_of_range("Vertex index out of range");
    }
    return buffer_[index];
}

uint32_t VertexBufferManager::getOrCreateIndex(const VertexID& id, IVertexGenerator& generator) {
    // Check if we already have this vertex
    auto it = indexMap_.find(id);
    if (it != indexMap_.end()) {
        return it->second;
    }
    
    // Generate the vertex
    CachedVertex vertex = generator.getVertex(id);
    
    // Add to buffer
    uint32_t index = static_cast<uint32_t>(buffer_.size());
    buffer_.push_back(vertex);
    indexMap_[id] = index;
    
    return index;
}

// ============================================================================
// VertexGeneratorSystem Implementation (Singleton)
// ============================================================================

VertexGeneratorSystem& VertexGeneratorSystem::getInstance() {
    static VertexGeneratorSystem instance;
    return instance;
}

VertexGeneratorSystem::VertexGeneratorSystem() {
    // Create the simple generator by default
    generator_ = std::make_unique<SimpleVertexGenerator>();
}

void VertexGeneratorSystem::setPlanetRadius(double radius) {
    if (auto* simpleGen = dynamic_cast<SimpleVertexGenerator*>(generator_.get())) {
        simpleGen->setPlanetRadius(radius);
    }
}

void VertexGeneratorSystem::reset() {
    generator_->clearCache();
    bufferManager_.clear();
}

} // namespace PlanetRenderer