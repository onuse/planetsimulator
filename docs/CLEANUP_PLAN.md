# Project Cleanup Plan

## Current Root Files Analysis (37 files)

### ✅ KEEP IN ROOT (Essential)
- `.gitignore` - Git configuration
- `CMakeLists.txt` - Build configuration  
- `CLAUDE.md` - AI assistant instructions
- `README.md` - (should be created if missing)

### 📁 MOVE TO `docs/` (Documentation)
- `DIAGNOSTICS_CHECKLIST.md` - Debugging documentation
- `DOTS_THEORY.md` - Technical analysis
- `FINAL_DIAGNOSIS.md` - Problem resolution docs
- `FIX_DEPTH_PRECISION.md` - Technical solution docs
- `FIX_SUMMARY.md` - Fix documentation
- `RENDERING_PIPELINE.md` - Architecture docs
- `SOLUTION_SUMMARY.md` - Solution documentation
- `SYSTEM_INTERACTION_MAP.md` - System architecture
- `TROUBLESHOOTING_LOG.md` - Debug history
- `debug_render_tracking.md` - Debug documentation
- `diagnose_rendering_issue.md` - Debug documentation

### 📁 MOVE TO `scripts/` (Build & Run Scripts)
- `build.bat` - Windows build script
- `build_simple.bat` - Simple build script
- `rebuild.bat` - Full rebuild script
- `run.bat` - Windows run script
- `run.ps1` - PowerShell run script
- `run_diagnostic_tests.bat` - Test runner
- `run_diagnostic_tests.ps1` - Test runner
- `run_existing_tests.bat` - Test runner
- `apply_vertex_fix.sh` - Fix script (might be obsolete?)

### 📁 MOVE TO `tools/` or `scripts/analysis/` (Analysis Tools)
- `analyze_mesh.py` - Mesh analysis tool
- `visualize_fix.py` - Visualization tool

### 📁 MOVE TO `debug_output/` (Debug Output Files)
- `complete_planet.obj` - Debug mesh output
- `gap_elimination_test.obj` - Debug mesh
- `isolation_test.obj` - Debug mesh
- `renderer_integration.obj` - Debug mesh
- `planet.png` - Screenshot
- `worse_farther.png` - Screenshot
- `planet_ascii.txt` - Debug visualization
- `planet_ascii_small.txt` - Debug visualization
- `planet_patches.txt` - Debug output

### ❌ DELETE (Temporary/Generated)
- `imgui.ini` - ImGui settings (regenerated)
- `nul` - Accidental file (duplicate)
- `nul\r` - Accidental file with carriage return

## Proposed Directory Structure
```
planetsimulator/
├── build/                 # Build output (already exists)
├── docs/                  # All documentation
│   ├── architecture/      # System design docs
│   ├── debugging/         # Debug guides
│   └── fixes/            # Fix documentation
├── include/              # Headers (already exists)
├── scripts/              # Build and utility scripts
│   ├── analysis/         # Analysis tools
│   └── testing/          # Test runners
├── shaders/              # Shaders (already exists)
├── src/                  # Source code (already exists)
├── tests/                # Test files (already organized)
├── tools/                # Development tools
├── debug_output/         # Debug files (gitignored)
├── .gitignore
├── CMakeLists.txt
├── CLAUDE.md
└── README.md

```

## Cleanup Commands

### 1. Create directories
```bash
mkdir -p docs/architecture docs/debugging docs/fixes
mkdir -p scripts/analysis scripts/testing  
mkdir -p tools
mkdir -p debug_output
```

### 2. Move documentation
```bash
mv DIAGNOSTICS_CHECKLIST.md DOTS_THEORY.md FINAL_DIAGNOSIS.md docs/debugging/
mv FIX_DEPTH_PRECISION.md FIX_SUMMARY.md docs/fixes/
mv RENDERING_PIPELINE.md SYSTEM_INTERACTION_MAP.md docs/architecture/
mv SOLUTION_SUMMARY.md TROUBLESHOOTING_LOG.md debug_render_tracking.md diagnose_rendering_issue.md docs/debugging/
```

### 3. Move scripts
```bash
mv run_diagnostic_tests.* run_existing_tests.bat scripts/testing/
mv analyze_mesh.py visualize_fix.py scripts/analysis/
mv apply_vertex_fix.sh scripts/
```

### 4. Move debug output
```bash
mv *.obj *.png planet_ascii*.txt planet_patches.txt debug_output/
```

### 5. Delete unwanted files
```bash
rm nul*
rm imgui.ini
```

### 6. Update .gitignore
Add:
```
debug_output/
imgui.ini
*.obj
```

## Files Needing Review
- `apply_vertex_fix.sh` - Check if still needed
- `visualize_fix.py` - Check if still relevant
- Old test files in root that might have been missed