#!/usr/bin/env python3
"""Complete cleanup of CMakeLists.txt - remove all broken test definitions"""

import re

# Read the file
with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()

# Tests that don't have corresponding source files
broken_tests = [
    'test_voxel_positions',
    'test_rendering_verification', 
    'test_full_pipeline',
    'test_planet_scale_materials',
    'test_material_isolation',
    'test_material_stages',
    'test_material_method',
    'test_grid_derivation',
    'test_planet_visibility',
    'test_voxel_data_flow'
]

output = []
i = 0
in_broken_section = False
current_test = None

while i < len(lines):
    line = lines[i]
    
    # Check if this is a broken test section (including commented ones)
    is_broken_line = False
    for test in broken_tests:
        if test in line and ('add_executable' in line or 'target_' in line or 'set_target_properties' in line):
            is_broken_line = True
            current_test = test
            in_broken_section = True
            print(f"Removing line {i+1}: {line.strip()}")
            break
    
    if is_broken_line:
        # Skip this line
        i += 1
        continue
    
    # If we're in a broken section, check if we've reached the end
    if in_broken_section:
        # End of section is marked by an empty line or a new command that's not related
        if (line.strip() == '' or 
            (line.strip() and not line.strip().startswith('#') and 
             not line.strip().startswith(')') and
             'PRIVATE' not in line and
             '${' not in line and
             current_test not in line)):
            in_broken_section = False
            current_test = None
            # Keep this line since it's not part of the broken section
            output.append(line)
        else:
            print(f"  Skipping line {i+1}: {line.strip()}")
        i += 1
        continue
    
    # Keep the line
    output.append(line)
    i += 1

# Write the cleaned version
with open('CMakeLists.txt', 'w') as f:
    f.writelines(output)

print(f"\nCleaned CMakeLists.txt: {len(lines)} lines -> {len(output)} lines")
print("Broken test definitions removed successfully")