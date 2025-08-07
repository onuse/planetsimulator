#!/usr/bin/env python3
"""
Simple GLSL extractor for templates that use GLSL_BEGIN/GLSL_END markers
"""

import sys
import re

def extract_glsl(input_file, output_file):
    with open(input_file, 'r') as f:
        content = f.read()
    
    # Look for content between // GLSL_BEGIN and // GLSL_END
    match = re.search(r'// GLSL_BEGIN(.*?)// GLSL_END', content, re.DOTALL)
    
    if match:
        glsl_code = match.group(1).strip()
        with open(output_file, 'w') as f:
            f.write(glsl_code)
        print(f"Extracted GLSL from {input_file} to {output_file}")
    else:
        print(f"WARNING: No GLSL_BEGIN/GLSL_END markers found in {input_file}")
        return 1
    
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python extract_simple_glsl.py input.c output.frag")
        sys.exit(1)
    
    sys.exit(extract_glsl(sys.argv[1], sys.argv[2]))