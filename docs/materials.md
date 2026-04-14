# Materials

This document describes the material system: what a `MaterialDef` contains, how the `MaterialRegistry` works, what the four built-in materials are, and how to add a new one.

## MaterialId

```cpp
using MaterialId = uint16_t;
```

Every material is identified by a `uint16_t` integer. The four built-in IDs are reserved as constants:

```cpp
constexpr MaterialId MAT_EMPTY = 0;
constexpr MaterialId MAT_SAND  = 1;
constexpr MaterialId MAT_WATER = 2;
constexpr MaterialId MAT_WALL  = 3;
```

Custom materials use IDs starting at 4. There is room for up to 65,535 distinct materials.

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

    // Optional hook called after the movement family runs.
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
| Water | 1.0 |
| Sand | 1.5 |
| Wall | 999.0 |

### spreadFactor

The maximum number of cells a Liquid or Gas particle searches horizontally each frame. Water uses 6. Set to 0 for Powder or Static.

### shadeMin / shadeMax

When a cell is painted or spawned, its `shade` field is randomised in `[shadeMin, shadeMax]`. The renderer modulates the base color by `shade / 128.0` — values below 128 darken the pixel, above 128 lighten it. This gives each particle a unique brightness without storing a full color per cell.

### color

The base RGBA color rendered at shade 128 (neutral). The actual rendered color is:

```
rendered.r = clamp(color.r * (shade / 128.0), 0, 255)
```

`ColorRGBA` is a plain struct with no SFML dependency.

### specialHook

A `std::function<void(Simulation&, int, int)>` called after the movement family runs. For Organic materials it is called instead of any family. Use this for behavior that cannot be expressed by the generic families: fire that spreads and burns out, plants that grow into neighbors, acid that transforms the cells it touches.

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

At startup, `main.cpp` calls `MaterialRegistry::buildDefaults()` which returns a registry pre-loaded with Empty, Sand, Water, and Wall. The registry is then moved into the `Simulation` constructor. After that, `sim.materials()` provides read-only access to it.

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

Water is `Displaceable` with density 1.0, so sand (density 1.5) automatically sinks through it via `tryDisplaceByDensity`.

### Wall (3)

- Movement: `Static` — never moves
- Traits: `SolidLike`
- Density: 999.0
- Shade: 100–120
- Color: `{110, 110, 115}` — grey

Nothing has a density high enough to displace a wall.

## Adding a new material

The entire change is one `registerMaterial()` call in `src/material_registry.cpp:buildDefaults()`. No changes to the simulation, renderer, or main loop.

**Example: Oil**

Oil floats on water (lower density), flows slowly (smaller spread factor), and will be flammable in the future.

```cpp
// In MaterialRegistry::buildDefaults(), after Wall:

{
    MaterialDef d;
    d.id            = 4;          // MAT_OIL — add constexpr to types.h if referenced elsewhere
    d.name          = "Oil";
    d.movementModel = MovementModel::Liquid;
    d.traits        = Trait::Movable | Trait::AffectedByGravity
                    | Trait::Displaceable | Trait::LiquidLike | Trait::Flammable;
    d.density       = 0.8f;       // lighter than water — floats on top
    d.spreadFactor  = 3;          // spreads more slowly than water
    d.shadeMin      = 40;
    d.shadeMax      = 60;
    d.color         = {70, 50, 15, 255};
    d.specialHook   = nullptr;    // ignition hook added later
    reg.registerMaterial(std::move(d));
}
```

Wire up a key binding in `main.cpp`:

```cpp
case sf::Keyboard::Key::Num4: currentMat = 4; break;
```

That's it. Because oil has `Displaceable` and `density = 0.8 < 1.0`, water will automatically sink through it. Because sand has `density = 1.5 > 0.8`, sand will automatically sink through it as well.

**Example: Smoke (Gas)**

```cpp
{
    MaterialDef d;
    d.id            = 5;
    d.name          = "Smoke";
    d.movementModel = MovementModel::Gas;
    d.traits        = Trait::Movable | Trait::GasLike;
    d.density       = 0.1f;
    d.spreadFactor  = 4;
    d.shadeMin      = 60;
    d.shadeMax      = 90;
    d.color         = {80, 80, 80, 255};
    d.specialHook   = nullptr;
    reg.registerMaterial(std::move(d));
}
```

Gas particles are processed in Pass 2 (top-to-bottom scan), so smoke rises correctly without any additional setup.
