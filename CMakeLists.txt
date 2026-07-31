cmake_minimum_required(VERSION 3.20)
project(PlanetSimulator VERSION 1.0.0 LANGUAGES CXX)

# GLFW 3.3.8 declares a cmake_minimum_required that CMake 4 rejects outright,
# so configuring needs -DCMAKE_POLICY_VERSION_MINIMUM=3.5 until it is bumped.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# ============================================================================
# Dependencies
# ============================================================================

include(FetchContent)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
    GIT_SHALLOW TRUE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm imgui)

# Vulkan's clip space uses a [0, 1] depth range; GLM defaults to OpenGL's
# [-1, 1]. Without this, glm::perspective() maps the near half of the frustum
# to negative NDC z, which Vulkan clips away - geometry silently disappears.
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)

# ============================================================================
# planetsim - the simulation, with no rendering in it
# ============================================================================
#
# Built once as a library rather than recompiled per test target. There used to
# be sixteen test executables each compiling these same files from scratch,
# which was the single largest cost in the build.

add_library(planetsim STATIC
    src/core/camera.cpp
    src/core/density_field.cpp
    src/core/material_table.cpp
    src/core/octree.cpp
    src/tools/control_server.cpp
    src/tools/control_commands.cpp
    src/utils/parallel.cpp
    src/simulation/climate.cpp
    src/simulation/crust_grid.cpp
    src/simulation/erosion.cpp
    src/simulation/plate_dynamics.cpp
    src/rendering/patch_tree.cpp   # surface geometry; no Vulkan, so it is testable
)
target_include_directories(planetsim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(planetsim PUBLIC glm::glm Threads::Threads)

if(WIN32)
    target_link_libraries(planetsim PUBLIC ws2_32)
endif()

if(WIN32)
    target_compile_definitions(planetsim PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ============================================================================
# PlanetSimulator - the application
# ============================================================================

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/rendering/imgui_manager.cpp
    src/rendering/vulkan_renderer.cpp
    src/rendering/vulkan_renderer_commands.cpp
    src/rendering/vulkan_renderer_cpu_upload.cpp
    src/rendering/vulkan_renderer_device.cpp
    src/rendering/vulkan_renderer_material.cpp
    src/rendering/patch_builder.cpp
    src/rendering/vulkan_renderer_patches.cpp
    src/rendering/vulkan_renderer_pipeline.cpp
    src/rendering/vulkan_renderer_resources.cpp
    src/rendering/vulkan_renderer_screenshot_impl.cpp
    src/rendering/vulkan_renderer_transvoxel.cpp
    src/utils/screenshot.cpp

    # ImGui has no CMake target of its own
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(${PROJECT_NAME} PRIVATE planetsim ${Vulkan_LIBRARIES} glfw)

# The shaders live beside the executable. compile.bat builds and copies them;
# this only guarantees the directory exists.
add_custom_target(CreateShaderDir ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
    COMMENT "Creating shader output directory")
add_dependencies(${PROJECT_NAME} CreateShaderDir)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX- /MP /permissive-
        $<$<CONFIG:Release>:/O2>)
    target_compile_options(planetsim PRIVATE /W4 /WX- /MP /permissive-
        $<$<CONFIG:Release>:/O2>)
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra
        $<$<CONFIG:Release>:-O3>)
    target_compile_options(planetsim PRIVATE -Wall -Wextra
        $<$<CONFIG:Release>:-O3>)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders DESTINATION bin)

# ============================================================================
# Tests
# ============================================================================
#
# Every test links the planetsim library, so the simulation is compiled once
# for the whole suite. Tests that covered deleted subsystems - the cube-sphere
# quadtree, the GPU compute mesh paths, the Transvoxel renderer - went with
# them; a passing test for code nobody calls is worse than no test, because it
# reads as coverage.

enable_testing()

function(planet_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE planetsim)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

planet_test(test_crust_grid        tests/test_crust_grid.cpp)         # plate tectonics
planet_test(test_density_field     tests/test_density_field.cpp)      # terrain field
planet_test(test_octree_core       tests/test_octree_core.cpp)        # voxel octree
planet_test(test_mixed_voxel       tests/test_mixed_voxel.cpp)        # voxel data model
planet_test(test_material_table    tests/test_material_table.cpp)     # materials
planet_test(test_materials         tests/test_materials.cpp)          # materials in the octree
planet_test(test_planet_math       tests/test_planet_math_simple.cpp) # planet-scale maths
planet_test(test_mesh_connectivity tests/test_mesh_connectivity.cpp)  # mesh topology
planet_test(test_chunk_boundaries  tests/test_chunk_boundaries.cpp)   # voxel chunk seams
planet_test(test_camera_drag       tests/test_camera_drag.cpp)        # dragging the planet
planet_test(test_patch_seams       tests/test_patch_seams.cpp)        # cracks between patches

message(STATUS "")
message(STATUS "=== PlanetSimulator ===")
message(STATUS "  Vulkan:  ${Vulkan_LIBRARY}")
message(STATUS "  Output:  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
