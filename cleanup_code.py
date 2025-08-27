#!/usr/bin/env python3
"""
Spring cleaning script to remove/disable dormant code paths
Run with: python cleanup_code.py
"""

import os
import shutil
from datetime import datetime

def backup_file(filepath):
    """Create a backup before modifying"""
    backup_path = filepath + f".backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    shutil.copy2(filepath, backup_path)
    print(f"Backed up: {filepath}")
    return backup_path

def comment_out_section(filepath, start_marker, end_marker):
    """Comment out a section of code between markers"""
    backup_file(filepath)
    
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    in_section = False
    modified = []
    for line in lines:
        if start_marker in line:
            in_section = True
            modified.append(f"// SPRING_CLEANING: Disabled GPU mesh generation\n")
            modified.append(f"#if 0 // {line}")
        elif end_marker in line and in_section:
            modified.append(f"{line}")
            modified.append("#endif // SPRING_CLEANING\n")
            in_section = False
        else:
            modified.append(line)
    
    with open(filepath, 'w') as f:
        f.writelines(modified)
    print(f"Modified: {filepath}")

def delete_unused_files():
    """Delete unused shader templates"""
    unused_templates = [
        "shaders/src/templates/mesh_generator_compute_altitude.c",
        "shaders/src/templates/mesh_generator_compute_analyze.c",
        "shaders/src/templates/mesh_generator_compute_template.c",
        "shaders/src/templates/mesh_generator_compute_template_debug.c",
        "shaders/src/templates/mesh_generator_compute_template_fixed.c",
        "shaders/src/templates/mesh_generator_compute_template_octree.c",
        "shaders/src/templates/mesh_generator_compute_template_old.c",
    ]
    
    for filepath in unused_templates:
        if os.path.exists(filepath):
            # Move to archive instead of deleting
            archive_dir = "shaders/archive/templates"
            os.makedirs(archive_dir, exist_ok=True)
            archive_path = os.path.join(archive_dir, os.path.basename(filepath))
            shutil.move(filepath, archive_path)
            print(f"Archived: {filepath} -> {archive_path}")
        else:
            print(f"Not found: {filepath}")

def main():
    print("=== Spring Cleaning - Removing Dormant Code Paths ===\n")
    
    # 1. Archive unused shader templates
    print("1. Archiving unused shader templates...")
    delete_unused_files()
    
    # 2. Disable GPU mesh generation in LODManager
    print("\n2. Disabling GPU mesh generation code...")
    # This would need proper C++ parsing to do safely
    print("   -> Manual review needed for LODManager.cpp")
    
    # 3. Remove test vertex generators
    print("\n3. Removing test vertex generators...")
    print("   -> Manual review needed for vertex_generator.hpp")
    
    print("\n=== Cleanup Summary ===")
    print("- Archived unused shader templates")
    print("- TODO: Manually remove GPU mesh generation code from LODManager")
    print("- TODO: Remove SimpleVertexGenerator and IVertexGenerator")
    print("- TODO: Consolidate to single CPU vertex generation path")
    
    print("\nNext steps:")
    print("1. Review and test the changes")
    print("2. Remove backup files once confirmed working")
    print("3. Update CMakeLists.txt to remove unused files")

if __name__ == "__main__":
    main()