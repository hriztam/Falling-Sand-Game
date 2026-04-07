# Sand-Water Interaction

One of the first interesting material interactions is making sand sink through water.

## Concept

Sand is denser than water.
So when sand encounters water below it, the sand should move downward and the water should move upward.

In a grid simulation, this is usually implemented as a **swap**.

## Why swap is the right model

Suppose this vertical arrangement exists:

```text
S
W
```

Where:
- `S` = sand
- `W` = water

If sand falls into water, the result should become:

```text
W
S
```

That is exactly a swap.

## Example helper

```cpp
bool Simulation::trySwap(int fromX, int fromY, int toX, int toY)
{
    if (!inBounds(fromX, fromY) || !inBounds(toX, toY))
    {
        return false;
    }

    std::swap(m_grid[index(fromX, fromY)], m_grid[index(toX, toY)]);
    return true;
}
```

This helper does not decide whether the swap is allowed.
It only performs the swap if both cells are valid.

That separation is important:
- rule function decides if a move should happen
- helper function performs the low-level data change

## Sand rule with water support

Conceptually, sand checks these targets:
- below
- down-left
- down-right

And it accepts either:
- `Empty`
- `Water`

Example:

```cpp
if (target == CellType::Empty || target == CellType::Water)
{
    trySwap(fromX, fromY, toX, toY);
}
```

## Resulting behavior

This creates a convincing effect:
- sand falls into pools
- water gets pushed upward or aside
- sand settles below water level

That makes the materials feel like they have different density.

## Why this is a good early interaction

This is a strong next step after independent sand and water logic because it introduces:
- cross-material rules
- density intuition
- richer visual behavior

It is still simple enough to implement with local neighbor checks.

## Future interaction ideas

Once you are comfortable with swapping, you can extend the idea:
- lava turns water into steam
- water wets sand
- acid destroys walls slowly
- smoke rises through air but not through solids
