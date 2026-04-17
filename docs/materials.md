# Materials

This document describes the material system: what a `MaterialDef` contains, how the `MaterialRegistry` works, what the built-in materials are, and how to add a new one.

## MaterialId

```cpp
using MaterialId = uint16_t;
```

Every material is identified by a `uint16_t` integer. The built-in IDs are reserved as constants:

```cpp
constexpr MaterialId MAT_EMPTY = 0;
constexpr MaterialId MAT_SAND  = 1;
constexpr MaterialId MAT_WATER = 2;
constexpr MaterialId MAT_WALL  = 3;
constexpr MaterialId MAT_OIL   = 4;
constexpr MaterialId MAT_SMOKE = 5;
constexpr MaterialId MAT_FIRE  = 6;
constexpr MaterialId MAT_STEAM = 7;
```

Custom materials use IDs starting at 8. There is room for up to 65,535 distinct materials.

## MaterialDef

`MaterialDef` is a plain struct stored by value in the registry. One entry describes every particle of a given kind — it is shared data, not a per-particle object.

```cpp
struct MaterialDef {
    MaterialId    id;
    std::string   name;
    MovementModel movementModel;
    uint16_t      traits;        // bitfield of Trait:: constants
    float         density;       // for displacement; higher = sinks through lower
    int           spreadFactor;  // lateral spread limit for Liquid/Gas families
    uint8_t       shadeMin;      // inclusive lower bound for per-particle shade
    uint8_t       shadeMax;      // inclusive upper bound
    ColorRGBA     color;         // base RGBA color modulated by shade at render time
    uint8_t       coolingRate;
    uint8_t       heatEmission;
    uint8_t       heatConductivity;
    CellSpawnDesc spawnState;    // default state for fresh particles
    std::vector<HeatReaction> heatReactions;
    std::vector<InteractionRule> interactionRules;

    // Optional hook called after movement and interaction rules.
    // Null means the family handles everything.
    std::function<void(Simulation&, int, int)> specialHook;
};
```

### MovementModel

Controls which update family handles the material:

| Value | Behavior |
|---|---|
| `Empty` | Never processed |
| `Static` | Never moves; specialHook may still fire |
| `Powder` | Falls, slides diagonally, displaces by density |
| `Liquid` | Falls, spreads laterally up to `spreadFactor` cells |
| `Gas` | Rises, spreads laterally up to `spreadFactor` cells |
| `Organic` | Driven entirely by `specialHook` |

### Trait flags

Traits are bitflags combined with `|`. They are checked by the simulation and helper functions:

```cpp
namespace Trait {
    Movable           // can be moved at all
    AffectedByGravity // pulled downward
    Displaceable      // can be pushed aside by a denser material
    Flammable         // future: can be ignited
    LiquidLike        // future: interacts with liquid systems
    GasLike           // future: interacts with gas systems
    SolidLike         // future: structural material
    SupportsGrowth    // future: organic growth target
    ConductsHeat      // future: temperature propagation
}
```

### density

A dimensionless float used exclusively by `tryDisplaceByDensity`. A cell with higher density will sink through a cell with lower density, provided the lower cell has `Trait::Displaceable`.

Approximate values used in the built-ins:

| Material | Density |
|---|---|
| Empty | 0.0 |
| Oil | 0.8 |
| Water | 1.0 |
| Sand | 1.5 |
| Wall | 999.0 |

### spreadFactor

The maximum number of cells a Liquid or Gas particle searches horizontally each frame. Water uses 6. Set to 0 for Powder or Static.

### shadeMin / shadeMax

When a cell is painted or spawned, its `shade` field is randomised in `[shadeMin, shadeMax]`. Granular materials use that value directly for per-particle brightness variation. Liquids use a softened version of the same shade plus a subtle surface highlight so water and oil read as continuous pools instead of noisy grains.

### color

The base RGBA color rendered at shade 128 (neutral). For granular materials the rendered color is:

```
rendered.r = clamp(color.r * (shade / 128.0), 0, 255)
```

Liquids use a similar modulation but with much weaker per-cell variation and a brighter exposed surface. `ColorRGBA` is a plain struct with no SFML dependency.

### spawnState

`spawnState` is the default state used when the user paints a material or when code calls `spawnInto(x, y, mat)`. For simple materials it is just the material id with zeroed state. For timed materials like Smoke and Fire it also carries the default lifetime range.

### coolingRate / heatEmission / heatConductivity

These fields drive the shared heat pass:

- `coolingRate`: how quickly the cell drifts back toward ambient temperature `0`
- `heatEmission`: heat added to each cardinal neighbor every tick
- `heatConductivity`: how quickly temperature equalizes with neighboring cells

This keeps temperature transfer data-driven instead of burying it in per-material code.

### heatReactions

`heatReactions` are temperature-triggered self-transforms evaluated during the heat pass. They are the basis for phase changes and ignition thresholds.

Current examples:

- Water turns into Steam when it gets hot enough
- Oil turns into Fire when heated past its ignition point
- Steam condenses back into Water when it cools

### interactionRules

`interactionRules` are reusable neighbor-triggered reactions evaluated after movement. Each rule can:

- match neighbors by exact material id, trait flags, or both
- search the 4-cardinal neighborhood or the full 8-neighbor Moore neighborhood
- apply a probability gate
- transform the current cell, the neighbor, or both

