#pragma once

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;
constexpr int CELL_SIZE = 4;

constexpr int GRID_WIDTH = WINDOW_WIDTH / CELL_SIZE;
constexpr int GRID_HEIGHT = WINDOW_HEIGHT / CELL_SIZE;

enum class CellType
{
    Empty,
    Sand,
    Wall,
    Water
};
