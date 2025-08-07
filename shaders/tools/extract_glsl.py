#!/usr/bin/env python3
"""
Extract GLSL code from C/GLSL template files
Extracts sections marked for GLSL and shared code
"""

import sys
import re

def extract_glsl(input_file, output_file):
    with open(input_file, 'r') as f:
        content = f.read()
    
    glsl_parts = []
    
    # Extract GLSL header section (between #ifdef GLSL and #endif within GLSL_HEADER)
    header_match = re.search(r'// GLSL_HEADER_START(.*?)// GLSL_HEADER_END', content, re.DOTALL)
    if header_match:
        header = header_match.group(1)
        # Extract only the GLSL part
        glsl_part = re.search(r'#ifdef GLSL(.*?)#endif', header, re.DOTALL)
        if glsl_part:
            glsl_parts.append(glsl_part.group(1).strip())
    
    # Extract shared code (between C_HEADER_END and GLSL_MAIN_START)
    shared_match = re.search(r'// C_HEADER_END(.*?)// GLSL_MAIN_START', content, re.DOTALL)
    if shared_match:
        shared = shared_match.group(1)
        # Remove any C-only blocks
        shared = re.sub(r'#ifndef GLSL.*?#endif', '', shared, flags=re.DOTALL)
        shared = re.sub(r'/\*\*.*?\*/', '', shared, flags=re.DOTALL)
        # Fix struct declarations for GLSL
        shared = re.sub(r'struct OctreeNode\s+(\w+)\s*=', r'OctreeNode \1 =', shared)
        glsl_parts.append(shared.strip())
    
    # Extract GLSL main section
    main_match = re.search(r'// GLSL_MAIN_START(.*?)// GLSL_MAIN_END', content, re.DOTALL)
    if main_match:
        main = main_match.group(1)
        # Extract only the GLSL part
        glsl_part = re.search(r'#ifdef GLSL(.*?)#endif', main, re.DOTALL)
        if glsl_part:
            glsl_parts.append(glsl_part.group(1).strip())
    
    # Join all parts
    glsl_code = '\n\n'.join(glsl_parts)
    
    # Write output
    with open(output_file, 'w') as f:
        f.write(glsl_code)
    
    print(f"Extracted GLSL from {input_file} to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python extract_glsl.py input.c output.frag")
        sys.exit(1)
    
    extract_glsl(sys.argv[1], sys.argv[2])