@echo off
echo Cleaning up unused shader templates...

REM Delete unused mesh_generator templates (keeping only simple_sphere)
del shaders\src\templates\mesh_generator_compute_altitude.c
del shaders\src\templates\mesh_generator_compute_analyze.c
del shaders\src\templates\mesh_generator_compute_template.c
del shaders\src\templates\mesh_generator_compute_template_debug.c
del shaders\src\templates\mesh_generator_compute_template_fixed.c
del shaders\src\templates\mesh_generator_compute_template_octree.c
del shaders\src\templates\mesh_generator_compute_template_old.c

echo Unused shader templates removed.
echo Keeping: mesh_generator_compute_simple_sphere.c (active template)