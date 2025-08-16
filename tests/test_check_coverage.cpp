#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <set>
#include <glm/glm.hpp>

int main() {
    std::cout << "=== CHECKING PLANET COVERAGE ===" << std::endl;
    
    // Load the patch data
    std::ifstream file("planet_patches.txt");
    if (!file.is_open()) {
        std::cerr << "Could not open planet_patches.txt" << std::endl;
        return 1;
    }
    
    struct PatchInfo {
        int faceId;
        int level;
        glm::vec3 minBounds;
        glm::vec3 maxBounds;
        glm::vec3 center;
    };
    
    std::vector<PatchInfo> patches;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.find("PATCH ") == 0 && line.find("PATCH_COUNT") == std::string::npos) {
            PatchInfo patch;
            
            // Read faceId
            std::getline(file, line);
            sscanf(line.c_str(), "  faceId: %d", &patch.faceId);
            
            // Read level
            std::getline(file, line);
            sscanf(line.c_str(), "  level: %d", &patch.level);
            
            // Read minBounds
            std::getline(file, line);
            sscanf(line.c_str(), "  minBounds: %f %f %f", 
                   &patch.minBounds.x, &patch.minBounds.y, &patch.minBounds.z);
            
            // Read maxBounds
            std::getline(file, line);
            sscanf(line.c_str(), "  maxBounds: %f %f %f",
                   &patch.maxBounds.x, &patch.maxBounds.y, &patch.maxBounds.z);
            
            // Read center
            std::getline(file, line);
            sscanf(line.c_str(), "  center: %f %f %f",
                   &patch.center.x, &patch.center.y, &patch.center.z);
            
            patches.push_back(patch);
        }
    }
    
    std::cout << "Loaded " << patches.size() << " patches" << std::endl;
    
    // Analyze coverage per face
    std::cout << "\n=== FACE COVERAGE ===" << std::endl;
    
    for (int face = 0; face < 6; face++) {
        std::cout << "\nFace " << face << " (";
        switch(face) {
            case 0: std::cout << "+X"; break;
            case 1: std::cout << "-X"; break;
            case 2: std::cout << "+Y"; break;
            case 3: std::cout << "-Y"; break;
            case 4: std::cout << "+Z"; break;
            case 5: std::cout << "-Z"; break;
        }
        std::cout << "):" << std::endl;
        
        // Count patches and levels for this face
        int patchCount = 0;
        std::set<int> levels;
        float minX = 10, maxX = -10;
        float minY = 10, maxY = -10;
        float minZ = 10, maxZ = -10;
        
        for (const auto& patch : patches) {
            if (patch.faceId == face) {
                patchCount++;
                levels.insert(patch.level);
                
                minX = std::min(minX, patch.minBounds.x);
                maxX = std::max(maxX, patch.maxBounds.x);
                minY = std::min(minY, patch.minBounds.y);
                maxY = std::max(maxY, patch.maxBounds.y);
                minZ = std::min(minZ, patch.minBounds.z);
                maxZ = std::max(maxZ, patch.maxBounds.z);
            }
        }
        
        std::cout << "  Patches: " << patchCount << std::endl;
        std::cout << "  Levels: ";
        for (int level : levels) {
            std::cout << level << " ";
        }
        std::cout << std::endl;
        std::cout << "  Coverage: X[" << minX << " to " << maxX << "] "
                  << "Y[" << minY << " to " << maxY << "] "
                  << "Z[" << minZ << " to " << maxZ << "]" << std::endl;
        
        // Check if face is fully covered
        bool fullyCovered = true;
        switch(face) {
            case 0: // +X face
                if (std::abs(minX - 1.0) > 0.01 || std::abs(maxX - 1.0) > 0.01) fullyCovered = false;
                if (minY > -0.99 || maxY < 0.99) fullyCovered = false;
                if (minZ > -0.99 || maxZ < 0.99) fullyCovered = false;
                break;
            case 1: // -X face
                if (std::abs(minX + 1.0) > 0.01 || std::abs(maxX + 1.0) > 0.01) fullyCovered = false;
                if (minY > -0.99 || maxY < 0.99) fullyCovered = false;
                if (minZ > -0.99 || maxZ < 0.99) fullyCovered = false;
                break;
            case 2: // +Y face
                if (minX > -0.99 || maxX < 0.99) fullyCovered = false;
                if (std::abs(minY - 1.0) > 0.01 || std::abs(maxY - 1.0) > 0.01) fullyCovered = false;
                if (minZ > -0.99 || maxZ < 0.99) fullyCovered = false;
                break;
            case 3: // -Y face
                if (minX > -0.99 || maxX < 0.99) fullyCovered = false;
                if (std::abs(minY + 1.0) > 0.01 || std::abs(maxY + 1.0) > 0.01) fullyCovered = false;
                if (minZ > -0.99 || maxZ < 0.99) fullyCovered = false;
                break;
            case 4: // +Z face
                if (minX > -0.99 || maxX < 0.99) fullyCovered = false;
                if (minY > -0.99 || maxY < 0.99) fullyCovered = false;
                if (std::abs(minZ - 1.0) > 0.01 || std::abs(maxZ - 1.0) > 0.01) fullyCovered = false;
                break;
            case 5: // -Z face
                if (minX > -0.99 || maxX < 0.99) fullyCovered = false;
                if (minY > -0.99 || maxY < 0.99) fullyCovered = false;
                if (std::abs(minZ + 1.0) > 0.01 || std::abs(maxZ + 1.0) > 0.01) fullyCovered = false;
                break;
        }
        
        std::cout << "  Status: " << (fullyCovered ? "FULLY COVERED" : "INCOMPLETE COVERAGE!") << std::endl;
    }
    
    // Check for gaps in coverage
    std::cout << "\n=== CHECKING FOR GAPS ===" << std::endl;
    
    // For each face, check if patches cover the full [-1,1] range
    for (int face = 0; face < 6; face++) {
        // Build a 2D coverage map for this face
        // We'll check coverage at a coarse level
        const int gridSize = 8;
        bool covered[gridSize][gridSize] = {false};
        
        for (const auto& patch : patches) {
            if (patch.faceId != face) continue;
            
            // Map patch bounds to grid
            float u_min, u_max, v_min, v_max;
            
            switch(face) {
                case 0: case 1: // X faces
                    u_min = (patch.minBounds.z + 1.0) / 2.0;
                    u_max = (patch.maxBounds.z + 1.0) / 2.0;
                    v_min = (patch.minBounds.y + 1.0) / 2.0;
                    v_max = (patch.maxBounds.y + 1.0) / 2.0;
                    break;
                case 2: case 3: // Y faces
                    u_min = (patch.minBounds.x + 1.0) / 2.0;
                    u_max = (patch.maxBounds.x + 1.0) / 2.0;
                    v_min = (patch.minBounds.z + 1.0) / 2.0;
                    v_max = (patch.maxBounds.z + 1.0) / 2.0;
                    break;
                case 4: case 5: // Z faces
                    u_min = (patch.minBounds.x + 1.0) / 2.0;
                    u_max = (patch.maxBounds.x + 1.0) / 2.0;
                    v_min = (patch.minBounds.y + 1.0) / 2.0;
                    v_max = (patch.maxBounds.y + 1.0) / 2.0;
                    break;
            }
            
            // Mark grid cells as covered
            int i_min = (int)(u_min * gridSize);
            int i_max = (int)(u_max * gridSize);
            int j_min = (int)(v_min * gridSize);
            int j_max = (int)(v_max * gridSize);
            
            for (int i = i_min; i < i_max && i < gridSize; i++) {
                for (int j = j_min; j < j_max && j < gridSize; j++) {
                    if (i >= 0 && j >= 0) {
                        covered[i][j] = true;
                    }
                }
            }
        }
        
        // Count gaps
        int gaps = 0;
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                if (!covered[i][j]) gaps++;
            }
        }
        
        if (gaps > 0) {
            std::cout << "Face " << face << ": " << gaps << " gaps out of " 
                      << (gridSize * gridSize) << " cells" << std::endl;
        }
    }
    
    return 0;
}