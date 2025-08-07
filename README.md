```
     .-..-. .-. .-..-. .-. .-..----..----.    .----..-..-..-.   .-..-..-.   .-.  .---. .----. .--.  .---. 
    ( '  _ \| | | || |\| | | || .--.`-' '-'    | .--'| || || |   | || || |   | | / .-. `--  --|    |/ .-. )
     | |(_) | |_| || | | | | || `--. / -. \    | `--..\ \/ / |   | || || |   | || | | |   | | | || || | | |
     | |  _ |  _  || | | | | || .-' ( (`-' )   | .--' )/  \| |   | || || |   | || | | |   | | | || || | | |
     | | ( )| | | || | | | | || `--. `.`-' /   | `--../ /\ \ |__ | || || `--.: || `-' |   | | `--' || `-' |
     `-' `-'`-' `-'`-' `-' `-'`----' `---''    `----'`-'  `-'`--'`-'`-'`----'`-' `---'    `-' .--. ( `---' 
                                                                                               (  ) | |      
    ·  ·  ˚  ★ 🌍 ·  · ✦  ·  · ⭐ · · ·  ·  ·  ·  ✦ ·  ·  · 🌙 ·  ·  ·  ★  ·  ·  ·  ·  ✦   `--' `-'      
```

# 🌍 Planet Simulator

**A scientifically-driven planet evolution simulator that models 4.5 billion years of geological processes in real-time**

> 🔬 *"We don't generate terrain - we evolve it."*

Watch as a molten protoplanet transforms through plate tectonics, mountain formation, erosion, and climate change - all simulated using hierarchical voxel rendering with C++17 and Vulkan.

## ✨ Features

### 🚀 Current Capabilities
- **Real-time planet rendering** with octree-based voxels (~40K nodes at 40+ FPS)
- **Material system** simulating rock, water, air, and magma
- **GPU-accelerated rendering** using Vulkan compute shaders
- **Adaptive detail levels** with frustum culling and LOD
- **Dear ImGui interface** for real-time monitoring and control
- **Screenshot system** with automatic file management

### 🔮 Coming Soon
- **GPU-native octree** for millions of voxels at 60+ FPS
- **Plate tectonics simulation** with emergent continents
- **Geological processes** including erosion, volcanism, and mountain building
- **Climate simulation** with atmosphere, weather, and ocean currents
- **4.5 billion year evolution** from molten ball to living world

## 🎯 Project Vision

This isn't just another terrain generator. This simulator starts with a young, molten planet and applies real physics over billions of simulated years. Mountains rise from tectonic collisions, oceans fill basins, continents drift apart - every feature emerges naturally from the underlying simulation.

```
    Timeline of a Planet's Life:
    
    0 years          →  🔴 Molten protoplanet
    100M years       →  🟤 Cooling crust forms
    500M years       →  🔵 First oceans appear
    1B years         →  🏔️ Continental plates emerge
    2B years         →  🌋 Active volcanism shapes land
    3B years         →  🏞️ Rivers carve valleys
    4B years         →  ❄️ Ice ages come and go
    4.5B years       →  🌍 Earth-like planet ready for life
```

## 🛠️ Technical Stack

- **Language**: Modern C++17
- **Graphics**: Vulkan 1.4+ with compute shaders
- **Math**: GLM (OpenGL Mathematics)
- **UI**: Dear ImGui for debug interface
- **Build**: CMake 3.20+
- **Platform**: Windows (Linux support planned)

## 📦 Prerequisites

- Visual Studio 2022 with C++ development tools
- [Vulkan SDK 1.4.321.1+](https://vulkan.lunarg.com/sdk/home)
- CMake 3.20+
- Git

## 🚀 Quick Start

```batch
# Clone the repository
git clone https://github.com/yourusername/planetsimulator.git
cd planetsimulator

# Build the project
build.bat

# Run with default settings (depth 7, optimized for 40+ FPS)
build\bin\Release\OctreePlanet.exe

# Or explore with higher detail (slower)
build\bin\Release\OctreePlanet.exe -max-depth 8

# Take screenshots during a 10-second run
build\bin\Release\OctreePlanet.exe -auto-terminate 10 -screenshot-interval 2
```

## 🎮 Controls

| Key/Mouse | Action |
|-----------|--------|
| W/A/S/D | Move camera |
| Mouse | Look around |
| Q/E | Move up/down |
| Shift | Move faster |
| ESC | Exit |

## 📊 Performance Targets

| Octree Depth | Voxel Count | Current FPS | Target FPS |
|--------------|-------------|-------------|------------|
| 7 | ~40K | 40-80 ✅ | 60+ |
| 8 | ~166K | 15 | 60+ |
| 9 | ~662K | 4 | 60+ |
| 10+ | 1M+ | <1 | 60+ |

*Next milestone: GPU-native octree for 1M+ voxels at 60 FPS*

## 🗺️ Roadmap Highlights

### Phase 3.6: GPU Octree (Currently in Development)
Moving from CPU instance rendering to GPU-native octree traversal

### Phase 4: Initial Planet State
Creating geologically young planets as evolution starting points

### Phase 5: Core Physics
Implementing plate tectonics, convection, and surface processes

### Phase 7: Atmosphere & Climate
Adding weather, ocean currents, and long-term climate cycles

### Phase 8: Life & Ecosystems (Stretch Goal)
The ultimate test - can life emerge on our simulated worlds?

## 🤝 Contributing

This is a research project exploring the intersection of computer graphics and geological simulation. Contributions are welcome! Whether you're interested in:

- 🎨 Graphics optimization
- 🌋 Geological accuracy
- 🔬 Physics simulation
- 🎮 User interface
- 📚 Documentation

Feel free to open an issue or submit a pull request!

## 📖 Documentation

- [ROADMAP.md](docs/ROADMAP.md) - Detailed development roadmap
- [CLAUDE.md](CLAUDE.md) - AI assistant development guide
- [VULKAN_RENDERER_ROADMAP.md](docs/VULKAN_RENDERER_ROADMAP.md) - Renderer architecture
- [PLANET_EVOLUTION_ARCHITECTURE.md](docs/PLANET_EVOLUTION_ARCHITECTURE.md) - Evolution system design

## 🏆 Acknowledgments

Built with inspiration from:
- Real planetary science and geology
- The voxel rendering community
- The Vulkan graphics ecosystem
- Everyone who's ever looked up at the stars and wondered "what if?"

---

*Remember: Every mountain, every ocean, every continent in this simulation earned its place through billions of years of simulated physics. No shortcuts, no cheats - just time, physics, and patience.*

```
     "The Earth does not belong to us; we belong to the Earth.
      All things are connected like the blood that unites one family."
                                                    - Chief Seattle
```
