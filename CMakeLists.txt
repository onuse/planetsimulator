cmake_minimum_required(VERSION 3.20)
project(OctreePlanet VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set Vulkan SDK path
if(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK $ENV{VULKAN_SDK})
elseif(EXISTS "C:/VulkanSDK/1.4.321.1")
    set(VULKAN_SDK "C:/VulkanSDK/1.4.321.1")
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

# Find packages
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# Collect source files
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Exclude the stub now that we have actual implementations
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/vulkan_renderer_stub.cpp")
# Exclude the placeholder screenshot file since we have a real implementation now
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/vulkan_renderer_screenshot.cpp")
# Exclude debug tools that aren't ready yet
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/debug_zoom_capture.cpp")

file(GLOB_RECURSE HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        ${Vulkan_LIBRARIES}
        Threads::Threads
)

# Fetch external dependencies
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

# stb is included locally in include/stb_image_write.h
# No need to fetch it

# FastNoise2 (for terrain generation)
FetchContent_Declare(
    FastNoise2
    GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_TESTS OFF CACHE BOOL "" FORCE)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.1
    GIT_SHALLOW TRUE
)

# Make dependencies available
FetchContent_MakeAvailable(glfw glm FastNoise2 imgui)

# ImGui doesn't provide a CMake target, so we need to add the source files manually
if(imgui_POPULATED)
    # Core ImGui files
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        # Backends for GLFW and Vulkan
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    
    # Add ImGui sources to our executable
    target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES})
    
    # Add ImGui include directories
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
endif()

# Link fetched libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        glfw
        glm::glm
        FastNoise
)

# Shader compilation
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

# Shader copying is now handled by build.bat to ensure freshness
# CMake no longer copies shaders automatically to avoid stale shader issues
# The build.bat script will:
#   1. Check shader timestamps
#   2. Recompile if needed
#   3. Copy fresh shaders to the output directory
# This ensures shaders are always up-to-date

# Just ensure the shader output directory exists
add_custom_target(CreateShaderDir ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    COMMENT "Creating shader output directory"
)

add_dependencies(${PROJECT_NAME} CreateShaderDir)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Compiler-specific settings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4           # Warning level 4
        /WX-          # Don't treat warnings as errors (for now)
        /MP           # Multi-processor compilation
        /permissive-  # Strict standards conformance
        $<$<CONFIG:Debug>:/Zi>      # Debug info
        $<$<CONFIG:Release>:/O2>    # Optimize for speed
        $<$<CONFIG:Release>:/GL>    # Whole program optimization
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG>  # Link-time code generation
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>
    )
endif()

# Set Visual Studio debugger working directory
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} 
        PROPERTY VS_DEBUGGER_WORKING_DIRECTORY 
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${SHADER_OUTPUT_DIR}
    DESTINATION bin
)

# Enable testing
enable_testing()

# ============================================================================
# Core Test Suite - Clean, focused tests that actually work
# ============================================================================

# Test 1: Core octree functionality
add_executable(test_octree_core
    tests/test_octree_core.cpp
    src/core/octree.cpp
    src/core/camera.cpp
)
target_include_directories(test_octree_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_octree_core PRIVATE glm::glm)
set_target_properties(test_octree_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME OctreeCore COMMAND test_octree_core)

# Test 1b: Material table functionality
add_executable(test_material_table
    tests/test_material_table.cpp
    src/core/material_table.cpp
)
target_include_directories(test_material_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_material_table PRIVATE glm::glm)
set_target_properties(test_material_table PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME MaterialTable COMMAND test_material_table)

# Test 2: Material system
add_executable(test_materials
    tests/test_materials.cpp
    src/core/octree.cpp
    src/core/camera.cpp
)
target_include_directories(test_materials PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_materials PRIVATE glm::glm)
set_target_properties(test_materials PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME Materials COMMAND test_materials)

# Test 3: GPU rendering pipeline
add_executable(test_gpu_rendering
    tests/test_gpu_rendering.cpp
    src/core/octree.cpp
    src/core/camera.cpp
)
target_include_directories(test_gpu_rendering PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_gpu_rendering PRIVATE glm::glm)
set_target_properties(test_gpu_rendering PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME GPURendering COMMAND test_gpu_rendering)

# Add custom target to run tests (can be run manually with: cmake --build . --target run_tests)
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_octree_core test_material_table test_materials test_gpu_rendering
            test_material_pipeline test_hierarchical_octree
            test_frustum_culling test_lod_selection
            test_mixed_voxel test_mixed_voxel_integration
            test_material_tracking test_gpu_validation
            test_shader_attributes test_gpu_attributes
            test_full_pipeline
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running full test suite..."
)

