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

# Print configuration summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Vulkan SDK: ${VULKAN_SDK}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "============================")
message(STATUS "")