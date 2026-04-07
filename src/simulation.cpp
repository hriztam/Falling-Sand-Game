#include "simulation.h"

#include <algorithm>
#include <cstdlib>

Simulation::Simulation()
    : m_grid(GRID_WIDTH * GRID_HEIGHT, CellType::Empty),
      m_nextGrid(GRID_WIDTH * GRID_HEIGHT, CellType::Empty),
      m_scanLeftToRight(true)
{
}

void Simulation::update()
{
    std::fill(m_nextGrid.begin(), m_nextGrid.end(), CellType::Empty);

    const int xStart = m_scanLeftToRight ? 0 : (GRID_WIDTH - 1);
    const int xEndExclusive = m_scanLeftToRight ? GRID_WIDTH : -1;
    const int xStep = m_scanLeftToRight ? 1 : -1;

    for (int y = GRID_HEIGHT - 1; y >= 0; --y)
    {
        for (int x = xStart; x != xEndExclusive; x += xStep)
        {
            if (getCell(x, y) != CellType::Sand)
            {
                continue;
            }

            if (!isEmptyInNext(x, y))
            {
                continue;
            }

            updateSand(x, y);
        }
    }

    for (int y = GRID_HEIGHT - 1; y >= 0; --y)
    {
        for (int x = xStart; x != xEndExclusive; x += xStep)
        {
            if (getCell(x, y) != CellType::Water)
            {
                continue;
            }

            if (!isEmptyInNext(x, y))
            {
                continue;
            }

            updateWater(x, y);
        }
    }

    for (int y = GRID_HEIGHT - 1; y >= 0; --y)
    {
        for (int x = xStart; x != xEndExclusive; x += xStep)
        {
            if (getCell(x, y) != CellType::Wall)
            {
                continue;
            }

            tryPlaceInNext(x, y, CellType::Wall);
        }
    }

    m_grid.swap(m_nextGrid);
    m_scanLeftToRight = !m_scanLeftToRight;
}

void Simulation::paint(int gridX, int gridY, int radius, CellType material)
{
    if (!inBounds(gridX, gridY))
    {
        return;
    }

    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int nx = gridX + dx;
            const int ny = gridY + dy;

            if (inBounds(nx, ny))
            {
                m_grid[index(nx, ny)] = material;
            }
        }
    }
}

void Simulation::clear()
{
    std::fill(m_grid.begin(), m_grid.end(), CellType::Empty);
    std::fill(m_nextGrid.begin(), m_nextGrid.end(), CellType::Empty);
}

CellType Simulation::getCell(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return CellType::Wall;
    }

    return m_grid[index(x, y)];
}

bool Simulation::inBounds(int x, int y) const
{
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

int Simulation::index(int x, int y) const
{
    return y * GRID_WIDTH + x;
}

bool Simulation::isEmptyInCurrent(int x, int y) const
{
    return getCell(x, y) == CellType::Empty;
}

bool Simulation::isWaterInCurrent(int x, int y) const
{
    return getCell(x, y) == CellType::Water;
}

bool Simulation::isEmptyInNext(int x, int y) const
{
    if (!inBounds(x, y))
    {
        return false;
    }

    return m_nextGrid[index(x, y)] == CellType::Empty;
}

bool Simulation::tryPlaceInNext(int x, int y, CellType material)
{
    if (!inBounds(x, y) || !isEmptyInNext(x, y))
    {
        return false;
    }

    m_nextGrid[index(x, y)] = material;
    return true;
}

void Simulation::placeOrStay(int fromX, int fromY, int toX, int toY, CellType material)
{
    if (tryPlaceInNext(toX, toY, material))
    {
        return;
    }

    tryPlaceInNext(fromX, fromY, material);
}

bool Simulation::tryDisplaceWater(int sandX, int sandY, int waterX, int waterY)
{
    if (!inBounds(waterX, waterY))
    {
        return false;
    }

    if (!isWaterInCurrent(waterX, waterY))
    {
        return false;
    }

    if (!isEmptyInNext(waterX, waterY))
    {
        return false;
    }

    if (!isEmptyInNext(sandX, sandY))
    {
        return false;
    }

    m_nextGrid[index(waterX, waterY)] = CellType::Sand;
    m_nextGrid[index(sandX, sandY)] = CellType::Water;
    return true;
}

void Simulation::updateSand(int x, int y)
{
    if (isEmptyInCurrent(x, y + 1))
    {
        placeOrStay(x, y, x, y + 1, CellType::Sand);
        return;
    }

    if (tryDisplaceWater(x, y, x, y + 1))
    {
        return;
    }

    const bool tryLeftFirst = (std::rand() % 2 == 0);
    const int firstDx = tryLeftFirst ? -1 : 1;
    const int secondDx = -firstDx;

    if (isEmptyInCurrent(x + firstDx, y + 1))
    {
        placeOrStay(x, y, x + firstDx, y + 1, CellType::Sand);
        return;
    }

    if (tryDisplaceWater(x, y, x + firstDx, y + 1))
    {
        return;
    }

    if (isEmptyInCurrent(x + secondDx, y + 1))
    {
        placeOrStay(x, y, x + secondDx, y + 1, CellType::Sand);
        return;
    }

    if (tryDisplaceWater(x, y, x + secondDx, y + 1))
    {
        return;
    }

    tryPlaceInNext(x, y, CellType::Sand);
}

void Simulation::updateWater(int x, int y)
{
    if (isEmptyInCurrent(x, y + 1))
    {
        placeOrStay(x, y, x, y + 1, CellType::Water);
        return;
    }

    const bool tryLeftFirst = (std::rand() % 2 == 0);
    const int firstDx = tryLeftFirst ? -1 : 1;
    const int secondDx = -firstDx;

    if (isEmptyInCurrent(x + firstDx, y + 1))
    {
        placeOrStay(x, y, x + firstDx, y + 1, CellType::Water);
        return;
    }

    if (isEmptyInCurrent(x + secondDx, y + 1))
    {
        placeOrStay(x, y, x + secondDx, y + 1, CellType::Water);
        return;
    }

    if (isEmptyInCurrent(x + firstDx, y))
    {
        placeOrStay(x, y, x + firstDx, y, CellType::Water);
        return;
    }

    if (isEmptyInCurrent(x + secondDx, y))
    {
        placeOrStay(x, y, x + secondDx, y, CellType::Water);
        return;
    }

    tryPlaceInNext(x, y, CellType::Water);
}
