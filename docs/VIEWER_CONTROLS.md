# Planet Viewer Controls

## Camera Controls

### Movement
- **W/S** - Move forward/backward
- **A/D** - Move left/right  
- **Q/E** - Move up/down
- **Shift** - Hold for 10x speed boost

### View Control
- **Mouse Drag** - Rotate camera view
- **Mouse Scroll** - Zoom in/out
- **R** - Reset camera to default view (2.5x planet radius)

### Display Options
- **1** - Material view (default) - Shows rock (beige/brown), water (blue), magma (orange)
- **2** - Temperature view
- **3** - Elevation view  
- **4** - Density view
- **5** - Wireframe toggle
- **H** - Toggle hierarchical octree (with frustum culling)
- **G** - Toggle GPU octree ray marching
- **V** - Toggle VSync

### Other Controls
- **ESC** - Exit application
- **P** - Pause simulation (if physics enabled)
- **Tab** - Toggle UI panels

## Troubleshooting Visual Issues

### If planet appears all beige/brown:
1. **You're too close** - Press R to reset camera or scroll out
2. **You're inside the planet** - The core/mantle is all rock (beige)
3. **Looking at continental side** - Rotate around to see oceans

### Optimal viewing:
- Camera should be at 2-3x planet radius for best view
- You should see a mix of:
  - **Brown/beige** continents (rock)
  - **Blue** oceans (water)
  - **Orange/red** volcanic regions (magma) if present

### Performance:
- Enable GPU octree (G key) for better performance
- Enable hierarchical octree (H key) for frustum culling
- Reduce window size if framerate is low
- Use lower octree depth (--depth 6) for faster rendering