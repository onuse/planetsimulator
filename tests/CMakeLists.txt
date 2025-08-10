cmake_minimum_required(VERSION 3.10)
project(OctreeTests)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(glm REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/../include)

# Test executable for material pipeline
add_executable(test_material_pipeline 
    pipeline/test_material_pipeline.cpp
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

# Test executable for Transvoxel algorithm
add_executable(test_transvoxel_algorithm
    test_transvoxel_algorithm.cpp
    ../src/algorithms/mesh_generation.cpp
    ../src/core/octree.cpp
    ../src/core/material_table.cpp
)

target_link_libraries(test_transvoxel_algorithm
    glm::glm
)

# Debug test for Transvoxel algorithm
add_executable(test_transvoxel_debug
    test_transvoxel_debug.cpp
    ../src/algorithms/mesh_generation.cpp
    ../src/core/octree.cpp
    ../src/core/material_table.cpp
)

target_link_libraries(test_transvoxel_debug
    glm::glm
)

# Test executable for LOD system
add_executable(test_lod_system
    test_lod_system.cpp
    ../src/core/density_field.cpp
    ../src/core/spherical_quadtree.cpp
)

target_link_libraries(test_lod_system
    glm::glm
)