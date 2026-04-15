#pragma once
#include "types.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Forward declaration so MaterialDef can reference Simulation in specialHook
// without creating a circular include.
class Simulation;

enum class InteractionNeighborhood : uint8_t {
    Cardinal = 0, // up, down, left, right
    Moore    = 1, // cardinal + diagonals
};

enum class ShadeMode : uint8_t {
    Randomized = 0,
    Preserve   = 1,
    Fixed      = 2,
};

struct MaterialMatch {
    std::optional<MaterialId> material;
    uint16_t                  requiredTraits  = 0;
    uint16_t                  forbiddenTraits = 0;
};

struct CellSpawnDesc {
    MaterialId material    = MAT_EMPTY;
    uint8_t    lifeMin     = 0;
    uint8_t    lifeMax     = 0;
    int8_t     temperature = 0;
    uint8_t    aux         = 0;
    ShadeMode  shadeMode   = ShadeMode::Randomized;
    uint8_t    shade       = 128;
};

struct InteractionRule {
    MaterialMatch                     neighbor;
    InteractionNeighborhood          neighborhood  = InteractionNeighborhood::Cardinal;
    uint8_t                          chancePercent = 100;
    bool                             stopAfterApply = true;
    std::optional<CellSpawnDesc>     selfResult;
    std::optional<CellSpawnDesc>     neighborResult;
};

// ---------------------------------------------------------------------------
// MaterialDef — complete description of one material type. Stored by value in
// the MaterialRegistry vector. One entry covers every particle of that kind;
// "material classes" here means registry definitions, not heap-allocated
// polymorphic objects per particle.
// ---------------------------------------------------------------------------
struct MaterialDef {
    MaterialId    id            = MAT_EMPTY;
    std::string   name;
    MovementModel movementModel = MovementModel::Empty;
    uint16_t      traits        = 0;          // bitfield of Trait:: constants
    float         density       = 0.f;        // higher density sinks through lower
    int           spreadFactor  = 0;          // lateral spread limit for Liquid/Gas
    uint8_t       shadeMin      = 128;        // inclusive lower bound for shade randomisation
    uint8_t       shadeMax      = 128;        // inclusive upper bound
    ColorRGBA     color         = {};         // base RGBA used by the renderer
    CellSpawnDesc spawnState    = {};         // default state for fresh particles of this material
    std::vector<InteractionRule> interactionRules;

    // Optional per-material hook called after movement and interaction rules.
    // Null means no special behaviour beyond the movement model.
    // Signature: (Simulation& sim, int x, int y)
    std::function<void(Simulation&, int, int)> specialHook;
};

// ---------------------------------------------------------------------------
// MaterialRegistry — owns all MaterialDef entries, looked up by MaterialId
// in O(1) via direct vector indexing.
// ---------------------------------------------------------------------------
class MaterialRegistry {
public:
    MaterialRegistry() = default;

    // Register a material. Grows the backing store to fit def.id.
    // Duplicate IDs or names are rejected (asserts in debug, silent no-op otherwise).
    void registerMaterial(MaterialDef def);

    // Look up by id — returns nullptr if no material is registered for id.
    [[nodiscard]] const MaterialDef* get(MaterialId id) const;

    // Look up by name — O(n) linear scan, call only at startup / UI time.
    [[nodiscard]] const MaterialDef* getByName(const std::string& name) const;

    // True if a MaterialDef for id has been registered.
    [[nodiscard]] bool has(MaterialId id) const;

    // Number of slots (not necessarily all filled).
    [[nodiscard]] std::size_t slotCount() const { return m_defs.size(); }

    // Full list for iteration (renderer, debug UI, etc.).
    [[nodiscard]] const std::vector<MaterialDef>& all() const { return m_defs; }

    // Build the canonical set of built-in materials:
    // Empty (0), Sand (1), Water (2), Wall (3), Oil (4), Smoke (5), Fire (6).
    static MaterialRegistry buildDefaults();

private:
    // Indexed directly by MaterialId for O(1) lookup.
    // Gaps are allowed; a gap slot has id == MAT_EMPTY and empty name.
    std::vector<MaterialDef> m_defs;
};
