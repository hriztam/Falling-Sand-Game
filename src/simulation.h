#pragma once
#include "types.h"
#include <vector>

class Simulation {
public:
    Simulation();

    void update();
    void paint(int cx, int cy, int radius, Material mat);
    void clear();

    [[nodiscard]] Cell getCell(int x, int y) const;
    [[nodiscard]] bool inBounds(int x, int y) const;

private:
    [[nodiscard]] int idx(int x, int y) const { return y * GRID_WIDTH + x; }

    // Move (x,y) into empty (nx,ny). Marks destination so it isn't moved again.
    bool moveCell(int x, int y, int nx, int ny);

    // Swap (x,y) [sand] with (nx,ny) [water] — sand sinks, water rises.
    bool swapWithWater(int x, int y, int nx, int ny);

    void updateSand(int x, int y);
    void updateWater(int x, int y);

    std::vector<Cell> m_grid;
    std::vector<bool> m_updated; // true = a particle moved INTO this cell this frame
    bool              m_scanLeft = true;
};
