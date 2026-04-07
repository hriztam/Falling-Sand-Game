#include "simulation.h"

#include <algorithm>
#include <cstdlib>

Simulation::Simulation()
    : m_grid(GRID_WIDTH * GRID_HEIGHT, CellType::Empty),
      m_scanLeftToRight(true)
{
}

void Simulation::update()
{
    const int xStart = m_scanLeftToRight ? 0 : (GRID_WIDTH - 1);
    const int xEndExclusive = m_scanLeftToRight ? GRID_WIDTH : -1;
    const int xStep = m_scanLeftToRight ? 1 : -1;

    for (int y = GRID_HEIGHT - 2; y >= 0; --y)
    {
        for (int x = xStart; x != xEndExclusive; x += xStep)
        {
            if (getCell(x, y) == CellType::Sand)
            {
                updateSand(x, y);
            }
        }
    }

    for (int y = GRID_HEIGHT - 2; y >= 0; --y)
    {
        for (int x = xStart; x != xEndExclusive; x += xStep)
        {
            if (getCell(x, y) == CellType::Water)
            {
                updateWater(x, y);
            }
        }
    }

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

void Simulation::updateSand(int x, int y)
{
    if (getCell(x, y + 1) == CellType::Empty || getCell(x, y + 1) == CellType::Water)
    {
        trySwap(x, y, x, y + 1);
        return;
    }

    const bool tryLeftFirst = (std::rand() % 2 == 0);

    if (tryLeftFirst)
    {
        if (getCell(x - 1, y + 1) == CellType::Empty || getCell(x - 1, y + 1) == CellType::Water)
        {
            trySwap(x, y, x - 1, y + 1);
        }
        else if (getCell(x + 1, y + 1) == CellType::Empty || getCell(x + 1, y + 1) == CellType::Water)
        {
            trySwap(x, y, x + 1, y + 1);
        }
    }
    else
    {
        if (getCell(x + 1, y + 1) == CellType::Empty || getCell(x + 1, y + 1) == CellType::Water)
        {
            trySwap(x, y, x + 1, y + 1);
        }
        else if (getCell(x - 1, y + 1) == CellType::Empty || getCell(x - 1, y + 1) == CellType::Water)
        {
            trySwap(x, y, x - 1, y + 1);
        }
    }
}

void Simulation::updateWater(int x, int y)
{
    if (tryMoveWater(x, y, x, y + 1))
    {
        return;
    }

    const bool tryLeftFirst = (std::rand() % 2 == 0);

    if (tryLeftFirst)
    {
        if (tryMoveWater(x, y, x - 1, y + 1))
        {
            return;
        }
        if (tryMoveWater(x, y, x + 1, y + 1))
        {
            return;
        }
        if (tryMoveWater(x, y, x - 1, y))
        {
            return;
        }
        tryMoveWater(x, y, x + 1, y);
        return;
    }

    if (tryMoveWater(x, y, x + 1, y + 1))
    {
        return;
    }
    if (tryMoveWater(x, y, x - 1, y + 1))
    {
        return;
    }
    if (tryMoveWater(x, y, x + 1, y))
    {
        return;
    }
    tryMoveWater(x, y, x - 1, y);
}

bool Simulation::trySwap(int fromX, int fromY, int toX, int toY)
{
    if (!inBounds(fromX, fromY) || !inBounds(toX, toY))
    {
        return false;
    }

    std::swap(m_grid[index(fromX, fromY)], m_grid[index(toX, toY)]);
    return true;
}

bool Simulation::tryMoveWater(int fromX, int fromY, int toX, int toY)
{
    if (!inBounds(toX, toY) || getCell(toX, toY) != CellType::Empty)
    {
        return false;
    }

    m_grid[index(toX, toY)] = CellType::Water;
    m_grid[index(fromX, fromY)] = CellType::Empty;
    return true;
}
