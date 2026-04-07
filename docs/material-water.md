# Water

Water is a fluid-like material.

## Basic behavior

A simple water particle usually follows this priority:
1. move down if possible
2. otherwise move diagonally down
3. otherwise move sideways
4. otherwise stay in place

That makes water:
- fall under gravity
- spread around obstacles
- pool at the bottom of enclosed spaces

## Example rule structure

```cpp
if (tryDown)
{
    moveDown();
}
else if (tryDiagonal)
{
    moveDiagonal();
}
else if (trySideways)
{
    moveSideways();
}
```

## Water in this project

Your water logic lives in `Simulation::updateWater(...)`.

Water only moves into `Empty` cells in the current implementation.

That means:
- it flows around walls
- it does not push sand
- it gets displaced by sand when sand swaps into it

## Why water needs horizontal spreading

If water only fell straight down, it would stack like sand.
That would not look like a fluid.

Sideways movement is what makes water flatten and fill containers.

## Example helper used in this project

```cpp
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
```

This helper keeps the movement code cleaner.

## Why water can still look imperfect

Simple water logic often has limits:
- it may spread too slowly
- it may show update-order artifacts
- it may not equalize pressure realistically

That is normal in beginner falling sand projects.

## Common improvements later

You might later add:
- multi-cell lateral flow
- pressure-based spreading
- a second grid for cleaner updates
- interactions with steam, lava, or plants
