# Planet Simulator

A planet whose surface is simulated rather than generated. Plate tectonics runs
continuously — continents drift, collide, weld into supercontinents and rift
apart again — and the terrain you see is the output of that, not of a noise
function tuned to look plausible.

## What is actually simulated

**Plate motion is solved, not prescribed.** A plate floating on the
asthenosphere has a Reynolds number around 10⁻²⁰, so its inertia is
meaningless: at every instant the driving torques and viscous drag balance
exactly. That reduces to a 3×3 solve per plate, `ω = D⁻¹T`, where the driving
forces are the ones that actually move plates:

- **slab pull**, from the negative buoyancy of cold lithosphere, growing as
  √age because the thermal boundary layer thickens that way
- **ridge push**, the gravitational sliding of the cooling column away from a
  spreading centre
- **basal drag**, stronger under continental keels, which is why
  continent-heavy plates are the slow ones
- **collision resistance** where neither side of a boundary can subduct

**Elevation is an output of isostasy.** Crustal columns float on the mantle, so
elevation is `thickness × (1 − ρ_crust / ρ_mantle)`. Continental crust at 35 km
and 2750 kg/m³ stands 5833 m above the compensation datum; oceanic crust at
7 km and 2950 stands 742 m. The 5.1 km step between them is not a tuned
parameter — it falls out of measured densities and matches the real
continent-to-abyssal-plain difference. Seafloor subsides as √age from
half-space cooling, and sea level is solved from a conserved water volume, so
growing continents displace water and raise it.

**Crust is carried on Lagrangian markers.** Each parcel rotates exactly with
its plate, so transport introduces no error at any timestep. Rotate a
single-plate planet once around — a pure coordinate change — and 97% of the
crustal contrast survives; the field-based transport it replaced destroyed 57%.
Each parcel carries a stack of rock layers with types and ages, so the planet
remembers its history: erosion strips from the top, delamination founders the
bottom, and a subducting slab is consumed entire.

**Nothing appears or disappears.** Crust is created from mantle melt at ridges
and arcs and returned by subduction, and every transfer is booked. Crust plus
mantle reservoir balances the starting volume to about 1 part in 10¹³.

Numbers that come out of this rather than being asserted: mean plate speed
~6 cm/yr on an Earth-sized body, slab pull exceeding ridge push by ~20×, land
covering ~29% of the surface, mean seafloor age ~59 My.

## Building

Needs Visual Studio 2022 Build Tools, CMake, the Vulkan SDK and Python (the
shader templates are transpiled by a small script).

```powershell
.\rebuild_windows.bat
```

GLFW 3.3.8 declares a `cmake_minimum_required` that CMake 4 rejects, so
configuring by hand needs the compatibility flag:

```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
& "C:\Program Files\CMake\bin\cmake.exe" -S . -B build_windows -G "Visual Studio 17 2022" -A x64 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
& "C:\Program Files\CMake\bin\cmake.exe" --build build_windows --config Release --parallel
```

## Running

```powershell
.\run.ps1
```

| flag | |
|---|---|
| `-radius <m>` | planet radius, default 1 000 000 |
| `-seed <n>` | a different planet |
| `-auto-terminate <s>` | exit after N seconds |
| `-screenshot-interval <s>` | write PNGs to `screenshot_dev\` |

Left-drag orbits, scroll zooms, WASD/QE moves, TAB switches orbital/free-fly,
R resets, P screenshots, ESC quits.

Look for pale bands in deep ocean — those are spreading ridges, where crust is
too young to have subsided. White belts across continents are collisional
orogens. Give it a minute; the surface visibly reshapes.

A note on scale: plate speed in metres per year barely depends on planet size,
so the default 1000 km world crosses its own circumference far faster than
Earth does and its tectonics run correspondingly fast. `-radius 6371000` gives
a more sedate view.

## Layout

```
include/simulation/  crust_grid.hpp     plates, markers, isostasy
include/core/        density_field.hpp  crust + sub-grid noise -> a signed distance field
                     octree.hpp         voxels, derived from the field
src/rendering/       Vulkan; one pipeline, which draws the planet
tests/               nine test binaries, all linking one shared library
```

The simulation runs on its own thread and publishes immutable snapshots of the
surface; the renderer holds whichever is newest and never blocks on it.

## Tests

```powershell
.\build_windows\bin\Release\test_crust_grid.exe
```

The tectonics tests check mechanism rather than appearance — that isostasy
predicts measured elevations, that the silicate budget balances, that plates
reorganise, that transport does not smear the planet.

## Known limits

- The mesh is rebuilt on the CPU in full whenever the surface changes. It
  cannot do level of detail and will not survive descending to the surface;
  it needs replacing with GPU-side displacement or chunked streaming.
- There is no erosion yet, so delamination is the only thing limiting orogens
  and continental crust slowly drains to the mantle.
- Surface colouring is elevation bands with a latitude-dependent snow line.
  It should come from a climate model.
- Voxels are generated once from the initial state and do not resync as
  tectonics runs.
