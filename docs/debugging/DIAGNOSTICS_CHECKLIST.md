# Diagnostics Checklist - Planet Renderer Missing Faces Issue

## Problem Statement
**ENTIRE CUBE FACES ARE MISSING** from the rendered planet. Not small gaps, but complete faces.

## Visual Evidence (UPDATED WITH FACE MAPPING)
From screenshot and shader analysis:
- **GREEN faces (+Y/-Y, Face 2/3)**: ✓ VISIBLE and complete (34 and 19 patches)
- **BLUE faces (+Z/-Z, Face 4/5)**: ✓ VISIBLE but with HOLES (16 and 49 patches)
- **RED faces (+X/-X, Face 0/1)**: ✗ MOSTLY MISSING! (16 and 55 patches)
  - Appear pink/salmon due to rim lighting effect
  - Only tiny sliver visible on right edge despite having 71 total patches!
- The "dots" are the bottom green face bleeding through holes in blue face

## Key Contradiction
Render logs show: `[LODManager] Got 189 patches. Per face: 16 55 34 19 16 49`
- Face 0 (+X): 16 patches - RED - MISSING!
- Face 1 (-X): 55 patches - DARK RED - MISSING!
- Face 2 (+Y): 34 patches - GREEN - Visible
- Face 3 (-Y): 19 patches - DARK GREEN - Visible
- Face 4 (+Z): 16 patches - BLUE - Visible with holes
- Face 5 (-Z): 49 patches - DARK BLUE - Visible with holes

**71 patches (Face 0+1) are being generated but NOT rendered!**

---

## CHECKLIST OF INVESTIGATIONS

### ✅ Completed Investigations

#### 1. Transform Mathematics
- [x] **GlobalPatchGenerator MIN_RANGE bug** - FIXED
  - Issue: Patches were 0.0005% of expected size
  - Fix: Modified createTransform() to not apply MIN_RANGE incorrectly
  - Result: Test passes, vertices align, but faces still missing
  - Files: `include/core/global_patch_generator.hpp`

#### 2. Vertex Alignment Tests
- [x] **Same-face boundary alignment** - PASSES
  - Test: `test_same_face_boundaries.cpp`
  - Result: 0 meter gaps between adjacent patches on same face
  
- [x] **Cross-face boundary alignment** - FIXED TEST
  - Test: `test_face_boundary_alignment.cpp` (test 27)
  - Issue: Test was comparing perpendicular edges
  - Fix: Corrected UV parameterization
  - Result: Test now passes with 0 meter gaps

#### 3. Face Culling
- [x] **Disabled face culling** - NOT THE ISSUE
  - Setting: `enableFaceCulling = false` in `spherical_quadtree.hpp`
  - Result: Still missing faces even with culling disabled

#### 4. Shader Issues
- [x] **Camera-relative position handling** - FIXED
  - Issue: Altitude calculated incorrectly
  - Fix: Reconstruct world position in vertex shader
  - Files: `shaders/src/vertex/triangle.vert`

#### 5. Patch Generation Verification
- [x] **Patches reach face boundaries** - CONFIRMED
  - Test: `test_patch_boundaries.cpp`
  - Result: Root patches cover full faces, subdivisions reach edges

---

### ❌ Not Yet Investigated

#### System-Level Checks

- [ ] **Count root nodes actually created**
  - Where: `SphericalQuadtree::initializeRoots()`
  - Check: Are all 6 roots non-null?

- [ ] **Count patches per face**
  - Where: `SphericalQuadtree::update()`
  - Log: How many patches does each face generate?

- [ ] **Track patch collection**
  - Where: `SphericalQuadtreeNode::selectLOD()`
  - Check: Are patches from all faces added to visiblePatches?

- [ ] **Verify vertex generation**
  - Where: `CPUVertexGenerator::generatePatchMesh()`
  - Log: Is it called for all patches? How many vertices per face?

- [ ] **Monitor GPU upload**
  - Where: `LODManager::updateQuadtreeBuffers()`
  - Check: Buffer sizes, instance counts

- [ ] **Count draw calls**
  - Where: `vkCmdDrawIndexed()` calls
  - Expected: Multiple calls or instances covering all faces

#### Vulkan Pipeline State

- [ ] **Depth testing configuration**
  - Check: Depth test enabled? Clear value?
  - Could faces be behind others?

- [ ] **Viewport and scissor**
  - Check: Full screen or limited area?

- [ ] **Winding order and face culling**
  - Check: GPU-side face culling settings
  - Even if CPU culling disabled, GPU might cull

- [ ] **Instance data**
  - Check: Instance buffer contents
  - Are transforms correct for all faces?

#### Data Flow Verification

- [ ] **Single face isolation test**
  - Modify: Render ONLY face 0, then ONLY face 4
  - Question: Can each face render individually?

- [ ] **Simple quad test**
  - Bypass: All LOD systems
  - Create: 6 simple quads, one per face
  - Question: Do all 6 render?

