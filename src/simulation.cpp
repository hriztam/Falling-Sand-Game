#include "simulation.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>

namespace {
constexpr int kMinTemperature = std::numeric_limits<int8_t>::min();
constexpr int kMaxTemperature = std::numeric_limits<int8_t>::max();

int clampTemperature(int value)
{
    return std::clamp(value, kMinTemperature, kMaxTemperature);
}
} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
Simulation::Simulation(MaterialRegistry registry)
    : m_registry(std::move(registry))
    , m_grid(GRID_WIDTH * GRID_HEIGHT)
    , m_temperatureDelta(GRID_WIDTH * GRID_HEIGHT, 0)
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

// Displace (nx,ny) when it holds a denser Displaceable material than (x,y).
// This is the inverse of tryDisplaceByDensity and is used for buoyant gases
// such as steam bubbling upward through liquids.
bool Simulation::tryDisplaceByBuoyancy(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))      return false;
    if (m_updated[idx(nx, ny)]) return false;

    const MaterialId targetId = m_grid[idx(nx, ny)].material;
    if (targetId == MAT_EMPTY) return false;

    const MaterialDef* mover  = m_registry.get(m_grid[idx(x, y)].material);
    const MaterialDef* target = m_registry.get(targetId);
    if (!mover || !target) return false;

    if (!(target->traits & Trait::Displaceable)) return false;
    if (mover->density >= target->density)       return false;

    return trySwap(x, y, nx, ny);
}

// Replace the cell at (x,y) with mat, reset aux fields, randomise shade.
void Simulation::spawnInto(int x, int y, MaterialId mat)
{
    CellSpawnDesc spawn;
    spawn.material = mat;

    if (const MaterialDef* def = m_registry.get(mat)) {
        spawn = def->spawnState;
        spawn.material = mat;
    }

    spawnInto(x, y, spawn, false);
}

void Simulation::spawnInto(int x, int y, const CellSpawnDesc& spawn, bool markUpdated)
{
    if (!inBounds(x, y)) return;
    const MaterialDef* def = m_registry.get(spawn.material);
    Cell& c = m_grid[idx(x, y)];

    c.material    = spawn.material;
    c.temperature = spawn.temperature;

    const int lifeRange = int(spawn.lifeMax) - int(spawn.lifeMin) + 1;
    c.life = uint8_t(spawn.lifeMin + (lifeRange > 1 ? std::rand() % lifeRange : 0));
    c.aux  = spawn.aux;

    switch (spawn.shadeMode) {
    case ShadeMode::Preserve:
        break;
    case ShadeMode::Fixed:
        c.shade = spawn.shade;
        break;
    case ShadeMode::Randomized:
    default:
        if (!def || spawn.material == MAT_EMPTY) {
            c.shade = 128;
        } else {
            const int range = int(def->shadeMax) - int(def->shadeMin) + 1;
            c.shade = uint8_t(def->shadeMin + (range > 1 ? std::rand() % range : 0));
        }
        break;
    }

    if (markUpdated)
        m_updated[idx(x, y)] = 1;
}

bool Simulation::cellMatches(int x, int y, const MaterialMatch& match) const
{
    if (!inBounds(x, y))
        return false;

    const Cell& cell = m_grid[idx(x, y)];
    if (match.material && cell.material != *match.material)
        return false;

    const MaterialDef* def = m_registry.get(cell.material);
    if (!def)
        return false;

    if ((def->traits & match.requiredTraits) != match.requiredTraits)
        return false;
    if ((def->traits & match.forbiddenTraits) != 0)
        return false;

    return true;
}

bool Simulation::tryApplyInteractionRule(int x, int y, const InteractionRule& rule)
{
    static constexpr std::array<std::pair<int, int>, 4> kCardinalOffsets{{
        { 0, -1},
        { 1,  0},
        { 0,  1},
        {-1,  0},
    }};
    static constexpr std::array<std::pair<int, int>, 8> kMooreOffsets{{
        {-1, -1},
        { 0, -1},
        { 1, -1},
        {-1,  0},
        { 1,  0},
        {-1,  1},
        { 0,  1},
        { 1,  1},
    }};

    auto tryNeighbor = [&](int nx, int ny) -> bool {
        if (!cellMatches(nx, ny, rule.neighbor))
            return false;
        if (rule.chancePercent < 100 && (std::rand() % 100) >= rule.chancePercent)
            return false;

        if (rule.selfResult)
            spawnInto(x, y, *rule.selfResult, true);
        if (rule.neighborResult)
            spawnInto(nx, ny, *rule.neighborResult, true);
        return true;
    };

    if (rule.neighborhood == InteractionNeighborhood::Moore) {
        for (const auto& [dx, dy] : kMooreOffsets) {
            if (tryNeighbor(x + dx, y + dy))
                return true;
        }
        return false;
    }

    for (const auto& [dx, dy] : kCardinalOffsets) {
        if (tryNeighbor(x + dx, y + dy))
            return true;
    }
    return false;
}