### specialHook

A `std::function<void(Simulation&, int, int)>` called after movement and interaction rules. For Organic materials it is still the per-tick driver because there is no movement family. Use this for behavior that cannot be expressed as a simple contact rule: burn-down timers, smoke dissipation, plants that age over time, acid that keeps dissolving after contact.

Set to `nullptr` for materials that need no special logic.

## MaterialRegistry

`MaterialRegistry` owns all `MaterialDef` entries in a `std::vector` indexed directly by `MaterialId`, giving O(1) lookup. The `getByName()` method does a linear scan and is only used at startup or for UI queries — never in the hot path.

Key methods:

```cpp
void              registerMaterial(MaterialDef def);
const MaterialDef* get(MaterialId id) const;         // O(1)
const MaterialDef* getByName(const std::string&) const; // O(n), startup only
bool              has(MaterialId id) const;
```

At startup, `main.cpp` calls `MaterialRegistry::buildDefaults()` which returns a registry pre-loaded with Empty, Sand, Water, Wall, Oil, Smoke, Fire, and Steam. The registry is then moved into the `Simulation` constructor. After that, `sim.materials()` provides read-only access to it.

## Built-in materials

### Empty (0)

Represents an air/void cell. Never processed by the update loop. `density = 0`, `spreadFactor = 0`, color is black.

### Sand (1)

- Movement: `Powder` — falls, slides diagonally, sinks through water
- Traits: `Movable | AffectedByGravity | Displaceable`
- Density: 1.5
- Shade: 96–159 (warm variation around the golden base color)
- Color: `{194, 160, 80}` — golden yellow

Sand is itself `Displaceable`, meaning a future material with density > 1.5 (lava, wet concrete) will automatically sink through sand without any code changes.

### Water (2)

- Movement: `Liquid` — falls, spreads up to 6 cells laterally
- Traits: `Movable | AffectedByGravity | Displaceable | LiquidLike`
- Density: 1.0
- Shade: 110–140 (subtle variation)
- Color: `{30, 120, 220}` — medium blue
- Heat reaction: can boil into Steam at high temperature

Water is `Displaceable` with density 1.0, so denser materials automatically sink through it via `tryDisplaceByDensity`. That includes sand (density 1.5) and any future liquid denser than water.

### Wall (3)

- Movement: `Static` — never moves
- Traits: `SolidLike`
- Density: 999.0
- Shade: 100–120
- Color: `{110, 110, 115}` — grey

Nothing has a density high enough to displace a wall.

### Oil (4)

- Movement: `Liquid` — falls and spreads laterally, but more slowly than water
- Traits: `Movable | AffectedByGravity | Displaceable | LiquidLike | Flammable`
- Density: 0.8
- Shade: 40–60
- Color: `{70, 50, 15}` — dark brown

Oil is lighter than water, so water automatically sinks below it. Sand is denser than both, so it sinks through oil as well.

With the heat system in place, Oil now ignites because it gets hot enough, not because Fire special-cases Oil directly.

### Smoke (5)

- Movement: `Gas` — rises and spreads laterally
- Traits: `Movable | Displaceable | GasLike`
- Density: 0.05
- Default lifetime: 20-40 ticks
- Color: `{95, 95, 95}`

Smoke uses `spawnState` plus a small `specialHook` to count down its lifetime until it dissipates.

### Fire (6)

- Movement: `Organic`
- Default lifetime: 24-42 ticks
- Color: `{255, 110, 25}`

Fire combines reusable contact rules with a tiny lifecycle hook:

- `interactionRules`: touching Water quenches Fire into Steam, and touching any `Flammable` neighbor can ignite it
- `specialHook`: emits smoke upward and burns down its own lifetime

It also emits heat each tick, so flammables can ignite either from direct contact or from getting hot enough over time.

### Steam (7)

- Movement: `Gas`
- Traits: `Movable | Displaceable | GasLike`
- Density: 0.04
- Default temperature: hot
- Color: `{205, 205, 205}`

Steam is the first real temperature-driven phase-change material:

- Water can transform into Steam once heated enough
- Steam cools over time and condenses back into Water
- Because it is a normal `Gas` material, it reuses the same gas movement family as Smoke

## Adding a new material

The entire change is one `registerMaterial()` call in `src/material_registry.cpp:buildDefaults()`. No changes to the simulation, renderer, or main loop.

**Example: Acid**

```cpp
// In MaterialRegistry::buildDefaults(), after the existing built-ins:

{
    MaterialDef d;
    d.id            = 8;          // MAT_ACID
    d.name          = "Acid";
    d.movementModel = MovementModel::Liquid;
    d.traits        = Trait::Movable | Trait::AffectedByGravity | Trait::Displaceable | Trait::LiquidLike;
    d.density       = 1.1f;
    d.spreadFactor  = 4;
    d.shadeMin      = 120;
    d.shadeMax      = 160;
    d.color         = {90, 200, 90, 255};
    d.spawnState    = CellSpawnDesc{d.id};
    d.specialHook   = nullptr;
    reg.registerMaterial(std::move(d));
}
```

Wire up a key binding in `main.cpp`:

```cpp
case sf::Keyboard::Key::Num8: currentMat = 8; break;
```

The same architecture works for non-gas materials too: acid would automatically reuse the Liquid movement family.
