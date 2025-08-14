# Phase 1: Vertex Identity System Tests

This directory contains all tests related to the Phase 1 implementation of the vertex identity system.

## Files

- `test_vertex_identity` - Core unit tests for VertexID class
- `test_canonical_boundaries` - Tests for canonical ID resolution at face boundaries  
- `test_vertex_id_integration` - Integration test with mesh generation
- `vertex_id_test.obj` - Example output showing vertex sharing

## Building

```bash
# From project root
g++ -o tests/phase1_vertex_id/test_vertex_identity \
    tests/test_vertex_identity.cpp \
    src/core/vertex_id_system.cpp \
    -I./include -I./external/glm -std=c++17

# Run tests
./tests/phase1_vertex_id/test_vertex_identity
```

## Results

All tests passing ✓
- Vertices at same 3D position get same ID
- Corner vertices shared by 3 faces
- Edge vertices shared by 2 faces
- 7.4% memory savings demonstrated with just 7 patches