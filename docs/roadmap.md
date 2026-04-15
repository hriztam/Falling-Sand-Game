# Roadmap

A record of every milestone from the first commit to today, followed by what comes next.

---

## History

### v0.1 — Bootstrap
*Commits: `initial commit`, `initialised cmake and sfml`*

Set up CMake, linked SFML 3, opened a window. Nothing moves yet.

---

### v0.2 — First particles
*Commits: `you can draw sand with mouse now`, `sand now falls down in both straight and diagonal way`*

- First working grid: flat `std::vector<Cell>`, 1D index helper `y * GRID_WIDTH + x`
- Sand falls straight down; added diagonal sliding (down-left / down-right)
- Mouse painting with a fixed brush

---

### v0.3 — Bug fixes and polish
*Commits: `fixed the left bias issue and added a brush size`, `fixed some bugs`, `added eraser`*

- Alternating left/right horizontal scan to eliminate directional bias in sand piles
- Configurable brush radius
- Eraser tool (right-click paints Empty)

---

### v0.4 — Wall and materials
*Commit: `added 'Wall' material`*

- Added immovable `Wall` material
- Material selection via keyboard (1 Sand, 2 Wall)

---

### v0.5 — Water
*Commits: `added water but the flow is not proper`, `water logic fixed`*

- Water falls and tries to spread laterally
- Early version had flow artifacts; fixed with the `WATER_FLOW`-limited search that finds the furthest reachable cell while preferring positions with an empty cell below

---

### v0.6 — Sand/water interaction and refactor
*Commit: `Refractored the code into different files and added water/sand swapping`*

- Split the monolithic `main.cpp` into `simulation.h`, `simulation.cpp`, and `types.h`
- Sand sinks through water via `swapWithWater()` — first cross-material interaction
- Material 3 = Water added to keyboard controls

---

### v0.7 — Rendering and performance
*Commits: `added the second grid logic for better rendering`, `added texture to the materials and improved the logic`, `improved the performance of the code`*

- Moved to a per-cell shade system for visual depth
- Texture-based rendering experiments
- General simulation and rendering performance improvements

---

### v0.8 — Registry-based architecture (current)
*April 2026*

Major architectural overhaul implementing the design from `implementation-plan.md`:

**Simulation**
- `enum class Material` replaced with `using MaterialId = uint16_t`
- `MaterialDef` struct: id, name, movement model, trait flags, density, spreadFactor, shadeMin/Max, color, specialHook
- `MaterialRegistry` class: O(1) lookup by id, `buildDefaults()` factory
- `Cell` expanded: material, shade, temperature, life, aux (8 bytes, cache-friendly)
- `updateSand` / `updateWater` replaced by reusable families: `updatePowder`, `updateLiquid`, `updateGas`
- `moveCell` / `swapWithWater` replaced by `tryMove`, `trySwap`, `tryDisplaceByDensity`, `spawnInto`
- Density-based displacement is now fully data-driven — no hardcoded material pairs
- Two-pass update loop: bottom-to-top (Powder/Liquid/Static), top-to-bottom (Gas)
- `std::vector<uint8_t>` for the updated flag (avoids `vector<bool>` bitfield overhead)

**Renderer**
- Six-vertices-per-cell `VertexArray` (360,000 writes/frame) replaced with a CPU RGBA pixel buffer and a single `sf::Texture::update()` call (60,000 writes/frame)
- Shade modulation in renderer: `rendered = base_color * (shade / 128.0)`
- `ColorRGBA` is SFML-agnostic — only `main.cpp` knows about SFML

**App**
- Scroll-wheel brush resize added
- HUD material name sourced from registry (`matDef->name`) rather than a hardcoded string switch

---

### v0.9 — Oil
*April 2026*

- Added `Oil` as a built-in registry material
- `MovementModel::Liquid`, `density = 0.8`, `spreadFactor = 3`
- Water now sinks through oil automatically via liquid density displacement
- Keyboard material selection updated: `4` = Oil

---

## Planned features

### Near term

**Smoke / Steam**
- Gas material that rises
- Gas family already in place (`MovementModel::Gas`, top-down Pass 2)
- Steam spawned by Water+Fire interaction via `specialHook`

**Fire**
- `MovementModel::Organic`, driven by `specialHook`
- Spreads to `Flammable` neighbors, decrements `life` each frame, transforms to Smoke when `life` reaches 0
- Uses `Cell::life` and `Cell::temperature` fields already in the struct

**Cryo / Ice**
- Freezes Water neighbors below a temperature threshold
- Uses `Cell::temperature` for local heat state
- `ConductsHeat` trait enables heat propagation between neighbors

---

### Medium term

**Double-buffer update**
All grid writes currently happen in-place, which means a particle moved early in the scan can affect particles scanned later in the same frame. A double-buffer (read from grid A, write to grid B, swap at end of frame) eliminates this. The `tryMove`/`trySwap`/`tryDisplaceByDensity`/`spawnInto` helpers are the only functions that would need to change.

**Lava**
- Dense, slow-spreading liquid (`Liquid`, low `spreadFactor`, high `density`)
- Destroys `Flammable` cells on contact (via `specialHook`)
- Cools to `Wall` when adjacent to Water (temperature interaction)

**Plants / Organic growth**
- `Organic` movement model, fully hook-driven
- Grows into neighboring Empty or Water cells up to a configured rate
- `Trait::SupportsGrowth` marks cells that plants can colonise

**Acid**
- Liquid that calls `spawnInto(neighbor, MAT_EMPTY)` on contact with `SolidLike` or `Flammable` neighbors
- Destroys itself in the process (converts to Empty or Steam)

---

### Long term

**Pressure / fluid equalization**
Currently water finds its level by gravity and lateral flow but does not model pressure. True pressure equalization (water rising through a U-tube) requires a more sophisticated fluid model.

**Temperature field**
A per-cell temperature that propagates via `ConductsHeat` neighbors each frame. Fire heats its surroundings; ice cools them. Materials with ignition or freeze thresholds stored in `MaterialDef` react automatically.

**JSON/YAML material authoring**
Right now materials are defined in C++ (`buildDefaults()`). A future phase could load material definitions from a data file at startup, enabling modding without recompiling.

**SDL3 / OpenGL backend**
The simulation layer has zero SFML dependency. Swapping the backend means rewriting only `main.cpp`. SDL3 is the preferred future target if SFML becomes a limitation. OpenGL would only make sense as a renderer implementation detail if GPU-side particle simulation becomes worthwhile.

**Performance: resting optimization**
Cells that have not moved for N frames could be put to sleep and skipped in the update loop. This is a common optimization in mature falling sand engines and would significantly reduce CPU load when the grid is mostly settled.
