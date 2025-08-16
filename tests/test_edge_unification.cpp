#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Test to understand the edge unification problem

int main() {
    // Consider two patches that share an edge:
    // Patch A: +Z face, right edge (U=1)
    // Patch B: +X face, top edge (V=1) 
    // They share the edge at X=1, Z=1
    
    // Patch A on +Z face
    // For +Z face: cube position = (U*2-1, V*2-1, 1)
    // At U=1, V=0.5: cube position = (1, 0, 1)
    
    // Patch B on +X face  
    // For +X face: cube position = (1, V*2-1, U*2-1)
    // At U=0.5, V=1: cube position = (1, 1, 0) 
    
    // WAIT! These don't match at all!
    // The problem is that the UV mappings are incompatible.
    
    std::cout << "Edge mismatch demonstration:\n";
    std::cout << "+Z face at U=1, V=0.5 -> cube position (1, 0, 1)\n";
    std::cout << "+X face at U=0.5, V=1 -> cube position (1, 1, 0)\n";
    std::cout << "These should be the same edge but they're not!\n\n";
    
    // The real issue: our face UV mappings don't align at edges!
    // For +Z face: U maps to X, V maps to Y
    // For +X face: U maps to Z, V maps to Y
    
    // So when +Z's right edge (U=1) meets +X's left edge (U=0),
    // they're parameterizing the shared edge differently!
    
    std::cout << "The fundamental problem:\n";
    std::cout << "+Z face edge (X=1, Y=[-1,1], Z=1) is parameterized by V\n"; 
    std::cout << "+X face edge (X=1, Y=[-1,1], Z=1) is parameterized by... NOTHING!\n";
    std::cout << "+X face can't even represent this edge with its UV mapping!\n\n";
    
    std::cout << "Actually, let me reconsider the mappings...\n";
    std::cout << "+Z face (Z=1 fixed): X = U*2-1, Y = V*2-1\n";
    std::cout << "  Right edge (U=1): X=1, Y=V*2-1, Z=1\n";
    std::cout << "+X face (X=1 fixed): Y = V*2-1, Z = U*2-1\n";
    std::cout << "  Top edge (V=1): X=1, Y=1, Z=U*2-1\n";
    std::cout << "\nThese edges are DIFFERENT!\n";
    std::cout << "+Z right edge: (1, [-1,1], 1) - vertical line\n";
    std::cout << "+X top edge: (1, 1, [-1,1]) - horizontal line\n";
    
    return 0;
}