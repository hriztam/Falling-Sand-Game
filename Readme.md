# Falling Sand Game

An experimental falling-sand simulation/game inspired by _Noita_.

## Why I’m building this

Most of my recent work has been in web development and AI, so I’m using this project to go deeper into “core” programming: simulation, performance, and game-dev mechanics. The goal is to force myself to think more rigorously, research as needed, and apply math + CS fundamentals I don’t use every day.

## Background

I’m learning as I go. The closest “engine-ish” thing I’ve built is a ray tracer from scratch in C++: https://github.com/hriztam/Ray-Tracer

## Tech

- Language: C++
- Library: SFML 3

## Current features

- Sand
- Water
- Walls
- Eraser
- Sand-water swapping

## Project structure

```text
src/
  main.cpp
  simulation.h
  simulation.cpp
  types.h

docs/
  README.md
  project-overview.md
  cellular-automata.md
  material-sand.md
  material-water.md
  sand-water-interaction.md
```

## Controls

- `1`: Sand
- `2`: Wall
- `3`: Water
- `0`: Eraser
- `C`: Clear the grid
- `Left Mouse`: Paint selected material

## Docs

- `docs/README.md`
- `docs/project-overview.md`
- `docs/cellular-automata.md`
- `docs/material-sand.md`
- `docs/material-water.md`
- `docs/sand-water-interaction.md`
