#include "simulation.h"
#include <algorithm>
#include <cstdlib>

Simulation::Simulation()
    : m_grid(GRID_WIDTH * GRID_HEIGHT)
    , m_updated(GRID_WIDTH * GRID_HEIGHT, false)
{}

// ---------------------------------------------------------------------------
// Core primitive: move a cell from (x,y) into (nx,ny) if it is empty and
// hasn't already received a particle this frame. Marks the destination so no
// other particle can land on it in the same frame.
// ---------------------------------------------------------------------------
bool Simulation::moveCell(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))                          return false;
    if (m_updated[idx(nx, ny)])                     return false;
    if (m_grid[idx(nx, ny)].material != Material::Empty) return false;

    m_grid[idx(nx, ny)] = m_grid[idx(x, y)];
    m_grid[idx(x, y)]   = Cell{};
    m_updated[idx(nx, ny)] = true;
    return true;
}

// ---------------------------------------------------------------------------
// Swap (x,y) [must be sand] with (nx,ny) [must be water].
// Sand sinks to (nx,ny); water rises to (x,y).
// Mark both cells so the displaced water cannot continue climbing via
// multiple swaps in the same frame.
// ---------------------------------------------------------------------------
bool Simulation::swapWithWater(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))                               return false;
    if (m_updated[idx(nx, ny)])                          return false;
    if (m_grid[idx(nx, ny)].material != Material::Water) return false;

    std::swap(m_grid[idx(x, y)], m_grid[idx(nx, ny)]);
    m_updated[idx(nx, ny)] = true; // sand now here — don't process again
    m_updated[idx(x, y)]   = true; // displaced water waits until next frame
    return true;
}

// ---------------------------------------------------------------------------
// Sand: fall down, sink through water, slide diagonally.
// ---------------------------------------------------------------------------
void Simulation::updateSand(int x, int y)
{
    if (moveCell(x, y, x, y + 1))       return;
    if (swapWithWater(x, y, x, y + 1))  return;

    const int d = (std::rand() % 2) ? 1 : -1;

    if (moveCell(x, y, x + d, y + 1)) return;
    if (moveCell(x, y, x - d, y + 1)) return;
}

// ---------------------------------------------------------------------------
// Water: fall → diagonal fall → spread one cell left/right.
//
// Key design decisions:
//  - Only 1-cell horizontal spread per frame: prevents the "teleport star"
//    artifact that appeared with "furthest reachable" spreading.
//  - Preferred direction follows scan direction so adjacent water cells
//    cooperate rather than fighting each other, giving smooth laminar flow.
//  - Diagonal fall is tried before horizontal so water hugs slopes naturally.
// ---------------------------------------------------------------------------
void Simulation::updateWater(int x, int y)
{
    if (moveCell(x, y, x, y + 1)) return;

    const int firstDir  = (std::rand() % 2) ? 1 : -1;
    const int secondDir = -firstDir;
    if (moveCell(x, y, x + firstDir, y + 1)) return;
    if (moveCell(x, y, x + secondDir, y + 1)) return;

    auto findFlowTarget = [&](int dir) -> int {
        int furthestSupported = x;

        for (int step = 1; step <= WATER_FLOW; ++step) {
            const int nx = x + dir * step;
            if (!inBounds(nx, y)) break;
            if (m_grid[idx(nx, y)].material != Material::Empty) break;
            if (m_updated[idx(nx, y)]) break;

            furthestSupported = nx;

            if (inBounds(nx, y + 1) &&
                m_grid[idx(nx, y + 1)].material == Material::Empty &&
                !m_updated[idx(nx, y + 1)]) {
                return nx;
            }
        }

        return furthestSupported;
    };

    const int firstTarget  = findFlowTarget(firstDir);
    const int secondTarget = findFlowTarget(secondDir);

    const int firstDist  = std::abs(firstTarget - x);
    const int secondDist = std::abs(secondTarget - x);

    if (firstDist > 0 && firstDist >= secondDist && moveCell(x, y, firstTarget, y)) return;
    if (secondDist > 0 && moveCell(x, y, secondTarget, y)) return;
    if (firstDist > 0 && moveCell(x, y, firstTarget, y)) return;

}

// ---------------------------------------------------------------------------
// Main update: single-grid, bottom-to-top, alternating L/R scan.
// ---------------------------------------------------------------------------
void Simulation::update()
{
    std::fill(m_updated.begin(), m_updated.end(), false);
    m_scanLeft = !m_scanLeft;

    const int xStart = m_scanLeft ? 0 : GRID_WIDTH - 1;
    const int xStep  = m_scanLeft ? 1 : -1;

    for (int y = GRID_HEIGHT - 1; y >= 0; --y) {
        for (int x = xStart; x >= 0 && x < GRID_WIDTH; x += xStep) {
            if (m_updated[idx(x, y)]) continue;

            switch (m_grid[idx(x, y)].material) {
            case Material::Sand:  updateSand(x, y);  break;
            case Material::Water: updateWater(x, y); break;
            default: break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Paint a circular brush.
// ---------------------------------------------------------------------------
void Simulation::paint(int cx, int cy, int radius, Material mat)
{
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            const int nx = cx + dx, ny = cy + dy;
            if (!inBounds(nx, ny)) continue;

            Cell& c = m_grid[idx(nx, ny)];
            c.material = mat;
            c.shade = (mat == Material::Empty)
                ? uint8_t(128)
                : uint8_t(96 + std::rand() % 64);
        }
    }
}

void Simulation::clear()
{
    std::fill(m_grid.begin(), m_grid.end(), Cell{});
    std::fill(m_updated.begin(), m_updated.end(), false);
}

Cell Simulation::getCell(int x, int y) const
{
    if (!inBounds(x, y)) return Cell{Material::Wall, 128};
    return m_grid[idx(x, y)];
}

bool Simulation::inBounds(int x, int y) const
{
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}
