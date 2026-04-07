# Cellular Automata

A falling sand game is usually a kind of **cellular automaton**.

## Core idea

The screen is divided into a grid.
Each grid cell stores a small piece of state.

In this project, the state is:

```cpp
enum class CellType
{
    Empty,
    Sand,
    Wall,
    Water
};
```

Each frame, the program updates cells according to local rules.

Examples:
- sand tries to move down
- water tries to flow down or sideways
- walls do not move

## Why a grid is useful

A grid makes the simulation simple because each cell only needs to inspect nearby neighbors.

Typical neighbors used in this project:
- below: `(x, y + 1)`
- down-left: `(x - 1, y + 1)`
- down-right: `(x + 1, y + 1)`
- left: `(x - 1, y)`
- right: `(x + 1, y)`

## 2D coordinates in a 1D array

The grid is stored in a flat vector instead of a vector of vectors.

Example:

```cpp
int index(int x, int y)
{
    return y * GRID_WIDTH + x;
}
```

Why this works:
- each row has `GRID_WIDTH` cells
- row `y` starts at `y * GRID_WIDTH`
- add `x` to reach the correct column

## Update rules

A cellular automaton usually follows this pattern:

```cpp
for each cell:
    read nearby cells
    decide movement or change
    write updated state
```

In falling sand games, rules are usually local and simple, but many particles together create complex motion.

## Important issue: update order bias

If you always scan in the same direction, particles can develop a visible bias.

Example problem:
- always scanning left to right can make fluids drift unnaturally

A simple fix is to alternate scan direction:

```cpp
const int xStart = scanLeftToRight ? 0 : (GRID_WIDTH - 1);
const int xEndExclusive = scanLeftToRight ? GRID_WIDTH : -1;
const int xStep = scanLeftToRight ? 1 : -1;
```

Then flip the direction each frame.

## In-place update vs next-grid update

### In-place update
This means you read and write the same grid during the update.

Pros:
- simple to implement
- fast for small projects

Cons:
- movement depends on iteration order
- particles may move more than expected in one frame
- interactions become harder to reason about

### Next-grid update
This means:
- read from `currentGrid`
- write into `nextGrid`
- swap them at the end of the frame

Pros:
- cleaner logic
- less directional bias
- easier to add complex materials

Cons:
- more code
- need to design conflict rules carefully

## Why cellular automata feel interesting

The rules are simple, but the combined result looks dynamic:
- piles form naturally
- fluids spread around walls
- dense materials sink
- motion emerges from local interactions
