# Falling Sand Game

An experimental falling-sand particle simulation inspired by _Noita_, written in C++17 with SFML 3.

Each grid cell is a particle obeying simple local rules — gravity, density, and fluid flow — and complex emergent behavior falls out naturally. The project started as a way to go deeper into simulation, performance, and systems programming, and has grown into a properly architected sandbox engine.

## Features

- Sand — falls, piles, sinks through water
- Water — flows around obstacles, finds its level
- Wall — immovable solid
- Density-based displacement (sand sinks through water automatically by density comparison, no special-casing)
- Shade variation per particle for visual depth
- Live HUD (material, FPS, brush size)
- Scroll-wheel brush resize

## Build

**Requirements**

- CMake 3.10+
- C++17 compiler (clang++ or g++)
- SFML 3 — `brew install sfml` on macOS

```bash
git clone https://github.com/hriztam/Falling-Sand-Game
cd Falling-Sand-Game
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/app
```

## Controls

| Input | Action |
|---|---|
| Left mouse | Paint selected material |
| Right mouse | Erase |
| Scroll wheel | Resize brush |
| `1` | Sand |
| `2` | Wall |
| `3` | Water |
| `0` | Eraser |
| `C` | Clear grid |

## Project structure

```
src/
  types.h               — MaterialId, Cell, MovementModel, Trait flags, ColorRGBA
  material_registry.h   — MaterialDef struct, MaterialRegistry class
  material_registry.cpp — Registry implementation + built-in material definitions
  simulation.h          — Simulation class, World view struct
  simulation.cpp        — Update loop, movement families, helper primitives
  main.cpp              — Window, input, pixel-buffer renderer, HUD

docs/
  architecture.md       — File layout, dependency graph, design decisions
  simulation-model.md   — Cellular automata, update loop, movement families
  materials.md          — MaterialDef, registry, how to add new materials
  roadmap.md            — Development history and planned features
```

## How the architecture works

The simulation is driven by a **MaterialRegistry** — a compact table of `MaterialDef` entries, each describing one material's movement model, density, spread rate, color, and optional behavior hook. The renderer and simulation loop never contain `switch` statements over material types, so adding a new material is a single registry entry with no changes elsewhere.

See [docs/architecture.md](docs/architecture.md) for the full design breakdown.

## Background

Most of my recent work has been in web development and AI. This project exists to force a different kind of thinking: simulation, memory layout, cache-friendly data, and emergent behavior from simple rules. Earlier projects in this space: [Ray Tracer in C++](https://github.com/hriztam/Ray-Tracer).

## Documentation

| Doc | What it covers |
|---|---|
| [docs/architecture.md](docs/architecture.md) | File structure, dependency graph, design decisions |
| [docs/simulation-model.md](docs/simulation-model.md) | Cellular automata, update loop, movement families, helper primitives |
| [docs/materials.md](docs/materials.md) | MaterialDef fields, built-in materials, how to add new ones |
| [docs/roadmap.md](docs/roadmap.md) | Full development history and planned features |
