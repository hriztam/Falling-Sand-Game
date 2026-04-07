#pragma once
#include <cstdint>

constexpr int WINDOW_WIDTH  = 1200;
constexpr int WINDOW_HEIGHT = 800;
constexpr int CELL_SIZE     = 4;
constexpr int GRID_WIDTH    = WINDOW_WIDTH  / CELL_SIZE;
constexpr int GRID_HEIGHT   = WINDOW_HEIGHT / CELL_SIZE;
constexpr int WATER_FLOW    = 6; // max horizontal cells water can travel per frame

enum class Material : uint8_t {
    Empty = 0,
    Sand,
    Water,
    Wall,
};

// Each cell carries its material and a per-particle shade for visual depth.
struct Cell {
    Material material = Material::Empty;
    uint8_t  shade    = 128; // 96..159 range, set randomly on paint
};
