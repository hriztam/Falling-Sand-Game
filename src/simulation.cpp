#include "simulation.h"
#include <algorithm>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
Simulation::Simulation(MaterialRegistry registry)
    : m_registry(std::move(registry))
    , m_grid(GRID_WIDTH * GRID_HEIGHT)
    , m_updated(GRID_WIDTH * GRID_HEIGHT, 0)
{}

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------
bool Simulation::inBounds(int x, int y) const
{
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

// ---------------------------------------------------------------------------
// Grid accessors
// ---------------------------------------------------------------------------
Cell Simulation::getCell(int x, int y) const
{
    if (!inBounds(x, y)) return Cell{};
    return m_grid[idx(x, y)];
}

World Simulation::world() const
{
    return {m_grid.data(), GRID_WIDTH, GRID_HEIGHT};
}

// ---------------------------------------------------------------------------
// UpdateContext helpers
// ---------------------------------------------------------------------------

// Move (x,y) into the empty cell (nx,ny). Marks destination updated so no
// other particle can land on it in the same frame.
bool Simulation::tryMove(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))                          return false;
    if (m_updated[idx(nx, ny)])                     return false;
    if (m_grid[idx(nx, ny)].material != MAT_EMPTY)  return false;

    m_grid[idx(nx, ny)] = m_grid[idx(x, y)];
    m_grid[idx(x, y)]   = Cell{};
    m_updated[idx(nx, ny)] = 1;
    return true;
}

// Unconditional swap of (x,y) and (nx,ny). Both cells are marked updated so
// neither is processed again this frame.
bool Simulation::trySwap(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))      return false;
    if (m_updated[idx(nx, ny)]) return false;

    std::swap(m_grid[idx(x, y)], m_grid[idx(nx, ny)]);
    m_updated[idx(nx, ny)] = 1;
    m_updated[idx(x, y)]   = 1;
    return true;
}

// Displace (nx,ny) if it holds a Displaceable material with lower density than
// (x,y). Implements density-stratification (sand sinks through water) without
// hard-coding any material pair.
bool Simulation::tryDisplaceByDensity(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))      return false;
    if (m_updated[idx(nx, ny)]) return false;

    const MaterialId targetId = m_grid[idx(nx, ny)].material;
    if (targetId == MAT_EMPTY) return false; // use tryMove for empty slots

    const MaterialDef* mover  = m_registry.get(m_grid[idx(x, y)].material);
    const MaterialDef* target = m_registry.get(targetId);
    if (!mover || !target) return false;

    if (!(target->traits & Trait::Displaceable)) return false;
    if (mover->density <= target->density)        return false;

    return trySwap(x, y, nx, ny);
}

// Replace the cell at (x,y) with mat, reset aux fields, randomise shade.
void Simulation::spawnInto(int x, int y, MaterialId mat)
{
    if (!inBounds(x, y)) return;
    const MaterialDef* def = m_registry.get(mat);
    Cell& c = m_grid[idx(x, y)];
    c.material    = mat;
    c.temperature = 0;
    c.life        = 0;
    c.aux         = 0;
    if (!def || mat == MAT_EMPTY) {
        c.shade = 128;
    } else {
        const int range = int(def->shadeMax) - int(def->shadeMin) + 1;
        c.shade = uint8_t(def->shadeMin + (range > 1 ? std::rand() % range : 0));
    }
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void Simulation::paint(int cx, int cy, int radius, MaterialId mat)
{
    const MaterialDef* def = m_registry.get(mat);
    const int range = def ? (int(def->shadeMax) - int(def->shadeMin) + 1) : 1;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            const int nx = cx + dx, ny = cy + dy;
            if (!inBounds(nx, ny)) continue;

            Cell& c       = m_grid[idx(nx, ny)];
            c.material    = mat;
            c.temperature = 0;
            c.life        = 0;
            c.aux         = 0;
            c.shade = (mat == MAT_EMPTY || !def)
                ? uint8_t(128)
                : uint8_t(def->shadeMin + (range > 1 ? std::rand() % range : 0));
        }
    }
}

