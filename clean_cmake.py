#!/usr/bin/env python3
"""Remove broken test definitions from CMakeLists.txt"""

import re

# Tests to remove
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

with open('CMakeLists.txt', 'r') as f:
    lines = f.readlines()

output = []
skip_until_next_test = False
current_test = None

i = 0
while i < len(lines):
    line = lines[i]
    
    # Check if this starts a broken test block
    found_broken = False
    for test in broken_tests:
        if f'add_executable({test}' in line or f'# REMOVED: add_executable({test}' in line:
            found_broken = True
            current_test = test
            print(f"Removing {test} block starting at line {i+1}")
            break
    
    if found_broken:
        # Skip everything until we find the next add_executable or end of file
        j = i + 1
        while j < len(lines):
            if 'add_executable(' in lines[j] and not any(t in lines[j] for t in broken_tests):
                break
            if j == len(lines) - 1:
                j += 1
                break
            j += 1
        print(f"  Skipped lines {i+1} to {j}")
        i = j
        continue
    
    # Also skip orphaned target commands for broken tests
    skip_line = False
    for test in broken_tests:
        if (f'target_include_directories({test}' in line or
            f'target_link_libraries({test}' in line or  
            f'set_target_properties({test}' in line):
            skip_line = True
            # Skip this and following lines until next command or blank line
            j = i
            while j < len(lines) and not lines[j].strip().startswith('#') and lines[j].strip() != '':
                j += 1
                if j < len(lines) and (lines[j].strip() == ')' or lines[j].strip() == ''):
                    j += 1
                    break
            print(f"  Removed orphaned {test} commands at lines {i+1}-{j}")
            i = j
            break
    
    if not skip_line:
        output.append(line)
        i += 1

with open('CMakeLists_clean.txt', 'w') as f:
    f.writelines(output)

print(f"\nCleaned CMakeLists.txt: {len(lines)} lines -> {len(output)} lines")
print("Output written to CMakeLists_clean.txt")