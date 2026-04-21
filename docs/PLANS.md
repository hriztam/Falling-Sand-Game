# Falling Sand Game — Future Plans & Ideas

> Goal: evolve this sandbox engine into a Noita-like experience with a playable character, procedural world, and rich material interactions.

---

## Phase 1 — Material Richness (do this now)

All infrastructure already exists. These are pure `registerMaterial()` calls with minimal new code.

### 1.1 Generic Flammable ignition rule
- Fire should spread to **any** cell with the `Flammable` trait via an `interactionRule` on Fire, not hardcoded per-material.
- Right now Oil ignites only through the heat reaction path. Adding `Flammable` → catch-fire interaction makes the system composable.

### 1.2 New materials

| Material    | Model   | Key behavior |
|-------------|---------|--------------|
| **Lava**    | Liquid  | High `heatEmission`, forms Stone on Water contact, solidifies inward as it cools, destroys Flammable neighbors, boils Water to Steam |
| **Ice**     | Static  | Freezes adjacent Water cells when cold (`temperature` field is ready), melts to Water via heat reaction |
| **Wood**    | Static  | `Flammable` trait, burns slowly (long `life`), emits Smoke; enable tree/structure building |
| **Gunpowder** | Powder | `Flammable`, explodes (fast fire burst) when ignited — needs a multi-cell spawn helper |
| **Acid**    | Liquid  | `specialHook` that dissolves `SolidLike` neighbors via `spawnInto(neighbor, MAT_EMPTY)` with probability |
| **Dirt**    | Powder  | Lower density than Sand, absorbs Water (Water converts to Mud) |
| **Mud**     | Powder  | Heavier/stickier than dirt; hardens to Dirt/Stone when hot |
| **Stone**   | Static  | High-conductivity solid; base terrain material |
| **Metal**   | Static  | Very high `heatConductivity`, non-flammable, conducts electricity (future) |
| **TNT**     | Static  | Explodes in a radius when ignited — needs explosion helper |

### 1.3 Explosion helper
- Add a `Simulation::explode(x, y, radius, force)` method that clears cells in a radius and spawns Fire/shockwave.
- Required for Gunpowder and TNT, and later for spells/projectiles.

### 1.4 Use existing trait flags
- Wire up `Flammable` in Fire's interaction rules (see 1.1).
- Wire up `ConductsHeat` — currently `heatConductivity` float drives everything; the trait flag is defined but unused.
- Wire up `LiquidLike` / `GasLike` for generic interaction rules (e.g., gases rise through liquids).

---

## Phase 2 — Simulation Quality

### 2.1 Dirty-rect / chunk-based update (HIGH PRIORITY)
- Currently all 60,000 cells are scanned every frame, even settled sand.
- Divide the grid into 16×16 chunks; mark a chunk dirty when any cell in it moves or changes state.
- Only simulate dirty chunks. Chunk goes clean when nothing moved for N consecutive frames.
- **Expected speedup: 5–20×** for scenes with a lot of settled material.

### 2.2 Double-buffer update
- Current in-place writes mean a particle that moves early in a scan can be processed again later in the same frame.
- Add a second `Cell` grid as a write buffer; swap at end of frame.
- Eliminates directional bias artifacts and makes the simulation order-independent.

### 2.3 Fluid pressure / U-tube equalization
- Water currently doesn't rise in a sealed tube (no pressure model).
- Implement a simple column-height pressure pass: for each liquid cell, if the column to the left/right is shorter, push liquid sideways.
- This makes water fill containers realistically.

### 2.4 Gas pressure / diffusion
- Gases currently use the same rise-and-spread logic as liquids.
- Add a pressure value per cell; gases push into lower-pressure regions.
- Required for realistic smoke trapping, steam pressure, poison gas clouds.

### 2.5 Temperature realism
- `temperature` is `int8_t` [-128, 127]. Widen to `int16_t` or use a float shadow buffer for physics while keeping the cell struct small.
- Add radiation/convection: fire radiates heat across gaps (not just by conduction).

---

## Phase 3 — World & Camera

### 3.1 Chunked infinite world
- Replace the fixed 300×200 grid with a map of `Chunk` objects (each 64×64 cells).
- Load/unload chunks around the camera. Serialize inactive chunks to disk.
- This is the foundation for a procedurally generated world.