void Simulation::clear()
{
    std::fill(m_grid.begin(), m_grid.end(), Cell{});
    std::fill(m_updated.begin(), m_updated.end(), uint8_t(0));
}

// ---------------------------------------------------------------------------
// Movement families
// ---------------------------------------------------------------------------

// Powder: falls straight down, displaces liquids by density, slides diagonally.
// Mirrors the old updateSand behaviour but is material-agnostic.
void Simulation::updatePowder(int x, int y, const MaterialDef& /*def*/)
{
    if (tryMove(x, y, x, y + 1))              return;
    if (tryDisplaceByDensity(x, y, x, y + 1)) return;

    const int d = (std::rand() % 2) ? 1 : -1;
    if (tryMove(x, y, x + d, y + 1))          return;
    if (tryMove(x, y, x - d, y + 1))          return;
}

// Liquid: falls, diagonal fall, then lateral spread up to def.spreadFactor cells.
// Mirrors the old updateWater behaviour; spread limit comes from the MaterialDef
// rather than the hard-coded WATER_FLOW constant.
void Simulation::updateLiquid(int x, int y, const MaterialDef& def)
{
    if (tryMove(x, y, x, y + 1)) return;

    const int firstDir  = (std::rand() % 2) ? 1 : -1;
    const int secondDir = -firstDir;
    if (tryMove(x, y, x + firstDir,  y + 1)) return;
    if (tryMove(x, y, x + secondDir, y + 1)) return;

    // Find the furthest reachable horizontal cell in a given direction.
    // Prefers positions where there is an empty cell below (liquid flows
    // toward drops rather than pooling uniformly).
    auto findFlowTarget = [&](int dir) -> int {
        int furthest = x;
        for (int step = 1; step <= def.spreadFactor; ++step) {
            const int nx = x + dir * step;
            if (!inBounds(nx, y))                                   break;
            if (m_grid[idx(nx, y)].material != MAT_EMPTY)          break;
            if (m_updated[idx(nx, y)])                              break;
            furthest = nx;
            // If this position has an empty cell below, prefer it immediately.
            if (inBounds(nx, y + 1) &&
                m_grid[idx(nx, y + 1)].material == MAT_EMPTY &&
                !m_updated[idx(nx, y + 1)]) {
                return nx;
            }
        }
        return furthest;
    };

    const int tFirst  = findFlowTarget(firstDir);
    const int tSecond = findFlowTarget(secondDir);
    const int dFirst  = std::abs(tFirst  - x);
    const int dSecond = std::abs(tSecond - x);

    // Move toward whichever reachable target is farther, breaking ties toward
    // the second direction (opposite of the current scan bias).
    if (dFirst > 0 && dFirst >= dSecond && tryMove(x, y, tFirst,  y)) return;
    if (dSecond > 0                      && tryMove(x, y, tSecond, y)) return;
    if (dFirst  > 0                      && tryMove(x, y, tFirst,  y)) return;
}

