# Build System Documentation

## Overview
The project now uses separate build directories for Windows and WSL/Linux to avoid CMake cache conflicts.

## Directory Structure
- `build_windows/` - Windows builds (Visual Studio)
- `build_wsl/` - WSL/Linux builds (GCC/Make)
- `build/` - Legacy directory (deprecated, can be deleted)

## Windows Build Scripts

### Primary Scripts (Recommended)
- `rebuild_windows.bat` - Full rebuild with shader compilation and tests
- `run_windows.bat` - Run the Windows executable
- `build_windows.bat` - Quick build without shader recompilation

### General Scripts (Redirect to Windows)
- `rebuild.bat` → calls `rebuild_windows.bat`
- `run.bat` → calls `run_windows.bat`
- `build.bat` → calls `rebuild_windows.bat`
- `run.ps1` → runs Windows executable directly

## WSL/Linux Build Scripts
- `rebuild_wsl.sh` - Full rebuild for Linux
- `run_wsl.sh` - Run the Linux executable

## Usage Examples

### Windows (Command Prompt or PowerShell)
```cmd
# Full rebuild
rebuild.bat

# Run with parameters
run.bat -radius 1000 -auto-terminate 5

# Clean rebuild
rebuild_windows.bat clean
```

### WSL/Linux
```bash
# Full rebuild
./rebuild_wsl.sh

# Run with parameters
./run_wsl.sh -radius 1000 -auto-terminate 5

# Clean rebuild
./rebuild_wsl.sh clean
```

## Notes
- Windows and WSL builds are completely independent
- Each platform has its own CMake cache and build artifacts
- Shader compilation is handled automatically by the rebuild scripts
- The general scripts (rebuild.bat, run.bat, etc.) default to Windows for convenience