#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>

// Simple render logging system to track what we're actually sending to GPU
class RenderLogger {
public:
    struct DrawCall {
        uint32_t vertexCount;
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t faceId;
        uint32_t patchLevel;
        glm::vec3 patchCenter;
        std::string label;
    };
    
    static RenderLogger& getInstance() {
        static RenderLogger instance;
        return instance;
    }
    
    void startFrame(uint32_t frameNumber) {
        currentFrame = frameNumber;
        drawCalls.clear();
        patchesPerFace.fill(0);
        verticesPerFace.fill(0);
    }
    
    void logPatchGenerated(uint32_t faceId, const glm::vec3& center, uint32_t level) {
        patchesPerFace[faceId]++;
        if (logVerbose) {
            std::cout << "[PATCH] Face " << faceId << " Level " << level 
                      << " Center(" << center.x << "," << center.y << "," << center.z << ")\n";
        }
    }
    
    void logVerticesGenerated(uint32_t faceId, uint32_t count) {
        verticesPerFace[faceId] += count;
    }
    
    void logDrawCall(uint32_t vertexCount, uint32_t indexCount, uint32_t instanceCount, 
                     const std::string& label = "") {
        DrawCall call;
        call.vertexCount = vertexCount;
        call.indexCount = indexCount;
        call.instanceCount = instanceCount;
        call.label = label;
        drawCalls.push_back(call);
        
        if (logVerbose) {
            std::cout << "[DRAW] " << label << " Vertices:" << vertexCount 
                      << " Indices:" << indexCount << " Instances:" << instanceCount << "\n";
        }
    }
    
    void endFrame() {
        // Log frame summary every N frames
        if (currentFrame % 60 == 0) {  // Every second at 60fps
            std::cout << "\n=== RENDER FRAME " << currentFrame << " SUMMARY ===\n";
            
            // Patches per face
            std::cout << "Patches per face: ";
            for (int i = 0; i < 6; i++) {
                std::cout << "F" << i << ":" << patchesPerFace[i] << " ";
            }
            std::cout << "\n";
            
            // Vertices per face
            std::cout << "Vertices per face: ";
            for (int i = 0; i < 6; i++) {
                std::cout << "F" << i << ":" << verticesPerFace[i] << " ";
            }
            std::cout << "\n";
            
            // Draw calls
            std::cout << "Total draw calls: " << drawCalls.size() << "\n";
            
            uint32_t totalVertices = 0;
            uint32_t totalIndices = 0;
            for (const auto& call : drawCalls) {
                totalVertices += call.vertexCount * call.instanceCount;
                totalIndices += call.indexCount * call.instanceCount;
            }
            std::cout << "Total vertices submitted: " << totalVertices << "\n";
            std::cout << "Total indices submitted: " << totalIndices << "\n";
            
            // Check for missing faces
            std::cout << "Missing faces: ";
            bool anyMissing = false;
            for (int i = 0; i < 6; i++) {
                if (patchesPerFace[i] == 0) {
                    std::cout << i << " ";
                    anyMissing = true;
                }
            }
            if (!anyMissing) std::cout << "None";
            std::cout << "\n";
            
            std::cout << "=====================================\n\n";
        }
    }
    
    void dumpToFile(const std::string& filename) {
        std::ofstream file(filename);
        file << "Frame " << currentFrame << " Render Log\n";
        file << "====================================\n\n";
        
        for (int i = 0; i < 6; i++) {
            file << "Face " << i << ": " << patchesPerFace[i] << " patches, " 
                 << verticesPerFace[i] << " vertices\n";
        }
        
        file << "\nDraw Calls:\n";
        for (size_t i = 0; i < drawCalls.size(); i++) {
            const auto& call = drawCalls[i];
            file << i << ": " << call.label << " V:" << call.vertexCount 
                 << " I:" << call.indexCount << " Inst:" << call.instanceCount << "\n";
        }
        
        file.close();
    }
    
    void setVerbose(bool verbose) { logVerbose = verbose; }
    
private:
    RenderLogger() = default;
    
    uint32_t currentFrame = 0;
    bool logVerbose = false;
    std::vector<DrawCall> drawCalls;
    std::array<uint32_t, 6> patchesPerFace = {0};
    std::array<uint32_t, 6> verticesPerFace = {0};
};