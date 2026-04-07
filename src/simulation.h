#pragma once

#include "types.h"

#include <vector>

class Simulation
{
public:
    Simulation();

    void update();
    void paint(int gridX, int gridY, int radius, CellType material);
    void clear();

    [[nodiscard]] CellType getCell(int x, int y) const;
    [[nodiscard]] bool inBounds(int x, int y) const;

private:
    [[nodiscard]] int index(int x, int y) const;

    void updateSand(int x, int y);
    void updateWater(int x, int y);
    bool trySwap(int fromX, int fromY, int toX, int toY);
    bool tryMoveWater(int fromX, int fromY, int toX, int toY);

    std::vector<CellType> m_grid;
    bool m_scanLeftToRight;
};
