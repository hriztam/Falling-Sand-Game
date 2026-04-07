# Sand

Sand is a gravity-driven material.

## Basic behavior

A sand particle usually follows this priority:
1. try to move straight down
2. if blocked, try diagonal down-left
3. if blocked, try diagonal down-right
4. otherwise stay in place

This gives sand its familiar pile-forming behavior.

## Example rule structure

```cpp
if (cellBelowIsEmpty)
{
    moveDown();
}
else if (downLeftIsEmpty)
{
    moveDownLeft();
}
else if (downRightIsEmpty)
{
    moveDownRight();
}
```

In the real code, left and right are randomized so the pile does not always prefer one side.

## Why sand forms slopes

Sand cannot move sideways on flat ground in your current model.
It only moves:
- down
- diagonally down

That means once it lands, it only spreads if it has a lower diagonal path.

This naturally creates triangular piles.

## Sand in this project

Your sand logic lives in `Simulation::updateSand(...)`.

The important idea is that sand can move into:
- `Empty`
- `Water`

That allows sinking behavior.

## Example from this project's design

```cpp
if (getCell(x, y + 1) == CellType::Empty || getCell(x, y + 1) == CellType::Water)
{
    trySwap(x, y, x, y + 1);
    return;
}
```

Why `trySwap(...)` is useful:
- if target is `Empty`, swapping acts like a normal move
- if target is `Water`, swapping makes sand sink and water rise

## Random left/right choice

Without randomness, sand piles often lean in a predictable direction.

Example pattern:

```cpp
const bool tryLeftFirst = (std::rand() % 2 == 0);
```

Then the simulation tries:
- left first, then right
or
- right first, then left

This small detail improves the look of the simulation a lot.

## Common improvements later

You might later add:
- heavier materials that displace sand
- wet sand behavior
- different densities
- a "resting" optimization for particles that no longer move
