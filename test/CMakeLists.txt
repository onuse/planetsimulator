cmake_minimum_required(VERSION 3.10)
project(PlanetSimulatorTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(glm REQUIRED)

# Add test executable
add_executable(test_octree_stages 
    test_octree_stages.cpp
)

# Include directories
target_include_directories(test_octree_stages PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${GLM_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(test_octree_stages PRIVATE
    glm::glm
)

# Add chrono for timing tests
if(UNIX)
    target_link_libraries(test_octree_stages PRIVATE pthread)
endif()

# Enable testing
enable_testing()
add_test(NAME OctreeStagesTest COMMAND test_octree_stages)