# Simulation Model

This document explains how the simulation works from first principles: what a cellular automaton is, how the grid is stored, how cells are updated, and how the movement families and helper primitives fit together.

## Cellular automata

A falling sand game is a cellular automaton. The grid is divided into cells. Each frame, every cell is examined and may change its state based on a small set of local rules — only its immediate neighbors matter. Global behavior (flowing water, collapsing sand piles) emerges from these simple local rules without any central coordination.

This makes the simulation fast and easy to reason about: adding a new material means writing a new local rule, not redesigning the system.

## Grid storage

The grid is a flat `std::vector<Cell>` of size `GRID_WIDTH * GRID_HEIGHT`. A 2D coordinate maps to a 1D index via:

```cpp
int idx(int x, int y) const { return y * GRID_WIDTH + x; }
```

This keeps the data contiguous in memory (cache-friendly) and avoids a vector-of-vectors, which would scatter rows across the heap.

Current grid size: 300 × 200 = 60,000 cells × 8 bytes = ~480 KB.

## The Cell struct

```cpp
struct Cell {
    MaterialId material    = MAT_EMPTY;  // which material this cell is
    uint8_t    shade       = 128;        // per-particle brightness variation
    int8_t     temperature = 0;          // future: heat/cold systems
    uint8_t    life        = 0;          // countdown timers (fire, smoke, etc.)
    uint8_t    aux         = 0;          // material-specific state
    uint8_t    _pad[2]     = {};         // explicit padding to 8 bytes
};
```

`shade` is randomised from the material's `shadeMin`/`shadeMax` range when a cell is painted. It stays with the particle as it moves, giving each grain of sand or drop of water a slightly different brightness — without this, the simulation looks like a flat texture moving around.

`temperature`, `life`, and `aux` give materials room for stateful behavior without changing the cell layout. Fire and Smoke already use `life`, and Water/Steam/Fire now actively use `temperature` for heat propagation and phase changes.

## The update loop

Each frame, `Simulation::update()` starts with a heat pass, then runs the two movement passes.

### Heat pass

The heat pass runs across the full grid before movement. It does four things:

1. ambient cooling toward temperature `0`
2. heat emission into cardinal neighbors
3. conductive temperature equalization between neighboring cells
4. temperature-triggered `heatReactions`

Because this happens before movement, a Water cell can boil into Steam and then immediately use Gas movement in the same tick.

After the heat pass, `Simulation::update()` runs the movement passes:

### Pass 1 — bottom-to-top (Powder, Liquid, Static, Organic)

```
for y = GRID_HEIGHT-1 down to 0:
    for x = (alternating left-to-right / right-to-left):
        skip if m_updated[x,y]
        look up material's MovementModel
        call the appropriate family function
        run interaction rules if present
        call specialHook if present and cell is still active
```

Scanning bottom-to-top means a falling particle's destination is processed before its source. A column of sand settles in a single frame instead of cascading one cell per frame across multiple frames.

### Pass 2 — top-to-bottom (Gas)

Rising particles need the inverse scan direction. A second pass over the same grid (same `m_updated` array, already partially filled by Pass 1) handles Gas cells from top to bottom.

### Alternating horizontal scan

`m_scanLeft` flips each frame. This eliminates the left/right directional bias that would appear if the horizontal scan always ran the same way — without it, water visibly drifts in one direction on a flat surface.

### The updated flag

`m_updated` is a `std::vector<uint8_t>` (one byte per cell) reset to zero at the top of each frame. When a particle moves into a new cell, that cell is marked `1`. The scan loop skips marked cells, preventing a particle from being processed twice in the same frame and preventing a newly-landed particle from immediately moving again.

## Movement families

Each `MaterialDef` has a `MovementModel` that maps it to one of these families:

### Powder (Sand)

Priority order:
1. Try to move straight down into empty
2. Try to displace a lighter, displaceable material below (density-based)
3. Pick a random horizontal direction; try diagonal-down in that direction
4. Try diagonal-down in the other direction
5. Stay put

This produces natural pile-forming behavior and makes sand sink through water.

### Liquid (Water)

Priority order:
1. Try to move straight down into empty
2. Try diagonal-down (random direction preference)
3. Search laterally up to `spreadFactor` cells in each direction, preferring positions that have an empty cell below them (liquid flows toward drops, not just sideways)
4. Move toward the farther reachable target, breaking ties
5. Stay put

`spreadFactor` is stored in the `MaterialDef`, not as a global constant. Water uses 6. A thicker liquid like lava would use 1 or 2.

### Gas (Smoke, Steam)

Mirror of Liquid with inverted vertical direction — rises instead of falls, spreads laterally, uses `spreadFactor`. Smoke and Steam both reuse this family.

### Static (Stone)

No movement. The family function is a no-op. `specialHook` can still fire for things like heat conduction in the future.

### Organic (Fire)

Driven entirely by `specialHook` plus reusable interaction rules. No base movement family runs. Used for materials where the behavior cannot be described by a generic family (plants that grow into neighbors, fire that spreads and burns out).

## Interaction rules

After movement, the simulation evaluates the current material's `interactionRules`. Each rule checks local neighbors and can transform the current cell, the neighbor, or both.

This keeps contact reactions out of the hot-path control flow. The simulation does not contain code like "if fire next to oil"; it only knows how to evaluate a generic rule against neighbor cells.

## Heat reactions

Temperature-driven changes are handled by `heatReactions`, which are evaluated in the heat pass before movement.

Current examples:

- Oil ignites into Fire once it reaches its heat threshold
- Water boils into Steam
- Steam condenses back into Water when it cools

## UpdateContext helpers

All grid mutations go through four helper functions. This means a future double-buffer migration only requires changing these four functions, not hunting through the entire codebase.

### `tryMove(x, y, nx, ny)`

Move cell (x,y) into (nx,ny) if and only if (nx,ny) is in bounds, not already updated this frame, and is Empty. On success, the source becomes Empty and the destination is marked updated.

### `trySwap(x, y, nx, ny)`

Unconditionally swap two cells if (nx,ny) is in bounds and not already updated. Both cells are marked updated after the swap.

### `tryDisplaceByDensity(x, y, nx, ny)`

Swap (x,y) into (nx,ny) only when:
- (nx,ny) holds a material with the `Displaceable` trait
- (x,y)'s material has higher density than (nx,ny)'s material

This is the mechanism that makes sand sink through water. It's completely data-driven — no material names are mentioned. Any future material with `Displaceable` and a lower density than sand will automatically be sinkable.

### `spawnInto(x, y, mat)`

Replace the cell at (x,y) with a freshly initialised instance of `mat`, using the material's `spawnState` so lifetime-bearing materials get the right default values when painted or spawned.

### `spawnInto(x, y, CellSpawnDesc, markUpdated)`

Lower-level variant used by interaction rules and `specialHook` code. It can set explicit life/aux/temperature values and preserve shade while a timer counts down.

## Density-based displacement in detail

Sand has `density = 1.5`. Water has `density = 1.0` and `Trait::Displaceable`. When `updatePowder` tries to move sand down and finds water, it calls `tryDisplaceByDensity(sandX, sandY, waterX, waterY)`. The function checks:

```
water.traits & Trait::Displaceable  →  true
sand.density (1.5) > water.density (1.0)  →  true
```

So it calls `trySwap`, which swaps the two cells and marks both updated. Sand is now one row lower; water is one row higher. The displaced water is marked updated so it doesn't try to move again in the same frame.

This generalises automatically: oil has `density = 0.8` and `Displaceable`, so water sinks through it and sand sinks through both in the right order without any simulation code changes.
