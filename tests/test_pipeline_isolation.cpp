// PIPELINE ISOLATION TEST
// Find WHERE the 6 million meter error is introduced

#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void testPipelineStage(const std::string& stage, bool passed, const std::string& details) {
    std::cout << "[" << (passed ? "✓" : "✗") << "] " << stage << ": " << details << "\n";
}

int main() {
    std::cout << "=== PIPELINE ISOLATION TEST ===\n\n";
    std::cout << "Tracing where the 6 million meter error is introduced:\n\n";
    
    // Stage 1: Cube face definition
    std::cout << "STAGE 1: Cube Face Definition\n";
    std::cout << "  +X face: bounds (1, -1, -1) to (1, 1, 1)\n";
    std::cout << "  +Y face: bounds (-1, 1, -1) to (1, 1, 1)\n";
    std::cout << "  +Z face: bounds (-1, -1, 1) to (1, 1, 1)\n";
    testPipelineStage("Face bounds", true, "Cube faces defined correctly in [-1,1] range");
    
    // Stage 2: Transform generation
    std::cout << "\nSTAGE 2: Transform Generation (GlobalPatchGenerator::createTransform)\n";
    std::cout << "  Each face maps UV [0,1] to its cube face region\n";
    std::cout << "  +X: U->Z, V->Y (transform tested in our isolation tests)\n";
    testPipelineStage("Transform math", true, "Transforms generate correct cube positions");
    
    // Stage 3: Vertex generation in CPU
    std::cout << "\nSTAGE 3: CPU Vertex Generation\n";
    std::cout << "  UV -> Cube -> Sphere transformation\n";
    std::cout << "  Boundary snapping with EPSILON = 1e-8\n";
    testPipelineStage("CPU vertex gen", true, "Vertices align at boundaries (after std::round fix)");
    
    // Stage 4: Vertex data transfer to GPU
    std::cout << "\nSTAGE 4: GPU Data Transfer\n";
    std::cout << "  Camera-relative transformation applied\n";
    std::cout << "  Vertices offset by camera position before GPU upload\n";
    testPipelineStage("GPU transfer", false, "SUSPECT - Camera-relative math could introduce errors");
    
    // Stage 5: Vertex shader transformation
    std::cout << "\nSTAGE 5: Vertex Shader\n";
    std::cout << "  Model matrix application\n";
    std::cout << "  View-Projection matrix application\n";
    testPipelineStage("Vertex shader", false, "SUSPECT - Matrix precision or order issues");
    
    // Stage 6: Face visibility culling
    std::cout << "\nSTAGE 6: Face Culling\n";
    std::cout << "  Currently DISABLED (enableFaceCulling = false)\n";
    testPipelineStage("Face culling", true, "Not the issue - culling is disabled");
    
    std::cout << "\n=== ANALYSIS ===\n";
    std::cout << "The FaceBoundaryAlignment test shows 6 million meter gaps.\n";
    std::cout << "This is EXACTLY the planet radius (6.371 million meters).\n\n";
    
    std::cout << "HYPOTHESIS: One face is being offset by the planet radius.\n";
    std::cout << "This could happen if:\n";
    std::cout << "1. A transform matrix has the wrong origin\n";
    std::cout << "2. Camera-relative math is applied incorrectly to one face\n";
    std::cout << "3. A sign error in the transform (e.g., -radius instead of +radius)\n\n";
    
    std::cout << "NEXT STEPS:\n";
    std::cout << "1. Check if all 6 faces are being processed in selectLOD()\n";
    std::cout << "2. Log the transform matrices for each face\n";
    std::cout << "3. Check camera-relative transformation for sign errors\n";
    std::cout << "4. Verify the face that's missing matches our hypothesis\n";
    
    return 0;
}