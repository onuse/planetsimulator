cmake_minimum_required(VERSION 3.10)
project(OctreeTests)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(glm REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/../include)

# Test executable for material pipeline
add_executable(test_material_pipeline 
    test_material_pipeline.cpp
    ../src/core/octree.cpp
)

target_link_libraries(test_material_pipeline 
    glm::glm
)

# Test executable for octree materials (existing test)
add_executable(test_octree_materials
    test_octree_materials.cpp
    ../src/core/octree.cpp
)

target_link_libraries(test_octree_materials
    glm::glm
)