bool Simulation::applyInteractionRules(int x, int y, const std::vector<InteractionRule>& rules)
{
    bool appliedAny = false;
    for (const InteractionRule& rule : rules) {
        if (tryApplyInteractionRule(x, y, rule)) {
            appliedAny = true;
            if (rule.stopAfterApply)
                break;
            if (m_updated[idx(x, y)])
                break;
        }
    }
    return appliedAny;
}

bool Simulation::tryApplyHeatReaction(int x, int y, const HeatReaction& reaction)
{
    if (!inBounds(x, y))
        return false;

    const Cell& cell = m_grid[idx(x, y)];
    if (reaction.minTemperature && cell.temperature < *reaction.minTemperature)
        return false;
    if (reaction.maxTemperature && cell.temperature > *reaction.maxTemperature)
        return false;
    if (reaction.chancePercent < 100 && (std::rand() % 100) >= reaction.chancePercent)
        return false;

    if (reaction.selfResult)
        spawnInto(x, y, *reaction.selfResult, false);
    return true;
}

bool Simulation::applyHeatReactions(int x, int y, const std::vector<HeatReaction>& reactions)
{
    bool appliedAny = false;
    for (const HeatReaction& reaction : reactions) {
        if (!tryApplyHeatReaction(x, y, reaction))
            continue;

        appliedAny = true;
        if (reaction.stopAfterApply)
            break;
    }
    return appliedAny;
}

void Simulation::addTemperatureDelta(int x, int y, int delta)
{
    if (!inBounds(x, y) || delta == 0)
        return;

    m_temperatureDelta[idx(x, y)] += static_cast<int16_t>(delta);
}

void Simulation::updateHeat()
{
    static constexpr std::array<std::pair<int, int>, 4> kCardinalOffsets{{
        { 0, -1},
        { 1,  0},
        { 0,  1},
        {-1,  0},
    }};

    std::fill(m_temperatureDelta.begin(), m_temperatureDelta.end(), int16_t(0));

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            const Cell& cell = m_grid[idx(x, y)];
            if (cell.material == MAT_EMPTY)
                continue;

            const MaterialDef* def = m_registry.get(cell.material);
            if (!def)
                continue;

            if (def->coolingRate > 0 && cell.temperature != 0) {
                const int cooling = std::min(def->coolingRate, static_cast<uint8_t>(std::abs(static_cast<int>(cell.temperature))));
                addTemperatureDelta(x, y, cell.temperature > 0 ? -cooling : cooling);
            }

            if (def->heatEmission > 0) {
                for (const auto& [dx, dy] : kCardinalOffsets) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (!inBounds(nx, ny))
                        continue;
                    const Cell& neighbor = m_grid[idx(nx, ny)];
                    if (neighbor.material == MAT_EMPTY)
                        continue;
                    if (neighbor.material == cell.material)
                        continue;
                    addTemperatureDelta(nx, ny, def->heatEmission);
                }
            }

            if (x + 1 < GRID_WIDTH) {
                const Cell& rightCell = m_grid[idx(x + 1, y)];
                if (rightCell.material != MAT_EMPTY) {
                    const MaterialDef* rightDef = m_registry.get(rightCell.material);
                    const int transferCap = rightDef
                        ? std::min(def->heatConductivity, rightDef->heatConductivity)
                        : 0;
                    const int diff = static_cast<int>(cell.temperature) - static_cast<int>(rightCell.temperature);

                    if (transferCap > 0 && std::abs(diff) >= 2) {
                        const int transfer = std::min(transferCap, std::abs(diff) / 2);
                        if (diff > 0) {
                            addTemperatureDelta(x, y, -transfer);
                            addTemperatureDelta(x + 1, y, transfer);
                        } else {
                            addTemperatureDelta(x, y, transfer);
                            addTemperatureDelta(x + 1, y, -transfer);
                        }
                    }
                }
            }

            if (y + 1 < GRID_HEIGHT) {
                const Cell& downCell = m_grid[idx(x, y + 1)];
                if (downCell.material != MAT_EMPTY) {
                    const MaterialDef* downDef = m_registry.get(downCell.material);
                    const int transferCap = downDef
                        ? std::min(def->heatConductivity, downDef->heatConductivity)
                        : 0;
                    const int diff = static_cast<int>(cell.temperature) - static_cast<int>(downCell.temperature);

                    if (transferCap > 0 && std::abs(diff) >= 2) {
                        const int transfer = std::min(transferCap, std::abs(diff) / 2);
                        if (diff > 0) {
                            addTemperatureDelta(x, y, -transfer);
                            addTemperatureDelta(x, y + 1, transfer);
                        } else {
                            addTemperatureDelta(x, y, transfer);
                            addTemperatureDelta(x, y + 1, -transfer);
                        }
                    }
                }
            }
        }
    }

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            Cell& cell = m_grid[idx(x, y)];
            if (cell.material == MAT_EMPTY)
                continue;

            const int nextTemp = clampTemperature(static_cast<int>(cell.temperature) + m_temperatureDelta[idx(x, y)]);
            cell.temperature = static_cast<int8_t>(nextTemp);
        }
    }

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            const Cell& cell = m_grid[idx(x, y)];
            if (cell.material == MAT_EMPTY)
                continue;

            const MaterialDef* def = m_registry.get(cell.material);
            if (!def || def->heatReactions.empty())
                continue;

            applyHeatReactions(x, y, def->heatReactions);
        }
    }
}