# Tests are run by build.bat after the build completes

# Print configuration summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Vulkan SDK: ${VULKAN_SDK}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "============================")
message(STATUS "")

# Add test executable for material pipeline debugging
add_executable(test_material_pipeline
    tests/pipeline/test_material_pipeline.cpp
    src/core/octree.cpp
)

target_include_directories(test_material_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)

target_link_libraries(test_material_pipeline 
    PRIVATE
        glm::glm
)

set_target_properties(test_material_pipeline PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# ============================================================================
# Additional Tests - Tests that exist and should be kept
# ============================================================================

# Test for hierarchical octree
add_executable(test_hierarchical_octree
    tests/test_hierarchical_octree.cpp
    src/core/octree.cpp
    src/rendering/hierarchical_gpu_octree.cpp
)
target_include_directories(test_hierarchical_octree PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_hierarchical_octree PRIVATE
    glm::glm
    ${Vulkan_LIBRARIES}
)
set_target_properties(test_hierarchical_octree PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME HierarchicalOctree COMMAND test_hierarchical_octree)

# Frustum culling tests
add_executable(test_frustum_culling
    tests/test_frustum_culling.cpp
)
target_include_directories(test_frustum_culling PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_frustum_culling PRIVATE 
    glm::glm
    ${Vulkan_LIBRARIES}
)
set_target_properties(test_frustum_culling PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME FrustumCulling COMMAND test_frustum_culling)

# LOD selection tests
add_executable(test_lod_selection
    tests/test_lod_selection.cpp
)
target_include_directories(test_lod_selection PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_lod_selection PRIVATE glm::glm)
set_target_properties(test_lod_selection PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME LODSelection COMMAND test_lod_selection)

# Material tracking test
add_executable(test_material_tracking
    tests/pipeline/test_material_tracking.cpp
    src/core/octree.cpp
    src/rendering/instance_buffer_manager.cpp
    src/rendering/gpu_octree.cpp
)
target_include_directories(test_material_tracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_material_tracking PRIVATE
    glm::glm
    ${Vulkan_LIBRARIES}
)
set_target_properties(test_material_tracking PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME MaterialTracking COMMAND test_material_tracking)


add_executable(test_mixed_voxel
    tests/test_mixed_voxel.cpp
)
target_include_directories(test_mixed_voxel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_mixed_voxel PRIVATE glm::glm)
set_target_properties(test_mixed_voxel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME MixedVoxel COMMAND test_mixed_voxel)


# Mixed voxel integration test
add_executable(test_mixed_voxel_integration
    tests/test_mixed_voxel_integration.cpp
    src/core/octree.cpp
    src/rendering/instance_buffer_manager.cpp
)
target_include_directories(test_mixed_voxel_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_mixed_voxel_integration PRIVATE glm::glm)
set_target_properties(test_mixed_voxel_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME MixedVoxelIntegration COMMAND test_mixed_voxel_integration)

# ============================================================================
# Additional Pipeline Tests
# ============================================================================

# GPU validation test
add_executable(test_gpu_validation
    tests/pipeline/test_gpu_validation.cpp
)
target_include_directories(test_gpu_validation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_gpu_validation PRIVATE 
    glm::glm
    ${Vulkan_LIBRARIES}
)
set_target_properties(test_gpu_validation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME GPUValidation COMMAND test_gpu_validation)

# Shader attributes test  
add_executable(test_shader_attributes
    tests/pipeline/test_shader_attributes.cpp
)
target_include_directories(test_shader_attributes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_shader_attributes PRIVATE 
    glm::glm
    ${Vulkan_LIBRARIES}
)
set_target_properties(test_shader_attributes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME ShaderAttributes COMMAND test_shader_attributes)

# GPU attributes test
add_executable(test_gpu_attributes
    tests/rendering/test_gpu_attributes.cpp
)
target_include_directories(test_gpu_attributes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(test_gpu_attributes PRIVATE ${Vulkan_LIBRARIES})
set_target_properties(test_gpu_attributes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME GPUAttributes COMMAND test_gpu_attributes)

# Full pipeline integration test
add_executable(test_full_pipeline
    tests/test_full_pipeline.cpp
    src/core/octree.cpp
    src/core/camera.cpp
)
target_include_directories(test_full_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
)
target_link_libraries(test_full_pipeline PRIVATE glm::glm)
set_target_properties(test_full_pipeline PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_test(NAME FullPipeline COMMAND test_full_pipeline)

