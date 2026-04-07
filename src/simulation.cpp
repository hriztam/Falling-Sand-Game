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
// We only mark the sand's destination — the water at (x,y) is behind the
// scan head and won't be revisited this frame, so no mark is needed there.
// ---------------------------------------------------------------------------
bool Simulation::swapWithWater(int x, int y, int nx, int ny)
{
    if (!inBounds(nx, ny))                               return false;
    if (m_updated[idx(nx, ny)])                          return false;
    if (m_grid[idx(nx, ny)].material != Material::Water) return false;

    std::swap(m_grid[idx(x, y)], m_grid[idx(nx, ny)]);
    m_updated[idx(nx, ny)] = true; // sand now here — don't process again
    // (x,y) now holds water; it's behind the scan cursor, safe to leave unmarked
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

    if (moveCell(x, y, x + d, y + 1))      return;
    if (swapWithWater(x, y, x + d, y + 1)) return;
    if (moveCell(x, y, x - d, y + 1))      return;
    if (swapWithWater(x, y, x - d, y + 1)) return;
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

    // Diagonal fall — prefer scan direction for smooth bias
    const int d = m_scanLeft ? 1 : -1;
    if (moveCell(x, y, x + d, y + 1)) return;
    if (moveCell(x, y, x - d, y + 1)) return;

    // Horizontal spread — one cell only; natural fluid feel without artifacts
    if (moveCell(x, y, x + d, y)) return;
    if (moveCell(x, y, x - d, y)) return;

    // Pressure: if completely trapped and sand is sitting directly above,
    // bubble upward so sand can eventually sink through even a dense pile.
    // Row y-1 hasn't been scanned yet (we go bottom-to-top), so water that
    // moves there will be processed normally this frame — no mark needed.
    const int ay = y - 1;
    if (inBounds(x, ay) &&
        m_grid[idx(x, ay)].material == Material::Sand &&
        !m_updated[idx(x, ay)] &&
        (std::rand() % 3 == 0)) // 33 % chance keeps bubble rate natural
    {
        std::swap(m_grid[idx(x, y)], m_grid[idx(x, ay)]);
        // (x,ay) now has water — will be picked up by the scan when it reaches row ay
        // (x,y)  now has sand  — already past our scan cursor for this row
    }
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
