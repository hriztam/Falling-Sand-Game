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

    [[nodiscard]] bool isEmptyInCurrent(int x, int y) const;
    [[nodiscard]] bool isWaterInCurrent(int x, int y) const;
    [[nodiscard]] bool isEmptyInNext(int x, int y) const;

    bool tryPlaceInNext(int x, int y, CellType material);
    void placeOrStay(int fromX, int fromY, int toX, int toY, CellType material);
    bool tryDisplaceWater(int sandX, int sandY, int waterX, int waterY);

    std::vector<CellType> m_grid;
    std::vector<CellType> m_nextGrid;
    bool m_scanLeftToRight;
};
