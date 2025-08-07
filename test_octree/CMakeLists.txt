cmake_minimum_required(VERSION 3.10)
project(TestOctree)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# CPU test executable
add_executable(test_octree_cpu main.cpp)
target_link_libraries(test_octree_cpu glm::glm)

# GPU test executable (optional, complex Vulkan setup)
add_executable(test_octree_gpu test_gpu.cpp)
target_link_libraries(test_octree_gpu 
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Shader compilation
add_custom_target(shaders ALL
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_octree.vert -o ${CMAKE_CURRENT_BINARY_DIR}/test_octree.vert.spv
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_octree.frag -o ${CMAKE_CURRENT_BINARY_DIR}/test_octree.frag.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_octree.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_octree.frag
    COMMENT "Compiling shaders..."
)