### 3.2 Camera / viewport
- Add a camera with pan and zoom.
- The renderer draws only the visible viewport from the chunk map.

### 3.3 Procedural world generation
- Use layered noise (FastNoise or similar) to generate terrain: dirt/stone layers, caves, water pockets, ore veins.
- Special biomes: lava caverns, ice caves, poison gas pockets.
- Noita-style: the world is one giant destructible pixel map.

### 3.4 Save / load
- Serialize active + cached chunks to disk (binary, RLE-compress repeated cells).
- Save camera position, player state, inventory.

---

## Phase 4 — Player & Gameplay

### 4.1 Player entity
- Add an `Entity` struct: position (float), velocity (float2), AABB, health.
- Player is NOT a cellular automaton cell — it's a separate physics body that queries the grid for collisions.
- Implement platformer movement: walk, jump, fall, land.
- Player can be set on fire, frozen, acidified by the simulation.

### 4.2 Basic interaction
- Player can dig (erase cells in a radius) and place materials (like the current brush).
- Health system: fall damage, fire damage, drowning.

### 4.3 Wand / projectile system
- Projectiles travel as rays (Bresenham line); at each step, interact with the cell they enter.
- Fire bolt → spawns Fire. Ice bolt → spawns Ice. Acid ball → spawns Acid.
- Wand definition: list of spell slots, cast delay, mana.
- This maps directly onto Noita's wand system.

### 4.4 Spell system
- Spells are data (like materials) — composable effects: damage, material spawn, explosion radius, multicast.
- Start simple: 3–4 spell types (Spark, Fireball, Freeze, Dig).

### 4.5 Enemies / AI
- Simple grid-aware enemies: path-find on solid terrain, avoid liquids/fire.
- Can be hurt by the simulation (fire, acid, lava) just like the player.

---

## Phase 5 — Polish & Systems

### 5.1 Lighting
- 2D ray-march lighting: point lights (fire, lava emit light), ambient, shadows through solid terrain.
- Render to a separate light buffer, multiply over the material color buffer.
- Fire and Lava should glow; caves should be dark.

### 5.2 Audio
- Spatial audio: fire crackle, water splash, explosion, material-specific sounds.
- Tie into the interaction/reaction system: play a sound when a reaction fires.

### 5.3 External material definitions
- Load materials from JSON/TOML files at startup instead of hardcoding in C++.
- Enables modding and faster iteration without recompiling.

### 5.4 Electricity / conductors
- Add a conductivity trait and a separate electrical propagation pass (like heat but discrete).
- Metal conducts, water conducts (and can be shocked), creates sparks at junctions.

### 5.5 Performance profiling & SIMD
- Profile per-system (heat pass, movement pass, render pass) with Tracy or `perf`.
- The pixel-buffer render loop and heat equalization are good SIMD candidates.

---

## Immediate Next Actions (Prioritized)

1. **Add generic Flammable ignition rule** — 30 min, pure data change in `material_registry.cpp`
2. **Add Wood, Lava, Ice** — 2–3 hours, tests the heat + interaction system end-to-end
3. **Add Gunpowder + explosion helper** — 2 hours, first "destructive" mechanic
4. **Dirty-rect chunk optimization** — half a day, unlocks bigger worlds without frame drops
5. **Add Acid** — 1 hour, first "dissolving" mechanic, feels very satisfying
6. **Chunked world + camera** — 1–2 days, needed before world gen makes sense
7. **Procedural world gen** — 1–2 days, makes the sandbox feel like a world
8. **Player entity + platformer movement** — 2–3 days, first step toward a real game

---

## Ideas Parking Lot

- **Vines / Algae**: `SupportsGrowth` trait exists; organic material that spreads along `SolidLike` surfaces
- **Poison gas**: Gas + periodic damage to entities in contact
- **Concrete**: Mix of Water + Stone powder; hardens over time to Static
- **Glass**: Transparent solid formed from Sand at high heat (from Lava contact)
- **Magnet**: Static material that attracts metal particles within a radius
- **Black hole**: Pulls all neighboring cells toward it and deletes them (novelty attractor)
- **Worm enemy**: Tunnels through terrain, leaves holes
- **Weather**: Rain (spawns Water from top), wind (horizontal force on Gas/Powder cells)
- **Portals**: Teleport cells from one location to another
- **Multiplayer** (very long-term): shared grid, each player controls a wand