void Simulation::runPostMoveBehavior(int x, int y)
{
    if (m_updated[idx(x, y)])
        return;

    const MaterialDef* def = m_registry.get(m_grid[idx(x, y)].material);
    if (!def)
        return;

    if (!def->interactionRules.empty())
        applyInteractionRules(x, y, def->interactionRules);

    if (m_updated[idx(x, y)])
        return;

    def = m_registry.get(m_grid[idx(x, y)].material);
    if (def && def->specialHook)
        def->specialHook(*this, x, y);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void Simulation::paint(int cx, int cy, int radius, MaterialId mat)
{
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            const int nx = cx + dx, ny = cy + dy;
            if (!inBounds(nx, ny)) continue;

            spawnInto(nx, ny, mat);
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

// Liquid: falls, sinks through lighter displaceable liquids by density,
// attempts diagonal downward movement, then spreads laterally up to
// def.spreadFactor cells.
void Simulation::updateLiquid(int x, int y, const MaterialDef& def)
{
    if (tryMove(x, y, x, y + 1))              return;
    if (tryDisplaceByDensity(x, y, x, y + 1)) return;

    const int firstDir  = (std::rand() % 2) ? 1 : -1;
    const int secondDir = -firstDir;
    if (tryMove(x, y, x + firstDir,  y + 1))              return;
    if (tryDisplaceByDensity(x, y, x + firstDir,  y + 1)) return;
    if (tryMove(x, y, x + secondDir, y + 1))              return;
    if (tryDisplaceByDensity(x, y, x + secondDir, y + 1)) return;

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
    if (tryDisplaceByBuoyancy(x, y, x, y - 1)) return;

    const int firstDir  = (std::rand() % 2) ? 1 : -1;
    const int secondDir = -firstDir;
    if (tryMove(x, y, x + firstDir,  y - 1)) return;
    if (tryDisplaceByBuoyancy(x, y, x + firstDir,  y - 1)) return;
    if (tryMove(x, y, x + secondDir, y - 1)) return;
    if (tryDisplaceByBuoyancy(x, y, x + secondDir, y - 1)) return;

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
    updateHeat();
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
                // Organic materials skip the movement family but still go
                // through the shared interaction + hook phase below.
                break;
            case MovementModel::Gas:
                // Gas is handled in Pass 2; skip here.
                continue;
            default:
                break;
            }

            runPostMoveBehavior(x, y);
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
            runPostMoveBehavior(x, y);
        }
    }
}
