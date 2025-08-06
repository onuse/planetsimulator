# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

### Performance Considerations

- Target: 60+ FPS with full planet visualization
- Use GPU for all heavy computation
- Minimize CPU-GPU data transfers
- Profile with Vulkan debugging tools

### Doing tasks
The user will primarily request you perform software engineering tasks. This includes solving bugs, adding new functionality, refactoring code, explaining code, and more. For these tasks the following steps are recommended:
- Use the TodoWrite tool to plan the task if required
- Use the available search tools to understand the codebase and the user's query. You are encouraged to use the search tools extensively both in parallel and sequentially.
- Implement the solution using all tools available to you

## Important Notes

- The project has fully transitioned to Vulkan, removing OpenGL support
- Windows builds require CGO_ENABLED=1 and MinGW-w64 (use build.bat)
- GPU acceleration: Currently CPU physics, Vulkan compute coming soon
- Elevation view is the default visualization mode
- Screenshots: P key for manual, `-screenshot-interval` for auto-capture
- Development screenshots go to `screenshot_dev/` and are cleaned on startup
- No automated tests currently - manual testing required

## Vulkan Debugging

### Prerequisites
- Install Vulkan SDK: https://vulkan.lunarg.com/sdk/home
- Default installation path: `C:\VulkanSDK\<version>`

### Enable Validation Layers

#### Method 1: Environment Variables (Windows/WSL)
```bash
# Bash/WSL
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LAYER_PATH="C:\VulkanSDK\1.4.321.1\Bin"
./octree_planet.exe 2>&1 | grep -E "ERROR|WARNING|validation"

# PowerShell
$env:VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"
$env:VK_LAYER_PATH="C:\VulkanSDK\1.4.321.1\Bin"
.\octree_planet.exe 2>&1 | Select-String "ERROR|WARNING|validation"

# CMD
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
set VK_LAYER_PATH=C:\VulkanSDK\1.4.321.1\Bin
octree_planet.exe 2>&1 | findstr "ERROR WARNING validation"
```

#### Method 2: vkconfig GUI
1. Run `C:\VulkanSDK\<version>\Bin\vkconfig.exe`
2. Select "Validation" or "Synchronization" configuration
3. Click "Apply"
4. Run application normally

### Common Validation Errors

#### Descriptor Set Issues
- `VUID-VkWriteDescriptorSet-descriptorType`: Descriptor type mismatch
- `VUID-VkGraphicsPipelineCreateInfo-layout`: Wrong shader stage flags
  - Fix: Ensure descriptor layouts use correct `VK_SHADER_STAGE_*` flags

#### Pipeline State Issues
- `VUID-VkGraphicsPipelineCreateInfo-renderPass`: Missing depth/stencil state
  - Fix: Add `pDepthStencilState` when using depth attachment
- `VUID-RuntimeSpirv-OpEntryPoint`: Shader input/output mismatch
  - Fix: Ensure vertex outputs match fragment inputs (locations & types)

#### Memory & Synchronization
- `VUID-VkSubmitInfo-pWaitDstStageMask`: Pipeline stage mismatch
- Buffer/Image layout transitions