- [ ] **RenderDoc capture**
  - Capture: One frame
  - Inspect: Draw calls, vertex data, pipeline state
  - Question: What does GPU actually receive?

---

### 🔄 Investigations That Didn't Help

#### Things We Thought Were Issues But Weren't

1. **6 million meter gaps** 
   - Spent lots of time on this
   - Was actually test error (perpendicular edges)
   - Real issue is missing faces, not gaps

2. **Transvoxel algorithm**
   - Created many tests for mesh generation
   - Not relevant - issue occurs in quadtree mode

3. **LOD transition zones**
   - Investigated altitude-based transitions
   - Not relevant - issue occurs at all altitudes

---

## DIAGNOSTIC COMMANDS TO RUN

### Quick Checks
```bash
# Run with vertex dump
./run.ps1 -vertex-dump -auto-terminate 5

# Check mesh output
cat mesh_debug_dump.txt | grep "Face"

# Count patches in vertex dump
grep "Patch" mesh_debug_dump.txt | wc -l
```

### Code Modifications for Logging

#### Add to SphericalQuadtree::update()
```cpp
std::cout << "=== QUADTREE UPDATE ===\n";
for (int i = 0; i < 6; i++) {
    std::cout << "Face " << i << ": ";
    if (!roots[i]) {
        std::cout << "NULL\n";
        continue;
    }
    int patchCount = 0;
    // ... after selectLOD
    std::cout << patchCount << " patches\n";
}
std::cout << "Total visible patches: " << visiblePatches.size() << "\n";
```

#### Add to LODManager::render()
```cpp
std::map<int, int> patchesPerFace;
for (const auto& patch : patches) {
    patchesPerFace[patch.faceId]++;
}
std::cout << "Patches per face: ";
for (int i = 0; i < 6; i++) {
    std::cout << "F" << i << ":" << patchesPerFace[i] << " ";
}
std::cout << "\n";
```

---

## THEORIES TO TEST

### Theory 1: Root Node Creation Failure
- Some root nodes might not be created
- Test: Log all 6 roots after creation

### Theory 2: Selective LOD Processing  
- selectLOD might skip some faces
- Test: Log entry/exit for each face

### Theory 3: Patch Collection Failure
- Patches generated but not collected
- Test: Count patches at each stage

### Theory 4: Vertex Generation Skip
- Generator not called for all patches
- Test: Log every generatePatchMesh call

### Theory 5: GPU State Issue
- Vulkan pipeline rejecting some geometry
- Test: RenderDoc capture

### Theory 6: Instance Buffer Limit
- Maximum instance count too low
- Test: Check buffer sizes and limits

---

## NEXT STEPS

1. **Add comprehensive logging** at each stage
2. **Run and collect data** for all 6 faces
3. **Find where faces disappear** in the pipeline
4. **Update this document** with findings

---

## FINDINGS LOG

### Test Execution Scripts Created
**Created diagnostic test runners:**
- `run_diagnostic_tests.ps1` - PowerShell version with detailed analysis
- `run_diagnostic_tests.bat` - Batch version for Windows CMD

**These scripts will:**
1. Run all priority tests in phases
2. Collect output in `diagnostic_results/` directory
3. Analyze patterns (missing faces, gaps, degenerate triangles)
4. Run main app with vertex dump
5. Generate summary report

**To run diagnostics:**
```bash
# PowerShell version (recommended):
./run_diagnostic_tests.ps1

# Or batch version:
run_diagnostic_tests.bat
```

### Priority Test Categories Identified
Based on analysis of 77 test files:

**Phase 1 - Problem Confirmation:**
- test_face_culling.cpp - Check if culling causes missing faces
- test_patch_coverage.cpp - Verify face coverage
- test_capture_real_data.cpp - Dump actual data
- test_check_coverage.cpp - Check missing areas

**Phase 2 - Pipeline Isolation:**
- test_pipeline_isolation.cpp - Find where faces disappear
- test_pipeline_diagnostic.cpp - Pipeline health check
- test_single_patch_render.cpp - Test individual patches
- test_systematic_verification.cpp - Full system check

**Phase 3 - Face-Specific:**
- test_xface_rendering.cpp - Cross-face rendering
- test_cross_face_fix.cpp - Cross-face issues
- test_face_boundary_solution.cpp - Boundary handling

**Phase 4 - Visual/Debug:**
- test_visual_output.cpp - Visual verification
- test_ascii_planet.cpp - ASCII visualization
- test_dot_diagnosis.cpp - Dot pattern analysis

### [Date] - Investigation Name
**What we tested:**
**Result:**
**Conclusion:**

(Add entries here as we investigate)

---

## PERMANENT NOTES

- The issue is NOT small gaps between patches
- The issue is NOT mathematical transform correctness  
- The issue IS entire faces missing from render
- Debug colors are intentional for visibility
- Face culling is disabled but faces still missing
- This suggests a logic error, not a math error