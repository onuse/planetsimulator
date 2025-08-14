# Test Organization Structure

## Directory Layout

```
tests/
├── phase1_vertex_id/     # Phase 1: Vertex Identity System
│   ├── test_vertex_identity
│   ├── test_canonical_boundaries
│   ├── test_vertex_id_integration
│   └── vertex_id_test.obj
│
├── proof_of_concept/     # Proof of concept implementations
│   └── proof_of_concept.cpp
│
├── investigation/        # Diagnostic and investigation tests
│   ├── Face boundary tests
│   ├── Patch alignment tests
│   ├── Transform tests
│   └── UV mapping tests
│
└── [existing tests]/     # Original test suite
    ├── test_octree_core.cpp
    ├── test_transvoxel_algorithm.cpp
    └── ...
```

## Cleanup Summary

### Moved to `tests/phase1_vertex_id/`
- All vertex identity system tests and outputs

### Moved to `tests/investigation/`
- Diagnostic tests created during face boundary investigation
- Patch alignment and gap tests
- Transform and UV mapping tests

### Moved to `tests/proof_of_concept/`
- Proof of concept implementation showing vertex sharing works

### Moved to `docs/`
- FACE_BOUNDARY_FIX.md
- PATCH_DISCONTINUITY_FIX_SUMMARY.md
- PATCH_FIX_ANALYSIS.md
- PIPELINE_ANALYSIS.md
- TERRAIN_INVESTIGATION.md

### Moved to `debug_data/`
- patch_boundaries.obj
- patch_data.txt
- zoom_diagnostics.csv

### Moved to `logs/`
- debug_output.txt
- debug_output2.txt
- output.log
- output.txt

### Removed
- Orphaned test executables without source files
- Duplicate compiled binaries
- Object files (.o)