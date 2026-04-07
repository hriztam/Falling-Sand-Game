# Project Overview

This project is a small grid-based falling sand simulation written in C++ with SFML.

## Current file structure

```text
src/
  main.cpp
  simulation.h
  simulation.cpp
  types.h
```

## Responsibility of each file

### `src/main.cpp`
This file handles application-level work:
- creating the SFML window
- reading keyboard and mouse input
- selecting the current material
- asking the simulation to update each frame
- drawing the grid to the screen

It should **not** contain the actual particle rules.

### `src/types.h`
This file contains shared definitions:
- window size
- cell size
- grid size
- `CellType`

This is a good place for simple shared constants and enums.

### `src/simulation.h`
This file declares the `Simulation` class.

It exposes the public API used by `main.cpp`, such as:
- `update()`
- `paint(...)`
- `clear()`
- `getCell(...)`

This file answers the question: "What can the simulation do?"

### `src/simulation.cpp`
This file implements the behavior of the world.

It contains:
- the grid data
- bounds/index helpers
- sand logic
- water logic
- material interaction logic

This file answers the question: "How does the world behave?"

## Main architecture idea

The project is split into two layers:

### App layer
This is `src/main.cpp`.

It handles:
- input
- frame loop
- rendering

### Simulation layer
This is `src/simulation.cpp` and `src/simulation.h`.

It handles:
- world state
- particle updates
- painting into the world

That split matters because it keeps rendering code separate from game logic.

## Example: main loop responsibility split

```cpp
while (window.isOpen())
{
    handleInput();
    simulation.update();
    drawWorld();
}
```

This is not your exact code, but it shows the intended structure.

## Why this structure scales better

When you add more materials like smoke, lava, or oil:
- `main.cpp` should stay mostly unchanged
- only the simulation code should grow

That is a good sign that the architecture is doing its job.

## Good next extensions

After sand and water, common next steps are:
- brush size controls
- pause / resume
- smoke that rises
- lava that destroys or transforms other cells
- a second grid for cleaner simulation updates