// Gas: rises straight up, diagonal rise, then lateral spread.
// Mirror of Liquid with inverted vertical direction.
void Simulation::updateGas(int x, int y, const MaterialDef& def)
{
    if (tryMove(x, y, x, y - 1)) return;

    const int firstDir  = (std::rand() % 2) ? 1 : -1;
    const int secondDir = -firstDir;
    if (tryMove(x, y, x + firstDir,  y - 1)) return;
    if (tryMove(x, y, x + secondDir, y - 1)) return;

    auto findFlowTarget = [&](int dir) -> int {
        int furthest = x;
        for (int step = 1; step <= def.spreadFactor; ++step) {
            const int nx = x + dir * step;
            if (!inBounds(nx, y))                          break;
            if (m_grid[idx(nx, y)].material != MAT_EMPTY) break;
            if (m_updated[idx(nx, y)])                     break;
            furthest = nx;
            if (inBounds(nx, y - 1) &&
                m_grid[idx(nx, y - 1)].material == MAT_EMPTY &&
                !m_updated[idx(nx, y - 1)]) {
                return nx;
            }
        }
        return furthest;
    };

    const int tFirst  = findFlowTarget(firstDir);
    const int tSecond = findFlowTarget(secondDir);
    const int dFirst  = std::abs(tFirst  - x);
    const int dSecond = std::abs(tSecond - x);

    if (dFirst > 0 && dFirst >= dSecond && tryMove(x, y, tFirst,  y)) return;
    if (dSecond > 0                      && tryMove(x, y, tSecond, y)) return;
    if (dFirst  > 0                      && tryMove(x, y, tFirst,  y)) return;
}

// ---------------------------------------------------------------------------
// Main update loop
//
// Two passes per frame to give each movement model its natural scan direction:
//
//   Pass 1 — bottom-to-top: Powder, Liquid, Static, Organic.
//             Falling particles are processed from the bottom up so a column
//             of sand settles correctly in one frame rather than cascading
//             one cell at a time over many frames.
//
//   Pass 2 — top-to-bottom: Gas.
//             Rising particles need the inverse scan so smoke reaches the
//             ceiling at the same rate. The same m_updated array from Pass 1
//             prevents double-processing.
//
// Within each pass the horizontal order alternates each frame (m_scanLeft)
// to eliminate left/right scan-direction bias.
// ---------------------------------------------------------------------------
void Simulation::update()
{
    std::fill(m_updated.begin(), m_updated.end(), uint8_t(0));
    m_scanLeft = !m_scanLeft;

    const int xStart = m_scanLeft ? 0 : GRID_WIDTH - 1;
    const int xStep  = m_scanLeft ? 1 : -1;

    // --- Pass 1: bottom-to-top (Powder, Liquid, Static, Organic) -----------
    for (int y = GRID_HEIGHT - 1; y >= 0; --y) {
        for (int x = xStart; x >= 0 && x < GRID_WIDTH; x += xStep) {
            if (m_updated[idx(x, y)]) continue;

            const MaterialId matId = m_grid[idx(x, y)].material;
            if (matId == MAT_EMPTY) continue;

            const MaterialDef* def = m_registry.get(matId);
            if (!def) continue;

            switch (def->movementModel) {
            case MovementModel::Powder:
                updatePowder(x, y, *def);
                break;
            case MovementModel::Liquid:
                updateLiquid(x, y, *def);
                break;
            case MovementModel::Static:
                // Immovable — no movement, but specialHook may still fire.
                break;
            case MovementModel::Organic:
                // Organic materials are driven entirely by their specialHook.
                if (def->specialHook)
                    def->specialHook(*this, x, y);
                continue; // skip the second hook call below
            case MovementModel::Gas:
                // Gas is handled in Pass 2; skip here.
                continue;
            default:
                break;
            }

            // Post-movement specialHook for Powder / Liquid / Static.
            // Only fires if the cell is still at (x,y) (i.e., it didn't move).
            if (def->specialHook && !m_updated[idx(x, y)])
                def->specialHook(*this, x, y);
        }
    }

    // --- Pass 2: top-to-bottom (Gas) ---------------------------------------
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = xStart; x >= 0 && x < GRID_WIDTH; x += xStep) {
            if (m_updated[idx(x, y)]) continue;

            const MaterialId matId = m_grid[idx(x, y)].material;
            if (matId == MAT_EMPTY) continue;

            const MaterialDef* def = m_registry.get(matId);
            if (!def || def->movementModel != MovementModel::Gas) continue;

            updateGas(x, y, *def);

            if (def->specialHook && !m_updated[idx(x, y)])
                def->specialHook(*this, x, y);
        }
    }
}
