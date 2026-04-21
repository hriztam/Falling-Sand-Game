# Architecture

This document describes the current file layout, the dependency graph between modules, and the key design decisions that shape how the codebase is structured.

## File layout

```
src/
  types.h               — shared constants, POD types, no includes beyond <cstdint>
  material_registry.h   — MaterialDef, MaterialRegistry (includes types.h)
  material_registry.cpp — registry implementation and built-in material definitions
  simulation.h          — Simulation class, World view struct (includes material_registry.h)
  simulation.cpp        — update loop, movement families, helper primitives
  main.cpp              — window, input handling, pixel-buffer renderer, HUD
```

## Dependency graph

```
types.h
  └── material_registry.h
        └── material_registry.cpp
        └── simulation.h
              └── simulation.cpp
              └── main.cpp  (also includes SFML headers)
```

`types.h` has no project includes, only `<cstdint>`. The simulation layer has no SFML dependency. SFML is isolated entirely to `main.cpp`.

## Two-layer design

### Simulation layer
`simulation.h`, `simulation.cpp`, `material_registry.h`, `material_registry.cpp`, `types.h`

Knows nothing about SFML, windows, or rendering. Owns the grid, drives particle physics, and exposes a read-only `World` view to whoever wants to render it.

### App layer
`main.cpp`

Owns the SFML window, reads input, calls `sim.update()` each frame, converts the `World` view into a pixel buffer, and uploads it to a GPU texture. No simulation logic lives here.

This split means you can replace SFML with SDL3 or a raw OpenGL context by rewriting only `main.cpp`.

## Key design decisions

### MaterialRegistry instead of a material enum

The old codebase had `enum class Material { Empty, Sand, Water, Stone }` and `switch (material)` in both the renderer and the simulation. Adding a material like Oil required changes in at least three places.

The registry replaces that with a table of `MaterialDef` entries built at startup. The simulation looks up the movement model and traits from the registry at runtime. The renderer looks up the base color. Neither contains any material-specific branching — they loop over the registry or index into it by `MaterialId`.

Adding a new material is one `registerMaterial()` call in `material_registry.cpp:buildDefaults()`.

The same idea now extends to reactions as well: `MaterialDef` can carry `InteractionRule` entries that describe neighbor-driven effects. The matcher is generic — exact material id, trait flags, or both — so the simulation still does not know about material pairs like "fire + oil".

### Cell is plain data, not a polymorphic object

`Cell` is an 8-byte POD struct: material id, shade, temperature, life, aux, padding. The full 300×200 grid fits in ~480 KB, well within L2 cache. There are no virtual calls, no heap allocation per particle, and no pointer chasing.

Material behavior is described by `MaterialDef` (data) and implemented by reusable family functions (`updatePowder`, `updateLiquid`, `updateGas`). Neighbor reactions are handled by generic `InteractionRule` data. An optional `specialHook` in `MaterialDef` is reserved for behavior that is truly per-tick and stateful, such as fire burnout or smoke dissipation.

### Pixel buffer instead of a vertex array

The old renderer built a `sf::VertexArray` with 6 vertices per cell — 360,000 vertex structs written every frame for a 300×200 grid. The current renderer writes one `ColorRGBA` (4 bytes) per cell into a CPU buffer and uploads it with a single `sf::Texture::update()` call. That is 60,000 writes instead of 360,000, and a single GPU upload instead of one draw call per batch.

The pixel buffer is also backend-agnostic: `ColorRGBA` is a plain struct with no SFML dependency, so switching to SDL3 or OpenGL requires only changing the upload call. The renderer now applies material-aware shading as well: powders keep stronger per-particle variation, while liquids use calmer fill shading plus a subtle surface highlight.

### `std::vector<uint8_t>` for the updated flag

`std::vector<bool>` in C++ is a bitfield specialization — reads and writes are not cache-friendly under random access. The `m_updated` array uses `std::vector<uint8_t>` instead, which gives simple byte-level access at the cost of 8× more memory (60 KB for a 300×200 grid, negligible).

### Two-pass update loop

Falling particles (Powder, Liquid) are naturally updated bottom-to-top so a column of sand settles in one frame rather than cascading one cell per frame. Rising particles (Gas) need the inverse direction. Rather than one pass with complicated direction logic, there are two passes:

1. Bottom-to-top — Powder, Liquid, Static, Organic
2. Top-to-bottom — Gas

Both passes share the same `m_updated` bitfield, which is cleared once at the start of the frame.

Within each pass, a cell goes through:

1. Movement family (unless it is `Organic`)
2. Reusable interaction rules
3. Optional `specialHook`

### UpdateContext helpers are public

`tryMove`, `trySwap`, `tryDisplaceByDensity`, `spawnInto`, and `applyInteractionRules` are public methods on `Simulation`. This is intentional — `specialHook` lambdas stored in `MaterialDef` need to call them. The alternative (a separate context struct, or friend declarations) adds indirection for no gain at the current scale.
