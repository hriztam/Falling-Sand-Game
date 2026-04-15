#pragma once
#include "types.h"
#include "material_registry.h"
#include <vector>

// ---------------------------------------------------------------------------
// World — lightweight, non-owning read-only view of the grid passed to the
// renderer. No Cell* ownership; no allocation.
// ---------------------------------------------------------------------------
struct World {
    const Cell* cells;  // GRID_WIDTH * GRID_HEIGHT entries, row-major
    int         width;
    int         height;
};

// ---------------------------------------------------------------------------
// Simulation — owns the grid and registry, drives all particle physics.
//
// Execution order per cell:
//   movement family  →  interaction rules  →  optional specialHook  →  finalize
//
// All grid reads and writes go through the helper primitives (tryMove,
// trySwap, tryDisplaceByDensity, spawnInto) so a later double-buffer
// migration stays localised to those four functions.
// ---------------------------------------------------------------------------
class Simulation {
public:
    explicit Simulation(MaterialRegistry registry);

    // Advance the simulation by one tick.
    void update();

    // Paint a circular brush of radius cells centred on (cx, cy).
    void paint(int cx, int cy, int radius, MaterialId mat);

    // Reset the entire grid to Empty.
    void clear();

    // Safe single-cell accessor (returns empty Cell for out-of-bounds).
    [[nodiscard]] Cell getCell(int x, int y) const;

    [[nodiscard]] bool inBounds(int x, int y) const;

    // Read-only grid view for the renderer.
    [[nodiscard]] World world() const;

    // Registry accessor — used by main.cpp for HUD queries.
    [[nodiscard]] const MaterialRegistry& materials() const { return m_registry; }

    // ---------------------------------------------------------------------------
    // UpdateContext helpers — public so specialHook lambdas can call them.
    // All functions return true if the operation succeeded.
    // ---------------------------------------------------------------------------

    // Move (x,y) into empty (nx,ny). Marks destination so it is not moved again
    // this frame. Source becomes Empty.
    bool tryMove(int x, int y, int nx, int ny);

    // Unconditional swap of any two valid cells. Marks both as updated.
    bool trySwap(int x, int y, int nx, int ny);

    // Swap (x,y) into (nx,ny) only when (nx,ny) holds a Displaceable material
    // with lower density than (x,y). Implements sand-sinks-through-water logic
    // without special-casing any material pair.
    bool tryDisplaceByDensity(int x, int y, int nx, int ny);

    // Replace (x,y) with mat, resetting all extra fields and randomising shade
    // from the material's shadeMin/shadeMax range.
    void spawnInto(int x, int y, MaterialId mat);

    // Replace (x,y) with a fully specified cell state. Used by interaction
    // rules and material hooks that need to set life/aux or preserve shade.
    void spawnInto(int x, int y, const CellSpawnDesc& spawn, bool markUpdated = false);

    // Evaluate a list of neighbour-driven interaction rules for the current
    // cell. Returns true if any rule fired.
    bool applyInteractionRules(int x, int y, const std::vector<InteractionRule>& rules);

private:
    [[nodiscard]] int idx(int x, int y) const { return y * GRID_WIDTH + x; }

    // Movement families — each corresponds to a MovementModel value.
    void updatePowder(int x, int y, const MaterialDef& def);
    void updateLiquid(int x, int y, const MaterialDef& def);
    void updateGas   (int x, int y, const MaterialDef& def);
    // Static and Organic are handled inline in update(); no dedicated function.
    void runPostMoveBehavior(int x, int y);
    [[nodiscard]] bool cellMatches(int x, int y, const MaterialMatch& match) const;
    bool tryApplyInteractionRule(int x, int y, const InteractionRule& rule);

    MaterialRegistry    m_registry;
    std::vector<Cell>   m_grid;
    std::vector<uint8_t> m_updated; // uint8_t avoids std::vector<bool> bitfield overhead
    bool                m_scanLeft = true;
};
