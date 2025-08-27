#!/usr/bin/env python3
"""
Simple C to GLSL transpiler for shader development
Allows writing shaders in C for testing, then converting to GLSL
"""

import sys
import re

def transpile_c_to_glsl(c_code, output_file):
    """
    Convert C-like shader code to GLSL
    """
    glsl = c_code
    
    # Detect shader type based on content or output file extension
    is_compute = output_file.endswith('.comp') or 'local_size_x' in c_code
    is_vertex = output_file.endswith('.vert') or 'gl_Position' in c_code
    
    # Replace C types with GLSL types
    type_map = {
        'typedef struct': 'struct',
        'float x, y, z;': 'vec3',
        'float x, y, z, w;': 'vec4',
        'uint32_t x, y, z, w;': 'uvec4',
        'uint32_t': 'uint',
        'int32_t': 'int',
        'bool': 'bool',
        'printf': '// printf',  # Comment out debug prints
        'assert': '// assert',
        'malloc': '// malloc',
        'free': '// free',
        'memset': '// memset',
        '#include <': '// #include <',
        'vec3 v = {': 'vec3 v = vec3(',
        'vec4 v = {': 'vec4 v = vec4(',
        'uvec4 v = {': 'uvec4 v = uvec4(',
    }
    
    for old, new in type_map.items():
        glsl = glsl.replace(old, new)
    
    # Convert vec3 operations
    glsl = re.sub(r'vec3_make\((.*?)\)', r'vec3(\1)', glsl)
    glsl = re.sub(r'vec3_add\((.*?), (.*?)\)', r'(\1 + \2)', glsl)
    glsl = re.sub(r'vec3_sub\((.*?), (.*?)\)', r'(\1 - \2)', glsl)
    glsl = re.sub(r'vec3_scale\((.*?), (.*?)\)', r'(\1 * \2)', glsl)
    glsl = re.sub(r'vec3_dot\((.*?), (.*?)\)', r'dot(\1, \2)', glsl)
    glsl = re.sub(r'vec3_length\((.*?)\)', r'length(\1)', glsl)
    glsl = re.sub(r'vec3_normalize\((.*?)\)', r'normalize(\1)', glsl)
    
    # Convert math functions
    glsl = glsl.replace('sqrtf(', 'sqrt(')
    glsl = glsl.replace('fmaxf(', 'max(')
    glsl = glsl.replace('fminf(', 'min(')
    
    # Convert struct initializers
    glsl = re.sub(r'= \{([^}]+)\}', r'= vec3(\1)', glsl)
    
    # For compute shaders, don't add any boilerplate - the template should be complete
    if is_compute:
        # Compute shaders should be self-contained, just return the processed GLSL
        return glsl
    
    # Add appropriate header for fragment/vertex shaders
    if is_vertex:
        header = """#version 450

// Vertex shader header
"""
    else:
        # Default fragment shader header
        header = """#version 450

layout(location = 0) out vec4 outColor;

// Uniforms
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float padding;
} ubo;

// Push constants
layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float planetRadius;
    int debugMode;
} pc;

// Storage buffer
layout(std430, binding = 1) readonly buffer NodeBuffer {
    OctreeNode nodes[];
} nodeBuffer;

"""
    
    # Find the main traversal function and convert it
    if 'TraversalResult traceOctree' in glsl:
        # Extract the function body and convert
        pass
    
    return header + glsl

def create_glsl_from_c(c_file, glsl_file):
    """
    Read C file, transpile, and write GLSL file
    """
    with open(c_file, 'r') as f:
        c_code = f.read()
    
    glsl_code = transpile_c_to_glsl(c_code, glsl_file)
    
    with open(glsl_file, 'w') as f:
        f.write(glsl_code)
    
    print(f"Transpiled {c_file} -> {glsl_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python c2glsl.py input.c output.frag")
        sys.exit(1)
    
    create_glsl_from_c(sys.argv[1], sys.argv[2])