# Archived Shaders

This directory contains deprecated shaders that are no longer used in the project.

## Why Archived?

The project originally used a ray-marching approach for rendering the octree planet data. This has been replaced with the Transvoxel algorithm that generates triangle meshes, which provides better performance and visual quality.

## Archived Files

### Vertex Shaders
- `hierarchical.vert` - Ray-marching hierarchical octree traversal
- `hierarchical_octree.vert` - Variant of hierarchical traversal
- `octree_raymarch.vert` - Simple octree ray-marching
- `test_simple.vert` - Test shader
- `triangle_debug.vert` - Debug version of triangle shader
- `triangle_new.vert` - Experimental version of triangle shader  
- `triangle_old.vert` - Old version of triangle shader

### Fragment Shaders
- `hierarchical.frag` - Ray-marching fragment shader (transpiled from template)
- `octree_raymarch.frag` - Octree ray-marching fragment shader (transpiled from template)

### Templates
- `hierarchical_template.c` - C template for hierarchical fragment shader
- `octree_raymarch_template.c` - C template for octree raymarch fragment shader

### Compiled SPIR-V
- `*.spv` files - Compiled versions of the above shaders

## Current Active Shaders

The project now uses only:
- `triangle.vert` - Vertex shader for Transvoxel mesh rendering
- `triangle.frag` - Fragment shader for Transvoxel mesh rendering (transpiled from `triangle_fragment_template.c`)

## Note

These files are preserved for reference and potential future use. The ray-marching approach was functional but has been superseded by the more efficient Transvoxel mesh rendering system.