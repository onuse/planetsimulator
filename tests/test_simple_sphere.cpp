#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <glm/glm.hpp>

// Generate a simple sphere mesh for testing
void generateSimpleSphere(float radius, int segments, const char* filename) {
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    
    // Generate vertices
    for (int lat = 0; lat <= segments; lat++) {
        float theta = M_PI * float(lat) / float(segments);
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= segments; lon++) {
            float phi = 2.0f * M_PI * float(lon) / float(segments);
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;
            
            vertices.push_back(glm::vec3(x * radius, y * radius, z * radius));
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < segments; lat++) {
        for (int lon = 0; lon < segments; lon++) {
            int first = lat * (segments + 1) + lon;
            int second = first + segments + 1;
            
            // First triangle
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            // Second triangle
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    // Write to OBJ file
    std::ofstream obj(filename);
    obj << "# Simple sphere mesh\n";
    obj << "# Vertices: " << vertices.size() << "\n";
    obj << "# Triangles: " << indices.size() / 3 << "\n\n";
    
    for (const auto& v : vertices) {
        obj << "v " << v.x << " " << v.y << " " << v.z << "\n";
    }
    
    obj << "\n";
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        obj << "f " << (indices[i] + 1) << " " << (indices[i+1] + 1) << " " << (indices[i+2] + 1) << "\n";
    }
    
    obj.close();
    
    std::cout << "Generated sphere with " << vertices.size() << " vertices and " 
              << indices.size() / 3 << " triangles\n";
    std::cout << "Saved to " << filename << "\n";
}

int main() {
    std::cout << "Generating simple sphere mesh for comparison...\n";
    generateSimpleSphere(1000.0f, 32, "simple_sphere.obj");
    
    std::cout << "\nThis sphere should look correct. Compare with mesh_debug.obj\n";
    std::cout << "to see what's wrong with the Transvoxel generation.\n";
    
    return 0;
}